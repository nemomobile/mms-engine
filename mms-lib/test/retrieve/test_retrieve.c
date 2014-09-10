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

#include "mms_codec.h"
#include "mms_file_util.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_settings.h"
#include "mms_dispatcher.h"

#include <libsoup/soup-status.h>

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

#define MMS_MESSAGE_TYPE_NONE (0)

#define DATA_DIR "data/"

typedef struct test_part_desc {
    const char* content_type;
    const char* content_id;
    const char* file_name;
} TestPartDesc;

typedef struct test_desc {
    const char* name;
    const char* dir;
    const char* ni_file;
    const char* rc_file;
    unsigned int status;
    const char* content_type;
    const char* attic_file;
    MMS_RECEIVE_STATE expected_state;
    enum mms_message_type reply_msg;
    const TestPartDesc* parts;
    unsigned int nparts;
    int flags;

#define TEST_PUSH_HANDLING_FAILURE_OK (0x01)
#define TEST_DEFER_RECEIVE            (0x02)
#define TEST_REJECT_RECEIVE           (0x04)
#define TEST_CONNECTION_FAILURE       (0x08)
#define TEST_OFFLINE                  (0x10)
#define TEST_CANCEL_RECEIVED          (0x20)

} TestDesc;

typedef struct test {
    const TestDesc* desc;
    const MMSConfig* config;
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

static const TestPartDesc retrieve_success1_parts [] = {
    { "application/smil;charset=utf-8", "<0>", "0.smil" },
    { "text/plain;charset=utf-8", "<text_0011.txt>", "text_0011.txt" },
    { "image/jpeg", "<131200181.jpg>", "131200181.jpg" },
    { "text/plain;charset=utf-8", "<text_0021.txt>", "text_0021.txt" },
    { "image/jpeg", "<140100041.jpg>", "140100041.jpg" }
};

static const TestPartDesc retrieve_success2_parts [] = {
    { "image/jpeg", "<2014_03_.jpg>", "2014_03_.jpg" },
    { "text/plain;charset=utf-8", "<Test_mms.txt>", "Test_mms.txt" },
    { "application/smil;charset=utf-8", "<332047400>", "332047400" },
};

static const TestPartDesc retrieve_success3_parts [] = {
    { "application/smil;charset=utf-8", "<0>", "0" },
    { "image/jpeg", "<1>", "1.jpg" },
    { "text/plain;charset=utf-8", "<2>", "text_001.txt" }
};

static const TestPartDesc retrieve_invalid_subject [] = {
    { "application/smil;charset=us-ascii", "<start>", "smil.smi" },
    { "image/jpeg", "<1>", "1" }
};

#define TEST_PARTS(parts) parts, G_N_ELEMENTS(parts)
#define TEST_PARTS_NONE NULL, 0

static const TestDesc retrieve_tests[] = {
    {
        "Success1",
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_success1_parts),
        0
    },{
        "Success2", /* Generated by Nokia C6 (Symbian "Belle") */
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_success2_parts),
        0
    },{
        "Success3", /* Generated by Nokia N9 */
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_success3_parts),
        0
    },{
        "InvalidSubject",
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_invalid_subject),
        0
   },{
        "DeferSuccess",
        "Success1",
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_success1_parts),
        TEST_DEFER_RECEIVE
    },{
        "CancelReceive",
        "Success1",
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING,
        MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND,
        TEST_PARTS(retrieve_success1_parts),
        TEST_DEFER_RECEIVE | TEST_CANCEL_RECEIVED
     },{
        "Expired1",
        NULL,
        "m-notification.ind",
        NULL,
        SOUP_STATUS_NOT_FOUND,
        NULL,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        0
     },{
        "Expired2",
        NULL,
        "m-notification.ind",
        NULL,
        SOUP_STATUS_OK,
        NULL,
        NULL,
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NOTIFYRESP_IND,
        TEST_PARTS_NONE,
        TEST_REJECT_RECEIVE
    },{
        "SoonExpired",
        NULL,
        "m-notification.ind",
        NULL,
        SOUP_STATUS_TRY_AGAIN,
        NULL,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        0
    },{
        "Offline",
        NULL,
        "m-notification.ind",
        NULL,
        0,
        NULL,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        TEST_OFFLINE
    },{
        "Timeout",
        NULL,
        "m-notification.ind",
        NULL,
        0,
        NULL,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        TEST_CONNECTION_FAILURE
    },{
        "NotAllowed",
        NULL,
        "m-notification.ind",
        "not-allowed.html",
        SOUP_STATUS_BAD_REQUEST,
        "text/html",
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        0
    },{
        "NotFound",
        NULL,
        "m-notification.ind",
        "not-found.html",
        SOUP_STATUS_NOT_FOUND,
        "text/html",
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        0
    },{
        "MessageNotFound",
        NULL,
        "m-notification.ind",
        "m-retrieve.conf",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        0
    },{
        "GarbageRetrieve",
        NULL,
        "m-notification.ind",
        "garbage",
        SOUP_STATUS_OK,
        MMS_CONTENT_TYPE,
        NULL,
        MMS_RECEIVE_STATE_DECODING_ERROR,
        MMS_MESSAGE_TYPE_NOTIFYRESP_IND,
        TEST_PARTS_NONE,
        0
    },{
        "GarbagePush",
        NULL,
        "garbage",
        NULL,
        0,
        NULL,
        "000/push.pdu",
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        TEST_PUSH_HANDLING_FAILURE_OK
    },{
        "UnsupportedPush",
        NULL,
        "unsupported",
        NULL,
        0,
        NULL,
        "000/push.pdu",
        MMS_RECEIVE_STATE_INVALID,
        MMS_MESSAGE_TYPE_NONE,
        TEST_PARTS_NONE,
        TEST_PUSH_HANDLING_FAILURE_OK
    }
};

