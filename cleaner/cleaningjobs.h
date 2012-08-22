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


#ifndef CLEANINGJOB_H
#define CLEANINGJOB_H

#include <KJob>
#include <KLocalizedString>
#include <QtCore/QUrl>

class CleaningJob : public KJob
{
    Q_OBJECT
public:
    explicit CleaningJob(QObject* parent = 0);
    virtual ~CleaningJob();

    virtual void start();
    virtual QString jobName() = 0;

    enum Status {
        Waiting,
        Started,
        Done
    };

    Status status() const;

protected slots:
    void done();

private slots:
    void slotStartExecution();

private:
    Status m_status;

    virtual void execute() = 0;
};

QList<CleaningJob*> allJobs();

//
// Jobs
//

class DuplicateMergingJob : public CleaningJob {
    Q_OBJECT
public:
    explicit DuplicateMergingJob(const QUrl& type, const QUrl& prop, QObject* parent = 0);

private slots:
    void slotJobFinished(KJob* job);

private:
    void execute();
    int m_jobs;
    QUrl m_type;
    QUrl m_prop;
};


#endif // CLEANINGJOB_H
