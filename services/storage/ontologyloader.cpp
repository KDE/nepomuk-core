/* This file is part of the KDE Project
   Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ontologyloader.h"
#include "ontologymanagermodel.h"
#include "ontologymanageradaptor.h"
#include "graphretriever.h"

#include <Soprano/Global>
#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/PluginManager>
#include <Soprano/StatementIterator>
#include <Soprano/Parser>
#include <Soprano/QueryResultIterator>

#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>
#include <KLocale>
#include <KDirWatch>
#include <kdbusconnectionpool.h>

#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <kpluginfactory.h>
#include <kpluginloader.h>


using namespace Soprano;


class Nepomuk2::OntologyLoader::Private
{
public:
    Private( OntologyLoader* p )
        : forceOntologyUpdate( false ),
          someOntologyUpdated( false ),
          q( p ) {
    }

    OntologyManagerModel* model;

    QTimer updateTimer;
    bool forceOntologyUpdate;
    QStringList desktopFilesToUpdate;

    // true if at least one ontology has been updated
    bool someOntologyUpdated;

    void updateOntology( const QString& filename );

private:
    OntologyLoader* q;
};


void Nepomuk2::OntologyLoader::Private::updateOntology( const QString& filename )
{
    KConfig ontologyDescFile( filename );
    KConfigGroup df( &ontologyDescFile, QLatin1String( "Ontology" ) );

    // only update if the modification date of the ontology file changed (not the desktop file).
    // ------------------------------------
    QFileInfo ontoFileInf( df.readEntry( QLatin1String("Path") ) );

#ifdef Q_OS_WIN
    if ( ! ontoFileInf.exists() ) {
        // On Windows the Path setting is always the install directory which is not
        // something like /usr/share/ontologies but something like
        // r:\kderoot\build\shared-desktop-ontologies-mingw-i686\image\shared\ontologies
        // so if you did not compile shared-desktop-ontologies yourself the Path
        // is useless.
        // We expect a default ontologies installation where the .ontology files are placed
        // in the same directory as the .trig files.
        QString alt_filename = filename;
        QFileInfo ontoAlternative( alt_filename.replace( QLatin1String(".ontology"),
                                                         QLatin1String(".trig") ) );
        ontoFileInf = ontoAlternative;
        kDebug() << "Ontology path: " << filename << " does not exist. Using "
                 << alt_filename << " instead";
    }
#endif
    QString ontoNamespace = df.readEntry( QLatin1String("Namespace") );
    QDateTime ontoLastModified = model->ontoModificationDate( ontoNamespace );
    bool update = false;

    if ( ontoLastModified < ontoFileInf.lastModified() ) {
        kDebug() << "Ontology" << ontoNamespace << "needs updating.";
        update = true;
    }
    //else {
    //    kDebug() << "Ontology" << ontoNamespace << "up to date.";
    //}

    if( !update && forceOntologyUpdate ) {
        kDebug() << "Ontology update forced.";
        update = true;
    }

    if( update ) {
        someOntologyUpdated = true;

        QString mimeType = df.readEntry( "MimeType", QString() );

        const Soprano::Parser* parser
            = Soprano::PluginManager::instance()->discoverParserForSerialization( Soprano::mimeTypeToSerialization( mimeType ),
                                                                                  mimeType );
        if ( !parser ) {
            kDebug() << "No parser to handle" << df.readEntry( QLatin1String( "Name" ) ) << "(" << mimeType << ")";
            return;
        }

        kDebug() << "Parsing" << ontoFileInf.filePath();

        Soprano::StatementIterator it = parser->parseFile( ontoFileInf.filePath(),
                                                           ontoNamespace,
                                                           Soprano::mimeTypeToSerialization( mimeType ),
                                                           mimeType );
        if ( !parser->lastError() ) {
            model->updateOntology( it, ontoNamespace );
            emit q->ontologyUpdated( ontoNamespace );
        }
        else {
            kDebug() << "Parsing of file " << ontoFileInf.filePath() << "failed (" << parser->lastError().message() << ")";
            emit q->ontologyUpdateFailed( ontoNamespace, i18n( "Parsing of file %1 failed (%2)", ontoFileInf.filePath(), parser->lastError().message() ) );
        }
    }
}



Nepomuk2::OntologyLoader::OntologyLoader( Soprano::Model* model, QObject* parent )
    : QObject( parent ),
      d( new Private(this) )
{
    // register ontology resource dir
    KGlobal::dirs()->addResourceType( "xdgdata-ontology", 0, "ontology" );

    // export ourselves on DBus
    ( void )new OntologyManagerAdaptor( this );
    QDBusConnection con = KDBusConnectionPool::threadConnection();
    con.registerObject( QLatin1String("/nepomukontologyloader"), this,
                        QDBusConnection::ExportAdaptors );

    // be backwards compatible
    con.registerService( QLatin1String("org.kde.nepomuk.services.nepomukontologyloader") );

    d->model = new OntologyManagerModel( model, this );
    connect( &d->updateTimer, SIGNAL(timeout()), this, SLOT(updateNextOntology()) );

    // watch both the global and local ontology folder for changes
    KDirWatch* dirWatch = KDirWatch::self();
    connect( dirWatch, SIGNAL( dirty(QString) ),
             this, SLOT( updateLocalOntologies() ) );
    connect( dirWatch, SIGNAL( created(QString) ),
             this, SLOT( updateLocalOntologies() ) );

    foreach( const QString& dir, KGlobal::dirs()->resourceDirs( "xdgdata-ontology" ) ) {
        kDebug() << "watching" << dir;
        dirWatch->addDir( dir, KDirWatch::WatchFiles|KDirWatch::WatchSubDirs );
    }
}


Nepomuk2::OntologyLoader::~OntologyLoader()
{
    delete d;
}


void Nepomuk2::OntologyLoader::updateLocalOntologies()
{
    d->someOntologyUpdated = false;
    d->desktopFilesToUpdate = KGlobal::dirs()->findAllResources( "xdgdata-ontology", "*.ontology", KStandardDirs::Recursive|KStandardDirs::NoDuplicates );
    if(d->desktopFilesToUpdate.isEmpty())
        kError() << "No ontology files found! Make sure the shared-desktop-ontologies project is installed and XDG_DATA_DIRS is set properly.";
    d->updateTimer.start(0);
}


void Nepomuk2::OntologyLoader::updateAllLocalOntologies()
{
    d->forceOntologyUpdate = true;
    updateLocalOntologies();
}


void Nepomuk2::OntologyLoader::updateNextOntology()
{
    if( !d->desktopFilesToUpdate.isEmpty() ) {
        d->updateOntology( d->desktopFilesToUpdate.takeFirst() );
    }
    else {
        d->forceOntologyUpdate = false;
        d->updateTimer.stop();
        emit ontologyUpdateFinished(d->someOntologyUpdated);
    }
}


QString Nepomuk2::OntologyLoader::findOntologyContext( const QString& uri )
{
    return QString::fromAscii( d->model->findOntologyContext( QUrl::fromEncoded( uri.toAscii() ) ).toEncoded() );
}


void Nepomuk2::OntologyLoader::importOntology( const QString& url )
{
    connect( GraphRetriever::retrieve( url ), SIGNAL( result( KJob* ) ),
             this, SLOT( slotGraphRetrieverResult( KJob* ) ) );
}


void Nepomuk2::OntologyLoader::slotGraphRetrieverResult( KJob* job )
{
    GraphRetriever* graphRetriever = static_cast<GraphRetriever*>( job );
    if ( job->error() ) {
        emit ontologyUpdateFailed( QString::fromAscii( graphRetriever->url().toEncoded() ), graphRetriever->errorString() );
    }
    else {
        // TODO: find a way to check if the imported version of the ontology
        // is newer than the already installed one
        if ( d->model->updateOntology( graphRetriever->statements(), QUrl()/*graphRetriever->url()*/ ) ) {
            emit ontologyUpdated( QString::fromAscii( graphRetriever->url().toEncoded() ) );
            emit ontologyUpdateFinished(true);
        }
        else {
            emit ontologyUpdateFailed( QString::fromAscii( graphRetriever->url().toEncoded() ), d->model->lastError().message() );
        }
    }
}

#include "ontologyloader.moc"