static
int
test_validate_parts(
    Test* test)
{
    const TestDesc* desc = test->desc;
    MMSMessage* msg = mms_handler_test_get_received_message(test->handler,NULL);
    if (!desc->nparts && (!msg || !g_slist_length(msg->parts))) {
        return RET_OK;
    } else {
        const unsigned int nparts = g_slist_length(msg->parts);
        if (desc->nparts == nparts) {
            const TestPartDesc* expect = test->desc->parts;
            GSList* list = msg->parts;
            while (list) {
                const MMSMessagePart* part = list->data;
                const char* fname;
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
                fname = g_basename(part->file);
                G_GNUC_END_IGNORE_DEPRECATIONS;
                if (strcmp(expect->content_type, part->content_type)) {
                    MMS_ERR("Content type mismatch: expected %s, got %s",
                        expect->content_type, part->content_type);
                    return RET_ERR;
                }
                if (strcmp(expect->file_name, fname)) {
                    MMS_ERR("File name mismatch: expected %s, got %s",
                        expect->file_name, fname);
                    return RET_ERR;
                }
                if (!part->content_id) {
                    MMS_ERR("Missing content id");
                    return RET_ERR;
                }
                if (strcmp(expect->content_id, part->content_id)) {
                    MMS_ERR("Content-ID mismatch: expected %s, got %s",
                        expect->content_id, part->content_id);
                    return RET_ERR;
                }
                list = list->next;
                expect++;
            }
            return RET_OK;
        }
        MMS_ERR("%u parts expected, got %u", desc->nparts, nparts);
        return RET_ERR;
    }
}

