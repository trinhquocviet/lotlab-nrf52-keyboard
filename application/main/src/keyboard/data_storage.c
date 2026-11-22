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
#include <string.h>

#include "data_storage.h"

#include "fds.h"
#include "nrf.h"
#include "app_util_platform.h"
#include "util.h"
#include "nrf_sdh.h"

// keymap
#include "keymap.h"
#ifndef ACTIONMAP_ENABLE
#include "keymap_common.h"
#include "report.h"
#endif
// actionmap
#include "actionmap.h"

// macro
#include "action_macro.h"

#define GET_WORD(_i) ((_i - 1) / 4 + 1)

#define REGISTER_FDS_BLOCK(_name, _size_word, _record_key)     \
    __ALIGN(4)                                                 \
    uint8_t _name##_block[(_size_word * 4)] = { 0 };           \
    __ALIGN(4)                                                 \
    static uint8_t _name##_tx_block[(_size_word * 4)] = { 0 }; \
    static fds_record_desc_t _name##_record_desc = { 0 };      \
    static fds_record_t _name##_record = {                     \
        .file_id = FILE_ID,                                    \
        .key = _record_key,                                    \
        .data = {                                              \
            .p_data = _name##_tx_block,                        \
            .length_words = _size_word,                        \
        }                                                      \
    };

#ifdef KEYMAP_STORAGE
#define KEYMAP_SIZE_WORD GET_WORD(KEYMAP_LAYER_SIZE* MAX_LAYER)
#define KEYMAP_RECORD_KEY 0x0514

REGISTER_FDS_BLOCK(keymap, KEYMAP_SIZE_WORD, KEYMAP_RECORD_KEY)

static bool keymap_valid = true;
static bool volatile s_fds_initialized;
static bool volatile s_fds_init_done;
static volatile ret_code_t s_fds_init_result;
static bool s_storage_degraded = false;
static uint8_t s_active_base_layer = 0;
static uint8_t s_active_swap_flags = 0;
__attribute__((weak)) const uint8_t keymaps_count = 2;
__attribute__((weak)) const uint8_t fn_actions_count = 8;
__attribute__((weak)) void store_config_load_defaults(void) { }

uint8_t keyboard_active_base_layer(void)
{
    return s_active_base_layer;
}

uint8_t keyboard_active_swap_flags(void)
{
    return s_active_swap_flags;
}

void keyboard_set_active_profile(uint8_t base_layer, uint8_t swap_flags)
{
    s_active_base_layer = (base_layer < MAX_LAYER) ? base_layer : 0;
    s_active_swap_flags = swap_flags;
}

static uint8_t apply_channel_swaps(uint8_t kc)
{
    uint8_t flags = s_active_swap_flags;
    if (flags == 0)
        return kc;

    if (flags & CH_SWAP_LALT_LGUI) {
        if (kc == KC_LALT)
            kc = KC_LGUI;
        else if (kc == KC_LGUI)
            kc = KC_LALT;
    }
    if (flags & CH_SWAP_RALT_RGUI) {
        if (kc == KC_RALT)
            kc = KC_RGUI;
        else if (kc == KC_RGUI)
            kc = KC_RALT;
    }
    if (flags & CH_SWAP_LCTL_LGUI) {
        if (kc == KC_LCTRL)
            kc = KC_LGUI;
        else if (kc == KC_LGUI)
            kc = KC_LCTRL;
    }
    if ((flags & CH_NO_GUI) && (kc == KC_LGUI || kc == KC_RGUI)) {
        return KC_NO;
    }
    return kc;
}

static void check_keymap()
{
    bool flag = false;
    for (uint16_t i = 0; i < KEYMAP_LAYER_SIZE * MAX_LAYER; i++) {
        flag |= (keymap_block[i] != 0);
        if (flag)
            break;
    }
    keymap_valid = flag;
}
#define USE_KEYMAP keymap_valid
#ifndef ACTIONMAP_ENABLE
static uint8_t read_raw_keymap_byte(uint8_t layer, keypos_t key)
{
    if (USE_KEYMAP)
        return keymap_block[layer * KEYMAP_LAYER_SIZE + key.row * MATRIX_COLS + key.col];
    return (layer < keymaps_count) ? keymaps[layer][key.row][key.col] : KC_NO;
}

uint8_t keymap_key_to_keycode(uint8_t layer, keypos_t key)
{
    if (layer >= MAX_LAYER || key.col >= MATRIX_COLS || key.row >= MATRIX_ROWS)
        return KC_NO;

    uint8_t kc;
    if (layer == 0 && s_active_base_layer > 0 && s_active_base_layer < MAX_LAYER) {
        kc = read_raw_keymap_byte(s_active_base_layer, key);
        if (kc == KC_TRNS) {
            kc = read_raw_keymap_byte(0, key);
        }
    } else {
        kc = read_raw_keymap_byte(layer, key);
    }
    return apply_channel_swaps(kc);
}
#else
extern const action_t actionmaps[][MATRIX_ROWS][MATRIX_COLS];

