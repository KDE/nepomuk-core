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
#include "nie.h"

#include <QtCore/QTimer>
#include <QtCore/QDir>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

BackupRestorationJob::BackupRestorationJob(Soprano::Model* model, Nepomuk2::OntologyLoader* loader,
                                           const QUrl& url, QObject* parent)
    : KJob(parent)
    , m_model( model )
    , m_ontologyLoader( loader )
    , m_url( url )
{
}

void BackupRestorationJob::start()
{
    QTimer::singleShot( 0, this, SLOT(doWork()) );
}

void BackupRestorationJob::doWork()
{
    // Discard all existing data
    m_model->removeAllStatements();

    // Re-insert the ontologies
    m_ontologyLoader->updateAllLocalOntologies();
    connect( m_ontologyLoader, SIGNAL(ontologyUpdateFinished(bool)), this, SLOT(slotOntologyUpdateFinished(bool)));
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

void BackupRestorationJob::slotOntologyUpdateFinished(bool)
{
    BackupFile bf = BackupFile::fromUrl( m_url );
    Soprano::StatementIterator it = bf.iterator();

    // TODO: Optimize this
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

        m_model->addStatement( st );

        numStatements++;
        emitPercent( numStatements, bf.numStatements() );
    }

    emitResult();
}


}
