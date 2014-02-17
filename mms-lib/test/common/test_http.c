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

#include "test_http.h"

#include "mms_log.h"

#include <libsoup/soup.h>

/* A single HTTP transaction */
struct test_http {
    gint ref_count;
    SoupServer* server;
    GMappedFile* resp_file;
    GBytes* req_bytes;
    int resp_status;
    char* resp_content_type;
};

static
void
test_http_callback(
    SoupServer* server,
    SoupMessage* msg,
    const char* path,
    GHashTable* query,
    SoupClientContext* context,
    gpointer data)
{
    char* uri = soup_uri_to_string(soup_message_get_uri (msg), FALSE);
    MMS_VERBOSE("%s %s HTTP/1.%d", msg->method, uri,
        soup_message_get_http_version(msg));
    g_free(uri);
    if (msg->method == SOUP_METHOD_CONNECT) {
        soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
    } else {
        TestHttp* http = data;
	if (msg->request_body->length) {
            SoupBuffer* request = soup_message_body_flatten(msg->request_body);
            if (http->req_bytes) g_bytes_unref(http->req_bytes);
            http->req_bytes = g_bytes_new_with_free_func(request->data,
                request->length, (GDestroyNotify)soup_buffer_free, request);
	}
        soup_message_set_status(msg, http->resp_status);
        soup_message_headers_set_content_type(msg->response_headers,
            http->resp_content_type ? http->resp_content_type : "text/plain",
            NULL);
        soup_message_headers_append(msg->response_headers,
            "Accept-Ranges", "bytes");
        soup_message_headers_append(msg->response_headers,
            "Connection", "close");
        if (http->resp_file) {
            soup_message_headers_set_content_length(msg->response_headers,
                g_mapped_file_get_length(http->resp_file));
            soup_message_body_append(msg->response_body, SOUP_MEMORY_TEMPORARY,
                g_mapped_file_get_contents(http->resp_file),
                g_mapped_file_get_length(http->resp_file));
        } else {
            soup_message_headers_set_content_length(msg->response_headers, 0);
        }
    }
    soup_message_body_complete(msg->request_body);
}

guint
test_http_get_port(
    TestHttp* http)
{
    return soup_server_get_port(http->server);
}

GBytes*
test_http_get_post_data(
    TestHttp* http)
{
    return http->req_bytes;
}

void
test_http_close(
    TestHttp* http)
{
    soup_server_quit(http->server);
}

TestHttp*
test_http_new(
    GMappedFile* resp_file,
    const char* resp_content_type,
    int resp_status)
{
    TestHttp* http = g_new0(TestHttp, 1);
    http->ref_count = 1;
    if (resp_file) {
        http->resp_file = g_mapped_file_ref(resp_file);
        http->resp_content_type = g_strdup(resp_content_type);
    }
    http->resp_status = resp_status;
    http->server = g_object_new(SOUP_TYPE_SERVER, NULL);
    MMS_DEBUG("Listening on port %hu", soup_server_get_port(http->server));
    soup_server_add_handler(http->server, NULL, test_http_callback, http, NULL);
    soup_server_run_async(http->server);
    return http;
}

TestHttp*
test_http_ref(
    TestHttp* http)
{
    if (http) {
        MMS_ASSERT(http->ref_count > 0);
        g_atomic_int_inc(&http->ref_count);
    }
    return http;
}

void
test_http_unref(
    TestHttp* http)
{
    if (http) {
        MMS_ASSERT(http->ref_count > 0);
        if (g_atomic_int_dec_and_test(&http->ref_count)) {
            test_http_close(http);
            if (http->resp_file) g_mapped_file_unref(http->resp_file);
            if (http->req_bytes) g_bytes_unref(http->req_bytes);
            g_object_unref(http->server);
            g_free(http->resp_content_type);
            g_free(http);
        }
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
