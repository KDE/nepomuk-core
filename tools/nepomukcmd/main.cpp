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
#include <QtCore/QHash>
#include <QtCore/QUrl>
#include <QtCore/QSet>
#include <QTime>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <KDE/KLocale>
#include <KComponentData>
#include <KUrl>
#include <KDebug>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include "resourcemanager.h"

int main( int argc, char *argv[] )
{
    KAboutData aboutData( "nepomukcmd",
                          "nepomukcmd",
                          ki18n("Nepomuk Cmd"),
                          "0.1",
                          ki18n("Nepomuk Command - A debugging tool"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2013, Vishesh Handa"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");
    aboutData.setProgramIconName( "nepomuk" );

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
	options.add("+command", ki18n("The command to use"));
    options.add("inference", ki18n("Uses Inference to print additional properties"));
    KCmdLineArgs::addCmdLineOptions( options );

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    QCoreApplication app( argc, argv );
    KComponentData comp( aboutData );

    if( args->count() == 0 )
        KCmdLineArgs::usage();

    QTextStream err( stdout );

    const QString command = args->arg( 0 ).toLower();
    if ( command != QLatin1String("query") ) {
        err << "Command " << command << " is not supported";
        return 1;
    }

    const QString query = args->arg( 1 );
    Soprano::Query::QueryLanguage lang = Soprano::Query::QueryLanguageSparqlNoInference;
    if ( args->isSet("inference") )
        lang = Soprano::Query::QueryLanguageSparql;

    Nepomuk2::ResourceManager* rm = Nepomuk2::ResourceManager::instance();
    if( !rm->initialized() ) {
        err << "Could not connect to Nepomuk";
        return 1;
    }

    Soprano::Model* model = rm->mainModel();
    QTextStream stream( stdout );

    int resultCount = 0;
    QTime timer;
    timer.start();
    Soprano::QueryResultIterator it = model->executeQuery( query, lang );
    int queryTime = timer.elapsed();
    while( it.next() ) {
        QStringList bindValues;
        foreach(const QString& binding, it.bindingNames()) {
            bindValues << QString::fromLatin1("%1 -> %2").arg( binding, it[binding].toN3() );
        }

        stream << bindValues.join("; ") << endl;
        resultCount++;
    }
    int totalTime = timer.elapsed();

    stream << "Total Results: " << resultCount << endl;
    stream << "Execution Time: " << QTime().addMSecs( queryTime ).toString( "hh:mm:ss.zz" ) << endl;
    stream << "Total Time: " << QTime().addMSecs( totalTime ).toString( "hh:mm:ss.zz" ) << endl;

    return 0;
}
