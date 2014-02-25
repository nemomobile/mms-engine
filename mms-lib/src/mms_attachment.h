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

#ifndef JOLLA_MMS_ATTACHMENT_H
#define JOLLA_MMS_ATTACHMENT_H

#include "mms_lib_types.h"

/* Attachment object */
struct _mms_attachment {
    GObject parent;                     /* Parent object */
    const MMSConfig* config;            /* Immutable configuration */
    char* file_name;                    /* Full path name */
    char* content_type;                 /* Content type */
    char* content_id;                   /* Content id */
    char* content_location;             /* Content location */
    GMappedFile* map;                   /* Mapped attachment file */
    unsigned int flags;                 /* Flags: */

#define MMS_ATTACHMENT_SMIL                 (0x01)
#define MMS_ATTACHMENT_DONT_DELETE_FILES    (0x02)

};

typedef struct mms_attachment_class {
    GObjectClass parent;
} MMSAttachmentClass;

GType mms_attachment_get_type(void);
#define MMS_TYPE_ATTACHMENT (mms_attachment_get_type())
#define MMS_ATTACHMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        MMS_TYPE_ATTACHMENT, MMSAttachmentClass))

MMSAttachment*
mms_attachment_new(
    const MMSConfig* config,
    const MMSAttachmentInfo* info,
    GError** error);

MMSAttachment*
mms_attachment_new_smil(
    const MMSConfig* config,
    const char* path,
    MMSAttachment** attachments,
    int count,
    GError** error);

MMSAttachment*
mms_attachment_ref(
    MMSAttachment* attachment);

void
mms_attachment_unref(
    MMSAttachment* attachment);

#endif /* JOLLA_MMS_ATTACHMENT_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
