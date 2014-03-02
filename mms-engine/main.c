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

#include <syslog.h>
#include <glib-unix.h>

#include "mms_engine.h"
#include "mms_ofono_log.h"
#include "mms_lib_log.h"
#include "mms_lib_util.h"
#include "mms_dispatcher.h"

/* Options configurable from the command line */
typedef struct mms_app_options {
    GBusType bus_type;
    gboolean keep_running;
    char* dir;
    MMSConfig config;
} MMSAppOptions;

/* All known log modules */
static MMSLogModule* mms_app_log_modules[] = {
    &mms_log_default,
#define MMS_LIB_LOG_MODULE(m) &(m),
    MMS_LIB_LOG_MODULES(MMS_LIB_LOG_MODULE)
    MMS_OFONO_LOG_MODULES(MMS_LIB_LOG_MODULE)
#undef MMS_LIB_LOG_MODULE
};

/* Signal handler */
static
gboolean
mms_app_signal(
    gpointer arg)
{
    GMainLoop* loop = arg;
    MMS_INFO("Caught signal, shutting down...");
    if (loop) {
        g_idle_add((GSourceFunc)g_main_loop_quit, loop);
    } else {
        exit(0);
    }
    return FALSE;
}

/* D-Bus event handlers */
static
void
mms_app_bus_acquired(
    GDBusConnection* bus,
    const gchar* name,
    gpointer arg)
{
    MMSEngine* engine = arg;
    GError* error = NULL;
    MMS_DEBUG("Bus acquired, starting...");
    if (!mms_engine_register(engine, bus, &error)) {
        MMS_ERR("Could not start: %s", MMS_ERRMSG(error));
        g_error_free(error);
        mms_engine_stop(engine);
    }
}

static
void
mms_app_name_acquired(
    GDBusConnection* bus,
    const gchar* name,
    gpointer arg)
{
    MMS_DEBUG("Acquired service name '%s'", name);
}

static
void
mms_app_name_lost(
    GDBusConnection* bus,
    const gchar* name,
    gpointer arg)
{
    MMSEngine* engine = arg;
    MMS_ERR("'%s' service already running or access denied", name);
    mms_engine_stop(engine);
}

/* Option parsing callbacks */
static
gboolean
mms_app_option_loglevel(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    return mms_log_parse_option(value, mms_app_log_modules,
        G_N_ELEMENTS(mms_app_log_modules), error);
}

static
gboolean
mms_app_option_logtype(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    if (mms_log_set_type(value, MMS_APP_LOG_PREFIX)) {
        return TRUE;
    } else {
        if (error) {
            *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "Invalid log type \'%s\'", value);
        }
        return FALSE;
    }
}

static
gboolean
mms_app_option_verbose(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
    return TRUE;
}

