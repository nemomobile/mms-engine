/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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

#include "mms_file_util.h"
#include "mms_log.h"
#include "mms_error.h"

/**
 * Callbacks for mms_file_is_smil
 */
static
void
mms_smil_parse_start(
    GMarkupParseContext* parser,
    const char* element_name,
    const char **attribute_names,
    const char **attribute_values,
    gpointer userdata,
    GError** error)
{
    static const GMarkupParser skip = { NULL, NULL, NULL, NULL, NULL };
    if (!strcmp(element_name, "smil")) {
        gboolean* smil_found = userdata;
        *smil_found = TRUE;
    }
    g_markup_parse_context_push(parser, &skip, NULL);
}

static
void
mms_smil_parse_end(
    GMarkupParseContext* parser,
    const gchar* element_name,
    gpointer userdata,
    GError** error)
{
    g_markup_parse_context_pop(parser);
}

/**
 * Checks if this ia a SMIL file, i.e. a parsable XML with <smil> as a
 * root tag.
 */
gboolean
mms_file_is_smil(
    const char* file)
{
    static const GMarkupParser toplevel = { mms_smil_parse_start,
        mms_smil_parse_end, NULL, NULL, NULL };
    gboolean smil = FALSE;
    GMappedFile* map = g_mapped_file_new(file, FALSE, NULL);
    if (map) {
        gboolean smil_found = FALSE;
        GMarkupParseContext* parser = g_markup_parse_context_new(&toplevel,
            G_MARKUP_TREAT_CDATA_AS_TEXT, &smil_found, NULL);
        if (g_markup_parse_context_parse(parser,
            g_mapped_file_get_contents(map),
            g_mapped_file_get_length(map), NULL)) {
            g_markup_parse_context_end_parse(parser, NULL);
            smil = smil_found;
        }
        g_mapped_file_unref(map);
        g_markup_parse_context_free(parser);
    }
    return smil;
}

/**
 * Removes both the file and the directory containing it, if it's empty.
 */
void
mms_remove_file_and_dir(
    const char* file)
{
    if (file) {
        char* dir = g_path_get_dirname(file);
        unlink(file);
        if (rmdir(dir) == 0) {
            MMS_VERBOSE("Deleted %s", dir);
        }
        g_free(dir);
    }
}

/**
 * Creates a file in the specified directory. Creates the directory if
 * it doesn't exist. If file already exists, truncates it. Returns file
 * discriptor positioned at the beginning of the new file or -1 if an I/O
 * error occurs.
 */
int
mms_create_file(
    const char* dir,
    const char* file,
    char** path,
    GError** error)
{
    int fd = -1;
    int err = g_mkdir_with_parents(dir, MMS_DIR_PERM);
    if (!err || errno == EEXIST) {
        char* fname = g_strconcat(dir, "/", file, NULL);
        fd = open(fname, O_CREAT|O_RDWR|O_TRUNC|O_BINARY, MMS_FILE_PERM);
        if (fd < 0) {
            MMS_ERROR(error, MMS_LIB_ERROR_IO,
                "Failed to create file %s: %s", fname, strerror(errno));
        } else if (path) {
            *path = fname;
            fname = NULL;
        }
        g_free(fname);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_IO,
            "Failed to create directory %s: %s", dir, strerror(errno));
    }
    return fd;
}

/**
 * Writes data to a file, creating the directory hierarhy if necessary.
 */
