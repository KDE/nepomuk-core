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

#include "property.h"
#include "property_p.h"
#include "class.h"
#include "ontology.h"
#include "literal.h"
#include "entitymanager.h"

#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/XMLSchema>

#undef D
#define D static_cast<Nepomuk2::Types::PropertyPrivate*>( d.data() )

Nepomuk2::Types::PropertyPrivate::PropertyPrivate( const QUrl& uri )
    : EntityPrivate( uri ),
      minCardinality( -1 ),
      maxCardinality( -1 ),
      cardinality( -1 )
{
}


bool Nepomuk2::Types::PropertyPrivate::addProperty( const QUrl& property, const Soprano::Node& value )
{
    // we avoid subclassing loops (as created for crappy inferencing) by checking for our own uri
    if( value.isResource() &&
        value.uri() != uri &&
        property == Soprano::Vocabulary::RDFS::subPropertyOf() ) {
        parents.append( value.uri() );
        return true;
    }

    else if( property == Soprano::Vocabulary::RDFS::domain() ) {
        domain = value.uri();
        return true;
    }

    else if( property == Soprano::Vocabulary::RDFS::range() ) {
        if ( value.toString().startsWith( Soprano::Vocabulary::XMLSchema::xsdNamespace().toString() ) ) {
            literalRange = Literal( value.uri() );
        }
        else if ( value.uri() == Soprano::Vocabulary::RDFS::Literal()) {
            literalRange = Literal( value.uri() );
        }
        else {
            range = value.uri();
        }
        return true;
    }

    else if( property == Soprano::Vocabulary::NRL::minCardinality() ) {
        minCardinality = value.literal().toInt();
        return true;
    }

    else if( property == Soprano::Vocabulary::NRL::maxCardinality() ) {
        maxCardinality = value.literal().toInt();
        return true;
    }

    else if ( property == Soprano::Vocabulary::NRL::cardinality() ) {
        cardinality = value.literal().toInt();
        return true;
    }

    else if ( property == Soprano::Vocabulary::NRL::inverseProperty() ) {
        inverse = value.uri();
        return true;
    }

    return false;
}


bool Nepomuk2::Types::PropertyPrivate::addAncestorProperty( const QUrl& ancestorResource, const QUrl& property )
{
    // we avoid subclassing loops (as created for crappy inferencing) by checking for our own uri
    if( property == Soprano::Vocabulary::RDFS::subPropertyOf() &&
        ancestorResource != uri ) {
        children.append( ancestorResource );
        return true;
    }
    else if ( property == Soprano::Vocabulary::NRL::inverseProperty() ) {
        inverse = ancestorResource;
        return true;
    }

    return false;
}


void Nepomuk2::Types::PropertyPrivate::reset( bool recursive )
{
    QMutexLocker lock( &mutex );

    if ( available != -1 ) {
        if ( recursive ) {
            range.reset( true );
            domain.reset( true );
            inverse.reset( true );
            foreach( Property p, parents ) {
                p.reset( true );
            }
        }

        parents.clear();
        available = -1;
    }

    if ( ancestorsAvailable != -1 ) {
        if ( recursive ) {
            foreach( Property p, children ) {
                p.reset( true );
            }
        }

        children.clear();
        ancestorsAvailable = -1;
    }

    EntityPrivate::reset( recursive );
}



Nepomuk2::Types::Property::Property()
    : Entity()
{
    d = 0;
}


Nepomuk2::Types::Property::Property( const QUrl& uri )
    : Entity()
{
    d = EntityManager::self()->getProperty( uri );
}


Nepomuk2::Types::Property::Property( const Property& other )
    : Entity( other )
{
}


Nepomuk2::Types::Property::~Property()
{
}


Nepomuk2::Types::Property& Nepomuk2::Types::Property::operator=( const Property& other )
{
    d = other.d;
    return *this;
}


QList<Nepomuk2::Types::Property> Nepomuk2::Types::Property::parentProperties()
{
    if ( d ) {
        D->init();
        return D->parents;
    }
    else {
        return QList<Nepomuk2::Types::Property>();
    }
}


QList<Nepomuk2::Types::Property> Nepomuk2::Types::Property::parentProperties() const
{
    return const_cast<Property*>(this)->parentProperties();
}


QList<Nepomuk2::Types::Property> Nepomuk2::Types::Property::subProperties()
{
    if ( d ) {
        D->initAncestors();
        return D->children;
    }
    else {
        return QList<Nepomuk2::Types::Property>();
    }
}


QList<Nepomuk2::Types::Property> Nepomuk2::Types::Property::subProperties() const
{
    return const_cast<Property*>(this)->subProperties();
}


Nepomuk2::Types::Property Nepomuk2::Types::Property::inverseProperty()
{
    if ( d ) {
        D->init();
        D->initAncestors();
        return D->inverse;
    }
    else {
        return Property();
    }
}


