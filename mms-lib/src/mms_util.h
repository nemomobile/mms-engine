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

#ifndef JOLLA_MMS_UTIL_H
#define JOLLA_MMS_UTIL_H

#include "mms_lib_types.h"

char*
mms_strip_address_type(
    char* address);

char**
mms_split_address_list(
    const char* addres_list);

char*
mms_address_normalize(
    const char* address);

MMSPdu*
mms_decode_bytes(
    GBytes* bytes);

/* NULL-resistant variant of g_strstrip */
G_INLINE_FUNC char* mms_strip(char* str)
    { return str ? g_strstrip(str) : NULL; }

/* Address type suffices */
#define MMS_ADDRESS_TYPE_SUFFIX         "/TYPE="
#define MMS_ADDRESS_TYPE_SUFFIX_PHONE   MMS_ADDRESS_TYPE_SUFFIX "PLMN"

#endif /* JOLLA_MMS_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
