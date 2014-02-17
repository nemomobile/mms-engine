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

#include "mmspart.h"

MmsPart::MmsPart(QObject* parent) :
    QObject(parent)
{
}

MmsPart::MmsPart(const MmsPart& other) :
    QObject(other.parent()),
    m_fileName(other.m_fileName),
    m_contentType(other.m_contentType),
    m_contentId(other.m_contentId)
{
}

MmsPart& MmsPart::operator=(const MmsPart& other)
{
    m_fileName = other.m_fileName;
    m_contentType = other.m_contentType;
    m_contentId = other.m_contentId;
    return *this;
}

void MmsPart::marshall(QDBusArgument& arg) const
{
    arg.beginStructure();
    arg << m_fileName << m_contentType << m_contentId;
    arg.endStructure();
}

void MmsPart::demarshall(const QDBusArgument& arg)
{
    arg.beginStructure();
    arg >> m_fileName >> m_contentType >> m_contentId;
    arg.endStructure();
}

QDBusArgument& operator<<(QDBusArgument& arg, const MmsPart& part)
{
    part.marshall(arg);
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, MmsPart& part)
{
    part.demarshall(arg);
    return arg;
}
