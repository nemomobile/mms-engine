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

#include "mms_ofono_connection.h"
#include "mms_ofono_context.h"
#include "mms_ofono_modem.h"
#include "mms_ofono_names.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_connection_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-ofono-connection");

/* Generated headers */
#include "org.ofono.ConnectionContext.h"

typedef MMSConnectionClass MMSOfonoConnectionClass;

G_DEFINE_TYPE(MMSOfonoConnection, mms_ofono_connection, MMS_TYPE_CONNECTION);
#define MMS_TYPE_OFONO_CONNECTION (mms_ofono_connection_get_type())
#define MMS_OFONO_CONNECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_OFONO_CONNECTION, MMSOfonoConnection))

MMSConnection*
mms_ofono_connection(
    MMSOfonoConnection* ofono)
{
    return &ofono->connection;
}

void
mms_ofono_connection_cancel(
    MMSOfonoConnection* ofono)
{
    if (ofono->connection.state <= MMS_CONNECTION_STATE_OPENING) {
        mms_ofono_connection_set_state(ofono, MMS_CONNECTION_STATE_FAILED);
    } else {
        mms_ofono_connection_set_state(ofono, MMS_CONNECTION_STATE_CLOSED);
    }
}

gboolean
mms_ofono_connection_set_state(
    MMSOfonoConnection* ofono,
    MMS_CONNECTION_STATE state)
{
    if (ofono->connection.state != state) {
        if (ofono->connection.state == MMS_CONNECTION_STATE_FAILED ||
            ofono->connection.state == MMS_CONNECTION_STATE_CLOSED) {
            /* These are terminal states, can't change those */
            return FALSE;
        } else if (ofono->connection.state > state) {
            /* Can't move back to a previous state */
            return FALSE;
        }
        if (state == MMS_CONNECTION_STATE_FAILED ||
            state == MMS_CONNECTION_STATE_CLOSED) {
            /* Stop listening for property changes */
            if (ofono->property_change_signal_id) {
                g_signal_handler_disconnect(ofono->proxy,
                    ofono->property_change_signal_id);
                ofono->property_change_signal_id = 0;
            }
        }
        ofono->connection.state = state;
        if (ofono->connection.delegate &&
            ofono->connection.delegate->fn_connection_state_changed) {
            ofono->connection.delegate->fn_connection_state_changed(
            ofono->connection.delegate, &ofono->connection);
        }
    }
    return TRUE;
}

static
void
mms_ofono_connection_property_changed(
    OrgOfonoConnectionContext* proxy,
    const char* key,
    GVariant* variant,
    MMSOfonoConnection* ofono)
{
    MMSConnection* conn = &ofono->connection;
    MMS_ASSERT(proxy == ofono->proxy);
    GVariant* value = g_variant_get_variant(variant);
    MMS_VERBOSE("%p %s %s", conn, conn->imsi, key);
    if (!strcmp(key, OFONO_CONTEXT_PROPERTY_ACTIVE)) {
        gboolean active = g_variant_get_boolean(value);
        if (active) {
            MMS_DEBUG("Connection %s opened", conn->imsi);
            mms_ofono_connection_set_state(ofono, MMS_CONNECTION_STATE_OPEN);
        } else {
            mms_ofono_connection_set_state(ofono, MMS_CONNECTION_STATE_CLOSED);
        }
    } else if (!strcmp(key, OFONO_CONTEXT_PROPERTY_SETTINGS)) {
        GVariant* interfaceValue = g_variant_lookup_value(value,
            OFONO_CONTEXT_SETTING_INTERFACE, G_VARIANT_TYPE_STRING);
        g_free(conn->netif);
        if (interfaceValue) {
            conn->netif = g_strdup(g_variant_get_string(interfaceValue, NULL));
            MMS_DEBUG("Interface: %s", conn->netif);
            g_variant_unref(interfaceValue);
        } else {
            conn->netif = NULL;
        }
    } else if (!strcmp(key, OFONO_CONTEXT_PROPERTY_MMS_PROXY)) {
        g_free(conn->mmsproxy);
        conn->mmsproxy = g_strdup(g_variant_get_string(value, NULL));
        MMS_DEBUG("MessageProxy: %s", conn->mmsproxy);
    } else if (!strcmp(key, OFONO_CONTEXT_PROPERTY_MMS_CENTER)) {
        g_free(conn->mmsc);
        conn->mmsc = g_strdup(g_variant_get_string(value, NULL));
        MMS_DEBUG("MessageCenter: %s", conn->mmsc);
  } else {
        MMS_ASSERT(strcmp(key, OFONO_CONTEXT_PROPERTY_TYPE));
    }
    g_variant_unref(value);
}

