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
    uriListFile.setAutoRemove( false );
    uriListFile.open();

    Backup::ResourceListGenerator* uriListGen = new Backup::ResourceListGenerator( m_model, uriListFile.fileName(), this );
    uriListGen->setProperty( "filename", uriListFile.fileName() );
    if( m_filter == Filter_TagsAndRatings )
        uriListGen->setFilter( Backup::ResourceListGenerator::Filter_FilesAndTags );

    connect( uriListGen, SIGNAL(finished(KJob*)), this, SLOT(slotResourceListFinished(KJob*)) );
    connect( uriListGen, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotResourceListProgress(KJob*,ulong)) );
    uriListGen->start();

}

void Nepomuk2::BackupGenerationJob::slotResourceListProgress(KJob*, ulong progress)
{
    // The ResourceListGeneration should take about 20% of the total time
    float amt = progress * 0.20;
    kDebug() << amt;
    setPercent( amt );
}

void Nepomuk2::BackupGenerationJob::slotResourceListFinished(KJob* job)
{
    Backup::ResourceListGenerator* uriListGen = static_cast<Backup::ResourceListGenerator*>(job);

    if( uriListGen->error() ) {
        setError(1);
        setErrorText( uriListGen->errorString() );
        emitResult();
        return;
    }

    const QString uriListFile = uriListGen->property( "filename" ).toString();

    KTemporaryFile stListFile;
    stListFile.setAutoRemove( false );
    stListFile.open();

    Backup::StatementGenerator* stGen = new Backup::StatementGenerator( m_model, uriListFile,
                                                                        stListFile.fileName(), this );
    stGen->setProperty( "filename", stListFile.fileName() );
    stGen->setResourceCount( uriListGen->resourceCount() );
    if( m_filter == Filter_TagsAndRatings )
        stGen->setFilter( Backup::StatementGenerator::Filter_TagsAndRatings );

    connect( stGen, SIGNAL(finished(KJob*)), this, SLOT(slotStatementListGenerationFinished(KJob*)) );
    connect( stGen, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotStatementListGeneratorProgress(KJob*,ulong)) );
    stGen->start();
}

void Nepomuk2::BackupGenerationJob::slotStatementListGeneratorProgress(KJob*, ulong progress)
{
    // The StatementGenerator should take about 40% of the time
    float amt = 20 + (progress*0.40);
    kDebug() << "FF" << amt;
    setPercent( amt );
}

void Nepomuk2::BackupGenerationJob::slotStatementListGenerationFinished(KJob* job)
{
    Backup::StatementGenerator* stGen = static_cast<Backup::StatementGenerator*>(job);
    if( stGen->error() ) {
        setError(1);
        setErrorText( stGen->errorString() );
        emitResult();
        return;
    }

    KTemporaryFile dataFile;
    dataFile.setAutoRemove( false );
    dataFile.open();

    const QString stListFile = stGen->property( "filename" ).toString();
    Backup::GraphGenerator* graphGen = new Backup::GraphGenerator( m_model, stListFile,
                                                                   dataFile.fileName(), this );

    connect( graphGen, SIGNAL(finished(KJob*)), this, SLOT(slotGraphGenerationFinished(KJob*)) );
    connect( graphGen, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotGraphGeneratorProgress(KJob*,ulong)) );

    graphGen->setInputCount( stGen->statementCount() );
    graphGen->setProperty( "filename", dataFile.fileName() );
    graphGen->start();
}

void Nepomuk2::BackupGenerationJob::slotGraphGeneratorProgress(KJob*, ulong progress)
{
    float amt = 60 + (progress*0.40);
    kDebug() << amt;
    setPercent( amt );
}

void Nepomuk2::BackupGenerationJob::slotGraphGenerationFinished(KJob* job)
{
    Backup::GraphGenerator* graphGen = static_cast<Backup::GraphGenerator*>(job);
    if( graphGen->error() ) {
        setError(1);
        setErrorText( graphGen->errorString() );
        emitResult();
        return;
    }

    const QString dataFile = graphGen->property("filename").toString();

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

    tarFile.addLocalFile( dataFile, "data" );
    tarFile.addLocalFile( metdataFile, "metadata" );

    emitResult();
}

