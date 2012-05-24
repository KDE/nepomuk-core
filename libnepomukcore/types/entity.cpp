/* This file is part of the Nepomuk-KDE libraries
    Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include "entity.h"
#include "entity_p.h"
#include "resourcemanager.h"

#include <QtCore/QHash>
#include <QtCore/QMutexLocker>

#include <Soprano/QueryResultIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>

#include <kicon.h>


Nepomuk2::Types::EntityPrivate::EntityPrivate( const QUrl& uri_ )
    : mutex(QMutex::Recursive),
      uri( uri_ ),
      userVisible( true ),
      available( uri_.isValid() ? -1 : 0 ),
      ancestorsAvailable( uri_.isValid() ? -1 : 0 )
{
}


void Nepomuk2::Types::EntityPrivate::init()
{
    QMutexLocker lock( &mutex );

    if ( available < 0 ) {
        available = load() ? 1 : 0;
    }
}


void Nepomuk2::Types::EntityPrivate::initAncestors()
{
    QMutexLocker lock( &mutex );

    if ( ancestorsAvailable < 0 ) {
        ancestorsAvailable = loadAncestors() ? 1 : 0;
    }
}


bool Nepomuk2::Types::EntityPrivate::load()
{
    const QString query = QString::fromLatin1( "select ?p ?o where { "
                                               "graph ?g { <%1> ?p ?o . } . "
                                               "{ ?g a %2 . } UNION { ?g a %3 . } . }" )
                          .arg( QString::fromAscii( uri.toEncoded() ),
                                Soprano::Node::resourceToN3( Soprano::Vocabulary::NRL::Ontology() ),
                                Soprano::Node::resourceToN3( Soprano::Vocabulary::NRL::KnowledgeBase() ) );
                          
    Soprano::QueryResultIterator it
        = ResourceManager::instance()->mainModel()->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while ( it.next() ) {
        QUrl property = it.binding( "p" ).uri();
        Soprano::Node value = it.binding( "o" );

        if ( property == Soprano::Vocabulary::RDFS::label() ) {
            if ( value.language().isEmpty() ) {
                label = value.toString();
            }
            else if( value.language() == KGlobal::locale()->language() ) {
                l10nLabel = value.toString();
            }
        }

        else if ( property == Soprano::Vocabulary::RDFS::comment() ) {
            if ( value.language().isEmpty() ) {
                comment = value.toString();
            }
            else if( value.language() == KGlobal::locale()->language() ) {
                l10nComment = value.toString();
            }
        }

        else if ( property == Soprano::Vocabulary::NAO::hasSymbol() ) {
            icon = KIcon( value.toString() );
        }

        else if ( property == Soprano::Vocabulary::NAO::userVisible() ) {
            userVisible = value.literal().toBool();
        }

        else {
            addProperty( property, value );
        }
    }

    return !it.lastError();
}


bool Nepomuk2::Types::EntityPrivate::loadAncestors()
{
    const QString query = QString::fromLatin1( "select ?s ?p where { "
                                               "graph ?g { ?s ?p <%1> . } . "
                                               "{ ?g a %2 . } UNION { ?g a %3 . } . }" )
                          .arg( QString::fromAscii( uri.toEncoded() ),
                                Soprano::Node::resourceToN3( Soprano::Vocabulary::NRL::Ontology() ),
                                Soprano::Node::resourceToN3( Soprano::Vocabulary::NRL::KnowledgeBase() ) );
                          
    Soprano::QueryResultIterator it
        = ResourceManager::instance()->mainModel()->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while ( it.next() ) {
        addAncestorProperty( it.binding( "s" ).uri(), it.binding( "p" ).uri() );
    }

    return !it.lastError();
}



void Nepomuk2::Types::EntityPrivate::reset( bool )
{
    QMutexLocker lock( &mutex );

    label.clear();
    comment.clear();
    l10nLabel.clear();
    l10nComment.clear();;

    icon = QIcon();

    available = -1;
    ancestorsAvailable = -1;
}


Nepomuk2::Types::Entity::Entity()
{
}


Nepomuk2::Types::Entity::Entity( const Entity& other )
{
    d = other.d;
}


Nepomuk2::Types::Entity::~Entity()
{
}


Nepomuk2::Types::Entity& Nepomuk2::Types::Entity::operator=( const Entity& other )
{
    d = other.d;
    return *this;
}


QUrl Nepomuk2::Types::Entity::uri() const
{
    return d ? d->uri : QUrl();
}


QString Nepomuk2::Types::Entity::name() const
{
    return d ? (d->uri.fragment().isEmpty() ? d->uri.toString().section('/',-1) : d->uri.fragment() ) : QString();
}


QString Nepomuk2::Types::Entity::label( const QString& language )
{
    if ( d ) {
        d->init();

        if ( language == KGlobal::locale()->language() &&
             !d->l10nLabel.isEmpty() ) {
            return d->l10nLabel;
        }
        else if( !d->label.isEmpty() ) {
            return d->label;
        }
        else {
            return name();
        }
    }
    else {
        return QString();
    }
}


QString Nepomuk2::Types::Entity::label( const QString& language ) const
{
    return const_cast<Entity*>(this)->label( language );
}


QString Nepomuk2::Types::Entity::comment( const QString& language )
{
    if ( d ) {
        d->init();

        if ( language == KGlobal::locale()->language() &&
             !d->l10nComment.isEmpty() ) {
            return d->l10nComment;
        }
        else {
            return d->comment;
        }
    }
    else {
        return QString();
    }
}


QString Nepomuk2::Types::Entity::comment( const QString& language ) const
{
    return const_cast<Entity*>(this)->comment( language );
}


QIcon Nepomuk2::Types::Entity::icon()
{
    if ( d ) {
        d->init();

        return d->icon;
    }
    else {
        return QIcon();
    }
}


QIcon Nepomuk2::Types::Entity::icon() const
{
    return const_cast<Entity*>(this)->icon();
}


bool Nepomuk2::Types::Entity::isValid() const
{
    return d ? d->uri.isValid() : false;
}


bool Nepomuk2::Types::Entity::isAvailable()
{
    if ( d ) {
        d->init();
        return d->available == 1;
    }
    else {
        return false;
    }
}


bool Nepomuk2::Types::Entity::isAvailable() const
{
    return const_cast<Entity*>(this)->isAvailable();
}


void Nepomuk2::Types::Entity::reset( bool recursive )
{
    if( d )
        d->reset( recursive );
}


bool Nepomuk2::Types::Entity::userVisible() const
{
    if ( d ) {
        d->init();
        return d->userVisible;
    }
    else {
        return true;
    }
}


bool Nepomuk2::Types::Entity::operator==( const Entity& other ) const
{
    // since we use one instace cache we can improve comparation operations
    // intensly by not comparing URLs but pointers.
    return( d.constData() == other.d.constData() );
}


bool Nepomuk2::Types::Entity::operator==( const QUrl& other ) const
{
    if( d )
        return( d->uri == other );
    else
        return other.isEmpty();
}


bool Nepomuk2::Types::Entity::operator!=( const Entity& other ) const
{
    // since we use one instace cache we can improve comparation operations
    // intensly by not comparing URLs but pointers.
    return( d.constData() != other.d.constData() );
}


bool Nepomuk2::Types::Entity::operator!=( const QUrl& other ) const
{
    if( d )
        return( d->uri != other );
    else
        return !other.isEmpty();
}
