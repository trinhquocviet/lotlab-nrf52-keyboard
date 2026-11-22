/*
Copyright (C) 2019 Jim Jiang <jim@lotlab.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "usb_comm.h"
#include "../config/keyboard_config.h"
#include "app_error.h"
#include "app_scheduler.h"
#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_atomic.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#include <stdlib.h>
#include <string.h>

#include "../ble/ble_hid_service.h"
#include "../main.h"
#include "app_timer.h"
#include "hid_configuration.h"
#include "host.h"

#include "keyboard_host_driver.h"
#include "queue.h"

#ifdef HAS_USB

#define QUEUE_SIZE 256
#define UART_RX_RING_SIZE 4U
#define UART_RX_RING_MASK (UART_RX_RING_SIZE - 1U)
#define UARTE_RX_BUF_SIZE 512U // Power of 2; <= 1023 (nRF52810 10-bit MAXCNT)
#define UARTE_RX_BUF_MASK (UARTE_RX_BUF_SIZE - 1U)
#define UARTE_IDLE_TIMEOUT_US 150U // ~1.7 byte times @ 115200 baud (< CH554 1 ms poll)
#define UARTE_PPI_CH_RXDRDY_COUNT 0U // PPI ch 0..16 are free under S112 (17..31 reserved)
#define UARTE_PPI_CH_RXDRDY_IDLE 1U
#define UARTE_PPI_CH_IDLE_CAPTURE 2U
#define UARTE_PPI_CH_MASK ((1UL << 0U) | (1UL << 1U) | (1UL << 2U))

struct uart_rx_frame {
    uint8_t len;
    uint8_t data[64];
};

uint8_t keyboard_led_val_usb;

static uint8_t recv_buf[64];
static uint8_t recv_index;
static uint8_t recv_len;

static volatile uint8_t s_dma_rx_buf[UARTE_RX_BUF_SIZE];
static uint8_t s_dma_tx_buf[64];
static uint32_t s_dma_rx_read_cnt = 0;
static uint32_t s_dma_endrx_cnt = 0;
static volatile bool s_tx_busy = false;

static struct uart_rx_frame s_rx_ring[UART_RX_RING_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;
static volatile uint8_t s_last_status = 0;
static nrf_atomic_u32_t s_ack_count = 0;
static nrf_atomic_flag_t s_status_pending = 0;
static nrf_atomic_flag_t s_sched_pending = 0;

enum uart_usb_state {
    UART_STATE_IDLE, // 当前 UART 没有正常工作
    UART_STATE_INITED, // 当前 UART 已经初始化，但还未接收到数据
    UART_STATE_WORKING // 当前 UART 正常工作中
};

struct usb_status {
    bool charging_full; // 当前充电状态已满
    bool host_connected; // 当前已连接到主机
    bool usb_disable; // 临时禁用 USB 通信功能
    bool uart_checked; // UART 通信检测标志
    bool usb_protocol; // USB 报告类型
    enum uart_usb_state state; // 当前 UART 所属状态
};

static struct host_driver usb_driver;
static struct usb_status status;

MIXED_QUEUE(uint8_t, uart_queue, QUEUE_SIZE);

/**
 * @brief 计算校验值
 * 
 * @param data 
 * @param len 
 * @return uint8_t 
 */
static uint8_t checksum(uint8_t* data, uint8_t len)
{
    uint16_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum = (checksum + data[i]) & 0xFF;
    }
    return (uint8_t)checksum;
}

/**
 * @brief 发送事件
 * 
 * @param 事件类型
 */
static void send_event(enum user_event event, uint8_t arg)
{
#ifdef NKRO_ENABLE
    switch (event) {
    case USER_EVT_USB:
        if (arg != USB_WORKING)
            trig_event_param(USER_EVT_PROTOCOL, HID_BOOT_PROTOCOL);
        else
            trig_event_param(USER_EVT_PROTOCOL, status.usb_protocol);
        break;
    default:
        break;
    }
#endif
    trig_event_param(event, arg);
}

/**
 * @brief 设置状态
 * 
 * @param host 是否连接到主机
 * @param charge_full 电量是否充满
 * @param protocol USB 当前协议类型
 */
