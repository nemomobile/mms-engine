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

#include "test_connman.h"
#include "test_handler.h"

#include "mms_log.h"
#include "mms_lib_util.h"
#include "mms_dispatcher.h"

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

typedef struct test_desc {
    const char* name;
    const guint8* pdu;
    gsize pdusize;
    int flags;

#define TEST_ASYNC_CANCEL (0x01)
#define TEST_ACCEPT_MSG   (0x02)

} TestDesc;

typedef struct test {
    const TestDesc* desc;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GBytes* pdu;
    GMainLoop* loop;
    guint cancel_id;
    guint timeout_id;
    char* id;
    int ret;
} Test;

static
void
test_done(
    MMSDispatcherDelegate* delegate,
    MMSDispatcher* dispatcher)
{
    Test* test = MMS_CAST(delegate,Test,delegate);
    MMS_INFO("%s: %s", (test->ret == RET_OK) ? "OK" : "FAILED",
        test->desc->name);
    g_main_loop_quit(test->loop);
}

static
gboolean
test_timeout(
    gpointer data)
{
    Test* test = data;
    test->timeout_id = 0;
    test->ret = RET_TIMEOUT;
    MMS_INFO("%s TIMEOUT", test->desc->name);
    mms_dispatcher_cancel(test->disp, NULL);
    return FALSE;
}

static
void
test_init(
    Test* test,
    const MMSConfig* config,
    const TestDesc* desc)
{
    memset(test, 0, sizeof(*test));
    test->desc = desc;
    test->cm = mms_connman_test_new();
    test->handler = mms_handler_test_new();
    test->disp = mms_dispatcher_new(config, test->cm, test->handler);
    test->pdu = g_bytes_new_static(desc->pdu, desc->pdusize);
    test->loop = g_main_loop_new(NULL, FALSE);
    test->delegate.fn_done = test_done;
    mms_dispatcher_set_delegate(test->disp, &test->delegate);
    test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
    test->ret = RET_ERR;
}

static
void
test_finalize(
    Test* test)
{
    if (test->timeout_id) {
        g_source_remove(test->timeout_id);
        test->timeout_id = 0;
    }
    mms_connman_unref(test->cm);
    mms_handler_unref(test->handler);
    mms_dispatcher_unref(test->disp);
    g_bytes_unref(test->pdu);
    g_main_loop_unref(test->loop);
    g_free(test->id);
}

static
char*
test_msg_id(
    MMSHandler* handler,
    void* param)
{
    Test* test = param;
    MMS_ASSERT(!test->id);
    if (test->id) {
        test->ret = RET_ERR;
        g_main_loop_quit(test->loop);
        return NULL;
    } else if (test->desc->flags & TEST_ACCEPT_MSG) {
        return g_strdup(test->id = g_strdup("21285"));
    } else {
        return NULL;
    }
}

static
gboolean
test_cancel(
    void* param)
{
    Test* test = param;
    test->cancel_id = 0;
    MMS_DEBUG("Asynchronous cancel %s", test->id);
    mms_dispatcher_cancel(test->disp, test->id);
    return FALSE;
}

static
void
test_connect(
    MMSConnMan* cm,
    void* param)
{
    Test* test = param;
    if (test->desc->flags & TEST_ASYNC_CANCEL) {
        if (!test->cancel_id) {
            test->cancel_id = g_idle_add_full(G_PRIORITY_HIGH,
                test_cancel, test, NULL);
        }
    } else {
        MMS_DEBUG("Synchronous cancel %s", test->id ? test->id : "all");
        mms_dispatcher_cancel(test->disp, test->id);
    }
}

static
int
test_once(
    const MMSConfig* config,
    const TestDesc* desc)
{
    Test test;
    GError* error = NULL;
    MMS_VERBOSE(">>>>>>>>>>>>>> %s <<<<<<<<<<<<<<", desc->name);
    test_init(&test, config, desc);
    if (mms_dispatcher_handle_push(test.disp, "IMSI", test.pdu, &error)) {
        mms_connman_set_connection_requested_cb(test.cm, test_connect, &test);
        mms_handler_set_message_id_cb(test.handler, test_msg_id, &test);
        if (mms_dispatcher_start(test.disp)) {
            test.ret = RET_OK;
            g_main_loop_run(test.loop);
        }
    } else {
        g_error_free(error);
    }
    test_finalize(&test);
    return test.ret;
}