/* Parses command line and sets up application options */
static
gboolean
mms_app_parse_options(
    MMSAppOptions* opt,
    int argc,
    char* argv[])
{
    gboolean ok;
    GError* error = NULL;
    gboolean session_bus = FALSE;
    gint size_limit_kb = opt->config.size_limit/1024;
    gdouble megapixels = opt->config.max_pixels / 1000000.0;
    char* root_dir_help = g_strdup_printf(
        "Root directory for MMS files [%s]",
        opt->config.root_dir);
    char* retry_secs_help = g_strdup_printf(
        "Retry period in seconds [%d]",
        opt->config.retry_secs);
    char* idle_secs_help = g_strdup_printf(
        "Inactivity timeout in seconds [%d]",
        opt->config.idle_secs);
    char* size_limit_help = g_strdup_printf(
        "Maximum size for outgoing messages [%d]",
        size_limit_kb);
    char* megapixels_help = g_strdup_printf(
        "Maximum pixel count for outgoing images [%.1f]",
        megapixels);
    char* description = mms_log_description(mms_app_log_modules,
        G_N_ELEMENTS(mms_app_log_modules));

    GOptionContext* options;
    GOptionEntry entries[] = {
        { "session", 0, 0, G_OPTION_ARG_NONE, &session_bus,
          "Use session bus (default is system)", NULL },
        { "root-dir", 'd', 0, G_OPTION_ARG_FILENAME,
          &opt->dir, root_dir_help, "DIR" },
        { "retry-secs", 'r', 0, G_OPTION_ARG_INT,
          &opt->config.retry_secs, retry_secs_help, "SEC" },
        { "idle-secs", 'i', 0, G_OPTION_ARG_INT,
          &opt->config.idle_secs, idle_secs_help, "SEC" },
        { "size-limit", 's', 0, G_OPTION_ARG_INT,
          &size_limit_kb, size_limit_help, "KB" },
        { "pix-limit", 'p', 0, G_OPTION_ARG_DOUBLE,
          &megapixels, megapixels_help, "MPIX" },
        { "keep-running", 'k', 0, G_OPTION_ARG_NONE, &opt->keep_running,
          "Keep running after everything is done", NULL },
        { "keep-temp-files", 't', 0, G_OPTION_ARG_NONE,
           &opt->config.keep_temp_files,
          "Don't delete temporary files", NULL },
        { "attic", 'a', 0, G_OPTION_ARG_NONE,
          &opt->config.attic_enabled,
          "Store unrecognized push messages in the attic", NULL },
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          mms_app_option_verbose, "Be verbose (equivalent to -l=verbose)",
          NULL },
        { "log-output", 'o', 0, G_OPTION_ARG_CALLBACK, mms_app_option_logtype,
          "Log output (stdout|syslog|glib) [stdout]", "TYPE" },
        { "log-level", 'l', 0, G_OPTION_ARG_CALLBACK, mms_app_option_loglevel,
          "Set log level (repeatable)", "[MODULE:]LEVEL" },
        { NULL }
    };

    options = g_option_context_new("- part of Jolla MMS system");
    g_option_context_add_main_entries(options, entries, NULL);
    g_option_context_set_description(options, description);
    ok = g_option_context_parse(options, &argc, &argv, &error);
    g_option_context_free(options);
    g_free(root_dir_help);
    g_free(retry_secs_help);
    g_free(idle_secs_help);
    g_free(size_limit_help);
    g_free(megapixels_help);
    g_free(description);

    if (ok) {
        MMS_INFO("Starting");
        if (size_limit_kb >= 0) {
            opt->config.size_limit = size_limit_kb * 1024;
        } else {
            opt->config.size_limit = 0;
        }
        if (megapixels >= 0) {
            opt->config.max_pixels = (int)(megapixels*1000)*1000;
        } else {
            opt->config.max_pixels = 0;
        }
        if (opt->dir) opt->config.root_dir = opt->dir;
        if (session_bus) {
            MMS_DEBUG("Attaching to session bus");
            opt->bus_type = G_BUS_TYPE_SESSION;
        } else {
            MMS_DEBUG("Attaching to system bus");
            opt->bus_type = G_BUS_TYPE_SYSTEM;
        }
        return TRUE;
    } else {
        fprintf(stderr, "%s\n", MMS_ERRMSG(error));
        g_error_free(error);
        return FALSE;
    }
}

int main(int argc, char* argv[])
{
    int result = 1;
    MMSAppOptions opt = {0};
    mms_lib_init(argv[0]);
    mms_log_default.name = MMS_APP_LOG_PREFIX;
    mms_lib_default_config(&opt.config);
    if (mms_app_parse_options(&opt, argc, argv)) {
        MMSEngine* engine;
        unsigned int engine_flags = 0;
        if (opt.keep_running) engine_flags |= MMS_ENGINE_FLAG_KEEP_RUNNING;

        /* Create engine instance. This may fail */
        engine = mms_engine_new(&opt.config, engine_flags,
            mms_app_log_modules, G_N_ELEMENTS(mms_app_log_modules));
        if (engine) {
            guint name_id;

            /* Setup main loop */
            GMainLoop* loop = g_main_loop_new(NULL, FALSE);
            g_unix_signal_add(SIGTERM, mms_app_signal, loop);
            g_unix_signal_add(SIGINT, mms_app_signal, loop);

            /* Acquire name, don't allow replacement */
            name_id = g_bus_own_name(opt.bus_type, MMS_ENGINE_SERVICE,
                G_BUS_NAME_OWNER_FLAGS_REPLACE, mms_app_bus_acquired,
                mms_app_name_acquired, mms_app_name_lost, engine, NULL);

            /* Run the main loop */
            mms_engine_run(engine, loop);

            /* Cleanup and exit */
            g_bus_unown_name(name_id);
            g_main_loop_unref(loop);
            mms_engine_unref(engine);
        }
        MMS_INFO("Exiting");
        result = 0;
    }
    if (mms_log_func == mms_log_syslog) {
        closelog();
    }
    g_free(opt.dir);
    mms_lib_deinit();
    return result;
}
