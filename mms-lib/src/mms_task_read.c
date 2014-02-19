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
#include "mms_log.h"
#include "mms_codec.h"
#include "mms_error.h"
#include "mms_file_util.h"

static
char*
mms_task_read_create_pdu_file(
    const MMSConfig* config,
    const char* id,
    const char* message_id,
    const char* to,
    MMSReadStatus status,
    GError** err)
{
    char* path = NULL;
    char* dir = mms_message_dir(config, id);
    int fd = mms_create_file(dir, MMS_READ_REC_IND_FILE, &path, err);
    if (fd >= 0) {
        MMSPdu* pdu = g_new0(MMSPdu, 1);
        pdu->type = MMS_MESSAGE_TYPE_READ_REC_IND;
        pdu->version = MMS_VERSION;
        pdu->ri.rr_status = (status == MMS_READ_STATUS_DELETED) ?
            MMS_MESSAGE_READ_STATUS_DELETED : MMS_MESSAGE_READ_STATUS_READ;
        pdu->ri.msgid = g_strdup(message_id);
        pdu->ri.to = g_strdup(to);
        time(&pdu->ri.date);
        if (!mms_message_encode(pdu, fd)) {
            MMS_ERROR(err, MMS_LIB_ERROR_ENCODE, "Failed to encode %s", path);
            g_free(path);
            path = NULL;
        }
        mms_message_free(pdu);
        close(fd);
    }
    g_free(dir);
    return path;
}

/**
 * Create MMS read report task
 */
MMSTask*
mms_task_read_new(
    const MMSConfig* cfg,
    MMSHandler* h,
    const char* id,
    const char* imsi,
    const char* msg_id,
    const char* to,
    MMSReadStatus rs,
    GError** err)
{
    char* path = mms_task_read_create_pdu_file(cfg, id, msg_id, to, rs, err);
    if (path) {
        MMSTask* task = mms_task_upload_new(cfg, h, "Read", id, imsi, path);
        g_free(path);
        return task;
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
