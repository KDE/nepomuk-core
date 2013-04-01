/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef URI_H
#define URI_H

#include <QtCore/QUrl>

namespace Nepomuk2 {

    class Uri
    {
    public:
        Uri( const QUrl& uri );
        virtual ~Uri();

        QString toN3();
        QString prefix();
        QString suffix();

    private:
        void init(const QUrl& uri);

        QUrl m_uri;

        QString m_prefix;
        QString m_suffix;
        QString m_n3;
    };

}
#endif // URI_H
