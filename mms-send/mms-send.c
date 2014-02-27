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

#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generated headers */
#include "org.nemomobile.MmsEngine.h"

static const char pname[] = "mms-send";

enum app_ret_value {
    RET_OK,
    RET_ERR_CMDLINE,
    RET_ERR_SEND
};

#define MMS_SEND_REQUEST_DELIVERY_REPORT (0x01)
#define MMS_SEND_REQUEST_READ_REPORT     (0x02)
#define MMS_SEND_FLAG_VERBOSE            (0x04)

/* Protocol flags */
#define MMS_SEND_REQUEST_PROTOCOL_FLAGS  (\
        MMS_SEND_REQUEST_DELIVERY_REPORT |\
        MMS_SEND_REQUEST_READ_REPORT     )

static
char*
mms_send(
    char* files[],
    int count,
    const char* to,
    const char* subject,
    int flags)
{
    char* imsi = NULL;
    GError* error = NULL;
    OrgNemomobileMmsEngine* proxy =
        org_nemomobile_mms_engine_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
            "org.nemomobile.MmsEngine", "/", NULL, &error);
    if (proxy) {
        int i;
        gboolean ok = TRUE;
        char* fname = g_malloc(PATH_MAX);
        GVariant* parts;
        GVariantBuilder b;

        g_variant_builder_init(&b, G_VARIANT_TYPE("a(sss)"));
        for (i=0; ok && i<count; i++) {
            if (realpath(files[i], fname)) {
                if (g_file_test(fname, G_FILE_TEST_IS_REGULAR)) {
                    g_variant_builder_add(&b, "(sss)", fname, "", "");
                } else {
                    fprintf(stderr, "%s: no such file: %s\n", pname, fname);
                    ok = FALSE;
                }
            } else {
                fprintf(stderr, "%s: %s\n", fname, strerror(errno));
                ok = FALSE;
            }
        }

        parts = g_variant_ref_sink(g_variant_builder_end(&b));
        if (ok) {
            char** to_list = g_strsplit(to, ",", 0);
            const char* none = NULL;
            if (!org_nemomobile_mms_engine_call_send_message_sync(proxy,
                0, "", (const gchar* const*)to_list, &none, &none, subject,
                flags & MMS_SEND_REQUEST_PROTOCOL_FLAGS,
                parts, &imsi, NULL, &error)) {
                /* D-Bus error */
                fprintf(stderr, "%s\n", error->message);
                g_error_free(error);
            }
            g_free(to_list);
        }

        g_free(fname);
        g_variant_unref(parts);
        g_object_unref(proxy);
    } else if (error) {
        fprintf(stderr, "%s\n", error->message);
        g_error_free(error);
    }
    return imsi;
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR_CMDLINE;
    gboolean ok, verbose = FALSE, dr = FALSE, rr = FALSE;
    GError* error = NULL;
    char* subject = NULL;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "subject", 's', 0, G_OPTION_ARG_STRING, &subject,
          "Set message subject", "TEXT" },
        { "delivery-report", 'd', 0, G_OPTION_ARG_NONE, &dr,
          "Request delivery report", NULL },
        { "read-report", 'r', 0, G_OPTION_ARG_NONE, &rr,
          "Request read report", NULL },
        { NULL }
    };
    GOptionContext* options = g_option_context_new("TO FILES...");
    g_option_context_add_main_entries(options, entries, NULL);
    ok = g_option_context_parse(options, &argc, &argv, &error);
    if (ok) {
        if (argc > 2) {
            int flags = 0;
            char* imsi;
            if (verbose) flags |= MMS_SEND_FLAG_VERBOSE;
            if (dr) flags |= MMS_SEND_REQUEST_DELIVERY_REPORT;
            if (rr) flags |= MMS_SEND_REQUEST_READ_REPORT;
            imsi = mms_send(argv+2, argc-2, argv[1], subject, flags);
            if (imsi) {
                if (verbose) printf("%s\n", imsi);
                g_free(imsi);
                ret = RET_OK;
            }
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            fprintf(stderr, "%s", help);
            g_free(help);
        }
    } else {
        printf("%s: %s\n", pname, error->message);
        g_error_free(error);
    }
    g_option_context_free(options);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
