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

MMSConnMan*
mms_connman_test_new(void);

void
mms_connman_test_set_port(
    MMSConnMan* cm,
    unsigned short port);

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
