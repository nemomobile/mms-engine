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

#ifndef JOLLA_MMS_MESSAGE_H
#define JOLLA_MMS_MESSAGE_H

#include "mms_lib_types.h"

typedef enum mms_message_prority {
    MMS_PRIORITY_LOW,
    MMS_PRIORITY_NORMAL,
    MMS_PRIORITY_HIGH
} MMS_PRIORITY;

struct _mms_message {
    gint ref_count;                         /* Reference count */
    char* id;                               /* Database record ID */
    char* message_id;                       /* Message-ID */
    char* from;                             /* Sender */
    char** to;                              /* To: list */
    char** cc;                              /* Cc: list */
    char* subject;                          /* Subject */
    time_t date;                            /* Original send date */
    MMS_PRIORITY priority;                  /* Message priority */
    char* cls;                              /* Message class */
    gboolean read_report_req;               /* Request for read report */
    char* msg_dir;                          /* Delete when done if empty */
    char* parts_dir;                        /* Where parts are stored */
    GSList* parts;                          /* Message parts */
    int flags;                              /* Message flags: */

#define MMS_MESSAGE_FLAG_KEEP_FILES (0x01)  /* Don't delete files */

};

typedef struct _mms_message_part {
    char* content_type;                     /* Content-Type */
    char* content_id;                       /* Content-ID */
    char* file;                             /* File name */
    char* orig;                             /* File prior to decoding */
} MMSMessagePart;

MMSMessage*
mms_message_new(
    void);

MMSMessage*
mms_message_ref(
    MMSMessage* msg);

void
mms_message_unref(
    MMSMessage* msg);

#endif /* JOLLA_MMS_MESSAGE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
