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

#ifndef JOLLA_MMS_TASK_H
#define JOLLA_MMS_TASK_H

#include "mms_lib_types.h"

/* Claim MMS 1.1 support */
#define MMS_VERSION MMS_MESSAGE_VERSION_1_1

/* mms_codec.h */
typedef enum mms_message_notify_status MMSNotifyStatus;

/* Task state */
typedef enum _MMS_TASK_STATE {
    MMS_TASK_STATE_READY,                /* Ready to run */
    MMS_TASK_STATE_NEED_CONNECTION,      /* Network connection us needed */
    MMS_TASK_STATE_NEED_USER_CONNECTION, /* Connection requested by user */
    MMS_TASK_STATE_TRANSMITTING,         /* Sending or receiving the data */
    MMS_TASK_STATE_WORKING,              /* Active but not using network */
    MMS_TASK_STATE_SLEEP,                /* Will change state later */
    MMS_TASK_STATE_DONE,                 /* Nothing left to do */
    MMS_TASK_STATE_COUNT                 /* Number of valid states */
} MMS_TASK_STATE;

/* Delegate (one per task) */
typedef struct mms_task MMSTask;
typedef struct mms_task_delegate MMSTaskDelegate;
struct mms_task_delegate {
    /* Submits new task to the queue */
    void (*fn_task_queue)(
        MMSTaskDelegate* delegate,
        MMSTask* task);
    /* Task has changed its state */
    void (*fn_task_state_changed)(
        MMSTaskDelegate* delegate,
        MMSTask* task);
};

/* Task object */
struct mms_task {
    GObject parent;                      /* Parent object */
    char* name;                          /* Task name for debug purposes */
    char* id;                            /* Database record ID */
    char* imsi;                          /* Associated subscriber identity */
    const MMSConfig* config;             /* Immutable configuration */
    MMSHandler* handler;                 /* Message database interface */
    MMSTaskDelegate* delegate;           /* Observer */
    MMS_TASK_STATE state;                /* Task state */
    time_t last_run_time;                /* Last run time */
    time_t deadline;                     /* Task deadline */
    time_t wakeup_time;                  /* Wake up time (if sleeping) */
    guint wakeup_id;                     /* ID of the wakeup source */
    int flags;                           /* Flags: */

#define MMS_TASK_FLAG_CANCELLED (0x01)   /* Task has been cancelled */

};

typedef struct mms_task_class {
    GObjectClass parent;
    time_t max_lifetime;                 /* Maximum lifetime, in seconds */
    /* Invoked in IDLE/RETRY state to get the task going */
    void (*fn_run)(MMSTask* task);
    /* Invoked in NEED_[USER_]CONNECTION state */
    void (*fn_transmit)(MMSTask* task, MMSConnection* conn);
    /* Invoked in NEED_[USER_]CONNECTION or TRANSMITTING state */
    void (*fn_network_unavailable)(MMSTask* task);
    /* May be invoked in any state */
    void (*fn_cancel)(MMSTask* task);
} MMSTaskClass;

GType mms_task_get_type(void);
#define MMS_TYPE_TASK (mms_task_get_type())
#define MMS_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        MMS_TYPE_TASK, MMSTaskClass))

void*
mms_task_alloc(
    GType type,
    const MMSConfig* config,
    MMSHandler* handler,
    const char* name,
    const char* id,
    const char* imsi);

MMSTask*
mms_task_ref(
    MMSTask* task);

void
mms_task_unref(
    MMSTask* task);

void
mms_task_run(
    MMSTask* task);

void
mms_task_transmit(
    MMSTask* task,
    MMSConnection* connection);

void
mms_task_network_unavailable(
    MMSTask* task);

void
mms_task_cancel(
    MMSTask* task);

void
mms_task_set_state(
    MMSTask* task,
    MMS_TASK_STATE state);

gboolean
mms_task_sleep(
    MMSTask* task,
    unsigned int secs);

#define mms_task_retry(task) \
    mms_task_sleep(task, 0)

/* Utilities */
const char*
mms_task_state_name(
    MMS_TASK_STATE state);

gboolean
mms_task_queue_and_unref(
    MMSTaskDelegate* delegate,
    MMSTask* task);

const char*
mms_task_make_id(
    MMSTask* task);

/* Create particular types of tasks */
MMSTask*
mms_task_notification_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* imsi,
    GBytes* bytes,
    GError** error);

MMSTask*
mms_task_retrieve_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const MMSPdu* pdu,
    GError** error);

MMSTask*
mms_task_decode_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* transaction_id,
    const char* file);

MMSTask*
mms_task_notifyresp_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* transaction_id,
    MMSNotifyStatus status);

MMSTask*
mms_task_ack_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* transaction_id);

MMSTask*
mms_task_read_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* message_id,
    const char* to,
    MMSReadStatus status,
    GError** error);

MMSTask*
mms_task_publish_new(
    const MMSConfig* config,
    MMSHandler* handler,
    MMSMessage* msg);

MMSTask*
mms_task_encode_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* to,
    const char* cc,
    const char* bcc,
    const char* subject,
    int flags,
    const MMSAttachmentInfo* parts,
    int nparts,
    GError** error);

MMSTask*
mms_task_send_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi);

#endif /* JOLLA_MMS_TASK_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
