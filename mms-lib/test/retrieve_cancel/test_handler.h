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

#ifndef TEST_HANDLER_H
#define TEST_HANDLER_H

#include "mms_handler.h"

MMSHandler*
mms_handler_test_new(void);

typedef
char*
(*mms_handler_message_id_cb)(
    MMSHandler* handler,
    void* param);

void
mms_handler_set_message_id_cb(
    MMSHandler* handler,
    mms_handler_message_id_cb cb,
    void* param);

#endif /* TEST_HANDLER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
