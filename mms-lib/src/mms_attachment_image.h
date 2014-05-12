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

#ifndef JOLLA_MMS_ATTACHMENT_IMAGE_H
#define JOLLA_MMS_ATTACHMENT_IMAGE_H

#include "mms_attachment.h"

typedef struct mms_attachment_image_size {
    unsigned int width;
    unsigned int height;
} MMSAttachmentImageSize;

typedef struct mms_attachment_image_resize {
    MMSAttachmentImageSize image;
    MMSAttachmentImageSize in;
    MMSAttachmentImageSize out;
} MMSAttachmentImageResize;

typedef struct mms_attachment_image {
    MMSAttachment attachment;
    int resize_step;
    char* resized;
} MMSAttachmentImage;

typedef struct mms_attachment_image_class {
    MMSAttachmentClass attachment;

    /* Creates the resize context, sets image size */
    MMSAttachmentImageResize*
    (*fn_resize_new)(
        MMSAttachmentImage* image,
        const char* file);

    /* Prepares the resize context for writing, sets input size */
    gboolean
    (*fn_resize_prepare)(
        MMSAttachmentImageResize* resize,
        const char* file);

    /* Reads the next scanline in RGB24 format */
    gboolean
    (*fn_resize_read_line)(
        MMSAttachmentImageResize* resize,
        unsigned char* rgb24);

    /* Writes the next scanline in RGB24 format */
    gboolean
    (*fn_resize_write_line)(
        MMSAttachmentImageResize* resize,
        const unsigned char* rgb24);

    /* Finishes resizing */
    void (*fn_resize_finish)(
        MMSAttachmentImageResize* resize);

    /* Frees the resize context */
    void (*fn_resize_free)(
        MMSAttachmentImageResize* resize);

} MMSAttachmentImageClass;

int
mms_attachment_image_next_resize_step(
    MMSAttachmentImage* image,
    const MMSSettingsSimData* settings,
    unsigned int columns,
    unsigned int rows);

const char*
mms_attachment_image_prepare_filename(
    MMSAttachmentImage* image);

#ifdef MMS_RESIZE_QT
gboolean
mms_attachment_image_resize_qt(
    MMSAttachmentImage* image,
    const MMSSettingsSimData* settings);
#endif

#endif /* JOLLA_MMS_ATTACHMENT_IMAGE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
