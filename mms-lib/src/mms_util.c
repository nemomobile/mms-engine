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

#include "mms_util.h"
#include "mms_lib_util.h"
#include "mms_connection.h"
#include "mms_codec.h"

#ifndef _WIN32
#  include <sys/ioctl.h>
#  include <arpa/inet.h>
#  include <net/if.h>
#endif

/* Appeared in libsoup somewhere between 2.41.5 and 2.41.90 */
#ifndef SOUP_SESSION_LOCAL_ADDRESS
#  define SOUP_SESSION_LOCAL_ADDRESS "local-address"
#endif

/* Logging */
#define MMS_LOG_MODULE_NAME mms_util_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-util");

/**
 * Strips leading spaces and "/TYPE=" suffix from the string.
 */
char*
mms_strip_address_type(
    char* address)
{
    if (address) {
        char* type = g_strrstr(g_strstrip(address), MMS_ADDRESS_TYPE_SUFFIX);
        if (type) *type = 0;
    }
    return address;
}

/**
 * Splits comma-separated list of addresses into an array of string pointers.
 * Strips "/TYPE=" suffix from each address. Caller needs to deallocate the
 * returned list with g_strfreev.
 */
char**
mms_split_address_list(
    const char* addres_list)
{
    char** list = NULL;
    if (addres_list && addres_list[0]) {
        int i;
        list = g_strsplit(addres_list, ",", 0);
        for (i=0; list[i]; i++) {
            list[i] = mms_strip_address_type(list[i]);
        }
    } else {
        list = g_new(char*, 1);
        list[0] = NULL;
    }
    return list;
}

/**
 * Allocates and decodes WAP push PDU. Returns NULL if decoding fails.
 */
MMSPdu*
mms_decode_bytes(
    GBytes* bytes)
{
    MMSPdu* pdu = NULL;
    if (bytes) {
        gsize len = 0;
        const guint8* data = g_bytes_get_data(bytes, &len);
        pdu = g_new0(MMSPdu, 1);
        if (!mms_message_decode(data, len, pdu)) {
            mms_message_free(pdu);
            pdu = NULL;
        }
    }
    return pdu;
}

/**
 * Utility to converts string URI into SoupURI
 */
SoupURI*
mms_parse_http_uri(
    const char* raw_uri)
{
    SoupURI* uri = NULL;
    if (raw_uri) {
        static const char* http = "http://";
        const char* uri_to_parse;
        char* tmp_uri = NULL;
        if (g_str_has_prefix(raw_uri, http)) {
            uri_to_parse = raw_uri;
        } else {
            uri_to_parse = tmp_uri = g_strconcat(http, raw_uri, NULL);
        }
        uri = soup_uri_new(uri_to_parse);
        if (!uri) {
            MMS_ERR("Could not parse %s as a URI", uri_to_parse);
        }
        g_free(tmp_uri);
    }
    return uri;
}

/**
 * Sets up new SOUP session
 */
static
SoupSession*
mms_create_http_session(
    const MMSConfig* cfg,
    MMSConnection* conn)
{
    SoupSession* session = NULL;

    /* Determine address of the MMS interface */
    if (conn->netif && conn->netif[0]) {
#ifndef _WIN32
        struct ifreq ifr;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd >= 0) {
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, conn->netif, IFNAMSIZ-1);
            if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0) {
                SoupAddress* local_address = soup_address_new_from_sockaddr(
                    &ifr.ifr_addr, sizeof(ifr.ifr_addr));
#  if MMS_LOG_DEBUG
                char buf[128];
                int af = ifr.ifr_addr.sa_family;
                buf[0] = 0;
                if (af == AF_INET) {
                    struct sockaddr_in* addr = (void*)&ifr.ifr_addr;
                    inet_ntop(af, &addr->sin_addr, buf, sizeof(buf));
                } else if (af == AF_INET6) {
                    struct sockaddr_in6* addr = (void*)&ifr.ifr_addr;
                    inet_ntop(af, &addr->sin6_addr, buf, sizeof(buf));
                } else {
                    snprintf(buf, sizeof(buf), "<address family %d>", af);
                }
                buf[sizeof(buf)-1] = 0;
                MMS_DEBUG("MMS interface address %s", buf);
#  endif /* MMS_LOG_DEBUG */
                MMS_ASSERT(local_address);
                session = soup_session_async_new_with_options(
                    SOUP_SESSION_LOCAL_ADDRESS, local_address,
                    NULL);
                g_object_unref(local_address);
            } else {
                MMS_ERR("Failed to query IP address of %s: %s",
                    conn->netif, strerror(errno));
            }
            close(fd);
        }
#endif /* _WIN32 */
    } else {
        MMS_WARN("MMS interface is unknown");
    }

    if (!session) {
        /* No local address so bind to any interface */
        session = soup_session_async_new();
    }

    if (conn->mmsproxy && conn->mmsproxy[0]) {
        SoupURI* proxy_uri = mms_parse_http_uri(conn->mmsproxy);
        if (proxy_uri) {
            MMS_DEBUG("MMS proxy %s", conn->mmsproxy);
            g_object_set(session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
            soup_uri_free(proxy_uri);
        }
    }

    if (cfg->user_agent) {
        g_object_set(session, SOUP_SESSION_USER_AGENT, cfg->user_agent, NULL);
    }

    return session;
}

/**
 * Create HTTP transfer context.
 */
MMSHttpTransfer*
mms_http_transfer_new(
    const MMSConfig* config,
    MMSConnection* connection,
    const char* method,
    const char* uri,
    int fd)
{
    SoupURI* soup_uri = soup_uri_new(uri);
    if (soup_uri) {
        MMSHttpTransfer* tx = g_new(MMSHttpTransfer, 1);
        tx->session = mms_create_http_session(config, connection);
        tx->message = soup_message_new_from_uri(method, soup_uri);
        tx->connection = mms_connection_ref(connection);
        tx->fd = fd;
        soup_uri_free(soup_uri);
        soup_message_set_flags(tx->message,
            SOUP_MESSAGE_NO_REDIRECT |
            SOUP_MESSAGE_NEW_CONNECTION);
        /* We shouldn't need this extra reference but otherwise
         * SoupMessage gets deallocated too early. Not sure why. */
        g_object_ref(tx->message);
        return tx;
    }
    return NULL;
}

/**
 * Deallocates HTTP transfer context created by mms_http_transfer_new()
 */
void
mms_http_transfer_free(
    MMSHttpTransfer* tx)
{
    if (tx) {
        soup_session_abort(tx->session);
        g_object_unref(tx->session);
        g_object_unref(tx->message);
        mms_connection_unref(tx->connection);
        close(tx->fd);
        g_free(tx);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
