/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "mms_util.h"
#include "mms_codec.h"

/**
 * Strips leading spaces and "/TYPE=" suffix from the string.
 */
char*
mms_strip_address_type(
    char* address)
{
    if (address) {
        char* type = g_strrstr(g_strstrip(address), MMS_ADDRESS_TYPE_SUFFIX);
        if (type) *type = 0;
    }
    return address;
}

/**
 * Splits comma-separated list of addresses into an array of string pointers.
 * Strips "/TYPE=" suffix from each address. Caller needs to deallocate the
 * returned list with g_strfreev.
 */
char**
mms_split_address_list(
    const char* addres_list)
{
    char** list = NULL;
    if (addres_list && addres_list[0]) {
        int i;
        list = g_strsplit(addres_list, ",", 0);
        for (i=0; list[i]; i++) {
            list[i] = mms_strip_address_type(list[i]);
        }
    } else {
        list = g_new(char*, 1);
        list[0] = NULL;
    }
    return list;
}

/**
 * Removes leading and trailing whitespaces from the address and appends
 * the address type if necessary (defaults to phone number). In any case
 * the caller must deallocate the returned string. Returns NULL if the
 * input string is either NULL or empty.
 */
char*
mms_address_normalize(
    const char* address)
{
    if (address) {
        gssize len;

        /* Skip leading whitespaces */
        while (*address && g_ascii_isspace(*address)) address++;

        /* Calculate the length without traling whitespaces */
        len = strlen(address);
        while (len > 0 && g_ascii_isspace(address[len-1])) len--;
        if (len > 0) {

            /* Append the address type if necessary */
            char* out;
            if (g_strrstr_len(address, len, MMS_ADDRESS_TYPE_SUFFIX)) {
                out = g_strndup(address, len);
            } else {
                out = g_new(char, len+sizeof(MMS_ADDRESS_TYPE_SUFFIX_PHONE));
                strncpy(out, address, len);
                strcpy(out+len, MMS_ADDRESS_TYPE_SUFFIX_PHONE);
            }
            return out;
        }
    }
    return NULL;
}

/**
 * Allocates and decodes WAP push PDU. Returns NULL if decoding fails.
 */
MMSPdu*
mms_decode_bytes(
    GBytes* bytes)
{
    MMSPdu* pdu = NULL;
    if (bytes) {
        gsize len = 0;
        const guint8* data = g_bytes_get_data(bytes, &len);
        pdu = g_new0(MMSPdu, 1);
        if (!mms_message_decode(data, len, pdu)) {
            mms_message_free(pdu);
            pdu = NULL;
        }
    }
    return pdu;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
