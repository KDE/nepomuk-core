/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2010 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _NEPOMUK2_RESOURCE_DATA_H_
#define _NEPOMUK2_RESOURCE_DATA_H_

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QAtomicInt>
#include <QtCore/QSet>

#include "variant.h"
#include <kurl.h>

#include <soprano/statement.h>


namespace Nepomuk2 {

    class Resource;
    class ResourceManagerPrivate;

    class ResourceData
    {
    public:
        explicit ResourceData( const QUrl& uri, const QUrl& kickOffUri, const QUrl& type_, ResourceManagerPrivate* rm );
        ~ResourceData();

        inline bool ref(Nepomuk2::Resource* res) {
            // Caller must lock ResourceManager mutex
            m_resources.push_back( res );
            return m_ref.ref();
        }

        inline bool deref(Nepomuk2::Resource* res) {
            // Caller must lock ResourceManager mutex
            m_resources.removeAll( res );
            return m_ref.deref();
        }

        QList<Resource*> resources() const {
            // Caller must lock ResourceManager mutex
            return m_resources;
        }

        inline int cnt() const {
            return m_ref;
        }

        /**
         * Tries to determine if this resource represents a file by examining the type and the uri.
         */
        bool isFile();

        /**
         * The URI of the resource. This might be empty if the resource was not synced yet.
         */
        QUrl uri() const;

        /**
         * \return The main type of the resource. ResourceData tries hard to make this the
         * most important type, i.e. that which is furthest down the hierachy.
         */
        QUrl type();

        QHash<QUrl, Variant> allProperties();

        bool hasProperty( const QUrl& uri );

        bool hasProperty( const QUrl& p, const Variant& v );

        Variant property( const QUrl& uri );

        /**
         * Set a property. The property will directly be saved to the RDF store.
         * Calls store to make sure this resource and property resources are properly
         * stored.
         */
        void setProperty( const QUrl& uri, const Variant& value );

        void addProperty( const QUrl& uri, const Variant& value );

        void removeProperty( const QUrl& uri );

        /**
         * Makes sure the resource is present in the RDF store. This means that if it does
         * not exist the type and the identifier (if one has been used to create the instance)
         * are stored.
         *
         * This is also the only place where a new URI is generated via ResourceManager::generateUniqueUri()
         * in case m_uri is empty.
         *
         * \sa exists, setProperty
         */
        bool store();

        bool load();

        /**
         * Remove this resource data from the store completely.
         * \param recursive If true all statements that contain this
         * resource as an object will be removed, too.
         */
        void remove( bool recursive = true );

        /**
         * This method only works with a proper URI, i.e. it does
         * not work on non-initialized resources that only know
         * their kickoffUriOrId
         */
        bool exists();

        bool isValid() const;

        /**
         * Searches for the resource in the Nepomuk store using m_kickoffId and m_kickoffUri.
         *
         * This will either get the actual resource URI from the database
         * and add m_data into ResourceManagerPrivate::m_initializedData
         * or it will find another ResourceData instance in m_initializedData
         * which represents the same resource. The ResourceData that should be
         * used is set in all the associated resources.
         */
        void determineUri();

        void invalidateCache();

        /**
         * Compares the properties of two ResourceData objects taking into account the Deleted flag
         */
        bool operator==( const ResourceData& other ) const;

        QDebug operator<<( QDebug dbg ) const;

        ResourceManagerPrivate* rm() const { return m_rm; }

        /// Updates ResourceManagerPrivate's list
        void updateKickOffLists( const QUrl& uri, const Variant& oldvariant, const Variant& newvariant );

        /// Called by ResourceManager (with the RM mutex locked)
        void propertyRemoved( const Types::Property &prop, const QVariant &value );
        void propertyAdded( const Types::Property &prop, const QVariant &value );

        void setWatchEnabled( bool status );
        bool watchEnabled();
    private:
        ResourceData(const ResourceData&); // = delete
        ResourceData& operator = (const ResourceData&); // = delete
        void updateUrlLists( const QUrl& oldUrl, const QUrl& newUrl );
        void updateIdentifierLists( const QString& oldIdentifier, const QString& newIdentifier );

        void addToWatcher();
        void removeFromWatcher();

        /// Will reset this instance to 0 as if constructed without parameters
        /// Used by remove() and deleteData()
        void resetAll();

        /// Contains a list of resources which use this ResourceData
        QList<Resource*> m_resources;

        /// final resource URI created by determineUri
        KUrl m_uri;

        /// the URL of file resources
        /// Only used before identification
        KUrl m_nieUrl;

        /// The nao:identifier of the tags
        /// Only used before identification
        QString m_naoIdentifier;

        /// Only used when creating a new Resource. It is not used in any other context
        QUrl m_type;

        QAtomicInt m_ref;

        // Protect m_cache, m_cacheDirty but also m_uri, m_nieUrl, m_naoIdentifier, m_addedToWatcher.
        // Never lock the ResourceManager mutex after locking this one. Always before (or not at all).
        mutable QMutex m_dataMutex;

        QHash<QUrl, Variant> m_cache;

        bool m_cacheDirty;
        bool m_addedToWatcher;
        bool m_watchEnabled;

        ResourceManagerPrivate* m_rm;
    };
}

QDebug operator<<( QDebug dbg, const Nepomuk2::ResourceData& );

#endif
