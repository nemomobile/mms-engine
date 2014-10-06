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
    mms_connman_connection_requested_cb connection_requested_cb;
    void* connection_requested_param;
    gboolean fail;
} MMSConnManTest;

G_DEFINE_TYPE(MMSConnManTest, mms_connman_test, MMS_TYPE_CONNMAN);
#define MMS_TYPE_CONNMAN_TEST (mms_connman_test_get_type())
#define MMS_CONNMAN_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
    MMS_TYPE_CONNMAN_TEST, MMSConnManTest))

void
mms_connman_set_connection_requested_cb(
    MMSConnMan* cm,
    mms_connman_connection_requested_cb cb,
    void* param)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(cm);
    test->connection_requested_cb = cb;
    test->connection_requested_param = param;
}

MMSConnection*
mms_connman_test_open_connection(
    MMSConnMan* cm,
    const char* imsi,
    gboolean user_request)
{
    MMSConnManTest* test = MMS_CONNMAN_TEST(cm);
    if (test->fail) {
        if (test->connection_requested_cb) {
            test->connection_requested_cb(cm,
            test->connection_requested_param);
        }
        return NULL;
    } else {
        test->fail = TRUE;
        return mms_connection_test_new(imsi, 0, FALSE);
    }
}

static
void
mms_connman_test_class_init(
    MMSConnManTestClass* klass)
{
    klass->fn_open_connection = mms_connman_test_open_connection;
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
