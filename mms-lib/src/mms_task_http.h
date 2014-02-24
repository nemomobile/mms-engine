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

#ifndef JOLLA_MMS_TASK_HTTP_H
#define JOLLA_MMS_TASK_HTTP_H

#include "mms_task.h"
#include <libsoup/soup-status.h>

#if !SOUP_CHECK_VERSION(2,43,5)
/* SoupStatus was called SoupKnownStatusCode prior to 2.43.5 */
#  define SoupStatus SoupKnownStatusCode
#endif

/* HTTP task object */
typedef struct mms_task_http_private MMSTaskHttpPrivate;
typedef struct mms_task_http {
    MMSTask task;                   /* Parent object */
    MMSTaskHttpPrivate* priv;       /* Private state */
} MMSTaskHttp;

typedef struct mms_task_http_class {
    MMSTaskClass task;
    void (*fn_started)(MMSTaskHttp* task);
    void (*fn_paused)(MMSTaskHttp* task);
    void (*fn_done)(MMSTaskHttp* task, const char* path, SoupStatus status);
} MMSTaskHttpClass;

GType mms_task_http_get_type(void);
#define MMS_TYPE_TASK_HTTP (mms_task_http_get_type())
#define MMS_TASK_HTTP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        MMS_TYPE_TASK_HTTP, MMSTaskHttpClass))

/* If send_file is not NULL, it does a POST, otherwise it's a GET */
void*
mms_task_http_alloc(
    GType type,                 /* Zero for MMS_TYPE_TASK_HTTP       */
    const MMSConfig* config,    /* Global configuration              */
    MMSHandler* handler,        /* MMS handler                       */
    const char* name,           /* Task name                         */
    const char* id,             /* Database message id               */
    const char* imsi,           /* IMSI associated with the message  */
    const char* uri,            /* NULL to use MMSC URL              */
    const char* receive_file,   /* File to write data to (optional)  */
    const char* send_file);     /* File to read data from (optional) */

#endif /* JOLLA_MMS_TASK_HTTP_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
