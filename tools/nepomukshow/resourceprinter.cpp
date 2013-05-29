/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "resourceprinter.h"
#include "uri.h"

#include <QtCore/QSet>
#include <QDateTime>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NRL>

#include "resourcemanager.h"
#include "nie.h"
#include "nmo.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

namespace {

    struct SyncResource {
        QUrl uri;
        QMultiHash<QUrl, Soprano::Node> prop;
    };

    SyncResource createSyncResource(const QUrl& uri, bool inference = false) {
        SyncResource syncRes;
        syncRes.uri = uri;

        QString query = QString::fromLatin1("select ?p ?o where { %1 ?p ?o. }")
                        .arg( Soprano::Node::resourceToN3( uri ) );

        Soprano::Query::QueryLanguage lang;
        if( inference )
            lang = Soprano::Query::QueryLanguageSparql;
        else
            lang = Soprano::Query::QueryLanguageSparqlNoInference;

        Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, lang );
        while( it.next() ) {
            Soprano::Node object = it[1];
            syncRes.prop.insertMulti( it[0].uri(), object );
        }

        return syncRes;
    }

    class TablePrinter {
    public:
        TablePrinter( int columns ) {
            m_columns = columns;

            // Isn't there a better way?
            for( int i = 0; i<columns; i++ ) {
                m_data.append( QStringList() );
                m_dataMaxLength.append( 0 );
            }
        }

        void insert( int column, const QString& data ) {
            m_data[ column ] << data;
            if( data.length() > m_dataMaxLength[column] )
                m_dataMaxLength[column] = data.length();
        }

        void print( QTextStream& stream ) {
            int rows = m_data[0].size();

            for( int i=0; i<rows; i++ ) {
                // For the initial indentation - Make it configurable
                stream << QString( 8, QChar::fromAscii(' ') );
                for( int c=0; c<m_columns; c++ ) {
                    stream << m_data[c][i];

                    int numSpaces = m_dataMaxLength[c] - m_data[c][i].length();
                    numSpaces += 2;

                    stream << QString( numSpaces, QChar::fromAscii(' ') );
                }
                stream << "\n";
            }
        }

    private:
        int m_columns;

        QList<QStringList> m_data;
        QList<int> m_dataMaxLength;
    };

    QList<QUrl> sortProperties(const QList<QUrl>& properties) {
        // Group the keys based on prefix
        QMultiHash<QString, QUrl> groupedProperties;
        foreach( const QUrl& propUri, properties ) {
            groupedProperties.insert( Nepomuk2::Uri(propUri).prefix(), propUri );
        }

        // Print on the basis of a custom sort
        QList<QUrl> sortedKeys;

        // Custom Sort
        sortedKeys += groupedProperties.values("rdf");
        sortedKeys += groupedProperties.values("rdfs");
        sortedKeys += groupedProperties.values("nrl");
        sortedKeys += groupedProperties.values("nao");
        sortedKeys += groupedProperties.values("nie");
        sortedKeys += groupedProperties.values("nfo");
        sortedKeys += groupedProperties.values("nmm");
        sortedKeys += groupedProperties.values("kext");
        sortedKeys += properties.toSet().subtract( sortedKeys.toSet() ).toList(); // Remaining

        return sortedKeys;
    }


    QString nodeToN3(const Soprano::Node& node) {
        QString str;
        if( node.isResource() ) {
            if( node.uri().scheme() == QLatin1String("nepomuk") || node.uri().scheme() == QLatin1String("file") )
                str = node.uri().toString();
            else
                str = Nepomuk2::Uri( node.uri() ).toN3();
        }
        else {
            str = node.literal().toString();
        }
        return str;
    }

    QString printGraphData(const QUrl& graphUri) {
        SyncResource res = createSyncResource(graphUri);

        QString app;
        QDateTime created;
        bool discardable = false;

        if( res.prop.contains(NAO::maintainedBy()) ) {
            QString query = QString::fromLatin1("select ?i where { %1 nao:identifier ?i. }")
                            .arg( res.prop.value(NAO::maintainedBy()).toN3() );
            Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
            Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
            if( it.next() ) {
                app = it[0].literal().toString();
            }
        }

        if( res.prop.contains(NAO::created()) ) {
            created = res.prop.value(NAO::created()).literal().toDateTime();
        }

        if( res.prop.contains(RDF::type(), Soprano::Node(NRL::DiscardableInstanceBase())) ) {
            discardable = true;
        }

        QString str = "| " + app + " | " + created.toLocalTime().toString( Qt::ISODate );
        if( discardable )
            str += " | D";

        return str;
    }

    void print(QTextStream& stream, const SyncResource& syncRes,
               bool printGraphInfo, bool printPlainText) {
        // Print the uri
        stream << "\n" << Soprano::Node::resourceToN3( syncRes.uri ) << "\n";

        TablePrinter tablePrinter( printGraphInfo ? 3: 2 );

        // Print on the basis of a custom sort
        QList<QUrl> sortedKeys = sortProperties( syncRes.prop.uniqueKeys() );
        foreach( const QUrl& propUri, sortedKeys ) {
            QList<Soprano::Node> values = syncRes.prop.values( propUri );
            foreach(const Soprano::Node& node, values) {
                if( !printPlainText ) {
                    if( propUri == NIE::plainTextContent() || propUri == NMO::plainTextMessageContent() )
                        continue;
                }

                QString propery = Nepomuk2::Uri( propUri ).toN3();
                QString object = nodeToN3( node );

                tablePrinter.insert( 0, propery );
                tablePrinter.insert( 1, object );

                if( printGraphInfo ) {
                    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
                    QString query = QString::fromLatin1("select ?g where { graph ?g { %1 %2 %3. } }")
                                    .arg( Soprano::Node::resourceToN3(syncRes.uri),
                                          Soprano::Node::resourceToN3(propUri),
                                          node.toN3() );
                    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
                    if( it.next() )
                        tablePrinter.insert( 2, printGraphData( it[0].uri() ) );
                    else
                        tablePrinter.insert( 2, "---");
                }
            }
        }

        tablePrinter.print( stream );
    }

}

Nepomuk2::ResourcePrinter::ResourcePrinter()
{
    m_printGraphs = false;
    m_inference = false;
    m_printPlainText = false;
}


void Nepomuk2::ResourcePrinter::print(QTextStream& stream, const QUrl& uri)
{
    ::print( stream, createSyncResource(uri, m_inference), m_printGraphs, m_printPlainText );
}

void Nepomuk2::ResourcePrinter::setGraphs(bool status)
{
    m_printGraphs = status;
}

void Nepomuk2::ResourcePrinter::setInference(bool status)
{
    m_inference = status;
}

void Nepomuk2::ResourcePrinter::setPlainText(bool status)
{
    m_printPlainText = status;
}
