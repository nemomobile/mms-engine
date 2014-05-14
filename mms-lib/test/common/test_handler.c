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
    int flags;

#define MMS_HANDLER_TEST_FLAG_REJECT_NOTIFY (0x01)

} MMSHandlerTest;

typedef enum mms_handler_record_type {
    MMS_HANDLER_RECORD_SEND = 1,
    MMS_HANDLER_RECORD_RECEIVE
} MMSHandlerRecordType;

typedef struct mms_handler_record {
    MMSHandlerRecordType type;
    char* id;
    char* imsi;
} MMSHandlerRecord;

typedef struct mms_handler_record_send {
    MMSHandlerRecord rec;
    MMS_SEND_STATE state;
    MMS_DELIVERY_STATUS delivery_status;
    MMS_READ_STATUS read_status;
    char* msgid;
} MMSHandlerRecordSend;

typedef struct mms_handler_record_receive {
    MMSHandlerRecord rec;
    MMS_RECEIVE_STATE state;
    MMSDispatcher* dispatcher;
    MMSMessage* msg;
    GBytes* data;
    guint defer_id;
} MMSHandlerRecordReceive;

G_DEFINE_TYPE(MMSHandlerTest, mms_handler_test, MMS_TYPE_HANDLER);
#define MMS_TYPE_HANDLER_TEST (mms_handler_test_get_type())
#define MMS_HANDLER_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    MMS_TYPE_HANDLER_TEST, MMSHandlerTest))

static inline
MMSHandlerRecordSend*
mms_handler_test_record_send(MMSHandlerRecord* rec)
{
    MMS_ASSERT(rec->type == MMS_HANDLER_RECORD_SEND);
    return MMS_CAST(rec, MMSHandlerRecordSend, rec);
}

static inline
MMSHandlerRecordReceive*
mms_handler_test_record_receive(MMSHandlerRecord* rec)
{
    MMS_ASSERT(rec->type == MMS_HANDLER_RECORD_RECEIVE);
    return MMS_CAST(rec, MMSHandlerRecordReceive, rec);
}

static
MMSHandlerRecord*
mms_handler_test_get_record(
    MMSHandlerTest* test,
    const char* id,
    MMSHandlerRecordType type)
{
    if (id) {
        MMSHandlerRecord* rec = g_hash_table_lookup(test->recs, id);
        if (rec && rec->type == type) {
            return rec;
        }
    } else if (g_hash_table_size(test->recs) == 1) {
        GList* values = g_hash_table_get_values(test->recs);
        GList* value = g_list_first(values);
        MMSHandlerRecord* found = NULL;
        while (value) {
            MMSHandlerRecord* rec = value->data;
            if (rec && rec->type == type) {
                found = rec;
                break;
            }
            value = value->next;
        }
        g_list_free(values);
        return found;
    }
    return NULL;
}

typedef struct mms_handler_send_msgid_search {
    const char* msgid;
    MMSHandlerRecordSend* send;
} MMSHandlerSendMsgIdSearch;

static
void 
mms_handler_get_send_record_for_msgid_cb(
    gpointer key,
    gpointer value,
    gpointer user_data)
{
    MMSHandlerRecord* rec = value;
    if (rec->type == MMS_HANDLER_RECORD_SEND) {
        MMSHandlerRecordSend* send = mms_handler_test_record_send(rec);
        MMSHandlerSendMsgIdSearch* search = user_data;
        if (!strcmp(send->msgid, search->msgid)) {
            MMS_ASSERT(!search->send);
            search->send = send;
        }
    }
}

static
MMSHandlerRecordSend*
mms_handler_get_send_record_for_msgid(
    MMSHandlerTest* test,
    const char* msgid)
{
    MMSHandlerSendMsgIdSearch search;
    search.send = NULL;
    if (msgid) {
        search.msgid = msgid;
        g_hash_table_foreach(test->recs,
            mms_handler_get_send_record_for_msgid_cb, &search);
    }
    return search.send;
}

static
MMSHandlerRecordSend*
mms_handler_test_get_send_record(
    MMSHandlerTest* test,
    const char* id)
{
    MMSHandlerRecord* rec =
    mms_handler_test_get_record(test, id, MMS_HANDLER_RECORD_SEND);
    return rec ? mms_handler_test_record_send(rec) : NULL;
}

static
MMSHandlerRecordReceive*
mms_handler_test_get_receive_record(
    MMSHandlerTest* test,
    const char* id)
{
    MMSHandlerRecord* rec =
    mms_handler_test_get_record(test, id, MMS_HANDLER_RECORD_RECEIVE);
    return rec ? mms_handler_test_record_receive(rec) : NULL;
}

MMS_SEND_STATE
mms_handler_test_send_state(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordSend* send =
    mms_handler_test_get_send_record(MMS_HANDLER_TEST(handler), id);
    return send ? send->state : MMS_SEND_STATE_INVALID;
}

