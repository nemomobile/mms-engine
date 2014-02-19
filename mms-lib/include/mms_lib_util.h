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

#ifndef JOLLA_MMS_LIB_UTIL_H
#define JOLLA_MMS_LIB_UTIL_H

#include "mms_lib_types.h"

/* Error reporting */
GQuark mms_lib_error_quark(void);
#define MMS_LIB_ERROR mms_lib_error_quark()
typedef enum {
    MMS_LIB_ERROR_ENCODE,
    MMS_LIB_ERROR_DECODE,
    MMS_LIB_ERROR_IO,
    MMS_LIB_ERROR_EXPIRED,
    MMS_LIB_ERROR_NOSIM,
    MMS_LIB_ERROR_ARGS
} MMSLibError;

/* One-time initialization */
void
mms_lib_init(void);

/* Reset configuration to default */
void
mms_lib_default_config(
    MMSConfig* config);

#endif /* JOLLA_MMS_LIB_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
