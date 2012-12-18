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
#include <QThread>

#include "resourcemanager.h"

JobModel::JobModel(QObject* parent): QAbstractListModel(parent)
{
    // Forcibly initializing the ResourceManager so that it gets initialized
    // in the main thread and not in the job thread
    Nepomuk2::ResourceManager::instance()->init();

    m_allJobs = allJobs();
    m_curJob = 0;
    m_status = NotStarted;

    m_jobThread = new QThread( this );
    m_jobThread->start();
}

JobModel::~JobModel()
{
    if( m_curJob )
        m_curJob->quit();

    m_jobThread->quit();
    m_jobThread->wait();
}

QVariant JobModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() || index.row() >= m_jobs.size() )
        return QVariant();

    switch(role) {
        case Qt::DisplayRole:
            return m_jobs[ index.row() ];

        //FIXME: Maybe one should an animated icon for the item that is being processed
        case Qt::DecorationRole:
            return KIcon("nepomuk");
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
    m_status = Running;
    startNextJob();
}

void JobModel::pause()
{
    m_status = Paused;
    if( m_curJob ) {
        //FIXME: This will leave this job half done
        m_curJob->quit();
        m_curJob = 0;
    }
}

void JobModel::resume()
{
    m_status = Running;
    startNextJob();
}

JobModel::Status JobModel::status()
{
    return m_status;
}


void JobModel::startNextJob()
{
    if( m_status == Paused )
        return;

    if( m_allJobs.isEmpty() || !Nepomuk2::ResourceManager::instance()->initialized() ) {
        m_status = Finished;
        m_curJob = 0;
        emit finished();
        return;
    }

    m_curJob = m_allJobs.takeFirst();
    m_curJob->moveToThread( m_jobThread );

    connect(m_curJob, SIGNAL(finished(KJob*)), this, SLOT(slotJobFinished(KJob*)));
    m_curJob->start();

    emit beginInsertRows( QModelIndex(), m_jobs.size(), m_jobs.size() + 1 );
    m_jobs << m_curJob->jobName();
    emit endInsertRows();
}

void JobModel::slotJobFinished(KJob*)
{
    m_curJob = 0;
    startNextJob();
}
