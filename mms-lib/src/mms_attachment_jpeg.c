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

#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

/* Logging */
#define MMS_LOG_MODULE_NAME mms_attachment_log
#include "mms_lib_log.h"

typedef MMSAttachmentImageClass MMSAttachmentJpegClass;
typedef MMSAttachmentImage MMSAttachmentJpeg;

G_DEFINE_TYPE(MMSAttachmentJpeg, mms_attachment_jpeg, \
        MMS_TYPE_ATTACHMENT_IMAGE);
#define MMS_ATTACHMENT_JPEG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        MMS_TYPE_ATTACHMENT_JPEG, MMSAttachmentJpeg))

typedef struct mms_attachment_jpeg_error {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buf;
} MMSAttachmentJpegError;

typedef struct mms_attachment_jpeg_resize {
    MMSAttachmentImageResize pub;
    MMSAttachmentJpegError err;
    struct jpeg_decompress_struct decomp;
    struct jpeg_compress_struct comp;
    FILE* in;
    FILE* out;
} MMSAttachmentJpegResize;

static inline MMSAttachmentJpegResize*
mms_attachment_jpeg_resize_cast(MMSAttachmentImageResize* resize)
    { return MMS_CAST(resize, MMSAttachmentJpegResize, pub); }

static
void
mms_attachment_jpeg_error_log(
    int level,
    j_common_ptr cinfo)
{
    char* buf = g_malloc(JMSG_LENGTH_MAX);
    buf[0] = 0;
    cinfo->err->format_message(cinfo, buf);
    buf[JMSG_LENGTH_MAX-1] = 0;
    mms_log(MMS_LOG_MODULE_CURRENT, level, "%s", buf);
    g_free(buf);
}

static
void
mms_attachment_jpeg_error_exit(
    j_common_ptr cinfo)
{
    MMSAttachmentJpegError* err = (MMSAttachmentJpegError*)cinfo->err;
    mms_attachment_jpeg_error_log(MMS_LOGLEVEL_WARN, cinfo);
    longjmp(err->setjmp_buf, 1);
}

static
void
mms_attachment_jpeg_error_output(
    j_common_ptr cinfo)
{
    mms_attachment_jpeg_error_log(MMS_LOGLEVEL_DEBUG, cinfo);
}

static
MMSAttachmentImageResize*
mms_attachment_jpeg_resize_new(
    MMSAttachmentImage* image,
    const char* file)
{
    MMSAttachmentJpegResize* jpeg = g_new0(MMSAttachmentJpegResize, 1);
    jpeg->in = fopen(file, "rb");
    if (jpeg->in) {
        jpeg->decomp.err = jpeg_std_error(&jpeg->err.pub);
        jpeg->err.pub.error_exit = mms_attachment_jpeg_error_exit;
        jpeg->err.pub.output_message = mms_attachment_jpeg_error_output;
        if (!setjmp(jpeg->err.setjmp_buf)) {
            int i;
            jpeg_create_decompress(&jpeg->decomp);
            jpeg_save_markers(&jpeg->decomp, JPEG_COM, 0xFFFF);
            for (i=0; i<16; i++) {
                jpeg_save_markers(&jpeg->decomp, JPEG_APP0+i, 0xFFFF);
            }
            jpeg_stdio_src(&jpeg->decomp, jpeg->in);
            jpeg_read_header(&jpeg->decomp, TRUE);
            jpeg->pub.image.width = jpeg->decomp.image_width;
            jpeg->pub.image.height = jpeg->decomp.image_height;
            jpeg->pub.in = jpeg->pub.image;
            return &jpeg->pub;
        }
        jpeg_destroy_decompress(&jpeg->decomp);
        fclose(jpeg->in);
    }
    g_free(jpeg);
    return NULL;
}

