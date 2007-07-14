/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "tools.h"
#include "resourcemanager.h"

#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>

#include <QtCore/QLocale>

static const char* RDF_NAMESPACE = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
static const char* RDFS_NAMESPACE = "http://www.w3.org/2000/01/rdf-schema#";
static const char* NRL_NAMESPACE = "http://semanticdesktop.org/ontologies/2006/11/24/nrl#";
static const char* NAO_NAMESPACE = "http://semanticdesktop.org/ontologies/2007/03/31/nao#";

static QString s_customRep;


static QString getLocaleLang()
{
    if( KGlobal::locale() )
        return KGlobal::locale()->language();
    else
        return QLocale::system().name(); // FIXME: does this make sense?
}


void Nepomuk::setDefaultRepository( const QString& s )
{
    s_customRep = s;
}


QString Nepomuk::defaultGraph()
{
    static QString s = "main";
    if( s_customRep.isEmpty() )
        return s;
    else
        return s_customRep;
}


QString Nepomuk::typePredicate()
{
    static QString s = RDF_NAMESPACE + QString( "type" );
    return s;
}


Soprano::Node Nepomuk::valueToRDFNode( const Nepomuk::Variant& v )
{
    return Soprano::Node( Soprano::LiteralValue( v.variant() ) );
}


QList<Soprano::Node> Nepomuk::valuesToRDFNodes( const Nepomuk::Variant& v )
{
    QList<Soprano::Node> nl;

    if( v.isList() ) {
        QStringList vl = v.toStringList();
        for( QStringList::const_iterator it = vl.begin(); it != vl.end(); ++it ) {
            nl.append( Soprano::Node( Soprano::LiteralValue::fromString( *it, ( QVariant::Type )v.simpleType() ) ) );
        }
    }
    else {
        nl.append( valueToRDFNode( v ) );
    }

    return nl;
}


Nepomuk::Variant Nepomuk::RDFLiteralToValue( const Soprano::Node& node )
{
    return Variant( node.literal().variant() );
}


QString Nepomuk::rdfNamepace()
{
    return QString( RDF_NAMESPACE );
}


QString Nepomuk::rdfsNamespace()
{
    return QString( RDFS_NAMESPACE );
}


QString Nepomuk::nrlNamespace()
{
    return QString( NRL_NAMESPACE );
}


QString Nepomuk::naoNamespace()
{
    return QString( NAO_NAMESPACE );
}
