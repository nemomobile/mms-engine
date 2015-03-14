/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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
#include "test_http.h"

#include "mms_log.h"
#include "mms_codec.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_settings.h"
#include "mms_dispatcher.h"

#include <gio/gio.h>
#include <libsoup/soup-status.h>

#define RET_OK       (0)
#define RET_ERR      (1)
#define RET_TIMEOUT  (2)

#define TEST_TIMEOUT (10) /* seconds */

#define TEST_IMSI    "IMSI"

typedef struct test_desc {
    const char* name;
    MMSReadStatus status;
    const char* phone;
    enum mms_message_read_status rr_status;
    const char* to;
} TestDesc;

typedef struct test {
    const TestDesc* desc;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GMainLoop* loop;
    const char* imsi;
    char* id;
    guint timeout_id;
    TestHttp* http;
    int ret;
} Test;

static const TestDesc tests[] = {
    {
        "Read",
        MMS_READ_STATUS_READ,
        "+358501111111",
        MMS_MESSAGE_READ_STATUS_READ,
        "+358501111111/TYPE=PLMN"
    },{
        "Deleted",
        MMS_READ_STATUS_DELETED,
        "+358501111111/TYPE=PLMN",
        MMS_MESSAGE_READ_STATUS_DELETED,
        "+358501111111/TYPE=PLMN"
    }
};

static
void
test_done(
    MMSDispatcherDelegate* delegate,
    MMSDispatcher* dispatcher)
{
    Test* test = MMS_CAST(delegate,Test,delegate);
    const TestDesc* desc = test->desc;
    const char* name = desc->name;
    if (test->ret == RET_OK) {
        const void* resp_data = NULL;
        gsize resp_len = 0;
        GBytes* reply = test_http_get_post_data(test->http);
        if (reply) resp_data = g_bytes_get_data(reply, &resp_len);
        if (resp_len > 0) {
            MMSPdu* pdu = g_new0(MMSPdu, 1);
            test->ret = RET_ERR;
            if (mms_message_decode(resp_data, resp_len, pdu)) {
                if (pdu->type != MMS_MESSAGE_TYPE_READ_REC_IND) {
                    MMS_ERR("Unexpected PDU type %u", pdu->type);
                } else if (pdu->ri.rr_status != desc->rr_status) {
                    MMS_ERR("Read status %d, expected %d",
                        pdu->ri.rr_status, desc->rr_status);
                } else if (g_strcmp0(pdu->ri.to, desc->to)) {
                    MMS_ERR("Phone number %s, expected %s",
                        pdu->ri.to, desc->to);
                } else {
                    MMS_READ_REPORT_STATUS status =
                        mms_handler_test_read_report_status(test->handler,
                        test->id);
                    if (status != MMS_READ_REPORT_STATUS_OK) {
                        MMS_ERR("Unexpected status %d", status);
                    } else {
                        test->ret = RET_OK;
                    }
                }
            } else {
                MMS_ERR("Can't decode PDU");
            }
            mms_message_free(pdu);
        }
    }
    MMS_INFO("%s: %s", (test->ret == RET_OK) ? "OK" : "FAILED", name);
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
    MMS_INFO("TIMEOUT");
    if (test->http) test_http_close(test->http);
    mms_connman_test_close_connection(test->cm);
    mms_dispatcher_cancel(test->disp, NULL);
    return FALSE;
}

