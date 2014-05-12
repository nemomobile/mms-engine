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

#ifndef JOLLA_MMS_DISPATCHER_H
#define JOLLA_MMS_DISPATCHER_H

#include "mms_lib_types.h"

/* Delegate (one per dispatcher) */
typedef struct mms_dispatcher_delegate MMSDispatcherDelegate;
struct mms_dispatcher_delegate {
    /* Dispatcher deactivated because it has nothing to do */
    void (*fn_done)(
        MMSDispatcherDelegate* delegate,
        MMSDispatcher* dispatcher);
};

MMSDispatcher*
mms_dispatcher_new(
    MMSSettings* settings,
    MMSConnMan* cm,
    MMSHandler* handler);

MMSDispatcher*
mms_dispatcher_ref(
    MMSDispatcher* dispatcher);

void
mms_dispatcher_unref(
    MMSDispatcher* dispatcher);

void
mms_dispatcher_set_delegate(
    MMSDispatcher* dispatcher,
    MMSDispatcherDelegate* delegate);

gboolean
mms_dispatcher_is_active(
    MMSDispatcher* dispatcher);

gboolean
mms_dispatcher_start(
    MMSDispatcher* dispatcher);

gboolean
mms_dispatcher_handle_push(
    MMSDispatcher* dispatcher,
    const char* imsi,
    GBytes* push,
    GError** error);

gboolean
mms_dispatcher_receive_message(
    MMSDispatcher* dispatcher,
    const char* id,
    const char* imsi,
    gboolean automatic,
    GBytes* push,
    GError** error);

gboolean
mms_dispatcher_send_read_report(
    MMSDispatcher* dispatcher,
    const char* id,
    const char* imsi,
    const char* message_id,
    const char* to,
    MMSReadStatus status,
    GError** error);

char*
mms_dispatcher_send_message(
    MMSDispatcher* dispatcher,
    const char* id,
    const char* imsi,
    const char* to,
    const char* cc,
    const char* bcc,
    const char* subject,
    unsigned int flags,
    const MMSAttachmentInfo* parts,
    unsigned int nparts,
    GError** error);

#define MMS_SEND_FLAG_REQUEST_DELIVERY_REPORT   (0x01)
#define MMS_SEND_FLAG_REQUEST_READ_REPORT       (0x02)

void
mms_dispatcher_cancel(
    MMSDispatcher* dispatcher,
    const char* id);

#endif /* JOLLA_MMS_DISPATCHER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
