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

#include "mms_handler_dbus.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_handler_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-handler-dbus");

/* Generated code */
#include "org.nemomobile.MmsHandler.h"

/* Class definition */
typedef MMSHandlerClass MMSHandlerDbusClass;
typedef struct mms_handler_dbus {
    MMSHandler handler;
    OrgNemomobileMmsHandler* proxy;
    GHashTable* notify_pending;
    GHashTable* received_pending;
} MMSHandlerDbus;

G_DEFINE_TYPE(MMSHandlerDbus, mms_handler_dbus, MMS_TYPE_HANDLER);
#define MMS_TYPE_HANDLER_DBUS (mms_handler_dbus_get_type())
#define MMS_HANDLER_DBUS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_HANDLER_DBUS, MMSHandlerDbus))

/*
 * messageNotification call context
 */
struct mms_handler_message_notify_call {
    MMSHandlerDbus* dbus;
    GCancellable* cancellable;
    mms_handler_message_notify_complete_fn cb;
};

static
MMSHandlerMessageNotifyCall*
mms_handler_notify_call_create(
    MMSHandlerDbus* dbus,
    mms_handler_message_notify_complete_fn cb,
    void* param)
{
    MMSHandlerMessageNotifyCall* call = g_new(MMSHandlerMessageNotifyCall,1);
    mms_handler_ref(&(call->dbus = dbus)->handler);
    call->cancellable = g_cancellable_new();
    call->cb = cb;
    g_hash_table_replace(dbus->notify_pending, call, param);
    return call;
}

static
void
mms_handler_notify_call_delete(
    MMSHandlerMessageNotifyCall* call)
{
    mms_handler_unref(&call->dbus->handler);
    g_object_unref(call->cancellable);
    g_free(call);
}

static
void
mms_handler_notify_call_destroy(
    gpointer key)
{
    mms_handler_notify_call_delete(key);
}

/*
 * messageReceived call context
 */
struct mms_handler_message_received_call {
    MMSHandlerDbus* dbus;
    MMSMessage* msg;
    GCancellable* cancellable;
    mms_handler_message_received_complete_fn cb;
};

static
MMSHandlerMessageReceivedCall*
mms_handler_message_received_call_create(
    MMSHandlerDbus* dbus,
    MMSMessage* msg,
    mms_handler_message_received_complete_fn cb,
    void* param)
{
    MMSHandlerMessageReceivedCall* call =
    g_new(MMSHandlerMessageReceivedCall,1);
    mms_handler_ref(&(call->dbus = dbus)->handler);
    call->msg = mms_message_ref(msg);
    call->cancellable = g_cancellable_new();
    call->cb = cb;
    g_hash_table_replace(dbus->received_pending, call, param);
    return call;
}

static
void
mms_handler_message_received_call_delete(
    MMSHandlerMessageReceivedCall* call)
{
    mms_handler_unref(&call->dbus->handler);
    mms_message_unref(call->msg);
    g_object_unref(call->cancellable);
    g_free(call);
}

static
void
mms_handler_message_received_call_destroy(
    gpointer key)
{
    mms_handler_message_received_call_delete(key);
}

/**
 * Creates D-Bus handler
 */
MMSHandler*
mms_handler_dbus_new()
{
    return g_object_new(MMS_TYPE_HANDLER_DBUS, NULL);
}

/**
 * Creates D-Bus connection to MMS handler. Caller owns the returned
 * reference.
 */
static
OrgNemomobileMmsHandler*
mms_handler_dbus_connect(
    MMSHandler* handler)
{
    MMSHandlerDbus* dbus = MMS_HANDLER_DBUS(handler);
    if (!dbus->proxy) {
        GError* error = NULL;
        dbus->proxy = org_nemomobile_mms_handler_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
            "org.nemomobile.MmsHandler", "/", NULL, &error);
        if (!dbus->proxy) {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return dbus->proxy;
}

/**
 * New message notification. Cancellable.
 */

static
void
mms_handler_dbus_message_notify_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    MMSHandlerMessageNotifyCall* call = data;
    MMSHandlerDbus* dbus = call->dbus;
    GError* error = NULL;
    char* id = NULL;
    gboolean ok = org_nemomobile_mms_handler_call_message_notification_finish(
        ORG_NEMOMOBILE_MMS_HANDLER(proxy), &id, result, &error);
    if (ok) {
        MMS_DEBUG_("id=%s", id);
        MMS_ASSERT(id);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        MMS_ASSERT(!id);
        g_error_free(error);
    }
    if (call->cb) {
        void* param = g_hash_table_lookup(dbus->notify_pending, call);
        call->cb(call, id, param);
    }
    MMS_VERIFY(g_hash_table_remove(dbus->notify_pending, call));
    mms_handler_busy_dec(&dbus->handler);
    g_free(id);
}