MMS_RECEIVE_STATE
mms_handler_test_receive_state(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordReceive* recv =
    mms_handler_test_get_receive_record(MMS_HANDLER_TEST(handler), id);
    return recv ? recv->state : MMS_RECEIVE_STATE_INVALID;
}

MMSMessage*
mms_handler_test_get_received_message(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordReceive* recv =
    mms_handler_test_get_receive_record(MMS_HANDLER_TEST(handler), id);
    return recv ? recv->msg : NULL;
}

const char*
mms_handler_test_send_msgid(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordSend* send =
    mms_handler_test_get_send_record(MMS_HANDLER_TEST(handler), id);
    return send ? send->msgid : NULL;
}

static
void 
mms_handler_test_receive_pending_check(
    gpointer key,
    gpointer value,
    gpointer user_data)
{
    MMSHandlerRecord* rec = value;
    if (rec->type == MMS_HANDLER_RECORD_RECEIVE) {
        MMSHandlerRecordReceive* recv = mms_handler_test_record_receive(rec);
        if (recv->defer_id) {
            gboolean* pending = user_data;
            *pending = TRUE;
        }
    }
}

gboolean
mms_handler_test_receive_pending(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    if (id) {
        MMSHandlerRecordReceive* recv;
        recv = mms_handler_test_get_receive_record(test, id);
        return recv && recv->defer_id;
    } else {
        gboolean pending = FALSE;
        g_hash_table_foreach(test->recs,
            mms_handler_test_receive_pending_check, &pending);
        return pending;
    }
}

MMS_DELIVERY_STATUS
mms_handler_test_delivery_status(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordSend* send =
    mms_handler_test_get_send_record(MMS_HANDLER_TEST(handler), id);
    return send ? send->delivery_status : MMS_DELIVERY_STATUS_INVALID;
}

MMS_READ_STATUS
mms_handler_test_read_status(
    MMSHandler* handler,
    const char* id)
{
    MMSHandlerRecordSend* send =
    mms_handler_test_get_send_record(MMS_HANDLER_TEST(handler), id);
    return send ? send->read_status : MMS_READ_STATUS_INVALID;
}

static
void
mms_handler_test_hash_remove_record(
    gpointer data)
{
    MMSHandlerRecord* rec = data;
    g_free(rec->imsi);
    g_free(rec->id);
    if (rec->type == MMS_HANDLER_RECORD_RECEIVE) {
        MMSHandlerRecordReceive* recv = mms_handler_test_record_receive(rec);
        if (recv->defer_id) {
            g_source_remove(recv->defer_id);
            recv->defer_id = 0;
        }
        mms_dispatcher_unref(recv->dispatcher);
        mms_message_unref(recv->msg);
        g_bytes_unref(recv->data);
        g_free(recv);
    } else {
        MMSHandlerRecordSend* send = mms_handler_test_record_send(rec);
        g_free(send->msgid);
        g_free(send);
    }
}

static
gboolean
mms_handler_test_receive(
    gpointer data)
{
    MMSHandlerRecordReceive* recv = data;
    MMSDispatcher* disp = recv->dispatcher;
    MMS_ASSERT(recv->defer_id);
    MMS_ASSERT(recv->dispatcher);
    MMS_DEBUG("Initiating receive of message %s", recv->rec.id);
    recv->defer_id = 0;
    recv->dispatcher = NULL;
    mms_dispatcher_receive_message(disp, recv->rec.id, recv->rec.imsi,
        TRUE, recv->data, NULL);
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
    if (test->flags & MMS_HANDLER_TEST_FLAG_REJECT_NOTIFY) {
        MMS_DEBUG("Rejecting push imsi=%s from=%s subj=%s", imsi, from, subj);
        return NULL;
    } else {
        MMSHandlerRecordReceive* recv = g_new0(MMSHandlerRecordReceive, 1);
        char* id = g_strdup_printf("%u", (++test->last_id));
        recv->rec.id = id;
        recv->rec.imsi = g_strdup(imsi);
        recv->rec.type = MMS_HANDLER_RECORD_RECEIVE;
        recv->state = MMS_RECEIVE_STATE_INVALID;
        recv->data = g_bytes_ref(data);
        MMS_DEBUG("Push %s imsi=%s from=%s subj=%s", id, imsi, from, subj);
        g_hash_table_replace(test->recs, id, &recv->rec);
        if (test->dispatcher) {
            MMS_DEBUG("Deferring push");
            recv->defer_id = g_idle_add(mms_handler_test_receive, recv);
            recv->dispatcher = mms_dispatcher_ref(test->dispatcher);
            return g_strdup("");
        } else {
            return g_strdup(id);
        }
    }
}

