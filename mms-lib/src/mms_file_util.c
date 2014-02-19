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

#include "mms_file_util.h"
#include "mms_log.h"
#include "mms_error.h"

#include <fcntl.h>

#ifndef O_BINARY
#  define O_BINARY 0
#endif

/**
 * Removes both the file and the directory containing it, if it's empty.
 */
void
mms_remove_file_and_dir(
    const char* file)
{
    char* dir = g_path_get_dirname(file);
    unlink(file);
    if (rmdir(dir) == 0) {
        MMS_VERBOSE("Deleted %s", dir);
    }
    g_free(dir);
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
            MMS_ERROR(error, MMS_LIB_ERROR_FILE,
                "Failed to create file %s: %s", fname, strerror(errno));
        } else if (path) {
            *path = fname;
            fname = NULL;
        }
        g_free(fname);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_FILE,
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

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