action_t action_for_key(uint8_t layer, keypos_t key)
{
    if (layer >= MAX_LAYER || key.col >= MATRIX_COLS || key.row >= MATRIX_ROWS) {
        action_t action = AC_NO;
        return action;
    }
    if (USE_KEYMAP) {
        action_t action;
        uint16_t index = layer * KEYMAP_LAYER_SIZE + key.row * KEYMAP_ROW_SIZE + key.col * 2;
        action.code = UINT16_READ(keymap_block, index);
        return action;
    } else {
        return actionmaps[layer][key.row][key.col];
    }
}
#endif

#ifndef ACTIONMAP_ENABLE
#define FN_BLOCK_SIZE_WORD GET_WORD(MAX_FN_KEYS * 2)
#define FN_RECORD_KEY 0x0495

REGISTER_FDS_BLOCK(fn, FN_BLOCK_SIZE_WORD, FN_RECORD_KEY)

action_t keymap_fn_to_action(uint8_t keycode)
{
    uint8_t fn_idx = FN_INDEX(keycode);
    if (USE_KEYMAP) {
        if (fn_idx >= MAX_FN_KEYS) {
            return (action_t)ACTION_NO;
        }
        uint8_t index = (uint8_t)(fn_idx * 2U);
        uint16_t action = UINT16_READ(fn_block, index);
        return (action_t)action;
    }
    return (fn_idx < fn_actions_count) ? fn_actions[fn_idx] : (action_t)ACTION_NO;
}
#endif
#endif

#ifndef ACTIONMAP_ENABLE
/*
 * macOS F3-F6 keycodes (0xE8-0xEB) mapped in application code via strong
 * override of TMK's weak action_for_key(), keeping the tmk/ submodule clean.
 */
#define LOT60_KC_MISSION_CONTROL    0xE8U
#define LOT60_KC_LAUNCHPAD          0xE9U
#define LOT60_KC_DICTATION          0xEAU
#define LOT60_KC_DO_NOT_DISTURB     0xEBU

#define LOT60_USAGE_VOICE_COMMAND   0x00CFU
#define LOT60_USAGE_MISSION_CONTROL 0x029FU
#define LOT60_USAGE_LAUNCHPAD       0x02A2U
#define LOT60_USAGE_DO_NOT_DISTURB  0x009BU

#ifdef BOOTMAGIC_ENABLE
extern keymap_config_t keymap_config;
#endif

static action_t keycode_to_action_app(uint8_t keycode)
{
    switch (keycode) {
    case KC_A ... KC_EXSEL:
    case KC_LCTRL ... KC_RGUI:
        return (action_t)ACTION_KEY(keycode);
    case KC_SYSTEM_POWER ... KC_SYSTEM_WAKE:
        return (action_t)ACTION_USAGE_SYSTEM(KEYCODE2SYSTEM(keycode));
    case LOT60_KC_DO_NOT_DISTURB:
        return (action_t)ACTION_USAGE_SYSTEM(LOT60_USAGE_DO_NOT_DISTURB);
    case KC_AUDIO_MUTE ... KC_BRIGHTNESS_DEC:
        return (action_t)ACTION_USAGE_CONSUMER(KEYCODE2CONSUMER(keycode));
    case LOT60_KC_MISSION_CONTROL:
        return (action_t)ACTION_USAGE_CONSUMER(LOT60_USAGE_MISSION_CONTROL);
    case LOT60_KC_LAUNCHPAD:
        return (action_t)ACTION_USAGE_CONSUMER(LOT60_USAGE_LAUNCHPAD);
    case LOT60_KC_DICTATION:
        return (action_t)ACTION_USAGE_CONSUMER(LOT60_USAGE_VOICE_COMMAND);
    case KC_MS_UP ... KC_MS_ACCEL2:
        return (action_t)ACTION_MOUSEKEY(keycode);
    case KC_TRNS:
        return (action_t)ACTION_TRANSPARENT;
    case KC_BOOTLOADER:
        return (action_t)ACTION_COMMAND(COMMAND_BOOTLOADER, 0);
    default:
        return (action_t)ACTION_NO;
    }
}

