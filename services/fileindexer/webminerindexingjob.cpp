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

#include "webminerindexingjob.h"
#include "util.h"
#include "fileindexerconfig.h"

#include <KUrl>
#include <KDebug>
#include <KProcess>
#include <KStandardDirs>

#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

namespace Nepomuk2 {

WebMinerIndexingJob::WebMinerIndexingJob(const QFileInfo& info, QObject* parent)
    : KJob(parent)
    , m_url( info.absoluteFilePath() )
{
    // setup the timer used to kill the indexer process if it seems to get stuck
    m_processTimer = new QTimer(this);
    m_processTimer->setSingleShot(true);
    connect(m_processTimer, SIGNAL(timeout()),
            this, SLOT(slotProcessTimerTimeout()));
}

void WebMinerIndexingJob::start()
{
    // setup the external process which does the actual indexing
    const QString exe = KStandardDirs::findExe(QLatin1String("nepomuk-webminer"));

    kDebug() << "Running" << exe << m_url.toLocalFile();

    m_process = new KProcess( this );

    QStringList args;
    args << "-a" << "-f";
    args << m_url.toLocalFile();

    m_process->setProgram( exe, args );
    m_process->setOutputChannelMode(KProcess::OnlyStdoutChannel);
    connect( m_process, SIGNAL(finished(int)), this, SLOT(slotIndexedFile(int)) );
    m_process->start();

    // start the timer which will kill the process if it does not terminate after 5 minutes
    m_processTimer->start(5*60*1000);
}


void WebMinerIndexingJob::slotIndexedFile(int exitCode)
{
    // stop the timer since there is no need to kill the process anymore
    m_processTimer->stop();

    //kDebug() << "Indexing of " << m_url.toLocalFile() << "finished with exit code" << exitCode;
    if(exitCode == 1 && FileIndexerConfig::self()->isDebugModeEnabled()) {
        QFile errorLogFile(KStandardDirs::locateLocal("data", QLatin1String("nepomuk/filer-indexer-error-log"), true));
        if(errorLogFile.open(QIODevice::Append)) {
            QTextStream s(&errorLogFile);
            s << m_url.toLocalFile() << ": " << QString::fromLocal8Bit(m_process->readAllStandardOutput()) << endl;
        }
    }
    emitResult();
}

void WebMinerIndexingJob::slotProcessTimerTimeout()
{
    kDebug() << "Killing the indexer process which seems stuck for" << m_url;
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished();
    emitResult();
}

}
