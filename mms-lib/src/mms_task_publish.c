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

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_publish_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-task-publish");

/* Class definition */
typedef MMSTaskClass MMSTaskPublishClass;
typedef struct mms_task_publish {
    MMSTask task;
    MMSMessage* msg;
} MMSTaskPublish;

G_DEFINE_TYPE(MMSTaskPublish, mms_task_publish, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_PUBLISH (mms_task_publish_get_type())
#define MMS_TASK_PUBLISH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_PUBLISH, MMSTaskPublish))

static
void
mms_task_publish_run(
    MMSTask* task)
{
    MMSTaskPublish* pub = MMS_TASK_PUBLISH(task);
    if (mms_handler_message_received(task->handler, pub->msg)) {
        mms_task_set_state(task, MMS_TASK_STATE_DONE);
    } else {
        mms_task_set_state(task, MMS_TASK_STATE_SLEEP);
    }
}

static
void
mms_task_publish_finalize(
    GObject* object)
{
    MMSTaskPublish* pub = MMS_TASK_PUBLISH(object);
    mms_message_unref(pub->msg);
    G_OBJECT_CLASS(mms_task_publish_parent_class)->finalize(object);
}

static
void
mms_task_publish_class_init(
    MMSTaskPublishClass* klass)
{
    klass->fn_run = mms_task_publish_run;
    G_OBJECT_CLASS(klass)->finalize = mms_task_publish_finalize;
}

static
void
mms_task_publish_init(
    MMSTaskPublish* publish)
{
}

/* Create MMS publish task */
MMSTask*
mms_task_publish_new(
    const MMSConfig* config,
    MMSHandler* handler,
    MMSMessage* msg)
{
    MMS_ASSERT(msg && msg->id);
    if (msg && msg->id) {
        MMSTaskPublish* pub = mms_task_alloc(MMS_TYPE_TASK_PUBLISH,
            config, handler, "Publish", msg->id, NULL);
        pub->msg = mms_message_ref(msg);
        return &pub->task;
    } else {
        return NULL;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
