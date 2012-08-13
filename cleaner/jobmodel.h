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


#ifndef JOBMODEL_H
#define JOBMODEL_H

#include "cleaningjobs.h"
#include <QtCore/QAbstractItemModel>


class JobModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit JobModel(QObject* parent = 0);
    virtual ~JobModel();

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;

    void start();
private slots:
    void slotJobFinished(KJob* job);

private:
    QList<CleaningJob*> m_jobs;
};

#endif // JOBMODEL_H
