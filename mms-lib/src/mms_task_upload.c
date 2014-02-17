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
#include "mms_util.h"
#include <fcntl.h>

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_upload_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-task-upload");

#define MMS_UPLOAD_MAX_CHUNK (2048)

/* Class definition */
typedef MMSTaskClass MMSTaskUploadClass;
typedef struct mms_task_upload {
    MMSTask task;
    MMSHttpTransfer* tx;
    gulong wrote_headers_signal_id;
    gulong wrote_chunk_signal_id;
    gsize bytes_total;
    gsize bytes_sent;
    char* file;
} MMSTaskUpload;

G_DEFINE_TYPE(MMSTaskUpload, mms_task_upload, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_UPLOAD (mms_task_upload_get_type())
#define MMS_TASK_UPLOAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_UPLOAD, MMSTaskUpload))

static
void
mms_task_upload_finish_transfer(
    MMSTaskUpload* up)
{
    if (up->tx) {
        SoupMessage* message = up->tx->message;
        g_signal_handler_disconnect(message, up->wrote_headers_signal_id);
        g_signal_handler_disconnect(message, up->wrote_chunk_signal_id);
        mms_http_transfer_free(up->tx);
        up->wrote_headers_signal_id = 0;
        up->wrote_chunk_signal_id = 0;
        up->tx = NULL;
    }
}

static
void
mms_task_upload_finished(
    SoupSession* session,
    SoupMessage* msg,
    gpointer user_data)
{
    MMSTaskUpload* up = user_data;
    MMS_ASSERT(up->tx && (up->tx->session == session));
    if (up->tx && (up->tx->session == session)) {
        MMS_TASK_STATE next_state;
        MMSTask* task = &up->task;
        MMS_DEBUG("Upload status %u", msg->status_code);
        if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
            next_state = MMS_TASK_STATE_DONE;
        } else {
            /* Will retry if this was an I/O error, otherwise we consider
             * it a permanent failure */
            if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
                next_state = MMS_TASK_STATE_SLEEP;
            } else {
                next_state = MMS_TASK_STATE_DONE;
                MMS_WARN("Upload failure %u", msg->status_code);
            }
        }
        mms_task_set_state(task, next_state);
    } else {
        MMS_VERBOSE_("ignoring stale completion message");
    }
}

static
void
mms_task_upload_write_next_chunk(
    SoupMessage* msg,
    MMSTaskUpload* up)
{
#if MMS_LOG_VERBOSE
    if (up->bytes_sent) MMS_VERBOSE("%u bytes", (guint)up->bytes_sent);
#endif
    MMS_ASSERT(up->tx && up->tx->message == msg);
    if (up->tx &&
        up->tx->message == msg &&
        up->bytes_total > up->bytes_sent) {
        int nbytes;
        void* chunk;
        gsize len = up->bytes_total - up->bytes_sent;
        if (len > MMS_UPLOAD_MAX_CHUNK) len = MMS_UPLOAD_MAX_CHUNK;
        chunk = g_malloc(len);
        nbytes = read(up->tx->fd, chunk, len);
        if (nbytes > 0) {
            up->bytes_sent += nbytes;
            soup_message_body_append_take(msg->request_body, chunk, nbytes);
            return;
        }
    }
    soup_message_body_complete(msg->request_body);
}

static
gboolean
mms_task_upload_start(
    MMSTaskUpload* up,
    MMSConnection* connection)
{
    int fd;
    MMS_ASSERT(mms_connection_is_open(connection));
    mms_task_upload_finish_transfer(up);
    up->bytes_sent = 0;
    fd = open(up->file, O_RDONLY);
    if (fd >= 0) {
        struct stat st;
        int err = fstat(fd, &st);
        if (!err) {
            /* Set up the transfer */
            up->bytes_total = st.st_size;
            up->tx = mms_http_transfer_new(up->task.config, connection,
                "POST", connection->mmsc, fd);
            if (up->tx) {
                /* Headers */
                SoupMessage* msg = up->tx->message;
                soup_message_headers_set_content_type(
                    msg->request_headers,
                    MMS_CONTENT_TYPE, NULL);
                soup_message_headers_set_content_length(
                    msg->request_headers,
                    st.st_size);

                /* Connect the signals */
                up->wrote_headers_signal_id =
                    g_signal_connect(msg, "wrote_headers",
                    G_CALLBACK(mms_task_upload_write_next_chunk), up);
                up->wrote_chunk_signal_id =
                    g_signal_connect(msg, "wrote_chunk",
                    G_CALLBACK(mms_task_upload_write_next_chunk), up);

                /* Start the transfer */
                MMS_DEBUG("%s -> %s (%u bytes)", up->file,
                    connection->mmsc, (guint)up->bytes_total);
                soup_session_queue_message(up->tx->session, msg,
                    mms_task_upload_finished, up);
                return TRUE;
            }
        } else {
            MMS_ERR("Can't stat %s: %s", up->file, strerror(errno));
        }
        close(fd);
    } else {
        MMS_WARN("Failed to open %s: %s", up->file, strerror(errno));
    }
    return FALSE;
}

static
void
mms_task_upload_transmit(
    MMSTask* task,
    MMSConnection* conn)
{
    if (task->state != MMS_TASK_STATE_TRANSMITTING) {
        mms_task_set_state(task,
            mms_task_upload_start(MMS_TASK_UPLOAD(task), conn) ?
            MMS_TASK_STATE_TRANSMITTING : MMS_TASK_STATE_DONE);
    }
}

static
void
mms_task_upload_run(
    MMSTask* task)
{
    mms_task_set_state(task, MMS_TASK_STATE_NEED_CONNECTION);
}

static
void
mms_task_upload_network_unavailable(
    MMSTask* task)
{
    MMSTaskUpload* up = MMS_TASK_UPLOAD(task);
    mms_task_upload_finish_transfer(up);
    mms_task_set_state(task, MMS_TASK_STATE_SLEEP);
}

static
void
mms_task_upload_cancel(
    MMSTask* task)
{
    mms_task_upload_finish_transfer(MMS_TASK_UPLOAD(task));
    MMS_TASK_CLASS(mms_task_upload_parent_class)->fn_cancel(task);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_task_upload_dispose(
    GObject* object)
{
    MMSTaskUpload* up = MMS_TASK_UPLOAD(object);
    mms_task_upload_finish_transfer(up);
    G_OBJECT_CLASS(mms_task_upload_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
mms_task_upload_finalize(
    GObject* object)
{
    MMSTaskUpload* up = MMS_TASK_UPLOAD(object);
    if (!up->task.config->keep_temp_files) {
        mms_remove_file_and_dir(up->file);
    }
    g_free(up->file);
    G_OBJECT_CLASS(mms_task_upload_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_task_upload_class_init(
    MMSTaskUploadClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    klass->fn_run = mms_task_upload_run;
    klass->fn_transmit = mms_task_upload_transmit;
    klass->fn_network_unavailable = mms_task_upload_network_unavailable;
    klass->fn_cancel = mms_task_upload_cancel;
    object_class->dispose = mms_task_upload_dispose;
    object_class->finalize = mms_task_upload_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_task_upload_init(
    MMSTaskUpload* up)
{
}

/**
 * Create MMS upload task
 */
MMSTask*
mms_task_upload_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* name,
    const char* id,
    const char* imsi,
    const char* file)
{
    MMSTaskUpload* up = mms_task_alloc(MMS_TYPE_TASK_UPLOAD,
        config, handler, name, id, imsi);
    up->file = g_strdup(file);
    MMS_ASSERT(g_file_test(up->file, G_FILE_TEST_IS_REGULAR));
    return &up->task;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
