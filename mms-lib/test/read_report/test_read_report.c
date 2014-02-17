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
#include "test_http.h"

#include "mms_log.h"
#include "mms_codec.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_dispatcher.h"

#include <gio/gio.h>
#include <libsoup/soup-status.h>

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

typedef struct test {
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GMainLoop* loop;
    guint timeout_id;
    TestHttp* http;
    int ret;
} Test;

static
void
test_done(
    MMSDispatcherDelegate* delegate,
    MMSDispatcher* dispatcher)
{
    Test* test = MMS_CAST(delegate,Test,delegate);
    if (test->ret == RET_OK) {
        const void* resp_data = NULL;
        gsize resp_len = 0;
        GBytes* reply = test_http_get_post_data(test->http);
        if (reply) resp_data = g_bytes_get_data(reply, &resp_len);
        if (resp_len > 0) {
            MMSPdu* pdu = g_new0(MMSPdu, 1);
            if (mms_message_decode(resp_data, resp_len, pdu)) {
                if (pdu->type != MMS_MESSAGE_TYPE_READ_REC_IND) {
                    test->ret = RET_ERR;
                    MMS_ERR("Unexpected PDU type %u", pdu->type);
                }
            } else {
                test->ret = RET_ERR;
                MMS_ERR("Can't decode PDU");
            }
            mms_message_free(pdu);
        }
    }
    MMS_INFO("%s", (test->ret == RET_OK) ? "OK" : "FAILED");
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
    const MMSConfig* config)
{
    test->cm = mms_connman_test_new();
    test->handler = mms_handler_test_new();
    test->disp = mms_dispatcher_new(config, test->cm, test->handler);
    test->loop = g_main_loop_new(NULL, FALSE);
    test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
    test->delegate.fn_done = test_done;
    mms_dispatcher_set_delegate(test->disp, &test->delegate);
    test->http = test_http_new(NULL, NULL, SOUP_STATUS_OK);
    mms_connman_test_set_port(test->cm, test_http_get_port(test->http));
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
test_read_report(
    const MMSConfig* config)
{
    Test test;
    test_init(&test, config);
    if (mms_dispatcher_send_read_report(test.disp, "1", "IMSI",
        "MessageID", "+358501111111", MMS_READ_STATUS_READ)) {
        if (mms_dispatcher_start(test.disp)) {
            test.ret = RET_OK;
            g_main_loop_run(test.loop);
        } else {
            MMS_INFO("FAILED");
        }
    } else {
        MMS_INFO("FAILED");
    }
    test_finalize(&test);
    return test.ret;
}

int main(int argc, char* argv[])
{
    int ret;
    MMSConfig config;
    char* tmpd = g_mkdtemp(g_strdup("/tmp/test_retrieve_XXXXXX"));
    mms_lib_init();
    mms_lib_default_config(&config);
    config.idle_secs = 0;
    config.root_dir = tmpd;
    mms_log_default.name = "test_read_report";
    if (argc > 1 && !strcmp(argv[1], "-v")) {
        mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
    } else {
        mms_log_default.level = MMS_LOGLEVEL_INFO;
        mms_util_log.level = 
        mms_task_decode_log.level =
        mms_task_retrieve_log.level =
        mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
        mms_log_stdout_timestamp = FALSE;
    }
    MMS_VERBOSE("Temporary directory %s", tmpd);
    ret = test_read_report(&config);
    remove(tmpd);
    g_free(tmpd);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
