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

#include "mms_log.h"
#include "mms_codec.h"

#define DATA_DIR "data/"

#define RET_OK   (0)
#define RET_ERR  (1)

static
gboolean
test_file(
    const char* file)
{
    GError* error = NULL;
    char* path = g_strconcat(DATA_DIR, file, NULL);
    GMappedFile* map = g_mapped_file_new(path, FALSE, &error);
    g_free(path);
    if (map) {
        struct mms_message* msg = g_new0(struct mms_message, 1);
        const void* data = g_mapped_file_get_contents(map);
        const gsize length = g_mapped_file_get_length(map);
        gboolean ok = mms_message_decode(data, length, msg);
        g_mapped_file_unref(map);
        mms_message_free(msg);
        if (ok) {
            MMS_INFO("OK: %s", file);
            return TRUE;
        }
        MMS_ERR("Failed to decode %s", file);
    } else {
        MMS_ERR("%s", error->message);
        g_error_free(error);
    }
    return FALSE;
}

int main(int argc, char* argv[])
{
    int i, ret = RET_OK;
    static const char* default_files[] = {
        "m-acknowledge.ind",
        "m-notification_1.ind",
        "m-notification_2.ind",
        "m-notification_3.ind",
        "m-delivery.ind",
        "m-read-orig.ind",
        "m-retrieve_1.conf",
        "m-retrieve_2.conf",
        "m-retrieve_3.conf",
        "m-retrieve_4.conf",
        "m-retrieve_5.conf",
        "m-retrieve_6.conf",
        "m-retrieve_7.conf",
        "m-retrieve_8.conf",
        "m-notifyresp.ind",
        "m-read-rec.ind",
        "m-send_1.req",
        "m-send_2.req",
        "m-send_3.req",
        "m-send.conf"
    };
    mms_log_set_type(MMS_LOG_TYPE_STDOUT, "test_mms_codec");
    mms_log_stdout_timestamp = FALSE;
    mms_log_default.level = MMS_LOGLEVEL_INFO;
    if (argc > 1) {
        for (i=1; i<argc; i++) {
            if (!test_file(argv[i])) {
                ret = RET_ERR;
            }
        }
    } else {
        /* Default set of test files */
        for (i=0; i<G_N_ELEMENTS(default_files); i++) {
            if (!test_file(default_files[i])) {
                ret = RET_ERR;
            }
        }
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