static
int
test(
    const MMSConfig* config)
{
    /*
     * WSP header:
     *   application/vnd.wap.mms-message
     * MMS headers:
     *   X-Mms-Message-Type: M-Notification.ind
     *   X-Mms-Transaction-Id: Ad0b9pXNC
     *   X-Mms-MMS-Version: 1.2
     *   From: +358540000000/TYPE=PLMN
     *   X-Mms-Delivery-Report: No
     *   X-Mms-Message-Class: Personal
     *   X-Mms-Message-Size: 137105
     *   X-Mms-Expiry: +259199 sec
     *   X-Mms-Content-Location: http://mmsc42:10021/mmsc/4_2?Ad0b9pXNC
     */
    static const guint8 plus_259199_sec[] = {
        0x8c,0x82,0x98,0x41,0x64,0x30,0x62,0x39,0x70,0x58,0x4e,0x43,
        0x00,0x8d,0x92,0x89,0x19,0x80,0x2b,0x33,0x35,0x38,0x35,0x34,
        0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x2f,0x54,0x59,0x50,0x45,
        0x3d,0x50,0x4c,0x4d,0x4e,0x00,0x86,0x81,0x8a,0x80,0x8e,0x03,
        0x02,0x17,0x91,0x88,0x05,0x81,0x03,0x03,0xf4,0x7f,0x83,0x68,
        0x74,0x74,0x70,0x3a,0x2f,0x2f,0x6d,0x6d,0x73,0x63,0x34,0x32,
        0x3a,0x31,0x30,0x30,0x32,0x31,0x2f,0x6d,0x6d,0x73,0x63,0x2f,
        0x34,0x5f,0x32,0x3f,0x41,0x64,0x30,0x62,0x39,0x70,0x58,0x4e,
        0x43,0x00
    };

    /*
     * WSP header:
     *   application/vnd.wap.mms-message
     * MMS headers:
     *   X-Mms-Message-Type: M-Notification.ind
     *   X-Mms-Transaction-Id: Ad0b9pXNC
     *   X-Mms-MMS-Version: 1.2
     *   From: +358540000000/TYPE=PLMN
     *   X-Mms-Delivery-Report: No
     *   X-Mms-Message-Class: Personal
     *   X-Mms-Message-Size: 137105
     *   X-Mms-Expiry: +1 sec
     *   X-Mms-Content-Location: http://mmsc42:10021/mmsc/4_2?Ad0b9pXNC
     */
    static const guint8 plus_1_sec[] = {
        0x8c,0x82,0x98,0x41,0x64,0x30,0x62,0x39,0x70,0x58,0x4e,0x43,
        0x00,0x8d,0x92,0x89,0x19,0x80,0x2b,0x33,0x35,0x38,0x35,0x34,
        0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x2f,0x54,0x59,0x50,0x45,
        0x3d,0x50,0x4c,0x4d,0x4e,0x00,0x86,0x81,0x8a,0x80,0x8e,0x03,
        0x02,0x17,0x91,0x88,0x03,0x81,0x01,0x01,0x83,0x68,0x74,0x74,
        0x70,0x3a,0x2f,0x2f,0x6d,0x6d,0x73,0x63,0x34,0x32,0x3a,0x31,
        0x30,0x30,0x32,0x31,0x2f,0x6d,0x6d,0x73,0x63,0x2f,0x34,0x5f,
        0x32,0x3f,0x41,0x64,0x30,0x62,0x39,0x70,0x58,0x4e,0x43,0x00
    };

    static const TestDesc tests[] = {
        {
            "AsyncCancel", plus_259199_sec, sizeof(plus_259199_sec),
            TEST_ASYNC_CANCEL | TEST_ACCEPT_MSG
        },{
            "SyncCancel", plus_259199_sec, sizeof(plus_259199_sec),
            TEST_ACCEPT_MSG
        },{
            "NoHandler", plus_1_sec, sizeof(plus_1_sec), 0
        }
    };

    int i, ret = RET_OK;
    for (i=0; i<G_N_ELEMENTS(tests); i++) {
        int test_status = test_once(config, tests + i);
        if (ret == RET_OK && test_status != RET_OK) ret = test_status;
    }
    return ret;
}

int main(int argc, char* argv[])
{
    int ret;
    MMSConfig config;
    mms_lib_init(argv[0]);
    mms_lib_default_config(&config);
    config.retry_secs = 0;
    config.root_dir = "."; /* Dispatcher will attempt to create it */
    mms_log_default.name = "test_retrieve_cancel";
    if (argc > 1 && !strcmp(argv[1], "-v")) {
        mms_log_stdout_timestamp = TRUE;
        mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
    } else {
        mms_log_stdout_timestamp = FALSE;
        mms_log_default.level = MMS_LOGLEVEL_INFO;
    }
    ret = test(&config);
    mms_lib_deinit();
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
