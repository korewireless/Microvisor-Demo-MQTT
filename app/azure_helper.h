/**
 *
 * Microvisor Azure Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#ifndef AZURE_HELPER_H
#define AZURE_HELPER_H


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TYPES
 */

struct AzureConnectionStringParams {
    uint8_t hostname[128];
    size_t  hostname_len;
    uint8_t device_id[128];
    size_t  device_id_len;
    uint8_t shared_access_key[128];
    size_t  shared_access_key_len;
};

/*
 * PROTOTYPES
 */
bool parse_azure_connection_string(uint8_t *connection_string, size_t connection_string_len, struct AzureConnectionStringParams *dest);
bool populate_azure_username(const struct AzureConnectionStringParams *params, uint8_t *username, size_t *username_len, const size_t username_size);
bool generate_azure_password(const struct AzureConnectionStringParams *params, uint8_t *password, size_t *password_len, const size_t password_size, const time_t expiry);

#ifdef __cplusplus
}
#endif


#endif /* AZURE_HELPER_H */
