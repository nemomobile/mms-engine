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
#include "mms_dispatcher.h"

#include <gio/gio.h>
#include <libsoup/soup-status.h>

#define RET_OK      (0)
#define RET_ERR     (1)
#define RET_TIMEOUT (2)

#define DATA_DIR "data/"

typedef struct test_desc {
    const char* name;
    const MMSAttachmentInfo* parts;
    int nparts;
    const char* subject;
    const char* to;
    const char* cc;
    const char* bcc;
    unsigned int flags;
    const char* resp_file;
    const char* resp_type;
    unsigned int resp_status;
    MMS_SEND_STATE expected_state;
    const char* msgid;
} TestDesc;

#define TEST_FLAG_REQUEST_DELIVERY_REPORT MMS_SEND_FLAG_REQUEST_DELIVERY_REPORT
#define TEST_FLAG_REQUEST_READ_REPORT     MMS_SEND_FLAG_REQUEST_READ_REPORT
#define TEST_DISPATCHER_FLAGS       (\
  TEST_FLAG_REQUEST_DELIVERY_REPORT |\
  TEST_FLAG_REQUEST_READ_REPORT     )

typedef struct test {
    const TestDesc* desc;
    const MMSConfig* config;
    MMSAttachmentInfo* parts;
    char** files;
    MMSDispatcherDelegate delegate;
    MMSConnMan* cm;
    MMSHandler* handler;
    MMSDispatcher* disp;
    GMainLoop* loop;
    guint timeout_id;
    TestHttp* http;
    char* id;
    GMappedFile* resp_file;
    int ret;
} Test;

static const MMSAttachmentInfo test_files_success [] = {
    { "0001.jpg", "image/jpeg", "image" },
    { "test.txt", "text/plain;charset=utf-8", "text" }
};

static const TestDesc send_tests[] = {
    {
        "Success",
        test_files_success,
        G_N_ELEMENTS(test_files_success),
        "Test of successful delivery",
        "+1234567890",
        NULL,
        NULL,
        0,
        "m-send.conf",
        MMS_CONTENT_TYPE,
        SOUP_STATUS_OK,
        MMS_SEND_STATE_SENDING,
        "TestMessageId"
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
        MMS_SEND_STATE state;
        state = mms_handler_test_send_state(test->handler, test->id);
        if (state != desc->expected_state) {
            test->ret = RET_ERR;
            MMS_ERR("%s state %d, expected %d", name, state,
                desc->expected_state);
        } else if (desc->msgid) {
            const char* msgid =
            mms_handler_test_send_msgid(test->handler, test->id);
            if (!msgid || strcmp(msgid, desc->msgid)) {
                test->ret = RET_ERR;
                MMS_ERR("%s msgid %s, expected %s", name, msgid, desc->msgid);
            }
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
    char* fn = g_strconcat(DATA_DIR, desc->name, "/", desc->resp_file, NULL);
    memset(test, 0, sizeof(*test));
    test->resp_file = g_mapped_file_new(fn, FALSE, &error);
    if (test->resp_file) {
        int i;
        guint port;
        test->parts = g_new0(MMSAttachmentInfo, desc->nparts);
        test->files = g_new0(char*, desc->nparts);
        for (i=0; i<desc->nparts; i++) {
            test->files[i] = g_strconcat(DATA_DIR, desc->name, "/",
               desc->parts[i].file_name, NULL);
            test->parts[i] = desc->parts[i];
            test->parts[i].file_name = test->files[i];
        }
        test->config = config;
        test->desc = desc;
        test->cm = mms_connman_test_new();
        test->handler = mms_handler_test_new();
        test->disp = mms_dispatcher_new(config, test->cm, test->handler);
        test->loop = g_main_loop_new(NULL, FALSE);
        test->delegate.fn_done = test_done;
        mms_dispatcher_set_delegate(test->disp, &test->delegate);
        test->http = test_http_new(test->resp_file, desc->resp_type,
            desc->resp_status);
        port = test_http_get_port(test->http);
        mms_connman_test_set_port(test->cm, port, TRUE);
        test->ret = RET_ERR;
        ok = TRUE;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    g_free(fn);
    return ok;
}

static
void
test_finalize(
    Test* test)
{
    int i;
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
    g_mapped_file_unref(test->resp_file);
    for (i=0; i<test->desc->nparts; i++) g_free(test->files[i]);
    g_free(test->files);
    g_free(test->parts);
    g_free(test->id);
}

static
int
test_run_once(
    const MMSConfig* config,
    const TestDesc* desc,
    gboolean debug)
{
    Test test;
    if (test_init(&test, config, desc)) {
        GError* error = NULL;
        const char* imsi = "IMSI";
        const char* id = mms_handler_test_send_new(test.handler, imsi);
        char* imsi2 = mms_dispatcher_send_message(test.disp, id, imsi,
            desc->to, desc->cc, desc->bcc, desc->subject,
            desc->flags & TEST_DISPATCHER_FLAGS, test.parts,
            desc->nparts, &error);
        MMS_ASSERT(!strcmp(imsi2, imsi));
        test.id = g_strdup(id);
        if (!strcmp(imsi2, imsi) && mms_dispatcher_start(test.disp)) {
            if (!debug) {
                test.timeout_id = g_timeout_add_seconds(10,
                    test_timeout, &test);
            }
            test.ret = RET_OK;
            g_main_loop_run(test.loop);
        } else {
            MMS_INFO("%s FAILED", desc->name);
        }
        g_free(imsi2);
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
    const char* name,
    gboolean debug)
{
    int i, ret;
    if (name) {
        const TestDesc* found = NULL;
        for (i=0, ret = RET_ERR; i<G_N_ELEMENTS(send_tests); i++) {
            const TestDesc* test = send_tests + i;
            if (!strcmp(test->name, name)) {
                ret = test_run_once(config, test, debug);
                found = test;
                break;
            }
        }
        if (!found) MMS_ERR("No such test: %s", name);
    } else {
        for (i=0, ret = RET_OK; i<G_N_ELEMENTS(send_tests); i++) {
            int test_status = test_run_once(config, send_tests + i, debug);
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
    gboolean debug = FALSE;

    GOptionContext* options;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "keep", 'k', 0, G_OPTION_ARG_NONE, &keep_temp,
          "Keep temporary files", NULL },
        { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
          "Disable timeout for debugging", NULL },
        { NULL }
    };

    mms_lib_init(argv[0]);
    options = g_option_context_new("[TEST] - MMS send test");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, NULL) && argc < 3) {
        MMSConfig config;
        const char* test_name = (argc == 2) ? argv[1] : NULL;
        char* tmpd = g_mkdtemp(g_strdup("/tmp/test_send_XXXXXX"));
        MMS_VERBOSE("Temporary directory %s", tmpd);

        mms_lib_default_config(&config);
        config.keep_temp_files = keep_temp;
        config.root_dir = tmpd;
        config.idle_secs = 0;

        mms_log_default.name = "test_send";
        if (verbose) {
            mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        } else {
            mms_task_send_log.level = MMS_LOGLEVEL_ERR;
            mms_log_default.level = MMS_LOGLEVEL_INFO;
            mms_log_stdout_timestamp = FALSE;
        }

        ret = test_run(&config, test_name, debug);
        remove(tmpd);
        g_free(tmpd);
    } else {
        printf("Usage: test_send [-v] [TEST]\n");
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
