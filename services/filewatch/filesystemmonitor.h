/* This file is part of the Nepomuk KDE Project
   Copyright (c) 2013 Gabriel Poesia <gabriel.poesia@gmail.com>

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

#ifndef _NEPOMUK_FILE_SYSTEM_MONITOR_H_
#define _NEPOMUK_FILE_SYSTEM_MONITOR_H_

#include <QString>
#include "nepomukservice.h"
#include "kinotify.h"

class RegExpCache;

namespace Nepomuk2 {   
    
    class FileSystemMonitor : public Service
    {
        Q_OBJECT

    public:
        FileSystemMonitor();
        virtual ~FileSystemMonitor() = 0;

        virtual void watchFolder( const QString& path ) = 0;
        void setFilterList( const RegExpCache* );

    Q_SIGNALS:
        /**
        * Emitted if a file is accessed
        */
        void accessed( const QString& file );

        /**
        * Emitted if file attributes are changed
        */
        void attributeChanged( const QString& file );

        /**
        * Emitted if file is closed after being opened in write mode
        */
        void closedWrite( const QString& file );

        /**
        * Emitted if file is closed after being opened in read mode
        */
        void closedRead( const QString& file );

        /**
        * Emitted if a new file has been created in one of the watched
        * folders
        */
        void created( const QString& file, bool isDir );

        /**
        * Emitted if a watched file or folder has been deleted.
        * This includes files in watched foldes
        */
        void deleted( const QString& file, bool isDir );

        /**
        * Emitted if a watched file is modified
        */
        void modified( const QString& file );

        /**
        * Emitted if a file or folder has been moved or renamed.
        *
        * \warning The moved signal will only be emitted if both the source and target folder
        * are being watched.
        */
        void moved( const QString& oldName, const QString& newName );

        /**
        * Emitted if a file is opened
        */
        void opened( const QString& file );

        /**
        * Emitted if a watched path has been unmounted
        */
        void unmounted( const QString& file );

        /**
        * Emitted if during updating the internal watch structures (recursive watches)
        * the back-end user watch limit was reached.
        *
        * This means that not all requested paths can be watched until the user watch
        * limit is increased.
        *
        * This signal will only be emitted once.
        */
        void watchUserLimitReached();

    private:
        bool filterWatch( const QString& path );

        const RegExpCache* m_pathExcludeRegExpCache;
    };
}

#endif
