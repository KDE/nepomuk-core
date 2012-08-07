/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "datagenerator.h"
#include "datamanagement.h"
#include "storeresourcesjob.h"

#include "nie.h"
#include "nfo.h"
#include "nco.h"
#include "nmm.h"

#include <KUrl>
#include <KDebug>

#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {
namespace Test {

DataGenerator::DataGenerator(int n)
    : m_numFiles(n)
{
}

DataGenerator::~DataGenerator()
{
}


SimpleResourceGraph DataGenerator::generateGraph()
{
    qsrand( QDateTime::currentMSecsSinceEpoch() );

    KTempDir dir;
    dir.setAutoRemove( false );

    // Fow now, doing it all in one go
    SimpleResourceGraph graph;
    for( int i=0; i<m_numFiles; i++) {
        int n = qrand() % 2;
        QUrl fileUrl = QUrl::fromLocalFile( dir.name() + "/" + QString::number(i) );
        switch( n ) {
            case 0:
                graph += createPlainTextFile(fileUrl, generateName());
                break;
            case 1:
                graph += createMusicFile(fileUrl, generateName(), generateName(), generateName() );
                break;
            case 2:
                graph += createImageFile(fileUrl);
        }
    }

    return graph;
}

bool DataGenerator::generate()
{
    StoreResourcesJob* job = generateGraph().save();
    job->exec();
    if( job->error() ) {
        kDebug() << job->error();
        return false;
    }

    return true;
}

QString DataGenerator::generateName()
{
    // FIXME: Use something better
    return QUuid::createUuid().toString();
}

SimpleResourceGraph DataGenerator::createPlainTextFile(const QUrl& url, const QString& content)
{
    QFile file( url.toLocalFile() );
    file.open( QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text );

    // Does this really matter?
    QTextStream stream(&file);
    stream << content;

    file.close();

    SimpleResource fileRes;
    fileRes.addType( NFO::TextDocument() );
    fileRes.addType( NFO::FileDataObject() );
    fileRes.addProperty( NIE::plainTextContent(), content );
    fileRes.addProperty( NIE::mimeType(), QLatin1String("text/plain") );
    fileRes.addProperty( NIE::url(), url );
    fileRes.addProperty( NFO::fileName(), KUrl(url).fileName() );

    return SimpleResourceGraph() << fileRes;
}

namespace {
    void createFile(const QUrl& url) {
        QFile file( url.toLocalFile() );
        file.open( QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text );
        file.close();
    }
}
SimpleResourceGraph DataGenerator::createMusicFile(const QUrl& url, const QString& title,
                                                   const QString& artistName, const QString& albumName)
{
    createFile( url );

    SimpleResource artistRes;
    artistRes.addType( NCO::Contact() );
    artistRes.setProperty( NCO::fullname(), artistName );

    SimpleResource albumRes;
    albumRes.addType( NMM::MusicAlbum() );
    albumRes.setProperty( NIE::title(), albumName );

    SimpleResource fileRes;
    fileRes.addType( NMM::MusicPiece() );
    fileRes.addType( NFO::FileDataObject() );
    fileRes.addProperty( NIE::title(), title );
    fileRes.addProperty( NIE::url(), url );
    fileRes.addProperty( NFO::fileName(), KUrl(url).fileName() );
    fileRes.addProperty( NMM::performer(), artistRes );
    fileRes.addProperty( NMM::musicAlbum(), albumRes );

    SimpleResourceGraph graph;
    graph << artistRes << albumRes << fileRes;

    return graph;
}

SimpleResourceGraph DataGenerator::createImageFile(const QUrl& url)
{
    createFile( url );

    return SimpleResourceGraph();
}

}
}
