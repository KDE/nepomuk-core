/*
 * This file is part of nepomuk-testlib
 *
 * Copyright (C) 2010 Vishesh Handa <handa.vish@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "testbase.h"
#include "nepomukservicemanagerinterface.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <KStandardDirs>
#include <KTempDir>
#include <KDebug>

#include <Nepomuk/ResourceManager>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/NodeIterator>


class Nepomuk::TestBase::Private {
public:
    org::kde::nepomuk::ServiceManager* m_serviceManager;
    QString m_repoLocation;
    KTempDir m_tempDir;
};

Nepomuk::TestBase::TestBase(QObject* parent)
    : QObject( parent ),
      d( new Nepomuk::TestBase::Private )
{
    d->m_serviceManager = new org::kde::nepomuk::ServiceManager( "org.kde.NepomukServer", "/servicemanager", QDBusConnection::sessionBus() );

    d->m_repoLocation = KStandardDirs::locateLocal( "data", "nepomuk/repository/" );

    // Stop all the other servies
    QSet<QString> services = runningServices().toSet();
    kDebug() << "Running Services : " << services;
    services.remove( "nepomukstorage" );
    services.remove( "nepomukontologyloader" );
    services.remove( "nepomukqueryservice" );

    Q_FOREACH( const QString & service, services )
        stopService( service );
}

Nepomuk::TestBase::~TestBase()
{
    delete d;
}



void Nepomuk::TestBase::initTestCase()
{
}

void Nepomuk::TestBase::cleanupTestCase()
{
}

void Nepomuk::TestBase::resetRepository()
{
    kDebug() << "Reseting the repository ..";
    QString query = "select ?r where { graph ?g { ?r ?p ?o. } . ?g a nrl:InstanceBase . }";
    Soprano::Model * model = Nepomuk::ResourceManager::instance()->mainModel();
    
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QList<Soprano::Node> resources = it.iterateBindings( 0 ).allNodes();

    foreach( const Soprano::Node & node, resources ) {
        model->removeAllStatements( node, Soprano::Node(), Soprano::Node() );
    }
}


void Nepomuk::TestBase::waitForServiceInitialization(const QString& service)
{
    while( !isServiceRunning( service ) ) {
        QTest::qSleep( 100 );
    }
    
    while( !isServiceInitialized( service ) ) {
        QTest::qSleep( 200 );
        kDebug() << runningServices();
    }
}


void Nepomuk::TestBase::startServiceAndWait(const QString& service)
{
    if( isServiceRunning( service ) )
        return;

    kDebug() << "Starting " << service << " ...";
    startService( service );
    kDebug() << "Waiting ...";
    waitForServiceInitialization( service );
}



//
// Service Manager
//

QStringList Nepomuk::TestBase::availableServices()
{
    QDBusPendingReply< QStringList > reply = d->m_serviceManager->availableServices();
    reply.waitForFinished();
    return reply.value();
}

bool Nepomuk::TestBase::isServiceAutostarted(const QString& service)
{
    QDBusPendingReply< bool > reply = d->m_serviceManager->isServiceAutostarted( service );
    reply.waitForFinished();
    return reply.value();
    
}

bool Nepomuk::TestBase::isServiceInitialized(const QString& name)
{
    QDBusPendingReply< bool > reply = d->m_serviceManager->isServiceInitialized( name );
    reply.waitForFinished();
    return reply.value();
}

bool Nepomuk::TestBase::isServiceRunning(const QString& name)
{
    QDBusPendingReply< bool > reply = d->m_serviceManager->isServiceRunning( name );
    reply.waitForFinished();
    return reply.value();
}

QStringList Nepomuk::TestBase::runningServices()
{
    QDBusPendingReply< QStringList > reply = d->m_serviceManager->runningServices();
    reply.waitForFinished();
    return reply.value();
}

void Nepomuk::TestBase::setServiceAutostarted(const QString& service, bool autostart)
{
    d->m_serviceManager->setServiceAutostarted( service, autostart );
}

bool Nepomuk::TestBase::startService(const QString& name)
{
    QDBusPendingReply< bool > reply = d->m_serviceManager->startService( name );
    reply.waitForFinished();
    return reply.value();
}

bool Nepomuk::TestBase::stopService(const QString& name)
{
    QDBusPendingReply< bool > reply = d->m_serviceManager->stopService( name );
    reply.waitForFinished();
    return reply.value();
}

#include "testbase.moc"