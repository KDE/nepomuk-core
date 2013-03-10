/* This file is part of the KDE Project
   Copyright (c) 2008-2011 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2013 Vishesh Handa <me@vhanda.in>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _NEPOMUK_SERVICE_CONTROL2_H_
#define _NEPOMUK_SERVICE_CONTROL2_H_

#include <QtCore/QObject>

namespace Nepomuk2 {
    class Service2;

    class ServiceControl2 : public QObject
    {
        Q_OBJECT

    public:
        ServiceControl2( Service2* service );
        ~ServiceControl2();

        bool failedToStart();

    Q_SIGNALS:
        void serviceInitialized( bool success );

    public Q_SLOTS:
        void setServiceInitialized( bool success );
        bool isInitialized() const;
        void shutdown();
        QString name() const;
        QString description() const;

    private:
        Service2 *m_service;
        bool m_initialized;
        bool m_failed;
    };
}

#endif
