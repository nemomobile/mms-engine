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
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#  ifdef DEBUG
#    include <windows.h>
#  endif
#  define vsnprintf     _vsnprintf
#  define snprintf      _snprintf
#  define strcasecmp    _stricmp
#  define strncasecmp   _strnicmp
#endif

/* Allows timestamps in stdout log */
gboolean mms_log_stdout_timestamp = TRUE;

/* Log configuration */
MMSLogFunc mms_log_func = mms_log_stdout;
MMSLogModule mms_log_default = {
    NULL,                   /* name      */
    MMS_LOGLEVEL_MAX,       /* max_level */
    MMS_LOGLEVEL_DEFAULT    /* level     */
};

/* Log level descriptions */
static const struct _mms_log_level {
    const char* name;
    const char* description;
} mms_log_levels [] = {
    { "none",    "Disable log output" },
    { "error",   "Errors only"},
    { "warning", "From warning level to errors" },
    { "info",    "From information level to errors" },
    { "debug",   "From debug messages to errors" },
    { "verbose", "From verbose trace messages to errors" }
};

const char MMS_LOG_TYPE_STDOUT[] = "stdout";
const char MMS_LOG_TYPE_GLIB[]   = "glib";
const char MMS_LOG_TYPE_CUSTOM[] = "custom";
#if MMS_LOG_SYSLOG
const char MMS_LOG_TYPE_SYSLOG[] = "syslog";
#endif

G_STATIC_ASSERT(G_N_ELEMENTS(mms_log_levels) > MMS_LOGLEVEL_MAX);
G_STATIC_ASSERT(G_N_ELEMENTS(mms_log_levels) > MMS_LOGLEVEL_DEFAULT);

/* Forwards output to stdout */
void
mms_log_stdout(
    const char* name,
    int level,
    const char* format,
    va_list va)
{
    char t[32];
    char buf[512];
    const char* prefix = "";
    if (mms_log_stdout_timestamp) {
        time_t now;
        time(&now);
        strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S ", localtime(&now));
    } else {
        t[0] = 0;
    }
    switch (level) {
    case MMS_LOGLEVEL_WARN: prefix = "WARNING: "; break;
    case MMS_LOGLEVEL_ERR:  prefix = "ERROR: ";   break;
    default:                break;
    }
    vsnprintf(buf, sizeof(buf), format, va);
    buf[sizeof(buf)-1] = 0;
#if defined(DEBUG) && defined(_WIN32)
    {
        char s[1023];
        if (name) {
            snprintf(s, sizeof(s), "%s[%s] %s%s\n", t, name, prefix, buf);
        } else {
            snprintf(s, sizeof(s), "%s%s%s\n", t, prefix, buf);
        }
        OutputDebugString(s);
    }
#endif
    if (name) {
        printf("%s[%s] %s%s\n", t, name, prefix, buf);
    } else {
        printf("%s%s%s\n", t, prefix, buf);
    }
}

/* Formards output to syslog */
#if MMS_LOG_SYSLOG
#include <syslog.h>
void
mms_log_syslog(
    const char* name,
    int level,
    const char* format,
    va_list va)
{
    int priority;
    const char* prefix = NULL;
    switch (level) {
    default:
    case MMS_LOGLEVEL_VERBOSE:
        priority = LOG_DEBUG;
        break;
    case MMS_LOGLEVEL_DEBUG:
        priority = LOG_INFO;
        break;
    case MMS_LOGLEVEL_INFO:
        priority = LOG_NOTICE;
        break;
    case MMS_LOGLEVEL_WARN:
        priority = LOG_WARNING;
        prefix = "WARNING! ";
        break;
    case MMS_LOGLEVEL_ERR:
        priority = LOG_ERR;
        prefix = "ERROR! ";
        break;
    }
    if (name || prefix) {
        char buf[512];
        vsnprintf(buf, sizeof(buf), format, va);
        if (!prefix) prefix = "";
        if (name) {
            syslog(priority, "[%s] %s%s", name, prefix, buf);
        } else {
            syslog(priority, "%s%s", prefix, buf);
        }
    } else {
        vsyslog(priority, format, va);
    }
}
#endif /* MMS_LOG_SYSLOG */

/* Forwards output to g_logv */
void
mms_log_glib(
    const char* name,
    int level,
    const char* format,
    va_list va)
{
    GLogLevelFlags flags;
    switch (level) {
    default:
    case MMS_LOGLEVEL_VERBOSE: flags = G_LOG_LEVEL_DEBUG;    break;
    case MMS_LOGLEVEL_DEBUG:   flags = G_LOG_LEVEL_INFO;     break;
    case MMS_LOGLEVEL_INFO:    flags = G_LOG_LEVEL_MESSAGE;  break;
    case MMS_LOGLEVEL_WARN:    flags = G_LOG_LEVEL_WARNING;  break;
    case MMS_LOGLEVEL_ERR:     flags = G_LOG_LEVEL_CRITICAL; break;
    }
    g_logv(name, flags, format, va);
}