static void set_state(bool host, bool charge_full, bool protocol)
{
    if ((status.state == UART_STATE_INITED) || host != status.host_connected) {
        status.host_connected = host;
        send_event(USER_EVT_USB, host ? (status.usb_disable ? USB_NOT_WORKING : USB_WORKING) : USB_NO_HOST);
    }
#ifdef PIN_CHARGING
    if ((status.state == UART_STATE_INITED) || charge_full != status.charging_full) {
        status.charging_full = charge_full;
        send_event(USER_EVT_CHARGE, status.charging_full ? BATT_CHARGED : BATT_CHARGING);
    }
#endif
    if ((status.state == UART_STATE_INITED) || status.usb_protocol != protocol) {
        status.usb_protocol = protocol;
        send_event(USER_EVT_PROTOCOL, protocol && usb_working() ? HID_REPORT_PROTOCOL : HID_BOOT_PROTOCOL);
    }
    if (status.state == UART_STATE_INITED) {
        status.state = UART_STATE_WORKING;
    }
}

/**
 * @brief 发送 UART 数据
 * 
 * @param data 
 * @param len 
 */
static void uart_send(const uint8_t* data, uint8_t len)
{
    if (len == 0U || len > sizeof(s_dma_tx_buf) || s_tx_busy) {
        return;
    }
    memcpy(s_dma_tx_buf, data, len);
    s_tx_busy = true;
    NRF_UARTE0->EVENTS_ENDTX = 0;
    NRF_UARTE0->EVENTS_TXSTOPPED = 0;
    NRF_UARTE0->TXD.PTR = (uint32_t)(uintptr_t)s_dma_tx_buf;
    NRF_UARTE0->TXD.MAXCNT = len;
    NRF_UARTE0->TASKS_STARTTX = 1;
}

static void rx_ring_push(const uint8_t* frame, uint8_t len)
{
    if ((uint8_t)(s_rx_head - s_rx_tail) >= UART_RX_RING_SIZE) {
        return; // Drop frame cleanly on full SPSC ring
    }
    struct uart_rx_frame* slot = &s_rx_ring[s_rx_head & UART_RX_RING_MASK];
    slot->len = len;
    memcpy(slot->data, frame, len);
    __DMB();
    s_rx_head++;
}

static void rx_ring_drain(void)
{
    uint8_t buf[64];
    while (s_rx_tail != s_rx_head) {
        uint8_t idx = (uint8_t)(s_rx_tail & UART_RX_RING_MASK);
        uint8_t len = s_rx_ring[idx].len;
        if (len >= 4U && len <= sizeof(buf)) {
            memcpy(buf, s_rx_ring[idx].data, len);
        }
        __DMB();
        s_rx_tail++;

        if (len >= 4U && len <= sizeof(buf)) {
            uint8_t sum = checksum(buf, len - 1U);
            if (sum == buf[len - 1U]) {
                // U_CMD H_CMD H_LEN H_DAT... U_SUM
                hid_on_recv(&usb_driver, buf[1], (uint8_t)(len - 4U), &buf[3]);
            } else {
                hid_response_generic(HID_RESP_UART_CHECKSUM_ERROR);
            }
        }
    }
}

static void uart_sched_handler(void* p_event_data, uint16_t event_size)
{
    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);

    (void)nrf_atomic_flag_clear(&s_sched_pending);
    if (status.state == UART_STATE_IDLE) {
        return;
    }

    if (nrf_atomic_flag_clear_fetch(&s_status_pending)) {
        uint8_t st = s_last_status;
        bool charging_status = (st & 0x02U) != 0;
        bool usb_status = (st & 0x04U) != 0;
        bool protocol = (st & 0x08U) != 0;

        set_state(usb_status, charging_status, protocol);

        uint32_t acks = nrf_atomic_u32_fetch_store(&s_ack_count, 0);
        while (acks--) {
            uart_queue_dequeue();
        }
        if (!uart_queue_empty()) {
            uint8_t data[64];
            uint8_t len = uart_queue_peek(data);
            uart_send(data, len);
        }
    }

    rx_ring_drain();
}

static void uart_wakeup(void)
{
    if (!nrf_atomic_flag_set_fetch(&s_sched_pending)) {
        if (app_sched_event_put(NULL, 0, uart_sched_handler) != NRF_SUCCESS) {
            (void)nrf_atomic_flag_clear(&s_sched_pending);
        }
    }
}

