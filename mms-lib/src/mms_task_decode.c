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
#include "mms_message.h"
#include "mms_file_util.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_decode_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-task-decode");

/* Class definition */
typedef MMSTaskClass MMSTaskDecodeClass;
typedef struct mms_task_decode {
    MMSTask task;
    GMappedFile* map;
    char* transaction_id;
    char* file;
} MMSTaskDecode;

G_DEFINE_TYPE(MMSTaskDecode, mms_task_decode, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_DECODE (mms_task_decode_get_type())
#define MMS_TASK_DECODE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_DECODE, MMSTaskDecode))

static
gboolean
mms_task_decode_array_contains_string(
    const GPtrArray* array,
    const char* str)
{
    guint i;
    for (i=0; i<array->len; i++) {
        if (!strcmp(array->pdata[i], str)) {
            return TRUE;
        }
    }
    return FALSE;
}

static
char*
mms_task_decode_add_file_name(
    GPtrArray* names,
    const char* proposed)
{
    const char* src;
    char* file = g_new(char, strlen(proposed)+1);
    char* dest = file;
    for (src = proposed; *src; src++) {
        switch (*src) {
        case '<': case '>': case '[': case ']':
            break;
        case '/': case '\\':
            *dest++ = '_';
            break;
        default:
            *dest++ = *src;
            break;
        }
    }
    *dest = 0;
    while (mms_task_decode_array_contains_string(names, file)) {
        char* _file = g_strconcat("_", file, NULL);
        g_free(file);
        file = _file;
    }
    g_ptr_array_add(names, file);
    return file;
}

static
MMSMessage*
mms_task_decode_process_retrieve_conf(
    MMSTask* task,
    const MMSPdu* pdu,
    const guint8* pdu_data,
    gsize pdu_size)
{
    GSList* entry;
    int i, nparts = g_slist_length(pdu->attachments);
    GPtrArray* part_files = g_ptr_array_new_full(nparts, g_free);
    char* dir = mms_task_dir(task);
    const struct mms_retrieve_conf* rc = &pdu->rc;
    MMSMessage* msg = mms_message_new();

#if MMS_LOG_DEBUG
    char date[128];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S%z", localtime(&rc->date));
    date[sizeof(date)-1] = '\0';
#endif /* MMS_LOG_DEBUG */

    MMS_ASSERT(pdu->type == MMS_MESSAGE_TYPE_RETRIEVE_CONF);
    MMS_INFO("Processing M-Retrieve.conf");
    MMS_INFO("  From: %s", rc->from);

#if MMS_LOG_DEBUG
    MMS_DEBUG("  To: %s", rc->to);
    if (rc->cc) MMS_DEBUG("  Cc: %s", rc->cc);
    MMS_DEBUG("  Message-ID: %s", rc->msgid);
    MMS_DEBUG("  Transaction-ID: %s", pdu->transaction_id);
    if (rc->subject) MMS_DEBUG("  Subject: %s", rc->subject);
    MMS_DEBUG("  Date: %s", date);
    MMS_DEBUG("  %u parts", nparts);
#endif /* MMS_LOG_DEBUG */

    if (task->config->keep_temp_files) {
        msg->flags |= MMS_MESSAGE_FLAG_KEEP_FILES;
    }

    msg->id = g_strdup(task->id);
    msg->msg_dir = g_strdup(dir);
    msg->message_id = g_strdup(rc->msgid);
    msg->from = mms_strip_address_type(g_strdup(rc->from));
    msg->to = mms_split_address_list(rc->to);
    msg->cc = mms_split_address_list(rc->cc);
    msg->subject = g_strdup(rc->subject);
    msg->cls = g_strdup(rc->cls ? rc->cls : MMS_MESSAGE_CLASS_PERSONAL);
    msg->date = rc->date ? rc->date : time(NULL);

    switch (rc->priority) {
    case MMS_MESSAGE_PRIORITY_LOW:
        msg->priority = MMS_PRIORITY_LOW;
        break;
    case MMS_MESSAGE_PRIORITY_NORMAL:
        msg->priority = MMS_PRIORITY_NORMAL;
        break;
    case MMS_MESSAGE_PRIORITY_HIGH:
        msg->priority = MMS_PRIORITY_HIGH;
        break;
    }

    msg->parts_dir = g_strconcat(dir, "/" , MMS_PARTS_DIR, NULL);
    for (i=0, entry = pdu->attachments; entry; entry = entry->next, i++) {
        struct mms_attachment* attach = entry->data;
        const char* id = attach->content_id;
        char* path = NULL;
        char* file;
        if (id && id[0]) {
            file = mms_task_decode_add_file_name(part_files, id);
        } else {
            char* name = g_strdup_printf("part_%d",i);
            file = mms_task_decode_add_file_name(part_files, name);
            g_free(name);
        }
        MMS_DEBUG("Part: %s %s", id, attach->content_type);
        MMS_ASSERT(attach->offset < pdu_size);
        if (mms_write_file(msg->parts_dir, file, pdu_data + attach->offset,
            attach->length, &path)) {
            MMSMessagePart* part = g_new0(MMSMessagePart, 1);
            part->content_type = g_strdup(attach->content_type);
            part->content_id = g_strdup(id);
            part->file = path;
            msg->parts = g_slist_append(msg->parts, part);
        }
    }

    g_ptr_array_free(part_files, TRUE);
    g_free(dir);
    return msg;
}

static
MMSMessage*
mms_task_decode_process_pdu(
    MMSTask* task,
    const guint8* data,
    gsize len)
{
    MMSMessage* msg = NULL;
    MMSPdu* pdu = g_new0(MMSPdu, 1);
    if (mms_message_decode(data, len, pdu)) {
        if (pdu->type == MMS_MESSAGE_TYPE_RETRIEVE_CONF) {
            msg = mms_task_decode_process_retrieve_conf(task, pdu, data, len);
        } else {
            MMS_ERR("Unexpected MMS PDU type %u", (guint)pdu->type);
        }
    } else {
        MMS_ERR("Failed to decode MMS PDU");
    }
    mms_message_free(pdu);
    return msg;
}

static
void
mms_task_decode_run(
    MMSTask* task)
{
    MMSTaskDecode* dec = MMS_TASK_DECODE(task);
    const void* data = g_mapped_file_get_contents(dec->map);
    const gsize size = g_mapped_file_get_length(dec->map);
    MMSMessage* msg = mms_task_decode_process_pdu(task, data, size);
    if (msg) {
        mms_task_queue_and_unref(task->delegate,
            mms_task_ack_new(task->config, task->handler, task->id, task->imsi,
                dec->transaction_id));
        mms_task_queue_and_unref(task->delegate,
            mms_task_publish_new(task->config, task->handler, msg));
        mms_message_unref(msg);
    } else {
        mms_handler_message_receive_state_changed(task->handler, task->id,
            MMS_RECEIVE_STATE_DECODING_ERROR);
        mms_task_queue_and_unref(task->delegate,
            mms_task_notifyresp_new(task->config, task->handler, task->id,
                task->imsi, dec->transaction_id,
                MMS_MESSAGE_NOTIFY_STATUS_UNRECOGNISED));
    }
    mms_task_set_state(task, MMS_TASK_STATE_DONE);
}

static
void
mms_task_decode_finalize(
    GObject* object)
{
    MMSTaskDecode* dec = MMS_TASK_DECODE(object);
    if (!dec->task.config->keep_temp_files) {
        mms_remove_file_and_dir(dec->file);
    }
    g_mapped_file_unref(dec->map);
    g_free(dec->transaction_id);
    g_free(dec->file);
    G_OBJECT_CLASS(mms_task_decode_parent_class)->finalize(object);
}

static
void
mms_task_decode_class_init(
    MMSTaskDecodeClass* klass)
{
    mms_task_init_class(MMS_TASK_CLASS(mms_task_decode_parent_class));
    klass->fn_run = mms_task_decode_run;
    G_OBJECT_CLASS(klass)->finalize = mms_task_decode_finalize;
}

static
void
mms_task_decode_init(
    MMSTaskDecode* decode)
{
}

/* Create MMS decode task */
MMSTask*
mms_task_decode_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* transaction_id,
    const char* file)
{
    MMSTaskDecode* dec = mms_task_alloc(MMS_TYPE_TASK_DECODE,
        config, handler, "Decode", id, imsi);
    GError* error = NULL;
    dec->map = g_mapped_file_new(file, FALSE, &error);
    if (dec->map) {
        dec->transaction_id = g_strdup(transaction_id);
        dec->file = g_strdup(file);
        return &dec->task;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
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
