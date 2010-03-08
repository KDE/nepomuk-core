/*
  Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this library; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "nepomukindexwriter.h"
#include "util.h"
#include "nfo.h"
#include "nie.h"
#include "nrl.h"

#include <Soprano/Soprano>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/Xesam>
#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/QueryResultIterator>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QVariant>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QThread>
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QUuid>
#include <QtCore/QStack>

#include <KUrl>
#include <KDebug>

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <map>
#include <sstream>
#include <algorithm>

#include <Nepomuk/Types/Property>
#include <Nepomuk/Types/Class>
#include <Nepomuk/Types/Literal>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Resource>


// IMPORTANT: strings in Strigi are apparently UTF8! Except for file names. Those are in local encoding.

using namespace Soprano;
using namespace std;


uint qHash( const std::string& s )
{
    return qHash( s.c_str() );
}

namespace {
    QString findArchivePath( const QString& path ) {
        QString p( path );
        int i = 0;
        while ( ( i = p.lastIndexOf( '/' ) ) > 0 ) {
            p.truncate( i );
            if ( QFileInfo( p ).isFile() ) {
                return p;
            }
        }
        return QString();
    }

    QUrl createFileUrl( const Strigi::AnalysisResult* idx ) {
        // HACK: Strigi includes analysers that recurse into tar or zip archives and index
        // the files therein. In KDE these files could perfectly be handled through kio slaves
        // such as tar:/ or zip:/
        // Here we try to use KDE-compatible URIs for these indexed files the best we can
        // everything else defaults to file:/
        QUrl uri;
        QString path = QFile::decodeName( idx->path().c_str() );
        if ( KUrl::isRelativeUrl( path ) )
            uri = QUrl::fromLocalFile( QFileInfo( path ).absoluteFilePath() );
        else
            uri = KUrl( path ); // try to support http and other URLs

        if ( idx->depth() > 0 ) {
            QString archivePath = findArchivePath( path );
            if ( QFile::exists( archivePath ) ) {
                if ( archivePath.endsWith( QLatin1String( ".tar" ) ) ||
                     archivePath.endsWith( QLatin1String( ".tar.gz" ) ) ||
                     archivePath.endsWith( QLatin1String( ".tar.bz2" ) ) ||
                     archivePath.endsWith( QLatin1String( ".tar.lzma" ) ) ||
                     archivePath.endsWith( QLatin1String( ".tar.xz" ) ) ) {
                    uri.setScheme( "tar" );
                }
                else if ( archivePath.endsWith( QLatin1String( ".zip" ) ) ) {
                    uri.setScheme( "zip" );
                }
            }
        }

        // fallback for all
        if ( uri.scheme().isEmpty() ) {
            uri.setScheme( "file" );
        }

        return uri;
    }

    /**
     * Data objects that are used to store information relative to one
     * indexing run.
     */
    class FileMetaData
    {
    public:
        FileMetaData( const Strigi::AnalysisResult* idx );

        /// stores basic data including the nie:url and the nrl:GraphMetadata in \p model
        void storeBasicData( Soprano::Model* model );

        /// map a blank node to a resource
        QUrl mapNode( const std::string& s );

        /// The resource URI
        QUrl resourceUri;

        /// The file URL (nie:url)
        KUrl fileUrl;

        /// The file info - saved to prevent multiple stats
        QFileInfo fileInfo;

        /// The URI of the graph that contains all indexed statements
        QUrl context;

        /// a buffer for all plain-text content generated by strigi
        std::string content;

    private:
        /// The Strigi result
        const Strigi::AnalysisResult* m_analysisResult;

        /// mapping from blank nodes used in addTriplet to our urns
        QMap<std::string, QUrl> m_blankNodeMap;
    };

    class RegisteredFieldData
    {
    public:
        RegisteredFieldData( const QUrl& prop, QVariant::Type t )
            : property( prop ),
              dataType( t ),
              isRdfType( prop == Vocabulary::RDF::type() ) {
        }

        /// The actual property URI
        QUrl property;

        /// the literal range of the property (if applicable)
        QVariant::Type dataType;

        /// caching QUrl comparison
        bool isRdfType;
    };

    FileMetaData::FileMetaData( const Strigi::AnalysisResult* idx )
        : m_analysisResult( idx )
    {
        fileUrl = createFileUrl( idx );
        fileInfo = fileUrl.toLocalFile();

        // determine the resource URI by using Nepomuk::Resource's power
        // this will automatically find previous uses of the file in question
        // with backwards compatibility
        resourceUri = Nepomuk::Resource( fileUrl ).resourceUri();

        // use a new random context URI
        context = Nepomuk::ResourceManager::instance()->generateUniqueUri( "ctx" );
    }

    QUrl FileMetaData::mapNode( const std::string& s )
    {
        if ( s[0] == ':' ) {
            if( m_blankNodeMap.contains( s ) ) {
                return m_blankNodeMap[s];
            }
            else {
                QUrl urn = Nepomuk::ResourceManager::instance()->generateUniqueUri( QString() );
                m_blankNodeMap.insert( s, urn );
                return urn;
            }
        }
        // special case to properly handle nie:isPartOf relations created for containers
        else if ( s == m_analysisResult->path() ) {
            return resourceUri;
        }
        else {
            return QUrl::fromEncoded( s.c_str() );
        }
    }

    void FileMetaData::storeBasicData( Soprano::Model* model )
    {
        model->addStatement( resourceUri, Nepomuk::Vocabulary::NIE::url(), fileUrl, context );

        // Strigi only indexes files and extractors mostly (if at all) store the nie:DataObject type (i.e. the contents)
        // Thus, here we go the easy way and mark each indexed file as a nfo:FileDataObject.
        model->addStatement( resourceUri,
                             Vocabulary::RDF::type(),
                             Nepomuk::Vocabulary::NFO::FileDataObject(),
                             context );
        if ( fileInfo.isDir() ) {
            model->addStatement( resourceUri,
                                 Vocabulary::RDF::type(),
                                 Nepomuk::Vocabulary::NFO::Folder(),
                                 context );
        }


        // create the provedance data for the data graph
        // TODO: add more data at some point when it becomes of interest
        QUrl metaDataContext = Nepomuk::ResourceManager::instance()->generateUniqueUri( "ctx" );
        model->addStatement( context,
                             Vocabulary::RDF::type(),
                             Nepomuk::Vocabulary::NRL::DiscardableInstanceBase(),
                             metaDataContext );
        model->addStatement( context,
                             Vocabulary::NAO::created(),
                             LiteralValue( QDateTime::currentDateTime() ),
                             metaDataContext );
        model->addStatement( context,
                             Strigi::Ontology::indexGraphFor(),
                             resourceUri,
                             metaDataContext );
        model->addStatement( metaDataContext,
                             Vocabulary::RDF::type(),
                             Nepomuk::Vocabulary::NRL::GraphMetadata(),
                             metaDataContext );
        model->addStatement( metaDataContext,
                             Nepomuk::Vocabulary::NRL::coreGraphMetadataFor(),
                             context,
                             metaDataContext );
    }

    FileMetaData* fileDataForResult( const Strigi::AnalysisResult* idx )
    {
        return static_cast<FileMetaData*>( idx->writerData() );
    }
}


