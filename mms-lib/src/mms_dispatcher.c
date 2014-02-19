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

#include "mms_dispatcher.h"
#include "mms_handler.h"
#include "mms_connection.h"
#include "mms_connman.h"
#include "mms_file_util.h"
#include "mms_codec.h"
#include "mms_util.h"
#include "mms_task.h"

#include <errno.h>

/* Logging */
#define MMS_LOG_MODULE_NAME mms_dispatcher_log
#include "mms_lib_log.h"
#include "mms_error.h"
MMS_LOG_MODULE_DEFINE("mms-dispatcher");

struct mms_dispatcher {
    gint ref_count;
    const MMSConfig* config;
    MMSTask* active_task;
    MMSTaskDelegate task_delegate;
    MMSHandler* handler;
    MMSConnMan* cm;
    MMSConnection* connection;
    MMSConnectionDelegate connection_delegate;
    MMSDispatcherDelegate* delegate;
    GQueue* tasks;
    guint next_run_id;
    guint network_idle_id;
};

typedef void (*MMSDispatcherIdleCallbackProc)(MMSDispatcher* disp);
typedef struct mms_dispatcher_idle_callback {
    MMSDispatcher* dispatcher;
    MMSDispatcherIdleCallbackProc proc;
} MMSDispatcherIdleCallback;

inline static MMSDispatcher*
mms_dispatcher_from_task_delegate(MMSTaskDelegate* delegate)
    { return MMS_CAST(delegate,MMSDispatcher,task_delegate); }
inline static MMSDispatcher*
mms_dispatcher_from_connection_delegate(MMSConnectionDelegate* delegate)
    { return MMS_CAST(delegate,MMSDispatcher,connection_delegate); }

static
void
mms_dispatcher_run(
    MMSDispatcher* disp);

/**
 * Close the network connection
 */
static
void
mms_dispatcher_close_connection(
    MMSDispatcher* disp)
{
    if (disp->connection) {
        disp->connection->delegate = NULL;
        mms_connection_close(disp->connection);
        mms_connection_unref(disp->connection);
        disp->connection = NULL;

        if (!mms_dispatcher_is_active(disp)) {
            /* Report to delegate that we are done */
            if (disp->delegate && disp->delegate->fn_done) {
                disp->delegate->fn_done(disp->delegate, disp);
            }
        }
    }
    if (disp->network_idle_id) {
        g_source_remove(disp->network_idle_id);
        disp->network_idle_id = 0;
    }
}

/**
 * Run loop callbacks
 */
static
void
mms_dispatcher_callback_free(
    gpointer data)
{
    MMSDispatcherIdleCallback* call = data;
    mms_dispatcher_unref(call->dispatcher);
    g_free(call);
}

static
gboolean
mms_dispatcher_idle_callback_cb(
    gpointer data)
{
    MMSDispatcherIdleCallback* call = data;
    call->proc(call->dispatcher);
    return FALSE;
}

static
guint
mms_dispatcher_callback_schedule(
    MMSDispatcher* disp,
    MMSDispatcherIdleCallbackProc proc)
{
    MMSDispatcherIdleCallback* call = g_new0(MMSDispatcherIdleCallback,1);
    call->dispatcher = mms_dispatcher_ref(disp);
    call->proc = proc;
    return g_idle_add_full(G_PRIORITY_HIGH, mms_dispatcher_idle_callback_cb,
        call, mms_dispatcher_callback_free);
}

static
guint
mms_dispatcher_timeout_callback_schedule(
    MMSDispatcher* disp,
    guint interval,
    MMSDispatcherIdleCallbackProc proc)
{
    MMSDispatcherIdleCallback* call = g_new0(MMSDispatcherIdleCallback,1);
    call->dispatcher = mms_dispatcher_ref(disp);
    call->proc = proc;
    return g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, interval,
        mms_dispatcher_idle_callback_cb, call, mms_dispatcher_callback_free);
}

/**
 * Network idle timeout
 */

static
void
mms_dispatcher_network_idle_run(
    MMSDispatcher* disp)
{
    MMS_ASSERT(disp->network_idle_id);
    disp->network_idle_id = 0;
    mms_dispatcher_close_connection(disp);
}

static
void
mms_dispatcher_network_idle_check(
    MMSDispatcher* disp)
{
    if (disp->connection && !disp->network_idle_id) {
        /* Schedule idle inactivity timeout callback */
        MMS_VERBOSE("Network connection is inactive");
        disp->network_idle_id = mms_dispatcher_timeout_callback_schedule(disp,
            disp->config->idle_secs, mms_dispatcher_network_idle_run);
    }
}

