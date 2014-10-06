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
#include "mms_settings.h"
#include "mms_lib_util.h"
#include "mms_lib_log.h"
#include "mms_file_util.h"

#include <libexif/exif-content.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-data.h>
#include <libexif/exif-tag.h>

#include <gio/gio.h>
#include <png.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

#define RET_OK  (0)
#define RET_ERR (1)

typedef struct test_size {
    unsigned int width;
    unsigned int height;
} TestSize;

typedef struct test_image_type {
    const char* content_type;
    gboolean (*filesize)(const char* file, TestSize* size);
} TestImageType;

typedef struct test_desc {
    const char* name;
    const char* file;
    const TestImageType* type;
    int steps;
    int max_pixels;
    TestSize size;
} TestDesc;

typedef struct test_jpeg_error {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buf;
} TestJpegError;

typedef enum exif_orientation {
    EXIF_ORIENTATION_UPPER_LEFT = 1,
    EXIF_ORIENTATION_LOWER_RIGHT = 3,
    EXIF_ORIENTATION_UPPER_RIGHT = 6,
    EXIF_ORIENTATION_LOWER_LEFT = 8
} EXIF_ORIENTATION;

typedef struct test_jpeg_decompress {
    struct jpeg_decompress_struct pub;
    EXIF_ORIENTATION orientation;
} TestJpegDecompress;

static
gboolean
test_jpeg_size(
    const char* file,
    TestSize* size);

static
gboolean
test_png_size(
    const char* file,
    TestSize* size);

static const TestImageType test_jpeg =
    { "image/jpeg", test_jpeg_size };

#ifdef HAVE_MAGIC
static const TestImageType test_auto_jpeg =
    { NULL, test_jpeg_size };
#else
#  define test_auto_jpeg test_jpeg
#endif

static const TestImageType test_png =
    { "image/png", test_png_size };

static const TestDesc resize_tests[] = {
    {
        "Jpeg_Portrait1",
        "data/0001.jpg",
        &test_jpeg,
        1,
        1000000,
        {613, 1088}
    },{
        "Jpeg_Portrait2",
        "data/0001.jpg",
        &test_jpeg,
        2,
        2000000,
        {613, 1088}
    },{
        "Jpeg_Portrait3",
        "data/0001.jpg",
        &test_jpeg,
        3,
        3000000,
        {460, 816}
    },{
        "Jpeg_Portrait4",
        "data/0004.jpg",
        &test_jpeg,
        2,
        3000000,
        {816, 1088}
    },{
        "Jpeg_Portrait5",
        "data/0004.jpg",
        &test_jpeg,
        3,
        3000000,
        {612, 816}
    },{
        "Jpeg_Landscape1",
        "data/0002.jpg",
        &test_auto_jpeg,
        1,
        1000000,
        {1088, 613}
    },{
        "Jpeg_Landscape2",
        "data/0002.jpg",
        &test_auto_jpeg,
        2,
        2000000,
        {1088, 613}
    },{
        "Jpeg_Landscape3",
        "data/0002.jpg",
        &test_auto_jpeg,
        3,
        3000000,
        {816, 460}
    },{
        "Jpeg_Broken",
        "data/junk.jpg",
        &test_jpeg,
        1,
        1000000,
        {0, 0} /* Expect failure */
    },{
        "Png_1",
        "data/0003.png",
        &test_png,
        1,
        1000000,
        {1000, 750}
    },{
        "Png_2",
        "data/0003.png",
        &test_png,
        2,
        2000000,
        {666, 500}
    },{
        "Png_3",
        "data/0003.png",
        &test_png,
        3,
        3000000,
        {500, 375}
    }
};

static
void
test_jpeg_error_log(
    int level,
    j_common_ptr cinfo)
{
    char* buf = g_malloc(JMSG_LENGTH_MAX);
    buf[0] = 0;
    cinfo->err->format_message(cinfo, buf);
    buf[JMSG_LENGTH_MAX] = 0;
    mms_log(NULL, level, "%s", buf);
    g_free(buf);
}

static
void
test_jpeg_error_exit(
    j_common_ptr cinfo)
{
    TestJpegError* err = (TestJpegError*)cinfo->err;
    test_jpeg_error_log(MMS_LOGLEVEL_ERR, cinfo);
    longjmp(err->setjmp_buf, 1);
}

