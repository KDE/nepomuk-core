/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2010 Sebastian Trueg <trueg@kde.org>
 * Copyright (C) 2012 Vishesh Handa <me@vhanda.in>
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

#include "resource.h"
#include "resourcedata.h"
#include "resourcemanager.h"
#include "resourcemanager_p.h"
#include "tools.h"
#include "tag.h"
#include "pimo.h"
#include "file.h"
#include "property.h"
#include "nfo.h"
#include "nie.h"
#include "nco.h"
#include "nuao.h"

#include "klocale.h"
#include "kdebug.h"
#include "kurl.h"
#include "kmimetype.h"

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

Nepomuk2::Resource::Resource()
{
    ResourceManager* rm = ResourceManager::instance();
    if( rm ) {
        QMutexLocker lock( &rm->d->mutex );
        m_data = rm->d->data( QUrl(), QUrl() );
        if ( m_data )
            m_data->ref( this );
    }
    else {
        kError() << "QCoreApplication does not exist. Resource cannot be initalialized";
    }
}


Nepomuk2::Resource::Resource( const Nepomuk2::Resource& res )
{
    ResourceManager* rm = ResourceManager::instance();
    if( rm ) {
        QMutexLocker lock( &rm->d->mutex );
        m_data = res.m_data;
        if ( m_data )
            m_data->ref( this );
    }
    else {
        kError() << "QCoreApplication does not exist. Resource cannot be initalialized";
    }
}


Nepomuk2::Resource::Resource( const QString& uri, const QUrl& type )
{
    ResourceManager* rm = ResourceManager::instance();
    if( rm ) {
        QMutexLocker lock( &rm->d->mutex );
        m_data = rm->d->data( uri, type );
        if ( m_data )
            m_data->ref( this );
    }
    else {
        kError() << "QCoreApplication does not exist. Resource cannot be initalialized";
    }
}


Nepomuk2::Resource::Resource( const QUrl& uri, const QUrl& type )
{
    ResourceManager* rm = ResourceManager::instance();
    if( rm ) {
        QMutexLocker lock( &rm->d->mutex );
        m_data = rm->d->data( uri, type );
        if ( m_data )
            m_data->ref( this );
    }
    else {
        kError() << "QCoreApplication does not exist. Resource cannot be initalialized";
    }
}



Nepomuk2::Resource::Resource( Nepomuk2::ResourceData* data )
{
    QMutexLocker lock( &data->rm()->mutex );
    m_data = data;
    if ( m_data )
        m_data->ref( this );
}


Nepomuk2::Resource::~Resource()
{
    if ( m_data ) {
        QMutexLocker lock(&m_data->rm()->mutex);
        m_data->deref( this );
        if ( m_data->rm()->shouldBeDeleted( m_data ) )
            delete m_data;
    }
}


Nepomuk2::Resource& Nepomuk2::Resource::operator=( const Resource& res )
{
    if( m_data != res.m_data ) {
        QMutexLocker lock(&m_data->rm()->mutex);
        if ( m_data && !m_data->deref( this ) && m_data->rm()->shouldBeDeleted( m_data ) ) {
            delete m_data;
        }
        m_data = res.m_data;
        if ( m_data )
            m_data->ref( this );
    }

    return *this;
}


Nepomuk2::Resource& Nepomuk2::Resource::operator=( const QUrl& res )
{
    return operator=( Resource( res ) );
}


QUrl Nepomuk2::Resource::uri() const
{
    if ( m_data ) {
        determineFinalResourceData();
        return m_data->uri();
    }
    else {
        return QUrl();
    }
}


QUrl Nepomuk2::Resource::type() const
{
    determineFinalResourceData();
    return m_data->type();
}


QList<QUrl> Nepomuk2::Resource::types() const
{
    determineFinalResourceData();
    return m_data->property( RDF::type() ).toUrlList();
}


void Nepomuk2::Resource::setTypes( const QList<QUrl>& types )
{
    determineFinalResourceData();
    m_data->setProperty( RDF::type(), types );
}


void Nepomuk2::Resource::addType( const QUrl& type )
{
    determineFinalResourceData();
    m_data->addProperty( RDF::type(), type );
}


bool Nepomuk2::Resource::hasType( const QUrl& typeUri ) const
{
    determineFinalResourceData();
    return m_data->hasProperty( RDF::type(), typeUri );
}


QHash<QUrl, Nepomuk2::Variant> Nepomuk2::Resource::properties() const
{
    determineFinalResourceData();
    return m_data->allProperties();
}


bool Nepomuk2::Resource::hasProperty( const QUrl& uri ) const
{
    determineFinalResourceData();
    return m_data->hasProperty( uri );
}


