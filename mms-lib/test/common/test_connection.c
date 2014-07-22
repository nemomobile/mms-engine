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

#include "test_connection.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_connection_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-connection-test");

typedef MMSConnectionClass MMSConnectionTestClass;
typedef MMSConnection MMSConnectionTest;

G_DEFINE_TYPE(MMSConnectionTest, mms_connection_test, MMS_TYPE_CONNECTION);
#define MMS_TYPE_CONNECTION_TEST (mms_connection_test_get_type())
#define MMS_CONNECTION_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_CONNECTION_TEST, MMSConnectionTest))

typedef struct test_connection_state_change {
    MMSConnectionTest* test;
    MMS_CONNECTION_STATE state;
} MMSConnectionStateChange;

static
gboolean
test_connection_test_state_change_cb(
    void* param)
{
    MMSConnectionStateChange* change = param;
    MMSConnectionTest* test = change->test;
    if (test->state != MMS_CONNECTION_STATE_CLOSED &&
        test->state != MMS_CONNECTION_STATE_FAILED &&
        test->state != change->state) {
        test->state = change->state;
        if (test->delegate &&
            test->delegate->fn_connection_state_changed) {
            test->delegate->fn_connection_state_changed(test->delegate, test);
        }
    }
    mms_connection_unref(change->test);
    g_free(change);
    return FALSE;
}

static
gboolean
mms_connection_test_set_state(
    MMSConnection* test,
    MMS_CONNECTION_STATE state)
{
    if (test->state != MMS_CONNECTION_STATE_CLOSED) {
        MMSConnectionStateChange* change = g_new0(MMSConnectionStateChange,1);
        change->state = state;
        change->test = mms_connection_ref(test);
        g_idle_add(test_connection_test_state_change_cb, change);
    }
    return TRUE;
}

MMSConnection*
mms_connection_test_new(
    const char* imsi,
    unsigned short port,
    gboolean proxy)
{
    MMSConnectionTest* test = g_object_new(MMS_TYPE_CONNECTION_TEST, NULL);
    test->imsi = g_strdup(imsi);
    if (port) {
        test->netif = g_strdup("lo");
        if (proxy) {
            test->mmsc = g_strdup("http://mmsc");
            test->mmsproxy = g_strdup_printf("127.0.0.1:%hu", port);
        } else {
            test->mmsc = g_strdup_printf("http://127.0.0.1:%hu", port);
        }
    }
    test->state = MMS_CONNECTION_STATE_OPENING;
    mms_connection_test_set_state(test, test->netif ?
        MMS_CONNECTION_STATE_OPEN : MMS_CONNECTION_STATE_FAILED);
    return test;
}

static
void
mms_connection_test_close(
    MMSConnection* test)
{
    mms_connection_test_set_state(test, MMS_CONNECTION_STATE_CLOSED);
}

/**
 * Per class initializer
 */
static
void
mms_connection_test_class_init(
    MMSConnectionTestClass* klass)
{
    klass->fn_close = mms_connection_test_close;
}

/**
 * Per instance initializer
 */
static
void
mms_connection_test_init(
    MMSConnectionTest* conn)
{
    MMS_VERBOSE_("%p", conn);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