static
MMSHandlerMessageNotifyCall*
mms_handler_dbus_message_notify(
    MMSHandler* handler,
    const char* imsi,
    const char* from,
    const char* subject,
    time_t expiry,
    GBytes* push,
    mms_handler_message_notify_complete_fn cb,
    void* param)
{
    MMSHandlerMessageNotifyCall* call = NULL;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (proxy) {
        gsize len = 0;
        const void* data = g_bytes_get_data(push, &len);
        GVariant* bytes = g_variant_ref_sink(g_variant_new_from_data(
            G_VARIANT_TYPE_BYTESTRING, data, len, TRUE, NULL, NULL));
        MMSHandlerDbus* dbus = MMS_HANDLER_DBUS(handler);

        mms_handler_busy_inc(handler);
        call = mms_handler_notify_call_create(dbus, cb, param);
        org_nemomobile_mms_handler_call_message_notification(
            proxy, imsi, from, subject, expiry, bytes, call->cancellable,
            mms_handler_dbus_message_notify_done, call);

        g_variant_unref(bytes);
    }
    return call;
}

static
void
mms_handler_dbus_message_notify_cancel(
    MMSHandler* handler,
    MMSHandlerMessageNotifyCall* call)
{
    MMS_ASSERT(call->cb);
    call->cb = NULL;
    g_cancellable_cancel(call->cancellable);
}

/**
 * Message receive notification. Cancellable.
 */

static
void
mms_handler_dbus_message_received_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    MMSHandlerMessageReceivedCall* call = data;
    MMSHandlerDbus* dbus = call->dbus;
    gboolean ok = org_nemomobile_mms_handler_call_message_received_finish(
        ORG_NEMOMOBILE_MMS_HANDLER(proxy), result, &error);
    if (!ok) {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    if (call->cb) {
        void* param = g_hash_table_lookup(dbus->received_pending, call);
        call->cb(call, call->msg, ok, param);
    }
    MMS_VERIFY(g_hash_table_remove(dbus->received_pending, call));
    mms_handler_busy_dec(&dbus->handler);
}

/* Message received notification */
static
MMSHandlerMessageReceivedCall*
mms_handler_dbus_message_received(
    MMSHandler* handler,
    MMSMessage* msg,
    mms_handler_message_received_complete_fn cb,
    void* param)
{
    MMSHandlerMessageReceivedCall* call = NULL;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(msg->id && msg->id[0]);
    if (msg->id && msg->id[0] && proxy) {
        MMSHandlerDbus* dbus = MMS_HANDLER_DBUS(handler);
        const char* nothing = NULL;
        const char* subject = msg->subject ? msg->subject : "";
        const char* from = msg->from ? msg->from : "<hidden>";
        const char** to = msg->to ? (const char**)msg->to : &nothing;
        const char** cc = msg->cc ? (const char**)msg->cc : &nothing;
        GSList* list = msg->parts;
        GVariant* parts;
        GVariantBuilder b;

        g_variant_builder_init(&b, G_VARIANT_TYPE("a(sss)"));
        while (list) {
            const MMSMessagePart* part = list->data;
            g_variant_builder_add(&b, "(sss)", part->file,
                part->content_type, part->content_id);
            list = list->next;
        }
        parts = g_variant_ref_sink(g_variant_builder_end(&b));

        mms_handler_busy_inc(handler);
        call = mms_handler_message_received_call_create(dbus, msg, cb, param);
        org_nemomobile_mms_handler_call_message_received(
            proxy, msg->id, msg->message_id, from, to, cc, subject,
            msg->date, msg->priority, msg->cls, msg->read_report_req,
            parts, NULL, mms_handler_dbus_message_received_done, call);

        g_variant_unref(parts);
    }
    return call;
}

static
void
mms_handler_dbus_message_received_cancel(
    MMSHandler* handler,
    MMSHandlerMessageReceivedCall* call)
{
    MMS_ASSERT(call->cb);
    call->cb = NULL;
    g_cancellable_cancel(call->cancellable);
}

/**
 * Generic D-Bus call completion callback for all other notifications.
 * They don't return anything so we just decrement the busy count and
 * drop the reference.
 */
static
void
mms_handler_dbus_call_done(
    GObject* proxy,
    GAsyncResult* res,
    gpointer data)
{
    MMSHandler* handler = data;
    GError* err = NULL;
    GVariant* ret = g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), res, &err);
    if (ret) {
        g_variant_unref(ret);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(err));
        g_error_free(err);
    }
    mms_handler_busy_dec(handler);
    mms_handler_unref(handler);
}

