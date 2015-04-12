/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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

#ifndef JOLLA_MMS_HANDLER_H
#define JOLLA_MMS_HANDLER_H

#include "mms_message.h"

/* Receive state */
typedef enum _mms_receive_state {
    MMS_RECEIVE_STATE_INVALID = -1,
    MMS_RECEIVE_STATE_RECEIVING,
    MMS_RECEIVE_STATE_DEFERRED,
    MMS_RECEIVE_STATE_NOSPACE,
    MMS_RECEIVE_STATE_DECODING,
    MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
    MMS_RECEIVE_STATE_DECODING_ERROR
} MMS_RECEIVE_STATE;

/* Send state */
typedef enum _mms_send_state {
    MMS_SEND_STATE_INVALID = -1,
    MMS_SEND_STATE_ENCODING,
    MMS_SEND_STATE_TOO_BIG,
    MMS_SEND_STATE_SENDING,
    MMS_SEND_STATE_DEFERRED,
    MMS_SEND_STATE_NO_SPACE,
    MMS_SEND_STATE_SEND_ERROR,
    MMS_SEND_STATE_REFUSED
} MMS_SEND_STATE;

/* Delivery status */
typedef enum _mms_delivery_status {
    MMS_DELIVERY_STATUS_INVALID = -1,
    MMS_DELIVERY_STATUS_UNKNOWN,
    MMS_DELIVERY_STATUS_EXPIRED,
    MMS_DELIVERY_STATUS_RETRIEVED,
    MMS_DELIVERY_STATUS_REJECTED,
    MMS_DELIVERY_STATUS_DEFERRED,
    MMS_DELIVERY_STATUS_UNRECOGNISED,
    MMS_DELIVERY_STATUS_FORWARDED,
    MMS_DELIVERY_STATUS_UNREACHABLE
} MMS_DELIVERY_STATUS;

/* Read status */
typedef MMSReadStatus MMS_READ_STATUS;

/* Read report status */
typedef enum _mms_read_report_status {
    MMS_READ_REPORT_STATUS_INVALID = -1,
    MMS_READ_REPORT_STATUS_OK,
    MMS_READ_REPORT_STATUS_IO_ERROR,
    MMS_READ_REPORT_STATUS_PERMANENT_ERROR
} MMS_READ_REPORT_STATUS;

/* Handler event callback */
typedef void
(*mms_handler_event_fn)(
    MMSHandler* handler,
    void* param);

/* Asynchronous incoming message notification. Non-empty message id means
 * that we start download the message immediately, empty string means that
 * download is postponed, NULL id means that an error has occured. */
typedef struct mms_handler_message_notify_call MMSHandlerMessageNotifyCall;
typedef void
(*mms_handler_message_notify_complete_fn)(
    MMSHandlerMessageNotifyCall* call,
    const char* id,
    void* param);

/* Asynchronous message received notification. Note that the files associated
 * with MMSMessage must not be deleted until after the call completes. The
 * call context is carrying a reference to MMSMessage with it, that should
 * take care of it even if the caller drops its reference immediately after
 * submitting the call. */
typedef struct mms_handler_message_received_call MMSHandlerMessageReceivedCall;
typedef void
(*mms_handler_message_received_complete_fn)(
    MMSHandlerMessageReceivedCall* call,
    MMSMessage* msg,
    gboolean ok,
    void* param);

/* Instance */
struct mms_handler {
    GObject object;
    int busy;
};

/* Class */
typedef struct mms_handler_class {
    GObjectClass parent;

    /* New incoming message notification (cancellable) */
    MMSHandlerMessageNotifyCall* (*fn_message_notify)(
        MMSHandler* handler,        /* Handler instance */
        const char* imsi,           /* Subscriber identity */
        const char* from,           /* Sender's phone number */
        const char* subject,        /* Subject (optional) */
        time_t expiry,              /* Message expiry time */
        GBytes* push,               /* Raw push message */
        mms_handler_message_notify_complete_fn cb,
        void* param);

    void (*fn_message_notify_cancel)(
        MMSHandler* handler,
        MMSHandlerMessageNotifyCall* call);

    /* Message has been successfully received (cancellable) */
    MMSHandlerMessageReceivedCall* (*fn_message_received)(
        MMSHandler* handler,        /* Handler instance */
        MMSMessage* msg,            /* Decoded message  */
        mms_handler_message_received_complete_fn cb,
        void* param);

    void (*fn_message_received_cancel)(
        MMSHandler* handler,
        MMSHandlerMessageReceivedCall* call);

    /* Sets the receive state */
    gboolean (*fn_message_receive_state_changed)(
        MMSHandler* handler,        /* Handler instance */
        const char* id,             /* Handler record id */
        MMS_RECEIVE_STATE state);   /* Receive state */

    /* Sets the send state */
    gboolean (*fn_message_send_state_changed)(
        MMSHandler* handler,        /* Handler instance */
        const char* id,             /* Handler record id */
        MMS_SEND_STATE state,       /* Receive state */
        const char* details);       /* Optional details */

    /* Message has been sent */
    gboolean (*fn_message_sent)(
        MMSHandler* handler,        /* Handler instance */
        const char* id,             /* Handler record id */
        const char* msgid);         /* Message id assigned by operator */

    /* Delivery report has been received */
    gboolean (*fn_delivery_report)(
        MMSHandler* handler,        /* Handler instance */
        const char* imsi,           /* Subscriber identity */
        const char* msgid,          /* Message id assigned by operator */
        const char* recipient,      /* Recipient's phone number */
        MMS_DELIVERY_STATUS ds);    /* Delivery status */

    /* Read report has been received */
    gboolean (*fn_read_report)(
        MMSHandler* handler,        /* Handler instance */
        const char* imsi,           /* Subscriber identity */
        const char* msgid,          /* Message id assigned by operator */
        const char* recipient,      /* Recipient's phone number */
        MMS_READ_STATUS rs);        /* Read status */

    /* Done with sending MMS read report */
    gboolean (*fn_read_report_send_status)(
        MMSHandler* handler,        /* Handler instance */
        const char* id,             /* Handler record id */
        MMS_READ_REPORT_STATUS s);  /* Status */

} MMSHandlerClass;

