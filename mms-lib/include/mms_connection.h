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

#ifndef JOLLA_MMS_CONNECTION_H
#define JOLLA_MMS_CONNECTION_H

#include "mms_lib_types.h"

/* Connection state. There are only two state change sequences allowed:
 * OPENING -> FAILED and OPENING -> OPEN -> CLOSE. Once connection fails
 * to open or gets closed, it will remain on FAILED or CLOSED state forever.
 * The client should drop its reference to the closed or failed connection
 * and open a new one when it needs it. */
typedef enum _MMS_CONNECTION_STATE {
    MMS_CONNECTION_STATE_INVALID,       /* Invalid state */
    MMS_CONNECTION_STATE_OPENING,       /* Connection is being opened */
    MMS_CONNECTION_STATE_FAILED,        /* Connection failed to open */
    MMS_CONNECTION_STATE_OPEN,          /* Connection is active */
    MMS_CONNECTION_STATE_CLOSED         /* Connection has been closed */
} MMS_CONNECTION_STATE;

/* Delegate (one per connection) */
typedef struct mms_connection_delegate MMSConnectionDelegate;
struct mms_connection_delegate {
    void (*fn_connection_state_changed)(
        MMSConnectionDelegate* delegate,
        MMSConnection* connection);
};

/* Connection data. The delegate field may be changed by the client at
 * any time. */
struct mms_connection {
    GObject parent;
    char* imsi;
    char* mmsc;
    char* mmsproxy;
    char* netif;
    gboolean user_connection;
    MMS_CONNECTION_STATE state;
    MMSConnectionDelegate* delegate;
};

/* Connection class for implementation */
typedef struct mms_connection_class {
    GObjectClass parent;
    void (*fn_close)(MMSConnection* connection);
} MMSConnectionClass;

GType mms_connection_get_type(void);
#define MMS_TYPE_CONNECTION (mms_connection_get_type())

MMSConnection*
mms_connection_ref(
    MMSConnection* connection);

void
mms_connection_unref(
    MMSConnection* connection);

const char*
mms_connection_state_name(
    MMSConnection* connection);

MMS_CONNECTION_STATE
mms_connection_state(
    MMSConnection* connection);

void
mms_connection_close(
    MMSConnection* connection);

#define mms_connection_is_open(connection) \
    (mms_connection_state(connection) == MMS_CONNECTION_STATE_OPEN)

#endif /* JOLLA_MMS_CONNECTION_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
