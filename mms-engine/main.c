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
#include "mms_settings.h"

#define RET_OK  (0)
#define RET_ERR (1)

/* Options configurable from the command line */
typedef struct mms_app_options {
    GBusType bus_type;
    int flags;
    char* dir;
    char* user_agent;
    MMSConfig config;
    MMSSettingsSimData settings;
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

/**
 * Parses command line and sets up application options. Returns TRUE if
 * we should go ahead and run the application, FALSE if we should exit
 * immediately.
 */
static
gboolean
mms_app_parse_options(
    MMSAppOptions* opt,
    int argc,
    char* argv[],
    int* result)
{
    gboolean ok;
    GError* error = NULL;
    gboolean session_bus = FALSE;
#ifdef MMS_ENGINE_VERSION
    gboolean print_version = FALSE;
#endif
    gboolean log_modules = FALSE;
    gboolean keep_running = FALSE;
    gint size_limit_kb = -1;
    gdouble megapixels = -1;
    char* root_dir_help = g_strdup_printf(
        "Root directory for MMS files [%s]",
        opt->config.root_dir);
    char* retry_secs_help = g_strdup_printf(
        "Retry period in seconds [%d]",
        opt->config.retry_secs);
    char* idle_secs_help = g_strdup_printf(
        "Inactivity timeout in seconds [%d]",
        opt->config.idle_secs);
    char* description = mms_log_description(NULL, 0);

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
          &size_limit_kb, "Maximum size for outgoing messages", "KB" },
        { "pix-limit", 'p', 0, G_OPTION_ARG_DOUBLE,
          &megapixels, "Maximum pixel count for outgoing images", "MPIX" },
        { "user-agent", 'u', 0, G_OPTION_ARG_STRING,
          &opt->user_agent, "User-Agent header", "STRING" },
        { "keep-running", 'k', 0, G_OPTION_ARG_NONE, &keep_running,
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
        { "log-modules", 0, 0, G_OPTION_ARG_NONE, &log_modules,
          "List available log modules", NULL },
#ifdef MMS_ENGINE_VERSION
        { "version", 0, 0, G_OPTION_ARG_NONE, &print_version,
          "Print program version and exit", NULL },
#endif
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
    g_free(description);

    if (!ok) {
        fprintf(stderr, "%s\n", MMS_ERRMSG(error));
        g_error_free(error);
        *result = RET_ERR;
        return FALSE;
    } else if (log_modules) {
        unsigned int i;
        for (i=0; i<G_N_ELEMENTS(mms_app_log_modules); i++) {
            printf("%s\n", mms_app_log_modules[i]->name);
        }
        *result = RET_OK;
        return FALSE;
#ifdef MMS_ENGINE_VERSION
#  define MMS_STRING__(x) #x
#  define MMS_STRING_(x) MMS_STRING__(x)
#  define MMS_VERVION_STRING MMS_STRING_(MMS_ENGINE_VERSION)
    } else if (print_version) {
        printf("MMS engine %s\n", MMS_VERVION_STRING);
        *result = RET_OK;
        return FALSE;
#endif
    } else {
#ifdef MMS_ENGINE_VERSION
        MMS_INFO("Version %s starting", MMS_VERVION_STRING);
#else
        MMS_INFO("Starting");
#endif
        if (size_limit_kb >= 0) {
            opt->settings.size_limit = size_limit_kb * 1024;
            opt->flags |= MMS_ENGINE_FLAG_OVERRIDE_SIZE_LIMIT;
        }
        if (megapixels >= 0) {
            opt->settings.max_pixels = (int)(megapixels*1000)*1000;
            opt->flags |= MMS_ENGINE_FLAG_OVERRIDE_MAX_PIXELS;
        }
        if (opt->user_agent) {
            opt->settings.user_agent = opt->user_agent;
            opt->flags |= MMS_ENGINE_FLAG_OVERRIDE_USER_AGENT;
        }
        if (opt->dir) opt->config.root_dir = opt->dir;
        if (keep_running) opt->flags |= MMS_ENGINE_FLAG_KEEP_RUNNING;
        if (session_bus) {
            MMS_DEBUG("Attaching to session bus");
            opt->bus_type = G_BUS_TYPE_SESSION;
        } else {
            MMS_DEBUG("Attaching to system bus");
            opt->bus_type = G_BUS_TYPE_SYSTEM;
        }
        *result = RET_OK;
        return TRUE;
    }
}

int main(int argc, char* argv[])
{
    int result = RET_ERR;
    MMSAppOptions opt = {0};
    mms_lib_init(argv[0]);
    mms_log_default.name = MMS_APP_LOG_PREFIX;
    mms_lib_default_config(&opt.config);
    mms_settings_sim_data_default(&opt.settings);
    if (mms_app_parse_options(&opt, argc, argv, &result)) {
        MMSEngine* engine;

        /* Create engine instance. This may fail */
        engine = mms_engine_new(&opt.config, &opt.settings, opt.flags,
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
    }
    if (mms_log_func == mms_log_syslog) {
        closelog();
    }
    g_free(opt.dir);
    g_free(opt.user_agent);
    mms_lib_deinit();
    return result;
}
