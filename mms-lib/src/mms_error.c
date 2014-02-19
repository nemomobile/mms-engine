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

#include "mms_log.h"
#include "mms_error.h"

void
mms_error_valist(
    const MMSLogModule* module,
    GError** error,
    MMSLibError code,
    const char* format,
    va_list args)
{
    if (error) {
        va_list args2;
        G_VA_COPY(args2, args);
        g_propagate_error(error,
        g_error_new_valist(MMS_LIB_ERROR, code, format, args2));
        va_end(args2);
    }
    mms_logv(module, MMS_LOGLEVEL_ERR, format, args);
}

void
mms_error(
    const MMSLogModule* module,
    GError** error,
    MMSLibError code,
    const char* format,
    ...)
{
    va_list args;
    va_start(args, format);
    mms_error_valist(module, error, code, format, args);
    va_end(args);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