static
void
test_jpeg_error_output(
    j_common_ptr cinfo)
{
    test_jpeg_error_log(MMS_LOGLEVEL_DEBUG, cinfo);
}

static
JOCTET
test_jpeg_getc(
    j_decompress_ptr cinfo)
{
    struct jpeg_source_mgr* src = cinfo->src;
    if (!src->bytes_in_buffer && !src->fill_input_buffer(cinfo)) {
        ERREXIT(cinfo, JERR_CANT_SUSPEND);
    }
    src->bytes_in_buffer--;
    return *src->next_input_byte++;
}

static
boolean
test_jpeg_APP1(
    j_decompress_ptr cinfo)
{
    TestJpegDecompress* dec = MMS_CAST(cinfo, TestJpegDecompress, pub);
    ExifLoader* eloader;
    ExifData* edata;
    unsigned int len;

    /* Read the marker length */
    unsigned char buf[2];
    buf[0] = test_jpeg_getc(cinfo);
    buf[1] = test_jpeg_getc(cinfo);
    len = buf[0] << 8;
    len += buf[1];
    MMS_DEBUG("Marker 0x%02X %u bytes", cinfo->unread_marker, len);
    if (len < 2) ERREXIT(cinfo, JERR_BAD_LENGTH);

    /* Feed the whole thing to the Exit loader */
    eloader = exif_loader_new();
    exif_loader_write(eloader, buf, sizeof(buf));
    len -= 2;
    while (len > 0) {
        struct jpeg_source_mgr* src = cinfo->src;
        if (src->bytes_in_buffer || src->fill_input_buffer(cinfo)) {
            unsigned int nbytes = MIN(src->bytes_in_buffer, len);
            exif_loader_write(eloader, (void*)src->next_input_byte, nbytes);
            src->bytes_in_buffer -= nbytes;
            src->next_input_byte += nbytes;
            len -= nbytes;
        } else {
            ERREXIT(cinfo, JERR_CANT_SUSPEND);
        }
    }
    edata = exif_loader_get_data(eloader);
    exif_loader_unref(eloader);
    if (edata) {
        ExifEntry* orientation = exif_content_get_entry(
            edata->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
        if (orientation) {
            /* Actually there are two bytes there but the second one
             * should be zero */
            dec->orientation = orientation->data[0];
            MMS_DEBUG("Orientation %d", dec->orientation);
        }
        exif_data_unref(edata);
    }
    return TRUE;
}

static
gboolean
test_jpeg_size(
    const char* file,
    TestSize* size)
{
    gboolean ok = FALSE;
    FILE* in = fopen(file, "rb");
    if (in) {
        TestJpegError err;
        TestJpegDecompress dec;
        dec.orientation = EXIF_ORIENTATION_UPPER_LEFT;
        dec.pub.err = jpeg_std_error(&err.pub);
        err.pub.error_exit = test_jpeg_error_exit;
        err.pub.output_message = test_jpeg_error_output;
        if (!setjmp(err.setjmp_buf)) {
            jpeg_create_decompress(&dec.pub);
            jpeg_set_marker_processor(&dec.pub, JPEG_APP0+1, test_jpeg_APP1);
            jpeg_stdio_src(&dec.pub, in);
            jpeg_read_header(&dec.pub, TRUE);
            switch (dec.orientation) {
            default:
            case EXIF_ORIENTATION_UPPER_LEFT:
            case EXIF_ORIENTATION_LOWER_RIGHT:
                size->width = dec.pub.image_width;
                size->height = dec.pub.image_height;
                break;
            case EXIF_ORIENTATION_UPPER_RIGHT:
            case EXIF_ORIENTATION_LOWER_LEFT:
                size->width = dec.pub.image_height;
                size->height = dec.pub.image_width;
                break;
            }
            ok = TRUE;
        }
        jpeg_destroy_decompress(&dec.pub);
        fclose(in);
    }
    return ok;
}

static
gboolean
test_png_size(
    const char* file,
    TestSize* size)
{
    gboolean ok = FALSE;
    FILE* in = fopen(file, "rb");
    if (in) {
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL);
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!setjmp(png_jmpbuf(png_ptr))) {
            png_init_io(png_ptr, in);
            png_read_info(png_ptr, info_ptr);
            size->width = png_get_image_width(png_ptr, info_ptr);
            size->height = png_get_image_height(png_ptr, info_ptr);
            ok = TRUE;
        }
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    return ok;
}

