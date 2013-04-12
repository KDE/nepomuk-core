/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
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


#include "extractorplugin.h"
#include "nco.h"

#include <KDebug>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

ExtractorPlugin::ExtractorPlugin(QObject* parent): QObject(parent)
{
}

ExtractorPlugin::~ExtractorPlugin()
{
}

ExtractorPlugin::ExtractingCritera ExtractorPlugin::criteria()
{
    return BasicMimeType;
}

QStringList ExtractorPlugin::mimetypes()
{
    return QStringList();
}

bool ExtractorPlugin::shouldExtract(const QUrl& url, const QString& type)
{
    Q_UNUSED( url );
    return mimetypes().contains( type );
}

//
// Helper functions
//

QDateTime ExtractorPlugin::dateTimeFromString(const QString& dateString)
{
    QDateTime dateTime;

    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy-MM-dd")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("dd-MM-yyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy-MM")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("MM-yyyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy.MM.dd")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("dd.MM.yyyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("dd MMMM yyyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("MM.yyyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy.MM")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy")); dateTime.setTimeSpec(Qt::UTC); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yy")); dateTime.setTimeSpec(Qt::UTC);  }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, Qt::ISODate); }
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("dddd d MMM yyyy h':'mm':'ss AP"));dateTime.setTimeSpec(Qt::LocalTime);}
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, QLatin1String("yyyy:MM:dd hh:mm:ss"));dateTime.setTimeSpec(Qt::LocalTime);}
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, Qt::SystemLocaleDate); dateTime.setTimeSpec(Qt::UTC);}
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, Qt::SystemLocaleShortDate); dateTime.setTimeSpec(Qt::UTC);}
    if(!dateTime.isValid()) { dateTime = QDateTime::fromString(dateString, Qt::SystemLocaleLongDate); dateTime.setTimeSpec(Qt::UTC);}
    if(!dateTime.isValid()) {
        kWarning() << "Could not determine correct datetime format from:" << dateString;
        return QDateTime();
    }

    return dateTime;
}

QList<SimpleResource> ExtractorPlugin::contactsFromString(const QString& string)
{
    QString cleanedString = string;
    cleanedString = cleanedString.remove( '{' );
    cleanedString = cleanedString.remove( '}' );

    QStringList contactStrings = string.split(',', QString::SkipEmptyParts);
    if( contactStrings.size() == 1 )
        contactStrings = string.split(';', QString::SkipEmptyParts);

    if( contactStrings.size() == 1 )
        contactStrings = string.split(" ft ", QString::SkipEmptyParts);

    if( contactStrings.size() == 1 )
        contactStrings = string.split(" feat. ", QString::SkipEmptyParts);

    if( contactStrings.size() == 1 )
        contactStrings = string.split(" feat ", QString::SkipEmptyParts);

    QList<SimpleResource> resList;
    foreach(const QString& contactName, contactStrings) {
        SimpleResource res;
        res.addType( NCO::Contact() );
        res.addProperty( NCO::fullname(), contactName.trimmed() );

        resList << res;
    }

    return resList;
}

}
