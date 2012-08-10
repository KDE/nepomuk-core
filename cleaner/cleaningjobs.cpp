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
#include "resourcemanager.h"
#include "datamanagement.h"

#include <QtCore/QTimer>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>

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


void DuplicateTagCleaner::execute()
{
    m_jobs = 0;

    QString query = QString::fromLatin1("select distinct ?i where { ?r a %1 . ?r %2 ?i . }")
                    .arg( Soprano::Node::resourceToN3( NAO::Tag() ),
                          Soprano::Node::resourceToN3( NAO::identifier() ) );

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        QString query = QString::fromLatin1("select distinct ?r where { ?r a %1 . ?r %2 %3 . }")
                        .arg( Soprano::Node::resourceToN3( NAO::Tag() ),
                              Soprano::Node::resourceToN3( NAO::identifier() ),
                              Soprano::Node::literalToN3( it[0].literal() ) );
        Soprano::QueryResultIterator iter = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        QList<QUrl> tagsToMerge;
        while( iter.next() )
            tagsToMerge << iter[0].uri();

        if( tagsToMerge.size() <= 1 )
            continue;

        KJob* job = Nepomuk2::mergeResources( tagsToMerge );
        connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)) );
        m_jobs++;
    }

    if( !m_jobs )
        done();
}

void DuplicateTagCleaner::slotJobFinished(KJob*)
{
    m_jobs--;
    if( !m_jobs )
        done();
}


QList< CleaningJob* > allJobs()
{
    QList<CleaningJob*> list;
    list << new CrappyInferenceData();
    list << new EmptyTagCleaner();
    list << new DuplicateTagCleaner();
    return list;
}

#include "cleaningjobs.moc"