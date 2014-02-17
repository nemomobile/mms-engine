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

#include "test_handler.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_handler_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-handler-test");

/* Class definition */
typedef MMSHandlerClass MMSHandlerTestClass;
typedef struct mms_handler_test {
    MMSHandler handler;
    mms_handler_message_id_cb message_id_cb;
    void*  message_id_param;
} MMSHandlerTest;

G_DEFINE_TYPE(MMSHandlerTest, mms_handler_test, MMS_TYPE_HANDLER);
#define MMS_TYPE_HANDLER_TEST (mms_handler_test_get_type())
#define MMS_HANDLER_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    MMS_TYPE_HANDLER_TEST, MMSHandlerTest))

static
char*
mms_handler_test_message_notify(
    MMSHandler* handler,
    const char* imsi,
    const char* from,
    const char* subject,
    time_t expiry,
    GBytes* push)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    return test->message_id_cb ? 
           test->message_id_cb(handler, test->message_id_param) :
           NULL;
}

static
gboolean
mms_handler_test_message_received(
    MMSHandler* handler,
    MMSMessage* msg)
{
    MMS_DEBUG("Message %s received", msg->id);
    return TRUE;
}

static
gboolean
mms_handler_test_message_receive_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_RECEIVE_STATE state)
{
    MMS_DEBUG("Message %s state %d", id, state);
    return TRUE;
}

static
void
mms_handler_test_class_init(
    MMSHandlerTestClass* klass)
{
    klass->fn_message_notify = mms_handler_test_message_notify;
    klass->fn_message_received = mms_handler_test_message_received;
    klass->fn_message_receive_state_changed =
        mms_handler_test_message_receive_state_changed;
}

static
void
mms_handler_test_init(
    MMSHandlerTest* test)
{
}

MMSHandler*
mms_handler_test_new()
{
    return g_object_new(MMS_TYPE_HANDLER_TEST, NULL);
}

void
mms_handler_set_message_id_cb(
    MMSHandler* handler,
    mms_handler_message_id_cb cb,
    void* param)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    test->message_id_cb = cb;
    test->message_id_param = param;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
