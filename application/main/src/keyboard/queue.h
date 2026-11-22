#pragma once

#include <stdbool.h>

#define QUEUE(TYPE, NAME, SIZE)                                          \
    static TYPE NAME[SIZE];                                              \
    static const int NAME##_size = SIZE;                                 \
    static int NAME##_front = 0, NAME##_rear = 0;                        \
    static bool NAME##_full = false;                                     \
    static inline void NAME##_pop(void)                                  \
    {                                                                    \
        if (NAME##_full || NAME##_front != NAME##_rear) {                \
            NAME##_rear = (NAME##_rear + 1) % NAME##_size;               \
            NAME##_full = false;                                         \
        }                                                                \
    }                                                                    \
    static inline bool NAME##_push(TYPE item)                            \
    {                                                                    \
        if (NAME##_full) {                                               \
            return false;                                                \
        }                                                                \
        NAME[NAME##_front] = item;                                       \
        NAME##_front = (NAME##_front + 1) % NAME##_size;                 \
        if (NAME##_front == NAME##_rear) {                               \
            NAME##_full = true;                                          \
        }                                                                \
        return true;                                                     \
    }                                                                    \
    static inline bool NAME##_empty(void)                                \
    {                                                                    \
        return (!NAME##_full && NAME##_front == NAME##_rear);            \
    }                                                                    \
    static inline TYPE* NAME##_peek(void)                                \
    {                                                                    \
        if (NAME##_empty())                                              \
            return 0;                                                    \
        return &NAME[NAME##_rear];                                       \
    }                                                                    \
    static inline void NAME##_clear(void)                                \
    {                                                                    \
        NAME##_front = 0;                                                \
        NAME##_rear = 0;                                                 \
        NAME##_full = false;                                             \
    }

/**
 * @brief 可变长度的队列
 * 
 */
#define MIXED_QUEUE(TYPE, NAME, SIZE)                                                                   \
    static uint16_t NAME##_head, NAME##_tail;                                                           \
    static uint8_t NAME##_data[SIZE];                                                                   \
    static inline bool NAME##_empty(void)                                                               \
    {                                                                                                   \
        return NAME##_tail == NAME##_head;                                                              \
    }                                                                                                   \
                                                                                                        \
    static inline uint16_t NAME##_free_space(void)                                                      \
    {                                                                                                   \
        uint16_t used = (NAME##_tail >= NAME##_head)                                                    \
            ? (uint16_t)(NAME##_tail - NAME##_head)                                                     \
            : (uint16_t)((SIZE) - NAME##_head + NAME##_tail);                                           \
        return (uint16_t)((SIZE) - 1U - used);                                                          \
    }                                                                                                   \
                                                                                                        \
    static inline void NAME##_dequeue(void)                                                             \
    {                                                                                                   \
        if (NAME##_empty())                                                                             \
            return;                                                                                     \
        uint8_t len = NAME##_data[NAME##_head];                                                         \
        NAME##_head = (NAME##_head + len + 1) % SIZE;                                                   \
    }                                                                                                   \
                                                                                                        \
    static inline uint8_t NAME##_peek(uint8_t* pointer)                                                 \
    {                                                                                                   \
        uint8_t len = NAME##_data[NAME##_head];                                                         \
        if (len + NAME##_head >= SIZE) {                                                                \
            uint8_t len1 = SIZE - NAME##_head - 1;                                                      \
            uint8_t len2 = len - len1;                                                                  \
            if (len1 > 0)                                                                               \
                memcpy(pointer, &NAME##_data[NAME##_head + 1], len1);                                   \
            memcpy(&pointer[len1], NAME##_data, len2);                                                  \
        } else {                                                                                        \
            memcpy(pointer, &NAME##_data[NAME##_head + 1], len);                                        \
        }                                                                                               \
        return len;                                                                                     \
    }                                                                                                   \
                                                                                                        \
    static inline bool NAME##_enqueue(uint8_t len, uint8_t* data)                                       \
    {                                                                                                   \
        if ((uint16_t)(len + 1U) > NAME##_free_space()) {                                               \
            return false;                                                                               \
        }                                                                                               \
        uint16_t next_tail = (uint16_t)(NAME##_tail + len + 1U);                                        \
        NAME##_data[NAME##_tail] = len;                                                                 \
        if (next_tail > SIZE) {                                                                         \
            uint8_t len1 = SIZE - NAME##_tail - 1;                                                      \
            uint8_t len2 = len - len1;                                                                  \
            if (len1 > 0)                                                                               \
                memcpy(&NAME##_data[NAME##_tail + 1], data, len1);                                      \
            memcpy(NAME##_data, &data[len1], len2);                                                     \
        } else {                                                                                        \
            memcpy(&NAME##_data[NAME##_tail + 1], data, len);                                           \
        }                                                                                               \
        NAME##_tail = next_tail % SIZE;                                                                 \
        return true;                                                                                    \
    }                                                                                                   \
                                                                                                        \
    static inline void NAME##_clear(void)                                                               \
    {                                                                                                   \
        NAME##_tail = 0;                                                                                \
        NAME##_head = 0;                                                                                \
    }