static
void
mms_dispatcher_network_idle_cancel(
    MMSDispatcher* disp)
{
    if (disp->network_idle_id) {
        MMS_VERBOSE("Cancel network inactivity timeout");
        g_source_remove(disp->network_idle_id);
        disp->network_idle_id = 0;
    }
}

/**
 * Dispatcher run on a fresh stack
 */
static
void
mms_dispatcher_next_run(
    MMSDispatcher* disp)
{
    MMS_ASSERT(disp->next_run_id);
    MMS_ASSERT(!disp->active_task);
    disp->next_run_id = 0;
    if (!disp->active_task) {
        mms_dispatcher_run(disp);
    }
}

static
void
mms_dispatcher_next_run_schedule(
    MMSDispatcher* disp)
{
    if (disp->next_run_id) g_source_remove(disp->next_run_id);
    disp->next_run_id = mms_dispatcher_callback_schedule(disp,
        mms_dispatcher_next_run);
}

/**
 * Set the delegate that receives dispatcher notifications.
 * One delegate per dispatcher.
 */
void
mms_dispatcher_set_delegate(
    MMSDispatcher* disp,
    MMSDispatcherDelegate* delegate)
{
    MMS_ASSERT(!disp->delegate || !delegate);
    disp->delegate = delegate;
}

/**
 * Checks if dispatcher has something to do.
 */
gboolean
mms_dispatcher_is_active(
    MMSDispatcher* disp)
{
    return disp && (disp->connection || disp->active_task ||
        !g_queue_is_empty(disp->tasks));
}

/**
 * Picks the next task for processing. Reference is passed to the caller.
 * Caller must eventually dereference the task or place it back to the queue.
 */
static
MMSTask*
mms_dispatcher_pick_next_task(
    MMSDispatcher* disp)
{
    GList* entry;
    gboolean connection_in_use = FALSE;

    /* Check the current connection */
    if (disp->connection) {

        /* Don't interfere with the task transmiting the data */
        for (entry = disp->tasks->head; entry; entry = entry->next) {
            MMSTask* task = entry->data;
            if (task->state == MMS_TASK_STATE_TRANSMITTING) {
                MMS_ASSERT(!strcmp(task->imsi, disp->connection->imsi));
                return NULL;
            }
        }

        /* Look for another task that has use for the existing connection
         * before we close it */
        for (entry = disp->tasks->head; entry; entry = entry->next) {
            MMSTask* task = entry->data;
            if (task->state == MMS_TASK_STATE_NEED_CONNECTION ||
                task->state == MMS_TASK_STATE_NEED_USER_CONNECTION) {
                 if (!strcmp(task->imsi, disp->connection->imsi)) {
                     if (mms_connection_state(disp->connection) ==
                         MMS_CONNECTION_STATE_OPEN) {
                        /* Found a task that can use this connection */
                        g_queue_delete_link(disp->tasks, entry);
                        mms_dispatcher_network_idle_cancel(disp);
                        return task;
                     }
                     connection_in_use = TRUE;
                 }
            }
        }
    }

    if (connection_in_use) {
        /* Connection is needed but isn't open yet, make sure that network
         * inactivity timer is off while connection is being established */
        mms_dispatcher_network_idle_cancel(disp);
    } else {
        /* Then look for a task that needs any sort of network connection */
        for (entry = disp->tasks->head; entry; entry = entry->next) {
            MMSTask* task = entry->data;
            if ((task->state == MMS_TASK_STATE_NEED_CONNECTION ||
                 task->state == MMS_TASK_STATE_NEED_USER_CONNECTION)) {
                mms_dispatcher_close_connection(disp);
                disp->connection = mms_connman_open_connection(
                    disp->cm, task->imsi, FALSE);
                if (disp->connection) {
                    disp->connection->delegate = &disp->connection_delegate;
                    g_queue_delete_link(disp->tasks, entry);
                    return task;
                } else {
                    mms_task_network_unavailable(task);
                }
            }
        }
    }

    /* Finally look for a runnable task that doesn't need network */
    for (entry = disp->tasks->head; entry; entry = entry->next) {
        MMSTask* task = entry->data;
        if (task->state == MMS_TASK_STATE_READY ||
            task->state == MMS_TASK_STATE_DONE) {
            g_queue_delete_link(disp->tasks, entry);
            return task;
        }
    }

    /* Nothing found, we are done for now */
    return NULL;
}

/**
 * Task dispatch loop.
 */
