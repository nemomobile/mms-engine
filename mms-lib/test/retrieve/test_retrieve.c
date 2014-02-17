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

#define MMS_MESSAGE_TYPE_NONE (0)

#define DATA_DIR "data/"

typedef struct test_desc {
    const char* name;
    const char* dir;
    const char* ni_file;
    const char* rc_file;
    unsigned int status;
    const char* content_type;
    MMS_RECEIVE_STATE expected_state;
    enum mms_message_type reply_msg;
    int flags;

#define TEST_PUSH_HANDLING_FAILURE_OK (0x01)
#define TEST_DEFER_RECEIVE (0x02)

} TestDesc;

typedef struct test {
    const TestDesc* desc;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GMappedFile* notification_ind;
    GMappedFile* retrieve_conf;
    GMainLoop* loop;
    guint timeout_id;
    TestHttp* http;
    int ret;
} Test;

static const TestDesc retrieve_tests[] = {
    {
        "Success",
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        0
    },{
        "DeferSuccess",
        "Success",
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_DEFER_RECEIVE
    },{
        "Expired",
        NULL,
        "m-notification.ind",
        NULL,
        SOUP_STATUS_NOT_FOUND,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        0
    },{
        "SoonExpired",
        NULL,
        "m-notification.ind",
        NULL,
        SOUP_STATUS_TRY_AGAIN,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        0
    },{
        "NotAllowed",
        NULL,
        "m-notification.ind",
        "not-allowed.html",
        SOUP_STATUS_BAD_REQUEST,
        "text/html",
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        0
    },{
        "NotFound",
        NULL,
        "m-notification.ind",
        "not-found.html",
        SOUP_STATUS_NOT_FOUND,
        "text/html",
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        0
    },{
        "GarbageRetrieve",
        NULL,
        "m-notification.ind",
        "garbage",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        MMS_RECEIVE_STATE_DECODING_ERROR,
        MMS_MESSAGE_TYPE_NOTIFYRESP_IND,
        0
    },{
        "GarbagePush",
        NULL,
        "garbage",
        NULL,
        0,
        NULL,
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PUSH_HANDLING_FAILURE_OK
    },{
        "UnsupportedPush",
        NULL,
        "unsupported",
        NULL,
        0,
        NULL,
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PUSH_HANDLING_FAILURE_OK
    },{
        "ReadOrigInd",
        NULL,
        "m-read-orig.ind",
        NULL,
        0,
        NULL,
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        0
    },{
        "DeliveryInd",
        NULL,
        "m-delivery.ind",
        NULL,
        0,
        NULL,
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        0
    }
};

