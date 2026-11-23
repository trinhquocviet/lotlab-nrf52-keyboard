#pragma once
#include "config.h"
#include "nrf_section.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef ACTIONMAP_ENABLE
#define SINGLE_KEY_SIZE 2
#else
#define SINGLE_KEY_SIZE 1
#endif

#define MAX_LAYER 8 // 存储区域中最大层数
#define MAX_FN_KEYS 32 // 存储区域最大Fn数目
#define MAX_MACRO_SIZE 256 // 宏存储区域最大容量
#define FILE_ID 0x0514

#define KEYMAP_ROW_SIZE (MATRIX_COLS * SINGLE_KEY_SIZE)
#define KEYMAP_LAYER_SIZE (MATRIX_ROWS * KEYMAP_ROW_SIZE)

// Extended device configuration record (0x0498, 56 bytes = 14 words)
#define EXT_CONFIG_MAGIC 0xA5
#define EXT_CONFIG_VERSION 0x02
#define EXT_CONFIG_V1_WORDS 6
#define MAX_CUSTOM_NAME_BYTES 16
#define CHANNEL_PROFILE_COUNT 4
#define LED_COLOR_COUNT 10

enum led_color_slot {
    LED_SLOT_BLE0 = 0,
    LED_SLOT_BLE1 = 1,
    LED_SLOT_BLE2 = 2,
    LED_SLOT_IDLE = 3,
    LED_SLOT_USB = 4,
    LED_SLOT_CHARGING = 5,
    LED_SLOT_FULL = 6,
    LED_SLOT_PASSKEY_REQ = 7,
    LED_SLOT_PASSKEY_SEND = 8,
    LED_SLOT_SLEEP = 9,
};

// Channel profile byte masks (channel: 0=BLE0, 1=BLE1, 2=BLE2, 3=USB)
#define CH_BASE_LAYER_MASK 0x07
#define CH_SWAP_LALT_LGUI 0x08
#define CH_SWAP_RALT_RGUI 0x10
#define CH_SWAP_LCTL_LGUI 0x20
#define CH_NO_GUI 0x40

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t version;
    uint8_t name_len;
    uint8_t reserved;
    uint8_t custom_name[MAX_CUSTOM_NAME_BYTES];
    uint8_t channel_profile[CHANNEL_PROFILE_COUNT];
    uint8_t led_colors[LED_COLOR_COUNT][3];
    uint8_t led_reserved[2];
} ext_config_t;

/**
 * @brief 存储类型
 * 
 */
enum storage_type {
    STORAGE_KEYMAP, // Keymap
    STORAGE_FN, // Fn 区域
    STORAGE_MACRO, // 自定义宏
    STORAGE_CONFIG // 配置区域
};

void storage_read(uint8_t mask);
void storage_init(void);
bool storage_write(uint8_t mask);
void storage_delete(uint8_t mask);
bool storage_degraded(void);
uint16_t storage_read_data(enum storage_type type, uint16_t offset, uint16_t len, uint8_t* data);
uint16_t storage_write_data(enum storage_type type, uint16_t offset, uint16_t len, uint8_t* data);

ext_config_t* storage_get_ext_config(void);
uint8_t keyboard_active_base_layer(void);
uint8_t keyboard_active_swap_flags(void);
void keyboard_set_active_profile(uint8_t base_layer, uint8_t swap_flags);

// 若启用了BOOTMAGIC_ENABLE则自动启用CONFIG_STORAGE
#ifdef BOOTMAGIC_ENABLE
#define CONFIG_STORAGE
#endif

#ifdef CONFIG_STORAGE
extern uint8_t config_block[];
extern const uint8_t default_led_colors[LED_COLOR_COUNT][3];

/**
 * @brief 配置记录区域
 * 
 */
struct config_section {
    /**
     * @brief 数据长度
     * 
     */
    uint8_t len;
    /**
     * @brief 数据指针
     * 
     */
    uint8_t* data;
};

/**
 * @brief 定义一个储存区
 * 
 */
#define CONFIG_SECTION_DEF() NRF_SECTION_DEF(config_def, struct config_section)
/**
 * @brief 定义一块数据记录区域
 * 
 */
#define CONFIG_SECTION(name, length) NRF_SECTION_ITEM_REGISTER(config_def, struct config_section) name = { .len = length };
#define CONFIG_SECTION_COUNT NRF_SECTION_ITEM_COUNT(config_def, struct config_section)
#define CONFIG_SECTION_GET(i) (NRF_SECTION_ITEM_GET(config_def, struct config_section, i))

#endif
