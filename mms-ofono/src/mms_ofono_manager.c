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

#include "mms_ofono_manager.h"
#include "mms_ofono_modem.h"
#include "mms_ofono_names.h"
#include "mms_ofono_names.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_ofono_manager_log
#include "mms_ofono_log.h"
MMS_LOG_MODULE_DEFINE("mms-ofono-manager");

/* Generated headers */
#include "org.ofono.Manager.h"

struct mms_ofono_manager {
    GDBusConnection* bus;
    GHashTable* modems;
    OrgOfonoManager* proxy;
    gulong modem_added_signal_id;
    gulong modem_removed_signal_id;
};

static
void
mms_ofono_manager_set_modems(
    MMSOfonoManager* ofono,
    GVariant* modems)
{
    GVariantIter iter;
    GVariant* child;
    MMS_DEBUG("%u modem(s) found", (guint)g_variant_n_children(modems));
    g_hash_table_remove_all(ofono->modems);

    for (g_variant_iter_init(&iter, modems);
         (child = g_variant_iter_next_value(&iter)) != NULL;
         g_variant_unref(child)) {

        MMSOfonoModem* modem;
        const char* path = NULL;
        GVariant* properties = NULL;

        g_variant_get(child, "(&o@a{sv})", &path, &properties);
        MMS_ASSERT(path);
        MMS_ASSERT(properties);

        modem = mms_ofono_modem_new(ofono->bus, path, properties);
        if (modem) g_hash_table_replace(ofono->modems, modem->path, modem);
        g_variant_unref(properties);
    }
}

static
void
mms_ofono_manager_modem_added(
    OrgOfonoManager* proxy,
    const char* path,
    GVariant* properties,
    MMSOfonoManager* ofono)
{
    MMSOfonoModem* modem;
    MMS_VERBOSE_("%p %s", ofono, path);
    MMS_ASSERT(proxy == ofono->proxy);
    g_hash_table_remove(ofono->modems, path);
    modem = mms_ofono_modem_new(ofono->bus, path, properties);
    if (modem) g_hash_table_replace(ofono->modems, modem->path, modem);
}

static
void
mms_ofono_manager_modem_removed(
    OrgOfonoManager* proxy,
    const char* path,
    MMSOfonoManager* ofono)
{
    MMS_VERBOSE_("%p %s", ofono, path);
    MMS_ASSERT(proxy == ofono->proxy);
    g_hash_table_remove(ofono->modems, path);
}

static
void
mms_ofono_manager_hash_remove_modem(
    gpointer data)
{
    mms_ofono_modem_free(data);
}

MMSOfonoManager*
mms_ofono_manager_new(
    GDBusConnection* bus)
{
    GError* error = NULL;
    OrgOfonoManager* proxy = org_ofono_manager_proxy_new_sync(bus,
        G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, "/", NULL, &error);
    if (proxy) {
        GVariant* modems = NULL;
        if (org_ofono_manager_call_get_modems_sync(proxy, &modems,
            NULL, &error)) {

            MMSOfonoManager* ofono = g_new0(MMSOfonoManager, 1);
            ofono->proxy = proxy;
            g_object_ref(ofono->bus = bus);
            ofono->modems = g_hash_table_new_full(g_str_hash, g_str_equal,
                NULL, mms_ofono_manager_hash_remove_modem);

            /* Subscribe for ModemAdded/Removed notifications */
            ofono->modem_added_signal_id = g_signal_connect(
                proxy, "modem-added",
                G_CALLBACK(mms_ofono_manager_modem_added),
                ofono);
            ofono->modem_removed_signal_id = g_signal_connect(
                proxy, "modem-removed",
                G_CALLBACK(mms_ofono_manager_modem_removed),
                ofono);

            mms_ofono_manager_set_modems(ofono, modems);
            g_variant_unref(modems);
            return ofono;

        } else {
            MMS_ERR("Can't get list of modems: %s", MMS_ERRMSG(error));
            g_error_free(error);
        }
        g_object_unref(proxy);
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    return NULL;
}

static
gboolean
mms_ofono_manager_modem_imsi_find_cb(
    gpointer key,
    gpointer value,
    gpointer user_data)
{
    MMSOfonoModem* modem = value;
    const char* imsi = user_data;
    MMS_ASSERT(imsi);
    return modem->imsi && !strcmp(modem->imsi, imsi);
}

MMSOfonoModem*
mms_ofono_manager_default_modem(
    MMSOfonoManager* ofono)
{
    if (g_hash_table_size(ofono->modems) > 0) {
        GHashTableIter iter;
        gpointer key, value = NULL;
        g_hash_table_iter_init(&iter, ofono->modems);
        if (g_hash_table_iter_next(&iter, &key, &value)) {
            return value;
        }
    }
    return NULL;
}

MMSOfonoModem*
mms_ofono_manager_modem_for_imsi(
    MMSOfonoManager* ofono,
    const char* imsi)
{
    return ofono ? g_hash_table_find(ofono->modems,
        mms_ofono_manager_modem_imsi_find_cb, (void*)imsi) : NULL;
}

void
mms_ofono_manager_free(
    MMSOfonoManager* ofono)
{
    if (ofono) {
        if (ofono->proxy) {
            g_signal_handler_disconnect(ofono->proxy,
                ofono->modem_added_signal_id);
            g_signal_handler_disconnect(ofono->proxy,
                ofono->modem_removed_signal_id);
            g_object_unref(ofono->proxy);
        }
        g_hash_table_destroy(ofono->modems);
        g_object_unref(ofono->bus);
        g_free(ofono);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