static
void
test_finish(
    Test* test)
{
    const char* name = test->desc->name;
    if (test->ret == RET_OK) {
        MMS_RECEIVE_STATE state;
        state = mms_handler_test_receive_state(test->handler, NULL);
        if (state != test->desc->expected_state) {
            test->ret = RET_ERR;
            MMS_ERR("Test %s state %d, expected %d", name, state,
                test->desc->expected_state);
        } else {
            const void* resp_data = NULL;
            gsize resp_len = 0;
            GBytes* reply = test_http_get_post_data(test->http);
            if (reply) resp_data = g_bytes_get_data(reply, &resp_len);
            if (resp_len > 0) {
                if (test->desc->reply_msg) {
                    MMSPdu* pdu = g_new0(MMSPdu, 1);
                    if (mms_message_decode(resp_data, resp_len, pdu)) {
                        if (pdu->type != test->desc->reply_msg) {
                            test->ret = RET_ERR;
                            MMS_ERR("Test %s reply %u, expected %u", name,
                                pdu->type, test->desc->reply_msg);
                        }
                    } else {
                        test->ret = RET_ERR;
                        MMS_ERR("Test %s can't decode reply message", name);
                    }
                    mms_message_free(pdu);
                } else {
                    test->ret = RET_ERR;
                    MMS_ERR("Test %s expects no reply", name);
                }
            } else if (test->desc->reply_msg) {
                test->ret = RET_ERR;
                MMS_ERR("Test %s expects reply", name);
            }
        }
    }
    MMS_INFO("Test %s %s", name, (test->ret == RET_OK) ? "OK" : "FAILED");
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
    MMS_INFO("Test %s TIMEOUT", test->desc->name);
    if (test->http) test_http_close(test->http);
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
    const char* dir = desc->dir ? desc->dir : desc->name;
    char* ni = g_strconcat(DATA_DIR, dir, "/", desc->ni_file, NULL);
    char* rc = desc->rc_file ? g_strconcat(DATA_DIR, dir, "/",
        desc->rc_file, NULL) : NULL;
    memset(test, 0, sizeof(*test));
    test->notification_ind = g_mapped_file_new(ni, FALSE, &error);
    if (test->notification_ind) {
        if (rc) test->retrieve_conf = g_mapped_file_new(rc, FALSE, &error);
        if (test->retrieve_conf || !rc) {
            g_mapped_file_ref(test->notification_ind);
            test->desc = desc;
            test->cm = mms_connman_test_new();
            test->handler = mms_handler_test_new();
            test->disp = mms_dispatcher_new(config, test->cm, test->handler);
            test->loop = g_main_loop_new(NULL, FALSE);
            test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
            test->delegate.fn_done = test_done;
            mms_dispatcher_set_delegate(test->disp, &test->delegate);
            test->http = test_http_new(test->retrieve_conf,
                test->desc->content_type, test->desc->status);
            mms_connman_test_set_port(test->cm, test_http_get_port(test->http));
            if (desc->flags & TEST_DEFER_RECEIVE) {
                mms_handler_test_defer_receive(test->handler, test->disp);
            }
            test->ret = RET_ERR;
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
        g_mapped_file_unref(test->notification_ind);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    g_free(ni);
    g_free(rc);
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
    g_mapped_file_unref(test->notification_ind);
    if (test->retrieve_conf) g_mapped_file_unref(test->retrieve_conf);
}

static
int
test_retrieve_once(
    const MMSConfig* config,
    const TestDesc* desc)
{
    Test test;
    if (test_init(&test, config, desc)) {
        GBytes* push = g_bytes_new_static(
            g_mapped_file_get_contents(test.notification_ind),
            g_mapped_file_get_length(test.notification_ind));
        if (mms_dispatcher_handle_push(test.disp, "TestConnection", push)) {
            if (mms_dispatcher_start(test.disp)) {
                test.ret = RET_OK;
                g_main_loop_run(test.loop);
            } else {
                MMS_INFO("Test %s FAILED", desc->name);
            }
        } else {
            if (desc->flags & TEST_PUSH_HANDLING_FAILURE_OK) {
                MMS_INFO("Test %s OK", desc->name);
                test.ret = RET_OK;
            } else {
                MMS_INFO("Test %s FAILED", desc->name);
            }
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
test_retrieve(
    const MMSConfig* config,
    const char* name)
{
    int i, ret;
    if (name) {
        const TestDesc* found = NULL;
        for (i=0, ret = RET_ERR; i<G_N_ELEMENTS(retrieve_tests); i++) {
            const TestDesc* test = retrieve_tests + i;
            if (!strcmp(test->name, name)) {
                ret = test_retrieve_once(config, test);
                found = test;
                break;
            }
        }
        if (!found) MMS_ERR("No such test: %s", name);
    } else {
        for (i=0, ret = RET_OK; i<G_N_ELEMENTS(retrieve_tests); i++) {
            int test_status = test_retrieve_once(config, retrieve_tests + i);
            if (ret == RET_OK && test_status != RET_OK) ret = test_status;
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    MMSConfig config;
    const char* test_name = NULL;

    mms_lib_init();
    mms_lib_default_config(&config);
    mms_log_default.name = "test_retrieve";

    if (argc > 1 && !strcmp(argv[1], "-v")) {
        mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        memmove(argv + 1, argv + 2, (argc-2)*sizeof(argv[0]));
        argc--;
    } else {
        mms_log_default.level = MMS_LOGLEVEL_INFO;
        mms_util_log.level = 
        mms_task_decode_log.level =
        mms_task_retrieve_log.level =
        mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
        mms_log_stdout_timestamp = FALSE;
    }

    if (argc == 2 && argv[1][0] != '-') {
        test_name = argv[1];
    }

    if (argc == 1 || test_name) {
        int ret;
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_retrieve_XXXXXX"));
        MMS_VERBOSE("Temporary directory %s", tmpd);
        config.root_dir = tmpd;
        config.idle_secs = 0;
        ret = test_retrieve(&config, test_name);
        remove(tmpd);
        g_free(tmpd);
        return ret;
    } else {
        printf("Usage: test_retrieve [-v] [TEST]\n");
        return RET_ERR;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