bool Nepomuk2::Resource::hasProperty( const Types::Property& p, const Variant& v ) const
{
    determineFinalResourceData();
    return m_data->hasProperty( p.uri(), v );
}


Nepomuk2::Variant Nepomuk2::Resource::property( const QUrl& uri ) const
{
    determineFinalResourceData();
    return m_data->property( uri );
}


void Nepomuk2::Resource::addProperty( const QUrl& uri, const Variant& value )
{
    determineFinalResourceData();
    m_data->addProperty( uri, value );
}


void Nepomuk2::Resource::setProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    determineFinalResourceData();
    m_data->setProperty( uri, value );
}


void Nepomuk2::Resource::removeProperty( const QUrl& uri )
{
    determineFinalResourceData();
    m_data->removeProperty( uri );
}


void Nepomuk2::Resource::removeProperty( const QUrl& uri, const Variant& value )
{
    QList<Variant> vl = property( uri ).toVariantList();
    foreach( const Variant& v, value.toVariantList() ) {
        vl.removeAll( v );
    }
    setProperty( uri, Variant( vl ) );
}


void Nepomuk2::Resource::remove()
{
    determineFinalResourceData();
    m_data->remove();
}


bool Nepomuk2::Resource::exists() const
{
    determineFinalResourceData();
    if ( m_data ) {
        return m_data->exists();
    }
    else {
        return false;
    }
}


bool Nepomuk2::Resource::isValid() const
{
    return m_data ? m_data->isValid() : false;
}


// TODO: cache this one in ResourceData
QString Nepomuk2::Resource::genericLabel() const
{
    QString label = this->label();
    if(!label.isEmpty())
        return label;

    label = property( Soprano::Vocabulary::RDFS::label() ).toString();
    if(!label.isEmpty())
        return label;

    label = property( Nepomuk2::Vocabulary::NIE::title() ).toString();
    if(!label.isEmpty())
        return label;

    label = property( Nepomuk2::Vocabulary::NCO::fullname() ).toString();
    if(!label.isEmpty())
        return label;

    label = property( Soprano::Vocabulary::NAO::identifier() ).toString();
    if(!label.isEmpty())
        return label;

    //label = m_data->pimoThing().label();
    //if(!label.isEmpty())
    //    return label;

    label = property( Nepomuk2::Vocabulary::NFO::fileName() ).toString();
    if(!label.isEmpty())
        return label;

    const KUrl nieUrl = property( Nepomuk2::Vocabulary::NIE::url() ).toUrl();
    if(!nieUrl.isEmpty()) {
        if(nieUrl.isLocalFile())
            return nieUrl.fileName();
        else
            return nieUrl.prettyUrl();
    }

    QList<Resource> go = property( Vocabulary::PIMO::groundingOccurrence() ).toResourceList();
    if( !go.isEmpty() ) {
        label = go.first().genericLabel();
        if( label != KUrl(go.first().uri()).pathOrUrl() ) {
            return label;
        }
    }

    QString hashValue = property( Vocabulary::NFO::hashValue() ).toString();
    if( !hashValue.isEmpty() )
        return hashValue;

    // ugly fallback
    return KUrl(uri()).pathOrUrl();
}


QString Nepomuk2::Resource::genericDescription() const
{
    QString s = property( Soprano::Vocabulary::NAO::description() ).toString();
    if ( !s.isEmpty() ) {
        return s;
    }

    s = property( Soprano::Vocabulary::RDFS::comment() ).toString();

    return s;
}


QString Nepomuk2::Resource::genericIcon() const
{
   if( hasType(NAO::FreeDesktopIcon()) ) {
        QString label = property(NAO::iconName()).toString();
        if( !label.isEmpty() )
            return label;

        label = property(NAO::prefLabel()).toString();
        if( !label.isEmpty() )
            return label;
    }

    // Symbol Resources
    Variant symbol = property( NAO::hasSymbol() );
    if( symbol.isResource() ) {
        QString icon = symbol.toResource().genericIcon();
        if(!icon.isEmpty())
            return icon;
    }
    else if( symbol.isString() ) {
        // Backward compatibiltiy
        QString icon = symbol.toString();
        if( !icon.isEmpty() )
            return icon;
    }

    // FIXME: NAO::prefSymbol is a sub-property of nao:hasSymbol, it should be auto detected
    symbol = property( NAO::prefSymbol() );
    if( symbol.isResource() ) {
        QString icon = symbol.toResource().genericIcon();
        if(!icon.isEmpty())
            return icon;
    }
    else if( symbol.isString() ) {
        // Backward compatibiltiy
        QString icon = symbol.toString();
        if( !icon.isEmpty() )
            return icon;
    }

    QString mimeType = property( NIE::mimeType() ).toString();
    if( !mimeType.isEmpty() ) {
        if( KMimeType::Ptr m = KMimeType::mimeType( mimeType ) )
            return m->iconName();
    }

    return QString();
}


