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

#ifndef JOLLA_MMS_OFONO_CONTEXT_H
#define JOLLA_MMS_OFONO_CONTEXT_H

#include "mms_ofono_types.h"

struct mms_ofono_context {
    MMSOfonoModem* modem;
    char* path;
    gboolean active;
    struct _OrgOfonoConnectionContext* proxy;
    gulong property_change_signal_id;
    MMSOfonoConnection* connection;
    GCancellable* set_active_cancel;
};

MMSOfonoContext*
mms_ofono_context_new(
    MMSOfonoModem* modem,
    const char* path,
    GVariant* properties);

void
mms_ofono_context_set_active(
    MMSOfonoContext* context,
    gboolean active);

void
mms_ofono_context_free(
    MMSOfonoContext* context);

#endif /* JOLLA_MMS_OFONO_CONTEXT_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
