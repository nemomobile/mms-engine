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

#ifndef JOLLA_MMS_CONNMAN_H
#define JOLLA_MMS_CONNMAN_H

#include "mms_lib_types.h"

typedef struct mms_connman_class {
    GObjectClass parent;
    MMSConnection* (*fn_open_connection)(MMSConnMan* cm, const char* imsi,
        gboolean user_request);
} MMSConnManClass;

GType mms_connman_get_type(void);
#define MMS_TYPE_CONNMAN (mms_connman_get_type())

/* Reference counting */
MMSConnMan*
mms_connman_ref(
    MMSConnMan* cm);

void
mms_connman_unref(
    MMSConnMan* cm);

/**
 * Creates a new connection or returns the reference to an aready active one.
 * The caller must release the reference.
 */
MMSConnection*
mms_connman_open_connection(
    MMSConnMan* cm,
    const char* imsi,
    gboolean user_request);

#endif /* JOLLA_MMS_CONNMAN_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
