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


#include "identifiermodeltree.h"

#include "nie.h"
#include "nfo.h"
#include <Soprano/Vocabulary/RDF>

#include <Soprano/Statement>
#include <Soprano/Vocabulary/RDFS>

#include "types/class.h"

Nepomuk2::IdentifierModelTreeItem::IdentifierModelTreeItem(const QString& url, bool isFolder)
    : FileSystemTreeItem( url, isFolder )
{
    m_discarded = false;
}

QUrl Nepomuk2::IdentifierModelTreeItem::type() const
{
    return m_type;
}

QString Nepomuk2::IdentifierModelTreeItem::toString() const
{
    if( !parent() ) {
        return url();
    }

    const QString parentString = parent()->url();
    const QString ownString = url();

    return ownString.mid( parentString.size() );
}

QUrl Nepomuk2::IdentifierModelTreeItem::identifiedUri() const
{
    return m_identifiedUrl;
}

bool Nepomuk2::IdentifierModelTreeItem::discarded() const
{
    return m_discarded;
}

void Nepomuk2::IdentifierModelTreeItem::setDiscarded(bool status)
{
    m_discarded = status;
}


// static
Nepomuk2::IdentifierModelTreeItem* Nepomuk2::IdentifierModelTreeItem::fromStatementList( const QList< Soprano::Statement >& stList)
{
    const QUrl nieUrl = Nepomuk2::Vocabulary::NIE::url();
    const QUrl rdfType = Soprano::Vocabulary::RDF::type();
    const QUrl nfoFolder = Nepomuk2::Vocabulary::NFO::Folder();
    
    bool isFolder = false;
    QString url;
    QUrl type = Soprano::Vocabulary::RDFS::Resource();
    
    foreach( const Soprano::Statement & st, stList ) {
        if( st.predicate().uri() == nieUrl ) {
            url = st.object().uri().toLocalFile();
            if( url.startsWith("file://") )
                url = url.mid( 7 );
        }
        else if( st.predicate().uri() == rdfType ) {
            QUrl objectUri = st.object().uri();
            if( objectUri == nfoFolder )
                isFolder = true;

            // Get the correct type and store it into type
            // FIXME: This may not be totally accurate
            Types::Class oldClass( type );
            Types::Class newClass( objectUri );
            if( newClass.isSubClassOf( oldClass ) ) {
                type = objectUri;
            }
        }
    }
    
    IdentifierModelTreeItem* item = new IdentifierModelTreeItem( url, isFolder );
    item->m_statements = stList;
    item->m_type = type;

    return item;
}

QUrl Nepomuk2::IdentifierModelTreeItem::resourceUri() const
{
    if( m_statements.isEmpty() )
        return QUrl();
    
    return m_statements.first().subject().uri();
}


bool Nepomuk2::IdentifierModelTreeItem::identified() const
{
    return !m_identifiedUrl.isEmpty();
}

void Nepomuk2::IdentifierModelTreeItem::setIdentified(const QUrl& newUri)
{
    m_identifiedUrl = newUri;
}

void Nepomuk2::IdentifierModelTreeItem::setUnidentified()
{
    m_identifiedUrl.clear();
}

void Nepomuk2::IdentifierModelTreeItem::discardAll()
{
    setDiscarded( true );
    foreach( FileSystemTreeItem* item, children() ) {
        IdentifierModelTreeItem* i = dynamic_cast<IdentifierModelTreeItem*>(item);
        i->discardAll();
    }
}


//
// IdentifierModelTree
//

Nepomuk2::IdentifierModelTree::IdentifierModelTree()
    : FileSystemTree()
{
}

void Nepomuk2::IdentifierModelTree::add(FileSystemTreeItem* fileItem)
{
    IdentifierModelTreeItem * item = dynamic_cast<IdentifierModelTreeItem *>( fileItem );
    
    QUrl resUri;
    if( !item->m_statements.isEmpty() )
        resUri = item->m_statements.first().subject().uri();
    
    m_resUrlHash.insert( resUri, item->url() );
    
    FileSystemTree::add(item);
}

Nepomuk2::IdentifierModelTreeItem* Nepomuk2::IdentifierModelTree::findByUri(const QUrl& uri)
{
    QHash<QUrl, QString>::const_iterator it = m_resUrlHash.constFind( uri );
    if( it != m_resUrlHash.constEnd() ) {
        FileSystemTreeItem* p = find( it.value() );
        return dynamic_cast<IdentifierModelTreeItem*>( p );
    }

    return 0;
}

void Nepomuk2::IdentifierModelTree::discardAll()
{
    foreach( FileSystemTreeItem* item, rootNodes() ) {
        IdentifierModelTreeItem* i = dynamic_cast<IdentifierModelTreeItem*>(item);
        i->discardAll();
    }
}   



