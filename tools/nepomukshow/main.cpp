/*
   Copyright (c) 2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2012 Vishesh Handa <me@vhanda.in>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QUrl>
#include <QtCore/QSet>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <KDE/KLocale>
#include <KComponentData>
#include <KUrl>
#include <KDebug>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include "resourceprinter.h"
#include "resource.h"
#include "variant.h"
#include "property.h"
#include "resourcemanager.h"

int main( int argc, char *argv[] )
{
    KAboutData aboutData( "nepomukshow",
                          "nepomukshow",
                          ki18n("Nepomuk Show"),
                          "0.1",
                          ki18n("The Nepomuk Resource Viewer - A debugging tool"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2012, Vishesh Handa"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");
    aboutData.addAuthor(ki18n("Sebastian Trüg"),ki18n("Developer"), "trueg@kde.org");
    aboutData.setProgramIconName( "nepomuk" );

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
	options.add("+resource", ki18n("The resource URI, a file URL, or a file path."));
    options.add("depth <depth>", ki18n("The number of sub resources to list"), "0");
    options.add("graphs", ki18n("Print information about the graphs"));
    options.add("inference", ki18n("Uses Inference to print additional properties"));
    options.add("plainText", ki18n("Print the plain text content of the file"));
    KCmdLineArgs::addCmdLineOptions( options );

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    QCoreApplication app( argc, argv );
    KComponentData comp( aboutData );

    if( args->count() == 0 )
        KCmdLineArgs::usage();

    QTextStream err( stdout );

    //
    // The Resource Uri
    //
    const QUrl url = args->url( 0 );
    Nepomuk2::Resource res( url );
    if ( !res.exists() ) {
        err << "Resource does not exist: " << args->url( 0 ).url() << endl;
        return 1;
    }

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    QSet<QUrl> uriList;

    //
    // If nie:url, then fetch the resources
    //
    if( url.scheme() != QLatin1String("nepomuk") ) {
        QString query = QString::fromLatin1("select ?r where { ?r nie:url %1 . }")
                        .arg( Soprano::Node::resourceToN3( url ) );
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        while( it.next() ) {
            uriList << it[0].uri();
        }
    }
    else {
        uriList << res.uri();
    }

    //
    // Fetch additional resource for the depth
    //
    int depth = args->getOption("depth").toInt();
    int oldSize = uriList.size();
    for( int i=0; i<depth; i++ ) {
        QSet<QUrl> newUris;
        foreach(const QUrl& uri, uriList) {
            QString query = QString::fromLatin1("select ?o where { %1 ?p ?o. FILTER(REGEX(STR(?o), '^nepomuk')). }")
                            .arg( Soprano::Node::resourceToN3(uri) );
            Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
            while( it.next() )
                newUris << it[0].uri();
        }
        uriList.unite( newUris );

        if( uriList.size() == oldSize )
            break;
        else
            oldSize = uriList.size();
    }

    //
    // Dump to stdout
    //
    QTextStream stream( stdout );
    Nepomuk2::ResourcePrinter printer;

    if( args->isSet("graphs") )
        printer.setGraphs( true );
    if( args->isSet("inference") )
        printer.setInference( true );
    if( args->isSet("plainText") )
        printer.setPlainText( true );

    foreach( const QUrl& resUri, uriList ) {
        printer.print( stream, resUri );
    }

    return 0;
}