MMSOfonoConnection*
mms_ofono_connection_new(
    MMSOfonoContext* context,
    gboolean user_request)
{
    GError* error = NULL;
    GVariant* properties = NULL;
    if (org_ofono_connection_context_call_get_properties_sync(context->proxy,
        &properties, NULL, &error)) {
        GVariant* value;
        MMSOfonoConnection* ofono;
        MMSConnection* conn;

        ofono = g_object_new(MMS_TYPE_OFONO_CONNECTION, NULL);
        conn = &ofono->connection;

        conn->user_connection = user_request;
        g_object_ref(ofono->proxy = context->proxy);
        ofono->context = context;
        ofono->connection.state = context->active ?
            MMS_CONNECTION_STATE_OPEN : MMS_CONNECTION_STATE_OPENING;

        value = g_variant_lookup_value(properties,
            OFONO_CONTEXT_PROPERTY_ACTIVE, G_VARIANT_TYPE_BOOLEAN);
        if (value) {
            if (g_variant_get_boolean(value)) {
                ofono->connection.state = MMS_CONNECTION_STATE_OPEN;
            }
            g_variant_unref(value);
        }
        value = g_variant_lookup_value(properties,
            OFONO_CONTEXT_PROPERTY_MMS_PROXY, G_VARIANT_TYPE_STRING);
        if (value) {
            conn->mmsproxy = g_strdup(g_variant_get_string(value, NULL));
            MMS_DEBUG("MessageProxy: %s", conn->mmsproxy);
            g_variant_unref(value);
        }
        value = g_variant_lookup_value(properties,
            OFONO_CONTEXT_PROPERTY_MMS_CENTER, G_VARIANT_TYPE_STRING);
        if (value) {
            conn->mmsc = g_strdup(g_variant_get_string(value, NULL));
            MMS_DEBUG("MessageCenter: %s", conn->mmsc);
            g_variant_unref(value);
        }
        value = g_variant_lookup_value(properties,
            OFONO_CONTEXT_PROPERTY_SETTINGS, G_VARIANT_TYPE_VARDICT);
        if (value) {
            GVariant* netif = g_variant_lookup_value(value,
                OFONO_CONTEXT_SETTING_INTERFACE, G_VARIANT_TYPE_STRING);
            if (netif) {
                conn->netif = g_strdup(g_variant_get_string(netif, NULL));
                MMS_DEBUG("Interface: %s", conn->netif);
                g_variant_unref(netif);
            }
            g_variant_unref(value);
        }

        /* Listen for property changes */
        ofono->property_change_signal_id = g_signal_connect(
            ofono->proxy, "property-changed",
            G_CALLBACK(mms_ofono_connection_property_changed),
            conn);
        conn->imsi = g_strdup(context->modem->imsi);
        g_variant_unref(properties);
        return ofono;
    } else {
        MMS_ERR("Error getting connection properties: %s", MMS_ERRMSG(error));
        if (error) g_error_free(error);
        return NULL;
    }
}

MMSOfonoConnection*
mms_ofono_connection_ref(
    MMSOfonoConnection* ofono)
{
    if (ofono) mms_connection_ref(&ofono->connection);
    return ofono;
}

void
mms_ofono_connection_unref(
    MMSOfonoConnection* ofono)
{
    if (ofono) mms_connection_unref(&ofono->connection);
}

static
void
mms_ofono_connection_close(
    MMSConnection* connection)
{
    MMSOfonoConnection* ofono = MMS_OFONO_CONNECTION(connection);
    if (ofono->context) mms_ofono_context_set_active(ofono->context, FALSE);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_ofono_connection_dispose(
    GObject* object)
{
    MMSOfonoConnection* ofono = MMS_OFONO_CONNECTION(object);
    MMS_VERBOSE_("%p", ofono);
    if (ofono->property_change_signal_id) {
        g_signal_handler_disconnect(ofono->proxy,
            ofono->property_change_signal_id);
        ofono->property_change_signal_id = 0;
    }
    if (ofono->proxy) {
        g_object_unref(ofono->proxy);
        ofono->proxy = NULL;
    }
    G_OBJECT_CLASS(mms_ofono_connection_parent_class)->dispose(object);
}

/**
 * Per class initializer
 */
static
void
mms_ofono_connection_class_init(
    MMSOfonoConnectionClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    klass->fn_close = mms_ofono_connection_close;
    object_class->dispose = mms_ofono_connection_dispose;
}

/**
 * Per instance initializer
 */
static
void
mms_ofono_connection_init(
    MMSOfonoConnection* ofono)
{
    MMS_VERBOSE_("%p", ofono);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
