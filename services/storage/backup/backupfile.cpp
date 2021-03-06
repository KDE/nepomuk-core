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

#include <QtCore/QFile>
#include <QtCore/QMutableListIterator>
#include <QtCore/QSettings>

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

    // Metadata
    QString metadataFileUrl = tempDir.name() + QLatin1String("metadata");
    QSettings iniFile( metadataFileUrl, QSettings::IniFormat );

    bf.m_numStatements = iniFile.value("NumStatements").toInt();
    bf.m_created = QDateTime::fromString( iniFile.value("Created").toString() );

    return bf;
}

QDateTime BackupFile::created()
{
    return m_created;
}

int BackupFile::numStatements()
{
    return m_numStatements;
}



}
