/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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
#include "mms_task_http.h"
#include "mms_file_util.h"
#include "mms_handler.h"
#include "mms_codec.h"
#include "mms_util.h"
#include "mms_log.h"
#include "mms_error.h"

/* Class definition */
typedef MMSTaskHttpClass MMSTaskReadClass;
typedef MMSTaskHttp MMSTaskRead;

G_DEFINE_TYPE(MMSTaskRead, mms_task_read, MMS_TYPE_TASK_HTTP);
#define MMS_TYPE_TASK_READ (mms_task_read_get_type())
#define MMS_TASK_READ(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
        MMS_TYPE_TASK_READ, MMSTaskRead))

static
const char*
mms_task_read_encode(
    const MMSConfig* config,
    const char* id,
    const char* message_id,
    const char* to,
    MMSReadStatus status,
    GError** err)
{
    const char* result = NULL;
    const char* file = MMS_READ_REC_IND_FILE;
    char* path = NULL;
    char* dir = mms_message_dir(config, id);
    int fd = mms_create_file(dir, file, &path, err);
    if (fd >= 0) {
        MMSPdu* pdu = g_new0(MMSPdu, 1);
        pdu->type = MMS_MESSAGE_TYPE_READ_REC_IND;
        pdu->version = MMS_VERSION;
        pdu->ri.rr_status = (status == MMS_READ_STATUS_DELETED) ?
            MMS_MESSAGE_READ_STATUS_DELETED : MMS_MESSAGE_READ_STATUS_READ;
        pdu->ri.msgid = g_strdup(message_id);
        pdu->ri.to = mms_address_normalize(to);
        time(&pdu->ri.date);
        if (mms_message_encode(pdu, fd)) {
            result = file;
        } else {
            MMS_ERROR(err, MMS_LIB_ERROR_ENCODE, "Failed to encode %s", path);
        }
        mms_message_free(pdu);
        g_free(path);
        close(fd);
    }
    g_free(dir);
    return result;
}

static
void
mms_task_read_done(
    MMSTaskHttp* http,
    const char* path,
    SoupStatus soup_status)
{
    MMSTask* task = &http->task;
    MMS_READ_REPORT_STATUS send_status;
    if (SOUP_STATUS_IS_INFORMATIONAL(soup_status) ||
        SOUP_STATUS_IS_SUCCESSFUL(soup_status)) {
        send_status = MMS_READ_REPORT_STATUS_OK;
    } else if (SOUP_STATUS_IS_TRANSPORT_ERROR(soup_status)) {
        send_status = MMS_READ_REPORT_STATUS_IO_ERROR;
    } else {
        send_status = MMS_READ_REPORT_STATUS_PERMANENT_ERROR;
    }
    mms_handler_read_report_send_status(task->handler, task->id, send_status);
}

/**
 * Per class initializer
 */
static
void
mms_task_read_class_init(
    MMSTaskReadClass* klass)
{
    klass->fn_done = mms_task_read_done;
}

/**
 * Per instance initializer
 */
static
void
mms_task_read_init(
    MMSTaskRead* self)
{
}

/**
 * Create MMS read report task
 */
MMSTask*
mms_task_read_new(
    MMSSettings* settings,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* msg_id,
    const char* to,
    MMSReadStatus rs,
    GError** err)
{
    const char* file = mms_task_read_encode(settings->config,
        id, msg_id, to, rs, err);
    if (file) {
        return mms_task_http_alloc(MMS_TYPE_TASK_READ, settings, handler,
            "Read", id, imsi, NULL, NULL, file);
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
