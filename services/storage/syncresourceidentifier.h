/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

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


#ifndef NEPOMUK_RESOURCEIDENTIFIER_H
#define NEPOMUK_RESOURCEIDENTIFIER_H

#include <QtCore/QMultiHash>
#include <QtCore/QSet>
#include <KUrl>

#include "syncresource.h"

namespace Soprano {
    class Statement;
    class Graph;
    class Model;
}

namespace Nepomuk2 {

    class Resource;

    namespace Sync {

        class SyncResource;
        class ResourceHash;

        /**
         * \class ResourceIdentifier resourceidentifier.h
         *
         * This class is used to identify already existing resources from a set of
         * properties and objects. It identifies the resources on the basis of the
         * identifying statements provided.
         *
         * \author Vishesh Handa <handa.vish@gmail.com>
         */
        class ResourceIdentifier
        {
        public:
            ResourceIdentifier( Soprano::Model * model );
            virtual ~ResourceIdentifier();

            //
            // Processing
            //
            void identifyAll();

            bool identify( const KUrl & uri );

            /**
             * Identifies all the resources present in the \p uriList.
             */
            void identify( const KUrl::List & uriList );

            /**
             * This returns true if ALL the external ResourceUris have been identified.
             * If this is false, you should manually identify some of the resources by
             * providing the resource.
             */
            bool allIdentified() const;

            virtual void addStatement( const Soprano::Statement & st );
            virtual void addStatements( const Soprano::Graph& graph );
            virtual void addStatements( const QList<Soprano::Statement> & stList );
            virtual void addSyncResource( const SyncResource & res );

            //
            // Getting the info
            //
            /**
             * Returns the detected uri for the given resourceUri.
             * This method useful only after identifyAll() method was called
             */
            KUrl mappedUri( const KUrl & resourceUri ) const;

            KUrl::List mappedUris() const;

            /**
             * Returns mappings of the identified uri
             */
            QHash<QUrl, QUrl> mappings() const;

            /**
             * Returns urls that were not successfully identified
             */
            QSet<KUrl> unidentified() const;

            QSet< QUrl > identified() const;

            /**
             * Returns all the statements that are being used to identify \p uri
             */
            Soprano::Graph statements( const KUrl & uri );

            QList<Soprano::Statement> identifyingStatements() const;

            SyncResource simpleResource( const KUrl & uri );

            ResourceHash resourceHash() const;

            virtual bool isIdentifyingProperty( const QUrl & uri );

        protected:
            /**
            * Called during identification if there is more than one match for one resource.
            *
            * The default behavior is to return an empty uri, which depicts identification failure
            */
            virtual KUrl duplicateMatch( const KUrl & uri, const QSet<KUrl> & matchedUris );

            /**
             * This function returns true if identification was successful, and false if it was not.
             * If you need to customize the identification process, you will need to overload this
             * function.
             */
            virtual bool runIdentification( const KUrl& uri );

            /**
             * Sets oldUri -> newUri in the mappings.
             * This is useful when runIdentification has been reimplemented.
             */
            void manualIdentification( const KUrl & oldUri, const KUrl & newUri );

        protected:
            Soprano::Model * m_model;

            /**
             * The main identification hash which maps external ResourceUris
             * with the internal ones
             */
            QHash<QUrl, QUrl> m_hash;

            QSet<KUrl> m_notIdentified;

            /// Used to store all the identification statements
            ResourceHash m_resourceHash;

            /**
             * This contains all the urls that are being identified, at any moment.
             * It is used to avoid infinite recursion while generating the sparql
             * query.
             */
            QSet<KUrl> m_beingIdentified;
        };
    }
}

#endif // NEPOMUK_RESOURCEIDENTIFIER_H
