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
#include "mms_handler.h"
#include "mms_file_util.h"

#ifdef _WIN32
#  define snprintf _snprintf
#endif

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-task");

#define MMS_TASK_DEFAULT_LIFETIME (600)

G_DEFINE_TYPE(MMSTask, mms_task, G_TYPE_OBJECT);

#define MMS_TASK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MMS_TYPE_TASK, MMSTask))
#define MMS_TASK_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MMS_TYPE_TASK, MMSTaskClass))

static
void
mms_task_wakeup_free(
    gpointer data)
{
    mms_task_unref(data);
}

static
gboolean
mms_task_wakeup_callback(
    gpointer data)
{
    MMSTask* task = data;
    task->wakeup_id = 0;
    MMS_ASSERT(task->state == MMS_TASK_STATE_SLEEP);
    mms_task_set_state(task, MMS_TASK_STATE_READY);
    return FALSE;
}

gboolean
mms_task_schedule_wakeup(
    MMSTask* task,
    unsigned int secs)
{
    const time_t now = time(NULL);
    if (!secs) secs = task->config->retry_secs;

    /* Cancel the previous sleep */
    if (task->wakeup_id) {
        MMS_ASSERT(task->state == MMS_TASK_STATE_SLEEP);
        g_source_remove(task->wakeup_id);
        task->wakeup_id = 0;
    }

    if (now < task->deadline) {
        /* Don't sleep past deadline */
        const unsigned int max_secs = task->deadline - now;
        if (secs > max_secs) secs = max_secs;
        /* Schedule wakeup */
        task->wakeup_time = now + secs;
        task->wakeup_id = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
            secs, mms_task_wakeup_callback, mms_task_ref(task),
            mms_task_wakeup_free);
        MMS_ASSERT(task->wakeup_id);
        MMS_VERBOSE("%s sleeping for %u sec", task->name, secs);
    }

    return (task->wakeup_id > 0);
}

gboolean
mms_task_sleep(
    MMSTask* task,
    unsigned int secs)
{
    gboolean ok = mms_task_schedule_wakeup(task, secs);
    mms_task_set_state(task, ok ? MMS_TASK_STATE_SLEEP : MMS_TASK_STATE_DONE);
    return ok;
}

static
void
mms_task_cancel_cb(
    MMSTask* task)
{
    if (task->wakeup_id) {
        MMS_ASSERT(task->state == MMS_TASK_STATE_SLEEP);
        g_source_remove(task->wakeup_id);
        task->wakeup_id = 0;
    }
    task->flags |= MMS_TASK_FLAG_CANCELLED;
    mms_task_set_state(task, MMS_TASK_STATE_DONE);
}

static
void
mms_task_finalize(
    GObject* object)
{
    MMSTask* task = MMS_TASK(object);
    MMS_VERBOSE_("%p", task);
    MMS_ASSERT(!task->delegate);
    MMS_ASSERT(!task->wakeup_id);
    g_free(task->name);
    g_free(task->id);
    g_free(task->imsi);
    mms_handler_unref(task->handler);
    G_OBJECT_CLASS(mms_task_parent_class)->finalize(object);
}

static
void
mms_task_class_init(
    MMSTaskClass* klass)
{
    klass->fn_cancel = mms_task_cancel_cb;
    G_OBJECT_CLASS(klass)->finalize = mms_task_finalize;
}

static
void
mms_task_init(
    MMSTask* task)
{
    MMS_VERBOSE_("%p", task);
}

void*
mms_task_alloc(
    GType type,
    const MMSConfig* config,
    MMSHandler* handler,
    const char* name,
    const char* id,
    const char* imsi)
{
    MMSTask* task = g_object_new(type, NULL);
    const time_t now = time(NULL);
    time_t max_lifetime = MMS_TASK_GET_CLASS(task)->max_lifetime;
    if (!max_lifetime) max_lifetime = MMS_TASK_DEFAULT_LIFETIME;
    task->config = config;
    task->handler = mms_handler_ref(handler);
    if (name) {
        task->name = id ?
            g_strdup_printf("%s[%.08s]", name, id) :
            g_strdup(name);
    }
    task->id = g_strdup(id);
    task->imsi = g_strdup(imsi);
    task->deadline = now + max_lifetime;
    return task;
}

