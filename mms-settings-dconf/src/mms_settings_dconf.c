/*
 * Copyright (C) 2014 Jolla Ltd.
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

#include "mms_settings_dconf.h"
#include <gio/gio.h>

/* Logging */
#define MMS_LOG_MODULE_NAME mms_settings_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-settings-dconf");

typedef MMSSettingsClass MMSSettingsDconfClass;
typedef struct mms_settings_dconf {
    MMSSettings settings;
    MMSSettingsSimDataCopy imsi_data;
    char* imsi;
    GSettings* gs;
    gulong gs_changed_signal_id;
} MMSSettingsDconf;

G_DEFINE_TYPE(MMSSettingsDconf, mms_settings_dconf, MMS_TYPE_SETTINGS);
#define MMS_TYPE_SETTINGS_DCONF mms_settings_dconf_get_type()
#define MMS_SETTINGS_DCONF_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
        MMS_TYPE_SETTINGS_DCONF, MMSSettingsDconfClass))
#define MMS_SETTINGS_DCONF(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        MMS_TYPE_SETTINGS_DCONF, MMSSettingsDconf))

#define MMS_DCONF_SCHEMA_ID         "org.nemomobile.mms.sim"
#define MMS_DCONF_CHANGED_SIGNAL    "changed"
#define MMS_DCONF_PATH_PREFIX       "/"

#define MMS_DCONF_KEY_USER_AGENT    "user-agent"
#define MMS_DCONF_KEY_UAPROF        "user-agent-profile"
#define MMS_DCONF_KEY_SIZE_LIMIT    "max-message-size"
#define MMS_DCONF_KEY_MAX_PIXELS    "max-pixels"
#define MMS_DCONF_KEY_ALLOW_DR      "allow-delivery-reports"

typedef struct mms_settings_dconf_key {
    const char* key;
    void (*fn_update)(MMSSettingsDconf* dconf, const char* key);
} MMSSettingsDconfKey;

static
void
mms_settings_dconf_update_user_agent(
    MMSSettingsDconf* dconf,
    const char* key)
{
    char* value = g_settings_get_string(dconf->gs, key);
    if (dconf->settings.flags & MMS_SETTINGS_FLAG_OVERRIDE_USER_AGENT) {
        MMS_DEBUG("%s = %s (ignored)", key, value);
        g_free(value);
    } else {
        MMSSettingsSimDataCopy* copy = &dconf->imsi_data;
        g_free(copy->user_agent);
        copy->data.user_agent = copy->user_agent = value;
        MMS_DEBUG("%s = %s", key, copy->data.user_agent);
    }
}

static
void
mms_settings_dconf_update_uaprof(
    MMSSettingsDconf* dconf,
    const char* key)
{
    char* value = g_settings_get_string(dconf->gs, key);
    if (dconf->settings.flags & MMS_SETTINGS_FLAG_OVERRIDE_UAPROF) {
        MMS_DEBUG("%s = %s (ignored)", key, value);
        g_free(value);
    } else {
        MMSSettingsSimDataCopy* copy = &dconf->imsi_data;
        g_free(copy->uaprof);
        copy->data.uaprof = copy->uaprof = value;
        MMS_DEBUG("%s = %s", key, copy->data.uaprof);
    }
}

static
void
mms_settings_dconf_update_size_limit(
    MMSSettingsDconf* dconf,
    const char* key)
{
    const guint value = g_settings_get_uint(dconf->gs, key);
    if (dconf->settings.flags & MMS_SETTINGS_FLAG_OVERRIDE_SIZE_LIMIT) {
        MMS_DEBUG("%s = %u (ignored)", key, value);
    } else {
        MMS_DEBUG("%s = %u", key, value);
        dconf->imsi_data.data.size_limit = value;
    }
}

static
void
mms_settings_dconf_update_max_pixels(
    MMSSettingsDconf* dconf,
    const char* key)
{
    const guint value = g_settings_get_uint(dconf->gs, key);
    if (dconf->settings.flags & MMS_SETTINGS_FLAG_OVERRIDE_MAX_PIXELS) {
        MMS_DEBUG("%s = %u (ignored)", key, value);
    } else {
        MMS_DEBUG("%s = %u", key, value);
        dconf->imsi_data.data.max_pixels = value;
    }
}

static
void
mms_settings_dconf_update_allow_dr(
    MMSSettingsDconf* dconf,
    const char* key)
{
    const gboolean value = g_settings_get_boolean(dconf->gs, key);
    if (dconf->settings.flags & MMS_SETTINGS_FLAG_OVERRIDE_ALLOW_DR) {
        MMS_DEBUG("%s = %s (ignored)", key, value ? "true" : "false");
    } else {
        MMS_DEBUG("%s = %s", key, value ? "true" : "false");
        dconf->imsi_data.data.allow_dr = value;
    }
}

