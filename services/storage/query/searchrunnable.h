/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#ifndef _NEPOMUK_SEARCH_THREAD_H_
#define _NEPOMUK_SEARCH_THREAD_H_

#include <QtCore/QRunnable>
#include <QtCore/QPointer>
#include <QtCore/QMutex>

#include "query/result.h"

#include "folder.h"


namespace Soprano {
    class QueryResultIterator;
}

namespace Nepomuk2 {
    namespace Query {
        class Folder;

        class SearchRunnable : public QObject, public QRunnable
        {
            Q_OBJECT
        public:
            SearchRunnable( Soprano::Model* model, const QString& sparqlQuery, const RequestPropertyMap& map );
            ~SearchRunnable();

            /**
             * Cancel the search and detach it from the folder.
             */
            void cancel();

        Q_SIGNALS:
            void newResult( const Nepomuk2::Query::Result& result );
            void listingFinished();

        protected:
            void run();

        private:
            Soprano::Model* m_model;

            QString m_sparqlQuery;
            RequestPropertyMap m_requestPropertyMap;
            bool m_cancelled;
        };
    }
}

#endif
