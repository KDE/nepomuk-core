/* This file is part of the Nepomuk-KDE libraries
    Copyright (c) 2007 Sebastian Trueg <trueg@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "ontology.h"
#include "ontology_p.h"
#include "class.h"
#include "property.h"
#include "entitymanager.h"
#include "resourcemanager.h"

#include <Soprano/QueryResultIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/XMLSchema>

#undef D
#define D static_cast<Nepomuk2::Types::OntologyPrivate*>( d.data() )

Nepomuk2::Types::OntologyPrivate::OntologyPrivate( const QUrl& uri )
    : EntityPrivate( uri ),
      entitiesAvailable( uri.isValid() ? -1 : 0 )
{
}


void Nepomuk2::Types::OntologyPrivate::initEntities()
{
    if ( entitiesAvailable < 0 ) {
        entitiesAvailable = loadEntities() ? 1 : 0;
    }
}


bool Nepomuk2::Types::OntologyPrivate::loadEntities()
{
    // load classes
    // We use a FILTER(STR(?ns)...) to support both Soprano 2.3 (with plain literals) and earlier (with only typed ones)
    Soprano::QueryResultIterator it
        = ResourceManager::instance()->mainModel()->executeQuery( QString("select ?c where { "
                                                                          "graph ?g { ?c a <%1> . } . "
                                                                          "?g <%2> ?ns . "
                                                                          "FILTER(STR(?ns) = \"%3\") . }" )
                                                                  .arg( Soprano::Vocabulary::RDFS::Class().toString() )
                                                                  .arg( Soprano::Vocabulary::NAO::hasDefaultNamespace().toString() )
                                                                  .arg( QString::fromAscii( uri.toEncoded() ) ),
                                                                  Soprano::Query::QueryLanguageSparql );
    while ( it.next() ) {
        classes.append( Class( it.binding( "c" ).uri() ) );
    }


    // load properties
    it = ResourceManager::instance()->mainModel()->executeQuery( QString("select ?p where { "
                                                                         "graph ?g { ?p a <%1> . } . "
                                                                         "?g <%2> ?ns . "
                                                                         "FILTER(STR(?ns) = \"%3\") . }" )
                                                                 .arg( Soprano::Vocabulary::RDF::Property().toString() )
                                                                 .arg( Soprano::Vocabulary::NAO::hasDefaultNamespace().toString() )
                                                                 .arg( QString::fromAscii( uri.toEncoded() ) ),
                                                                 Soprano::Query::QueryLanguageSparql );
    while ( it.next() ) {
        properties.append( Property( it.binding( "p" ).uri() ) );
    }

    return !it.lastError();
}


bool Nepomuk2::Types::OntologyPrivate::addProperty( const QUrl&, const Soprano::Node& )
{
    return false;
}


bool Nepomuk2::Types::OntologyPrivate::addAncestorProperty( const QUrl&, const QUrl& )
{
    return false;
}


void Nepomuk2::Types::OntologyPrivate::reset( bool recursive )
{
    QMutexLocker lock( &mutex );

    if ( entitiesAvailable != -1 ) {
        if ( recursive ) {
            foreach( Class c, classes ) {
                c.reset( true );
            }
            foreach( Property p, properties ) {
                p.reset( true );
            }
        }
        classes.clear();
        properties.clear();

        entitiesAvailable = -1;
    }

    EntityPrivate::reset( recursive );
}



Nepomuk2::Types::Ontology::Ontology()
{
    d = new OntologyPrivate();
}


Nepomuk2::Types::Ontology::Ontology( const QUrl& uri )
{
    d = EntityManager::self()->getOntology( uri );
}


Nepomuk2::Types::Ontology::Ontology( const Ontology& other )
    : Entity( other )
{
}


Nepomuk2::Types::Ontology::~Ontology()
{
}


Nepomuk2::Types::Ontology& Nepomuk2::Types::Ontology::operator=( const Ontology& other )
{
    d = other.d;
    return *this;
}


QList<Nepomuk2::Types::Class> Nepomuk2::Types::Ontology::allClasses()
{
    D->initEntities();
    return D->classes;
}


Nepomuk2::Types::Class Nepomuk2::Types::Ontology::findClassByName( const QString& name )
{
    D->initEntities();
    for ( QList<Class>::const_iterator it = D->classes.constBegin();
          it != D->classes.constEnd(); ++it ) {
        const Class& c = *it;
        if ( c.name() == name ) {
            return c;
        }
    }

    return Class();
}


Nepomuk2::Types::Class Nepomuk2::Types::Ontology::findClassByLabel( const QString& label, const QString& language )
{
    D->initEntities();
    for ( QList<Class>::iterator it = D->classes.begin();
          it != D->classes.end(); ++it ) {
        Class& c = *it;
        if ( c.label( language ) == label ) {
            return c;
        }
    }

    return Class();
}


QList<Nepomuk2::Types::Property> Nepomuk2::Types::Ontology::allProperties()
{
    D->initEntities();
    return D->properties;
}


Nepomuk2::Types::Property Nepomuk2::Types::Ontology::findPropertyByName( const QString& name )
{
    D->initEntities();
    for ( QList<Property>::const_iterator it = D->properties.constBegin();
          it != D->properties.constEnd(); ++it ) {
        const Property& p = *it;
        if ( p.name() == name ) {
            return p;
        }
    }

    return Property();
}


Nepomuk2::Types::Property Nepomuk2::Types::Ontology::findPropertyByLabel( const QString& label, const QString& language )
{
    D->initEntities();
    for ( QList<Property>::iterator it = D->properties.begin();
          it != D->properties.end(); ++it ) {
        Property& p = *it;
        if ( p.label( language ) == label ) {
            return p;
        }
    }

    return Property();
}