MMSTask*
mms_task_ref(
    MMSTask* task)
{
    if (task) g_object_ref(MMS_TASK(task));
    return task;
}

void
mms_task_unref(
    MMSTask* task)
{
    if (task) g_object_unref(MMS_TASK(task));
}

void
mms_task_run(
    MMSTask* task)
{
    MMS_ASSERT(task->state == MMS_TASK_STATE_READY);
    MMS_TASK_GET_CLASS(task)->fn_run(task);
    time(&task->last_run_time);
    MMS_ASSERT(task->state != MMS_TASK_STATE_READY);
}

void
mms_task_transmit(
    MMSTask* task,
    MMSConnection* connection)
{
    MMS_ASSERT(task->state == MMS_TASK_STATE_NEED_CONNECTION ||
               task->state == MMS_TASK_STATE_NEED_USER_CONNECTION);
    MMS_TASK_GET_CLASS(task)->fn_transmit(task, connection);
    time(&task->last_run_time);
    MMS_ASSERT(task->state != MMS_TASK_STATE_NEED_CONNECTION &&
               task->state != MMS_TASK_STATE_NEED_USER_CONNECTION);
}

void
mms_task_network_unavailable(
    MMSTask* task)
{
    if (task->state != MMS_TASK_STATE_DONE) {
        MMS_ASSERT(task->state == MMS_TASK_STATE_NEED_CONNECTION ||
                   task->state == MMS_TASK_STATE_NEED_USER_CONNECTION ||
                   task->state == MMS_TASK_STATE_TRANSMITTING);
        MMS_TASK_GET_CLASS(task)->fn_network_unavailable(task);
        MMS_ASSERT(task->state != MMS_TASK_STATE_NEED_CONNECTION &&
                   task->state != MMS_TASK_STATE_NEED_USER_CONNECTION &&
                   task->state != MMS_TASK_STATE_TRANSMITTING);
        time(&task->last_run_time);
    }
}

void
mms_task_cancel(
    MMSTask* task)
{
    MMS_DEBUG_("%s", task->name);
    MMS_TASK_GET_CLASS(task)->fn_cancel(task);
}

void
mms_task_set_state(
    MMSTask* task,
    MMS_TASK_STATE state)
{
    if (task->state != state) {
        MMS_DEBUG("%s %s -> %s", task->name,
            mms_task_state_name(task->state),
            mms_task_state_name(state));
        if (state == MMS_TASK_STATE_SLEEP && !task->wakeup_id) {
            if (!mms_task_schedule_wakeup(task, task->config->retry_secs)) {
                MMS_DEBUG("%s SLEEP -> DONE (no time left)", task->name);
                state = MMS_TASK_STATE_DONE;
            }
        }
        task->state = state;
        if (task->delegate && task->delegate->fn_task_state_changed) {
            task->delegate->fn_task_state_changed(task->delegate, task);
        }
    }
}

/* Utilities */

static const char* mms_task_names[] = {"READY", "NEED_CONNECTION",
    "NEED_USER_CONNECTION", "TRANSMITTING", "WORKING", "SLEEP", "DONE"
};
G_STATIC_ASSERT(G_N_ELEMENTS(mms_task_names) == MMS_TASK_STATE_COUNT);

const char*
mms_task_state_name(
    MMS_TASK_STATE state)
{
    if (state >= 0 && state < G_N_ELEMENTS(mms_task_names)) {
        return mms_task_names[state];
    } else {
        /* This shouldn't happen */
        static char unknown[32];
        snprintf(unknown, sizeof(unknown), "%d ????", state);
        return unknown;
    }
}

gboolean
mms_task_queue_and_unref(
    MMSTaskDelegate* delegate,
    MMSTask* task)
{
    gboolean ok = FALSE;
    if (task) {
        if (delegate && delegate->fn_task_queue) {
            delegate->fn_task_queue(delegate, task);
            ok = TRUE;
        }
        mms_task_unref(task);
    }
    return ok;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
