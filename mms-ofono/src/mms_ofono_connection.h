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

#ifndef JOLLA_MMS_CONNECTION_OFONO_H
#define JOLLA_MMS_CONNECTION_OFONO_H

#include "mms_connection.h"
#include "mms_ofono_types.h"

struct mms_ofono_connection {
    MMSConnection connection;
    MMSOfonoContext* context;
    struct _OrgOfonoConnectionContext* proxy;
    gulong property_change_signal_id;
};

MMSOfonoConnection*
mms_ofono_connection_new(
    MMSOfonoContext* context,
    gboolean user_request);

MMSOfonoConnection*
mms_ofono_connection_ref(
    MMSOfonoConnection* connection);

void
mms_ofono_connection_unref(
    MMSOfonoConnection* connection);

void
mms_ofono_connection_cancel(
    MMSOfonoConnection* connection);

gboolean
mms_ofono_connection_set_state(
    MMSOfonoConnection* connection,
    MMS_CONNECTION_STATE state);

#endif /* JOLLA_MMS_CONNECTION_OFONO_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
