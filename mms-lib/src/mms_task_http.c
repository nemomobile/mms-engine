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

#include "mms_task_http.h"
#include "mms_connection.h"
#include "mms_settings.h"
#include "mms_file_util.h"

#ifndef _WIN32
#  include <sys/ioctl.h>
#  include <arpa/inet.h>
#  include <net/if.h>
#endif

/* Appeared in libsoup somewhere between 2.41.5 and 2.41.90 */
#ifndef SOUP_SESSION_LOCAL_ADDRESS
#  define SOUP_SESSION_LOCAL_ADDRESS "local-address"
#endif

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_http_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-task-http");

/* HTTP task state */
typedef enum _mms_http_state {
    MMS_HTTP_READY,         /* Ready to run */
    MMS_HTTP_ACTIVE,        /* Sending or receiving the data */
    MMS_HTTP_PAUSED,        /* Sleeping or waiting for connection */
    MMS_HTTP_DONE           /* HTTP transaction has been finished */
} MMS_HTTP_STATE;

#define MMS_HTTP_MAX_CHUNK (4046)

/* Transfer context */
typedef struct mms_http_transfer {
    MMSConnection* connection;
    SoupSession* session;
    SoupMessage* message;
    int receive_fd;
    int send_fd;
} MMSHttpTransfer;

/* Private state */
struct mms_task_http_private {
    MMSHttpTransfer* tx;
    char* uri;
    char* send_path;
    char* receive_path;
    const char* receive_file;
    gulong wrote_headers_signal_id;
    gulong wrote_chunk_signal_id;
    gulong got_chunk_signal_id;
    gsize bytes_sent;
    guint bytes_received;
    MMS_HTTP_STATE transaction_state;
};

G_DEFINE_TYPE(MMSTaskHttp, mms_task_http, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_HTTP (mms_task_http_get_type())
#define MMS_TASK_HTTP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_HTTP, MMSTaskHttp))
#define MMS_TASK_HTTP_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MMS_TYPE_TASK_HTTP, MMSTaskHttpClass))

static
SoupURI*
mms_http_uri_parse(
    const char* raw_uri)
{
    SoupURI* uri = NULL;
    if (raw_uri) {
        static const char* http = "http://";
        const char* uri_to_parse;
        char* tmp_uri = NULL;
        if (g_str_has_prefix(raw_uri, http)) {
            uri_to_parse = raw_uri;
        } else {
            uri_to_parse = tmp_uri = g_strconcat(http, raw_uri, NULL);
        }
        uri = soup_uri_new(uri_to_parse);
        if (!uri) {
            MMS_ERR("Could not parse %s as a URI", uri_to_parse);
        }
        g_free(tmp_uri);
    }
    return uri;
}

static
SoupSession*
mms_http_create_session(
    const MMSSettingsSimData* cfg,
    MMSConnection* conn)
{
    SoupSession* session = NULL;

    /* Determine address of the MMS interface */
    if (conn->netif && conn->netif[0]) {
#ifndef _WIN32
        struct ifreq ifr;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd >= 0) {
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, conn->netif, IFNAMSIZ-1);
            if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0) {
                SoupAddress* local_address = soup_address_new_from_sockaddr(
                    &ifr.ifr_addr, sizeof(ifr.ifr_addr));
#  if MMS_LOG_DEBUG
                char buf[128];
                int af = ifr.ifr_addr.sa_family;
                buf[0] = 0;
                if (af == AF_INET) {
                    struct sockaddr_in* addr = (void*)&ifr.ifr_addr;
                    inet_ntop(af, &addr->sin_addr, buf, sizeof(buf));
                } else if (af == AF_INET6) {
                    struct sockaddr_in6* addr = (void*)&ifr.ifr_addr;
                    inet_ntop(af, &addr->sin6_addr, buf, sizeof(buf));
                } else {
                    snprintf(buf, sizeof(buf), "<address family %d>", af);
                }
                buf[sizeof(buf)-1] = 0;
                MMS_DEBUG("MMS interface address %s", buf);
#  endif /* MMS_LOG_DEBUG */
                MMS_ASSERT(local_address);
                session = soup_session_async_new_with_options(
                    SOUP_SESSION_LOCAL_ADDRESS, local_address,
                    NULL);
                g_object_unref(local_address);
            } else {
                MMS_ERR("Failed to query IP address of %s: %s",
                    conn->netif, strerror(errno));
            }
            close(fd);
        }
