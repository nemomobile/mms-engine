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

#ifndef MMSPART_H
#define MMSPART_H

#include <QtDBus>
#include <QString>

class MmsPart : public QObject
{
    Q_OBJECT
public:
    explicit MmsPart(QObject* parent = 0);
    MmsPart(const MmsPart& other);
    MmsPart& operator=(const MmsPart& other);

    inline QString fileName() const { return m_fileName; }
    inline QString contentType() const { return m_contentType; }
    inline QString contentId() const { return m_contentId; }

    void marshall(QDBusArgument& arg) const;
    void demarshall(const QDBusArgument& arg);

private:
    QString m_fileName;
    QString m_contentType;
    QString m_contentId;
};

QDBusArgument& operator<<(QDBusArgument& arg, const MmsPart& part);
const QDBusArgument& operator>>(const QDBusArgument& arg, MmsPart& part);

typedef QList<MmsPart> MmsPartList;

Q_DECLARE_METATYPE(MmsPart);
Q_DECLARE_METATYPE(MmsPartList);

#endif // MMSPART_H
