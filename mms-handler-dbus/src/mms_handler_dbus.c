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
} MMSHandlerDbus;

G_DEFINE_TYPE(MMSHandlerDbus, mms_handler_dbus, MMS_TYPE_HANDLER);
#define MMS_TYPE_HANDLER_DBUS (mms_handler_dbus_get_type())
#define MMS_HANDLER_DBUS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_HANDLER_DBUS, MMSHandlerDbus))

MMSHandler*
mms_handler_dbus_new()
{
    MMSHandlerDbus* db = g_object_new(MMS_TYPE_HANDLER_DBUS, NULL);
    return &db->handler;
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

static
char*
mms_handler_dbus_message_notify(
    MMSHandler* handler,
    const char* imsi,
    const char* from,
    const char* subject,
    time_t expiry,
    GBytes* push)
{
    char* id = NULL;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (proxy) {
        gsize len = 0;
        const void* data = g_bytes_get_data(push, &len);
        GError* error = NULL;
        GVariant* bytes = g_variant_ref_sink(g_variant_new_from_data(
            G_VARIANT_TYPE_BYTESTRING, data, len, TRUE, NULL, NULL));
        if (!org_nemomobile_mms_handler_call_message_notification_sync(
            proxy, imsi, from, subject, expiry, bytes, &id, NULL, &error)) {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
        g_variant_unref(bytes);
    }
    return id;
}

static
gboolean
mms_handler_dbus_message_received(
    MMSHandler* handler,
    MMSMessage* msg)
{
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(msg->id && msg->id[0]);
    if (msg->id && msg->id[0] && proxy) {
        const char* nothing = NULL;
        const char* subject = msg->subject ? msg->subject : "";
        const char* from = msg->from ? msg->from : "<hidden>";
        const char** to = msg->to ? (const char**)msg->to : &nothing;
        const char** cc = msg->cc ? (const char**)msg->cc : &nothing;
        GError* error = NULL;
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
        ok = org_nemomobile_mms_handler_call_message_received_sync(
            proxy, msg->id, msg->message_id, from, to, cc, subject,
            msg->date, msg->priority, msg->cls, msg->read_report_req,
            parts, NULL, &error);
        if (!ok) {
            MMS_ERR("Failed to notify commhistoryd: %s", MMS_ERRMSG(error));
            g_error_free(error);
        }

        g_variant_unref(parts);
    }
    return ok;
}

/* Updates message receive state in the database */
static
gboolean
mms_handler_dbus_message_receive_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_RECEIVE_STATE state)
{
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && proxy) {
        GError* error = NULL;
        if (org_nemomobile_mms_handler_call_message_receive_state_changed_sync(
            proxy, id, state, NULL, &error)) {
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return ok;
}

/* Updates message send state in the database */
static
gboolean
mms_handler_dbus_message_send_state_changed(
    MMSHandler* handler,
    const char* id,
    MMS_SEND_STATE state)
{
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && proxy) {
        GError* error = NULL;
        if (org_nemomobile_mms_handler_call_message_send_state_changed_sync(
            proxy, id, state, NULL, &error)) {
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return ok;
}

/* Message has been sent */
static
gboolean
mms_handler_dbus_message_sent(
    MMSHandler* handler,
    const char* id,
    const char* msgid)
{
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    MMS_ASSERT(id && id[0]);
    if (id && id[0] && msgid && msgid[0] && proxy) {
        GError* error = NULL;
        if (org_nemomobile_mms_handler_call_message_sent_sync(proxy,
            id, msgid, NULL, &error)) {
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return ok;
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
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (msgid && msgid[0] && recipient && recipient[0] && proxy) {
        GError* error = NULL;
        if (org_nemomobile_mms_handler_call_delivery_report_sync(proxy,
            imsi, msgid, recipient, status, NULL, &error)) {
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return ok;
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
    gboolean ok = FALSE;
    OrgNemomobileMmsHandler* proxy = mms_handler_dbus_connect(handler);
    if (msgid && msgid[0] && recipient && recipient[0] && proxy) {
        GError* error = NULL;
        if (org_nemomobile_mms_handler_call_read_report_sync(proxy,
            imsi, msgid, recipient, status, NULL, &error)) {
            ok = TRUE;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
    }
    return ok;
}

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

static
void
mms_handler_dbus_class_init(
    MMSHandlerDbusClass* klass)
{
    klass->fn_message_notify = mms_handler_dbus_message_notify;
    klass->fn_message_received = mms_handler_dbus_message_received;
    klass->fn_message_receive_state_changed =
        mms_handler_dbus_message_receive_state_changed;
    klass->fn_message_send_state_changed =
        mms_handler_dbus_message_send_state_changed;
    klass->fn_message_sent = mms_handler_dbus_message_sent;
    klass->fn_delivery_report = mms_handler_dbus_delivery_report;
    klass->fn_read_report = mms_handler_dbus_read_report;
    G_OBJECT_CLASS(klass)->dispose = mms_handler_dbus_dispose;
}

static
void
mms_handler_dbus_init(
    MMSHandlerDbus* dbus)
{
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
