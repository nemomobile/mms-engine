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

#include "mms_ofono_context.h"
#include "mms_ofono_modem.h"
#include "mms_ofono_connection.h"
#include "mms_ofono_names.h"

/* Generated headers */
#include "org.ofono.ConnectionContext.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_ofono_context_log
#include "mms_ofono_log.h"
MMS_LOG_MODULE_DEFINE("mms-ofono-context");

static
void
mms_ofono_context_drop_connection(
    MMSOfonoContext* context,
    MMS_CONNECTION_STATE state)
{
    MMSOfonoConnection* ofono = context->connection;
    if (ofono) {
        context->connection = NULL;
        ofono->context = NULL;
        if (state != MMS_CONNECTION_STATE_INVALID) {
            mms_ofono_connection_set_state(ofono, state);
        }
        mms_ofono_connection_unref(ofono);
    }
}

static
void
mms_ofono_context_property_changed(
    OrgOfonoConnectionContext* proxy,
    const char* key,
    GVariant* variant,
    MMSOfonoContext* context)
{
    MMS_ASSERT(proxy == context->proxy);
    if (!strcmp(key, OFONO_CONTEXT_PROPERTY_ACTIVE)) {
        GVariant* value = g_variant_get_variant(variant);
        context->active = g_variant_get_boolean(value);
        MMS_DEBUG("%s %sactive", context->path, context->active ? "" : "not ");
        g_variant_unref(value);
        if (context->active) {
            if (context->connection && !mms_ofono_connection_set_state(
                context->connection, MMS_CONNECTION_STATE_OPEN)) {
                /* Connection is in a wrong state? */
                mms_ofono_context_drop_connection(context,
                    MMS_CONNECTION_STATE_INVALID);
            }
            if (!context->connection) {
                context->connection = mms_ofono_connection_new(context, FALSE);
            }
        } else if (context->connection) {
            mms_ofono_context_drop_connection(context,
                MMS_CONNECTION_STATE_CLOSED);
        }
    } else {
        MMS_VERBOSE_("%s %s", context->path, key);
        MMS_ASSERT(strcmp(key, OFONO_CONTEXT_PROPERTY_TYPE));
    }
}

static
void
mms_ofono_context_activate_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer user_data)
{
    GError* error = NULL;
    MMSOfonoContext* context = user_data;
    gboolean ok = org_ofono_connection_context_call_set_property_finish(
        ORG_OFONO_CONNECTION_CONTEXT(proxy), result, &error);

    if (!ok) {
        MMSOfonoConnection* ofono = context->connection;
        if (ofono) {
            MMS_ERR("Connection %s failed: %s", ofono->connection.imsi,
                MMS_ERRMSG(error));
            if (ofono->connection.state == MMS_CONNECTION_STATE_OPENING) {
                mms_ofono_context_drop_connection(context,
                    MMS_CONNECTION_STATE_FAILED);
            }
        }
        g_error_free(error);
    }
}

static
void
mms_ofono_context_deactivate_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer user_data)
{
    GError* error = NULL;
    MMSOfonoContext* context = user_data;
    gboolean ok = org_ofono_connection_context_call_set_property_finish(
        ORG_OFONO_CONNECTION_CONTEXT(proxy), result, &error);

    if (!ok) {
        if (context->connection) {
            MMS_DEBUG("Connection %s failed tp deactivate: %s",
                context->connection->connection.imsi, MMS_ERRMSG(error));
        }
        g_error_free(error);
    }

    /* Regardless of whether or nor deactivation has succeeded, we mark this
     * connection as closed. */
    mms_ofono_context_drop_connection(context, MMS_CONNECTION_STATE_CLOSED);
}

void
mms_ofono_context_set_active(
    MMSOfonoContext* context,
    gboolean active)
{
    MMS_DEBUG("%s connection %s", active ? "Opening":"Closing",
        context->modem->imsi);
    if (context->set_active_cancel) {
        g_cancellable_cancel(context->set_active_cancel);
        g_object_unref(context->set_active_cancel);
    }
    context->set_active_cancel = g_cancellable_new();
    org_ofono_connection_context_call_set_property(context->proxy,
        OFONO_CONTEXT_PROPERTY_ACTIVE, g_variant_new_variant(
        g_variant_new_boolean(active)), context->set_active_cancel,
        active ? mms_ofono_context_activate_done :
        mms_ofono_context_deactivate_done, context);
}

MMSOfonoContext*
mms_ofono_context_new(
    MMSOfonoModem* modem,
    const char* path,
    GVariant* properties)
{
    GError* error = NULL;
    OrgOfonoConnectionContext* proxy;
    proxy = org_ofono_connection_context_proxy_new_sync(modem->bus,
        G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, path, NULL, &error);
    if (proxy) {
        MMSOfonoContext* context = g_new0(MMSOfonoContext, 1);
        GVariant* value = g_variant_lookup_value(
            properties, OFONO_CONTEXT_PROPERTY_ACTIVE,
            G_VARIANT_TYPE_BOOLEAN);
        if (value) {
            context->active = g_variant_get_boolean(value);
            g_variant_unref(value);
        }
        context->path = g_strdup(path);
        context->proxy = proxy;
        context->modem = modem;

        /* Subscribe for PropertyChanged notifications */
        context->property_change_signal_id = g_signal_connect(
            proxy, "property-changed",
            G_CALLBACK(mms_ofono_context_property_changed),
            context);

        return context;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
        return NULL;
    }
}

void
mms_ofono_context_free(
    MMSOfonoContext* context)
{
    if (context) {
        if (context->connection) {
            context->connection->context = NULL;
            mms_ofono_connection_cancel(context->connection);
            mms_ofono_connection_unref(context->connection);
        }
        if (context->set_active_cancel) {
            g_cancellable_cancel(context->set_active_cancel);
            g_object_unref(context->set_active_cancel);
        }
        if (context->proxy) {
            g_signal_handler_disconnect(context->proxy,
                context->property_change_signal_id);
            g_object_unref(context->proxy);
        }
        g_free(context->path);
        g_free(context);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
