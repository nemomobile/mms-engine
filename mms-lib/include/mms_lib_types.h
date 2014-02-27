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

#ifndef JOLLA_MMS_LIB_TYPES_H
#define JOLLA_MMS_LIB_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <io.h>
#  include <direct.h>
#else
#  include <unistd.h>
#endif

#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>

#ifndef O_BINARY
#  define O_BINARY (0)
#endif

#ifdef __linux__
#  define HAVE_MAGIC
#  define HAVE_REALPATH
#endif

/* Static configuration, chosen at startup and never changing since then */
typedef struct mms_config {
    const char* root_dir;       /* Root directory for storing MMS files */
    const char* user_agent;     /* User agent string */
    int retry_secs;             /* Retry timeout in seconds */
    int idle_secs;              /* Idle timeout */
    gsize size_limit;           /* Maximum size of m-Send.req PDU */
    gboolean keep_temp_files;   /* Keep temporary files around */
    gboolean attic_enabled;     /* Keep unrecognized push message in attic */
    gboolean send_dr;           /* Allow sending delivery reports */
} MMSConfig;

/* Attachment information */
typedef struct mms_attachment_info {
    const char* file_name;      /* Full path name */
    const char* content_type;   /* Content type */
    const char* content_id;     /* Content id */
} MMSAttachmentInfo;

/* Types */
typedef GObject MMSHandler;
typedef GObject MMSConnMan;
typedef struct mms_log_module MMSLogModule;
typedef struct mms_dispatcher MMSDispatcher;
typedef struct mms_connection MMSConnection;
typedef struct mms_message MMSPdu;
typedef struct _mms_message MMSMessage;
typedef struct _mms_attachment MMSAttachment;

/* MMS content type */
#define MMS_CONTENT_TYPE        "application/vnd.wap.mms-message"
#define SMIL_CONTENT_TYPE       "application/smil"

/* MMS read status */
typedef enum mms_read_status {
    MMS_READ_STATUS_READ,       /* Message has been read */
    MMS_READ_STATUS_DELETED     /* Message has been deleted without reading */
} MMSReadStatus;

/* Convenience macros */
#define MMS_CAST(address,type,field) ((type *)( \
                                      (char*)(address) - \
                                      (char*)(&((type *)0)->field)))

#endif /* JOLLA_MMS_LIB_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
