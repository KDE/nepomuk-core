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


#include "jobmodel.h"
#include "cleaningjobs.h"
#include <KIcon>
#include <KDebug>

JobModel::JobModel(QObject* parent): QAbstractListModel(parent)
{
    m_jobs = allJobs();
}

JobModel::~JobModel()
{
    qDeleteAll( m_jobs );
}

QVariant JobModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() || index.row() >= m_jobs.size() )
        return QVariant();

    CleaningJob* job = m_jobs[index.row()];
    switch(role) {
        case Qt::DisplayRole:
            return job->jobName();
            break;
        case Qt::DecorationRole: {
            switch(job->status()) {
                case CleaningJob::Waiting:
                    return KIcon();
                case CleaningJob::Started:
                    return KIcon("nepomuk");
                case CleaningJob::Done:
                    return KIcon("nepomuk");
            }
        }
    }

    return QVariant();
}

int JobModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;

    return m_jobs.size();
}

void JobModel::start()
{
    if( m_jobs.isEmpty() )
        return;

    CleaningJob* job = m_jobs.first();
    connect(job, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)));
    job->start();

    emit dataChanged(createIndex(0, 0), createIndex(0, 0));
}

void JobModel::slotJobFinished(KJob* job)
{
    CleaningJob* cjob = dynamic_cast<CleaningJob*>(job);
    kDebug() << "Done with " << cjob->jobName();

    int index = m_jobs.indexOf( cjob );
    if( index == m_jobs.size() - 1 )
        return;

    CleaningJob* nextJob = m_jobs[ index + 1 ];
    connect(nextJob, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)));
    nextJob->start();

    emit dataChanged(createIndex(index, 0), createIndex(index+1, 0));
}
