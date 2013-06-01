/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2008-2010 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "nepomukmainmodel.h"
#include "resourcemanager.h"

#include <Soprano/Node>
#include <Soprano/Statement>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/Query/QueryLanguage>
#include <Soprano/Util/DummyModel>
#include <Soprano/StorageModel>

#include <Soprano/Backend>
#include <Soprano/PluginManager>

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>

using namespace Soprano;

namespace {
    // FIXME: This is hack taken from the virtuoso Soprano backend. We need a proper way of
    //        communicating the port numbber!
    quint16 getFreePortNumber() {
        #ifdef Q_OS_WIN
        static QMutex portNumberMutex;
        static quint16 p = 1113;
        QMutexLocker l(&portNumberMutex);
        return p++;
        #else
        int p = 1113;
        while ( QFile::exists( QString( "/tmp/virt_%1" ).arg( p ) ) ) {
            ++p;
        }
        return p;
        #endif
    }
}

class Nepomuk2::MainModel::Private
{
public:
    Private()
        : virtuosoModel( 0 ),
          dummyModel( 0 ),
          m_initMutex( QMutex::Recursive ) {
    }

    ~Private() {
        delete virtuosoModel;
        delete dummyModel;
    }

    Soprano::StorageModel* virtuosoModel;
    Soprano::Util::DummyModel* dummyModel;

    void init( bool forced ) {
        QMutexLocker lock( &m_initMutex );

        if( !forced && virtuosoModel )
            return;

        Soprano::PluginManager* pm = Soprano::PluginManager::instance();
        const Soprano::Backend* backend = pm->discoverBackendByName( QLatin1String( "virtuosobackend" ) );

        if ( !backend || !backend->isAvailable() ) {
            kError() << "Could not find virtuoso backend";
        }

        Soprano::BackendSettings settings;

        // FIXME: The port might not always be 1113
        settings << Soprano::BackendSetting( Soprano::BackendOptionHost, "localhost" );
        // The -1 is because if virtuoso exists, it would be on the previous port
        settings << Soprano::BackendSetting( Soprano::BackendOptionPort, getFreePortNumber()-1 );
        settings << Soprano::BackendSetting( Soprano::BackendOptionUsername, "dba" );
        settings << Soprano::BackendSetting( Soprano::BackendOptionPassword, "dba" );
        settings << Soprano::BackendSetting( "noStatementSignals", true );
        settings << Soprano::BackendSetting( "fakeBooleans", false );
        settings << Soprano::BackendSetting( "emptyGraphs", false );

        // FIXME: Can we really delete the model? What about open iterators?
        if( virtuosoModel )
            virtuosoModel->deleteLater();

        virtuosoModel = backend ? backend->createModel( settings ) : 0;
        // Listen to the virtuoso model crashing?
    }

    Soprano::Model* model() {
        QMutexLocker lock( &m_initMutex );

        init( false );

        if ( virtuosoModel ) {
            return virtuosoModel;
        }
        else {
            if ( !dummyModel ) {
                dummyModel = new Soprano::Util::DummyModel();
            }
            return dummyModel;
        }
    }

    QMutex m_initMutex;
};


Nepomuk2::MainModel::MainModel( QObject* parent )
    : Soprano::Model(),
      d( new Private() )
{
    setParent( parent );
}


Nepomuk2::MainModel::~MainModel()
{
    delete d;
}


bool Nepomuk2::MainModel::isValid() const
{
    QMutexLocker lock( &d->m_initMutex );
    return d->virtuosoModel;
}


bool Nepomuk2::MainModel::init()
{
    d->init( true );
    return isValid();
}

void Nepomuk2::MainModel::disconnect()
{
    QMutexLocker lock( &d->m_initMutex );
    d->virtuosoModel->deleteLater();
    d->virtuosoModel = 0;
}


Soprano::StatementIterator Nepomuk2::MainModel::listStatements( const Statement& partial ) const
{
    Soprano::StatementIterator it = d->model()->listStatements( partial );
    setError( d->model()->lastError() );
    return it;
}


Soprano::NodeIterator Nepomuk2::MainModel::listContexts() const
{
    Soprano::NodeIterator it = d->model()->listContexts();
    setError( d->model()->lastError() );
    return it;
}

//
// Copied from services/storage/virtuosoinferencemodel.cpp
//
namespace {
    const char* s_nepomukInferenceRuleSetName = "nepomukinference";
}

Soprano::QueryResultIterator Nepomuk2::MainModel::executeQuery( const QString& query,
                                                               Soprano::Query::QueryLanguage language,
                                                               const QString& userQueryLanguage ) const
{
    Soprano::QueryResultIterator it;
    if(language == Soprano::Query::QueryLanguageSparqlNoInference) {
        it = d->model()->executeQuery(query, Soprano::Query::QueryLanguageSparql);
    }
    else if(language == Soprano::Query::QueryLanguageSparql ) {
        it = d->model()->executeQuery(QString::fromLatin1("DEFINE input:inference <%1> ")
                                         .arg(QLatin1String(s_nepomukInferenceRuleSetName)) + query, language);
    }
    else {
        it = d->model()->executeQuery(query, language, userQueryLanguage);
    }
    setError( d->model()->lastError() );
    return it;
}


bool Nepomuk2::MainModel::containsStatement( const Statement& statement ) const
{
    bool b = d->model()->containsStatement( statement );
    setError( d->model()->lastError() );
    return b;
}


bool Nepomuk2::MainModel::containsAnyStatement( const Statement &statement ) const
{
    bool b = d->model()->containsAnyStatement( statement );
    setError( d->model()->lastError() );
    return b;
}


bool Nepomuk2::MainModel::isEmpty() const
{
    bool b = d->model()->isEmpty();
    setError( d->model()->lastError() );
    return b;
}


int Nepomuk2::MainModel::statementCount() const
{
    int c = d->model()->statementCount();
    setError( d->model()->lastError() );
    return c;
}


Soprano::Error::ErrorCode Nepomuk2::MainModel::addStatement( const Statement& statement )
{
    Soprano::Error::ErrorCode c = d->model()->addStatement( statement );
    setError( d->model()->lastError() );
    return c;
}


Soprano::Error::ErrorCode Nepomuk2::MainModel::removeStatement( const Statement& statement )
{
    Soprano::Error::ErrorCode c = d->model()->removeStatement( statement );
    setError( d->model()->lastError() );
    return c;
}


Soprano::Error::ErrorCode Nepomuk2::MainModel::removeAllStatements( const Statement& statement )
{
    Soprano::Error::ErrorCode c = d->model()->removeAllStatements( statement );
    setError( d->model()->lastError() );
    return c;
}


Soprano::Node Nepomuk2::MainModel::createBlankNode()
{
    Soprano::Node n = d->model()->createBlankNode();
    setError( d->model()->lastError() );
    return n;
}

#include "nepomukmainmodel.moc"
