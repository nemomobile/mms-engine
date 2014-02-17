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

#include "mmshandler.h"
#include "mmsadaptor.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const char* rootDir = "/tmp/mms";
    if (argc > 1) rootDir = argv[1];
    qDebug() << "Using" << rootDir << "as storage for MMS files";
    MmsHandler* service = new MmsHandler(&app, rootDir);
    if (service->isRegistered()) {
        new MmsAdaptor(service);
        qDebug() << "MmsHandler created";
        return app.exec();
    } else {
        qCritical() << "MmsHandler registration failed (already running or DBus not found), exiting";
        return 1;
    }
}
