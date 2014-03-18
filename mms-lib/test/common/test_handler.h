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

#ifndef TEST_HANDLER_H
#define TEST_HANDLER_H

#include "mms_handler.h"

MMSHandler*
mms_handler_test_new(void);

const char*
mms_handler_test_send_new(
    MMSHandler* handler,
    const char* imsi);

const char*
mms_handler_test_send_msgid(
    MMSHandler* handler,
    const char* id);

MMS_SEND_STATE
mms_handler_test_send_state(
    MMSHandler* handler,
    const char* id);

MMS_RECEIVE_STATE
mms_handler_test_receive_state(
    MMSHandler* handler,
    const char* id);

MMSMessage*
mms_handler_test_get_received_message(
    MMSHandler* handler,
    const char* id);

gboolean
mms_handler_test_receive_pending(
    MMSHandler* handler,
    const char* id);

MMS_DELIVERY_STATUS
mms_handler_test_delivery_status(
    MMSHandler* handler,
    const char* id);

MMS_READ_STATUS
mms_handler_test_read_status(
    MMSHandler* handler,
    const char* id);

void
mms_handler_test_defer_receive(
    MMSHandler* handler,
    MMSDispatcher* dispatcher);

void
mms_handler_test_reset(
    MMSHandler* handler);

#endif /* TEST_HANDLER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
