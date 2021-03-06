/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "datamanagementcommand.h"
#include "datamanagementmodel.h"

#include <Soprano/Error/Error>
#include <Soprano/Error/ErrorCode>

#include <kdbusconnectionpool.h>

#include <QtCore/QStringList>
#include <QtCore/QEventLoop>

#include <KUrl>


namespace {
QDBusError::ErrorType convertSopranoErrorCode(int code)
{
    switch(code) {
    case Soprano::Error::ErrorInvalidArgument:
        return QDBusError::InvalidArgs;
    default:
        return QDBusError::Failed;
    }
}
}


Nepomuk2::DataManagementCommand::DataManagementCommand(DataManagementModel* model, const QDBusMessage& msg)
    : QRunnable(),
      m_model(model),
      m_msg(msg)
{
}

Nepomuk2::DataManagementCommand::~DataManagementCommand()
{
}

void Nepomuk2::DataManagementCommand::run()
{
    QVariant result = runCommand();
    Soprano::Error::Error error = model()->lastError();
    QDBusConnection con = KDBusConnectionPool::threadConnection();
    if(error) {
        // send error reply
        con.send(m_msg.createErrorReply(convertSopranoErrorCode(error.code()), error.message()));
    }
    else {
        // encode result (ie. convert QUrl to QString)
        if(result.isValid()) {
            if(result.type() == QVariant::Url) {
                result = encodeUrl(result.toUrl());
            }
            con.send(m_msg.createReply(result));
        }
        else {
            con.send(m_msg.createReply());
        }
    }

    //
    // DBus requires event handling for signals to be emitted properly.
    // (for example the Soprano statement signals which are emitted a
    // lot during command execution.)
    // Otherwise memory will fill up with queued DBus message objects.
    // Instead of executing an event loop we avoid all the hassle and
    // simply handle all events here.
    //
    QEventLoop loop;
    loop.processEvents();
}


// static
QUrl Nepomuk2::decodeUrl(const QString& urlsString)
{
    // we use the power of KUrl to automatically convert file paths to file:/ URLs
    return KUrl(urlsString);
}

// static
QList<QUrl> Nepomuk2::decodeUrls(const QStringList& urlStrings)
{
    QList<QUrl> urls;
    Q_FOREACH(const QString& urlString, urlStrings) {
        urls << decodeUrl(urlString);
    }
    return urls;
}

// static
QString Nepomuk2::encodeUrl(const QUrl& url)
{
    return QString::fromAscii(url.toEncoded());
}
