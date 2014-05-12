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

#include "mms_settings.h"

G_DEFINE_TYPE(MMSSettings, mms_settings, G_TYPE_OBJECT);
#define MMS_SETTINGS_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MMS_TYPE_SETTINGS, MMSSettingsClass))
#define MMS_SETTINGS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MMS_TYPE_SETTINGS, MMSSettings))

MMSSettings*
mms_settings_ref(
    MMSSettings* s)
{
    if (s) g_object_ref(MMS_SETTINGS(s));
    return s;
}

void
mms_settings_unref(
    MMSSettings* s)
{
    if (s) g_object_unref(MMS_SETTINGS(s));
}

void
mms_settings_sim_data_default(
    MMSSettingsSimData* data)
{
    memset(data, 0, sizeof(*data));
    data->user_agent = MMS_SETTINGS_DEFAULT_USER_AGENT;
    data->size_limit = MMS_SETTINGS_DEFAULT_SIZE_LIMIT;
    data->max_pixels = MMS_SETTINGS_DEFAULT_MAX_PIXELS;
    data->allow_dr = MMS_SETTINGS_DEFAULT_ALLOW_DR;
}

static
void
mms_settings_sim_data_copy(
    MMSSettingsSimDataCopy* dest,
    const MMSSettingsSimData* src)
{
    g_free(dest->user_agent);
    if (src) {
        dest->data = *src;
        dest->data.user_agent = dest->user_agent = g_strdup(src->user_agent);
    } else {
        dest->user_agent = NULL;
        mms_settings_sim_data_default(&dest->data);
    }
}

MMSSettingsSimDataCopy*
mms_settings_sim_data_copy_new(
    const MMSSettingsSimData* data)
{
    MMSSettingsSimDataCopy* copy = NULL;
    if (data) {
        copy = g_new0(MMSSettingsSimDataCopy, 1);
        mms_settings_sim_data_copy(copy, data);
    }
    return copy;
}

void
mms_settings_sim_data_copy_free(
    MMSSettingsSimDataCopy* copy)
{
    if (copy) {
        g_free(copy->user_agent);
        g_free(copy);
    }
}

void
mms_settings_set_sim_defaults(
    MMSSettings* settings,
    const MMSSettingsSimData* data)
{
    if (settings) {
        mms_settings_sim_data_copy(&settings->sim_defaults, data);
    }
}

const MMSSettingsSimData*
mms_settings_get_sim_data(
    MMSSettings* settings,
    const char* imsi)
{
    if (settings) {
        MMSSettingsClass* klass = MMS_SETTINGS_GET_CLASS(settings);
        return klass->fn_get_sim_data(settings, imsi);
    }
    return NULL;
}

MMSSettings*
mms_settings_default_new(
    const MMSConfig* config)
{
    MMSSettings* settings = g_object_new(MMS_TYPE_SETTINGS, NULL);
    settings->config = config;
    return settings;
}

static
const MMSSettingsSimData*
mms_settings_get_default_sim_data(
    MMSSettings* settings,
    const char* imsi)
{
    return &settings->sim_defaults.data;
}

static
void
mms_settings_finalize(
    GObject* object)
{
    MMSSettings* settings = MMS_SETTINGS(object);
    g_free(settings->sim_defaults.user_agent);
    G_OBJECT_CLASS(mms_settings_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
mms_settings_class_init(
    MMSSettingsClass* klass)
{
    klass->fn_get_sim_data = mms_settings_get_default_sim_data;
    G_OBJECT_CLASS(klass)->finalize = mms_settings_finalize;
}

/**
 * Per instance initializer
 */
static
void
mms_settings_init(
    MMSSettings* settings)
{
    mms_settings_sim_data_default(&settings->sim_defaults.data);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
