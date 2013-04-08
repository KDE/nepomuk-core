/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

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

#include "backupgenerationjob.h"
#include "backupfile.h"
#include "simpleresource.h"
#include "resourcelistgenerator.h"
#include "statementgenerator.h"
#include "graphgenerator.h"

#include <Soprano/PluginManager>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>

#include <QtCore/QTimer>
#include <QtCore/QSettings>

#include <KTar>
#include <KTemporaryFile>
#include <KDebug>
#include <KTempDir>

Nepomuk2::BackupGenerationJob::BackupGenerationJob(Soprano::Model *model, const QUrl& url, QObject* parent)
    : KJob(parent)
    , m_model(model)
    , m_url( url )
    , m_filter( Filter_None )
{
}

void Nepomuk2::BackupGenerationJob::start()
{
    QTimer::singleShot( 0, this, SLOT(doWork()) );
}

void Nepomuk2::BackupGenerationJob::setFilter(Nepomuk2::BackupGenerationJob::Filter filter)
{
    m_filter = filter;
}


void Nepomuk2::BackupGenerationJob::doWork()
{
    KTemporaryFile uriListFile;
    uriListFile.open();

    Backup::ResourceListGenerator* uriListGen = new Backup::ResourceListGenerator( m_model, uriListFile.fileName(), this );
    if( m_filter == Filter_TagsAndRatings )
        uriListGen->setFilter( Backup::ResourceListGenerator::Filter_FilesAndTags );
    uriListGen->exec();

    if( uriListGen->error() ) {
        setError(1);
        setErrorText( uriListGen->errorString() );
        emitResult();
        return;
    }

    KTemporaryFile stListFile;
    stListFile.open();
    Backup::StatementGenerator* stGen = new Backup::StatementGenerator( m_model, uriListFile.fileName(),
                                                                        stListFile.fileName(), this );
    if( m_filter == Filter_TagsAndRatings )
        stGen->setFilter( Backup::StatementGenerator::Filter_TagsAndRatings );

    stGen->exec();
    if( stGen->error() ) {
        setError(1);
        setErrorText( stGen->errorString() );
        emitResult();
        return;
    }

    KTemporaryFile dataFile;
    dataFile.open();

    Backup::GraphGenerator* graphGen = new Backup::GraphGenerator( m_model, stListFile.fileName(),
                                                                   dataFile.fileName(), this );

    graphGen->exec();
    if( graphGen->error() ) {
        setError(1);
        setErrorText( graphGen->errorString() );
        emitResult();
        return;
    }

    // Metadata
    KTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.setAutoRemove( false );
    QString metdataFile = tmpFile.fileName();
    tmpFile.close();

    QSettings iniFile( metdataFile, QSettings::IniFormat );
    iniFile.setValue("NumStatements", graphGen->numStatements());
    iniFile.setValue("Created", QDateTime::currentDateTime().toString() );
    iniFile.sync();

    // Push to tar file
    KTar tarFile( m_url.toLocalFile(), QString::fromLatin1("application/x-gzip") );
    if( !tarFile.open( QIODevice::WriteOnly ) ) {
        kWarning() << "File could not be opened : " << m_url.toLocalFile();
        return;
    }

    tarFile.addLocalFile( dataFile.fileName(), "data" );
    tarFile.addLocalFile( metdataFile, "metadata" );

    emitResult();
}



