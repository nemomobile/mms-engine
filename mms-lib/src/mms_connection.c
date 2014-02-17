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

#include "mms_connection.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_connection_log
#include "mms_lib_log.h"

G_DEFINE_TYPE(MMSConnection, mms_connection, G_TYPE_OBJECT);
#define MMS_CONNECTION_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MMS_TYPE_CONNECTION, MMSConnectionClass))
#define MMS_CONNECTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MMS_TYPE_CONNECTION, MMSConnection))

GQuark
mms_connection_error_quark()
{
    return g_quark_from_static_string("mms-connection-error-quark");
}

MMSConnection*
mms_connection_ref(
    MMSConnection* conn)
{
    if (conn) g_object_ref(MMS_CONNECTION(conn));
    return conn;
}

void
mms_connection_unref(
    MMSConnection* conn)
{
    if (conn) g_object_unref(MMS_CONNECTION(conn));
}

const char*
mms_connection_state_name(
    MMSConnection* conn)
{
    static const char* names[] = {"????","OPENING","FAILED","OPEN","CLOSED"};
    return names[mms_connection_state(conn)];
}

MMS_CONNECTION_STATE
mms_connection_state(
    MMSConnection* conn)
{
    return conn ? conn->state : MMS_CONNECTION_STATE_INVALID;
}

void
mms_connection_close(
    MMSConnection* conn)
{
    if (conn) MMS_CONNECTION_GET_CLASS(conn)->fn_close(conn);
}

/**
 * Final stage of deinitialization
 */
static
void
mms_connection_finalize(
    GObject* object)
{
    MMSConnection* conn = MMS_CONNECTION(object);
    MMS_VERBOSE_("%p", conn);
    MMS_ASSERT(!conn->delegate);
    g_free(conn->imsi);
    g_free(conn->mmsc);
    g_free(conn->mmsproxy);
    g_free(conn->netif);
    G_OBJECT_CLASS(mms_connection_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_connection_class_init(
    MMSConnectionClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = mms_connection_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_connection_init(
    MMSConnection* conn)
{
    MMS_VERBOSE_("%p", conn);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