gboolean
mms_write_file(
    const char* dir,
    const char* file,
    const void* data,
    gsize size,
    char** path)
{
    gboolean saved = FALSE;
    int err = g_mkdir_with_parents(dir, MMS_DIR_PERM);
    if (!err || errno == EEXIST) {
        GError* error = NULL;
        char* fname = g_strconcat(dir, "/", file, NULL);
        unlink(fname);
        if (g_file_set_contents(fname, data, size, &error)) {
            MMS_VERBOSE("Created %s", fname);
            chmod(fname, MMS_FILE_PERM);
            saved = TRUE;
            if (path) {
                *path = fname;
                fname = NULL;
            }
        } else {
            MMS_ERR("%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
        g_free(fname);
    } else {
        MMS_ERR("Failed to create directory %s: %s", dir, strerror(errno));
    }
    return saved;
}

/**
 * Same as mms_write_file, only works with GBytes
 */
gboolean
mms_write_bytes(
    const char* dir,
    const char* file,
    GBytes* bytes,
    char** path)
{
    gsize len = 0;
    const guint8* data = g_bytes_get_data(bytes, &len);
    return mms_write_file(dir, file, data, len, path);
}

/**
 * Copies the file. Assumes that the destination directory exists.
 */
gboolean
mms_file_copy(
    const char* src,
    const char* dest,
    GError** error)
{
    gboolean ok = FALSE;
    int out = open(dest, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, MMS_FILE_PERM);
    if (out >= 0) {
        int in = open(src, O_RDONLY | O_BINARY);
        if (in >= 0) {
            int nbytes;
            const size_t buflen = 4096;
            void* buf = g_malloc(buflen);
            ok = TRUE;
            while ((nbytes = read(in, buf, buflen)) > 0) {
                if (write(out, buf, nbytes) < nbytes) {
                    ok = FALSE;
                    MMS_ERROR(error, MMS_LIB_ERROR_IO,
                        "Failed to write %s: %s", dest, strerror(errno));
                    break;
                }
            }
            if (nbytes < 0) {
                ok = FALSE;
                MMS_ERROR(error, MMS_LIB_ERROR_IO,
                    "Failed to read %s: %s", src, strerror(errno));
            }
            g_free(buf);
            close(in);
        } else {
            MMS_ERROR(error, MMS_LIB_ERROR_IO,
                "Failed to open file %s: %s", src, strerror(errno));
        }
        close(out);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_IO,
            "Failed to create file %s: %s", dest, strerror(errno));
    }
    return ok;
}

/*
 * MIME decoding
 */
typedef struct mms_mime_decode {
    int out;
    const char* dest;
    size_t outbytes;
    size_t outbuflen;
    void* outbuf;
    GMimeEncoding state;
} MMSMimeDecode;

static
gboolean
mms_file_decode_init(
    MMSMimeDecode* dec,
    const char* dest,
    GMimeContentEncoding enc,
    GError** error)
{
    memset(dec, 0, sizeof(*dec));
    dec->out = open(dest, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, MMS_FILE_PERM);
    if (dec->out >= 0) {
        dec->dest = dest;
        g_mime_encoding_init_decode(&dec->state, enc);
        return TRUE;
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_IO, "Failed to create file %s: %s",
            dest, strerror(errno));
        return FALSE;
    }
}

static
void
mms_file_decode_deinit(
    MMSMimeDecode* dec)
{
    g_free(dec->outbuf);
    close(dec->out);
}

gboolean
mms_file_decode_step(
    MMSMimeDecode* dec,
    const void* inbuf,
    size_t inlen,
    size_t (*step)(GMimeEncoding*, const char*, size_t, char*),
    GError** error)
{
    ssize_t nbytes;
    size_t need = g_mime_encoding_outlen(&dec->state, inlen);
    if (need > dec->outbuflen) {
        g_free(dec->outbuf);
        dec->outbuf = g_malloc(need);
        dec->outbuflen = need;
    }
    nbytes = step(&dec->state, inbuf, inlen, dec->outbuf);
    if (write(dec->out, dec->outbuf, nbytes) == nbytes) {
        dec->outbytes += nbytes;
        return TRUE;
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_IO, "Failed to write %s: %s",
            dec->dest, strerror(errno));
        return FALSE;
    }
}

gboolean
mms_file_decode(
    const char* src,
    const char* dest,
    GMimeContentEncoding enc,
    GError** error)
{
    gboolean ok = FALSE;
    int in = open(src, O_RDONLY | O_BINARY);
    if (in >= 0) {
        MMSMimeDecode dec;
        if (mms_file_decode_init(&dec, dest, enc, error)) {
            int nbytes;
            size_t inbytes = 0;
            const size_t buflen = 1024;
            void* inbuf = g_malloc(buflen);
            ok = TRUE;
            while ((nbytes = read(in, inbuf, buflen)) > 0) {
                inbytes += nbytes;
                if (!mms_file_decode_step(&dec, inbuf, nbytes,
                    g_mime_encoding_step, error)) {
                    ok = FALSE;
                    break;
                }
            }
            if (nbytes < 0) {
                ok = FALSE;
                MMS_ERROR(error, MMS_LIB_ERROR_IO,
                    "Failed to read %s: %s", src, strerror(errno));
            } else if (ok) {
                if (mms_file_decode_step(&dec, NULL, 0,
                    g_mime_encoding_flush, error)) {
                    MMS_DEBUG("Decoded %s (%d bytes) -> %s (%d bytes)",
                        src, (int)inbytes, dest, (int)dec.outbytes);
                } else {
                    ok = FALSE;
                }
            }
            mms_file_decode_deinit(&dec);
            g_free(inbuf);
        }
        close(in);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_IO,
            "Failed to open file %s: %s", src, strerror(errno));
    }
    return ok;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

