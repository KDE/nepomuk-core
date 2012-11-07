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

JobModel::JobModel(QObject* parent): QAbstractListModel(parent)
{
    m_allJobs = allJobs();
    m_curJob = 0;

    m_jobThread = new QThread( this );
    m_jobThread->start();
}

JobModel::~JobModel()
{
    if( m_curJob ) {
        m_curJob->kill();
        delete m_curJob;
    }

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
    startNextJob();
}

void JobModel::startNextJob()
{
    if( m_allJobs.isEmpty() )
        return;

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
