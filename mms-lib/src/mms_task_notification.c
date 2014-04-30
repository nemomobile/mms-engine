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

#include "mms_task.h"
#include "mms_util.h"
#include "mms_codec.h"
#include "mms_handler.h"
#include "mms_file_util.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_notification_log
#include "mms_lib_log.h"
#include "mms_error.h"
MMS_LOG_MODULE_DEFINE("mms-task-notification");

/* Class definition */
typedef MMSTaskClass MMSTaskNotificationClass;
typedef struct mms_task_notification {
    MMSTask task;
    MMSPdu* pdu;
    GBytes* push;
} MMSTaskNotification;

G_DEFINE_TYPE(MMSTaskNotification, mms_task_notification, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_NOTIFICATION (mms_task_notification_get_type())
#define MMS_TASK_NOTIFICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_NOTIFICATION, MMSTaskNotification))

/**
 * Writes the datagram to a file in the message directory.
 */
static
gboolean
mms_task_notification_write_file(
    MMSTaskNotification* ind,
    const char* file)
{
    char* dir;
    gboolean ok;
    mms_task_make_id(&ind->task);
    dir = mms_task_dir(&ind->task);
    ok = mms_write_bytes(dir, file, ind->push, NULL);
    g_free(dir);
    return ok;
}

/**
 * Handles M-Notification.ind PDU
 */
static
void
mms_task_notification_ind(
    MMSTaskNotification* ind)
{
    MMSTask* task = &ind->task;
    const struct mms_notification_ind* ni = &ind->pdu->ni;
    char* id;

#if MMS_LOG_DEBUG
    char expiry[128];
    strftime(expiry, sizeof(expiry), "%Y-%m-%dT%H:%M:%S%z",
        localtime(&ni->expiry));
    expiry[sizeof(expiry)-1] = '\0';

    MMS_DEBUG("Processing M-Notification.ind");
    MMS_DEBUG("  From: %s", ni->from);
    if (ni->subject) MMS_DEBUG("  Subject: %s", ni->subject);
    MMS_DEBUG("  Size: %d bytes", ni->size);
    MMS_DEBUG("  Location: %s", ni->location);
    MMS_DEBUG("  Expiry: %s", expiry);
#endif /* MMS_LOG_DEBUG */

    id = mms_handler_message_notify(task->handler, task->imsi,
        mms_strip_address_type(ni->from), ni->subject, ni->expiry, ind->push);
    if (id) {
        if (id[0]) {
            MMS_DEBUG("  Database id: %s", id);
            if (task->id) {
                /* Remove temporary directory and files */
                char* dir = mms_task_dir(task);
                char* file = mms_task_file(task, MMS_NOTIFICATION_IND_FILE);
                remove(file);
                remove(dir);
                g_free(file);
                g_free(dir);
                g_free(task->id);
            }
            task->id = id;

            /* Schedule the download task */
            if (!mms_task_queue_and_unref(task->delegate,
                    mms_task_retrieve_new(task->config, task->handler,
                        task->id, task->imsi, ind->pdu, NULL))) {
                mms_handler_message_receive_state_changed(task->handler, id,
                    MMS_RECEIVE_STATE_DOWNLOAD_ERROR);
            }
        } else {
            g_free(id);
        }
    } else if (!mms_task_retry(task)) {
        mms_task_make_id(task);
        mms_task_queue_and_unref(task->delegate,
            mms_task_notifyresp_new(task->config, task->handler, task->id,
                task->imsi, ind->pdu->transaction_id,
                MMS_MESSAGE_NOTIFY_STATUS_REJECTED));
    }

    if (task->config->keep_temp_files) {
        mms_task_notification_write_file(ind, MMS_NOTIFICATION_IND_FILE);
    }
}

/**
 * Handles M-Delivery.ind PDU
 */
static
void
mms_task_delivery_ind(
    MMSTaskNotification* ind)
{
    MMS_DELIVERY_STATUS ds;
    MMSTask* t = &ind->task;
    const struct mms_delivery_ind* di = &ind->pdu->di;
    const char* to = mms_strip_address_type(di->to);
    MMS_DEBUG("Processing M-Delivery.ind PDU");
    MMS_DEBUG("  MMS message id: %s", di->msgid);
    MMS_DEBUG("  Recipient: %s", to);
    switch (di->dr_status) {
    case MMS_MESSAGE_DELIVERY_STATUS_EXPIRED:
        ds = MMS_DELIVERY_STATUS_EXPIRED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_RETRIEVED:
        ds = MMS_DELIVERY_STATUS_RETRIEVED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_REJECTED:
        ds = MMS_DELIVERY_STATUS_REJECTED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_DEFERRED:
        ds = MMS_DELIVERY_STATUS_DEFERRED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_UNRECOGNISED:
        ds = MMS_DELIVERY_STATUS_UNRECOGNISED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_FORWARDED:
        ds = MMS_DELIVERY_STATUS_FORWARDED;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_UNREACHABLE:
        ds = MMS_DELIVERY_STATUS_UNREACHABLE;
        break;
    case MMS_MESSAGE_DELIVERY_STATUS_INDETERMINATE:
    default:
        ds = MMS_DELIVERY_STATUS_UNKNOWN;
        break;
    }
    mms_handler_delivery_report(t->handler, t->imsi, di->msgid, to, ds);
    if (t->config->keep_temp_files) {
        mms_task_notification_write_file(ind, MMS_DELIVERY_IND_FILE);
    }
}

/**
 * Handles M-Read-Orig.ind PDU
 */
static
void
mms_task_read_orig_ind(
    MMSTaskNotification* ind)
{
    MMS_READ_STATUS rs;
    MMSTask* t = &ind->task;
    const struct mms_read_ind* ri = &ind->pdu->ri;
    const char* to = mms_strip_address_type(ri->to);
    MMS_DEBUG("Processing M-Read-Orig.ind");
    MMS_DEBUG("  MMS message id: %s", ri->msgid);
    MMS_DEBUG("  Recipient: %s", to);
    switch (ri->rr_status) {
    case MMS_MESSAGE_READ_STATUS_READ:
        rs = MMS_READ_STATUS_READ;
        break;
    case MMS_MESSAGE_READ_STATUS_DELETED:
        rs = MMS_READ_STATUS_DELETED;
        break;
    default:
        rs = MMS_READ_STATUS_INVALID;
        break;
    }
    mms_handler_read_report(t->handler, t->imsi, ri->msgid, to, rs);
    if (ind->task.config->keep_temp_files) {
        mms_task_notification_write_file(ind,  MMS_READ_ORIG_IND_FILE);
    }
}

/**
 * Handles unrecognized PDU
 */
static
void
mms_task_notification_unrecornized(
    const MMSConfig* config,
    GBytes* push)
{
    if (config->attic_enabled) {
        char* attic_dir = NULL;
        int i;
        for (i=0; i<100; i++) {
            g_free(attic_dir);
            attic_dir = g_strdup_printf("%s/" MMS_ATTIC_DIR "/%03d",
                config->root_dir, i);
            if (!g_file_test(attic_dir, G_FILE_TEST_IS_DIR)) break;
        }
        mms_write_bytes(attic_dir, MMS_UNRECOGNIZED_PUSH_FILE, push, NULL);
        g_free(attic_dir);
    }
}

/**
 * Runs the task
 */
static
void
mms_task_notification_run(
    MMSTask* task)
{
    MMSTaskNotification* ind = MMS_TASK_NOTIFICATION(task);
    switch (ind->pdu->type) {
    case MMS_MESSAGE_TYPE_NOTIFICATION_IND:
        mms_task_notification_ind(ind);
        break;
    case MMS_MESSAGE_TYPE_DELIVERY_IND:
        mms_task_delivery_ind(ind);
        break;
    case MMS_MESSAGE_TYPE_READ_ORIG_IND:
        mms_task_read_orig_ind(ind);
        break;
    default:
        MMS_INFO("Ignoring MMS push PDU of type %u", ind->pdu->type);
        mms_task_notification_unrecornized(task->config, ind->push);
        break;
    }
    if (task->state == MMS_TASK_STATE_READY) {
        mms_task_set_state(task, MMS_TASK_STATE_DONE);
    }
}

static
void
mms_task_notification_finalize(
    GObject* object)
{
    MMSTaskNotification* ind = MMS_TASK_NOTIFICATION(object);
    g_bytes_unref(ind->push);
    mms_message_free(ind->pdu);
    G_OBJECT_CLASS(mms_task_notification_parent_class)->finalize(object);
}

static
void
mms_task_notification_class_init(
    MMSTaskNotificationClass* klass)
{
    klass->fn_run = mms_task_notification_run;
    G_OBJECT_CLASS(klass)->finalize = mms_task_notification_finalize;
}

static
void
mms_task_notification_init(
    MMSTaskNotification* notification)
{
}

/* Create MMS notification task */
MMSTask*
mms_task_notification_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* imsi,
    GBytes* bytes,
    GError** error)
{
    MMSPdu* pdu = mms_decode_bytes(bytes);
    MMS_ASSERT(!error || !(*error));
    if (pdu) {
        MMSTaskNotification* ind;

        /* Looks like a legitimate MMS Push PDU */
#if MMS_LOG_DEBUG
        MMS_DEBUG("  MMS version: %u.%u", (pdu->version & 0x70) >> 4,
            pdu->version & 0x0f);
        if (pdu->transaction_id) {
            MMS_DEBUG("  MMS transaction id: %s", pdu->transaction_id);
        }
#endif /* MMS_LOG_DEBUG */

        ind = mms_task_alloc(MMS_TYPE_TASK_NOTIFICATION,
            config, handler, "Notification", NULL, imsi);
        ind->push = g_bytes_ref(bytes);
        ind->pdu = pdu;
        return &ind->task;
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_DECODE, "Failed to decode MMS PDU");
        mms_task_notification_unrecornized(config, bytes);
        return NULL;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
