/**
 *
 * Microvisor Config Handler
 * Version 1.1.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */


#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"


/*
 * MACROS
 */
#define STRING_ITEM(name, str) \
    . name = { .data = (uint8_t*)str, . length = strlen(str) }


/*
 * DEFINES
 */
#define TAG_CHANNEL_CONFIG 100

#define BUF_READ_BUFFER 3*1024


#ifdef __cplusplus
extern "C" {
#endif


/*
 * TYPES
 */
enum ConfigItemType {
    CONFIG_ITEM_TYPE_UINT8           = 0x0, //< uint8_t buffer reception; populates into .u8_item
    CONFIG_ITEM_TYPE_B64             = 0x1, //< uint8_t buffer reception, but b64 decode received bytes; populates into .u8_item
    CONFIG_ITEM_TYPE_ULONG           = 0x2, //< uint16_t value reception; populates into .ulong_item
    CONFIG_ITEM_TYPE_LONG            = 0x3, //< int16_t value reception; populates into .long_item
};

struct ConfigHelperItem {
    enum ConfigItemType config_type;
    struct MvConfigKeyToFetch item;
    union {
        struct {
            uint8_t *buf; // pointer to the buffer
            size_t buf_size; // size of allocated buffer
            size_t *buf_len; // pointer to populate length of data read into
        } u8_item;

        struct {
            uint16_t *val;
        } ulong_item;

        struct {
            int16_t *val;
        } long_item;
    };
};

/*
 * PROTOTYPES
 */
void start_configuration_fetch(const struct ConfigHelperItem *items, uint8_t count);
void receive_configuration_items(struct ConfigHelperItem *items, uint8_t count);
void finish_configuration_fetch();


#ifdef __cplusplus
}
#endif


#endif /* CONFIG_HANDLER_H */
