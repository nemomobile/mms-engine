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
#include "mms_file_util.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_settings.h"
#include "mms_dispatcher.h"

#include <gio/gio.h>
#include <libsoup/soup-status.h>

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

#define MMS_MESSAGE_TYPE_NONE (0)

#define DATA_DIR "data/"

typedef struct test {
    const MMSConfig* config;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GBytes* notification_ind;
    GMappedFile* retrieve_conf;
    GMainLoop* loop;
    guint timeout_id;
    TestHttp* http;
    int ret;
} Test;

static
void
test_finish(
    Test* test)
{
    if (test->ret == RET_OK) {
        MMS_RECEIVE_STATE state;
        state = mms_handler_test_receive_state(test->handler, NULL);
        if (state != MMS_RECEIVE_STATE_DECODING) {
            test->ret = RET_ERR;
            MMS_ERR("Unexpected state %d", state);
        } else {
            const void* resp_data = NULL;
            gsize resp_len = 0;
            GBytes* reply = test_http_get_post_data(test->http);
            if (reply) resp_data = g_bytes_get_data(reply, &resp_len);
            if (resp_len > 0) {
                MMSPdu* pdu = g_new0(MMSPdu, 1);
                if (mms_message_decode(resp_data, resp_len, pdu)) {
                    if (pdu->type != MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND) {
                        test->ret = RET_ERR;
                        MMS_ERR("Unexpected reply %u", pdu->type);
                    }
                } else {
                    test->ret = RET_ERR;
                    MMS_ERR("Failed to decode reply message");
                }
                mms_message_free(pdu);
            } else {
                test->ret = RET_ERR;
                MMS_ERR("Reply expected");
            }
        }
    }
    MMS_INFO("%s", (test->ret == RET_OK) ? "OK" : "FAILED");
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
    test_finish(test);
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
gboolean
test_init(
    Test* test,
    const MMSConfig* config)
{
    static const guint8 push_template[] = {
        0x8C,0x82,0x98,0x42,0x49,0x33,0x52,0x34,0x56,0x32,0x49,0x53,
        0x4C,0x52,0x34,0x31,0x40,0x78,0x6D,0x61,0x2E,0x37,0x32,0x34,
        0x2E,0x63,0x6F,0x6D,0x00,0x8D,0x91,0x86,0x80,0x88,0x05,0x81,
        0x03,0x03,0xF4,0x7E,0x89,0x19,0x80,0x2B,0x33,0x35,0x38,0x35,
        0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x2F,0x54,0x59,0x50,
        0x45,0x3D,0x50,0x4C,0x4D,0x4E,0x00,0x8A,0x80,0x8E,0x03,0x01,
        0xB9,0x7A,0x96,0x0D,0xEA,0x7F,0xD0,0x9F,0xD0,0xB8,0xD1,0x82,
        0xD0,0xB5,0xD1,0x80,0x00,0x83,0x68,0x74,0x74,0x70,0x3A,0x2F,
        0x2F,0x31,0x32,0x37,0x2E,0x30,0x2E,0x30,0x2E,0x31,0x3A
    };

    gboolean ok = FALSE;
    GError* err = NULL;
    memset(test, 0, sizeof(*test));
    test->config = config;
    test->retrieve_conf = g_mapped_file_new("data/m-retrieve.conf",FALSE,&err);
    if (test->retrieve_conf) {
        guint port;
        char* port_string;
        char* push_data;
        gsize push_len;
        MMSSettings* settings = mms_settings_default_new(config);
        test->cm = mms_connman_test_new();
        test->handler = mms_handler_test_new();
        test->disp = mms_dispatcher_new(settings, test->cm, test->handler);
        test->loop = g_main_loop_new(NULL, FALSE);
        test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
        test->delegate.fn_done = test_done;
        mms_dispatcher_set_delegate(test->disp, &test->delegate);
        test->http = test_http_new(test->retrieve_conf, MMS_CONTENT_TYPE,
            SOUP_STATUS_OK);

        port = test_http_get_port(test->http);
        mms_connman_test_set_port(test->cm, port, FALSE);
        port_string = g_strdup_printf("%u", port);

        push_len = strlen(port_string) + 1 + sizeof(push_template);
        push_data = g_malloc(push_len);
        memcpy(push_data, push_template, sizeof(push_template));
        strcpy(push_data + sizeof(push_template), port_string);
        test->notification_ind = g_bytes_new(push_data, push_len);

        g_free(push_data);
        g_free(port_string);
        mms_settings_unref(settings);
        test->ret = RET_ERR;
        ok = TRUE;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(err));
        g_error_free(err);
    }
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
    if (test->http) {
        test_http_close(test->http);
        test_http_unref(test->http);
    }
    mms_connman_test_close_connection(test->cm);
    mms_connman_unref(test->cm);
    mms_handler_unref(test->handler);
    mms_dispatcher_unref(test->disp);
    g_main_loop_unref(test->loop);
    g_bytes_unref(test->notification_ind);
    g_mapped_file_unref(test->retrieve_conf);
}

static
int
test_retrieve_no_proxy(
    const MMSConfig* config)
{
    Test test;
    if (test_init(&test, config)) {
        GError* error = NULL;
        if (mms_dispatcher_handle_push(test.disp, "TestConnection",
            test.notification_ind, &error)) {
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
    } else {
        return RET_ERR;
    }
}

int main(int argc, char* argv[])
{
    int ret;
    MMSConfig config;

    mms_lib_init(argv[0]);
    mms_lib_default_config(&config);
    mms_log_default.name = "test_retrieve_no_proxy";

    if (argc > 1 && !strcmp(argv[1], "-v")) {
        mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        argc--;
    } else {
        mms_log_default.level = MMS_LOGLEVEL_INFO;
        mms_task_decode_log.level =
        mms_task_retrieve_log.level =
        mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
        mms_log_stdout_timestamp = FALSE;
    }

    if (argc == 1) {
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_retrieve_XXXXXX"));
        MMS_VERBOSE("Temporary directory %s", tmpd);
        config.root_dir = tmpd;
        config.idle_secs = 0;
        config.attic_enabled = TRUE;
        ret = test_retrieve_no_proxy(&config);
        remove(tmpd);
        g_free(tmpd);
    } else {
        printf("Usage: test_retrieve [-v] [TEST]\n");
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