static
void
test_init(
    Test* test,
    const MMSConfig* config,
    const TestDesc* desc,
    gboolean debug)
{
    MMSSettings* settings = mms_settings_default_new(config);
    MMS_DEBUG(">>>>>>>>>> %s <<<<<<<<<<", desc->name);
    test->desc = desc;
    test->cm = mms_connman_test_new();
    test->handler = mms_handler_test_new();
    test->disp = mms_dispatcher_new(settings, test->cm, test->handler);
    test->loop = g_main_loop_new(NULL, FALSE);
    test->delegate.fn_done = test_done;
    mms_dispatcher_set_delegate(test->disp, &test->delegate);
    test->http = test_http_new(NULL, NULL, SOUP_STATUS_OK);
    test->id = g_strdup(mms_handler_test_receive_new(test->handler, TEST_IMSI));
    mms_connman_test_set_port(test->cm, test_http_get_port(test->http), TRUE);
    mms_settings_unref(settings);
    test->ret = RET_ERR;
    if (!debug) {
        test->timeout_id = g_timeout_add_seconds(TEST_TIMEOUT,
            test_timeout, test);
    }
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
    g_free(test->id);
    test_http_close(test->http);
    test_http_unref(test->http);
    mms_connman_test_close_connection(test->cm);
    mms_connman_unref(test->cm);
    mms_handler_unref(test->handler);
    mms_dispatcher_unref(test->disp);
    g_main_loop_unref(test->loop);
}

static
int
test_read_report_once(
    const MMSConfig* config,
    const TestDesc* desc,
    gboolean debug)
{
    Test test;
    GError* error = NULL;
    test_init(&test, config, desc, debug);
    if (mms_dispatcher_send_read_report(test.disp, test.id, TEST_IMSI,
        "MessageID", desc->phone, desc->status, &error)) {
        if (mms_dispatcher_start(test.disp)) {
            test.ret = RET_OK;
            g_main_loop_run(test.loop);
        } else {
            MMS_INFO("FAILED");
        }
    } else {
        g_error_free(error);
        MMS_INFO("FAILED");
    }
    test_finalize(&test);
    return test.ret;
}

static
int
test_read_report(
    const MMSConfig* config,
    const char* name,
    gboolean debug)
{
    int i, ret;
    if (name) {
        const TestDesc* found = NULL;
        for (i=0, ret = RET_ERR; i<G_N_ELEMENTS(tests); i++) {
            const TestDesc* test = tests + i;
            if (!strcmp(test->name, name)) {
                ret = test_read_report_once(config, test, debug);
                found = test;
                break;
            }
        }
        if (!found) MMS_ERR("No such test: %s", name);
    } else {
        for (i=0, ret = RET_OK; i<G_N_ELEMENTS(tests); i++) {
            int status = test_read_report_once(config, tests + i, debug);
            if (ret == RET_OK && status != RET_OK) ret = status;
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    gboolean debug = FALSE;
    gboolean verbose = FALSE;
    GError* error = NULL;
    GOptionContext* options;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
          "Disable timeout for debugging", NULL },
        { NULL }
    };

    options = g_option_context_new("[TEST] - MMS read report test");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        MMSConfig config;
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_read_report_XXXXXX"));
        char* msgdir = g_strconcat(tmpd, "/msg", NULL);

        mms_lib_init(argv[0]);
        mms_lib_default_config(&config);
        config.idle_secs = 0;
        config.root_dir = tmpd;
        mms_log_default.name = "test_read_report";
        if (verbose) {
            mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        } else {
            mms_log_default.level = MMS_LOGLEVEL_INFO;
            mms_task_decode_log.level =
            mms_task_retrieve_log.level =
            mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
            mms_log_stdout_timestamp = FALSE;
        }

        MMS_VERBOSE("Temporary directory %s", tmpd);
        if (argc < 2) {
            ret = test_read_report(&config, NULL, debug);
        } else {
            int i;
            for (i=1, ret = RET_OK; i<argc; i++) {
                int test_status =  test_read_report(&config, argv[i], debug);
                if (ret == RET_OK && test_status != RET_OK) ret = test_status;
            }
        }
        rmdir(msgdir);
        rmdir(tmpd);
        remove(tmpd);
        g_free(tmpd);
        g_free(msgdir);
        mms_lib_deinit();
    } else {
        fprintf(stderr, "%s\n", MMS_ERRMSG(error));
        g_error_free(error);
        ret = RET_ERR;
    }
    g_option_context_free(options);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
