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

#include "mms_handler.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_handler_log
#include "mms_lib_log.h"

G_DEFINE_TYPE(MMSHandler, mms_handler, G_TYPE_OBJECT);

#define MMS_HANDLER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MMS_TYPE_HANDLER, MMSHandler))
#define MMS_HANDLER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MMS_TYPE_HANDLER, MMSHandlerClass))
#define MMS_HANDLER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MMS_TYPE_HANDLER, MMSHandlerClass))

static
void
mms_handler_finalize(
    GObject* object)
{
    MMS_VERBOSE_("%p", object);
    G_OBJECT_CLASS(mms_handler_parent_class)->finalize(object);
}

static
void
mms_handler_class_init(
    MMSHandlerClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = mms_handler_finalize;
}

static
void
mms_handler_init(
    MMSHandler* h)
{
    MMS_VERBOSE_("%p", h);
}

MMSHandler*
mms_handler_ref(
    MMSHandler* h)
{
    if (h) {
        MMS_ASSERT(MMS_HANDLER(h));
        g_object_ref(h);
    }
    return h;
}

void
mms_handler_unref(
    MMSHandler* h)
{
    if (h) {
        MMS_ASSERT(MMS_HANDLER(h));
        g_object_unref(h);
    }
}

char*
mms_handler_message_notify(
    MMSHandler* h,
    const char* imsi,
    const char* from,
    const char* subj,
    time_t exp,
    GBytes* push)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_message_notify) {
            if (!from) from = "";
            if (!subj) subj = "";
            return klass->fn_message_notify(h, imsi, from, subj, exp, push);
        }
        MMS_ERR("mms_handler_message_notify not implemented");
    }
    return NULL;
}

gboolean
mms_handler_message_receive_state_changed(
    MMSHandler* h,
    const char* id,
    MMS_RECEIVE_STATE state)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_message_receive_state_changed) {
            return klass->fn_message_receive_state_changed(h, id, state);
        }
        MMS_ERR("mms_handler_message_receive_state_changed not implemented");
    }
    return FALSE;
}

gboolean
mms_handler_message_received(
    MMSHandler* h,
    MMSMessage* msg)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_message_received) {
            return klass->fn_message_received(h, msg);
        }
        MMS_ERR("mms_handler_message_received not implemented");
    }
    return FALSE;
}

gboolean
mms_handler_message_send_state_changed(
    MMSHandler* h,
    const char* id,
    MMS_SEND_STATE state)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_message_send_state_changed) {
            return klass->fn_message_send_state_changed(h, id, state);
        }
        MMS_ERR("mms_handler_message_send_state_changed not implemented");
    }
    return FALSE;
}

gboolean
mms_handler_message_sent(
    MMSHandler* h,
    const char* id,
    const char* msgid)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_message_sent) {
            return klass->fn_message_sent(h, id, msgid);
        }
        MMS_ERR("mms_handler_message_sent not implemented");
    }
    return FALSE;
}

gboolean
mms_handler_delivery_report(
    MMSHandler* h,
    const char* imsi,
    const char* msgid,
    const char* recipient,
    MMS_DELIVERY_STATUS ds)
{
    if (h) {
        MMSHandlerClass* klass = MMS_HANDLER_GET_CLASS(h);
        if (klass->fn_delivery_report) {
            return klass->fn_delivery_report(h, imsi, msgid, recipient, ds);
        }
        MMS_ERR("mms_handler_delivery_report not implemented");
    }
    return FALSE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
