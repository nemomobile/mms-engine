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

#ifndef JOLLA_MMS_LIB_LOG_H
#define JOLLA_MMS_LIB_LOG_H

#include "mms_log.h"

#define MMS_LIB_LOG_MODULES(log) \
    log(mms_dispatcher_log)\
    log(mms_handler_log)\
    log(mms_message_log)\
    log(mms_attachment_log)\
    log(mms_codec_log)\
    log(mms_task_log)\
    log(mms_task_http_log)\
    log(mms_task_decode_log)\
    log(mms_task_encode_log)\
    log(mms_task_notification_log)\
    log(mms_task_retrieve_log)\
    log(mms_task_publish_log)\
    log(mms_task_send_log)\
    log(mms_connman_log)\
    log(mms_connection_log)

MMS_LIB_LOG_MODULES(MMS_LOG_MODULE_DECL)

#endif /* JOLLA_MMS_LIB_LOG_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