action_t action_for_key(uint8_t layer, keypos_t key)
{
    uint8_t keycode = keymap_key_to_keycode(layer, key);
    switch (keycode) {
    case KC_FN0 ... KC_FN31:
        return keymap_fn_to_action(keycode);
#ifdef BOOTMAGIC_ENABLE
    case KC_CAPSLOCK:
    case KC_LOCKING_CAPS:
        if (keymap_config.swap_control_capslock || keymap_config.capslock_to_control) {
            return keycode_to_action_app(KC_LCTL);
        }
        return keycode_to_action_app(keycode);
    case KC_LCTL:
        if (keymap_config.swap_control_capslock) {
            return keycode_to_action_app(KC_CAPSLOCK);
        }
        return keycode_to_action_app(KC_LCTL);
    case KC_LALT:
        if (keymap_config.swap_lalt_lgui) {
            if (keymap_config.no_gui) {
                return keycode_to_action_app(KC_NO);
            }
            return keycode_to_action_app(KC_LGUI);
        }
        return keycode_to_action_app(KC_LALT);
    case KC_LGUI:
        if (keymap_config.swap_lalt_lgui) {
            return keycode_to_action_app(KC_LALT);
        }
        if (keymap_config.no_gui) {
            return keycode_to_action_app(KC_NO);
        }
        return keycode_to_action_app(KC_LGUI);
    case KC_RALT:
        if (keymap_config.swap_ralt_rgui) {
            if (keymap_config.no_gui) {
                return keycode_to_action_app(KC_NO);
            }
            return keycode_to_action_app(KC_RGUI);
        }
        return keycode_to_action_app(KC_RALT);
    case KC_RGUI:
        if (keymap_config.swap_ralt_rgui) {
            return keycode_to_action_app(KC_RALT);
        }
        if (keymap_config.no_gui) {
            return keycode_to_action_app(KC_NO);
        }
        return keycode_to_action_app(KC_RGUI);
    case KC_GRAVE:
        if (keymap_config.swap_grave_esc) {
            return keycode_to_action_app(KC_ESC);
        }
        return keycode_to_action_app(KC_GRAVE);
    case KC_ESC:
        if (keymap_config.swap_grave_esc) {
            return keycode_to_action_app(KC_GRAVE);
        }
        return keycode_to_action_app(KC_ESC);
    case KC_BSLASH:
        if (keymap_config.swap_backslash_backspace) {
            return keycode_to_action_app(KC_BSPACE);
        }
        return keycode_to_action_app(KC_BSLASH);
    case KC_BSPACE:
        if (keymap_config.swap_backslash_backspace) {
            return keycode_to_action_app(KC_BSLASH);
        }
        return keycode_to_action_app(KC_BSPACE);
#endif
    default:
        return keycode_to_action_app(keycode);
    }
}
#endif

#ifdef MACRO_STORAGE
#define MACRO_BLOCK_SIZE_WORD GET_WORD(MAX_MACRO_SIZE)
#define MACRO_RECORD_KEY 0x0496
#define MACRO_INDEX_CAP 32U

REGISTER_FDS_BLOCK(macro, MACRO_BLOCK_SIZE_WORD, MACRO_RECORD_KEY)

static uint16_t s_macro_offsets[MACRO_INDEX_CAP];
static uint8_t s_macro_count = 0;

static void macro_rebuild_index(void)
{
    macro_block[MAX_MACRO_SIZE - 1] = END;
    s_macro_count = 0;
    s_macro_offsets[s_macro_count++] = 0;

    uint16_t index = 0;
    while (index < MAX_MACRO_SIZE && s_macro_count < MACRO_INDEX_CAP) {
        uint8_t code = macro_block[index++];
        if (code == END) {
            if (index < MAX_MACRO_SIZE) {
                s_macro_offsets[s_macro_count++] = index;
            }
        } else if (code == KEY_DOWN || code == KEY_UP || code == WAIT || code == INTERVAL) {
            if (index < MAX_MACRO_SIZE) {
                index++; // Skip operand byte of 2-byte macro command.
            }
        }
    }
}

const macro_t* action_get_macro(keyrecord_t* record, uint8_t id, uint8_t opt)
{
    (void)opt;
    if (!record->event.pressed)
        return MACRO_NONE;

    if (s_macro_count == 0) {
        macro_rebuild_index();
    }
    if (id < s_macro_count) {
        return &macro_block[s_macro_offsets[id]];
    }
    if (s_macro_count < MACRO_INDEX_CAP) {
        return MACRO_NONE;
    }

    uint16_t index = s_macro_offsets[MACRO_INDEX_CAP - 1U];
    uint8_t rem = (uint8_t)(id - (MACRO_INDEX_CAP - 1U));
    while (index < MAX_MACRO_SIZE) {
        uint8_t code = macro_block[index++];
        if (code == END && --rem == 0) {
            return (index < MAX_MACRO_SIZE) ? &macro_block[index] : MACRO_NONE;
        }
        if (code == KEY_DOWN || code == KEY_UP || code == WAIT || code == INTERVAL) {
            if (index < MAX_MACRO_SIZE) {
                index++; // 2 byte command
            }
        }
    }

    return MACRO_NONE;
}
#endif

#ifdef CONFIG_STORAGE
#define CONFIG_BLOCK_SIZE_WORD 4
#define CONFIG_BLOCK_SIZE_BYTE CONFIG_BLOCK_SIZE_WORD * 4
#define CONFIG_RECORD_KEY 0x0497

REGISTER_FDS_BLOCK(config, CONFIG_BLOCK_SIZE_WORD, CONFIG_RECORD_KEY)
CONFIG_SECTION_DEF();

#define EXT_CONFIG_SIZE_WORD GET_WORD(sizeof(ext_config_t))
#define EXT_CONFIG_RECORD_KEY 0x0498
REGISTER_FDS_BLOCK(ext_config, EXT_CONFIG_SIZE_WORD, EXT_CONFIG_RECORD_KEY)

