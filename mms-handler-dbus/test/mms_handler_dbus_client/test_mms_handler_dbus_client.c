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

#include "mms_handler_dbus.h"

#define RET_ERR (1)
#define RET_OK (0)

static int run_test(MMSHandler* handler)
{
    const static guint8 data [] = {0,1,2,3,4,5};
    static const char IMSI[] = "IMSI";
    static const char Sender[] = "Sender";
    static const char Subject[] = "Subject";

    int ret = RET_ERR;
    GBytes* pdu = g_bytes_new(data, sizeof(data));
    time_t date = time(NULL);
    time_t expiry = date + 10;
    char* id = mms_handler_message_notify(handler, IMSI, Sender, Subject,
        expiry, pdu);
    g_bytes_unref(pdu);
    if (id && id[0]) {
        printf("Record id: %s\n", id); 
        if (mms_handler_message_receive_state_changed(handler, id,
            MMS_RECEIVE_STATE_RECEIVING) &&
            mms_handler_message_receive_state_changed(handler, id,
            MMS_RECEIVE_STATE_DECODING)) {

            gboolean ok;
            MMSMessage* msg = mms_message_new();
            MMSMessagePart* p1 = g_new0(MMSMessagePart, 1);
            MMSMessagePart* p2 = g_new0(MMSMessagePart, 1);
            MMSMessagePart* p3 = g_new0(MMSMessagePart, 1);
            msg->id = g_strdup(id);
            msg->message_id = g_strdup("MessageID");
            msg->from = g_strdup(Sender);
            msg->to = g_strsplit("To1,To2", ",", 0);
            msg->cc = g_strsplit("Cc1,Cc2,Cc3", ",", 0);
            msg->subject = g_strdup(Subject);
            msg->date = date;
            msg->cls = g_strdup("Personal");
            msg->flags |= MMS_MESSAGE_FLAG_KEEP_FILES;

            p1->content_type = g_strdup("application/smil;charset=utf-8");
            p1->content_id = g_strdup("<0>");
            p1->file = g_strdup("0");
            msg->parts = g_slist_append(msg->parts, p1);

            p2->content_type = g_strdup("text/plain;charset=utf-8");
            p2->content_id = g_strdup("<text_0011.txt>");
            p2->file = g_strdup("text_0011.txt");
            msg->parts = g_slist_append(msg->parts, p2);

            p3->content_type = g_strdup("image/jpeg");
            p3->content_id = g_strdup("<131200181.jpg>");
            p3->file = g_strdup("131200181.jpg");
            msg->parts = g_slist_append(msg->parts, p3);

            ok = mms_handler_message_received(handler, msg);
            mms_message_unref(msg);

            if (ok) {
                printf("OK\n");
                ret = RET_OK;
            }
        }
    } else {
        printf("ERROR: no record id\n");
    }
    g_free(id);
    return ret;
}

int main(int argc, char* argv[])
{
    int ret;
    MMSHandler* handler;

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    /* g_type_init has been deprecated since version 2.36
     * the type system is initialised automagically since then */
    g_type_init();
#pragma GCC diagnostic pop

    handler = mms_handler_dbus_new();
    ret = run_test(handler);
    mms_handler_unref(handler);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
