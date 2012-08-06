/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2008-2009 Sebastian Trueg <trueg@kde.org>
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

#include "thing.h"
#include "pimo.h"
#include "variant.h"

Nepomuk2::Thing::Thing( const QUrl& uri, const QUrl& pimoType )
    : Resource( uri, pimoType.isEmpty() ? Vocabulary::PIMO::Thing() : pimoType )
{
    // FIXME: somehow ensure that pimoType is actually a pimo:Thing. Maybe
    // use operator= instead of the contructor and check the value beforehand
}


Nepomuk2::Thing::Thing( const QString& uriOrName, const QUrl& pimoType )
    : Resource( uriOrName, pimoType.isEmpty() ? Vocabulary::PIMO::Thing() : pimoType )
{
    // FIXME: s.o.
}


Nepomuk2::Thing::Thing( const Thing& other )
    : Resource( other )
{
    // FIXME: s.o.
}


Nepomuk2::Thing::Thing( const Resource& other )
    : Resource( other )
{
    // FIXME: s.o.
}


Nepomuk2::Thing::Thing( Nepomuk2::ResourceData* data )
    : Resource( data )
{
}


Nepomuk2::Thing& Nepomuk2::Thing::operator=( const Thing& res )
{
    Resource::operator=( res );
    return *this;
}


Nepomuk2::Thing& Nepomuk2::Thing::operator=( const Resource& res )
{
    Resource::operator=( res );
    return *this;
}


Nepomuk2::Thing& Nepomuk2::Thing::operator=( const QUrl& res )
{
    Resource::operator=( res );
    return *this;
}


Nepomuk2::Thing::~Thing()
{
}


QList<Nepomuk2::Resource> Nepomuk2::Thing::groundingOccurrences() const
{
    return property( Vocabulary::PIMO::groundingOccurrence() ).toResourceList();
}


QList<Nepomuk2::Resource> Nepomuk2::Thing::referencingOccurrences() const
{
    return property( Vocabulary::PIMO::referencingOccurrence() ).toResourceList();
}


QList<Nepomuk2::Resource> Nepomuk2::Thing::occurrences() const
{
    return property( Vocabulary::PIMO::occurrence() ).toResourceList();
}


void Nepomuk2::Thing::addGroundingOccurrence( const Nepomuk2::Resource &res )
{
    addProperty(Nepomuk2::Vocabulary::PIMO::groundingOccurrence(), res);
}


// void Nepomuk2::Thing::merge( Thing other )
// {
// }