/**
 *
 * Microvisor Azure Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#include <ctype.h>
#include "azure_helper.h"
#include "log_helper.h"
#include "base64.h"
#include "sha2.h"
#include "hmac.h"

bool parse_azure_connection_string(uint8_t *connection_string, size_t connection_string_len, struct AzureConnectionStringParams *dest) {
    char hostname[128] = { 0 };
    char device_id[128] = { 0 };
    char shared_access_key_b64[128] = { 0 };

    int ret = sscanf((char*)connection_string, "HostName=%127[^;];DeviceId=%127[^;];SharedAccessKey=%127[^;]", hostname, device_id, shared_access_key_b64);
    if (ret != 3) {
        server_error("encountered an error parsing connection string - parsed %d values", ret);
        return false;
    }
    dest->hostname_len = strlen(hostname);
    memcpy(dest->hostname, hostname, dest->hostname_len);
    dest->device_id_len = strlen(device_id);
    memcpy(dest->device_id, device_id, dest->device_id_len);

    dest->shared_access_key_len = owl_base64decode((unsigned char *)dest->shared_access_key, shared_access_key_b64);

    return true;
}

bool populate_azure_username(const struct AzureConnectionStringParams *params, uint8_t *username, size_t *username_len, const size_t username_size) {
    char tmp[params->hostname_len + params->device_id_len + 2];
    sprintf(tmp, "%.*s/%.*s", 
            (unsigned)params->hostname_len, params->hostname,
            (unsigned)params->device_id_len, params->device_id);

    if (strlen(tmp) > username_size) {
        return false;
    }

    *username_len = strlen(tmp);
    memcpy(username, tmp, *username_len);
   
    return true;
}

// target buffer must be 3x size of src + 1 to be safe
void url_encode(char *dest, const char *src) {
    while (*src) {
        if ((unsigned)*src < 0x80
                && (isalnum((unsigned)*src) || *src == '-' || *src == '.' || *src == '_' || *src == '~')) {
            *dest++ = *src;
        } else {
            char s[4];
            unsigned char c = (unsigned char)*src;
            sprintf(s, "%%%02X", c);
            memcpy(dest, s, 3);
            dest+=3;
        }
        ++src;
    }
    *dest = '\0'; // null terminate the result
}

bool generate_azure_password(const struct AzureConnectionStringParams *params, uint8_t *password, size_t *password_len, const size_t password_size, const time_t expiry) {
    const size_t resource_size = 9 + params->hostname_len + params->device_id_len + 1;
    char resource[resource_size];
    bzero(resource, sizeof(resource));
    char resource_encoded[resource_size*3];
    bzero(resource_encoded, sizeof(resource_encoded));

    const cf_chash *hash = &cf_sha256;
    char message[resource_size+16];
    bzero(message, sizeof(message));
    uint8_t signature[CF_MAXHASH];

    // build resource string
    sprintf(resource, "%.*s/devices/%.*s", 
            (unsigned)params->hostname_len, params->hostname,
            (unsigned)params->device_id_len, params->device_id);
    url_encode(resource_encoded, resource);

    // build hmac sha256 signature
    sprintf(message, "%s\n%u", resource_encoded, (unsigned)expiry);
    cf_hmac(params->shared_access_key, params->shared_access_key_len,
            (uint8_t *)message, strlen(message),
            signature,
            hash);

    *password_len = owl_base64encode_len(hash->hashsz);
    const size_t signature_size = *password_len + 1;
    char signature_b64[signature_size];
    bzero(signature_b64, sizeof(signature_b64));
    char signature_b64_encoded[signature_size*3];
    bzero(signature_b64_encoded, sizeof(signature_b64_encoded));
    owl_base64encode(signature_b64, (const unsigned char *)signature, hash->hashsz);
    url_encode(signature_b64_encoded, signature_b64);

    // compose SAS authorization
    char tmp[resource_size+signature_size+64];
    bzero(tmp, sizeof(tmp));
    sprintf(tmp, "SharedAccessSignature sr=%s&sig=%s&se=%u", resource_encoded, signature_b64_encoded, (unsigned)expiry);

    if (strlen(tmp) > password_size) {
        server_error("target buffer not large enough to receive SAS token");
        return false;
    }
    *password_len = strlen(tmp);
    memcpy(password, tmp, *password_len);

    return true;
}
