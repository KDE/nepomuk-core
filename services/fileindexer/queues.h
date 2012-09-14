/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef NEPOMUK_FILE_INDEXER_QUEUES_H
#define NEPOMUK_FILE_INDEXER_QUEUES_H

#include "indexingqueue.h"

namespace Nepomuk2 {

    class FastIndexingQueue : public IndexingQueue {
        Q_OBJECT
    public:
        explicit FastIndexingQueue(QObject* parent = 0);

        virtual void indexDir(const QString& dir);
        virtual void indexFile(const QString& file);

        virtual bool shouldIndex(const QString& file);
        virtual bool shouldIndexContents(const QString& dir);
    private:
    };

    class SlowIndexingQueue : public IndexingQueue {
        Q_OBJECT
    public:
        explicit SlowIndexingQueue(QObject* parent = 0);

        virtual void indexDir(const QString& dir) {}
        virtual void indexFile(const QString& file);

        virtual bool shouldIndex(const QString&) { return true; }
        virtual bool shouldIndexContents(const QString&) { return true; }
    };
}

#endif // NEPOMUK_FILE_INDEXER_QUEUES_H