#endif /* _WIN32 */
    } else {
        MMS_WARN("MMS interface is unknown");
    }

    if (!session) {
        /* No local address so bind to any interface */
        session = soup_session_async_new();
    }

    if (conn->mmsproxy && conn->mmsproxy[0]) {
        SoupURI* proxy_uri = mms_http_uri_parse(conn->mmsproxy);
        if (proxy_uri) {
            MMS_DEBUG("MMS proxy %s", conn->mmsproxy);
            g_object_set(session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
            soup_uri_free(proxy_uri);
        }
    }

    if (cfg && cfg->user_agent) {
        g_object_set(session, SOUP_SESSION_USER_AGENT, cfg->user_agent, NULL);
    }

    return session;
}

static
MMSHttpTransfer*
mms_http_transfer_new(
    const MMSSettingsSimData* cfg,
    MMSConnection* connection,
    const char* method,
    const char* uri,
    int receive_fd,
    int send_fd)
{
    SoupURI* soup_uri = mms_http_uri_parse(uri);
    if (soup_uri) {
        MMSHttpTransfer* tx = g_new(MMSHttpTransfer, 1);
        tx->session = mms_http_create_session(cfg, connection);
        tx->message = soup_message_new_from_uri(method, soup_uri);
        tx->connection = mms_connection_ref(connection);
        tx->receive_fd = receive_fd;
        tx->send_fd = send_fd;
        soup_uri_free(soup_uri);
        soup_message_set_flags(tx->message,
            SOUP_MESSAGE_NO_REDIRECT |
            SOUP_MESSAGE_NEW_CONNECTION);
        if (cfg->uaprof && cfg->uaprof[0]) {
            const char* uaprof_header = "x-wap-profile";
            MMS_VERBOSE("%s %s", uaprof_header, cfg->uaprof);
            soup_message_headers_append(tx->message->request_headers,
                uaprof_header, cfg->uaprof);
        }
        return tx;
    }
    return NULL;
}

static
void
mms_http_transfer_free(
    MMSHttpTransfer* tx)
{
    if (tx) {
        soup_session_abort(tx->session);
        g_object_unref(tx->session);
        g_object_unref(tx->message);
        mms_connection_unref(tx->connection);
        if (tx->receive_fd >= 0) close(tx->receive_fd);
        if (tx->send_fd >= 0) close(tx->send_fd);
        g_free(tx);
    }
}

static
void
mms_task_http_set_state(
    MMSTaskHttp* http,
    MMS_HTTP_STATE new_state,
    SoupStatus ss)
{
    MMSTaskHttpPrivate* priv = http->priv;
    if (priv->transaction_state != new_state &&
        priv->transaction_state != MMS_HTTP_DONE) {
        MMSTaskHttpClass* klass = MMS_TASK_HTTP_GET_CLASS(http);
        priv->transaction_state = new_state;
        switch (new_state) {
        case MMS_HTTP_ACTIVE:
            if (klass->fn_started) klass->fn_started(http);
            break;
        case MMS_HTTP_PAUSED:
            if (klass->fn_paused) klass->fn_paused(http);
            break;
        case MMS_HTTP_DONE:
            if (klass->fn_done) klass->fn_done(http, priv->receive_path, ss);
            break;
        case MMS_HTTP_READY:
            break;
        }
    }
}

static
void
mms_task_http_finish_transfer(
    MMSTaskHttp* http)
{
    MMSTaskHttpPrivate* priv = http->priv;
    if (priv->tx) {
        SoupMessage* msg = priv->tx->message;
        if (priv->wrote_headers_signal_id) {
            g_signal_handler_disconnect(msg, priv->wrote_headers_signal_id);
            priv->wrote_headers_signal_id = 0;
        }
        if (priv->wrote_chunk_signal_id) {
            g_signal_handler_disconnect(msg, priv->wrote_chunk_signal_id);
            priv->wrote_chunk_signal_id = 0;
        }
        if (priv->got_chunk_signal_id) {
            g_signal_handler_disconnect(msg, priv->got_chunk_signal_id);
            priv->got_chunk_signal_id = 0;
        }
        mms_http_transfer_free(priv->tx);
        priv->tx = NULL;
    }
}