const uint8_t default_led_colors[LED_COLOR_COUNT][3] = {
    { 0x00, 0xFF, 0xFF }, // LED_SLOT_BLE0: Aqua
    { 0x4B, 0x00, 0xFF }, // LED_SLOT_BLE1: Indigo
    { 0xFF, 0xD7, 0x00 }, // LED_SLOT_BLE2: Gold
    { 0xFF, 0xFF, 0xFF }, // LED_SLOT_IDLE: White
    { 0x00, 0x99, 0xFF }, // LED_SLOT_USB: Blue
    { 0xFF, 0x80, 0x00 }, // LED_SLOT_CHARGING: Orange
    { 0x00, 0xFF, 0x00 }, // LED_SLOT_FULL: Green
    { 0xFF, 0xFF, 0x00 }, // LED_SLOT_PASSKEY_REQ: Yellow
    { 0xFF, 0x00, 0x80 }, // LED_SLOT_PASSKEY_SEND: Pink
    { 0xFF, 0x00, 0xFF }, // LED_SLOT_SLEEP: Magenta
};

static void ext_config_reset_defaults(void)
{
    memset(ext_config_block, 0, sizeof(ext_config_t));
    ext_config_t* cfg = (ext_config_t*)ext_config_block;
    cfg->magic = EXT_CONFIG_MAGIC;
    cfg->version = EXT_CONFIG_VERSION;
    memcpy(cfg->led_colors, default_led_colors, sizeof(cfg->led_colors));
}

ext_config_t* storage_get_ext_config(void)
{
    return (ext_config_t*)ext_config_block;
}

/**
 * @brief 存储初始化，分配指针
 * 
 */
static void config_storage_init()
{
    // 分配指针
    uint8_t cnt = 0;
    for (int i = 0; i < CONFIG_SECTION_COUNT; i++) {
        struct config_section* item = CONFIG_SECTION_GET(i);
        if (cnt + item->len <= CONFIG_BLOCK_SIZE_BYTE) {
            item->data = &config_block[cnt];
            cnt += item->len;
        } else {
            item->data = 0;
        }
    }
}
#endif

enum fds_op_type {
    FDS_OP_UPDATE,
    FDS_OP_WRITE,
};

enum storage_slot {
#ifdef KEYMAP_STORAGE
    SLOT_KEYMAP,
#ifndef ACTIONMAP_ENABLE
    SLOT_FN,
#endif
#endif
#ifdef MACRO_STORAGE
    SLOT_MACRO,
#endif
#ifdef CONFIG_STORAGE
    SLOT_CONFIG,
    SLOT_EXT_CONFIG,
#endif
    STORAGE_SLOT_COUNT
};

struct storage_slot_state {
    uint8_t* ram_buf;
    uint8_t* tx_buf;
    uint16_t size_bytes;
    fds_record_t const* record;
    fds_record_desc_t* record_desc;
    uint16_t record_key;
    bool in_flight;
    bool retry_pending;
    bool has_record;
    enum fds_op_type retry_op;
};

static struct storage_slot_state s_slots[STORAGE_SLOT_COUNT] = {
#ifdef KEYMAP_STORAGE
    [SLOT_KEYMAP] = {
        .ram_buf = keymap_block,
        .tx_buf = keymap_tx_block,
        .size_bytes = sizeof(keymap_block),
        .record = &keymap_record,
        .record_desc = &keymap_record_desc,
        .record_key = KEYMAP_RECORD_KEY,
    },
#ifndef ACTIONMAP_ENABLE
    [SLOT_FN] = {
        .ram_buf = fn_block,
        .tx_buf = fn_tx_block,
        .size_bytes = sizeof(fn_block),
        .record = &fn_record,
        .record_desc = &fn_record_desc,
        .record_key = FN_RECORD_KEY,
    },
#endif
#endif
#ifdef MACRO_STORAGE
    [SLOT_MACRO] = {
        .ram_buf = macro_block,
        .tx_buf = macro_tx_block,
        .size_bytes = sizeof(macro_block),
        .record = &macro_record,
        .record_desc = &macro_record_desc,
        .record_key = MACRO_RECORD_KEY,
    },
#endif
#ifdef CONFIG_STORAGE
    [SLOT_CONFIG] = {
        .ram_buf = config_block,
        .tx_buf = config_tx_block,
        .size_bytes = sizeof(config_block),
        .record = &config_record,
        .record_desc = &config_record_desc,
        .record_key = CONFIG_RECORD_KEY,
    },
    [SLOT_EXT_CONFIG] = {
        .ram_buf = ext_config_block,
        .tx_buf = ext_config_tx_block,
        .size_bytes = sizeof(ext_config_block),
        .record = &ext_config_record,
        .record_desc = &ext_config_record_desc,
        .record_key = EXT_CONFIG_RECORD_KEY,
    },
#endif
};

static bool s_gc_in_flight = false;