/**
 * @brief Parse one received byte from the EasyDMA RX ring in O(1) IRQ context.
 * 
 */
static void uart_parse_byte(uint8_t buff)
{
    // 通信协议：
    // CMD DAT ... DAT SUM
    if (recv_index >= sizeof(recv_buf)) {
        recv_index = 0;
        recv_len = 0;
    }
    recv_buf[recv_index] = buff;
    if (!recv_index) {
        if (buff >= 0x80) { // 配置下发
            uint16_t packet_len = (uint16_t)(buff - 0x80) + 2; // CMD 和 SUM 各占 1 byte
            if (packet_len >= 4 && packet_len <= sizeof(recv_buf)) {
                recv_len = (uint8_t)packet_len;
                recv_index++;
            } else {
                recv_len = 0;
            }
        } else if (buff >= 0x20) { // led
            keyboard_led_val_usb = buff & 0x1F; // 5bit
            status.uart_checked = true;
        } else if (buff >= 0x10) { // status
            s_last_status = buff;
            if (buff & 0x01U) {
                (void)nrf_atomic_u32_add(&s_ack_count, 1);
            }
            (void)nrf_atomic_flag_set(&s_status_pending);
            status.uart_checked = true;
            uart_wakeup();
        }
    } else {
        recv_index++;
        if (recv_index >= recv_len) {
            rx_ring_push(recv_buf, recv_len);
            recv_index = 0;
            recv_len = 0;
            status.uart_checked = true;
            uart_wakeup();
        }
    }
}

static void uarte_consume_until(uint32_t target)
{
    int32_t diff = (int32_t)(target - s_dma_rx_read_cnt);
    if (diff <= 0) {
        return;
    }
    if ((uint32_t)diff > UARTE_RX_BUF_SIZE) {
        s_dma_rx_read_cnt = target - UARTE_RX_BUF_SIZE;
        recv_index = 0;
        recv_len = 0;
    }
    __DMB();
    while (s_dma_rx_read_cnt != target) {
        uint8_t b = s_dma_rx_buf[s_dma_rx_read_cnt & UARTE_RX_BUF_MASK];
        s_dma_rx_read_cnt++;
        uart_parse_byte(b);
    }
}

/**
 * @brief 关闭UART
 * 
 */
static void uart_to_idle()
{
    NVIC_DisableIRQ(UARTE0_UART0_IRQn);
    NVIC_DisableIRQ(TIMER2_IRQn);
    NRF_PPI->CHENCLR = UARTE_PPI_CH_MASK;

    NRF_TIMER1->TASKS_STOP = 1;
    NRF_TIMER1->TASKS_SHUTDOWN = 1;
    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->TASKS_SHUTDOWN = 1;
    NRF_TIMER2->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;

    NRF_UARTE0->SHORTS = 0;
    NRF_UARTE0->INTENCLR = 0xFFFFFFFFUL;
    if (NRF_UARTE0->ENABLE != 0) {
        NRF_UARTE0->EVENTS_RXTO = 0;
        NRF_UARTE0->TASKS_STOPRX = 1;
        NRF_UARTE0->TASKS_STOPTX = 1;
        for (uint32_t spin = 0; spin < 2000U && !NRF_UARTE0->EVENTS_RXTO; spin++) {
        }
    }
    NRF_UARTE0->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE0->PSEL.TXD = 0xFFFFFFFFUL;
    NRF_UARTE0->PSEL.RXD = 0xFFFFFFFFUL;
    NRF_UARTE0->PSEL.RTS = 0xFFFFFFFFUL;
    NRF_UARTE0->PSEL.CTS = 0xFFFFFFFFUL;

    NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
    NVIC_ClearPendingIRQ(TIMER2_IRQn);
    nrf_gpio_cfg_default(UART_TXD);
#ifndef UART_DET
    nrf_gpio_cfg_input(UART_RXD, NRF_GPIO_PIN_PULLDOWN);
#else
    nrf_gpio_cfg_default(UART_RXD);
#endif

    s_dma_rx_read_cnt = 0;
    s_dma_endrx_cnt = 0;
    s_tx_busy = false;
    uart_queue_clear();
    (void)nrf_atomic_u32_store(&s_ack_count, 0);
    (void)nrf_atomic_flag_clear(&s_status_pending);
    (void)nrf_atomic_flag_clear(&s_sched_pending);
    s_last_status = 0;
    s_rx_head = 0;
    s_rx_tail = 0;
    recv_index = 0;
    recv_len = 0;

    // 做一些状态清理
    status.state = UART_STATE_IDLE;
    status.usb_disable = false;
    status.host_connected = false;
    send_event(USER_EVT_USB, USB_NOT_CONNECT);
#ifdef PIN_CHARGING
    send_event(USER_EVT_CHARGE, BATT_NOT_CHARGING);
#endif
    send_event(USER_EVT_PROTOCOL, HID_BOOT_PROTOCOL); // 蓝牙下默认使用BootProtocol（即不启用NKRO）
}

