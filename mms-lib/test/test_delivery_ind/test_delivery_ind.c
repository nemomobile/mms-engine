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

#include "mms_file_util.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_settings.h"
#include "mms_dispatcher.h"

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

#define DATA_DIR "data/"

typedef struct test_desc {
    const char* name;
    const char* ind_file;
    const char* mmsid;
    MMS_DELIVERY_STATUS status;
} TestDesc;

typedef struct test {
    const TestDesc* desc;
    const MMSConfig* config;
    char* id;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GMappedFile* notification_ind;
    GMainLoop* loop;
    guint timeout_id;
    int ret;
} Test;

static const TestDesc delivery_tests[] = {
    {
        "DeliveryOK",
        "m-delivery.ind",
        "BH24CBJJA40W1",
        MMS_DELIVERY_STATUS_RETRIEVED
    },{
        "DeliveryUnexpected",
        "m-delivery.ind",
        "UNKNOWN",
        MMS_DELIVERY_STATUS_INVALID
    },{
        "DeliveryRejected",
        "m-delivery.ind",
        "BH24CBJJA40W1",
        MMS_DELIVERY_STATUS_REJECTED
    }
};

static
void
test_finish(
    Test* test)
{
    const TestDesc* desc = test->desc;
    const char* name = desc->name;
    if (test->ret == RET_OK) {
        MMS_DELIVERY_STATUS ds;
        ds = mms_handler_test_delivery_status(test->handler, test->id);
        if (ds != desc->status) {
            test->ret = RET_ERR;
            MMS_ERR("%s status %d, expected %d", name, ds, desc->status);
        }
    }
    MMS_INFO("%s: %s", (test->ret == RET_OK) ? "OK" : "FAILED", name);
    mms_handler_test_reset(test->handler);
    g_main_loop_quit(test->loop);
}

static
void
test_done(
    MMSDispatcherDelegate* delegate,
    MMSDispatcher* dispatcher)
{
    Test* test = MMS_CAST(delegate,Test,delegate);
    if (!mms_handler_test_receive_pending(test->handler, NULL)) {
        test_finish(test);
    }
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
    mms_connman_test_close_connection(test->cm);
    mms_dispatcher_cancel(test->disp, NULL);
    return FALSE;
}

static
gboolean
test_init(
    Test* test,
    const MMSConfig* config,
    const TestDesc* desc)
{
    gboolean ok = FALSE;
    GError* error = NULL;
    const char* dir = desc->name;
    char* ni = g_strconcat(DATA_DIR, dir, "/", desc->ind_file, NULL);
    memset(test, 0, sizeof(*test));
    test->config = config;
    test->notification_ind = g_mapped_file_new(ni, FALSE, &error);
    if (test->notification_ind) {
        MMSSettings* settings = mms_settings_default_new(config);
        test->desc = desc;
        test->cm = mms_connman_test_new();
        test->handler = mms_handler_test_new();
        test->disp = mms_dispatcher_new(settings, test->cm, test->handler);
        test->loop = g_main_loop_new(NULL, FALSE);
        test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
        test->delegate.fn_done = test_done;
        mms_dispatcher_set_delegate(test->disp, &test->delegate);
        test->id = g_strdup(mms_handler_test_send_new(test->handler, "IMSI"));
        mms_handler_message_sent(test->handler, test->id, desc->mmsid);
        mms_settings_unref(settings);
        test->ret = RET_ERR;
        ok = TRUE;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    g_free(ni);
    return ok;
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
    mms_connman_test_close_connection(test->cm);
    mms_connman_unref(test->cm);
    mms_handler_unref(test->handler);
    mms_dispatcher_unref(test->disp);
    g_main_loop_unref(test->loop);
    g_mapped_file_unref(test->notification_ind);
    g_free(test->id);
}

static
int
test_run_one(
    const MMSConfig* config,
    const TestDesc* desc)
{
    Test test;
    if (test_init(&test, config, desc)) {
        GError* error = NULL;
        GBytes* push = g_bytes_new_static(
            g_mapped_file_get_contents(test.notification_ind),
            g_mapped_file_get_length(test.notification_ind));
        if (mms_dispatcher_handle_push(test.disp, "TestConnection",
            push, &error)) {
            if (mms_dispatcher_start(test.disp)) {
                test.ret = RET_OK;
                g_main_loop_run(test.loop);
            } else {
                MMS_INFO("%s FAILED", desc->name);
            }
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            MMS_INFO("%s FAILED", desc->name);
            g_error_free(error);
        }
        g_bytes_unref(push);
        test_finalize(&test);
        return test.ret;
    } else {
        return RET_ERR;
    }
}

static
int
test_run(
    const MMSConfig* config,
    const char* name)
{
    int i, ret;
    if (name) {
        const TestDesc* found = NULL;
        for (i=0, ret = RET_ERR; i<G_N_ELEMENTS(delivery_tests); i++) {
            const TestDesc* test = delivery_tests + i;
            if (!strcmp(test->name, name)) {
                ret = test_run_one(config, test);
                found = test;
                break;
            }
        }
        if (!found) MMS_ERR("No such test: %s", name);
    } else {
        for (i=0, ret = RET_OK; i<G_N_ELEMENTS(delivery_tests); i++) {
            int test_status = test_run_one(config, delivery_tests + i);
            if (ret == RET_OK && test_status != RET_OK) ret = test_status;
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    int ret;
    MMSConfig config;
    const char* test_name = NULL;

    mms_lib_init(argv[0]);
    mms_lib_default_config(&config);
    mms_log_default.name = "test_delivery_ind";

    if (argc > 1 && !strcmp(argv[1], "-v")) {
        mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        memmove(argv + 1, argv + 2, (argc-2)*sizeof(argv[0]));
        argc--;
    } else {
        mms_log_default.level = MMS_LOGLEVEL_INFO;
        mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
        mms_log_stdout_timestamp = FALSE;
    }

    if (argc == 2 && argv[1][0] != '-') {
        test_name = argv[1];
    }

    if (argc == 1 || test_name) {
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_delivery_ind_XXXXXX"));
        MMS_VERBOSE("Temporary directory %s", tmpd);
        config.root_dir = tmpd;
        config.idle_secs = 0;
        config.attic_enabled = TRUE;
        ret = test_run(&config, test_name);
        remove(tmpd);
        g_free(tmpd);
    } else {
        printf("Usage: test_delivery_ind [-v] [TEST]\n");
        ret = RET_ERR;
    }

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
