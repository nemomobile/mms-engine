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

#ifndef TEST_HTTP_H
#define TEST_HTTP_H

#include <gio/gio.h>

/* Local HTTP server for emulating MMSC */
typedef struct test_http TestHttp;

TestHttp*
test_http_new(
    GMappedFile* get_file,
    const char* resp_content_type,
    int resp_status);

TestHttp*
test_http_ref(
    TestHttp* http);

void
test_http_unref(
    TestHttp* http);

guint
test_http_get_port(
    TestHttp* http);

GBytes*
test_http_get_post_data(
    TestHttp* http);

void
test_http_close(
    TestHttp* http);

#endif /* TEST_CONNMAN_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