static
void
mms_task_http_finished(
    SoupSession* session,
    SoupMessage* msg,
    gpointer user_data)
{
    MMSTaskHttp* http = user_data;
    MMSTaskHttpPrivate* priv = http->priv;
    if (priv->tx && priv->tx->session == session) {
        MMS_HTTP_STATE next_http_state;
        MMSTask* task = &http->task;
        SoupStatus http_status = msg->status_code;

#if MMS_LOG_DEBUG
        if (priv->bytes_received) {
            MMS_DEBUG("HTTP status %u [%s] %u byte(s)", msg->status_code,
                soup_message_headers_get_content_type(msg->response_headers,
                NULL), priv->bytes_received);
        } else {
            MMS_DEBUG("HTTP status %u", msg->status_code);
        }
#endif /* MMS_LOG_DEBUG */

        if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
            next_http_state = MMS_HTTP_DONE;
            mms_task_set_state(task, MMS_TASK_STATE_DONE);
        } else {
            /* Will retry if this was an I/O error, otherwise we consider
             * it a permanent failure */
            if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
                if (mms_task_retry(task)) {
                    next_http_state = MMS_HTTP_PAUSED;
                } else {
                    next_http_state = MMS_HTTP_DONE;
                    http_status = SOUP_STATUS_CANCELLED;
                }
            } else {
                next_http_state = MMS_HTTP_DONE;
                MMS_WARN("HTTP error %u", msg->status_code);
                mms_task_set_state(task, MMS_TASK_STATE_DONE);
            }
        }
        mms_task_http_set_state(http, next_http_state, http_status);
    } else {
        MMS_VERBOSE_("ignoring stale completion message");
    }
}

static
void
mms_task_http_write_next_chunk(
    SoupMessage* msg,
    MMSTaskHttp* http)
{
    MMSTaskHttpPrivate* priv = http->priv;
    MMSHttpTransfer* tx = priv->tx;
#if MMS_LOG_VERBOSE
    if (priv->bytes_sent) MMS_VERBOSE("%d bytes sent", (int)priv->bytes_sent);
#endif
    MMS_ASSERT(tx && tx->message == msg);
    if (tx && tx->message == msg) {
        void* chunk = g_malloc(MMS_HTTP_MAX_CHUNK);
        int nbytes = read(tx->send_fd, chunk, MMS_HTTP_MAX_CHUNK);
        if (nbytes > 0) {
            priv->bytes_sent += nbytes;
            soup_message_body_append_take(msg->request_body, chunk, nbytes);
            return;
        }
        g_free(chunk);
    }
    soup_message_body_complete(msg->request_body);
}

static
void
mms_task_http_got_chunk(
    SoupMessage* msg,
    SoupBuffer* buf,
    MMSTaskHttp* http)
{
    MMSTaskHttpPrivate* priv = http->priv;
    MMSHttpTransfer* tx = priv->tx;
    MMS_ASSERT(tx && tx->message == msg);
    if (tx && tx->message == msg) {
        priv->bytes_received += buf->length;
        MMS_VERBOSE("%u bytes received", priv->bytes_received);
        if (write(tx->receive_fd, buf->data, buf->length) != (int)buf->length) {
            MMS_ERR("Write error: %s", strerror(errno));
            mms_task_http_finish_transfer(http);
            mms_task_http_set_state(http, MMS_HTTP_PAUSED, 0);
            mms_task_set_state(&http->task, MMS_TASK_STATE_SLEEP);
        }
    }
}

