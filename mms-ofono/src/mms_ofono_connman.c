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

#include "mms_ofono_connman.h"
#include "mms_ofono_connection.h"
#include "mms_ofono_manager.h"
#include "mms_ofono_modem.h"
#include "mms_ofono_names.h"
#include "mms_connection.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_connman_log
#include "mms_ofono_log.h"
MMS_LOG_MODULE_DEFINE("mms-ofono-connman");

typedef MMSConnManClass MMSOfonoConnManClass;
typedef struct mms_ofono_connman {
    GObject cm;
    guint ofono_watch_id;
    GDBusConnection* bus;
    MMSOfonoManager* man;
} MMSOfonoConnMan;

G_DEFINE_TYPE(MMSOfonoConnMan, mms_ofono_connman, MMS_TYPE_CONNMAN);
#define MMS_TYPE_OFONO_CONNMAN (mms_ofono_connman_get_type())
#define MMS_OFONO_CONNMAN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
    MMS_TYPE_OFONO_CONNMAN, MMSOfonoConnMan))

/**
 * Returns IMSI of the default SIM
 */
static
char*
mms_ofono_connman_default_imsi(
    MMSConnMan* cm)
{
    MMSOfonoConnMan* ofono = MMS_OFONO_CONNMAN(cm);
    if (ofono->man) {
        MMSOfonoModem* modem = mms_ofono_manager_default_modem(ofono->man);
        if (modem && modem->imsi) {
            return g_strdup(modem->imsi);
        }
    }
    return NULL;
}

/**
 * Creates a new connection or returns the reference to an aready active one.
 * The caller must release the reference.
 */
static
MMSConnection*
mms_ofono_connman_open_connection(
    MMSConnMan* cm,
    const char* imsi,
    gboolean user_request)
{
    MMSOfonoConnMan* ofono = MMS_OFONO_CONNMAN(cm);
    MMSOfonoModem* modem = mms_ofono_manager_modem_for_imsi(ofono->man, imsi);
    if (modem) {
        MMSOfonoContext* mms = modem->mms_context;
        if (mms) {
            if (!mms->connection) {
                mms->connection = mms_ofono_connection_new(mms, user_request);
            }
            if (mms->connection && !mms->active) {
                mms_ofono_context_set_active(mms, TRUE);
            }
            return mms_connection_ref(&mms->connection->connection);
        }
    } else {
        MMS_DEBUG("SIM %s is not avialable", imsi);
    }
    return NULL;
}

static
void
mms_connman_ofono_appeared(
    GDBusConnection* bus,
    const gchar* name,
    const gchar* owner,
    gpointer self)
{
    MMSOfonoConnMan* ofono = self;
    MMS_DEBUG("Name '%s' is owned by %s", name, owner);
    MMS_ASSERT(!ofono->man);
    mms_ofono_manager_free(ofono->man);
    ofono->man = mms_ofono_manager_new(bus);
}

static
void
mms_connman_ofono_vanished(
    GDBusConnection* bus,
    const gchar* name,
    gpointer self)
{
    MMSOfonoConnMan* ofono = self;
    MMS_DEBUG("Name '%s' has disappeared", name);
    mms_ofono_manager_free(ofono->man);
    ofono->man = NULL;
}

/**
 * Creates oFono connection manager
 */
MMSConnMan*
mms_connman_ofono_new()
{
    GError* error = NULL;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (bus) {
        MMSOfonoConnMan* ofono = g_object_new(MMS_TYPE_OFONO_CONNMAN, NULL);
        ofono->bus = bus;
        ofono->ofono_watch_id = g_bus_watch_name_on_connection(bus,
            OFONO_SERVICE, G_BUS_NAME_WATCHER_FLAGS_NONE,
            mms_connman_ofono_appeared, mms_connman_ofono_vanished,
            ofono, NULL);
        MMS_ASSERT(ofono->ofono_watch_id);
        return &ofono->cm;
    } else {
        MMS_ERR("Failed to connect to system bus: %s", MMS_ERRMSG(error));
        g_error_free(error);
        return NULL;
    }
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_ofono_connman_dispose(
    GObject* object)
{
    MMSOfonoConnMan* ofono = MMS_OFONO_CONNMAN(object);
    MMS_VERBOSE_("");
    if (ofono->ofono_watch_id) {
        g_bus_unwatch_name(ofono->ofono_watch_id);
        ofono->ofono_watch_id = 0;
    }
    if (ofono->man) {
        mms_ofono_manager_free(ofono->man);
        ofono->man = NULL;
    }
    if (ofono->bus) {
        g_object_unref(ofono->bus);
        ofono->bus = NULL;
    }
    G_OBJECT_CLASS(mms_ofono_connman_parent_class)->dispose(object);
}

/**
 * Per class initializer
 */
static
void
mms_ofono_connman_class_init(
    MMSOfonoConnManClass* klass)
{
    klass->fn_default_imsi = mms_ofono_connman_default_imsi;
    klass->fn_open_connection = mms_ofono_connman_open_connection;
    G_OBJECT_CLASS(klass)->dispose = mms_ofono_connman_dispose;
}

/**
 * Per instance initializer
 */
static
void
mms_ofono_connman_init(
    MMSOfonoConnMan* cm)
{
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