static void storage_load_slot_defaults(enum storage_slot slot)
{
    if (slot >= STORAGE_SLOT_COUNT) {
        return;
    }
    switch (slot) {
#ifdef KEYMAP_STORAGE
    case SLOT_KEYMAP: {
        memset(keymap_block, 0, sizeof(keymap_block));
#ifndef ACTIONMAP_ENABLE
        uint8_t layers = keymaps_count > MAX_LAYER ? MAX_LAYER : keymaps_count;
        memcpy(keymap_block, keymaps, (size_t)layers * KEYMAP_LAYER_SIZE);
#endif
        check_keymap();
        break;
    }
#ifndef ACTIONMAP_ENABLE
    case SLOT_FN: {
        uint8_t cnt = fn_actions_count > MAX_FN_KEYS ? MAX_FN_KEYS : fn_actions_count;
        memset(fn_block, 0, sizeof(fn_block));
        for (uint8_t i = 0; i < cnt; i++) {
            uint16_t code = fn_actions[i].code;
            fn_block[i * 2] = (uint8_t)(code & 0xFF);
            fn_block[i * 2 + 1] = (uint8_t)((code >> 8) & 0xFF);
        }
        break;
    }
#endif
#endif
#ifdef MACRO_STORAGE
    case SLOT_MACRO:
        memset(macro_block, 0, sizeof(macro_block));
        macro_rebuild_index();
        break;
#endif
#ifdef CONFIG_STORAGE
    case SLOT_CONFIG:
        memset(config_block, 0, sizeof(config_block));
        store_config_load_defaults();
        break;
    case SLOT_EXT_CONFIG:
        ext_config_reset_defaults();
        break;
#endif
    default:
        break;
    }
}

static void storage_load_defaults_mask(uint8_t mask)
{
#ifdef KEYMAP_STORAGE
    if (mask & 0x01U) {
        storage_load_slot_defaults(SLOT_KEYMAP);
    }
#ifndef ACTIONMAP_ENABLE
    if (mask & 0x02U) {
        storage_load_slot_defaults(SLOT_FN);
    }
#endif
#endif
#ifdef MACRO_STORAGE
    if (mask & 0x04U) {
        storage_load_slot_defaults(SLOT_MACRO);
    }
#endif
#ifdef CONFIG_STORAGE
    if (mask & 0x08U) {
        storage_load_slot_defaults(SLOT_CONFIG);
        storage_load_slot_defaults(SLOT_EXT_CONFIG);
    }
#endif
}

bool storage_degraded(void)
{
    return s_storage_degraded;
}

static void storage_submit_op(enum storage_slot slot, enum fds_op_type op)
{
    if (s_storage_degraded || slot >= STORAGE_SLOT_COUNT) {
        return;
    }

    struct storage_slot_state* s = &s_slots[slot];
    enum fds_op_type actual_op = op;
    bool skip = false;

    CRITICAL_REGION_ENTER();
    if (s->in_flight) {
        s->retry_pending = true;
        s->retry_op = s->has_record ? FDS_OP_UPDATE : FDS_OP_WRITE;
        skip = true;
    } else {
        actual_op = s->has_record ? op : FDS_OP_WRITE;
    }
    CRITICAL_REGION_EXIT();

    if (skip) {
        return;
    }

    if (s->has_record && actual_op == FDS_OP_UPDATE) {
        fds_flash_record_t flash_record = { 0 };
        if (fds_record_open(s->record_desc, &flash_record) == FDS_SUCCESS) {
            bool unchanged = (flash_record.p_header != NULL
                && flash_record.p_header->length_words == s->record->data.length_words
                && memcmp(flash_record.p_data, s->ram_buf, s->size_bytes) == 0);
            (void)fds_record_close(s->record_desc);
            if (unchanged) {
                return;
            }
        }
    }

    // Copy snapshot to dedicated TX buffer and mark in_flight BEFORE FDS call (pre-SD writes fire callback synchronously).
    bool raced = false;
    CRITICAL_REGION_ENTER();
    if (s->in_flight) {
        s->retry_pending = true;
        s->retry_op = s->has_record ? FDS_OP_UPDATE : FDS_OP_WRITE;
        raced = true;
    } else {
        memcpy(s->tx_buf, s->ram_buf, s->size_bytes);
        s->in_flight = true;
    }
    CRITICAL_REGION_EXIT();

    if (raced) {
        return;
    }

    ret_code_t err_code = (actual_op == FDS_OP_UPDATE)
        ? fds_record_update(s->record_desc, s->record)
        : fds_record_write(s->record_desc, s->record);

    if (err_code == FDS_ERR_NOT_FOUND && actual_op == FDS_OP_UPDATE) {
        actual_op = FDS_OP_WRITE;
        CRITICAL_REGION_ENTER();
        s->has_record = false;
        s->in_flight = true;
        CRITICAL_REGION_EXIT();
        err_code = fds_record_write(s->record_desc, s->record);
    }

    if (err_code == FDS_SUCCESS) {
        return;
    }

    bool start_gc = false;
    CRITICAL_REGION_ENTER();
    s->in_flight = false;
    if (err_code == FDS_ERR_NO_SPACE_IN_FLASH) {
        s->retry_pending = true;
        s->retry_op = actual_op;
        if (!s_gc_in_flight) {
            s_gc_in_flight = true;
            start_gc = true;
        }
    } else if (err_code == FDS_ERR_NO_SPACE_IN_QUEUES || err_code == FDS_ERR_BUSY) {
        s->retry_pending = true;
        s->retry_op = actual_op;
    }
    CRITICAL_REGION_EXIT();

    if (start_gc) {
        ret_code_t gc_err = fds_gc();
        if (gc_err != FDS_SUCCESS) {
            CRITICAL_REGION_ENTER();
            s_gc_in_flight = false;
            CRITICAL_REGION_EXIT();
        }
    }
}