/* Updates message receive state in the database */
static
gboolean
mms_handler_dbus_message_receive_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_RECEIVE_STATE state)
{
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && proxy) {
        mms_handler_ref(handler);
        mms_handler_busy_inc(handler);
        org_nemomobile_mms_handler_call_message_receive_state_changed(
            proxy, id, state, NULL, mms_handler_dbus_call_done, handler);
        return TRUE;
    }
    return FALSE;
}

/* Updates message send state in the database */
static
gboolean
mms_handler_dbus_message_send_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_SEND_STATE state)
{
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && proxy) {
        mms_handler_ref(handler);
        mms_handler_busy_inc(handler);
        org_nemomobile_mms_handler_call_message_send_state_changed(proxy, id,
            state, NULL, mms_handler_dbus_call_done, handler);
        return TRUE;
    }
    return FALSE;
}

/* Message has been sent */
static
gboolean
mms_handler_dbus_message_sent(
    MMSHandler* handler,
    const char* id,
    const char* msgid)
{
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && msgid && msgid[0] && proxy) {
        mms_handler_ref(handler);
        mms_handler_busy_inc(handler);
        org_nemomobile_mms_handler_call_message_sent(proxy, id, msgid, NULL,
            mms_handler_dbus_call_done, handler);
        return TRUE;
    }
    return FALSE;
}

/* Delivery report has been received */
static
gboolean
mms_handler_dbus_delivery_report(
    MMSHandler* handler,
    const char* imsi,
    const char* msgid,
    const char* recipient,
    MMS_DELIVERY_STATUS status)
{
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (msgid && msgid[0] && recipient && recipient[0] && proxy) {
        mms_handler_ref(handler);
        mms_handler_busy_inc(handler);
        org_nemomobile_mms_handler_call_delivery_report(proxy, imsi, msgid,
            recipient, status, NULL, mms_handler_dbus_call_done, handler);
        return TRUE;
    }
    return FALSE;
}

/* Read report has been received */
static
gboolean
mms_handler_dbus_read_report(
    MMSHandler* handler,
    const char* imsi,
    const char* msgid,
    const char* recipient,
    MMS_READ_STATUS status)
{
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (msgid && msgid[0] && recipient && recipient[0] && proxy) {
        mms_handler_ref(handler);
        mms_handler_busy_inc(handler);
        org_nemomobile_mms_handler_call_read_report(proxy, imsi, msgid,
            recipient, status, NULL, mms_handler_dbus_call_done, handler);
        return TRUE;
    }
    return FALSE;
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_handler_dbus_dispose(
    GObject* object)
{
    MMSHandlerDbus* dbus = MMS_HANDLER_DBUS(object);
    if (dbus->proxy) {
        g_object_unref(dbus->proxy);
        dbus->proxy = NULL;
    }
    G_OBJECT_CLASS(mms_handler_dbus_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
mms_handler_dbus_finalize(
    GObject* object)
{
    MMSHandlerDbus* dbus = MMS_HANDLER_DBUS(object);
    g_hash_table_unref(dbus->notify_pending);
    g_hash_table_unref(dbus->received_pending);
    G_OBJECT_CLASS(mms_handler_dbus_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_handler_dbus_class_init(
    MMSHandlerDbusClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    klass->fn_message_notify = mms_handler_dbus_message_notify;
    klass->fn_message_notify_cancel = mms_handler_dbus_message_notify_cancel;
    klass->fn_message_received = mms_handler_dbus_message_received;
    klass->fn_message_received_cancel =
        mms_handler_dbus_message_received_cancel;
    klass->fn_message_receive_state_changed =
        mms_handler_dbus_message_receive_state_changed;
    klass->fn_message_send_state_changed =
        mms_handler_dbus_message_send_state_changed;
    klass->fn_message_sent = mms_handler_dbus_message_sent;
    klass->fn_delivery_report = mms_handler_dbus_delivery_report;
    klass->fn_read_report = mms_handler_dbus_read_report;
    object_class->dispose = mms_handler_dbus_dispose;
    object_class->finalize = mms_handler_dbus_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_handler_dbus_init(
    MMSHandlerDbus* dbus)
{
    dbus->notify_pending = g_hash_table_new_full(g_direct_hash,
        g_direct_equal, mms_handler_notify_call_destroy, NULL);
    dbus->received_pending = g_hash_table_new_full(g_direct_hash,
        g_direct_equal, mms_handler_message_received_call_destroy, NULL);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
