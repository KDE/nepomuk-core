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
#include <Soprano/Client/LocalSocketClient>
#include <Soprano/Query/QueryLanguage>
#include <Soprano/Util/DummyModel>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>


// FIXME: disconnect localSocketClient after n seconds of idling (but take care of not
//        disconnecting when iterators are open)

using namespace Soprano;

class Nepomuk2::MainModel::Private
{
public:
    Private()
        : localSocketModel( 0 ),
          dummyModel( 0 ),
          m_socketConnectFailed( false ),
          m_initMutex( QMutex::Recursive ) {
    }

    ~Private() {
        delete localSocketModel;
        delete dummyModel;
    }

    Soprano::Client::LocalSocketClient localSocketClient;
    Soprano::Model* localSocketModel;

    Soprano::Util::DummyModel* dummyModel;

    void init( bool forced ) {
        QMutexLocker lock( &m_initMutex );

        if( forced ) {
            m_socketConnectFailed = false;
        }

        // we may get disconnected from the server but we don't want to try
        // to connect every time the model is requested
        if ( forced || (!m_socketConnectFailed && !localSocketClient.isConnected()) ) {
            localSocketClient.disconnect();
            QString socketName = KGlobal::dirs()->locateLocal( "socket", "nepomuk-socket" );

            kDebug() << "Connecting to local socket" << socketName;
            if ( localSocketClient.connect( socketName ) ) {
                m_socketConnectFailed = false;
                kDebug() << "Connected :)";

                // FIXME: This results in a slight memory leak
                // Always recreate the model - We need to do this cause Soprano has this concept
                // of multiple models where each model name is mapped to an id.
                // Normally, when calling init() again, it is fine to not re-create the model
                // since nothing has really changed.
                // However, when the storage service is restarted, we need to recreate the model
                // so as to make the storage service map the name "main" to this id, and
                // inform the client about it. We cannot keep using the old ID.
                // ( The IDs are randomly generated, so there is no chance of reusing the last id )
                //
                //if( !localSocketModel )
                    localSocketModel = localSocketClient.createModel( "main" );
            }
            else {
                m_socketConnectFailed = true;
                kDebug() << "Failed to connect to Nepomuk server via local socket" << socketName;
            }
        }
    }

    Soprano::Model* model() {
        QMutexLocker lock( &m_initMutex );

        init( false );

        // we always prefer the faster local socket client
        if ( localSocketModel ) {
            return localSocketModel;
        }
        else {
            if ( !dummyModel ) {
                dummyModel = new Soprano::Util::DummyModel();
            }
            return dummyModel;
        }
    }

private:
    bool m_socketConnectFailed;
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
    return d->localSocketClient.isConnected();
}


bool Nepomuk2::MainModel::init()
{
    d->init( true );
    return isValid();
}

void Nepomuk2::MainModel::disconnect()
{
    d->localSocketClient.disconnect();
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


Soprano::QueryResultIterator Nepomuk2::MainModel::executeQuery( const QString& query,
                                                               Soprano::Query::QueryLanguage language,
                                                               const QString& userQueryLanguage ) const
{
    Soprano::QueryResultIterator it = d->model()->executeQuery( query, language, userQueryLanguage );
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
