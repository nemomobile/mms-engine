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

#include "mms_connman.h"

G_DEFINE_TYPE(MMSConnMan, mms_connman, G_TYPE_OBJECT);
#define MMS_CONNMAN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
    MMS_TYPE_CONNMAN, MMSConnMan))
#define MMS_CONNMAN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
    MMS_TYPE_CONNMAN, MMSConnManClass))

MMSConnMan*
mms_connman_ref(
    MMSConnMan* cm)
{
    if (cm) g_object_ref(MMS_CONNMAN(cm));
    return cm;
}

void
mms_connman_unref(
    MMSConnMan* cm)
{
    if (cm) g_object_unref(MMS_CONNMAN(cm));
}

/**
 * Per class initializer
 */
static
void
mms_connman_class_init(
    MMSConnManClass* klass)
{
}

/**
 * Per instance initializer
 */
static
void
mms_connman_init(
    MMSConnMan* cm)
{
}

/**
 * Creates a new connection or returns the reference to an aready active one.
 * The caller must release the reference.
 */
MMSConnection*
mms_connman_open_connection(
    MMSConnMan* cm,
    const char* imsi,
    gboolean user_request)
{
    if (cm) {
        MMSConnManClass* klass = MMS_CONNMAN_GET_CLASS(cm);
        if (klass->fn_open_connection) {
            return klass->fn_open_connection(cm, imsi, user_request);
        }
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
