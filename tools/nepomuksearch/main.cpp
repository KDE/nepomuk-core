/*
   Copyright (c) 2013 Vishesh Handa <me@vhanda.in>

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
#include <QtCore/QUrl>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <KDE/KLocale>
#include <KComponentData>
#include <KUrl>
#include <KDebug>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>

#include "nie.h"
#include "nmo.h"
#include "query.h"
#include "resultiterator.h"
#include "queryparser.h"
#include "resourcemanager.h"
#include "resource.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;
using namespace Nepomuk2;

QString highlightBold(const QString& input) {
    QLatin1String colorStart("\033[0;33m");
    QLatin1String colorEnd("\033[0;0m");

    QString out(input);
    out.replace( "<b>", colorStart );
    out.replace( "</b>", colorEnd );

    return out;
}

QString colorString(const QString& input, int color) {
    QString colorStart = QString::fromLatin1("\033[0;%1m").arg( color );
    QLatin1String colorEnd("\033[0;0m");

    return colorStart + input + colorEnd;
}

QString fetchProperty(const QUrl& uri, const QUrl& prop) {
    QString query = QString::fromLatin1("select ?o where { %1 %2 ?o . } LIMIT 1")
                    .arg( Soprano::Node::resourceToN3(uri),
                          Soprano::Node::resourceToN3(prop) );

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    if( it.next() )
        return it[0].toString();

    return QString();
}

int main( int argc, char *argv[] )
{
    KAboutData aboutData( "nepomuksearch",
                          "nepomuksearch",
                          ki18n("Nepomuk Search"),
                          "0.1",
                          ki18n("Nepomuk Search - A debugging tool"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2013, Vishesh Handa"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");
    aboutData.setProgramIconName( "nepomuk" );

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    options.add("+query", ki18n("The words to search for"));
    KCmdLineArgs::addCmdLineOptions( options );

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    if( args->count() == 0 ) {
        KCmdLineArgs::usage();
        return 1;
    }

    QCoreApplication app( argc, argv );
    KComponentData comp( aboutData );
    KGlobal::locale()->insertCatalog("libnepomukcore");

    QTextStream out( stdout );

    ResourceManager* rm = ResourceManager::instance();
    if( !rm->initialized() ) {
        out << "Could not connect to Nepomuk";
        return 1;
    }

    // HACK: Find a better way!
    QStringList argList = args->allArguments();
    argList.takeFirst();
    QString queryStr = argList.join(" ");

    Query::Query query = Query::QueryParser::parseQuery( queryStr, Query::QueryParser::DetectFilenamePattern );
    query.addRequestProperty( Query::Query::RequestProperty( NIE::url(), false ) );
    query.setQueryFlags( Query::Query::WithFullTextExcerpt );
    query.setLimit( 10 );

    out << "\n";
    Query::ResultIterator iter( query );
    while( iter.next() ) {
        Query::Result result = iter.current();
        const QUrl url = result.requestProperty( NIE::url() ).uri();

        QString title;
        if( url.isLocalFile() ) {
            title = colorString(url.toLocalFile(), 32);
        }
        else {
            QUrl resUri = result.resource().uri();
            title = colorString(url.toString(), 32) + "  " +
                    colorString(fetchProperty(resUri, NMO::sentDate()), 31) + " " +
                    colorString(fetchProperty(resUri, NAO::prefLabel()), 32);
        }

        out << "  " << title << endl;
        out << "  " << highlightBold( result.excerpt() ) << endl;
        out << endl;
    }

    return 0;
}