/* Logging function */
void
mms_logv(
    const MMSLogModule* module,
    int level,
    const char* format,
    va_list va)
{
    if (level != MMS_LOGLEVEL_NONE) {
        MMSLogFunc log = mms_log_func;
        if (log) {
            int max_level;
            if (module) {
                max_level = (module->level < 0) ?
                    mms_log_default.level :
                    module->level;
            } else {
                module = &mms_log_default;
                max_level = mms_log_default.level;
            }
            if (level <= max_level) {
                log(module->name, level, format, va);
            }
        }
    }
}

void
mms_log(
    const MMSLogModule* module,
    int level,
    const char* format,
    ...)
{
    va_list va;
    va_start(va, format);
    mms_logv(module, level, format, va);
    va_end(va);
}

void
mms_log_assert(
    const MMSLogModule* module,
    const char* expr,
    const char* file,
    int line)
{
    mms_log(module, MMS_LOGLEVEL_ASSERT, "Assert %s failed at %s:%d\r",
        expr, file, line);
}

/* mms_log_parse_option helper */
static
int
mms_log_parse_level(
    const char* str,
    GError** error)
{
    if (str && str[0]) {
        guint i;
        const size_t len = strlen(str);
        if (len == 1) {
            const char* valid_numbers = "012345";
            const char* number = strchr(valid_numbers, str[0]);
            if (number) {
                return number - valid_numbers;
            }
        }

        for (i=0; i<G_N_ELEMENTS(mms_log_levels); i++) {
            if (!strncmp(mms_log_levels[i].name, str, len)) {
                return i;
            }
        }
    }

    if (error) {
        *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            "Invalid log level '%s'", str);
    }

    return -1;
}

/**
 * Command line parsing helper. Option format is [module:]level where level
 * can be either a number or log level name ("none", "error", etc.)
 */
gboolean
mms_log_parse_option(
    const char* opt,
    MMSLogModule** modules,
    int count,
    GError** error)
{
    const char* sep = strchr(opt, ':');
    if (sep) {
        const int modlevel = mms_log_parse_level(sep+1, error);
        if (modlevel >= 0) {
            int i;
            const size_t namelen = sep - opt;
            for (i=0; i<count; i++) {
                if (!strncasecmp(modules[i]->name, opt, namelen)) {
                    MMS_ASSERT(modules[i]->max_level >= modlevel);
                    modules[i]->level = modlevel;
                    return TRUE;
                }
            }
            if (error) {
                *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                    "Unknown log module '%.*s'", (int)namelen, opt);
            }
        }
    } else {
        const int deflevel = mms_log_parse_level(opt, error);
        if (deflevel >= 0) {
            mms_log_default.level = deflevel;
            return TRUE;
        }
    }
    return FALSE;
}

/* Generates the string containg description of log levels and list of
 * log modules. The caller must deallocate the string with g_free */
char*
mms_log_description(
    MMSLogModule** modules,         /* Known modules */
    int count)                      /* Number of known modules */
{
    int i;
    GString* desc = g_string_sized_new(128);
    g_string_append(desc, "Log Levels:\n");
    for (i=0; i<=MMS_LOGLEVEL_MAX; i++) {
        g_string_append_printf(desc, "   %d, ", i);
        g_string_append_printf(desc, "%-8s    ", mms_log_levels[i].name);
        g_string_append(desc, mms_log_levels[i].description);
        if (i == MMS_LOGLEVEL_DEFAULT) g_string_append(desc, " (default)");
        g_string_append(desc, "\n");
    }
    if (modules) {
        g_string_append(desc, "\nLog Modules:\n");
        for (i=0; i<count; i++) {
            g_string_append_printf(desc, "  %s\n", modules[i]->name);
        }
    }
    return g_string_free(desc, FALSE);
}

gboolean
mms_log_set_type(
    const char* type,
    const char* default_name)
{
#if MMS_LOG_SYSLOG
    if (!strcasecmp(type, MMS_LOG_TYPE_SYSLOG)) {
        if (mms_log_func != mms_log_syslog) {
            openlog(NULL, LOG_PID | LOG_CONS, LOG_USER);
        }
        mms_log_default.name = NULL;
        mms_log_func = mms_log_syslog;
        return TRUE;
    }
    if (mms_log_func == mms_log_syslog) {
        closelog();
    }
#endif /* MMS_LOG_SYSLOG */
    mms_log_default.name = default_name;
    if (!strcasecmp(type, MMS_LOG_TYPE_STDOUT)) {
        mms_log_func = mms_log_stdout;
        return TRUE;
    } else if (!strcasecmp(type, MMS_LOG_TYPE_GLIB)) {
        mms_log_func = mms_log_glib;
        return TRUE;
    }
    return FALSE;
}

const char*
mms_log_get_type()
{
    return (mms_log_func == mms_log_stdout) ? MMS_LOG_TYPE_STDOUT :
#if MMS_LOG_SYSLOG
           (mms_log_func == mms_log_syslog) ? MMS_LOG_TYPE_SYSLOG :
#endif /* MMS_LOG_SYSLOG */
           (mms_log_func == mms_log_glib)   ? MMS_LOG_TYPE_STDOUT :
                                              MMS_LOG_TYPE_CUSTOM;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