static void storage_drain_pending(void)
{
    for (uint8_t i = 0; i < STORAGE_SLOT_COUNT; i++) {
        enum fds_op_type op = FDS_OP_UPDATE;
        bool should_submit = false;
        CRITICAL_REGION_ENTER();
        if (s_slots[i].retry_pending && !s_slots[i].in_flight) {
            op = s_slots[i].retry_op;
            s_slots[i].retry_pending = false;
            should_submit = true;
        }
        CRITICAL_REGION_EXIT();
        if (should_submit) {
            storage_submit_op((enum storage_slot)i, op);
            if (s_slots[i].retry_pending) {
                break;
            }
        }
    }
}

/**
 * @brief FDS操作回调
 * 
 * @param p_evt 
 */
static void storage_callback(fds_evt_t const* p_evt)
{
    switch (p_evt->id) {
    case FDS_EVT_INIT:
        s_fds_init_result = p_evt->result;
        s_fds_init_done = true;
        if (p_evt->result == FDS_SUCCESS) {
            s_fds_initialized = true;
        }
        break;
    case FDS_EVT_GC:
        s_gc_in_flight = false;
        if (p_evt->result == FDS_SUCCESS) {
            storage_drain_pending();
        }
        break;
    case FDS_EVT_UPDATE:
    case FDS_EVT_WRITE:
        if (p_evt->write.file_id == FILE_ID) {
            for (uint8_t i = 0; i < STORAGE_SLOT_COUNT; i++) {
                if (s_slots[i].record_key == p_evt->write.record_key) {
                    CRITICAL_REGION_ENTER();
                    s_slots[i].in_flight = false;
                    if (p_evt->result == FDS_SUCCESS) {
                        s_slots[i].has_record = true;
                    } else if (p_evt->result == FDS_ERR_NOT_FOUND) {
                        s_slots[i].has_record = false;
                        s_slots[i].retry_pending = true;
                        s_slots[i].retry_op = FDS_OP_WRITE;
                    } else if (p_evt->result == FDS_ERR_NO_SPACE_IN_FLASH
                        || p_evt->result == FDS_ERR_NO_SPACE_IN_QUEUES
                        || p_evt->result == FDS_ERR_BUSY
                        || p_evt->result == FDS_ERR_OPERATION_TIMEOUT) {
                        s_slots[i].retry_pending = true;
                        s_slots[i].retry_op = s_slots[i].has_record ? FDS_OP_UPDATE : FDS_OP_WRITE;
                    }
                    CRITICAL_REGION_EXIT();
                    break;
                }
            }
        }
        storage_drain_pending();
        break;
    case FDS_EVT_DEL_RECORD:
    case FDS_EVT_DEL_FILE:
        storage_drain_pending();
        break;
    default:
        break;
    }
}

/**
 * @brief 初始化FDS
 * 
 */
static void storage_callback_init(void)
{
    ret_code_t err_code;
    s_fds_initialized = false;
    s_fds_init_done = false;
    s_fds_init_result = FDS_SUCCESS;
    s_storage_degraded = false;

    (void)fds_register(&storage_callback); // 注册FDS
    err_code = fds_init();                 // 初始化FDS
    if (err_code != FDS_SUCCESS) {
        s_storage_degraded = true;
        return;
    }

    uint32_t spins = 0;
    while (!s_fds_init_done && spins < 500000U) {
        spins++;
#ifdef SOFTDEVICE_PRESENT
        if (nrf_sdh_is_enabled()) {
            sd_app_evt_wait();
        } else
#endif
        {
            __WFE();
        }
    }

    if (!s_fds_init_done || s_fds_init_result != FDS_SUCCESS || !s_fds_initialized) {
        s_storage_degraded = true;
    }
}

/**
 * @brief 内部更新FDS的数据
 * 
 * @param record FDS 记录
 * @param record_desc FDS 记录描述。必须先调用 storage_read_inner 或其他函数生成此描述后才能调用此函数。
 */
/**
 * @brief Internal FDS read; creates record with defaults if missing or unopenable.
 */
