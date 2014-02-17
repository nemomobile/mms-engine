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

#include "test_connman.h"
#include "test_connection.h"

/* Logging */
#define MMS_LOG_MODULE_NAME mms_connman_log
#include "mms_lib_log.h"
MMS_LOG_MODULE_DEFINE("mms-connman-test");

typedef MMSConnManClass MMSConnManTestClass;
typedef struct mms_connman_test {
    MMSConnMan cm;
    MMSConnection* connection;
    unsigned short port;
} MMSConnManTest;

G_DEFINE_TYPE(MMSConnManTest, mms_connman_test, MMS_TYPE_CONNMAN);
#define MMS_TYPE_CONNMAN_TEST (mms_connman_test_get_type())
#define MMS_CONNMAN_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
    MMS_TYPE_CONNMAN_TEST, MMSConnManTest))

void
mms_connman_test_set_port(
    MMSConnMan* cm,
    unsigned short port)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(cm);
    test->port = port;
}

void
mms_connman_test_close_connection(
    MMSConnMan* cm)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(cm);
    if (test->connection) {
        MMS_DEBUG("Closing connection...");
        mms_connection_close(test->connection);
        mms_connection_unref(test->connection);
        test->connection = NULL;
    }
}

static
MMSConnection*
mms_connman_test_open_connection(
    MMSConnMan* cm,
    const char* imsi,
    gboolean user_request)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(cm);
    mms_connman_test_close_connection(cm);
    if (test->port) {
        test->connection = mms_connection_test_new(imsi, test->port);
        return mms_connection_ref(test->connection);
    } else {
        return NULL;
    }
}

static
void
mms_connman_test_dispose(
    GObject* object)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(object);
    mms_connman_test_close_connection(&test->cm);
}

static
void
mms_connman_test_class_init(
    MMSConnManTestClass* klass)
{
    klass->fn_open_connection = mms_connman_test_open_connection;
    G_OBJECT_CLASS(klass)->dispose = mms_connman_test_dispose;
}

static
void
mms_connman_test_init(
    MMSConnManTest* cm)
{
}

MMSConnMan*
mms_connman_test_new()
{
    return g_object_new(MMS_TYPE_CONNMAN_TEST,NULL);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