static const MMSSettingsDconfKey mms_settings_dconf_keys[] = {
    { MMS_DCONF_KEY_USER_AGENT, mms_settings_dconf_update_user_agent },
    { MMS_DCONF_KEY_UAPROF,     mms_settings_dconf_update_uaprof     },
    { MMS_DCONF_KEY_SIZE_LIMIT, mms_settings_dconf_update_size_limit },
    { MMS_DCONF_KEY_MAX_PIXELS, mms_settings_dconf_update_max_pixels },
    { MMS_DCONF_KEY_ALLOW_DR,   mms_settings_dconf_update_allow_dr   },
};

static
void
mms_settings_dconf_changed(
    GSettings* gs,
    const gchar* key,
    gpointer user_data)
{
    unsigned int i;
    MMSSettingsDconf* dconf = user_data;
    MMS_ASSERT(dconf->gs == gs);
    for (i=0; i<G_N_ELEMENTS(mms_settings_dconf_keys); i++) {
        if (!strcmp(key, mms_settings_dconf_keys[i].key)) {
            mms_settings_dconf_keys[i].fn_update(dconf, key);
            return;
        }
    }
    MMS_DEBUG("%s changed", key);
}

static
void
mms_settings_dconf_disconnect(
    MMSSettingsDconf* dconf)
{
    if (dconf->imsi) {
        g_free(dconf->imsi);
        dconf->imsi = NULL;
    }
    if (dconf->gs) {
        if (dconf->gs_changed_signal_id) {
            g_signal_handler_disconnect(dconf->gs,
                dconf->gs_changed_signal_id);
            dconf->gs_changed_signal_id = 0;
        }
        g_object_unref(dconf->gs);
        dconf->gs = NULL;
    }
}

static
const MMSSettingsSimData*
mms_settings_dconf_get_sim_data(
    MMSSettings* settings,
    const char* imsi)
{
    MMSSettingsDconf* dconf = MMS_SETTINGS_DCONF(settings);
    if (imsi) {
        if (!dconf->imsi || strcmp(dconf->imsi, imsi)) {
            char* path = g_strconcat(MMS_DCONF_PATH_PREFIX, imsi, "/", NULL);
            mms_settings_dconf_disconnect(dconf);
            mms_settings_sim_data_copy(&dconf->imsi_data,
                &settings->sim_defaults.data);

            /* Attach to the new path */
            dconf->gs = g_settings_new_with_path(MMS_DCONF_SCHEMA_ID, path);
            if (dconf->gs) {
                unsigned int i;
                dconf->imsi = g_strdup(imsi);

                /* Query current settings */
                for (i=0; i<G_N_ELEMENTS(mms_settings_dconf_keys); i++) {
                    mms_settings_dconf_keys[i].fn_update(dconf,
                    mms_settings_dconf_keys[i].key);
                }

                /* And register for change notifications */
                dconf->gs_changed_signal_id = g_signal_connect(
                    dconf->gs, MMS_DCONF_CHANGED_SIGNAL,
                    G_CALLBACK(mms_settings_dconf_changed), dconf);
            }
            g_free(path);
        }
        return &dconf->imsi_data.data;
    } else {
        return &dconf->settings.sim_defaults.data;
    }
}

static
void
mms_settings_dconf_dispose(
    GObject* object)
{
    mms_settings_dconf_disconnect(MMS_SETTINGS_DCONF(object));
    G_OBJECT_CLASS(mms_settings_dconf_parent_class)->dispose(object);
}

static
void
mms_settings_dconf_finalize(
    GObject* object)
{
    MMSSettingsDconf* dconf = MMS_SETTINGS_DCONF(object);
    mms_settings_dconf_disconnect(dconf);
    mms_settings_sim_data_reset(&dconf->imsi_data);
    G_OBJECT_CLASS(mms_settings_dconf_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_settings_dconf_class_init(
    MMSSettingsClass* klass)
{
    klass->fn_get_sim_data = mms_settings_dconf_get_sim_data;
    G_OBJECT_CLASS(klass)->dispose = mms_settings_dconf_dispose;
    G_OBJECT_CLASS(klass)->finalize = mms_settings_dconf_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_settings_dconf_init(
    MMSSettingsDconf* dconf)
{
}

/**
 * Instantiates GSettings/Dconf settings implementation.
 */
MMSSettings*
mms_settings_dconf_new(
    const MMSConfig* config)
{
    MMSSettings* settings = g_object_new(MMS_TYPE_SETTINGS_DCONF, NULL);
    settings->config = config;
    return settings;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