static
void
mms_dispatcher_run(
    MMSDispatcher* disp)
{
    MMSTask* task;
    MMS_ASSERT(!disp->active_task);
    while ((task = mms_dispatcher_pick_next_task(disp)) != NULL) {
        MMS_DEBUG("%s %s", task->name, mms_task_state_name(task->state));
        disp->active_task = task;
        switch (task->state) {
        case MMS_TASK_STATE_READY:
            mms_task_run(task);
            break;

        case MMS_TASK_STATE_NEED_CONNECTION:
        case MMS_TASK_STATE_NEED_USER_CONNECTION:
            MMS_ASSERT(disp->connection);
            if (mms_connection_is_open(disp->connection)) {
                /* Connection is already active, send/receive the data */
                mms_task_transmit(task, disp->connection);
            }
            break;

        default:
            break;
        }

        if (task->state == MMS_TASK_STATE_DONE) {
            task->delegate = NULL;
            mms_task_unref(task);
        } else {
            g_queue_push_tail(disp->tasks, task);
        }
        disp->active_task = NULL;
    }

    if (disp->connection) {
        /* Check if network connection is being used */
        GList* entry;
        gboolean connection_in_use = FALSE;
        for (entry = disp->tasks->head; entry; entry = entry->next) {
            MMSTask* task = entry->data;
            if (task->state == MMS_TASK_STATE_NEED_CONNECTION ||
                task->state == MMS_TASK_STATE_NEED_USER_CONNECTION ||
                task->state == MMS_TASK_STATE_TRANSMITTING) {
                connection_in_use = TRUE;
                break;
            }
        }
        if (connection_in_use) {
            /* It's in use, disable idle inactivity callback */
            mms_dispatcher_network_idle_cancel(disp);
        } else {
            /* Make sure that network inactivity timer is ticking */
            mms_dispatcher_network_idle_check(disp);
        }
    }

    if (!mms_dispatcher_is_active(disp)) {
        /* Report to delegate that we are done */
        if (disp->delegate && disp->delegate->fn_done) {
            disp->delegate->fn_done(disp->delegate, disp);
        }
    }
}

/**
 * Starts task processing.
 */
gboolean
mms_dispatcher_start(
    MMSDispatcher* disp)
{
    int err = g_mkdir_with_parents(disp->config->root_dir, MMS_DIR_PERM);
    if (!err || errno == EEXIST) {
        if (!g_queue_is_empty(disp->tasks)) {
            mms_dispatcher_next_run_schedule(disp);
            return TRUE;
        }
    } else {
        MMS_ERR("Failed to create %s: %s", disp->config->root_dir,
            strerror(errno));
    }
    return FALSE;
}

static
void
mms_dispatcher_queue_task(
    MMSDispatcher* disp,
    MMSTask* task)
{
    task->delegate = &disp->task_delegate;
    g_queue_push_tail(disp->tasks, mms_task_ref(task));
}

static
gboolean
mms_dispatcher_queue_and_unref_task(
    MMSDispatcher* disp,
    MMSTask* task)
{
    if (task) {
        mms_dispatcher_queue_task(disp, task);
        mms_task_unref(task);
        return TRUE;
    } else {
        return FALSE;
    }
}

/**
 * Creates a WAP push receive task and adds it to the queue.
 */
gboolean
mms_dispatcher_handle_push(
    MMSDispatcher* disp,
    const char* imsi,
    GBytes* push,
    GError** error)
{
    return mms_dispatcher_queue_and_unref_task(disp,
        mms_task_notification_new(disp->config, disp->handler,
            imsi, push, error));
}

/**
 * Creates download task and adds it to the queue.
 */
gboolean
mms_dispatcher_receive_message(
    MMSDispatcher* disp,
    const char* id,
    const char* imsi,
    gboolean automatic,
    GBytes* bytes,
    GError** error)
{
    gboolean ok = FALSE;
    MMSPdu* pdu = mms_decode_bytes(bytes);
    if (pdu) {
        MMS_ASSERT(pdu->type == MMS_MESSAGE_TYPE_NOTIFICATION_IND);
        if (pdu->type == MMS_MESSAGE_TYPE_NOTIFICATION_IND) {
            ok = mms_dispatcher_queue_and_unref_task(disp,
                mms_task_retrieve_new(disp->config, disp->handler,
                    id, imsi, pdu, error));
        }
        mms_message_free(pdu);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_DECODE, "Failed to decode MMS PDU");
    }
    return ok;
}

/**
 * Sends read report
 */
gboolean
mms_dispatcher_send_read_report(
    MMSDispatcher* disp,
    const char* id,
    const char* imsi,
    const char* message_id,
    const char* to,
    MMSReadStatus status,
    GError** error)
{
    return mms_dispatcher_queue_and_unref_task(disp,
        mms_task_read_new(disp->config, disp->handler,
            id, imsi, message_id, to, status, error));
}

/**
 * Cancels al the activity associated with the specified message
 */
