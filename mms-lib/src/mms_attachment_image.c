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

#include "mms_attachment.h"
#include "mms_file_util.h"

#ifdef HAVE_IMAGEMAGICK
#  include <magick/api.h>
#endif

/* Logging */
#define MMS_LOG_MODULE_NAME mms_attachment_log
#include "mms_lib_log.h"

typedef MMSAttachmentClass MMSAttachmentImageClass;
typedef struct mma_attachment_image {
    MMSAttachment attachment;
    int resize_step;
    char* resized;
} MMSAttachmentImage;

G_DEFINE_TYPE(MMSAttachmentImage, mms_attachment_image, MMS_TYPE_ATTACHMENT);

#define MMS_ATTACHMENT_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        MMS_TYPE_ATTACHMENT_IMAGE, MMSAttachmentImage))
#define MMS_ATTACHMENT_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        MMS_TYPE_ATTACHMENT_IMAGE, MMSAttachmentImageClass))

static
gboolean
mms_attachment_image_resize(
    MMSAttachment* at)
{
    gboolean ok = FALSE;

#ifdef HAVE_IMAGEMAGICK
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(at);
    ExceptionInfo ex;
    Image* src;
    ImageInfo* info = CloneImageInfo(NULL);
    GetExceptionInfo(&ex);
    strncpy(info->filename, at->original_file, G_N_ELEMENTS(info->filename));
    info->filename[G_N_ELEMENTS(info->filename)-1] = 0;

    if (image->resized) {
        remove(image->resized);
        image->attachment.file_name = image->attachment.original_file;
    } else {
        char* dir = g_path_get_dirname(at->original_file);
        char* subdir = g_strconcat(dir, "/"  MMS_RESIZE_DIR, NULL);
        g_mkdir_with_parents(subdir, MMS_DIR_PERM);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        image->resized = g_strconcat(subdir, "/",
            g_basename(at->original_file), NULL);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_free(dir);
        g_free(subdir);
    }

    src = ReadImage(info, &ex);
    if (src) {
        if (src->magick_columns > 1 && src->magick_rows > 1) {;
            const guint cols = src->magick_columns/(image->resize_step+2);
            const guint rows = src->magick_rows/(image->resize_step+2);
            Image* dest = ResizeImage(src, cols, rows, BoxFilter, 1.0, &ex);
            if (dest) {
                const char* fname = image->resized;
                image->resize_step++;
                strncpy(info->filename, fname, G_N_ELEMENTS(info->filename));
                strncpy(dest->filename, fname, G_N_ELEMENTS(dest->filename));
                info->filename[G_N_ELEMENTS(info->filename)-1] = 0;
                dest->filename[G_N_ELEMENTS(dest->filename)-1] = 0;
                if (WriteImage(info, dest)) {
                    GError* err = NULL;
                    GMappedFile* map = g_mapped_file_new(fname, FALSE, &err);
                    if (map) {
                        MMS_DEBUG("Resized %s (%ux%u)", fname, cols, rows);
                        image->attachment.file_name = fname;
                        if (at->map) g_mapped_file_unref(at->map);
                        at->map = map;
                        ok = TRUE;
                    } else {
                        MMS_ERR("%s", MMS_ERRMSG(err));
                        g_error_free(err);
                    }
                } else {
                    MMS_ERR("Failed to write %s", dest->filename);
                }
                DestroyImage(dest);
            }
        }
        DestroyImage(src);
    } else {
        MMS_ERR("Failed to read %s", info->filename);
    }
    ClearMagickException(&ex);
    DestroyExceptionInfo(&ex);
    DestroyImageInfo(info);
#endif

    return ok;
}

static
void
mms_attachment_image_reset(
    MMSAttachment* at)
{
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(at);
    at->file_name = at->original_file;
    if (image->resize_step > 0) {
        if (at->map) g_mapped_file_unref(at->map);
        at->map = g_mapped_file_new(at->original_file, FALSE, NULL);
    }
    image->resize_step = 0;
}

static
void
mms_attachment_image_finalize(
    GObject* object)
{
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(object);
    if (!image->attachment.config->keep_temp_files &&
        !(image->attachment.flags & MMS_ATTACHMENT_DONT_DELETE_FILES)) {
        mms_remove_file_and_dir(image->resized);
    }
    g_free(image->resized);
    G_OBJECT_CLASS(mms_attachment_image_parent_class)->finalize(object);
}

static
void
mms_attachment_image_class_init(
    MMSAttachmentImageClass* klass)
{
    klass->fn_reset = mms_attachment_image_reset;
    klass->fn_resize = mms_attachment_image_resize;
    G_OBJECT_CLASS(klass)->finalize = mms_attachment_image_finalize;
}

static
void
mms_attachment_image_init(
    MMSAttachmentImage* image)
{
    image->attachment.flags |= MMS_ATTACHMENT_RESIZABLE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