void TIMER2_IRQHandler(void)
{
    if (NRF_TIMER2->EVENTS_COMPARE[0]) {
        NRF_TIMER2->EVENTS_COMPARE[0] = 0;
        (void)NRF_TIMER2->EVENTS_COMPARE[0];
        uint32_t target = NRF_TIMER1->CC[0];
        uarte_consume_until(target);
    }
}

void UARTE0_UART0_IRQHandler(void)
{
    if (NRF_UARTE0->EVENTS_ENDRX) {
        NRF_UARTE0->EVENTS_ENDRX = 0;
        (void)NRF_UARTE0->EVENTS_ENDRX;
        s_dma_endrx_cnt += UARTE_RX_BUF_SIZE;
        uarte_consume_until(s_dma_endrx_cnt);
    }
    if (NRF_UARTE0->EVENTS_ENDTX) {
        NRF_UARTE0->EVENTS_ENDTX = 0;
        (void)NRF_UARTE0->EVENTS_ENDTX;
        NRF_UARTE0->TASKS_STOPTX = 1;
    }
    if (NRF_UARTE0->EVENTS_TXSTOPPED) {
        NRF_UARTE0->EVENTS_TXSTOPPED = 0;
        (void)NRF_UARTE0->EVENTS_TXSTOPPED;
        s_tx_busy = false;
    }
    if (NRF_UARTE0->EVENTS_ERROR) {
        NRF_UARTE0->EVENTS_ERROR = 0;
        (void)NRF_UARTE0->EVENTS_ERROR;
        NRF_UARTE0->ERRORSRC = 0x0FUL;
        recv_index = 0;
        recv_len = 0;
    }
}

/**
 * @brief 启用UART
 * 
 */