class Strigi::NepomukIndexWriter::Private
{
public:
    Private()
    {
        literalTypes[FieldRegister::stringType] = QVariant::String;
        literalTypes[FieldRegister::floatType] = QVariant::Double;
        literalTypes[FieldRegister::integerType] = QVariant::Int;
        literalTypes[FieldRegister::binaryType] = QVariant::ByteArray;
        literalTypes[FieldRegister::datetimeType] = QVariant::DateTime; // Strigi encodes datetime as unsigned integer, i.e. addValue( ..., uint )
    }

    QVariant::Type literalType( const Strigi::FieldProperties& strigiType ) {
        // it looks as if the typeUri can contain arbitrary values, URIs or stuff like "string"
        QHash<std::string, QVariant::Type>::const_iterator it = literalTypes.constFind( strigiType.typeUri() );
        if ( it == literalTypes.constEnd() ) {
            return LiteralValue::typeFromDataTypeUri( QUrl::fromEncoded( strigiType.typeUri().c_str() ) );
        }
        else {
            return *it;
        }
    }

    LiteralValue createLiteralValue( QVariant::Type type,
                                     const unsigned char* data,
                                     uint32_t size ) {
        QString value = QString::fromUtf8( ( const char* )data, size );
        if ( type == QVariant::DateTime ) { // dataTime is stored as integer in strigi!
            return LiteralValue( QDateTime::fromTime_t( value.toUInt() ) );
        }
        else if ( type != QVariant::Invalid ) {
            return LiteralValue::fromString( value, type );
        }
        else {
            // we default to string
            return LiteralValue( value );
        }
    }

