/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "changelogrecord.h"

#include <QtCore/QRegExp>
#include <QtCore/QFile>

#include <Soprano/Node>
#include <Soprano/PluginManager>
#include <Soprano/Parser>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>

#include <KDebug>

#include <QtCore/QSharedData>

class Nepomuk2::ChangeLogRecord::Private : public QSharedData {
public:
    QDateTime dateTime;
    bool added;
    Soprano::Statement st;

    const static Soprano::Parser * parser;
    const static Soprano::Serializer * serializer;

    const static QString s_dateTimeFormat;
};

const QString Nepomuk2::ChangeLogRecord::Private::s_dateTimeFormat = QString::fromLatin1("yyyy-MM-ddThh:mm:ss.zzz");

//
// Create the serializer and parser
//
const Soprano::Parser* Nepomuk2::ChangeLogRecord::Private::parser = Soprano::PluginManager::instance()->discoverParserForSerialization( Soprano::SerializationNQuads );

const Soprano::Serializer* Nepomuk2::ChangeLogRecord::Private::serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization( Soprano::SerializationNQuads );


Nepomuk2::ChangeLogRecord::ChangeLogRecord()
    : d( new Nepomuk2::ChangeLogRecord::Private )
{
    //Nothing to do
}

Nepomuk2::ChangeLogRecord::~ChangeLogRecord()
{
}


Nepomuk2::ChangeLogRecord::ChangeLogRecord(const QDateTime& dt, bool add, const Soprano::Statement& statement)
    : d( new Nepomuk2::ChangeLogRecord::Private )
{
    d->dateTime = dt;
    d->added = add;
    d->st = statement;
}


Nepomuk2::ChangeLogRecord::ChangeLogRecord(const Soprano::Statement& statement)
    : d( new Nepomuk2::ChangeLogRecord::Private )
{
    d->dateTime = QDateTime::currentDateTime();
    d->added = true;
    d->st = statement;
}


Nepomuk2::ChangeLogRecord::ChangeLogRecord(const Nepomuk2::ChangeLogRecord& rhs)
    : d( rhs.d )
{
}


Nepomuk2::ChangeLogRecord::ChangeLogRecord( QString& string )
    : d( new Nepomuk2::ChangeLogRecord::Private )
{
    QTextStream ts( &string );

    QString dt, ad;
    ts >> dt >> ad ;

    d->dateTime = QDateTime::fromString(dt, Private::s_dateTimeFormat);
    d->added = (ad == "+") ? true : false;

    QList<Soprano::Statement> statements = Private::parser->parseStream( ts, QUrl(), Soprano::SerializationNQuads ).allElements();

    Q_ASSERT( statements.size() == 1 );
    d->st = statements.first();
}


Nepomuk2::ChangeLogRecord& Nepomuk2::ChangeLogRecord::operator=(const Nepomuk2::ChangeLogRecord& rhs)
{
    this->d = rhs.d;
    return *this;
}


QString Nepomuk2::ChangeLogRecord::toString() const
{
    QString s;
    s += d->dateTime.toString( Private::s_dateTimeFormat ) + ' ';
    s += d->added ? '+' : '-';
    s += " ";

    Soprano::Util::SimpleStatementIterator it( QList<Soprano::Statement>() << d->st );

    QString statement;
    QTextStream ts( &statement );

    if( !Private::serializer->serialize( it, ts, Soprano::SerializationNQuads) ) {
        kWarning() << "Serialization using NQuads failed for " << d->st;
        return QString();
    }

    return s + statement;
}


bool Nepomuk2::ChangeLogRecord::operator < (const Nepomuk2::ChangeLogRecord & rhs) const
{
    return d->dateTime < rhs.d->dateTime;
}

bool Nepomuk2::ChangeLogRecord::operator > (const Nepomuk2::ChangeLogRecord & rhs) const
{
    return d->dateTime > rhs.d->dateTime;
}

bool Nepomuk2::ChangeLogRecord::operator==(const Nepomuk2::ChangeLogRecord& rhs) const
{
    return d->dateTime == rhs.d->dateTime && d->st == rhs.d->st && d->added == rhs.d->added;
}


