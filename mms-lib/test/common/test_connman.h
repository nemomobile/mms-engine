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

#ifndef TEST_CONNMAN_H
#define TEST_CONNMAN_H

#include "mms_connman.h"

typedef
void
(*mms_connman_test_connect_fn)(
    void* param);

MMSConnMan*
mms_connman_test_new(void);

void
mms_connman_test_set_port(
    MMSConnMan* cm,
    unsigned short port,
    gboolean proxy);

void
mms_connman_test_set_offline(
    MMSConnMan* cm,
    gboolean offline);

void
mms_connman_test_set_default_imsi(
    MMSConnMan* cm,
    const char* imsi);

void
mms_connman_test_set_connect_callback(
    MMSConnMan* cm,
    mms_connman_test_connect_fn fn,
    void* param);

void
mms_connman_test_close_connection(
    MMSConnMan* cm);

#endif /* TEST_CONNMAN_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
