/*
 * Copyright (C) 2013-2014 Jolla Ltd.
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
