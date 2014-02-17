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

#include "mms_message.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_message_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-message");

static
void
mms_message_part_free(
    gpointer data,
    gpointer user_data)
{
    MMSMessagePart* part = data;
    MMSMessage* msg = user_data;
    g_free(part->content_type);
    g_free(part->content_id);
    if (part->file) {
        if (!(msg->flags & MMS_MESSAGE_FLAG_KEEP_FILES)) remove(part->file);
        g_free(part->file);
    }
    g_free(part);
}

static
void
mms_message_finalize(
    MMSMessage* msg)
{
    MMS_VERBOSE_("%p", msg);
    g_free(msg->id);
    g_free(msg->message_id);
    g_free(msg->from);
    g_strfreev(msg->to);
    g_strfreev(msg->cc);
    g_free(msg->subject);
    g_free(msg->cls);
    g_slist_foreach(msg->parts, mms_message_part_free, msg);
    g_slist_free(msg->parts);
    if (msg->parts_dir) {
        if (!(msg->flags & MMS_MESSAGE_FLAG_KEEP_FILES)) remove(msg->parts_dir);
        g_free(msg->parts_dir);
    }
}

MMSMessage*
mms_message_new()
{
    MMSMessage* msg = g_new0(MMSMessage, 1);
    MMS_VERBOSE_("%p", msg);
    msg->ref_count = 1;
    msg->priority = MMS_PRIORITY_NORMAL;
    return msg;
}

MMSMessage*
mms_message_ref(
    MMSMessage* msg)
{
    if (msg) {
        MMS_ASSERT(msg->ref_count > 0);
        g_atomic_int_inc(&msg->ref_count);
    }
    return msg;
}

void
mms_message_unref(
    MMSMessage* msg)
{
    if (msg) {
        MMS_ASSERT(msg->ref_count > 0);
        if (g_atomic_int_dec_and_test(&msg->ref_count)) {
            mms_message_finalize(msg);
            g_free(msg);
        }
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
