/*
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>

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

#ifndef _NEPOMUK_VIRTUAL_FOLDER_H_
#define _NEPOMUK_VIRTUAL_FOLDER_H_

#include <QtCore/QObject>

#include "query/result.h"
#include "query/query.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtCore/QMutex>
#include <Soprano/Model>

#include <KUrl>

namespace Nepomuk2 {
    namespace Query {

        uint qHash( const Result& );

        class SearchRunnable;
        class CountQueryRunnable;
        class FolderConnection;

        /**
         * One search folder which automatically updates itself.
         * Once all connections to the folder have been deleted,
         * the folder deletes itself.
         */
        class Folder : public QObject
        {
            Q_OBJECT

        public:
            Folder( Soprano::Model* model, const Query& query, QObject* parent = 0 );
            Folder( Soprano::Model* model, const QString& sparqlQuery,
                    const RequestPropertyMap& requestProps, QObject* parent = 0 );
            ~Folder();

            bool isSparqlQueryFolder() const { return m_isSparqlQueryFolder; }

            /**
             * \return A list of all cached results in the folder.
             * If initial listing is not finished yet, the results found
             * so far are returned.
             */
            QList<Result> entries() const;

            /**
             * \return true if the initial listing is done, ie. further
             * signals only mean a change in the folder.
             */
            bool initialListingDone() const;

            QList<FolderConnection*> openConnections() const;

            Query query() const { return m_query; }

            QString sparqlQuery() const;

            RequestPropertyMap requestPropertyMap() const;

            /**
             * Get the result count. This value will not be available
             * right away since it is calculated async.
             *
             * As soon as the count is ready resultCount() is emitted.
             *
             * \return The result count or -1 in case it is not ready yet.
             */
            int getResultCount() const { return m_resultCount; }

        private Q_SLOTS:
            void addResult( const Nepomuk2::Query::Result& result );
            void listingFinished();

            void update();

            /// called by the CountQueryRunnable
            void countQueryFinished( int count );

        Q_SIGNALS:
            void newEntries( const QList<Nepomuk2::Query::Result>& entries );
            void entriesRemoved( const QList<QUrl>& entries );
            void entriesRemoved( const QList<Nepomuk2::Query::Result>& entries );

            /**
             * Emitted once the result count is available.
             *
             * \sa getResultCount()
             */
            void resultCount( int count );

            void finishedListing();
            void aboutToBeDeleted( Nepomuk2::Query::Folder* );

        private Q_SLOTS:
            void slotStorageChanged();
            void slotUpdateTimeout();

        private:
            void init();

            /**
             * Called by the FolderConnection constructor.
             */
            void addConnection( FolderConnection* );

            /**
             * Called by the FolderConnection destructor.
             */
            void removeConnection( FolderConnection* );

            /// true if this query has been created with a string query rather than a Query object
            const bool m_isSparqlQueryFolder;

            /// if valid m_sparqlQuery and m_requestProperties are ignored
            Query m_query;

            /// only used if m_query is not valid
            QString m_sparqlQuery;

            /// only used if m_query is not valid
            RequestPropertyMap m_requestProperties;

            /// Used for running the queries
            Soprano::Model* m_model;

            /// all listening connections
            QList<FolderConnection*> m_connections;

            /// the result count. -1 until determined
            int m_resultCount;

            /// true once the initial listing is done and only updates are to be signalled
            bool m_initialListingDone;

            /// the actual current results
            QHash<QUrl, Result> m_results;

            /// the results gathered during an update, needed to find removed items
            QHash<QUrl, Result> m_newResults;

            /// the runnable doing work at the moment or 0 if idle
            SearchRunnable* m_currentSearchRunnable;
            CountQueryRunnable* m_currentCountQueryRunnable;

            /// did the nepomuk store change after the last update - used for caching of update signals via m_updateTimer
            bool m_storageChanged;

            /// used to ensure that we do not update all the time if the storage changes a lot
            QTimer m_updateTimer;

            // for addConnection and removeConnection
            friend class FolderConnection;
        };
    }
}

#endif
