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


#include "changelog.h"
#include "changelogrecord.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtCore/QMutableListIterator>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QDebug>

#include <Soprano/Statement>
#include <Soprano/Model>

// trueg: why not use QSharedData and avoid all the hassle or returning references in SyncFile?
class Nepomuk2::ChangeLog::Private {
public:
    // vHanda: Maybe I should use a QMap<ChangeLogRecord, QHashDummyValue> ?
    // That way the records will be ordered.
    // But then the ordering is only required while syncing ( not merging )
    QList<ChangeLogRecord> m_records;
};

Nepomuk2::ChangeLog::ChangeLog()
    : d( new Nepomuk2::ChangeLog::Private )
{
}

Nepomuk2::ChangeLog::ChangeLog(const Nepomuk2::ChangeLog& rhs)
    : d( new Nepomuk2::ChangeLog::Private )
{
    this->operator=( rhs );
}

Nepomuk2::ChangeLog::~ChangeLog()
{
    delete d;
}

Nepomuk2::ChangeLog& Nepomuk2::ChangeLog::operator=(const Nepomuk2::ChangeLog& rhs)
{
    (*d) = (*rhs.d);
    return *this;
}


int Nepomuk2::ChangeLog::size() const
{
    return d->m_records.size();
}


bool Nepomuk2::ChangeLog::empty() const
{
    return d->m_records.empty();
}


void Nepomuk2::ChangeLog::clear()
{
    d->m_records.clear();
}


void Nepomuk2::ChangeLog::add(const Nepomuk2::ChangeLogRecord& r)
{
    d->m_records.append( r );
}


QList<Nepomuk2::ChangeLogRecord> Nepomuk2::ChangeLog::toList() const
{
    return d->m_records;
}


// trueg: in case you change ChangeLog to useing QSharedData this method should be
//        const and return a sorted version. Then if there is only the one instance
//        of the changelog the performance is the same as no copying takes place
//        but the API would be much cleaner or at least more in sync with the Qt/KDE
//        API style
void Nepomuk2::ChangeLog::sort()
{
    qSort( d->m_records );
}


// trueg: here the same as above applies.
void Nepomuk2::ChangeLog::invert()
{
    QMutableListIterator<ChangeLogRecord> it( d->m_records );
    while( it.hasNext() ) {
        ChangeLogRecord & record = it.next();
        record.setAdded( !record.added() );
    }
}


// trueg: and once more as above - maybe the methods could then be named "sorted()", "inverted()", and "filtered()"
void Nepomuk2::ChangeLog::filter(const QSet< QUrl >& nepomukUris)
{
    QMutableListIterator<ChangeLogRecord> it( d->m_records );
    while( it.hasNext() ) {
        const ChangeLogRecord & r = it.next();
        if( !nepomukUris.contains( r.st().subject().uri() ) ) {
            it.remove();
        }
    }
}


Nepomuk2::ChangeLog& Nepomuk2::ChangeLog::operator +=(const Nepomuk2::ChangeLog& log)
{
    d->m_records += log.d->m_records;
    return *this;
}


Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromUrl(const QUrl& url)
{
    ChangeLog log;
    log.d->m_records = ChangeLogRecord::loadRecords( url );
    return log;
}


Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromUrl(const QUrl& url, const QDateTime& min)
{
    ChangeLog log;
    log.d->m_records = ChangeLogRecord::loadRecords( url, min );
    return log;
}


bool Nepomuk2::ChangeLog::save(const QUrl& url) const
{
    return ChangeLogRecord::saveRecords( d->m_records, url );
}


void Nepomuk2::ChangeLog::removeRecordsAfter(const QDateTime& dt)
{
    QMutableListIterator<ChangeLogRecord> it( d->m_records );
    while( it.hasNext() ) {
        it.next();

        if( it.value().dateTime() >= dt )
            it.remove();
    }
}

void Nepomuk2::ChangeLog::removeRecordsBefore(const QDateTime& dt)
{
    QMutableListIterator<ChangeLogRecord> it( d->m_records );
    while( it.hasNext() ) {
        it.next();

        if( dt <= it.value().dateTime() )
            it.remove();
    }
}

QSet<QUrl> Nepomuk2::ChangeLog::resources() const
{
    QSet<QUrl> uniqueUris;

    foreach(const ChangeLogRecord & r, d->m_records) {
        QUrl sub = r.subject().uri();
        uniqueUris.insert(sub);

        // If the Object is a resource, then it has to be identified as well.
        const Soprano::Node obj = r.object();
        if(obj.isResource()) {
            QUrl uri = obj.uri();
            //if(uri.scheme() == QLatin1String("nepomuk"))
                uniqueUris.insert(uri);
        }
    }

    return uniqueUris;
}

QSet<QUrl> Nepomuk2::ChangeLog::subjects() const
{
    QSet<QUrl> uniqueUris;

    foreach(const ChangeLogRecord & r, d->m_records) {
        QUrl sub = r.subject().uri();
        uniqueUris.insert(sub);
    }

    return uniqueUris;
}

QSet<QUrl> Nepomuk2::ChangeLog::objects() const
{
    QSet<QUrl> uniqueUris;

    foreach(const ChangeLogRecord & r, d->m_records) {
        const Soprano::Node obj = r.object();
        if(obj.isResource()) {
            QUrl uri = obj.uri();
            //if(uri.scheme() == QLatin1String("nepomuk"))
                uniqueUris.insert(uri);
        }
    }

    return uniqueUris;
}

Nepomuk2::ChangeLog& Nepomuk2::ChangeLog::operator+=(const Nepomuk2::ChangeLogRecord& record)
{
    add( record );
    return *this;
}


QTextStream& Nepomuk2::operator<<(QTextStream& ts, const Nepomuk2::ChangeLog& log)
{
    foreach( const ChangeLogRecord & record, log.toList() )
        ts << record << "\n";
    return ts;
}


QString Nepomuk2::ChangeLog::dateTimeFormat()
{
    return ChangeLogRecord::dateTimeFormat();
}

QDebug operator<<( QDebug debug, const Nepomuk2::ChangeLog & log )
{
    foreach( const Nepomuk2::ChangeLogRecord & rec, log.toList() )
        debug << rec.toString();
    return debug;
}


// static
Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromList(const QList< Soprano::Statement >& st)
{
    ChangeLog log;
    log.d->m_records = ChangeLogRecord::toRecordList( st );
    return log;
}

// static
Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromList(const QList< Nepomuk2::ChangeLogRecord >& records)
{
    ChangeLog log;
    log.d->m_records = records;
    return log;
}

// static
Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromGraphUri(const QUrl& graphUri, Soprano::Model* model)
{
    ChangeLog log;
    log.d->m_records = ChangeLogRecord::toRecordList(graphUri, model);
    return log;
}

// static
Nepomuk2::ChangeLog Nepomuk2::ChangeLog::fromGraphUriList(const QList< QUrl >& graphUriList, Soprano::Model* model)
{
    ChangeLog log;
    log.d->m_records = ChangeLogRecord::toRecordList(graphUriList, model);
    return log;
}