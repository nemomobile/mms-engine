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
#include "mms_task_http.h"
#include "mms_file_util.h"
#include "mms_codec.h"

static
const char*
mms_task_notifyresp_encode(
    const MMSConfig* config,
    const char* id,
    const char* transaction_id,
    MMSNotifyStatus status)
{
    const char* result = NULL;
    const char* file = MMS_NOTIFYRESP_IND_FILE;
    char* dir = mms_message_dir(config, id);
    int fd = mms_create_file(dir, file, NULL, NULL);
    if (fd >= 0) {
        MMSPdu* pdu = g_new0(MMSPdu, 1);
        pdu->type = MMS_MESSAGE_TYPE_NOTIFYRESP_IND;
        pdu->version = MMS_VERSION;
        pdu->transaction_id = g_strdup(transaction_id);
        pdu->nri.notify_status = status;
        if (mms_message_encode(pdu, fd)) result = file;
        mms_message_free(pdu);
        close(fd);
    }
    g_free(dir);
    return result;
}

/**
 * Create MMS retrieve confirmation task
 */
MMSTask*
mms_task_notifyresp_new(
    MMSTask* parent,
    const char* tx_id,
    MMSNotifyStatus ns)
{
    const char* file = mms_task_notifyresp_encode(task_config(parent),
        parent->id, tx_id, ns);
    if (file) {
        return mms_task_http_alloc_with_parent(0, parent, "NotifyResp",
            NULL, NULL, file);
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