static void uart_init_hardware()
{
    s_dma_rx_read_cnt = 0;
    s_dma_endrx_cnt = 0;
    s_tx_busy = false;
    recv_index = 0;
    recv_len = 0;

    nrf_gpio_pin_set(UART_TXD);
    nrf_gpio_cfg_output(UART_TXD);
    nrf_gpio_cfg_input(UART_RXD, NRF_GPIO_PIN_NOPULL);

    NRF_UARTE0->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE0->INTENCLR = 0xFFFFFFFFUL;
    NRF_UARTE0->PSEL.TXD = UART_TXD;
    NRF_UARTE0->PSEL.RXD = UART_RXD;
    NRF_UARTE0->PSEL.RTS = 0xFFFFFFFFUL;
    NRF_UARTE0->PSEL.CTS = 0xFFFFFFFFUL;
    NRF_UARTE0->CONFIG = (UARTE_CONFIG_HWFC_Disabled << UARTE_CONFIG_HWFC_Pos)
        | (UARTE_CONFIG_PARITY_Excluded << UARTE_CONFIG_PARITY_Pos);
    NRF_UARTE0->BAUDRATE = UARTE_BAUDRATE_BAUDRATE_Baud115200;
    NRF_UARTE0->SHORTS = UARTE_SHORTS_ENDRX_STARTRX_Msk;
    NRF_UARTE0->RXD.PTR = (uint32_t)(uintptr_t)s_dma_rx_buf;
    NRF_UARTE0->RXD.MAXCNT = UARTE_RX_BUF_SIZE;
    NRF_UARTE0->EVENTS_ENDRX = 0;
    NRF_UARTE0->EVENTS_ENDTX = 0;
    NRF_UARTE0->EVENTS_TXSTOPPED = 0;
    NRF_UARTE0->EVENTS_ERROR = 0;
    NRF_UARTE0->EVENTS_RXTO = 0;
    NRF_UARTE0->EVENTS_RXDRDY = 0;
    NRF_UARTE0->ERRORSRC = 0x0FUL;

    NRF_TIMER1->TASKS_STOP = 1;
    NRF_TIMER1->TASKS_CLEAR = 1;
    NRF_TIMER1->MODE = (TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos);
    NRF_TIMER1->BITMODE = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
    NRF_TIMER1->CC[0] = 0;
    NRF_TIMER1->TASKS_START = 1;

    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->TASKS_CLEAR = 1;
    NRF_TIMER2->MODE = (TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos);
    NRF_TIMER2->BITMODE = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
    NRF_TIMER2->PRESCALER = 4UL; // 16 MHz / 2^4 = 1 MHz (1 us ticks)
    NRF_TIMER2->CC[0] = UARTE_IDLE_TIMEOUT_US;
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    NRF_TIMER2->INTENCLR = 0xFFFFFFFFUL;
    NRF_TIMER2->INTENSET = TIMER_INTENSET_COMPARE0_Msk;

    NRF_PPI->CHENCLR = UARTE_PPI_CH_MASK;
    NRF_PPI->CH[UARTE_PPI_CH_RXDRDY_COUNT].EEP = (uint32_t)(uintptr_t)&NRF_UARTE0->EVENTS_RXDRDY;
    NRF_PPI->CH[UARTE_PPI_CH_RXDRDY_COUNT].TEP = (uint32_t)(uintptr_t)&NRF_TIMER1->TASKS_COUNT;
    NRF_PPI->FORK[UARTE_PPI_CH_RXDRDY_COUNT].TEP = 0;

    NRF_PPI->CH[UARTE_PPI_CH_RXDRDY_IDLE].EEP = (uint32_t)(uintptr_t)&NRF_UARTE0->EVENTS_RXDRDY;
    NRF_PPI->CH[UARTE_PPI_CH_RXDRDY_IDLE].TEP = (uint32_t)(uintptr_t)&NRF_TIMER2->TASKS_START;
    NRF_PPI->FORK[UARTE_PPI_CH_RXDRDY_IDLE].TEP = (uint32_t)(uintptr_t)&NRF_TIMER2->TASKS_CLEAR;

    NRF_PPI->CH[UARTE_PPI_CH_IDLE_CAPTURE].EEP = (uint32_t)(uintptr_t)&NRF_TIMER2->EVENTS_COMPARE[0];
    NRF_PPI->CH[UARTE_PPI_CH_IDLE_CAPTURE].TEP = (uint32_t)(uintptr_t)&NRF_TIMER2->TASKS_SHUTDOWN;
    NRF_PPI->FORK[UARTE_PPI_CH_IDLE_CAPTURE].TEP = (uint32_t)(uintptr_t)&NRF_TIMER1->TASKS_CAPTURE[0];
    NRF_PPI->CHENSET = UARTE_PPI_CH_MASK;

    NRF_UARTE0->INTENSET = UARTE_INTENSET_ENDRX_Msk
        | UARTE_INTENSET_ENDTX_Msk
        | UARTE_INTENSET_TXSTOPPED_Msk
        | UARTE_INTENSET_ERROR_Msk;

    NVIC_SetPriority(UARTE0_UART0_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
    NVIC_EnableIRQ(UARTE0_UART0_IRQn);

    NVIC_SetPriority(TIMER2_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_ClearPendingIRQ(TIMER2_IRQn);
    NVIC_EnableIRQ(TIMER2_IRQn);

    NRF_UARTE0->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
    NRF_UARTE0->TASKS_STARTRX = 1;

    status.state = UART_STATE_INITED;
}

static void uart_task(void* context)
{
    UNUSED_PARAMETER(context);
    if (status.state != UART_STATE_IDLE) {
        if (s_status_pending || s_ack_count > 0U || s_rx_tail != s_rx_head) {
            uart_sched_handler(NULL, 0);
        }
        // 已经连接状态，检查是否断开
        if (!status.uart_checked) {
            uart_to_idle();
        } else {
            status.uart_checked = false;
        }
    } else {
        // 未连接状态，检查是否连接
#ifdef UART_DET
        if (!nrf_gpio_pin_read(UART_DET)) {
#else
        if (nrf_gpio_pin_read(UART_RXD)) {
#endif
            uart_init_hardware();
        }
    }
}

/**
 * @brief 是否已经连接到主机
 * 
 * @return true 
 * @return false 
 */
bool usb_working(void)
{
    return (status.state == UART_STATE_WORKING) && (status.host_connected) && !(status.usb_disable);
}

/**
 * @brief 通过USB发送按键数据包
 * 
 * @param index 数据包类型
 * @param len 长度
 * @param pattern 
 */
void usb_send(uint8_t index, uint8_t len, uint8_t* pattern)
{
    if (index == PACKET_CONF)
    {
        uart_send_conf(len, pattern);
        return;
    }

    if (len > 61)
        return;

    // Reserve 64 B of uart_queue headroom for PACKET_CONF replies under heavy typing.
    if (uart_queue_free_space() < (uint16_t)(len + 4U + 64U)) {
        return;
    }

    uint8_t data[64];
    data[0] = 0x40 + len;
    data[1] = index;

    memcpy(&data[2], pattern, len);
    data[len + 2] = checksum(data, len + 2);

    (void)uart_queue_enqueue(len + 3, data);
}

/**
 * 通过UART发送配置数据包
 */
void uart_send_conf(uint8_t len, uint8_t* data)
{
    if (len > 62)
        return;

    uint8_t buff[64];
    buff[0] = 0x80 + len;
    memcpy(&buff[1], data, len);
    buff[len + 1] = checksum(buff, len + 1);
    (void)uart_queue_enqueue(len + 2, buff);
}

APP_TIMER_DEF(uart_check_timer);
#define UART_CHECK_INTERVAL APP_TIMER_TICKS(250)

/**
 * @brief 初始化USB通信
 * 
 */
void usb_comm_init()
{
    uint32_t err_code;

    err_code = app_timer_create(&uart_check_timer,
        APP_TIMER_MODE_REPEATED,
        uart_task);

    APP_ERROR_CHECK(err_code);
#ifdef UART_DET
    nrf_gpio_cfg_input(UART_DET, NRF_GPIO_PIN_PULLUP);
    __DSB();
    nrf_delay_us(10);
    if (!nrf_gpio_pin_read(UART_DET)) {
        uart_init_hardware();
    }
#else
    nrf_gpio_cfg_input(UART_RXD, NRF_GPIO_PIN_PULLDOWN);
    __DSB();
    bool uart_high = false;
    for (uint8_t i = 0; i < 15U; i++) {
        nrf_delay_us(10);
        if (nrf_gpio_pin_read(UART_RXD)) {
            uart_high = true;
            break;
        }
    }
    if (uart_high) {
        // 初始化时启用UART尝试接收事件，若没有主机则在超时处关闭
        uart_init_hardware();
    }
#endif
}

/**
 * @brief 启动USB通信计时器
 * 
 */
void usb_comm_timer_start()
{
    uint32_t err_code = app_timer_start(uart_check_timer, UART_CHECK_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief USB 准备睡眠
 * 
 */
void usb_comm_sleep_prepare()
{
    uart_to_idle();
#ifdef UART_DET
    nrf_gpio_cfg_sense_input(UART_DET, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
#else
    nrf_gpio_cfg_sense_input(UART_RXD, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
#endif
}

/**
 * @brief 切换USB和蓝牙设备
 * 
 */
void usb_comm_switch()
{
    if ((status.state == UART_STATE_WORKING) && status.host_connected) {
        status.usb_disable = !status.usb_disable;
        send_event(USER_EVT_USB, status.usb_disable ? USB_NOT_WORKING : USB_WORKING);
    }
}

/**
 * @brief 获取USB键盘下当前的LED状态
 * 
 * @return uint8_t 
 */
static uint8_t usb_get_keyboard_led()
{
    return keyboard_led_val_usb;
}

/**
 * @brief USB 通信驱动
 * 
 */
static struct host_driver usb_driver = {
    .keyboard_leds = &usb_get_keyboard_led,
    .queue_empty = &uart_queue_empty,
    .send_packet = &usb_send,
    .driver_working = &usb_working,
    .mtu = 62,
};

// 以一个较高优先级注册USB驱动
KEYBOARD_HOST_DRIVER(5, usb_driver);

#endif
