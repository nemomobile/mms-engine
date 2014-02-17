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

#ifndef JOLLA_MMS_OFONO_NAMES_H
#define JOLLA_MMS_OFONO_NAMES_H

#define OFONO_SERVICE                   "org.ofono"

#define OFONO_MANAGER_INTERFACE         OFONO_SERVICE ".Manager"
#define OFONO_MODEM_INTERFACE           OFONO_SERVICE ".Modem"
#define OFONO_SIM_INTERFACE             OFONO_SERVICE ".SimManager"
#define OFONO_GPRS_INTERFACE            OFONO_SERVICE ".ConnectionManager"
#define OFONO_CONTEXT_INTERFACE         OFONO_SERVICE ".ConnectionContext"

#define OFONO_MODEM_PROPERTY_INTERFACES        "Interfaces"
#define OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY "SubscriberIdentity"

#define OFONO_CONTEXT_PROPERTY_ACTIVE          "Active"
#define OFONO_CONTEXT_PROPERTY_MMS_PROXY       "MessageProxy"
#define OFONO_CONTEXT_PROPERTY_MMS_CENTER      "MessageCenter"

#define OFONO_CONTEXT_PROPERTY_SETTINGS        "Settings"
#define OFONO_CONTEXT_SETTING_INTERFACE        "Interface"

#define OFONO_CONTEXT_PROPERTY_TYPE            "Type"
#define OFONO_CONTEXT_TYPE_MMS                 "mms"

#endif /* JOLLA_MMS_OFONO_NAMES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
