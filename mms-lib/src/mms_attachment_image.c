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

#include "mms_attachment_image.h"
#include "mms_file_util.h"

#ifdef HAVE_IMAGEMAGICK
#  include <magick/api.h>
#endif

/* Logging */
#define MMS_LOG_MODULE_NAME mms_attachment_log
#include "mms_lib_log.h"

G_DEFINE_TYPE(MMSAttachmentImage, mms_attachment_image, MMS_TYPE_ATTACHMENT);
#define MMS_ATTACHMENT_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        MMS_TYPE_ATTACHMENT_IMAGE, MMSAttachmentImage))
#define MMS_ATTACHMENT_IMAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
        MMS_TYPE_ATTACHMENT_IMAGE, MMSAttachmentImageClass))

static
int
mms_attachment_image_next_resize_step(
    MMSAttachmentImage* image,
    unsigned int columns,
    unsigned int rows)
{
    int next_step = image->resize_step + 1;
    if (image->attachment.config->max_pixels > 0) {
        unsigned int size = (columns/(next_step+1))*(rows/(next_step+1));
        while (size > 0 && size > image->attachment.config->max_pixels) {
            next_step++;
            size = (columns/(next_step+1))*(rows/(next_step+1));
        }
    }
    return next_step;
}

static
const char*
mms_attachment_image_prepare_filename(
    MMSAttachmentImage* image)
{
    if (image->resized) {
        remove(image->resized);
        image->attachment.file_name = image->attachment.original_file;
    } else {
        char* dir = g_path_get_dirname(image->attachment.original_file);
        char* subdir = g_strconcat(dir, "/"  MMS_RESIZE_DIR, NULL);
        g_mkdir_with_parents(subdir, MMS_DIR_PERM);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        image->resized = g_strconcat(subdir, "/",
            g_basename(image->attachment.original_file), NULL);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_free(dir);
        g_free(subdir);
    }
    return image->resized;
}

