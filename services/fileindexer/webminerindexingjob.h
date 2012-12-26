/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Jörg Ehrichs <joerg.ehrichs@gmx.de>

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

#ifndef WEBMINERINDEXINGJOB_H
#define WEBMINERINDEXINGJOB_H

#include <KJob>
#include <KUrl>

class KProcess;
class QFileInfo;
class QTimer;

namespace Nepomuk2 {

    //class Resource;

    class WebMinerIndexingJob : public KJob
    {
        Q_OBJECT
    public:
        explicit WebMinerIndexingJob(const QFileInfo& info, QObject *parent = 0);

        KUrl url() const { return m_url; }

        virtual void start();

    private slots:
        void slotIndexedFile(int exitCode);
        void slotProcessTimerTimeout();

    private:
        KUrl m_url;
        KProcess* m_process;
        int m_exitCode;
        QTimer* m_processTimer;

    };
}

#endif // WEBMINERINDEXINGJOB_H