    Soprano::Model* repository;

    //
    // The Strigi API does not provide context information in addTriplet, i.e. the AnalysisResult.
    // However, we only use one thread, only one AnalysisResult at the time.
    // Thus, we can just remember that and use it in addTriplet.
    //

    QStack<const Strigi::AnalysisResult*> currentResultStack;

private:
    QHash<std::string, QVariant::Type> literalTypes;
};


Strigi::NepomukIndexWriter::NepomukIndexWriter( Soprano::Model* model )
    : Strigi::IndexWriter()
{
    d = new Private;
    d->repository = model;
    Util::storeStrigiMiniOntology( d->repository );
}


Strigi::NepomukIndexWriter::~NepomukIndexWriter()
{
    kDebug();
    delete d;
}


// unused
void Strigi::NepomukIndexWriter::commit()
{
    // do nothing
}


// delete all indexed data for the files listed in entries
void Strigi::NepomukIndexWriter::deleteEntries( const std::vector<std::string>& entries )
{
    for ( unsigned int i = 0; i < entries.size(); ++i ) {
        QString path = QString::fromUtf8( entries[i].c_str() );
        removeIndexedData( path );
    }
}


// unused
void Strigi::NepomukIndexWriter::deleteAllEntries()
{
    // do nothing
}


// called for each indexed file
void Strigi::NepomukIndexWriter::startAnalysis( const AnalysisResult* idx )
{
    // we need to remember the AnalysisResult since addTriplet does not provide it
    d->currentResultStack.push(idx);

    // for now we ignore embedded files -> too many false positives and useless query results
    if ( idx->depth() > 0 ) {
        return;
    }

    // create the file data used during the analysis
    FileMetaData* data = new FileMetaData( idx );

    // remove previously indexed data
    removeIndexedData( data->resourceUri, true );

    // It is important to keep the resource URI between updates (especially for sharing of files)
    // However, when updating data from pre-KDE 4.4 times we want to get rid of old file:/ resource
    // URIs. However, we can only do that if we were the only ones to write info about that file
    // Thus, we need to use Nepomuk::Resource again
    if ( data->resourceUri.scheme() == QLatin1String( "file" ) ) {
        // we need to clear the ResourceManager cache. Otherwise the bug/shortcoming in libnepomuk will
        // always get us back to the cached file:/ URI
        Nepomuk::ResourceManager::instance()->clearCache();
        data->resourceUri = Nepomuk::Resource( data->fileUrl ).resourceUri();
    }

    // create a new resource URI for non-existing file resources
    if ( data->resourceUri.isEmpty() )
        data->resourceUri = Nepomuk::ResourceManager::instance()->generateUniqueUri( QString() );

    // store initial data to make sure newly created URIs are reused directly by libnepomuk
    data->storeBasicData( d->repository );

    // remember the file data
    idx->setWriterData( data );
}


