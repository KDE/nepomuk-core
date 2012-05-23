/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2008 Sebastian Trueg <trueg@kde.org>
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

#include "tools.h"
#include "resourcemanager.h"

#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>

#include <QtCore/QLocale>

#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>



void Nepomuk2::setDefaultRepository( const QString& )
{
    // deprecated - do nothing
}


QString Nepomuk2::defaultGraph()
{
    return QLatin1String("main");
}


QString Nepomuk2::typePredicate()
{
    return Soprano::Vocabulary::RDF::type().toString();
}


Soprano::Node Nepomuk2::valueToRDFNode( const Nepomuk2::Variant& v )
{
    return v.toNode();
}


QList<Soprano::Node> Nepomuk2::valuesToRDFNodes( const Nepomuk2::Variant& v )
{
    return v.toNodeList();
}


Nepomuk2::Variant Nepomuk2::RDFLiteralToValue( const Soprano::Node& node )
{
    return Variant::fromNode( node );
}


QString Nepomuk2::rdfNamepace()
{
    return Soprano::Vocabulary::RDF::rdfNamespace().toString();
}


QString Nepomuk2::rdfsNamespace()
{
    return Soprano::Vocabulary::RDFS::rdfsNamespace().toString();
}


QString Nepomuk2::nrlNamespace()
{
    return Soprano::Vocabulary::NRL::nrlNamespace().toString();
}


QString Nepomuk2::naoNamespace()
{
    return Soprano::Vocabulary::NAO::naoNamespace().toString();
}
