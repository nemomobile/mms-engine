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

#ifndef MMSHANDLER_H
#define MMSHANDLER_H

#include "mmspart.h"
#include <QStringList>

class MmsHandler : public QObject
{
    Q_OBJECT
public:
    MmsHandler(QObject* parent, QString rootDir);

    bool isRegistered() const { return m_isRegistered; }

public Q_SLOTS:
    QString messageNotification(QString imsi, QString from, QString subject,
        uint expiry, QByteArray data);
    void messageReceiveStateChanged(QString recId, int state);
    void messageReceived(QString recId, QString mmsId, QString from,
        QStringList to, QStringList cc, QString subj, uint date, int priority,
        QString cls, bool readReport, MmsPartList parts);
    void messageSendStateChanged(QString recId, int state);
    void messageSent(QString recId, QString mmsId);

private:
    bool m_isRegistered;
    QString m_rootDir;
};

#endif // MMSHANDLER_H