GType mms_handler_get_type(void);
#define MMS_TYPE_HANDLER (mms_handler_get_type())

MMSHandler*
mms_handler_ref(
    MMSHandler* handler);

void
mms_handler_unref(
    MMSHandler* handler);

MMSHandlerMessageNotifyCall*
mms_handler_message_notify(
    MMSHandler* handler,            /* Handler instance */
    const char* imsi,               /* Subscriber identity */
    const char* from,               /* Sender's phone number */
    const char* subject,            /* Subject (optional) */
    time_t expiry,                  /* Message expiry time */
    GBytes* push,                   /* Raw push message */
    mms_handler_message_notify_complete_fn cb,
    void* param);

void
mms_handler_message_notify_cancel(
    MMSHandler* handler,
    MMSHandlerMessageNotifyCall* call);

MMSHandlerMessageReceivedCall*
mms_handler_message_received(
    MMSHandler* handler,            /* Handler instance */
    MMSMessage* msg,                /* Decoded message  */
    mms_handler_message_received_complete_fn cb,
    void* param);

void
mms_handler_message_received_cancel(
    MMSHandler* handler,
    MMSHandlerMessageReceivedCall* call);

gboolean
mms_handler_message_receive_state_changed(
    MMSHandler* handler,            /* Handler instance */
    const char* id,                 /* Handler record id */
    MMS_RECEIVE_STATE state);       /* Receive state */

gboolean
mms_handler_message_send_state_changed(
    MMSHandler* handler,            /* Handler instance */
    const char* id,                 /* Handler record id */
    MMS_SEND_STATE state,           /* Receive state */
    const char* details);           /* Optional details */

gboolean
mms_handler_message_sent(
    MMSHandler* handler,            /* Handler instance */
    const char* id,                 /* Handler record id */
    const char* msgid);             /* Message id assigned by operator */

gboolean
mms_handler_delivery_report(
    MMSHandler* handler,            /* Handler instance */
    const char* imsi,               /* Subscriber identity */
    const char* msgid,              /* Message id assigned by operator */
    const char* recipient,          /* Recipient's phone number */
    MMS_DELIVERY_STATUS ds);        /* Delivery status */

gboolean
mms_handler_read_report(
    MMSHandler* handler,            /* Handler instance */
    const char* imsi,               /* Subscriber identity */
    const char* msgid,              /* Message id assigned by operator */
    const char* recipient,          /* Recipient's phone number */
    MMS_READ_STATUS rs);            /* Read status */

gboolean
mms_handler_read_report_send_status(
    MMSHandler* handler,            /* Handler instance */
    const char* id,                 /* Handler record id */
    MMS_READ_REPORT_STATUS status); /* Status */

void
mms_handler_busy_update(
    MMSHandler* handler,            /* Handler instance */
    int change);                    /* Normally +1 or -1 */

gulong
mms_handler_add_done_callback(
    MMSHandler* handler,            /* Handler instance */
    mms_handler_event_fn fn,        /* Callback function */
    void* param);                   /* Callback parameter */

void
mms_handler_remove_callback(
    MMSHandler* handler,            /* Handler instance */
    gulong handler_id);             /* Idenfies the callback to remove */

#define mms_handler_busy(handler) ((handler) && ((handler)->busy > 0))
#define mms_handler_busy_inc(handler) mms_handler_busy_update(handler,1)
#define mms_handler_busy_dec(handler) mms_handler_busy_update(handler,-1)

#endif /* JOLLA_MMS_HANDLER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