#ifdef HAVE_IMAGEMAGICK
static
gboolean
mms_attachment_image_resize_imagemagick(
    MMSAttachmentImage* image)
{
    gboolean ok = FALSE;
    ExceptionInfo ex;
    Image* src;
    ImageInfo* info = CloneImageInfo(NULL);
    const char* fname = mms_attachment_image_prepare_filename(image);
    GetExceptionInfo(&ex);
    strncpy(info->filename, image->attachment.original_file,
        G_N_ELEMENTS(info->filename));
    info->filename[G_N_ELEMENTS(info->filename)-1] = 0;
    src = ReadImage(info, &ex);
    if (src) {
        if (src->magick_columns > 1 && src->magick_rows > 1) {;
            const int next_step = mms_attachment_image_next_resize_step(image,
                src->magick_columns, src->magick_rows);
            const unsigned int src_cols = src->magick_columns;
            const unsigned int src_rows = src->magick_rows;
            const unsigned int cols = src_cols/(next_step+1);
            const unsigned int rows = src_rows/(next_step+1);
            Image* dest;
            MMS_DEBUG("Resizing (%ux%u -> %ux%u) with ImageMagick",
                src_cols, src_rows, cols, rows);
            dest = ResizeImage(src, cols, rows, BoxFilter, 1.0, &ex);
            if (dest) {
                image->resize_step = next_step;
                strncpy(info->filename, fname, G_N_ELEMENTS(info->filename));
                strncpy(dest->filename, fname, G_N_ELEMENTS(dest->filename));
                info->filename[G_N_ELEMENTS(info->filename)-1] = 0;
                dest->filename[G_N_ELEMENTS(dest->filename)-1] = 0;
                if (WriteImage(info, dest)) {
                    MMS_DEBUG("Resized %s with ImageMagick", fname);
                    ok = TRUE;
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
    return ok;
}
#endif /* HAVE_IMAGEMAGICK */

static
gboolean
mms_attachment_image_resize_type_specific(
    MMSAttachmentImage* image)
{
    /* If klass->fn_resize_new is not NULL, then we assume that all
     * other callbacks are present as well */
    gboolean ok = FALSE;
    MMSAttachment* at = &image->attachment;
    MMSAttachmentImageClass* klass = MMS_ATTACHMENT_IMAGE_GET_CLASS(image);
    MMSAttachmentImageResize* resize;
    if (klass->fn_resize_new && (resize =
        klass->fn_resize_new(image, at->original_file)) != NULL) {
        gboolean can_resize;
        const char* fname = mms_attachment_image_prepare_filename(image);
        const int next_step = mms_attachment_image_next_resize_step(image,
            resize->image.width, resize->image.height);
        MMSAttachmentImageSize image_size;
        MMSAttachmentImageSize out_size;
        image_size = resize->image;
        out_size.width = image_size.width/(next_step+1);
        out_size.height = image_size.height/(next_step+1);

        resize->in = resize->out = out_size;
        can_resize = klass->fn_resize_prepare(resize, fname);
        if (!can_resize) {
            klass->fn_resize_free(resize);
            resize = klass->fn_resize_new(image, at->original_file);
            if (!resize) return FALSE;
            MMS_ASSERT(resize->image.width == image_size.width);
            MMS_ASSERT(resize->image.height == image_size.height);
            resize->in = image_size;
            resize->out = out_size;
            can_resize = klass->fn_resize_prepare(resize, fname);
        }

        if (can_resize) {
            unsigned char* line = g_malloc(3*resize->in.width);
            guint y;
            if (resize->in.width == resize->out.width &&
                resize->in.height == resize->out.height) {
                /* Nothing to resize, image decompressor is doing all
                 * the job for us */
                MMS_DEBUG("Decoder-assisted resize (%ux%u -> %ux%u)",
                    image_size.width, image_size.height,
                    out_size.width, out_size.height);
                for (y=0;
                     y<resize->in.height &&
                     klass->fn_resize_read_line(resize, line) &&
                     klass->fn_resize_write_line(resize, line);
                     y++);
            } else {
                const guint nx = (resize->in.width/resize->out.width);
                const guint ny = (resize->in.height/resize->out.height);
                gsize bufsize = 3*resize->out.width*sizeof(guint);
                guint* buf = g_malloc(bufsize);
                memset(buf, 0, bufsize);
                MMS_DEBUG("Resizing (%ux%u -> %ux%u)",
                    image_size.width, image_size.height,
                    out_size.width, out_size.height);
                for (y=0;
                     y<resize->in.height &&
                     klass->fn_resize_read_line(resize, line);
                     y++) {

                    /* Update the resize buffer */
                    guint x;
                    guint* bufptr = buf;
                    const unsigned char* lineptr = line;
                    for (x=0; x<resize->out.width; x++) {
                        guint k;
                        for (k=0; k<nx; k++) {
                            bufptr[0] += (*lineptr++);
                            bufptr[1] += (*lineptr++);
                            bufptr[2] += (*lineptr++);
                        }
                        bufptr += 3;
                    }

                    if ((y % ny) == (ny-1)) {
                        /* Average the pixels */
                        unsigned char* outptr = line;
                        const guint denominator = nx*ny;
                        bufptr = buf;
                        for (x=0; x<resize->out.width; x++) {
                            (*outptr++) = (*bufptr++)/denominator;
                            (*outptr++) = (*bufptr++)/denominator;
                            (*outptr++) = (*bufptr++)/denominator;
                        }

                        /* And write the next line */
                        if (klass->fn_resize_write_line(resize, line)) {
                            memset(buf, 0, bufsize);
                        } else {
                            break;
                        }
                    }
                }
                g_free(buf);
            }

            if (klass->fn_resize_finish) {
                klass->fn_resize_finish(resize);
            }

            if (y == resize->in.height) {
                MMS_DEBUG("Resized %s", fname);
                image->resize_step = next_step;
                ok = TRUE;
            }

            g_free(line);
        }

        klass->fn_resize_free(resize);
    }

    return ok;
}

static
gboolean
mms_attachment_image_resize(
    MMSAttachment* at)
{
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(at);
    gboolean ok = mms_attachment_image_resize_type_specific(image);
#ifdef HAVE_IMAGEMAGICK
    if (!ok) ok = mms_attachment_image_resize_imagemagick(image);
#endif /* HAVE_IMAGEMAGICK */
    if (ok) {
        GError* error = NULL;
        GMappedFile* map = g_mapped_file_new(image->resized, FALSE, &error);
        if (map) {
            at->file_name = image->resized;
            if (at->map) g_mapped_file_unref(at->map);
            at->map = map;
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
            ok = FALSE;
        }
    }
    return ok;
}

static
void
mms_attachment_image_reset(
    MMSAttachment* at)
{
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(at);
    at->file_name = at->original_file;
    if (image->resize_step) {
        image->resize_step = 0;
        if (at->map) g_mapped_file_unref(at->map);
        at->map = g_mapped_file_new(at->original_file, FALSE, NULL);
    }
}

static
void
mms_attachment_image_finalize(
    GObject* object)
{
    MMSAttachmentImage* image = MMS_ATTACHMENT_IMAGE(object);
    if (!image->attachment.config->keep_temp_files &&
        !(image->attachment.flags & MMS_ATTACHMENT_KEEP_FILES)) {
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
    klass->attachment.fn_reset = mms_attachment_image_reset;
    klass->attachment.fn_resize = mms_attachment_image_resize;
    G_OBJECT_CLASS(klass)->finalize = mms_attachment_image_finalize;
}

static
void
mms_attachment_image_init(
    MMSAttachmentImage* image)
{
#ifdef HAVE_IMAGEMAGICK
    image->attachment.flags |= MMS_ATTACHMENT_RESIZABLE;
#endif
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
