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


#include "cleaningjobs.h"
#include "resource.h"
#include "variant.h"
#include "resourcemanager.h"
#include "datamanagement.h"
#include "createresourcejob.h"

#include <QtCore/QTimer>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>

#include "nie.h"
#include "nfo.h"

#include <KDebug>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

CleaningJob::CleaningJob(QObject* parent)
    : KJob(parent)
    , m_status(CleaningJob::Waiting)
{
    setAutoDelete( false );
}

CleaningJob::~CleaningJob()
{
}

CleaningJob::Status CleaningJob::status() const
{
    return m_status;
}

void CleaningJob::start() {
    QTimer::singleShot( 0, this, SLOT(slotStartExecution()) );
}

void CleaningJob::slotStartExecution()
{
    m_status = Started;
    execute();
}

void CleaningJob::done()
{
    m_status = Done;
    emitResult();
}


//
// Crappy Inference Data
//

class CrappyInferenceData : public CleaningJob {
public:
    QString jobName() {
        return i18n("Clear Crappy Interference Data");
    }
private:
    void execute();
};

void CrappyInferenceData::execute()
{
    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();

    model->removeContext(QUrl::fromEncoded("urn:crappyinference:inferredtriples"));
    model->removeContext(QUrl::fromEncoded("urn:crappyinference2:inferredtriples"));

    done();
}

//
// Tags
//

class EmptyTagCleaner : public CleaningJob {
public:
    QString jobName() {
        return i18n("Clear Empty Tags");
    }
private:
    void execute();
};

void EmptyTagCleaner::execute()
{
    QString query = QString::fromLatin1("select ?r where { ?r a %1 . FILTER NOT EXISTS { ?r %2 ?i . } }")
                    .arg( Soprano::Node::resourceToN3( NAO::Tag() ),
                          Soprano::Node::resourceToN3( NAO::identifier() ) );

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QList<QUrl> deleteList;
    while( it.next() )
        deleteList << it[0].uri();

    KJob* job = Nepomuk2::removeResources( deleteList );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(done()) );
}


DuplicateMergingJob::DuplicateMergingJob(const QUrl& type, const QUrl& prop, QObject* parent)
    : CleaningJob(parent)
    , m_type(type)
    , m_prop(prop)
{
}

void DuplicateMergingJob::execute()
{
    m_jobs = 0;

    QString query = QString::fromLatin1("select distinct ?i where { ?r a %1 . ?r %2 ?i . }")
                    .arg( Soprano::Node::resourceToN3( m_type ),
                          Soprano::Node::resourceToN3( m_prop ) );

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        QString query = QString::fromLatin1("select distinct ?r where { ?r a %1 . ?r %2 %3 . }")
                        .arg( Soprano::Node::resourceToN3( m_type ),
                              Soprano::Node::resourceToN3( m_prop ),
                              it[0].toN3() );
        Soprano::QueryResultIterator iter = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        QList<QUrl> resourcesToMerge;
        while( iter.next() )
            resourcesToMerge << iter[0].uri();

        if( resourcesToMerge.size() <= 1 )
            continue;

        kDebug() << resourcesToMerge;
        KJob* job = Nepomuk2::mergeResources( resourcesToMerge );
        connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)) );
        m_jobs++;
    }

    if( !m_jobs )
        done();
}

void DuplicateMergingJob::slotJobFinished(KJob* job)
{
    if( job->error() )
        kError() << job->errorString();

    m_jobs--;
    if( !m_jobs )
        done();
}

class DuplicateTagCleaner : public DuplicateMergingJob {
public:
    explicit DuplicateTagCleaner(QObject* parent = 0)
    : DuplicateMergingJob(NAO::Tag(), NAO::identifier(), parent) {}

    QString jobName() {
        return i18n("Merge Duplicate Tags");
    }
};


class DuplicateFileCleaner : public DuplicateMergingJob {
public:
    explicit DuplicateFileCleaner(QObject* parent = 0)
    : DuplicateMergingJob(NFO::FileDataObject(), NIE::url(), parent) {}

    QString jobName() {
        return i18n("Merge Duplicate File Metadata");
    }
};

//
// Akonadi
//

class AkonadiMigrationJob : public CleaningJob {
public:
    explicit AkonadiMigrationJob(QObject* parent = 0)
    : CleaningJob(parent) {}

    QString jobName() {
        return i18n("Akonadi Migration Job");
    }

private:
    void execute();
};

void AkonadiMigrationJob::execute()
{
    const QUrl akonadiDataObject("http://akonadi-project.org/ontologies/aneo#AkonadiDataObject");

    QLatin1String query("select distinct ?r where { ?r ?p ?o. FILTER(REGEX(STR(?r), '^akonadi')). }");

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        // FIXME: What about the agent?
        Nepomuk2::CreateResourceJob* cjob = Nepomuk2::createResource( QList<QUrl>() << akonadiDataObject, QString(), QString() );
        cjob->exec();
        if( cjob->error() ) {
            kDebug() << cjob->errorString();
            return;
        }

        kDebug() << cjob->resourceUri() << " " << it[0].uri();
        KJob* job = Nepomuk2::mergeResources( cjob->resourceUri(), it[0].uri() );
        job->exec();
        if( job->error() ) {
            kDebug() << job->errorString();
            return;
        }

        job = Nepomuk2::setProperty( QList<QUrl>() << cjob->resourceUri(), NIE::url(), QVariantList() << it[0].uri() );
        job->exec();
        if( job->error() ) {
            kDebug() << job->errorString();
            return;
        }
    }
    done();
}



QList< CleaningJob* > allJobs()
{
    QList<CleaningJob*> list;
    list << new CrappyInferenceData();
    list << new EmptyTagCleaner();
    list << new DuplicateTagCleaner();
    list << new DuplicateFileCleaner();
    list << new AkonadiMigrationJob();
    return list;
}

#include "cleaningjobs.moc"