static
gboolean
mms_handler_test_message_received(
    MMSHandler* h,
    MMSMessage* msg)
{
    MMSHandlerRecordReceive* recv =
    mms_handler_test_get_receive_record(MMS_HANDLER_TEST(h), msg->id);
    MMS_DEBUG("Message %s from=%s subj=%s", msg->id, msg->from, msg->subject);
    MMS_ASSERT(recv);
    if (recv) {
        MMS_ASSERT(!recv->msg);
        mms_message_unref(recv->msg);
        recv->msg = mms_message_ref(msg);
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
    MMSHandlerRecordReceive* recv =
    mms_handler_test_get_receive_record(MMS_HANDLER_TEST(handler), id);
    if (recv) {
        recv->state = state;
        MMS_DEBUG("Message %s receive state %d", id, state);
        return TRUE;
    } else {
        MMS_ERR("No such incoming message: %s", id);
        return FALSE;
    }
}

const char*
mms_handler_test_send_new(
    MMSHandler* handler,
    const char* imsi)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecordSend* send = g_new0(MMSHandlerRecordSend, 1);
    char* id = g_strdup_printf("%u", (++test->last_id));
    send->rec.id = id;
    send->rec.imsi = g_strdup(imsi);
    send->rec.type = MMS_HANDLER_RECORD_SEND;
    send->state = MMS_SEND_STATE_INVALID;
    send->delivery_status = MMS_DELIVERY_STATUS_INVALID;
    send->read_status = MMS_READ_STATUS_INVALID;
    MMS_DEBUG("New send %s imsi=%s", id, imsi);
    g_hash_table_replace(test->recs, id, &send->rec);
    return id;
}

static
gboolean
mms_handler_test_message_send_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_SEND_STATE state)
{
    MMSHandlerRecordSend* send =
    mms_handler_test_get_send_record(MMS_HANDLER_TEST(handler), id);
    if (send) {
        send->state = state;
        MMS_DEBUG("Message %s send state %d", id, state);
        return TRUE;
    } else {
        MMS_ERR("No such outbound message: %s", id);
        return FALSE;
    }
}

static
gboolean
mms_handler_test_message_sent(
    MMSHandler* handler,
    const char* id,
    const char* msgid)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecordSend* send = mms_handler_test_get_send_record(test, id);
    MMS_DEBUG("Message %s sent, msgid=%s", id, msgid);
    MMS_ASSERT(send);
    if (send) {
        MMS_ASSERT(!send->msgid);
        if (!send->msgid) {
            send->msgid = g_strdup(msgid);
            return TRUE;
        }
    }
    return FALSE;
}

static
gboolean
mms_handler_test_delivery_report(
    MMSHandler* handler,
    const char* imsi,
    const char* msgid,
    const char* recipient,
    MMS_DELIVERY_STATUS status)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecordSend* send =
    mms_handler_get_send_record_for_msgid(test, msgid);
    MMS_DEBUG("Message %s delivered to %s", msgid, recipient); 
    if (send) {
        MMS_ASSERT(send->delivery_status == MMS_DELIVERY_STATUS_INVALID);
        if (send->delivery_status == MMS_DELIVERY_STATUS_INVALID) {
            send->delivery_status = status;
            return TRUE;
        }
    } else {
        MMS_DEBUG("Unknown message id %s (this may be OK)", msgid); 
    }
    return FALSE;
}

static
gboolean
mms_handler_test_read_report(
    MMSHandler* handler,
    const char* imsi,
    const char* msgid,
    const char* recipient,
    MMS_READ_STATUS status)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    MMSHandlerRecordSend* send =
    mms_handler_get_send_record_for_msgid(test, msgid);
    MMS_DEBUG("Message %s read by %s", msgid, recipient); 
    if (send) {
        MMS_ASSERT(send->read_status == MMS_READ_STATUS_INVALID);
        if (send->read_status == MMS_READ_STATUS_INVALID) {
            send->read_status = status;
            return TRUE;
        }
    } else {
        MMS_DEBUG("Unknown message id %s (this may be OK)", msgid); 
    }
    return FALSE;
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
    klass->fn_message_sent = mms_handler_test_message_sent;
    klass->fn_message_send_state_changed =
        mms_handler_test_message_send_state_changed;
    klass->fn_message_received = mms_handler_test_message_received;
    klass->fn_message_receive_state_changed =
        mms_handler_test_message_receive_state_changed;
    klass->fn_delivery_report =  mms_handler_test_delivery_report;
    klass->fn_read_report =  mms_handler_test_read_report;
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
mms_handler_test_reject_receive(
    MMSHandler* handler)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    test->flags |= MMS_HANDLER_TEST_FLAG_REJECT_NOTIFY;
}

void
mms_handler_test_reset(
    MMSHandler* handler)
{
    MMSHandlerTest* test = MMS_HANDLER_TEST(handler);
    g_hash_table_remove_all(test->recs);
    mms_dispatcher_unref(test->dispatcher);
    test->dispatcher = NULL;
    test->flags = 0;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