static
gboolean
mms_task_http_start(
    MMSTaskHttp* http,
    MMSConnection* connection)
{
    int send_fd = -1;
    int receive_fd = -1;
    guint bytes_to_send = 0;
    MMSTaskHttpPrivate* priv = http->priv;
    MMS_ASSERT(mms_connection_is_open(connection));
    mms_task_http_finish_transfer(http);
    priv->bytes_sent = 0;

    /* Open the files */
    if (priv->send_path) {
        send_fd = open(priv->send_path, O_RDONLY);
        if (send_fd >= 0) {
            struct stat st;
            int err = fstat(send_fd, &st);
            if (!err) {
                bytes_to_send = st.st_size;
            } else {
                MMS_ERR("Can't stat %s: %s", priv->send_path, strerror(errno));
                close(send_fd);
                send_fd = -1;
            }
        } else {
            MMS_ERR("Can't open %s: %s", priv->send_path, strerror(errno));
        }
    }

    if (priv->receive_file) {
        char* dir = mms_task_dir(&http->task);
        if (priv->receive_path) {
            unlink(priv->receive_path);
            g_free(priv->receive_path);
            priv->receive_path = NULL;
        }
        receive_fd = mms_create_file(dir, priv->receive_file,
            &priv->receive_path, NULL);
        g_free(dir);
    }

    if ((!priv->send_path || send_fd >= 0) &&
        (!priv->receive_path || receive_fd >= 0) &&
        (send_fd >= 0 || receive_fd >= 0)) {

        /* Set up the transfer */
        const char* uri = priv->uri ? priv->uri : connection->mmsc;
        priv->tx = mms_http_transfer_new(mms_task_sim_settings(&http->task),
            connection, priv->send_path ? SOUP_METHOD_POST : SOUP_METHOD_GET,
            uri, receive_fd, send_fd);
        if (priv->tx) {
            SoupMessage* msg = priv->tx->message;
            soup_message_body_set_accumulate(msg->response_body, FALSE);

            /* If we have data to send */
            if (priv->send_path) {
                soup_message_headers_set_content_type(
                    msg->request_headers,
                    MMS_CONTENT_TYPE, NULL);
                soup_message_headers_set_content_length(
                    msg->request_headers,
                    bytes_to_send);

                /* Connect the signals */
                priv->wrote_headers_signal_id =
                    g_signal_connect(msg, "wrote_headers",
                    G_CALLBACK(mms_task_http_write_next_chunk), http);
                priv->wrote_chunk_signal_id =
                    g_signal_connect(msg, "wrote_chunk",
                    G_CALLBACK(mms_task_http_write_next_chunk), http);
            }

            /* If we expect to receive data */
            if (priv->receive_path) {
                priv->got_chunk_signal_id =
                    g_signal_connect(msg, "got-chunk",
                    G_CALLBACK(mms_task_http_got_chunk), http);
            }

            /* Start the transfer */
#if MMS_LOG_DEBUG
            if (priv->send_path) {
                if (priv->receive_path) {
                    MMS_DEBUG("%s (%u bytes) -> %s -> %s", priv->send_path,
                        bytes_to_send, uri, priv->receive_path);
                } else {
                    MMS_DEBUG("%s (%u bytes) -> %s", priv->send_path,
                        bytes_to_send, uri);
                }
            } else {
                MMS_DEBUG("%s -> %s", uri, priv->receive_path);

            }
#endif /* MMS_LOG_DEBUG */

            mms_task_http_set_state(http, MMS_HTTP_ACTIVE, 0);

            /* Soup message queue will unref the message when it's finished
             * with it, so we need to add one more reference if we need to
             * keep the message pointer too. */
            g_object_ref(msg);
            soup_session_queue_message(priv->tx->session, msg,
                mms_task_http_finished, http);

            return TRUE;
        }
    }
    if (receive_fd >= 0) close(receive_fd);
    if (send_fd >= 0) close(send_fd);
    return FALSE;
}

static
void
mms_task_http_transmit(
    MMSTask* task,
    MMSConnection* conn)
{
    if (task->state != MMS_TASK_STATE_TRANSMITTING) {
        MMSTaskHttp* http = MMS_TASK_HTTP(task);
        if (mms_task_http_start(http, conn)) {
            mms_task_set_state(task, MMS_TASK_STATE_TRANSMITTING);
        } else {
            MMSTaskHttpClass* klass = MMS_TASK_HTTP_GET_CLASS(task);
            if (klass->fn_done) {
                klass->fn_done(http, NULL, SOUP_STATUS_IO_ERROR);
            }
            mms_task_set_state(task, MMS_TASK_STATE_DONE);
        }
    }
}