void
mms_dispatcher_cancel(
    MMSDispatcher* disp,
    const char* id)
{
    GList* entry;
    for (entry = disp->tasks->head; entry; entry = entry->next) {
        MMSTask* task = entry->data;
        if (!id || !strcmp(task->id, id)) {
            mms_task_cancel(task);
        }
    }
    if (disp->active_task && (!id || !strcmp(disp->active_task->id, id))) {
        mms_task_cancel(disp->active_task);
    }
}

/**
 * Connection delegate callbacks
 */
static
void
mms_dispatcher_delegate_connection_state_changed(
    MMSConnectionDelegate* delegate,
    MMSConnection* conn)
{
    MMSDispatcher* disp = mms_dispatcher_from_connection_delegate(delegate);
    MMS_CONNECTION_STATE state = mms_connection_state(conn);
    MMS_VERBOSE_("%s %s", conn->imsi, mms_connection_state_name(conn));
    MMS_ASSERT(conn == disp->connection);
    if (state == MMS_CONNECTION_STATE_FAILED ||
        state == MMS_CONNECTION_STATE_CLOSED) {
        GList* entry;
        mms_dispatcher_close_connection(disp);
        for (entry = disp->tasks->head; entry; entry = entry->next) {
            MMSTask* task = entry->data;
            switch (task->state) {
            case MMS_TASK_STATE_NEED_CONNECTION:
            case MMS_TASK_STATE_NEED_USER_CONNECTION:
            case MMS_TASK_STATE_TRANSMITTING:
                if (!strcmp(conn->imsi, task->imsi)) {
                    mms_task_network_unavailable(task);
                }
            default:
                break;
            }
        }
    }
    if (!disp->active_task) {
        mms_dispatcher_next_run_schedule(disp);
    }
}

/**
 * Task delegate callbacks
 */
static
void
mms_dispatcher_delegate_task_queue(
    MMSTaskDelegate* delegate,
    MMSTask* task)
{
    MMSDispatcher* disp = mms_dispatcher_from_task_delegate(delegate);
    mms_dispatcher_queue_task(disp, task);
    if (!disp->active_task) {
        mms_dispatcher_next_run_schedule(disp);
    }
}

static
void
mms_dispatcher_delegate_task_state_changed(
    MMSTaskDelegate* delegate,
    MMSTask* task)
{
    MMSDispatcher* disp = mms_dispatcher_from_task_delegate(delegate);
    if (!disp->active_task) {
        mms_dispatcher_next_run_schedule(disp);
    }
}

/**
 * Creates the dispatcher object. Caller must clal mms_dispatcher_unref
 * when it no longer needs it.
 */
MMSDispatcher*
mms_dispatcher_new(
    const MMSConfig* config,
    MMSConnMan* cm,
    MMSHandler* handler)
{
    MMSDispatcher* disp = g_new0(MMSDispatcher, 1);
    disp->ref_count = 1;
    disp->config = config;
    disp->tasks = g_queue_new();
    disp->handler = mms_handler_ref(handler);
    disp->cm = mms_connman_ref(cm);
    disp->task_delegate.fn_task_queue =
        mms_dispatcher_delegate_task_queue;
    disp->task_delegate.fn_task_state_changed =
        mms_dispatcher_delegate_task_state_changed;
    disp->connection_delegate.fn_connection_state_changed =
        mms_dispatcher_delegate_connection_state_changed;
    return disp;
}

/**
 * Deinitializer
 */
static
void
mms_dispatcher_finalize(
    MMSDispatcher* disp)
{
    MMSTask* task;
    char* msg_dir = g_strconcat(disp->config->root_dir,
        "/" MMS_MESSAGE_DIR "/", NULL);

    MMS_VERBOSE_("");
    mms_dispatcher_close_connection(disp);
    while ((task = g_queue_pop_head(disp->tasks)) != NULL) {
        task->delegate = NULL;
        mms_task_cancel(task);
        mms_task_unref(task);
    }
    g_queue_free(disp->tasks);
    mms_handler_unref(disp->handler);
    mms_connman_unref(disp->cm);

    /* Try to remove the message directory */
    remove(msg_dir);
    g_free(msg_dir);
}

/**
 * Reference counting. NULL argument is safely ignored.
 */
MMSDispatcher*
mms_dispatcher_ref(
    MMSDispatcher* disp)
{
    if (disp) {
        MMS_ASSERT(disp->ref_count > 0);
        g_atomic_int_inc(&disp->ref_count);
    }
    return disp;
}

void
mms_dispatcher_unref(
    MMSDispatcher* disp)
{
    if (disp) {
        MMS_ASSERT(disp->ref_count > 0);
        if (g_atomic_int_dec_and_test(&disp->ref_count)) {
            mms_dispatcher_finalize(disp);
            g_free(disp);
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
