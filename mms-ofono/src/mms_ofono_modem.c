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

#include "mms_ofono_modem.h"
#include "mms_ofono_names.h"

/* Generated headers */
#include "org.ofono.Modem.h"
#include "org.ofono.SimManager.h"
#include "org.ofono.ConnectionManager.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_ofono_modem_log
#include "mms_ofono_log.h"
MMS_LOG_MODULE_DEFINE("mms-ofono-modem");

typedef struct mms_context_info {
    char* path;
    GVariant* properties;
} MMSContextInfo;

static
MMSContextInfo*
mms_context_info_new(
    const char* path,
    GVariant* properties)
{
    MMSContextInfo* info = g_new(MMSContextInfo, 1);
    info->path = g_strdup(path);
    g_variant_ref(info->properties = properties);
    return info;
}

void
mms_context_info_free(
    MMSContextInfo* info)
{
    if (info) {
        g_variant_unref(info->properties);
        g_free(info->path);
        g_free(info);
    }
}

static
void
mms_ofono_modem_disconnect_sim_proxy(
    MMSOfonoModem* modem)
{
    if (modem->sim_proxy) {
        g_signal_handler_disconnect(modem->sim_proxy,
            modem->sim_property_change_signal_id);
        g_object_unref(modem->sim_proxy);
        modem->sim_proxy = NULL;
    }
}

static
void
mms_ofono_modem_disconnect_gprs_proxy(
    MMSOfonoModem* modem)
{
    if (modem->gprs_proxy) {
        g_signal_handler_disconnect(modem->gprs_proxy,
            modem->gprs_context_added_signal_id);
        g_signal_handler_disconnect(modem->gprs_proxy,
            modem->gprs_context_removed_signal_id);
        g_object_unref(modem->gprs_proxy);
        modem->gprs_proxy = NULL;
    }
}

