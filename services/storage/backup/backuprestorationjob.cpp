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


#include "backuprestorationjob.h"
#include "backupfile.h"
#include "storage.h"
#include "nie.h"
#include <KDebug>

#include <QtCore/QTimer>
#include <QtCore/QDir>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

BackupRestorationJob::BackupRestorationJob(Storage* storageService, const QUrl& url, QObject* parent)
    : KJob(parent)
    , m_model( storageService->model() )
    , m_storageService( storageService )
    , m_url( url )
{
}

void BackupRestorationJob::start()
{
    QTimer::singleShot( 0, this, SLOT(doWork()) );
}

void BackupRestorationJob::doWork()
{
    kDebug() << "RESTORING!!!";
    connect( m_storageService, SIGNAL(resetRepositoryDone(QString, QString)), this, SLOT(slotRestRepo(QString, QString)) );
    m_storageService->resetRepository();
}

namespace {

    //
    // Removes the old home directory and replaces it with the current one
    // TODO: Make it OS independent
    //
    QUrl translateHomeUri( const QUrl & uri ) {
        QString uriString = uri.toString();

        QRegExp regEx("^file://(/home/[^/]*)(/.*)$");
        if( regEx.exactMatch( uriString ) ) {
            QString newUriString = "file://" + QDir::homePath() + regEx.cap(2);

            uriString.replace( regEx, newUriString );
            return QUrl( newUriString );
        }
        return uri;
    }
}

void BackupRestorationJob::slotRestRepo(const QString&, const QString& newPath)
{
    m_oldRepoPath = newPath;

    BackupFile bf = BackupFile::fromUrl( m_url );
    Soprano::StatementIterator it = bf.iterator();

    kDebug() << "Restore Statements:" << bf.numStatements();
    int numStatements = 0;
    while( it.next() ) {
        Soprano::Statement st = it.current();
        if( st.predicate() == NIE::url() ) {
            QUrl url = st.object().uri();
            if( url.scheme() == QLatin1String("file") ) {
                //
                // Check if the file exists
                //
                if( !QFile::exists( url.toLocalFile() ) ) {
                    url = translateHomeUri( url );
                    if( !QFile::exists( url.toLocalFile() ) ) {
                        url.setScheme("nepomuk-backup");
                    }
                    st.setObject( url );
                }
            }
        }

        Soprano::Error::ErrorCode err = m_model->addStatement( st );
        if( err ) {
            kWarning() << m_model->lastError();
            setErrorText( m_model->lastError().message() );
            emitResult();
            return;
        }

        numStatements++;
        emitPercent( numStatements, bf.numStatements() );
    }

    m_storageService->openPublicInterfaces();
    emitResult();
}


}
