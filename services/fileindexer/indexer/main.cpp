/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-11 Vishesh Handa <handa.vish@gmail.com>
   Copyright (C) 2010-2011 Sebastian Trueg <trueg@kde.org>

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

#include "indexer.h"
#include "../util.h"
#include "../../../servicestub/priority.h"
#include "nepomukversion.h"
#include "nie.h"
#include "simpleresourcegraph.h"

#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <KComponentData>
#include <KApplication>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTextStream>

#include <iostream>
#include <KDebug>
#include <KUrl>
#include <KJob>

using namespace Nepomuk2::Vocabulary;

int main(int argc, char *argv[])
{
    lowerIOPriority();
    lowerSchedulingPriority();
    lowerPriority();

    KAboutData aboutData("nepomukindexer", 0, ki18n("NepomukIndexer"),
                         NEPOMUK_VERSION_STRING,
                         ki18n("NepomukIndexer indexes the contents of a file and saves the results in Nepomuk"),
                         KAboutData::License_LGPL_V2,
                         ki18n("(C) 2011, Vishesh Handa, Sebastian Trueg"));
    aboutData.addAuthor(ki18n("Vishesh Handa"), ki18n("Current maintainer"), "handa.vish@gmail.com");
    aboutData.addCredit(ki18n("Sebastian Trüg"), ki18n("Developer"), "trueg@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("+[url]", ki18n("The URL of the file to be indexed"));
    options.add("clear", ki18n("Remove all indexed data of the URL provided"));
    options.add("data", ki18n("Streams the indexed data to stdout"));

    KCmdLineArgs::addCmdLineOptions(options);
    const KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    // Application
    QCoreApplication app( argc, argv );
    KComponentData data( aboutData, KComponentData::RegisterAsMainComponent );

    if( args->count() == 0 ) {
        QTextStream err( stderr );
        err << "Must input url of the file to be indexed";

        return 1;
    }

    if( args->isSet("clear") ) {
        KJob* job = Nepomuk2::clearIndexedData( args->url(0) );
        job->exec();
        if( job->error() ) {
            kDebug() << job->errorString();
            return 1;
        }
        kDebug() << "Removed indexed data for" << args->url(0);
        return 0;
    }

    Nepomuk2::Indexer indexer;

    if( args->isSet("data") ) {
        Nepomuk2::SimpleResourceGraph graph = indexer.indexFileGraph( args->url(0) );

        // Optimization, in this case we generally don't care about the plain text content
        graph.removeAll( QUrl(), NIE::plainTextContent() );

        QByteArray byteArray;
        QDataStream s(&byteArray, QIODevice::WriteOnly);
        s << graph;

        std::cout << byteArray.toBase64().constData();

        return 0;
    }

    // Normal Indexing
    if( !indexer.indexFile( args->url(0) ) ) {
        QTextStream s(stdout);
        s << indexer.lastError();

        return 1;
    }

    return 0;
}
