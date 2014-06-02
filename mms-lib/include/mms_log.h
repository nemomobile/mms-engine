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

#ifndef JOLLA_MMS_LOG_H
#define JOLLA_MMS_LOG_H

#include "mms_lib_types.h"
#include <stdarg.h>

/* Log levels */
#define MMS_LOGLEVEL_GLOBAL         (-1)
#define MMS_LOGLEVEL_NONE           (0)
#define MMS_LOGLEVEL_ERR            (1)
#define MMS_LOGLEVEL_WARN           (2)
#define MMS_LOGLEVEL_INFO           (3)
#define MMS_LOGLEVEL_DEBUG          (4)
#define MMS_LOGLEVEL_VERBOSE        (5)

/* Allow these to be redefined */
#ifndef MMS_LOGLEVEL_MAX
#  ifdef DEBUG
#    define MMS_LOGLEVEL_MAX        MMS_LOGLEVEL_VERBOSE
#  else
#    define MMS_LOGLEVEL_MAX        MMS_LOGLEVEL_DEBUG
#  endif
#endif /* MMS_LOGLEVEL_MAX */

#ifndef MMS_LOGLEVEL_DEFAULT
#  ifdef DEBUG
#    define MMS_LOGLEVEL_DEFAULT    MMS_LOGLEVEL_DEBUG
#  else
#    define MMS_LOGLEVEL_DEFAULT    MMS_LOGLEVEL_INFO
#  endif
#endif /* MMS_LOGLEVEL_DEFAULT */

/* Do we need a separate log level for ASSERTs? */
#ifndef MMS_LOGLEVEL_ASSERT
#  ifdef DEBUG
#    define MMS_LOGLEVEL_ASSERT     MMS_LOGLEVEL_ERR
#  else
     /* No asserts in release build */
#    define MMS_LOGLEVEL_ASSERT     (MMS_LOGLEVEL_MAX+1)
#  endif
#endif

/* Log module */
struct mms_log_module {
    const char* name;
    const int max_level;
    int level;
};

/* Command line parsing helper. Option format is [module]:level
 * where level can be either a number or log level name ("none", err etc.) */
gboolean
mms_log_parse_option(
    const char* opt,                /* String to parse */
    MMSLogModule** modules,         /* Known modules */
    int count,                      /* Number of known modules */
    GError** error);                /* Optional error message */

/* Set log type by name ("syslog", "stdout" or "glib"). This is also
 * primarily for parsing command line options */
gboolean
mms_log_set_type(
    const char* type,
    const char* default_name);

const char*
mms_log_get_type(
    void);

/* Generates the string containg description of log levels and list of
 * log modules. The caller must deallocate the string with g_free */
char*
mms_log_description(
    MMSLogModule** modules,         /* Known modules */
    int count);                     /* Number of known modules */

/* Logging function */
void
mms_log(
    const MMSLogModule* module,     /* Calling module (NULL for default) */
    int level,                      /* Message log level */
    const char* format,             /* Message format */
    ...) G_GNUC_PRINTF(3,4);        /* Followed by arguments */

void
mms_logv(
    const MMSLogModule* module,
    int level,
    const char* format,
    va_list va);

#ifndef MMS_LOG_SYSLOG
#  ifdef unix
#    define MMS_LOG_SYSLOG 1
#  else
#    define MMS_LOG_SYSLOG 0
#  endif
#endif /* MMS_LOG_SYSLOG */

extern const char MMS_LOG_TYPE_STDOUT[];
extern const char MMS_LOG_TYPE_GLIB[];
extern const char MMS_LOG_TYPE_CUSTOM[];
#if MMS_LOG_SYSLOG
extern const char MMS_LOG_TYPE_SYSLOG[];
#endif

/* Available log handlers */
#define MMS_DEFINE_LOG_FN(fn)  void fn(const char* name, int level, \
    const char* format, va_list va)
MMS_DEFINE_LOG_FN(mms_log_stdout);
MMS_DEFINE_LOG_FN(mms_log_glib);
#if MMS_LOG_SYSLOG
MMS_DEFINE_LOG_FN(mms_log_syslog);
#endif

/* Log configuration */
#define MMS_LOG_MODULE_DECL(m) extern MMSLogModule m;
MMS_LOG_MODULE_DECL(mms_log_default)
typedef MMS_DEFINE_LOG_FN((*MMSLogFunc));
extern MMSLogFunc mms_log_func;
extern gboolean mms_log_stdout_timestamp;

/* Log module (optional) */
#define MMS_LOG_MODULE_DEFINE_(mod,name) \
  MMSLogModule mod = {name, \
  MMS_LOGLEVEL_MAX,  MMS_LOGLEVEL_GLOBAL}
#ifdef MMS_LOG_MODULE_NAME
extern MMSLogModule MMS_LOG_MODULE_NAME;
#  define MMS_LOG_MODULE_CURRENT    (&MMS_LOG_MODULE_NAME)
#  define MMS_LOG_MODULE_DEFINE(name) \
    MMS_LOG_MODULE_DEFINE_(MMS_LOG_MODULE_NAME,name)
#else
#  define MMS_LOG_MODULE_CURRENT    NULL
#endif

/* Logging macros */

#define MMS_LOG_NOTHING ((void)0)
#define MMS_ERRMSG(err) (((err) && (err)->message) ? (err)->message : \
  "Unknown error")

#if !defined(MMS_LOG_VARARGS) && defined(__GNUC__)
#  define MMS_LOG_VARARGS
#endif

