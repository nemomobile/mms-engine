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

MmsHandler::MmsHandler(QObject* parent, QString rootDir) :
    QObject(parent),
    m_isRegistered(false),
    m_rootDir(rootDir)
{
    qDBusRegisterMetaType<MmsPart>();
    qDBusRegisterMetaType<MmsPartList>();
    QDBusConnection dbus = QDBusConnection::systemBus();
    if (dbus.isConnected()) {
        if (dbus.registerObject("/", this)) {
            if (dbus.registerService("org.nemomobile.MmsHandler")) {
                m_isRegistered = true;
            } else {
                qWarning() << "Unable to register service!" << dbus.lastError();
            }
        } else {
            qWarning() << "Object registration failed!" << dbus.lastError();
        }
    } else {
        qCritical() << "ERROR: No DBus session bus found!";
    }
}

QString MmsHandler::messageNotification(QString imsi, QString from,
    QString subject, uint expiry, QByteArray data)
{
    qDebug() << "messageNotification" << imsi << from << subject << expiry
             << data.size() << "bytes";
    for (int i=1; i<=1000; i++) {
        QString id = QString::number(i);
        QDir dir(m_rootDir + "/" + id);
        if (!dir.exists() && dir.mkpath(dir.path())) {
            QString path(dir.filePath("pdu"));
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                qDebug() << "Record id" << id;
                return id;
            } else {
                qWarning() << "messageNotification failed to create" << path;
            }
        }
    }
    qWarning() << "messageNotification failed to generate message id";
    return QString();
}

void MmsHandler::messageReceiveStateChanged(QString recId, int state)
{
    qDebug() << "messageReceiveStateChanged" << recId << state;
}

void MmsHandler::messageSendStateChanged(QString recId, int state)
{
    qDebug() << "messageSendStateChanged" << recId << state;
}

void MmsHandler::messageSent(QString recId, QString mmsId)
{
    qDebug() << "messageSent" << recId << mmsId;
}

void MmsHandler::messageReceived(QString recId, QString mmsId, QString from,
    QStringList to, QStringList cc, QString subj, uint date, int priority,
    QString cls, bool readReport, MmsPartList parts)
{
    QDir dir(m_rootDir + "/" + recId);
    if (dir.exists()) {
        qDebug() << "messageReceived" << recId << mmsId << from
                 << subj << date << priority << cls << readReport;
        foreach (QString addr, to) {
            qDebug() << "  To:" << addr;
        }
        foreach (QString addr, cc) {
            qDebug() << "  Cc:" << addr;
        }

        qDebug() << parts.size() << "parts";
        QDir partDir(dir.filePath("parts"));
        if (partDir.mkpath(partDir.path())) {
            foreach (MmsPart part, parts) {
                QFileInfo partInfo(part.fileName());
                if (partInfo.isFile()) {
                    QFile partFile(partInfo.canonicalFilePath());
                    qDebug() << "  " << part.contentId() << part.contentType()
                             << partInfo.canonicalFilePath();
                    QString destName(partDir.filePath(partInfo.fileName()));
                    if (!partFile.copy(destName)) {
                        qWarning() << "Failed to copy" << partFile.fileName()
                                   << "->" << destName;
                    }
                } else {
                    qDebug() << "  " << part.contentId() << part.contentType()
                             << part.fileName() << "(no such file)";
                }
            }
        }
    } else {
        qWarning() << "Invalid record id" << recId;
    }
}
