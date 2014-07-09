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

#ifndef JOLLA_MMS_OFONO_MODEM_H
#define JOLLA_MMS_OFONO_MODEM_H

#include "mms_ofono_context.h"

struct mms_ofono_modem {
    GDBusConnection* bus;
    char* path;
    gboolean online;

    struct _OrgOfonoModem* proxy;
    gulong property_change_signal_id;

    struct _OrgOfonoSimManager* sim_proxy;
    gulong sim_property_change_signal_id;
    char* imsi;

    struct _OrgOfonoConnectionManager* gprs_proxy;
    gulong gprs_context_added_signal_id;
    gulong gprs_context_removed_signal_id;

    MMSOfonoContext* mms_context;
};

MMSOfonoModem*
mms_ofono_modem_new(
    GDBusConnection* bus,
    const char* path,
    GVariant* properties);

void
mms_ofono_modem_free(
    MMSOfonoModem* modem);

#endif /* JOLLA_MMS_OFONO_MODEM_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