#ifndef MMS_LOG_VARARGS
#  define MMS_LOG_VA_NONE(x) static inline void MMS_##x(const char* f, ...) {}
#  define MMS_LOG_VA(x) static inline void MMS_##x(const char* f, ...) {    \
    if (f && f[0]) {                                                        \
        va_list va; va_start(va,f);                                         \
        mms_logv(MMS_LOG_MODULE_CURRENT, MMS_LOGLEVEL_##x, f, va);          \
        va_end(va);                                                         \
    }                                                                       \
}
#endif /* MMS_LOG_VARARGS */

#define MMS_LOG_ENABLED             (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_NONE)
#define MMS_LOG_ERR                 (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_ERR)
#define MMS_LOG_WARN                (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_WARN)
#define MMS_LOG_INFO                (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_INFO)
#define MMS_LOG_DEBUG               (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_DEBUG)
#define MMS_LOG_VERBOSE             (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_VERBOSE)
#define MMS_LOG_ASSERT              (MMS_LOGLEVEL_MAX >= MMS_LOGLEVEL_ASSERT)

#if MMS_LOG_ASSERT
void
mms_log_assert(
    const MMSLogModule* module,     /* Calling module (NULL for default) */
    const char* expr,               /* Assert expression */
    const char* file,               /* File name */
    int line);                      /* Line number */
#  define MMS_ASSERT(expr)          ((expr) ? MMS_LOG_NOTHING : \
    mms_log_assert(MMS_LOG_MODULE_CURRENT, #expr, __FILE__, __LINE__))
#  define MMS_VERIFY(expr)          MMS_ASSERT(expr)
#else
#  define MMS_ASSERT(expr)
#  define MMS_VERIFY(expr)          (expr)
#endif

#ifdef MMS_LOG_VARARGS
#  if MMS_LOG_ERR
#    define MMS_ERR(f,args...)      mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_ERR, f, ##args)
#    define MMS_ERR_(f,args...)     mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_ERR, "%s() " f, __FUNCTION__, ##args)
#  else
#    define MMS_ERR(f,args...)      MMS_LOG_NOTHING
#    define MMS_ERR_(f,args...)     MMS_LOG_NOTHING
#  endif /* MMS_LOG_ERR */
#else
#  define MMS_ERR_                  MMS_ERR
#  if MMS_LOG_ERR
     MMS_LOG_VA(ERR)
#  else
     MMS_LOG_VA_NONE(ERR)
#  endif /* MMS_LOG_ERR */
#endif /* MMS_LOG_VARARGS */

#ifdef MMS_LOG_VARARGS
#  if MMS_LOG_WARN
#    define MMS_WARN(f,args...)     mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_WARN, f, ##args)
#    define MMS_WARN_(f,args...)    mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_WARN, "%s() " f, __FUNCTION__, ##args)
#  else
#    define MMS_WARN(f,args...)     MMS_LOG_NOTHING
#    define MMS_WARN_(f,args...)    MMS_LOG_NOTHING
#  endif /* MMS_LOGL_WARN */
#else
#  define MMS_WARN_                 MMS_WARN
#  if MMS_LOG_WARN
     MMS_LOG_VA(WARN)
#  else
     MMS_LOG_VA_NONE(WARN)
#  endif /* MMS_LOGL_WARN */
#  endif /* MMS_LOG_VARARGS */

#ifdef MMS_LOG_VARARGS
#  if MMS_LOG_INFO
#    define MMS_INFO(f,args...)     mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_INFO, f, ##args)
#    define MMS_INFO_(f,args...)    mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_INFO, "%s() " f, __FUNCTION__, ##args)
#  else
#    define MMS_INFO(f,args...)     MMS_LOG_NOTHING
#    define MMS_INFO_(f,args...)    MMS_LOG_NOTHING
#  endif /* MMS_LOG_INFO */
#else
#  define MMS_INFO_                 MMS_INFO
#  if MMS_LOG_INFO
     MMS_LOG_VA(INFO)
#  else
     MMS_LOG_VA_NONE(INFO)
#  endif /* MMS_LOG_INFO */
#endif /* MMS_LOG_VARARGS */

#ifdef MMS_LOG_VARARGS
#  if MMS_LOG_DEBUG
#    define MMS_DEBUG(f,args...)    mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_DEBUG, f, ##args)
#    define MMS_DEBUG_(f,args...)   mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_DEBUG, "%s() " f, __FUNCTION__, ##args)
#  else
#    define MMS_DEBUG(f,args...)    MMS_LOG_NOTHING
#    define MMS_DEBUG_(f,args...)   MMS_LOG_NOTHING
#  endif /* MMS_LOG_DEBUG */
#else
#  define MMS_DEBUG_                MMS_DEBUG
#  if MMS_LOG_DEBUG
     MMS_LOG_VA(DEBUG)
#  else
     MMS_LOG_VA_NONE(DEBUG)
#  endif /* MMS_LOG_DEBUG */
#endif /* MMS_LOG_VARARGS */

#ifdef MMS_LOG_VARARGS
#  if MMS_LOG_VERBOSE
#    define MMS_VERBOSE(f,args...)  mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_VERBOSE, f, ##args)
#    define MMS_VERBOSE_(f,args...) mms_log(MMS_LOG_MODULE_CURRENT, \
       MMS_LOGLEVEL_VERBOSE, "%s() " f, __FUNCTION__, ##args)
#  else
#    define MMS_VERBOSE(f,args...)  MMS_LOG_NOTHING
#    define MMS_VERBOSE_(f,args...) MMS_LOG_NOTHING
#  endif /* MMS_LOG_VERBOSE */
#else
#  define MMS_VERBOSE_              MMS_VERBOSE
#  if MMS_LOG_VERBOSE
     MMS_LOG_VA(VERBOSE)
#  else
     MMS_LOG_VA_NONE(VERBOSE)
#  endif /* MMS_LOG_VERBOSE */
#endif /* MMS_LOG_VARARGS */

#endif /* JOLLA_MMS_LOG_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
