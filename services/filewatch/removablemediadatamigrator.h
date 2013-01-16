/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2013  Vishesh Handa <me@vhanda.in>
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

#ifndef REMOVABLEMEDIADATAMIGRATOR_H
#define REMOVABLEMEDIADATAMIGRATOR_H

#include <QtCore/QRunnable>

#include "removablemediacache.h"

namespace Nepomuk2 {
    /**
     * This class migrates all the filex data from a given entry to the
     * normal file:/ based url scheme.
     *
     */
    class RemovableMediaDataMigrator : public QRunnable
    {
    public:
        RemovableMediaDataMigrator( const RemovableMediaCache::Entry* entry );

        virtual void run();

    private:
        QString m_uuid;
        QString m_mountPoint;
    };

}

#endif // REMOVABLEMEDIADATAMIGRATOR_H
