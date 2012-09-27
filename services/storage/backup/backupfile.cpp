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


#include "backupfile.h"
#include "backupstatementiterator.h"

#include <QtCore/QFile>
#include <QtCore/QMutableListIterator>

#include <Soprano/Node>
#include <Soprano/PluginManager>
#include <Soprano/Parser>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>

#include <KTar>
#include <KTemporaryFile>
#include <KDebug>
#include <KTempDir>

namespace Nepomuk2 {

BackupFile::BackupFile()
{
}

Soprano::StatementIterator BackupFile::iterator()
{
    return m_stIter;
}

bool BackupFile::createBackupFile(const QUrl& url, BackupStatementIterator& it)
{
    KTemporaryFile tempFile;
    tempFile.open();

    QFile file( tempFile.fileName() );
    if( !file.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        kWarning() << "File couldn't be opened for saving : " << url;
        return false;
    }

    QTextStream out( &file );

    const Soprano::Serializer * serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization( Soprano::SerializationNQuads );
    while( it.next() ) {
        QList<Soprano::Statement> stList;
        stList << it.current();
        kDebug() << stList;

        Soprano::Util::SimpleStatementIterator iter( stList );
        serializer->serialize( iter, out, Soprano::SerializationNQuads );
    }
    file.close();

    KTar tarFile( url.toLocalFile(), QString::fromLatin1("application/x-gzip") );
    if( !tarFile.open( QIODevice::WriteOnly ) ) {
        kWarning() << "File could not be opened : " << url.toLocalFile();
        return false;
    }

    tarFile.addLocalFile( tempFile.fileName(), "data" );
    tarFile.close();

    return true;
}

BackupFile BackupFile::fromUrl(const QUrl& url)
{
    KTar tarFile( url.toLocalFile(), QString::fromLatin1("application/x-gzip") );
    if( !tarFile.open( QIODevice::ReadOnly ) ) {
        kWarning() << "File could not be opened : " << url.toLocalFile();
        return BackupFile();
    }

    const KArchiveDirectory * dir = tarFile.directory();
    if( !dir )
        return BackupFile();

    KTempDir tempDir;
    dir->copyTo( tempDir.name() );

    QUrl fileUrl = QUrl::fromLocalFile(tempDir.name() + "data" );
    QFile file( fileUrl.toLocalFile() );
    if( !file.open( QIODevice::ReadOnly ) ) {
        return BackupFile();
    }

    const Soprano::Parser* parser = Soprano::PluginManager::instance()->discoverParserForSerialization( Soprano::SerializationNQuads );

    BackupFile bf;
    bf.m_stIter = parser->parseFile( fileUrl.toLocalFile(), QUrl(), Soprano::SerializationNQuads );
    return bf;
}

}