static void storage_read_inner(enum storage_slot slot)
{
    if (slot >= STORAGE_SLOT_COUNT) {
        return;
    }
    struct storage_slot_state* s = &s_slots[slot];
    fds_find_token_t ftok = { 0 };
    fds_flash_record_t flash_record = { 0 };
    bool record_open = false;

    if (fds_record_find(s->record->file_id, s->record->key, s->record_desc, &ftok) == FDS_SUCCESS) {
        if (fds_record_open(s->record_desc, &flash_record) == FDS_SUCCESS && flash_record.p_header != NULL) {
            record_open = true;
            s->has_record = true;
            if (flash_record.p_header->length_words == s->record->data.length_words) {
                memcpy(s->ram_buf, flash_record.p_data, s->size_bytes);
                (void)fds_record_close(s->record_desc);
#ifdef CONFIG_STORAGE
            } else if (slot == SLOT_EXT_CONFIG && flash_record.p_header->length_words == EXT_CONFIG_V1_WORDS) {
                // Migrate v1 ext_config (24 bytes) to v2 (56 bytes) while preserving custom_name and channel_profile.
                memset(ext_config_block, 0, sizeof(ext_config_t));
                memcpy(ext_config_block, flash_record.p_data, EXT_CONFIG_V1_WORDS * 4);
                (void)fds_record_close(s->record_desc);
                ext_config_t* ext = (ext_config_t*)ext_config_block;
                if (ext->magic == EXT_CONFIG_MAGIC && ext->version == 1 && ext->name_len <= MAX_CUSTOM_NAME_BYTES) {
                    ext->version = EXT_CONFIG_VERSION;
                    memcpy(ext->led_colors, default_led_colors, sizeof(ext->led_colors));
                } else {
                    ext_config_reset_defaults();
                }
                storage_submit_op(slot, FDS_OP_UPDATE);
#endif
            } else {
                (void)fds_record_close(s->record_desc);
                storage_submit_op(slot, FDS_OP_UPDATE);
            }
        }
    }

    if (!record_open) {
        // Missing record or open failure (H4: do not call fds_record_delete): load defaults first, then write.
        s->has_record = false;
        storage_load_slot_defaults(slot);
        storage_submit_op(slot, FDS_OP_WRITE);
    }
}

/**
 * @brief 读取存储的记录并写到内存（重置内存中的数据）
 * 
 * @param mask 要读取的记录mask。1: keymap, 2: fn, 4: macro, 8: config
 */
void storage_read(uint8_t mask)
{
    if (s_storage_degraded) {
        storage_load_defaults_mask(mask);
        return;
    }

    bool loaded_default_keymap = false;
#ifdef KEYMAP_STORAGE
    if (mask & 0x01) {
        storage_read_inner(SLOT_KEYMAP);
        check_keymap();
#ifndef ACTIONMAP_ENABLE
        if (!keymap_valid) {
            storage_load_slot_defaults(SLOT_KEYMAP);
            loaded_default_keymap = true;
        }
#endif
    }
#endif
#if defined(KEYMAP_STORAGE) && !defined(ACTIONMAP_ENABLE)
    if (mask & 0x02) {
        storage_read_inner(SLOT_FN);
        if (loaded_default_keymap) {
            storage_load_slot_defaults(SLOT_FN);
        }
    }
#endif
#ifdef MACRO_STORAGE
    if (mask & 0x04) {
        storage_read_inner(SLOT_MACRO);
        macro_rebuild_index();
    }
#endif
#ifdef CONFIG_STORAGE
    if (mask & 0x08) {
        storage_read_inner(SLOT_CONFIG);
        store_config_load_defaults();
        storage_read_inner(SLOT_EXT_CONFIG);
        ext_config_t* ext = (ext_config_t*)ext_config_block;
        if (ext->magic != EXT_CONFIG_MAGIC || ext->version != EXT_CONFIG_VERSION
            || ext->name_len > MAX_CUSTOM_NAME_BYTES) {
            ext_config_reset_defaults();
        }
    }
#endif
}

/**
 * @brief 删除存储记录中的数据
 * 
 * @param mask 
 */
void storage_delete(uint8_t mask)
{
    if (s_storage_degraded) {
        storage_load_defaults_mask(mask);
        return;
    }

#ifdef KEYMAP_STORAGE
    if (mask & 0x01) {
        CRITICAL_REGION_ENTER();
        s_slots[SLOT_KEYMAP].retry_pending = false;
        s_slots[SLOT_KEYMAP].has_record = false;
        CRITICAL_REGION_EXIT();
        (void)fds_record_delete(&keymap_record_desc);
    }
#endif
#if defined(KEYMAP_STORAGE) && !defined(ACTIONMAP_ENABLE)
    if (mask & 0x02) {
        CRITICAL_REGION_ENTER();
        s_slots[SLOT_FN].retry_pending = false;
        s_slots[SLOT_FN].has_record = false;
        CRITICAL_REGION_EXIT();
        (void)fds_record_delete(&fn_record_desc);
    }
#endif
#ifdef MACRO_STORAGE
    if (mask & 0x04) {
        CRITICAL_REGION_ENTER();
        s_slots[SLOT_MACRO].retry_pending = false;
        s_slots[SLOT_MACRO].has_record = false;
        CRITICAL_REGION_EXIT();
        (void)fds_record_delete(&macro_record_desc);
    }
#endif
#ifdef CONFIG_STORAGE
    if (mask & 0x08) {
        CRITICAL_REGION_ENTER();
        s_slots[SLOT_CONFIG].retry_pending = false;
        s_slots[SLOT_CONFIG].has_record = false;
        s_slots[SLOT_EXT_CONFIG].retry_pending = false;
        s_slots[SLOT_EXT_CONFIG].has_record = false;
        CRITICAL_REGION_EXIT();
        (void)fds_record_delete(&config_record_desc);
        (void)fds_record_delete(&ext_config_record_desc);
        ext_config_reset_defaults();
    }
#endif
}

