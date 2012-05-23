/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2008-2010 Sebastian Trueg <trueg@kde.org>

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

#include "result.h"
#include "variant.h"

#include <QtCore/QSharedData>

#include "resource.h"
#include "property.h"

#include <Soprano/Node> // for qHash( QUrl() )
#include <Soprano/BindingSet>


class Nepomuk2::Query::Result::Private : public QSharedData
{
public:
    Resource resource;
    double score;
    QHash<Types::Property, Soprano::Node> requestProperties;
    Soprano::BindingSet additionalBindings;
    QString excerpt;
};


Nepomuk2::Query::Result::Result()
    : d( new Private() )
{
}


Nepomuk2::Query::Result::Result( const Resource& res, double score )
    : d( new Private() )
{
    d->resource = res;
    d->score = score;
}


Nepomuk2::Query::Result::Result( const Result& other )
{
    d = other.d;
}


Nepomuk2::Query::Result::~Result()
{
}


Nepomuk2::Query::Result& Nepomuk2::Query::Result::operator=( const Result& other )
{
    d = other.d;
    return *this;
}


double Nepomuk2::Query::Result::score() const
{
    return d->score;
}


Nepomuk2::Resource Nepomuk2::Query::Result::resource() const
{
    return d->resource;
}


void Nepomuk2::Query::Result::setScore( double score )
{
    d->score = score;
}


void Nepomuk2::Query::Result::addRequestProperty( const Nepomuk2::Types::Property& property, const Soprano::Node& value )
{
    d->requestProperties[property] = value;
}


Soprano::Node Nepomuk2::Query::Result::operator[]( const Nepomuk2::Types::Property& property ) const
{
    return requestProperty( property );
}


Soprano::Node Nepomuk2::Query::Result::requestProperty( const Nepomuk2::Types::Property& property ) const
{
    QHash<Nepomuk2::Types::Property, Soprano::Node>::const_iterator it = d->requestProperties.find( property );
    if ( it != d->requestProperties.end() ) {
        return *it;
    }
    else {
        return Soprano::Node();
    }
}


QHash<Nepomuk2::Types::Property, Soprano::Node> Nepomuk2::Query::Result::requestProperties() const
{
    return d->requestProperties;
}


void Nepomuk2::Query::Result::setAdditionalBindings( const Soprano::BindingSet& bindings )
{
    d->additionalBindings = bindings;
}


Soprano::BindingSet Nepomuk2::Query::Result::additionalBindings() const
{
    return d->additionalBindings;
}


Nepomuk2::Variant Nepomuk2::Query::Result::additionalBinding( const QString& name ) const
{
    return Variant::fromNode( d->additionalBindings.value(name) );
}


void Nepomuk2::Query::Result::setExcerpt( const QString& text )
{
    d->excerpt = text;
}


QString Nepomuk2::Query::Result::excerpt() const
{
    return d->excerpt;
}


bool Nepomuk2::Query::Result::operator==( const Result& other ) const
{
    if ( d->resource != other.d->resource ||
         d->score != other.d->score ) {
        return false;
    }
    for ( QHash<Types::Property, Soprano::Node>::const_iterator it = d->requestProperties.constBegin();
          it != d->requestProperties.constEnd(); ++it ) {
        QHash<Types::Property, Soprano::Node>::const_iterator it2 = other.d->requestProperties.constFind( it.key() );
        if ( it2 == other.d->requestProperties.constEnd() ||
             it2.value() != it.value() ) {
            return false;
        }
    }
    for ( QHash<Types::Property, Soprano::Node>::const_iterator it = other.d->requestProperties.constBegin();
          it != other.d->requestProperties.constEnd(); ++it ) {
        QHash<Types::Property, Soprano::Node>::const_iterator it2 = d->requestProperties.constFind( it.key() );
        if ( it2 == d->requestProperties.constEnd() ||
             it2.value() != it.value() ) {
            return false;
        }
    }
    return d->additionalBindings == other.d->additionalBindings;
}