static
int
test_run_one(
    const MMSConfig* config,
    const TestDesc* test)
{
    int ret = RET_ERR;
    char* tmpl;
    const char* dir;
    char* name = g_path_get_basename(test->file);
    tmpl = g_strconcat(config->root_dir, "/resize_XXXXXX", NULL);
    dir = g_mkdtemp(tmpl);
    if (dir) {
        GError* error = NULL;
        char* testfile = g_strconcat(dir, "/", name, NULL);
        if (mms_file_copy(test->file, testfile, NULL)) {
            MMSAttachment* at;
            MMSAttachmentInfo info;
            MMSSettingsSimData sim_settings;
            mms_settings_sim_data_default(&sim_settings);
            sim_settings.max_pixels = test->max_pixels;
            info.file_name = testfile;
            info.content_type = test->type->content_type;
            info.content_id = name;
            at = mms_attachment_new(config, &info, &error);
            if (at) {
                int i;
                gboolean ok = TRUE;
                for (i=0; i<test->steps && ok; i++) {
                    if (!mms_attachment_resize(at, &sim_settings)) {
                        ok = FALSE;
                    }
                }
                if (ok && test->size.width && test->size.height) {
                    TestSize size;
                    if (test->type->filesize(at->file_name, &size)) {
                        if (size.width == test->size.width &&
                            size.height == test->size.height) {
                            mms_attachment_reset(at);
                            if (!strcmp(at->file_name, testfile)) {
                                ret = RET_OK;
                            } else {
                                MMS_DEBUG("Reset didn't work");
                            }
                        } else {
                            MMS_ERR("Output size mismatch: (%ux%u) vs (%ux%u)",
                                size.width, size.height,
                                test->size.width, test->size.height);
                        }
                    }
                } else if (!ok && !test->size.width && !test->size.height) {
                    ret = RET_OK;
                }
                /* Extra ref/unref improves the coverage */
                mms_attachment_ref(at);
                mms_attachment_unref(at);
                mms_attachment_unref(at);
            } else {
                MMS_ERR("%s", MMS_ERRMSG(error));
                g_error_free(error);
            }
        } else {
            MMS_ERR("Failed to copy %s -> %s", test->file, testfile);
        }
        g_free(testfile);
    }
    MMS_INFO("%s: %s", (ret == RET_OK) ? "OK" : "FAILED", test->name);
    g_free(name);
    g_free(tmpl);
    return ret;
}

static
int
test_run(
    const MMSConfig* config,
    const char* name)
{
    int i, ret;
    if (name) {
        const TestDesc* found = NULL;
        for (i=0, ret = RET_ERR; i<G_N_ELEMENTS(resize_tests); i++) {
            const TestDesc* test = resize_tests + i;
            if (!strcmp(test->name, name)) {
                ret = test_run_one(config, test);
                found = test;
                break;
            }
        }
        if (!found) MMS_ERR("No such test: %s", name);
    } else {
        for (i=0, ret = RET_OK; i<G_N_ELEMENTS(resize_tests); i++) {
            int test_status = test_run_one(config, resize_tests + i);
            if (ret == RET_OK && test_status != RET_OK) ret = test_status;
        }
    }
    return ret;
}
int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    gboolean keep_temp = FALSE;
    gboolean verbose = FALSE;

    GOptionContext* options;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "keep", 'k', 0, G_OPTION_ARG_NONE, &keep_temp,
          "Keep temporary files", NULL },
        { NULL }
    };

    mms_lib_init(argv[0]);
    options = g_option_context_new("[TEST] - Resizing test");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, NULL) && argc < 3) {
        const char* test = (argc == 2) ? argv[1] : NULL;
        MMSConfig config;
        mms_lib_default_config(&config);
        config.root_dir = "/tmp";
        config.keep_temp_files = keep_temp;

        mms_log_stdout_timestamp = FALSE;
        mms_log_default.name = "test_resize";
        if (verbose) {
            mms_log_default.level = MMS_LOGLEVEL_VERBOSE;
        } else {
            mms_log_default.level = MMS_LOGLEVEL_INFO;
            mms_attachment_log.level = MMS_LOGLEVEL_ERR;
        }
        ret = test_run(&config, test);
    }
    g_option_context_free(options);
    mms_lib_deinit();
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
