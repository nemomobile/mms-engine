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

static
gboolean
test_parse_mms_pdu(
    GBytes* bytes,
    struct mms_message* msg)
{
    gsize len = 0;
    const guint8* data = g_bytes_get_data(bytes, &len);
    return mms_message_decode(data, len, msg);
}

static
gboolean
test_file(
    const char* file,
    gboolean (*parse)(GBytes* bytes, struct mms_message* msg))
{
    GError* error = NULL;
    GMappedFile* map = g_mapped_file_new(file, FALSE, &error);
    if (map) {
        struct mms_message* msg = g_new0(struct mms_message, 1);
        const void* data = g_mapped_file_get_contents(map);
        const gsize length = g_mapped_file_get_length(map);
        GBytes* bytes = g_bytes_new_static(data, length);
        gboolean ok = parse(bytes, msg);
        g_bytes_unref(bytes);
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

static
gboolean
test_files(
    const char* files[],
    int count,
    gboolean (*parse)(GBytes* bytes, struct mms_message* msg))
{
    int i;
    gboolean ok = TRUE;
    for (i=0; i<count; i++) {
        if (!test_file(files[i], parse)) {
            ok = FALSE;
        }
    }
    return ok;
}

int main(int argc, char* argv[])
{
    const char* mms_files[] = {
        "data/m-notification_1.0.ind",
        "data/m-notification_1.1.ind",
        "data/m-notification_1.2.ind",
        "data/m-delivery.ind",
        "data/m-read-orig.ind",
        "data/m-retrieve_1.0.conf",
        "data/m-retrieve_1.1.conf",
        "data/m-retrieve_1.2.conf",
        "data/m-notifyresp.ind",
        "data/m-read-rec.ind"
    };
    mms_log_stdout_timestamp = FALSE;
    mms_log_default.level = MMS_LOGLEVEL_INFO;
    if (test_files(mms_files, G_N_ELEMENTS(mms_files), test_parse_mms_pdu)) {
        return 0;
    } else {
        return 1;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
