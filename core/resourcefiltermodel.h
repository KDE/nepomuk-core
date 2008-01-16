/* 
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
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

#ifndef _NEPOMUK_RESOURCE_FILTER_MODEL_H_
#define _NEPOMUK_RESOURCE_FILTER_MODEL_H_

#include <Soprano/FilterModel>
#include <Soprano/Node>
#include <Soprano/Vocabulary/RDFS>

#include <QtCore/QList>

namespace Nepomuk {
    /**
     * Filter model that provides a set of convinience methods
     * for maintaining resource properties.
     *
     * It does automatic NRL named graph handling, i.e. provedance
     * data is created and deleted automatically.
     *
     * \warning This model assumes that no property value is stored twice,
     * i.e. in two different named graphs.
     */
    class ResourceFilterModel : public Soprano::FilterModel
    {
    public:
        ResourceFilterModel( Soprano::Model* model = 0 );
        ~ResourceFilterModel();

        /**
         * Update a property. This means an existing property is replaced if it differs from
         * the provided value. Otherwise nothing is done.
         *
         * This method assumes that the cardinality or property is 1.
         */
        Soprano::Error::ErrorCode updateProperty( const QUrl& resource, const QUrl& property, const Soprano::Node& value );

        /**
         * Update a property with a cardinality > 1.
         * This method optmizes the add and remove actions necessary.
         */
        Soprano::Error::ErrorCode updateProperty( const QUrl& resource, const QUrl& property, const QList<Soprano::Node>& values );

        /**
         * Remove a property from a resource and make sure no dangling graphs are left
         */
        Soprano::Error::ErrorCode removeProperty( const QUrl& resource, const QUrl& property );

        /**
         * Ensures that resoruce exists with type.
         */
        Soprano::Error::ErrorCode ensureResource( const QUrl& resource, const QUrl& type = Soprano::Vocabulary::RDFS::Resource() );

        /**
         * Reimplemented to remove metadata about graphs.
         */
        Soprano::Error::ErrorCode removeStatement( const Soprano::Statement &statement );

        /**
         * Reimplemented to remove metadata about graphs.
         */
        Soprano::Error::ErrorCode removeAllStatements( const Soprano::Statement &statement );

    private:
        Soprano::Error::ErrorCode removeGraphIfEmpty( const Soprano::Node& graph );

        class Private;
        Private* const d;
    };
}


uint qHash( const Soprano::Node& node );

#endif
