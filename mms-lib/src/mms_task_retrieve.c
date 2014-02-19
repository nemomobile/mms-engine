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
#include "mms_connection.h"
#include "mms_file_util.h"
#include "mms_handler.h"
#include "mms_codec.h"
#include "mms_util.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_retrieve_log
#include "mms_lib_log.h"
#include "mms_error.h"
MMS_LOG_MODULE_DEFINE("mms-task-retrieve");

/* Class definition */
typedef MMSTaskClass MMSTaskRetrieveClass;
typedef struct mms_task_retrieve {
    MMSTask task;
    MMSHttpTransfer* tx;
    char* uri;
    char* transaction_id;
    gulong got_chunk_signal_id;
    guint bytes_received;
    guint status_code;
} MMSTaskRetrieve;

G_DEFINE_TYPE(MMSTaskRetrieve, mms_task_retrieve, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_RETRIEVE (mms_task_retrieve_get_type())
#define MMS_TASK_RETRIEVE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_RETRIEVE, MMSTaskRetrieve))

#define mms_task_retrieve_state(t,rs) \
    mms_handler_message_receive_state_changed((t)->task.handler,\
    (t)->task.id, rs)

static
void
mms_task_retrieve_got_chunk(
    SoupMessage* message,
    SoupBuffer* chunk,
    MMSTaskRetrieve* retrieve);

static
void
mms_task_retrieve_run(
    MMSTask* task)
{
    mms_task_set_state(task, MMS_TASK_STATE_NEED_CONNECTION);
}

static
void
mms_task_retrieve_finish_transfer(
    MMSTaskRetrieve* retrieve)
{
    if (retrieve->tx) {
        MMSHttpTransfer* tx = retrieve->tx;
        g_signal_handler_disconnect(retrieve->tx->message,
            retrieve->got_chunk_signal_id);
        retrieve->got_chunk_signal_id = 0;
        retrieve->tx = NULL;
        mms_http_transfer_free(tx);
    }
}

static
void
mms_task_retrieve_got_chunk(
    SoupMessage* message,
    SoupBuffer* buf,
    MMSTaskRetrieve* retrieve)
{
    retrieve->bytes_received += buf->length;
    MMS_VERBOSE("%u bytes", retrieve->bytes_received);
    if (retrieve->tx &&
        write(retrieve->tx->fd, buf->data, buf->length) != (int)buf->length) {
        MMS_ERR("Write error: %s", strerror(errno));
        mms_task_retrieve_finish_transfer(retrieve);
        mms_task_set_state(&retrieve->task, MMS_TASK_STATE_SLEEP);
    }
}

static
void
mms_task_retrieve_finished(
    SoupSession* session,
    SoupMessage* message,
    gpointer user_data)
{
    MMSTaskRetrieve* retrieve = user_data;
    if (retrieve->tx && (retrieve->tx->session == session)) {
        MMS_TASK_STATE next_state = MMS_TASK_STATE_SLEEP;
        MMSTask* task = &retrieve->task;
        const MMSConfig* config = task->config;
        retrieve->status_code = message->status_code;
        MMS_DEBUG("Retrieve status %u", retrieve->status_code);
        mms_task_retrieve_finish_transfer(retrieve);
        if (SOUP_STATUS_IS_SUCCESSFUL(retrieve->status_code)) {
            char* file = mms_task_file(task, MMS_RETRIEVE_CONF_FILE);

            /* Content retrieved successfully */
            MMS_DEBUG("Retrieved %s", retrieve->uri);
            next_state = MMS_TASK_STATE_DONE;
            mms_task_retrieve_state(retrieve, MMS_RECEIVE_STATE_DECODING);

            /* Queue the decoding task */
            mms_task_queue_and_unref(task->delegate,
                mms_task_decode_new(task->config, task->handler, task->id,
                    task->imsi, retrieve->transaction_id, file));

            g_free(file);
        } else {

            /* Will retry if this was an I/O error, otherwise we consider
             * it a permanent failure */
            if (SOUP_STATUS_IS_TRANSPORT_ERROR(retrieve->status_code)) {
                mms_task_retrieve_state(retrieve, MMS_RECEIVE_STATE_DEFERRED);
            } else {
                next_state = MMS_TASK_STATE_DONE;
                MMS_WARN("Retrieve error %u", retrieve->status_code);
            }
        }

        if (!config->keep_temp_files) {
            char* dir = mms_task_dir(task);
            char* file = g_strconcat(dir, "/" MMS_RETRIEVE_CONF_FILE, NULL);
            remove(file);
            remove(dir);
            g_free(file);
            g_free(dir);
        }

        /* Switch the state */
        mms_task_set_state(task, next_state);
    } else {
        MMS_VERBOSE_("Ignoring stale completion message");
    }
}

static
void
mms_task_retrieve_start(
    MMSTaskRetrieve* retrieve,
    MMSConnection* connection)
{
    MMSTask* task = &retrieve->task;
    char* dir = mms_task_dir(task);
    char* file = NULL;
    int fd;
    MMS_ASSERT(mms_connection_is_open(connection));

    /* Cleanup any leftovers */
    mms_task_retrieve_finish_transfer(retrieve);
    retrieve->bytes_received = 0;

    /* Create new temporary file */
    fd = mms_create_file(dir, MMS_RETRIEVE_CONF_FILE, &file, NULL);
    if (fd >= 0) {
        /* Set up the transfer */
        retrieve->tx = mms_http_transfer_new(task->config,
            connection, SOUP_METHOD_GET, retrieve->uri, fd);
        if (retrieve->tx) {
            /* Start the transfer */
            SoupMessage* message = retrieve->tx->message;
            MMS_DEBUG("%s -> %s", retrieve->uri, file);
            soup_message_body_set_accumulate(message->response_body, FALSE);
            retrieve->got_chunk_signal_id = g_signal_connect(message,
                "got-chunk", G_CALLBACK(mms_task_retrieve_got_chunk),
                retrieve);
            soup_session_queue_message(retrieve->tx->session, message,
                mms_task_retrieve_finished, retrieve);
        }
    }

    if (retrieve->tx) {
        mms_task_set_state(task, MMS_TASK_STATE_TRANSMITTING);
        mms_task_retrieve_state(retrieve, MMS_RECEIVE_STATE_RECEIVING);
    } else {
        retrieve->status_code = SOUP_STATUS_NONE;
        mms_task_set_state(task, MMS_TASK_STATE_DONE);
        close(fd);
    }

    g_free(file);
    g_free(dir);
}

static
void
mms_task_retrieve_transmit(
    MMSTask* task,
    MMSConnection* connection)
{
    if (task->state != MMS_TASK_STATE_TRANSMITTING) {
        mms_task_retrieve_start(MMS_TASK_RETRIEVE(task), connection);
    }
}

static
void
mms_task_retrieve_cancel(
    MMSTask* task)
{
    mms_task_retrieve_finish_transfer(MMS_TASK_RETRIEVE(task));
    MMS_TASK_CLASS(mms_task_retrieve_parent_class)->fn_cancel(task);
}

static
void
mms_task_retrieve_network_unavailable(
    MMSTask* task)
{
    mms_task_retrieve_finish_transfer(MMS_TASK_RETRIEVE(task));
    mms_task_set_state(task, MMS_TASK_STATE_SLEEP);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_task_retrieve_dispose(
    GObject* object)
{
    MMSTaskRetrieve* retrieve = MMS_TASK_RETRIEVE(object);
    mms_task_retrieve_finish_transfer(retrieve);
    G_OBJECT_CLASS(mms_task_retrieve_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
mms_task_retrieve_finalize(
    GObject* object)
{
    MMSTaskRetrieve* retrieve = MMS_TASK_RETRIEVE(object);
    if (!SOUP_STATUS_IS_SUCCESSFUL(retrieve->status_code)) {
        mms_task_retrieve_state(retrieve, MMS_RECEIVE_STATE_DOWNLOAD_ERROR);
    }
    g_free(retrieve->uri);
    g_free(retrieve->transaction_id);
    G_OBJECT_CLASS(mms_task_retrieve_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_task_retrieve_class_init(
    MMSTaskRetrieveClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = mms_task_retrieve_dispose;
    object_class->finalize = mms_task_retrieve_finalize;
    klass->fn_run = mms_task_retrieve_run;
    klass->fn_cancel = mms_task_retrieve_cancel;
    klass->fn_transmit = mms_task_retrieve_transmit;
    klass->fn_network_unavailable = mms_task_retrieve_network_unavailable;
}

/**
 * Per instance initializer
 */
static
void
mms_task_retrieve_init(
    MMSTaskRetrieve* retrieve)
{
    retrieve->status_code = SOUP_STATUS_CANCELLED;
}

/* Create MMS retrieve task */
MMSTask*
mms_task_retrieve_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const MMSPdu* pdu,
    GError** error)
{
    const time_t now = time(NULL);

    MMS_ASSERT(pdu);
    MMS_ASSERT(pdu->type == MMS_MESSAGE_TYPE_NOTIFICATION_IND);
    MMS_ASSERT(pdu->transaction_id);
    if (pdu->ni.expiry > now) {
        MMSTaskRetrieve* retrieve = mms_task_alloc(MMS_TYPE_TASK_RETRIEVE,
            config, handler, "Retrieve", id, imsi);
        retrieve->task.deadline = pdu->ni.expiry;
        retrieve->uri = g_strdup(pdu->ni.location);
        retrieve->transaction_id = g_strdup(pdu->transaction_id);
        return &retrieve->task;
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_EXPIRED, "Message already expired");
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