//static
QList<Nepomuk2::ChangeLogRecord> Nepomuk2::ChangeLogRecord::toRecordList(const QList<Soprano::Statement>& stList)
{
    QList<ChangeLogRecord> list;
    foreach( const Soprano::Statement & st, stList ) {
        list << ChangeLogRecord( st );
    }
    return list;
}


//static
QList< Nepomuk2::ChangeLogRecord> Nepomuk2::ChangeLogRecord::toRecordList( const QUrl& contextUrl, Soprano::Model* model )
{
    QList<ChangeLogRecord> list;
    Soprano::StatementIterator it = model->listStatementsInContext(contextUrl);
    while( it.next() ) {
        list << ChangeLogRecord( QDateTime::currentDateTime(), true, it.current());
    }
    return list;
}


//static
QList< Nepomuk2::ChangeLogRecord> Nepomuk2::ChangeLogRecord::toRecordList( const QList< QUrl >& contextUrlList, Soprano::Model* model )
{
    QList<ChangeLogRecord> list;
    foreach( const QUrl & contextUrl, contextUrlList) {
        Soprano::StatementIterator it = model->listStatementsInContext(contextUrl);
        while( it.next() ) {
            list << ChangeLogRecord( QDateTime::currentDateTime(), true, it.current());
        }
    }
    return list;
}

// static
bool Nepomuk2::ChangeLogRecord::saveRecords(const QList<Nepomuk2::ChangeLogRecord>& records, const QUrl & url)
{
    if( records.empty() )
        return false;
    
    QFile file( url.toLocalFile() );
    if( !file.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        kWarning() << "File couldn't be opened for saving : " << url;
        return false;
    }
    
    QTextStream out( &file );
    
    foreach( const ChangeLogRecord& r, records ) {
        out << r.toString();
    }
    return true;
}

// static
QList< Nepomuk2::ChangeLogRecord > Nepomuk2::ChangeLogRecord::loadRecords(const QUrl& url, const QDateTime & min)
{
    QFile file( url.path() );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text) ) {
        kWarning() << "File could not be opened : " << url.path();
        return QList<ChangeLogRecord>();
    }
    
    QTextStream in( &file );

    QList<ChangeLogRecord> records;
    while( !in.atEnd() )
    {
        QString line = in.readLine();
        ChangeLogRecord r( line );
        
        if( min <= r.dateTime() ) {
            records.push_back( r );
        }
    }
    return records;
}


// static
QList< Nepomuk2::ChangeLogRecord > Nepomuk2::ChangeLogRecord::loadRecords(const QUrl& url)
{
    QDateTime min;
    min.setTime_t( 0 );
    
    return loadRecords( url, min );
}

bool Nepomuk2::ChangeLogRecord::added() const
{
    return d->added;
}

QDateTime Nepomuk2::ChangeLogRecord::dateTime() const
{
    return d->dateTime;
}

void Nepomuk2::ChangeLogRecord::setAdded(bool add)
{
    d->added = add;
}

void Nepomuk2::ChangeLogRecord::setRemoved()
{
    d->added = false;
}

void Nepomuk2::ChangeLogRecord::setDateTime(const QDateTime& dt)
{
    d->dateTime = dt;
}

const Soprano::Statement& Nepomuk2::ChangeLogRecord::st() const
{
    return d->st;
}

void Nepomuk2::ChangeLogRecord::setSubject(const Soprano::Node& subject)
{
    d->st.setSubject( subject );
}


void Nepomuk2::ChangeLogRecord::setPredicate(const Soprano::Node& predicate)
{
    d->st.setPredicate( predicate );
}

void Nepomuk2::ChangeLogRecord::setObject(const Soprano::Node& object)
{
    d->st.setObject( object );
}


void Nepomuk2::ChangeLogRecord::setContext(const Soprano::Node& context)
{
    d->st.setContext( context );
}

Soprano::Node Nepomuk2::ChangeLogRecord::subject() const
{
    return d->st.subject();
}

Soprano::Node Nepomuk2::ChangeLogRecord::predicate() const
{
    return d->st.predicate();
}

Soprano::Node Nepomuk2::ChangeLogRecord::object() const
{
    return d->st.object();
}

QTextStream& Nepomuk2::operator<<(QTextStream& ts, const Nepomuk2::ChangeLogRecord& record)
{
    return ts << record.toString();
}

QString Nepomuk2::ChangeLogRecord::dateTimeFormat()
{
    return Private::s_dateTimeFormat;
}