bool Nepomuk2::Resource::operator==( const Resource& other ) const
{
    if( this == &other )
        return true;

    if( this->m_data == other.m_data )
        return true;

    if ( !m_data || !other.m_data ) {
        return false;
    }

    // get the resource URIs since two different ResourceData instances
    // can still represent the same Resource
    determineFinalResourceData();
    other.determineFinalResourceData();

    if( m_data->uri().isEmpty() )
        return *m_data == *other.m_data;
    else
        return uri() == other.uri();
}


bool Nepomuk2::Resource::operator!=( const Resource& other ) const
{
    return !operator==( other );
}


QString Nepomuk2::errorString( ErrorCode code )
{
    switch( code ) {
    case NoError:
        return i18n("Success");
    case CommunicationError:
        return i18n("Communication error");
    case InvalidType:
        return i18n("Invalid type in Database");
    default:
        return i18n("Unknown error");
    }
}


QString Nepomuk2::Resource::description() const
{
    return ( property( Soprano::Vocabulary::NAO::description() ).toStringList() << QString() ).first();
}


void Nepomuk2::Resource::setDescription( const QString& value )
{
    setProperty( Soprano::Vocabulary::NAO::description(), Variant( value ) );
}


QStringList Nepomuk2::Resource::identifiers() const
{
    return property( Soprano::Vocabulary::NAO::identifier() ).toStringList();
}


void Nepomuk2::Resource::setIdentifiers( const QStringList& value )
{
    setProperty( Soprano::Vocabulary::NAO::identifier(), Variant( value ) );
}


void Nepomuk2::Resource::addIdentifier( const QString& value )
{
    addProperty( Soprano::Vocabulary::NAO::identifier(), Variant( value ) );
}


QList<Nepomuk2::Tag> Nepomuk2::Resource::tags() const
{
    // We always store all Resource types as plain Resource objects.
    // It does not introduce any overhead (due to the implicit sharing of
    // the data and has the advantage that we can mix setProperty calls
    // with the special Resource subclass methods.
    // More importantly Resource loads the data as Resource objects anyway.
    return convertResourceList<Tag>( property( Soprano::Vocabulary::NAO::hasTag() ).toResourceList() );
}


void Nepomuk2::Resource::setTags( const QList<Nepomuk2::Tag>& value )
{
    // We always store all Resource types as plain Resource objects.
    // It does not introduce any overhead (due to the implicit sharing of
    // the data and has the advantage that we can mix setProperty calls
    // with the special Resource subclass methods.
    // More importantly Resource loads the data as Resource objects anyway.
    QList<Resource> l;
    for( QList<Tag>::const_iterator it = value.constBegin();
         it != value.constEnd(); ++it ) {
        l.append( Resource( (*it) ) );
    }
    setProperty( Soprano::Vocabulary::NAO::hasTag(), Variant( l ) );
}


void Nepomuk2::Resource::addTag( const Nepomuk2::Tag& value )
{
    // We always store all Resource types as plain Resource objects.
    // It does not introduce any overhead (due to the implicit sharing of
    // the data and has the advantage that we can mix setProperty calls
    // with the special Resource subclass methods.
    // More importantly Resource loads the data as Resource objects anyway.
    addProperty( Soprano::Vocabulary::NAO::hasTag(), Resource(value) );
}


QList<Nepomuk2::Resource> Nepomuk2::Resource::isRelateds() const
{
    // We always store all Resource types as plain Resource objects.
    // It does not introduce any overhead (due to the implicit sharing of
    // the data and has the advantage that we can mix setProperty calls
    // with the special Resource subclass methods.
    // More importantly Resource loads the data as Resource objects anyway.
    return property( Soprano::Vocabulary::NAO::isRelated() ).toResourceList();
}


void Nepomuk2::Resource::setIsRelateds( const QList<Nepomuk2::Resource>& value )
{
    setProperty( Soprano::Vocabulary::NAO::isRelated(), Variant( value ) );
}


void Nepomuk2::Resource::addIsRelated( const Nepomuk2::Resource& value )
{
    // We always store all Resource types as plain Resource objects.
    // It does not introduce any overhead (due to the implicit sharing of
    // the data and has the advantage that we can mix setProperty calls
    // with the special Resource subclass methods.
    // More importantly Resource loads the data as Resource objects anyway.
    addProperty( Soprano::Vocabulary::NAO::isRelated(), Resource(value) );
}


