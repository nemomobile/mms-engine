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

#ifndef JOLLA_MMS_OFONO_MANAGER_H
#define JOLLA_MMS_OFONO_MANAGER_H

#include "mms_ofono_types.h"

MMSOfonoManager*
mms_ofono_manager_new(
    GDBusConnection* bus);

void
mms_ofono_manager_free(
    MMSOfonoManager* ofono);

MMSOfonoModem*
mms_ofono_manager_modem_for_imsi(
    MMSOfonoManager* ofono,
    const char* imsi);

#endif /* JOLLA_MMS_OFONO_MANAGER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
