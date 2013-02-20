/* This file is part of the Nepomuk KDE Project
   Copyright (c) 2013 Gabriel Poesia <gabriel.poesia@gmail.com>
   Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include <QObject>
#include <QString>

class RegExpCache;

namespace Nepomuk2 {

    class FileSystemMonitor : public QObject
    {
        Q_OBJECT

    public:
        FileSystemMonitor();
        virtual ~FileSystemMonitor() = 0;

        /**
        * File system events that can occur. Use with addWatch
        * to define the events that should be watched.
        *
        * These flags correspond to the native Linux inotify flags.
        */
        enum WatchEvent {
            EventAccess = 0x00000001, /**< File was accessed (read, compare inotify's IN_ACCESS) */
            EventAttributeChange = 0x00000004, /**< Metadata changed (permissions, timestamps, extended attributes, etc., compare inotify's IN_ATTRIB) */
            EventCloseWrite = 0x00000008, /**< File opened for writing was closed (compare inotify's IN_CLOSE_WRITE) */
            EventCloseRead = 0x00000010, /**< File not opened for writing was closed (compare inotify's IN_CLOSE_NOWRITE) */
            EventCreate = 0x00000100, /** File/directory created in watched directory (compare inotify's IN_CREATE) */
            EventDelete = 0x00000200, /**< File/directory deleted from watched directory (compare inotify's IN_DELETE) */
            EventDeleteSelf = 0x00000400, /**< Watched file/directory was itself deleted (compare inotify's IN_DELETE_SELF) */
            EventModify = 0x00000002, /**< File was modified (compare inotify's IN_MODIFY) */
            EventMoveSelf = 0x00000800, /**< Watched file/directory was itself moved (compare inotify's IN_MOVE_SELF) */
            EventMoveFrom = 0x00000040, /**< File moved out of watched directory (compare inotify's IN_MOVED_FROM) */
            EventMoveTo = 0x00000080, /**< File moved into watched directory (compare inotify's IN_MOVED_TO) */
            EventOpen = 0x00000020, /**< File was opened (compare inotify's IN_OPEN) */
            EventUnmount = 0x00002000, /**< Backing fs was unmounted (compare inotify's IN_UNMOUNT) */
            EventQueueOverflow = 0x00004000, /**< Event queued overflowed (compare inotify's IN_Q_OVERFLOW) */
            EventIgnored = 0x00008000, /**< File was ignored (compare inotify's IN_IGNORED) */
            EventMove = ( EventMoveFrom|EventMoveTo),
            EventAll = ( EventAccess|
                        EventAttributeChange|
                        EventCloseWrite|
                        EventCloseRead|
                        EventCreate|
                        EventDelete|
                        EventDeleteSelf|
                        EventModify|
                        EventMoveSelf|
                        EventMoveFrom|
                        EventMoveTo|
                        EventOpen )
        };
        Q_DECLARE_FLAGS(WatchEvents, WatchEvent)

        /**
        * Watch flags
        *
        * These flags correspond to the native Linux inotify flags.
        */
        enum WatchFlag {
            FlagOnlyDir = 0x01000000, /**< Only watch the path if it is a directory (IN_ONLYDIR) */
            FlagDoNotFollow = 0x02000000, /**< Don't follow a sym link (IN_DONT_FOLLOW) */
            FlagOneShot = 0x80000000, /**< Only send event once (IN_ONESHOT) */
            FlagExclUnlink = 0x04000000 /**< Do not generate events for unlinked files (IN_EXCL_UNLINK) */
        };
        Q_DECLARE_FLAGS(WatchFlags, WatchFlag)

        /**
        * \return \p true if the monitor's back-end is available and usable.
        */
        virtual bool available() const = 0;

        virtual bool watchingPath( const QString& path ) const = 0;

        /**
        * Sets the filter list for watched paths. Filtered paths won't be watched.
        */
        void setFilterList( const RegExpCache* );

    public Q_SLOTS:
        virtual bool addWatch( const QString& path, WatchEvents modes, WatchFlags flags = WatchFlags() ) = 0;

        virtual bool removeWatch( const QString& path ) = 0;

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

    protected:
        bool filterWatch( const QString& path );

    private:

        const RegExpCache* m_pathExcludeRegExpCache;
    };
}

#endif
