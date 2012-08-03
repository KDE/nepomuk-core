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
#include <resource.h>
#include <resourcemanager.h>
#include <KLocalizedString>
#include <Soprano/Model>
#include <QTimer>

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

QList< CleaningJob* > allJobs()
{
    QList<CleaningJob*> list;
    list << new CrappyInferenceData;
    return list;
}

//
// Crappy Inference Data
//

QString CrappyInferenceData::jobName()
{
    return i18n("Clear Crappy Interference Data");
}

void CrappyInferenceData::execute()
{
    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();

    model->removeContext(QUrl::fromEncoded("urn:crappyinference:inferredtriples"));
    model->removeContext(QUrl::fromEncoded("urn:crappyinference2:inferredtriples"));

    done();
}
