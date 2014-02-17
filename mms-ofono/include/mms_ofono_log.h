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

#ifndef JOLLA_MMS_OFONO_LOG_H
#define JOLLA_MMS_OFONO_LOG_H

#include "mms_log.h"

#define MMS_OFONO_LOG_MODULES(log) \
    log(mms_ofono_modem_log)\
    log(mms_ofono_manager_log)\
    log(mms_ofono_context_log)

MMS_OFONO_LOG_MODULES(MMS_LOG_MODULE_DECL)

#endif /* JOLLA_MMS_OFONO_LOG_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