static
gboolean
mms_attachment_jpeg_resize_prepare(
    MMSAttachmentImageResize* resize,
    const char* file)
{
    MMSAttachmentJpegResize* jpeg = mms_attachment_jpeg_resize_cast(resize);
    jpeg->out = fopen(file, "wb");
    if (jpeg->out) {
        jpeg->comp.err = &jpeg->err.pub;
        if (!setjmp(jpeg->err.setjmp_buf)) {
            jpeg_saved_marker_ptr marker;
            jpeg_create_compress(&jpeg->comp);

            jpeg->decomp.scale_num = resize->in.width;
            jpeg->decomp.scale_denom = resize->image.width;
            jpeg->decomp.out_color_space = JCS_RGB;
            jpeg_start_decompress(&jpeg->decomp);

            if (jpeg->decomp.output_width == resize->in.width &&
                jpeg->decomp.output_height == resize->in.height) {

                jpeg->comp.image_width = resize->out.width;
                jpeg->comp.image_height = resize->out.height;
                jpeg->comp.input_components = 3;
                jpeg->comp.in_color_space = JCS_RGB;

                jpeg_stdio_dest(&jpeg->comp, jpeg->out);
                jpeg_set_defaults(&jpeg->comp);
                jpeg_set_quality(&jpeg->comp, 90, TRUE);

                jpeg->comp.write_JFIF_header = jpeg->decomp.saw_JFIF_marker;
                jpeg_start_compress(&jpeg->comp, TRUE);

                for (marker = jpeg->decomp.marker_list;
                     marker != NULL;
                    marker = marker->next) {
                    /* Avoid duplicating markers */
                    if (jpeg->comp.write_JFIF_header &&
                        marker->marker == JPEG_APP0 &&
                        marker->data_length >= 5 &&
                        memcmp("JFIF", marker->data, 5) == 0) {
                        continue;
                    }
                    if (jpeg->comp.write_Adobe_marker &&
                        marker->marker == JPEG_APP0+14 &&
                        marker->data_length >= 5 &&
                        memcmp("Adobe", marker->data, 5) == 0) {
                        continue;
                    }
                    jpeg_write_marker(&jpeg->comp, marker->marker,
                        marker->data, marker->data_length);
                }

                return TRUE;
            }
        }
    }
    return FALSE;
}

static
gboolean
mms_attachment_jpeg_read_line(
    MMSAttachmentImageResize* resize,
    unsigned char* rgb24)
{
    MMSAttachmentJpegResize* jpeg = mms_attachment_jpeg_resize_cast(resize);
    if (!setjmp(jpeg->err.setjmp_buf)) {
        JSAMPROW row = rgb24;
        jpeg_read_scanlines(&jpeg->decomp, &row, 1);
        return TRUE;
    }
    return FALSE;
}

static
gboolean
mms_attachment_jpeg_write_line(
    MMSAttachmentImageResize* resize,
    const unsigned char* rgb24)
{
    MMSAttachmentJpegResize* jpeg = mms_attachment_jpeg_resize_cast(resize);
    if (!setjmp(jpeg->err.setjmp_buf)) {
        JSAMPROW row = (void*)rgb24;
        jpeg_write_scanlines(&jpeg->comp, &row, 1);
        return TRUE;
    }
    return FALSE;
}

static
void
mms_attachment_jpeg_resize_finish(
    MMSAttachmentImageResize* resize)
{
    MMSAttachmentJpegResize* jpeg = mms_attachment_jpeg_resize_cast(resize);
    if (!setjmp(jpeg->err.setjmp_buf)) {
        jpeg_finish_compress(&jpeg->comp);
        jpeg_finish_decompress(&jpeg->decomp);
    }
}

static
void
mms_attachment_jpeg_resize_free(
    MMSAttachmentImageResize* resize)
{
    MMSAttachmentJpegResize* jpeg = mms_attachment_jpeg_resize_cast(resize);
    jpeg_destroy_compress(&jpeg->comp);
    jpeg_destroy_decompress(&jpeg->decomp);
    if (jpeg->in) fclose(jpeg->in);
    if (jpeg->out) fclose(jpeg->out);
    g_free(jpeg);
}

static
void
mms_attachment_jpeg_class_init(
    MMSAttachmentJpegClass* klass)
{
    klass->fn_resize_new = mms_attachment_jpeg_resize_new;
    klass->fn_resize_prepare = mms_attachment_jpeg_resize_prepare;
    klass->fn_resize_read_line = mms_attachment_jpeg_read_line;
    klass->fn_resize_write_line = mms_attachment_jpeg_write_line;
    klass->fn_resize_finish = mms_attachment_jpeg_resize_finish;
    klass->fn_resize_free = mms_attachment_jpeg_resize_free;
}

static
void
mms_attachment_jpeg_init(
    MMSAttachmentJpeg* jpeg)
{
    jpeg->attachment.flags |= MMS_ATTACHMENT_RESIZABLE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