void Strigi::NepomukIndexWriter::addText( const AnalysisResult* idx, const char* text, int32_t length )
{
    if ( idx->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( idx );
    md->content.append( text, length );
}


void Strigi::NepomukIndexWriter::addValue( const AnalysisResult* idx,
                                           const RegisteredField* field,
                                           const std::string& value )
{
    if ( idx->depth() > 0 ) {
        return;
    }

    if ( value.length() > 0 ) {
        FileMetaData* md = fileDataForResult( idx );
        RegisteredFieldData* rfd = reinterpret_cast<RegisteredFieldData*>( field->writerData() );

        // the statement we will create, we will determine the object below
        Soprano::Statement statement( md->resourceUri, rfd->property, Soprano::Node(), md->context );

        //
        // Strigi uses rdf:type improperly since it stores the value as a string. We have to
        // make sure it is a resource.
        //
        if ( rfd->isRdfType ) {
            statement.setPredicate( Soprano::Vocabulary::RDF::type() );
            statement.setObject( QUrl::fromEncoded( value.c_str(), QUrl::StrictMode ) );

            // we handle the basic File/Folder typing ourselves
            if ( statement.object().uri() == Nepomuk::Vocabulary::NFO::FileDataObject() ) {
                return;
            }
        }

        else if ( field->key() == FieldRegister::pathFieldName ) {
            // ignore it as we handle that ourselves in startAnalysis
            return;
        }

        else {
            //
            // Like the URIs of the files themselves the folders also need uuid resource URIs.
            //
            if ( field->key() == FieldRegister::parentLocationFieldName ) {
                QUrl folderUri = determineFolderResourceUri( QUrl::fromLocalFile( QFile::decodeName( QByteArray::fromRawData( value.c_str(), value.length() ) ) ) );
                if ( folderUri.isEmpty() )
                    return;
                statement.setObject( folderUri );
            }
            else {
                statement.setObject( d->createLiteralValue( rfd->dataType, ( unsigned char* )value.c_str(), value.length() ) );
            }

            //
            // Strigi uses anonymeous nodes prefixed with ':'. However, it is possible that literals
            // start with a ':'. Thus, we also check the range of the property
            //
            if ( value[0] == ':' ) {
                Nepomuk::Types::Property property( rfd->property );
                if ( property.range().isValid() ) {
                    statement.setObject( md->mapNode( value ) );
                }
            }
        }

        d->repository->addStatement( statement );
    }
}


// the main addValue method
void Strigi::NepomukIndexWriter::addValue( const AnalysisResult* idx,
                                           const RegisteredField* field,
                                           const unsigned char* data,
                                           uint32_t size )
{
    addValue( idx, field, std::string( ( const char* )data, size ) );
}


void Strigi::NepomukIndexWriter::addValue( const AnalysisResult*, const RegisteredField*,
                                           const std::string&, const std::string& )
{
    // we do not support map types
}


void Strigi::NepomukIndexWriter::addValue( const AnalysisResult* idx,
                                           const RegisteredField* field,
                                           uint32_t value )
{
    if ( idx->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( idx );
    RegisteredFieldData* rfd = reinterpret_cast<RegisteredFieldData*>( field->writerData() );

    LiteralValue val( value );
    if ( field->type() == FieldRegister::datetimeType ) {
        val = QDateTime::fromTime_t( value );
    }

    d->repository->addStatement( Statement( md->resourceUri,
                                            rfd->property,
                                            val,
                                            md->context) );
}


void Strigi::NepomukIndexWriter::addValue( const AnalysisResult* idx,
                                           const RegisteredField* field,
                                           int32_t value )
{
    if ( idx->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( idx );
    RegisteredFieldData* rfd = reinterpret_cast<RegisteredFieldData*>( field->writerData() );

    d->repository->addStatement( Statement( md->resourceUri,
                                            rfd->property,
                                            LiteralValue( value ),
                                            md->context) );
}


void Strigi::NepomukIndexWriter::addValue( const AnalysisResult* idx,
                                           const RegisteredField* field,
                                           double value )
{
    if ( idx->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( idx );
    RegisteredFieldData* rfd = reinterpret_cast<RegisteredFieldData*>( field->writerData() );

    d->repository->addStatement( Statement( md->resourceUri,
                                            rfd->property,
                                            LiteralValue( value ),
                                            md->context) );
}


void Strigi::NepomukIndexWriter::addTriplet( const std::string& s,
                                             const std::string& p,
                                             const std::string& o )
{
    if ( d->currentResultStack.top()->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( d->currentResultStack.top() );

    QUrl subject = md->mapNode( s );
    Nepomuk::Types::Property property( md->mapNode( p ) );
    Soprano::Node object;
    if ( property.range().isValid() )
        object = md->mapNode( o );
    else
        object = Soprano::LiteralValue::fromString( QString::fromUtf8( o.c_str() ), property.literalRangeType().dataTypeUri() );

    d->repository->addStatement( subject, property.uri(), object, md->context );
}


// called after each indexed file
void Strigi::NepomukIndexWriter::finishAnalysis( const AnalysisResult* idx )
{
    d->currentResultStack.pop();

    if ( idx->depth() > 0 ) {
        return;
    }

    FileMetaData* md = fileDataForResult( idx );

    // store the full text of the file
    if ( md->content.length() > 0 ) {
        d->repository->addStatement( Statement( md->resourceUri,
                                                Nepomuk::Vocabulary::NIE::plainTextContent(),
                                                LiteralValue( QString::fromUtf8( md->content.c_str() ) ),
                                                md->context ) );
        if ( d->repository->lastError() )
            kDebug() << "Failed to add" << md->resourceUri << "as text" << QString::fromUtf8( md->content.c_str() );
    }

    // cleanup
    delete md;
    idx->setWriterData( 0 );
}


void Strigi::NepomukIndexWriter::initWriterData( const Strigi::FieldRegister& f )
{
    map<string, RegisteredField*>::const_iterator i;
    map<string, RegisteredField*>::const_iterator end = f.fields().end();
    for (i = f.fields().begin(); i != end; ++i) {
        QUrl prop = Util::fieldUri( i->second->key() );
        i->second->setWriterData( new RegisteredFieldData( prop,
                                                           prop == Vocabulary::RDF::type()
                                                           ? QVariant::Invalid
                                                           : d->literalType( i->second->properties() ) ) );
    }
}


void Strigi::NepomukIndexWriter::releaseWriterData( const Strigi::FieldRegister& f )
{
    map<string, RegisteredField*>::const_iterator i;
    map<string, RegisteredField*>::const_iterator end = f.fields().end();
    for (i = f.fields().begin(); i != end; ++i) {
        delete static_cast<RegisteredFieldData*>( i->second->writerData() );
        i->second->setWriterData( 0 );
    }
}


void Strigi::NepomukIndexWriter::removeIndexedData( const KUrl& url, bool isResourceUri )
{
//    kDebug() << url;

    if ( url.isEmpty() )
        return;

    //
    // We are compatible with old Xesam data where the url was encoded as a string instead of a url,
    // thus the weird query
    //
    QString query;
    if ( isResourceUri ) {
         query = QString::fromLatin1( "select ?g where { ?g %1 %2 . }" )
                    .arg( Soprano::Node::resourceToN3( Strigi::Ontology::indexGraphFor() ),
                          Soprano::Node::resourceToN3( url ) );
    }
    else {
        query = QString::fromLatin1( "select ?g where { "
                                     "{ ?r %3 %1 . } "
                                     "UNION "
                                     "{ ?r %3 %2 . } "
                                     "UNION "
                                     "{ ?r %4 %2 . } . "
                                     "?g %5 ?r . }" )
                    .arg( Node::literalToN3( url.path() ),
                          Node::resourceToN3( url ),
                          Node::resourceToN3( Vocabulary::Xesam::url() ),
                          Node::resourceToN3( Nepomuk::Vocabulary::NIE::url() ),
                          Node::resourceToN3( Strigi::Ontology::indexGraphFor() ) );
    }

//        kDebug() << "deleteEntries query:" << query;

    QueryResultIterator result = d->repository->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while ( result.next() ) {
        Node indexGraph = result.binding( "g" );

        // delete the indexed data (The Soprano::NRLModel in the storage service will take care of
        // the metadata graph)
        d->repository->removeContext( indexGraph );
    }
}


QUrl Strigi::NepomukIndexWriter::determineFolderResourceUri( const KUrl& fileUrl )
{
    Nepomuk::Resource res( fileUrl );
    if ( res.exists() ) {
        return res.resourceUri();
    }
//     QString query = QString::fromLatin1( "select ?r where { ?r %1 %2 . }" )
//                     .arg( Node::resourceToN3( Nepomuk::Vocabulary::NIE::url() ),
//                           Node::resourceToN3( fileUrl ) );
//     QueryResultIterator result = d->repository->executeQuery( query, Soprano::Query::QueryLanguageSparql );
//     if ( result.next() ) {
//         return result["r"].uri();
//     }
    else {
        kDebug() << "Could not find resource URI for folder (this is not an error)" << fileUrl;
        return QUrl();
    }
}