static
void
mms_task_http_run(
    MMSTask* task)
{
    mms_task_set_state(task, MMS_TASK_STATE_NEED_CONNECTION);
}

static
void
mms_task_http_network_unavailable(
    MMSTask* task,
    gboolean can_retry)
{
    if (can_retry) {
        mms_task_http_finish_transfer(MMS_TASK_HTTP(task));
        mms_task_set_state(task, MMS_TASK_STATE_SLEEP);
    } else {
        mms_task_cancel(task);
    }
}

static
void
mms_task_http_cancel(
    MMSTask* task)
{
    MMSTaskHttp* http = MMS_TASK_HTTP(task);
    mms_task_http_finish_transfer(http);
    mms_task_http_set_state(http, MMS_HTTP_DONE, SOUP_STATUS_CANCELLED);
    MMS_TASK_CLASS(mms_task_http_parent_class)->fn_cancel(task);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_task_http_dispose(
    GObject* object)
{
    MMSTaskHttp* http = MMS_TASK_HTTP(object);
    mms_task_http_finish_transfer(http);
    G_OBJECT_CLASS(mms_task_http_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
mms_task_http_finalize(
    GObject* object)
{
    MMSTaskHttp* http = MMS_TASK_HTTP(object);
    MMS_ASSERT(!http->priv->tx);
    MMS_ASSERT(!http->priv->got_chunk_signal_id);
    MMS_ASSERT(!http->priv->wrote_headers_signal_id);
    MMS_ASSERT(!http->priv->wrote_chunk_signal_id);
    if (!task_config(&http->task)->keep_temp_files) {
        mms_remove_file_and_dir(http->priv->send_path);
        mms_remove_file_and_dir(http->priv->receive_path);
    }
    g_free(http->priv->uri);
    g_free(http->priv->send_path);
    g_free(http->priv->receive_path);
    g_free(http->priv);
    G_OBJECT_CLASS(mms_task_http_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_task_http_class_init(
    MMSTaskHttpClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    MMSTaskClass* task_class = &klass->task;
    task_class->fn_run = mms_task_http_run;
    task_class->fn_transmit = mms_task_http_transmit;
    task_class->fn_network_unavailable = mms_task_http_network_unavailable;
    task_class->fn_cancel = mms_task_http_cancel;
    object_class->dispose = mms_task_http_dispose;
    object_class->finalize = mms_task_http_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_task_http_init(
    MMSTaskHttp* up)
{
}

/**
 * Create MMS http task
 */
void*
mms_task_http_alloc(
    GType type,                 /* Zero for MMS_TYPE_TASK_HTTP       */
    MMSSettings* settings,      /* Settings                          */
    MMSHandler* handler,        /* MMS handler                       */
    const char* name,           /* Task name                         */
    const char* id,             /* Database message id               */
    const char* imsi,           /* IMSI associated with the message  */
    const char* uri,            /* NULL to use MMSC URL              */
    const char* receive_file,   /* File to write data to (optional)  */
    const char* send_file)      /* File to read data from (optional) */
{
    MMSTaskHttp* http = mms_task_alloc(type ? type : MMS_TYPE_TASK_HTTP,
        settings, handler, name, id, imsi);
    MMSTaskHttpPrivate* priv = g_new0(MMSTaskHttpPrivate, 1);
    http->priv = priv;
    priv->uri = g_strdup(uri);
    priv->receive_file = receive_file; /* Always static, don't strdup */
    if (send_file) {
        priv->send_path = mms_task_file(&http->task, send_file);
        MMS_ASSERT(g_file_test(priv->send_path, G_FILE_TEST_IS_REGULAR));
    }
    return http;
}

void*
mms_task_http_alloc_with_parent(
    GType type,                 /* Zero for MMS_TYPE_TASK_HTTP       */
    MMSTask* parent,            /* Parent task                       */
    const char* name,           /* Task name                         */
    const char* uri,            /* NULL to use MMSC URL              */
    const char* receive_file,   /* File to write data to (optional)  */
    const char* send_file)      /* File to read data from (optional) */
{
    return mms_task_http_alloc(type, parent->settings, parent->handler,
        name, parent->id, parent->imsi, uri, receive_file, send_file);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
