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
#include "mms_file_util.h"

static
char*
mms_task_notifyresp_create_pdu_file(
    const MMSConfig* config,
    const char* id,
    const char* transaction_id,
    MMSNotifyStatus status)
{
    char* path = NULL;
    char* dir = mms_message_dir(config, id);
    int fd = mms_create_file(dir, MMS_NOTIFYRESP_IND_FILE, &path);
    if (fd >= 0) {
        MMSPdu* pdu = g_new0(MMSPdu, 1);
        pdu->type = MMS_MESSAGE_TYPE_NOTIFYRESP_IND;
        pdu->version = MMS_VERSION;
        pdu->transaction_id = g_strdup(transaction_id);
        pdu->nri.notify_status = status;
        if (!mms_message_encode(pdu, fd)) {
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
 * Create MMS retrieve confirmation task
 */
MMSTask*
mms_task_notifyresp_new(
    const MMSConfig* cfg,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* tx_id,
    MMSNotifyStatus ns)
{
    char* path = mms_task_notifyresp_create_pdu_file(cfg, id, tx_id, ns);
    if (path) {
        MMSTask* task = mms_task_upload_new(cfg, handler, "NotifyResp",
            id, imsi, path);
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
