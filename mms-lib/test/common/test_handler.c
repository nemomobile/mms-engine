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
#include "mms_dispatcher.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_handler_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-handler-test");

/* Class definition */
typedef MMSHandlerClass MMSHandlerTestClass;
typedef struct mms_handler_test {
    MMSHandler handler;
    unsigned int last_id;
    GHashTable* recs;
    MMSDispatcher* dispatcher;
} MMSHandlerTest;

typedef struct mms_handler_record {
    char* id;
    char* imsi;
    MMSMessage* msg;
    GBytes* data;
    MMS_RECEIVE_STATE receive_state;
    guint receive_message_id;
    MMSDispatcher* dispatcher;
} MMSHandlerRecord;

G_DEFINE_TYPE(MMSHandlerTest, mms_handler_test, MMS_TYPE_HANDLER);
#define MMS_TYPE_HANDLER_TEST (mms_handler_test_get_type())
#define MMS_HANDLER_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    MMS_TYPE_HANDLER_TEST, MMSHandlerTest))

MMS_RECEIVE_STATE
mms_handler_test_receive_state(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecord* rec = NULL;
    if (id) {
        rec = g_hash_table_lookup(test->recs, id);
    } else if (g_hash_table_size(test->recs) == 1) {
        GList* values = g_hash_table_get_values(test->recs);
        rec = g_list_first(values)->data;
        g_list_free(values);
    }
    return rec ? rec->receive_state : MMS_RECEIVE_STATE_INVALID;
}

static
void 
mms_handler_test_receive_pending_check(
    gpointer key,
    gpointer value,
    gpointer user_data)
{
    MMSHandlerRecord* rec = value;
    gboolean* pending = user_data;
    if (rec->receive_message_id) *pending = TRUE;
}

gboolean
mms_handler_test_receive_pending(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecord* rec = NULL;
    if (id) {
        rec = g_hash_table_lookup(test->recs, id);
        return rec && rec->receive_message_id;
    } else {
        gboolean pending = FALSE;
        g_hash_table_foreach(test->recs,
            mms_handler_test_receive_pending_check, &pending);
        return pending;
    }
}

static
void
mms_handler_test_hash_remove_record(
    gpointer data)
{
    MMSHandlerRecord* rec = data;
    if (rec->receive_message_id) {
        g_source_remove(rec->receive_message_id);
        rec->receive_message_id = 0;
    }
    mms_dispatcher_unref(rec->dispatcher);
    mms_message_unref(rec->msg);
    g_bytes_unref(rec->data);
    g_free(rec->imsi);
    g_free(rec->id);
    g_free(rec);
}

static
gboolean
mms_handler_test_receive(
    gpointer data)
{
    MMSHandlerRecord* rec = data;
    MMSDispatcher* disp = rec->dispatcher;
    MMS_ASSERT(rec->receive_message_id);
    MMS_ASSERT(rec->dispatcher);
    MMS_DEBUG("Initiating receive of message %s", rec->id);
    rec->receive_message_id = 0;
    rec->dispatcher = NULL;
    mms_dispatcher_receive_message(disp, rec->id, rec->imsi,
        TRUE, rec->data, NULL);
    mms_dispatcher_start(disp);
    mms_dispatcher_unref(disp);
    return FALSE;
}

static
char*
mms_handler_test_message_notify(
    MMSHandler* handler,
    const char* imsi,
    const char* from,
    const char* subj,
    time_t expiry,
    GBytes* data)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecord* rec = g_new0(MMSHandlerRecord, 1);
    unsigned int rec_id = (++test->last_id);
    rec->id = g_strdup_printf("%u", rec_id);
    rec->imsi = g_strdup(imsi);
    rec->receive_state = MMS_RECEIVE_STATE_INVALID;
    rec->data = g_bytes_ref(data);
    MMS_DEBUG("Push %s imsi=%s from=%s subj=%s", rec->id, imsi, from, subj);
    g_hash_table_replace(test->recs, rec->id, rec);
    if (test->dispatcher) {
        rec->receive_message_id = g_idle_add(mms_handler_test_receive, rec);
        rec->dispatcher = mms_dispatcher_ref(test->dispatcher);
        return g_strdup("");
    } else {
        return g_strdup(rec->id);
    }
}

static
gboolean
mms_handler_test_message_received(
    MMSHandler* handler,
    MMSMessage* msg)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecord* rec = g_hash_table_lookup(test->recs, msg->id);
    MMS_DEBUG("Message %s from=%s subj=%s", msg->id, msg->from, msg->subject);
    MMS_ASSERT(rec);
    if (rec) {
        mms_message_unref(rec->msg);
        rec->msg = mms_message_ref(msg);
        return TRUE;
    } else {
        return FALSE;
    }
}

static
gboolean
mms_handler_test_message_receive_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_RECEIVE_STATE state)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecord* rec = g_hash_table_lookup(test->recs, id);
    if (rec) {
        rec->receive_state = state;
        MMS_DEBUG("Message %s state %d", id, state);
        return TRUE;
    } else {
        MMS_ERR("Invalid message id %s", id);
        return FALSE;
    }
}

void
mms_handler_test_finalize(
    GObject* object)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(object);
    mms_dispatcher_unref(test->dispatcher);
    g_hash_table_remove_all(test->recs);
    g_hash_table_unref(test->recs);
    G_OBJECT_CLASS(mms_handler_test_parent_class)->finalize(object);
}

static
void
mms_handler_test_class_init(
    MMSHandlerTestClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = mms_handler_test_finalize;
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
    test->recs = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, mms_handler_test_hash_remove_record);
}

MMSHandler*
mms_handler_test_new()
{
    return g_object_new(MMS_TYPE_HANDLER_TEST, NULL);
}

void
mms_handler_test_defer_receive(
    MMSHandler* handler,
    MMSDispatcher* dispatcher)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    mms_dispatcher_unref(test->dispatcher);
    test->dispatcher = mms_dispatcher_ref(dispatcher);
}

void
mms_handler_test_reset(
    MMSHandler* handler)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    g_hash_table_remove_all(test->recs);
    mms_dispatcher_unref(test->dispatcher);
    test->dispatcher = NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