static
MMSContextInfo*
mms_ofono_modem_find_mms_context(
    OrgOfonoConnectionManager* proxy)
{
    GError* error = NULL;
    MMSContextInfo* mms_context = NULL;
    GVariant* contexts = NULL;
    if (org_ofono_connection_manager_call_get_contexts_sync(proxy,
        &contexts, NULL, &error)) {
        GVariantIter iter;
        GVariant* child;
        MMS_VERBOSE("  %d context(s)", (guint)g_variant_n_children(contexts));
        for (g_variant_iter_init(&iter, contexts);
             !mms_context && (child = g_variant_iter_next_value(&iter));
             g_variant_unref(child)) {

            const char* path = NULL;
            GVariant* properties = NULL;
            g_variant_get(child, "(&o@a{sv})", &path, &properties);
            if (properties) {
                GVariant* value = g_variant_lookup_value(properties,
                    OFONO_CONTEXT_PROPERTY_TYPE, G_VARIANT_TYPE_STRING);
                if (value) {
                    const char* type = g_variant_get_string(value, NULL);
                    if (path && type && !strcmp(type, OFONO_CONTEXT_TYPE_MMS)) {
                        mms_context = mms_context_info_new(path, properties);
                    }
                    g_variant_unref(value);
                }
                g_variant_unref(properties);
            }
        }
        g_variant_unref(contexts);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    return mms_context;
}

static
void
mms_ofono_modem_gprs_context_added(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    GVariant* properties,
    MMSOfonoModem* modem)
{
    GVariant* value = g_variant_lookup_value(properties,
        OFONO_CONTEXT_PROPERTY_TYPE, G_VARIANT_TYPE_STRING);
    const char* type = g_variant_get_string(value, NULL);
    MMS_VERBOSE_("%p %s", modem, path);
    MMS_ASSERT(proxy == modem->gprs_proxy);
    if (type && !strcmp(type, OFONO_CONTEXT_TYPE_MMS)) {
        MMS_DEBUG("MMS context %s created", path);
    }
    g_variant_unref(value);
}

static
void
mms_ofono_modem_gprs_context_removed(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    MMSOfonoModem* modem)
{
    MMS_VERBOSE_("%p %s", modem, path);
    MMS_ASSERT(proxy == modem->gprs_proxy);
    if (modem->mms_context && !g_strcmp0(modem->mms_context->path, path)) {
        MMS_DEBUG("MMS context %s removed", path);
        mms_ofono_context_free(modem->mms_context);
        modem->mms_context = NULL;
    }
}

static
char*
mms_ofono_modem_query_imsi(
    OrgOfonoSimManager* proxy)
{
    char* imsi = NULL;
    GError* error = NULL;
    GVariant* properties = NULL;
    if (org_ofono_sim_manager_call_get_properties_sync(proxy, &properties,
        NULL, &error)) {
        GVariant* imsi_value = g_variant_lookup_value(properties,
            OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY, G_VARIANT_TYPE_STRING);
        if (imsi_value) {
            imsi = g_strdup(g_variant_get_string(imsi_value, NULL));
            g_variant_unref(imsi_value);
        }
        g_variant_unref(properties);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    return imsi;
}

static
void
mms_ofono_modem_sim_property_changed(
    OrgOfonoSimManager* proxy,
    const char* key,
    GVariant* value,
    MMSOfonoModem* modem)
{
    MMS_VERBOSE_("%p %s", modem, key);
    MMS_ASSERT(proxy == modem->sim_proxy);
    if (!strcmp(key, OFONO_SIM_PROPERTY_SUBSCRIBER_IDENTITY)) {
        GVariant* variant = g_variant_get_variant(value);
        g_free(modem->imsi);
        modem->imsi = g_strdup(g_variant_get_string(variant, NULL));
        g_variant_unref(variant);
        MMS_VERBOSE("IMSI: %s", modem->imsi);
    }
}

static
void
mms_ofono_modem_scan_interfaces(
    MMSOfonoModem* m,
    GVariant* ifs)
{
    GError* error = NULL;
    gboolean sim_interface = FALSE;
    gboolean gprs_interface = FALSE;

    if (ifs) {
        GVariantIter iter;
        GVariant* child;
        for (g_variant_iter_init(&iter, ifs);
             (child = g_variant_iter_next_value(&iter)) &&
             (!sim_interface || !gprs_interface);
             g_variant_unref(child)) {
            const char* ifname = NULL;
            g_variant_get(child, "&s", &ifname);
            if (ifname) {
                if (!strcmp(ifname, OFONO_SIM_INTERFACE)) {
                    MMS_VERBOSE("  Found %s", ifname);
                    sim_interface = TRUE;
                } else if (!strcmp(ifname, OFONO_GPRS_INTERFACE)) {
                    MMS_VERBOSE("  Found %s", ifname);
                    gprs_interface = TRUE;
                }
            }
        }
    }

    /* org.ofono.SimManager */
    if (m->imsi) {
        g_free(m->imsi);
        m->imsi = NULL;
    }
    if (sim_interface) {
        if (!m->sim_proxy) {
            m->sim_proxy = org_ofono_sim_manager_proxy_new_sync(m->bus,
                G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, m->path, NULL, &error);
            if (m->sim_proxy) {
                /* Subscribe for PropertyChanged notifications */
                m->sim_property_change_signal_id = g_signal_connect(
                    m->sim_proxy, "property-changed",
                    G_CALLBACK(mms_ofono_modem_sim_property_changed),
                    m);
            } else {
                MMS_ERR("SimManager %s: %s", m->path, MMS_ERRMSG(error));
                g_error_free(error);
            }
        }
        if (m->sim_proxy) {
            m->imsi = mms_ofono_modem_query_imsi(m->sim_proxy);
            MMS_VERBOSE("IMSI: %s", m->imsi ? m->imsi : "");
        }
    } else if (m->sim_proxy) {
        mms_ofono_modem_disconnect_sim_proxy(m);
    }

    /* org.ofono.ConnectionManager */
    if (gprs_interface) {
        MMSContextInfo* context_info = NULL;
        if (!m->gprs_proxy) {
            m->gprs_proxy = org_ofono_connection_manager_proxy_new_sync(m->bus,
                G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, m->path, NULL, &error);
            if (m->gprs_proxy) {
                /* Subscribe for ContextAdded/Removed notifications */
                m->gprs_context_added_signal_id = g_signal_connect(
                    m->gprs_proxy, "context-added",
                    G_CALLBACK(mms_ofono_modem_gprs_context_added),
                    m);
                m->gprs_context_removed_signal_id = g_signal_connect(
                    m->gprs_proxy, "context-removed",
                    G_CALLBACK(mms_ofono_modem_gprs_context_removed),
                    m);
            } else {
                MMS_ERR("ConnectionManager %s: %s", m->path, MMS_ERRMSG(error));
                g_error_free(error);
            }
        }
        if (m->gprs_proxy) {
            context_info = mms_ofono_modem_find_mms_context(m->gprs_proxy);
        }
        if (context_info) {
            if (m->mms_context &&
                !g_strcmp0(context_info->path, m->mms_context->path)) {
                mms_ofono_context_free(m->mms_context);
                m->mms_context = NULL;
            }
            if (!m->mms_context) {
                m->mms_context = mms_ofono_context_new(m, context_info->path,
                    context_info->properties);
            }
            if (m->mms_context) {
                MMS_DEBUG("MMS context: %s (%sactive)", m->mms_context->path,
                    m->mms_context->active ? "" : "not ");
            }
            mms_context_info_free(context_info);
        } else {
            MMS_DEBUG("No MMS context");
            if (m->mms_context) {
                mms_ofono_context_free(m->mms_context);
                m->mms_context = NULL;
            }
        }
    } else if (m->gprs_proxy) {
        mms_ofono_modem_disconnect_gprs_proxy(m);
        if (m->mms_context) {
            MMS_DEBUG("No MMS context");
            mms_ofono_context_free(m->mms_context);
            m->mms_context = NULL;
        }
    }
}

static
void
mms_ofono_modem_property_changed(
    OrgOfonoModem* proxy,
    const char* key,
    GVariant* variant,
    MMSOfonoModem* modem)
{
    MMS_VERBOSE_("%p %s", modem, key);
    MMS_ASSERT(proxy == modem->proxy);
    if (!strcmp(key, OFONO_MODEM_PROPERTY_INTERFACES)) {
        GVariant* value = g_variant_get_variant(variant);
        mms_ofono_modem_scan_interfaces(modem, value);
        g_variant_unref(value);
    } else if (!strcmp(key, OFONO_MODEM_PROPERTY_ONLINE)) {
        GVariant* value = g_variant_get_variant(variant);
        modem->online = g_variant_get_boolean(value);
        MMS_DEBUG("Modem %s is %sline", modem->path,
            modem->online? "on" : "off");
        g_variant_unref(value);
    }
}

MMSOfonoModem*
mms_ofono_modem_new(
    GDBusConnection* bus,
    const char* path,
    GVariant* properties)
{
    GError* error = NULL;
    MMSOfonoModem* modem = NULL;
    OrgOfonoModem* proxy = org_ofono_modem_proxy_new_sync(bus,
        G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, path, NULL, &error);
    if (proxy) {
        GVariant* interfaces = g_variant_lookup_value(properties,
            OFONO_MODEM_PROPERTY_INTERFACES, G_VARIANT_TYPE_STRING_ARRAY);
        GVariant* online = g_variant_lookup_value(properties,
            OFONO_MODEM_PROPERTY_ONLINE, G_VARIANT_TYPE_BOOLEAN);
        modem = g_new0(MMSOfonoModem, 1);
        MMS_DEBUG("Modem path '%s'", path);
        MMS_VERBOSE_("%p '%s'", modem, path);
        modem->path = g_strdup(path);
        modem->proxy = proxy;
        g_object_ref(modem->bus = bus);

        /* Check what we currently have */
        mms_ofono_modem_scan_interfaces(modem, interfaces);
        g_variant_unref(interfaces);

        modem->online = g_variant_get_boolean(online);
        MMS_DEBUG("Modem %s is %sline", path, modem->online ? "on" : "off");
        g_variant_unref(online);

        /* Register to receive PropertyChanged notifications */
        modem->property_change_signal_id = g_signal_connect(
            proxy, "property-changed",
            G_CALLBACK(mms_ofono_modem_property_changed),
            modem);
    } else {
        MMS_ERR("%s: %s", path, MMS_ERRMSG(error));
        g_error_free(error);
    }
    return modem;
}

void
mms_ofono_modem_free(
    MMSOfonoModem* modem)
{
    if (modem) {
        MMS_VERBOSE_("%p '%s'", modem, modem->path);
        mms_ofono_modem_disconnect_sim_proxy(modem);
        mms_ofono_modem_disconnect_gprs_proxy(modem);
        mms_ofono_context_free(modem->mms_context);
        if (modem->proxy) {
            g_signal_handler_disconnect(modem->proxy,
                modem->property_change_signal_id);
            g_object_unref(modem->proxy);
        }
        g_object_unref(modem->bus);
        g_free(modem->path);
        g_free(modem->imsi);
        g_free(modem);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