static
void
test_finish(
    Test* test)
{
    const TestDesc* desc = test->desc;
    const char* name = desc->name;
    if (test->ret == RET_OK) {
        MMS_RECEIVE_STATE state;
        state = mms_handler_test_receive_state(test->handler, NULL);
        if (state != desc->expected_state) {
            test->ret = RET_ERR;
            MMS_ERR("%s state %d, expected %d", name, state,
                desc->expected_state);
        } else {
            const void* resp_data = NULL;
            gsize resp_len = 0;
            GBytes* reply = test_http_get_post_data(test->http);
            if (reply) resp_data = g_bytes_get_data(reply, &resp_len);
            if (resp_len > 0) {
                if (desc->reply_msg) {
                    MMSPdu* pdu = g_new0(MMSPdu, 1);
                    if (mms_message_decode(resp_data, resp_len, pdu)) {
                        if (pdu->type == desc->reply_msg) {
                            test->ret = test_validate_parts(test);
                        } else {
                            test->ret = RET_ERR;
                            MMS_ERR("%s reply %u, expected %u", name,
                                pdu->type, desc->reply_msg);
                        }
                    } else {
                        test->ret = RET_ERR;
                        MMS_ERR("%s can't decode reply message", name);
                    }
                    mms_message_free(pdu);
                } else {
                    test->ret = RET_ERR;
                    MMS_ERR("%s expects no reply", name);
                }
            } else if (desc->reply_msg) {
                test->ret = RET_ERR;
                MMS_ERR("%s expects reply", name);
            }
        }
    }
    if (test->ret == RET_OK && desc->attic_file) {
        gboolean ok = FALSE;
        const char* dir = desc->dir ? desc->dir : desc->name;
        char* f1 = g_strconcat(DATA_DIR, dir, "/", desc->ni_file, NULL);
        char* f2 = g_strconcat(test->config->root_dir, "/" MMS_ATTIC_DIR "/",
            desc->attic_file, NULL);
        GMappedFile* m1 = g_mapped_file_new(f1, FALSE, NULL);
        if (m1) {
            GMappedFile* m2 = g_mapped_file_new(f2, FALSE, NULL);
            if (m2) {
                const gsize len = g_mapped_file_get_length(m1);
                if (len == g_mapped_file_get_length(m2)) {
                    const void* ptr1 = g_mapped_file_get_contents(m1);
                    const void* ptr2 = g_mapped_file_get_contents(m2);
                    ok = !memcmp(ptr1, ptr2, len);
                }
                g_mapped_file_unref(m2);
            }
            g_mapped_file_unref(m1);
        }
        if (ok) {
            char* dir = g_path_get_dirname(f2);
            remove(f2);
            rmdir(dir);
            g_free(dir);
        } else {
            test->ret = RET_ERR;
            MMS_ERR("%s is not identical to %s", f2, f1);
        }
        g_free(f1);
        g_free(f2);
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
    if (test->http) test_http_close(test->http);
    mms_connman_test_close_connection(test->cm);
    mms_dispatcher_cancel(test->disp, NULL);
    return FALSE;
}

static
gboolean
test_notify(
    MMSHandler* handler,
    const char* imsi,
    const char* from,
    const char* subject,
    time_t expiry,
    GBytes* data,
    void* param)
{
    Test* test = param;
    return !(test->desc->flags & TEST_REJECT_RECEIVE);
}

typedef struct test_cancel_msg {
    MMSDispatcher* disp;
    MMSMessage* msg;
} TestCancelMsg;

static
gboolean
test_cancel_msg_proc(
    void* param)
{
   TestCancelMsg* cancel = param;
   MMS_VERBOSE("Cancelling message %s", cancel->msg->id);
   mms_dispatcher_cancel(cancel->disp, cancel->msg->id);
   mms_dispatcher_unref(cancel->disp);
   mms_message_unref(cancel->msg);
   g_free(cancel);
   return FALSE;
}

static
void
test_msgreceived(
    MMSHandler* handler,
    MMSMessage* msg,
    void* param)
{
    Test* test = param;
    if (test->desc->flags & TEST_CANCEL_RECEIVED) {
        TestCancelMsg* cancel = g_new(TestCancelMsg,1);
        cancel->disp = mms_dispatcher_ref(test->disp);
        cancel->msg = mms_message_ref(msg);
        g_idle_add_full(G_PRIORITY_HIGH, test_cancel_msg_proc, cancel, NULL);
    }
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
    MMS_DEBUG(">>>>>>>>>> %s <<<<<<<<<<", desc->name);
    memset(test, 0, sizeof(*test));
    test->config = config;
    test->notification_ind = g_mapped_file_new(ni, FALSE, &error);
    if (test->notification_ind) {
        if (rc) test->retrieve_conf = g_mapped_file_new(rc, FALSE, &error);
        if (test->retrieve_conf || !rc) {
            MMSSettings* settings = mms_settings_default_new(config);
            g_mapped_file_ref(test->notification_ind);
            test->desc = desc;
            test->cm = mms_connman_test_new();
            test->handler = mms_handler_test_new();
            test->disp = mms_dispatcher_new(settings, test->cm, test->handler);
            test->loop = g_main_loop_new(NULL, FALSE);
            test->timeout_id = g_timeout_add_seconds(10, test_timeout, test);
            test->delegate.fn_done = test_done;
            mms_dispatcher_set_delegate(test->disp, &test->delegate);
            if (!(desc->flags & TEST_CONNECTION_FAILURE)) {
                test->http = test_http_new(test->retrieve_conf,
                    test->desc->content_type, test->desc->status);
                mms_connman_test_set_port(test->cm,
                    test_http_get_port(test->http), TRUE);
            }
            if (desc->flags & TEST_OFFLINE) {
                mms_connman_test_set_offline(test->cm, TRUE);
            }
            if (desc->flags & TEST_DEFER_RECEIVE) {
                mms_handler_test_defer_receive(test->handler, test->disp);
            }
            mms_handler_test_set_prenotify_fn(test->handler, test_notify, test);
            mms_handler_test_set_msgreceived_fn(test->handler,
                test_msgreceived, test);
            mms_settings_unref(settings);
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
            g_error_free(error);
            if (desc->flags & TEST_PUSH_HANDLING_FAILURE_OK) {
                test.ret = RET_OK;
                test_finish(&test);
            } else {
                MMS_INFO("%s FAILED", desc->name);
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
    int ret = RET_ERR;
    gboolean keep_temp = FALSE;
    gboolean verbose = FALSE;
    GError* error = NULL;
    GOptionContext* options;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "keep", 'k', 0, G_OPTION_ARG_NONE, &keep_temp,
          "Keep temporary files", NULL },
        { NULL }
    };

    mms_lib_init(argv[0]);
    options = g_option_context_new("[TEST] - MMS retrieve test");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        MMSConfig config;
        const char* test_name = (argc == 2) ? argv[1] : NULL;
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_retrieve_XXXXXX"));
        MMS_VERBOSE("Temporary directory %s", tmpd);
 
        mms_lib_default_config(&config);
        config.root_dir = tmpd;
        config.keep_temp_files = keep_temp;
        config.idle_secs = 0;
        config.attic_enabled = TRUE;

        mms_log_set_type(MMS_LOG_TYPE_STDOUT, "test_retrieve");
        if (verbose) {
            mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        } else {
            mms_log_default.level = MMS_LOGLEVEL_INFO;
            mms_task_http_log.level =
            mms_task_decode_log.level =
            mms_task_retrieve_log.level =
            mms_task_notification_log.level = MMS_LOGLEVEL_NONE;
            mms_log_stdout_timestamp = FALSE;
        }

        if (argc < 2) {
            ret = test_retrieve(&config, test_name);
        } else {
            int i;
            for (i=1, ret = RET_OK; i<argc; i++) {
                int test_status =  test_retrieve(&config, argv[i]);
                if (ret == RET_OK && test_status != RET_OK) ret = test_status;
            }
        }

        remove(tmpd);
        g_free(tmpd);
    } else {
        fprintf(stderr, "%s\n", MMS_ERRMSG(error));
        g_error_free(error);
        ret = RET_ERR;
    }
    g_option_context_free(options);
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