/**
 * @brief 初始化存储模块并读取记录
 * 
 */
void storage_init()
{
    storage_callback_init();
#ifdef CONFIG_STORAGE
    // 初始化配置分片
    config_storage_init();
#endif
    storage_read(0xFF);
}

/**
 * @brief 写存储模块记录
 * 
 * @param mask 要写出的记录mask。1: keymap, 2: fn, 4: macro, 8: config
 * @return true 操作成功
 * @return false 含有未定义的操作或处于降级模式
 */
bool storage_write(uint8_t mask)
{
    if (s_storage_degraded) {
        return false;
    }

    bool success = true;

#ifdef KEYMAP_STORAGE
    if (mask & 0x01) {
        storage_submit_op(SLOT_KEYMAP, FDS_OP_UPDATE);
    }
#endif
#if defined(KEYMAP_STORAGE) && !defined(ACTIONMAP_ENABLE)
    if (mask & 0x02) {
        storage_submit_op(SLOT_FN, FDS_OP_UPDATE);
    }
#endif
#ifdef MACRO_STORAGE
    if (mask & 0x04) {
        storage_submit_op(SLOT_MACRO, FDS_OP_UPDATE);
    }
#endif
#ifdef CONFIG_STORAGE
    if (mask & 0x08) {
        storage_submit_op(SLOT_CONFIG, FDS_OP_UPDATE);
        storage_submit_op(SLOT_EXT_CONFIG, FDS_OP_UPDATE);
    }
#endif

    return success;
}

/**
 * @brief 获取指定类型的内存区域的指针和大小
 * 
 * @param type 存储类型
 * @param pointer 目标指针
 * @return uint16_t 存储空间大小。0则表示不支持。
 */
static uint16_t storage_get_data_pointer(enum storage_type type, uint8_t** pointer)
{
    switch (type) {
#ifdef KEYMAP_STORAGE
    case STORAGE_KEYMAP:
        *pointer = keymap_block;
        return KEYMAP_SIZE_WORD * 4;
        break;
#endif
#if defined(KEYMAP_STORAGE) && !defined(ACTIONMAP_ENABLE)
    case STORAGE_FN:
        *pointer = fn_block;
        return FN_BLOCK_SIZE_WORD * 4;
        break;
#endif
#ifdef MACRO_STORAGE
    case STORAGE_MACRO:
        *pointer = macro_block;
        return MACRO_BLOCK_SIZE_WORD * 4;
        break;
#endif
#ifdef CONFIG_STORAGE
    case STORAGE_CONFIG:
        *pointer = config_block;
        return CONFIG_BLOCK_SIZE_WORD * 4;
        break;
#endif
    default:
        return 0;
    }
}

/**
 * @brief 读取内存中的数据
 * 
 * @param type 数据类型 0: keymap, 1: fn, 2: macro, 3: config
 * @param offset 数据偏移量
 * @param len 数据长度
 * @param data 目标指针
 * @return uint16_t 读取实际长度
 */
uint16_t storage_read_data(enum storage_type type, uint16_t offset, uint16_t len, uint8_t* data)
{
    uint8_t* pointer = 0;
    uint16_t size = storage_get_data_pointer(type, &pointer);

    if (pointer == 0 || data == NULL || offset >= size)
        return 0;

    if (len > size - offset)
        len = size - offset;

    memcpy(data, &pointer[offset], len);
    return len;
}

/**
 * @brief 将数据写到内存中
 * 
 * @param type 数据类型 0: keymap, 1: fn, 2: macro, 3: config
 * @param offset 数据偏移量
 * @param len 数据长度
 * @param data 数据指针
 * @return uint16_t 实际写入长度
 */
uint16_t storage_write_data(enum storage_type type, uint16_t offset, uint16_t len, uint8_t* data)
{
    uint8_t* pointer = 0;
    uint16_t size = storage_get_data_pointer(type, &pointer);

    if (pointer == 0 || data == NULL || offset >= size)
        return 0;

    if (len > size - offset)
        len = size - offset;

    memcpy(&pointer[offset], data, len);

#ifdef KEYMAP_STORAGE
    if (type == STORAGE_KEYMAP) {
        bool chunk_non_zero = false;
        for (uint16_t i = 0; i < len; i++) {
            if (data[i] != 0U) {
                chunk_non_zero = true;
                break;
            }
        }
        if (chunk_non_zero) {
            keymap_valid = true;
        } else {
            check_keymap();
        }
    }
#endif
#ifdef MACRO_STORAGE
    if (type == STORAGE_MACRO) {
        macro_rebuild_index();
    }
#endif

    return len;
}