QString Nepomuk2::Resource::label() const
{
    return ( property( Soprano::Vocabulary::NAO::prefLabel() ).toStringList() << QString() ).first();
}


void Nepomuk2::Resource::setLabel( const QString& value )
{
    setProperty( Soprano::Vocabulary::NAO::prefLabel(), Variant( value ) );
}


quint32 Nepomuk2::Resource::rating() const
{
    return ( property( Soprano::Vocabulary::NAO::numericRating() ).toUnsignedIntList() << 0 ).first();
}


void Nepomuk2::Resource::setRating( const quint32& value )
{
    setProperty( Soprano::Vocabulary::NAO::numericRating(), Variant( value ) );
}

QStringList Nepomuk2::Resource::symbols() const
{
    QList<Resource> symbolResources = property( Soprano::Vocabulary::NAO::hasSymbol() ).toResourceList();

    QStringList symbolStrings;
    foreach(const Resource& symbolRes, symbolResources ) {
        symbolStrings << symbolRes.label();
    }

    return symbolStrings;
}

namespace {
    QUrl uriForSymbolName(const QString& symbolName) {
        // Check if it exists
        // We aren't using Soprano::Node::literalToN3 cause prefLabel has a range of a literal not
        // of a string
        QString query = QString::fromLatin1("select ?r where { ?r a %1 . ?r %2 \"%3\" . } LIMIT 1")
        .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::FreeDesktopIcon()),
              Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::iconName()),
              symbolName );

        Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        if( it.next() ) {
            return it["r"].uri();
        }
        else {
            Nepomuk2::Resource res(QUrl(), Soprano::Vocabulary::NAO::FreeDesktopIcon());
            res.setProperty( NAO::iconName(), symbolName );

            return res.uri();
        }
    }
}

void Nepomuk2::Resource::setSymbols( const QStringList& value )
{
    QList<QUrl> symbolList;
    foreach( const QString& symbolName, value ) {
        symbolList << uriForSymbolName(symbolName);
    }

    setProperty( Soprano::Vocabulary::NAO::hasSymbol(), Variant(symbolList) );
}


void Nepomuk2::Resource::addSymbol( const QString& value )
{
    addProperty( Soprano::Vocabulary::NAO::hasSymbol(), uriForSymbolName(value) );
}


QList<Nepomuk2::Resource> Nepomuk2::Resource::isRelatedOf() const
{
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QList<Soprano::Node> list = model->listStatements( Soprano::Node(), NAO::isRelated(), uri() ).iterateSubjects().allNodes();
    QList<Nepomuk2::Resource> resources;
    foreach(const Soprano::Node& node, list)
        resources << node.uri();
    return resources;
}


int Nepomuk2::Resource::usageCount() const
{
    return property( Vocabulary::NUAO::usageCount() ).toInt();
}


void Nepomuk2::Resource::increaseUsageCount()
{
    int cnt = 0;
    const QDateTime now = QDateTime::currentDateTime();
    if( hasProperty( Vocabulary::NUAO::usageCount() ) )
        cnt = property( Vocabulary::NUAO::usageCount() ).toInt();
    else
        setProperty( Vocabulary::NUAO::firstUsage(), now );
    ++cnt;
    setProperty( Vocabulary::NUAO::usageCount(), cnt );
    setProperty( Vocabulary::NUAO::lastUsage(), now );
}


bool Nepomuk2::Resource::isFile() const
{
    if( m_data ) {
        determineFinalResourceData();
        m_data->load();
        return m_data->isFile();
    }
    else {
        return false;
    }
}


Nepomuk2::File Nepomuk2::Resource::toFile() const
{
    return File( *this );
}

void Nepomuk2::Resource::setWatchEnabled(bool status)
{
    determineFinalResourceData();
    if( m_data )
        return m_data->setWatchEnabled( status );
}

bool Nepomuk2::Resource::watchEnabled()
{
    determineFinalResourceData();
    if( m_data )
        return m_data->watchEnabled();

    return false;
}


// static
Nepomuk2::Resource Nepomuk2::Resource::fromResourceUri( const KUrl& uri, const Nepomuk2::Types::Class& type )
{
    ResourceManager* manager = ResourceManager::instance();
    QMutexLocker lock( &manager->d->mutex );
    return Resource( manager->d->dataForResourceUri( uri, type.uri() ) );
}


void Nepomuk2::Resource::determineFinalResourceData() const
{
    if (!m_data->uri().isEmpty()) {
        return;
    }

    // Get an initialized ResourceData instance
    ResourceData* oldData = m_data;

    m_data->determineUri(); // note that this can change the value of m_data

    if ( !oldData->cnt() )
        delete oldData;
}


uint Nepomuk2::qHash( const Resource& res )
{
    return qHash(res.uri());
}
