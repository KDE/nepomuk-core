
#include <nepomuk2/variant.h>
#include <nepomuk2/resourcemanager.h>
#include "NEPOMUK_RESOURCENAMELOWER.h"

NEPOMUK_INCLUDES

#include <QtCore/QDateTime>
#include <QtCore/QDate>
#include <QtCore/QTime>


Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME()
  : NEPOMUK_PARENTRESOURCE( QUrl(), QUrl::fromEncoded("NEPOMUK_RESOURCETYPEURI") )
{
}



Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const NEPOMUK_RESOURCENAME& res )
  : NEPOMUK_PARENTRESOURCE( res )
{
}


Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const Nepomuk2::Resource& res )
  : NEPOMUK_PARENTRESOURCE( res )
{
}


Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QString& uri )
  : NEPOMUK_PARENTRESOURCE( uri, QUrl::fromEncoded("NEPOMUK_RESOURCETYPEURI") )
{
}

Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QUrl& uri )
  : NEPOMUK_PARENTRESOURCE( uri, QUrl::fromEncoded("NEPOMUK_RESOURCETYPEURI") )
{
}

Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QString& uri, const QUrl& type )
  : NEPOMUK_PARENTRESOURCE( uri, type )
{
}


Nepomuk2::NEPOMUK_RESOURCENAME::NEPOMUK_RESOURCENAME( const QUrl& uri, const QUrl& type )
  : NEPOMUK_PARENTRESOURCE( uri, type )
{
}

Nepomuk2::NEPOMUK_RESOURCENAME::~NEPOMUK_RESOURCENAME()
{
}

Nepomuk2::NEPOMUK_RESOURCENAME& Nepomuk2::NEPOMUK_RESOURCENAME::operator=( const NEPOMUK_RESOURCENAME& res )
{
    Resource::operator=( res );
    return *this;
}

QString Nepomuk2::NEPOMUK_RESOURCENAME::resourceTypeUri()
{
    return QLatin1String("NEPOMUK_RESOURCETYPEURI");
}

NEPOMUK_METHODS
