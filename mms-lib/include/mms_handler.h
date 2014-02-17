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

#ifndef JOLLA_MMS_HANDLER_H
#define JOLLA_MMS_HANDLER_H

#include "mms_message.h"

/* Receive state */
typedef enum _mmm_receive_state {
    MMS_RECEIVE_STATE_INVALID = -1,
    MMS_RECEIVE_STATE_RECEIVING,
    MMS_RECEIVE_STATE_DEFERRED,
    MMS_RECEIVE_STATE_NOSPACE,
    MMS_RECEIVE_STATE_DECODING,
    MMS_RECEIVE_STATE_DOWNLOAD_ERROR,
    MMS_RECEIVE_STATE_DECODING_ERROR
} MMS_RECEIVE_STATE;

/* Class */
typedef struct mms_handler_class {
    GObjectClass parent;

    /* New incoming message notification. Returns the handler message id
     * to start download immediately, NULL or empty string to postpone it. */
    char* (*fn_message_notify)(
        MMSHandler* handler,        /* Handler instance */
        const char* imsi,           /* Subscriber identity */
        const char* from,           /* Sender's phone number */
        const char* subject,        /* Subject (optional) */
        time_t expiry,              /* Message expiry time */
        GBytes* push);              /* Raw push message */

    /* Sets the receive state */
    gboolean (*fn_message_receive_state_changed)(
        MMSHandler* handler,        /* Handler instance */
        const char* id,             /* Handler record id */
        MMS_RECEIVE_STATE state);   /* Receive state */

    /* Message has been successfully received */
    gboolean (*fn_message_received)(
        MMSHandler* handler,        /* Handler instance */
        MMSMessage* msg);           /* Decoded message  */

} MMSHandlerClass;

GType mms_handler_get_type(void);
#define MMS_TYPE_HANDLER (mms_handler_get_type())

MMSHandler*
mms_handler_ref(
    MMSHandler* handler);

void
mms_handler_unref(
    MMSHandler* handler);

char*
mms_handler_message_notify(
    MMSHandler* handler,            /* Handler instance */
    const char* imsi,               /* Subscriber identity */
    const char* from,               /* Sender's phone number */
    const char* subject,            /* Subject (optional) */
    time_t expiry,                  /* Message expiry time */
    GBytes* push);                  /* Raw push message */

gboolean
mms_handler_message_receive_state_changed(
    MMSHandler* handler,            /* Handler instance */
    const char* id,                 /* Handler record id */
    MMS_RECEIVE_STATE state);       /* Receive state */

gboolean
mms_handler_message_received(
    MMSHandler* handler,            /* Handler instance */
    MMSMessage* msg);               /* Decoded message  */

#endif /* JOLLA_MMS_HANDLER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