Nepomuk2::Types::Property Nepomuk2::Types::Property::inverseProperty() const
{
    return const_cast<Property*>(this)->inverseProperty();
}


Nepomuk2::Types::Class Nepomuk2::Types::Property::range()
{
    if ( d ) {
        D->init();

        if( D->range.isValid() ) {
            return D->range;
        }
        else if( !literalRangeType().isValid() ) {
            // try getting a domain from one of the parent properties
            for( int i = 0; i < D->parents.count(); ++i ) {
                Class pr = D->parents[i].range();
                if( pr.isValid() ) {
                    return pr;
                }
            }

            // if we have no literal range type, we fall back to rdfs:Resource
            return Class( Soprano::Vocabulary::RDFS::Resource() );
        }
        else {
            // other than domain() we do not use a general fallback since the range
            // might be a literalRangeType()
            return Class();
        }
    }
    else {
        return Class();
    }
}


Nepomuk2::Types::Class Nepomuk2::Types::Property::range() const
{
    return const_cast<Property*>(this)->range();
}


Nepomuk2::Types::Literal Nepomuk2::Types::Property::literalRangeType()
{
    if ( d ) {
        D->init();

        if( D->literalRange.isValid() ) {
            return D->literalRange;
        }
        else {
            // try getting a domain from one of the parent properties
            // We cannot check the resource range here since that would
            // result in an endless loop
            for( int i = 0; i < D->parents.count(); ++i ) {
                Literal pr = D->parents[i].literalRangeType();
                if( pr.isValid() ) {
                    return pr;
                }
            }

            // fallback is an invalid range which will then result in
            // range() returning a valid one
            return Literal();
        }
    }
    else {
        return Literal();
    }
}


Nepomuk2::Types::Literal Nepomuk2::Types::Property::literalRangeType() const
{
    return const_cast<Property*>(this)->literalRangeType();
}


Nepomuk2::Types::Class Nepomuk2::Types::Property::domain()
{
    if ( d ) {
        D->init();

        if( D->domain.isValid() ) {
            return D->domain;
        }
        else {
            // try getting a domain from one of the parent properties
            for( int i = 0; i < D->parents.count(); ++i ) {
                Class pd = D->parents[i].domain();
                if( pd.isValid() ) {
                    return pd;
                }
            }

            // fallback: rdfs:Resource
            return Class( Soprano::Vocabulary::RDFS::Resource() );
        }
    }
    else {
        return Class();
    }
}


Nepomuk2::Types::Class Nepomuk2::Types::Property::domain() const
{
    return const_cast<Property*>(this)->domain();
}


int Nepomuk2::Types::Property::cardinality()
{
    if ( d ) {
        D->init();
        return D->cardinality;
    }
    else {
        return -1;
    }
}


int Nepomuk2::Types::Property::cardinality() const
{
    return const_cast<Property*>(this)->cardinality();
}


int Nepomuk2::Types::Property::minCardinality()
{
    if ( d ) {
        D->init();
        if ( D->minCardinality > 0 ) {
            return D->minCardinality;
        }
        else {
            return D->cardinality;
        }
    }
    else {
        return -1;
    }
}


int Nepomuk2::Types::Property::minCardinality() const
{
    return const_cast<Property*>(this)->minCardinality();
}


int Nepomuk2::Types::Property::maxCardinality()
{
    if ( d ) {
        D->init();
        if ( D->maxCardinality > 0 ) {
            return D->maxCardinality;
        }
        else {
            return D->cardinality;
        }
    }
    else {
        return -1;
    }
}


int Nepomuk2::Types::Property::maxCardinality() const
{
    return const_cast<Property*>(this)->maxCardinality();
}


bool Nepomuk2::Types::Property::isParentOf( const Property& other )
{
    if ( d ) {
        D->initAncestors();

        if ( D->children.contains( other ) ) {
            return true;
        }
        else {
            for ( QList<Nepomuk2::Types::Property>::iterator it = D->children.begin();
                  it != D->children.end(); ++it ) {
                if ( ( *it ).isParentOf( other ) ) {
                    return true;
                }
            }
        }
    }

    return false;
}


bool Nepomuk2::Types::Property::isParentOf( const Property& other ) const
{
    return const_cast<Property*>(this)->isParentOf( other );
}


bool Nepomuk2::Types::Property::isSubPropertyOf( const Property& other )
{
    if ( d ) {
        D->init();

        if ( D->parents.contains( other ) ) {
            return true;
        }
        else {
            for ( QList<Nepomuk2::Types::Property>::iterator it = D->parents.begin();
                  it != D->parents.end(); ++it ) {
                if ( ( *it ).isSubPropertyOf( other ) ) {
                    return true;
                }
            }
        }
    }

    return false;
}


bool Nepomuk2::Types::Property::isSubPropertyOf( const Property& other ) const
{
    return const_cast<Property*>(this)->isSubPropertyOf( other );
}
