/* 
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _NEPOMUK_VARIANT_H_
#define _NEPOMUK_VARIANT_H_

#include "nepomuk_export.h"
#include "resource.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

namespace Nepomuk {

    class Resource;

    /**
     * The KMetaData Variant extends over QVariant by introducing
     * direct support for Resource embedding, automatic list conversion 
     * and a restricted set of supported types.
     *
     * Important differences are:
     * \li No new types can be added other than the ones that have defined
     *     constructors and get-methods
     * \li Variant supports automatic list generation. For example a Variant
     *     containing an int also can produce an int-list via the toIntList
     *     method.
     * \li toString and toStringList always return a valid list and do automatic
     *     conversion from the actual type used in the Variant. Thus, if one only
     *     needs to display the value in a Variant toString and toStringList 
     *     do the job.
     */
    class NEPOMUK_EXPORT Variant
    {
    public:
        Variant();
        ~Variant();
        Variant( const Variant& other );

        /**
         * Will create an invalid Variant if other has an unsupported type.
         */
        explicit Variant( const QVariant& other );
        Variant( int i );
        Variant( qlonglong i );
        Variant( uint i );
        Variant( qulonglong i );
        Variant( bool b );
        Variant( double d );
        Variant( const char* string );
        Variant( const QString& string );
        Variant( const QDate& date );
        Variant( const QTime& time );
        Variant( const QDateTime& datetime );
        Variant( const QUrl& url );
        Variant( const Resource& r );
        Variant( const QList<int>& i );
        Variant( const QList<qlonglong>& i );
        Variant( const QList<uint>& i );
        Variant( const QList<qulonglong>& i );
        Variant( const QList<bool>& b );
        Variant( const QList<double>& d );
        Variant( const QStringList& stringlist );
        Variant( const QList<QDate>& date );
        Variant( const QList<QTime>& time );
        Variant( const QList<QDateTime>& datetime );
        Variant( const QList<QUrl>& url );
        Variant( const QList<Resource>& r );

        Variant& operator=( const Variant& );
        Variant& operator=( int i );
        Variant& operator=( qlonglong i );
        Variant& operator=( uint i );
        Variant& operator=( qulonglong i );
        Variant& operator=( bool b );
        Variant& operator=( double d );
        Variant& operator=( const QString& string );
        Variant& operator=( const QDate& date );
        Variant& operator=( const QTime& time );
        Variant& operator=( const QDateTime& datetime );
        Variant& operator=( const QUrl& url );
        Variant& operator=( const Resource& r );
        Variant& operator=( const QList<int>& i );
        Variant& operator=( const QList<qlonglong>& i );
        Variant& operator=( const QList<uint>& i );
        Variant& operator=( const QList<qulonglong>& i );
        Variant& operator=( const QList<bool>& b );
        Variant& operator=( const QList<double>& d );
        Variant& operator=( const QStringList& stringlist );
        Variant& operator=( const QList<QDate>& date );
        Variant& operator=( const QList<QTime>& time );
        Variant& operator=( const QList<QDateTime>& datetime );
        Variant& operator=( const QList<QUrl>& url );
        Variant& operator=( const QList<Resource>& r );

        /**
         * Append \a i to this variant. If the variant already
         * contains an int it will be converted to a list of int.
         */
        void append( int i );
        void append( qlonglong i );
        void append( uint i );
        void append( qulonglong i );
        void append( bool b );
        void append( double d );
        void append( const QString& string );
        void append( const QDate& date );
        void append( const QTime& time );
        void append( const QDateTime& datetime );
        void append( const QUrl& url );
        void append( const Resource& r );

        /**
         * Appends the value stored in \a v to the list in this
         * Variant. If this Variant contains a value with the same
         * simple type as \a v they are merged into a list. Otherwise
         * this Variant will contain one list of simple type v.simpleType()
         */
        void append( const Variant& v );

        /**
         * Does compare two Variant objects. single-valued lists are treated
         * as the single value itself. For example a QStringList variant with
         * one element "x" equals a QString variant with value "x".
         */
        bool operator==( const Variant& other ) const;

        /**
         * Inverse of operator==
         */
        bool operator!=( const Variant& other ) const;

        bool isValid() const;
                
        /**
         * \return the QT Meta type id of the type
         */
        int type() const;

        /**
         * \return the type of the simple value, i.e. with
         * the list stripped.
         */
        int simpleType() const;

        /**
         * This methods does not handle all list types.
         * It checks the following:
         * \li QList<Resource>
         * \li QList<int>
         * \li QList<double>
         * \li QList<bool>
         * \li QList<QDate>
         * \li QList<QTime>
         * \li QList<QDateTime>
         * \li QList<QUrl>
         * \li QList<String> (QStringList)
         */
        bool isList() const;

        bool isInt() const;
        bool isInt64() const;
        bool isUnsignedInt() const;
        bool isUnsignedInt64() const;
        bool isBool() const;
        bool isDouble() const;
        bool isString() const;
        bool isDate() const;
        bool isTime() const;
        bool isDateTime() const;
        bool isUrl() const;
        bool isResource() const;

        bool isIntList() const;
        bool isInt64List() const;
        bool isUnsignedIntList() const;
        bool isUnsignedInt64List() const;
        bool isBoolList() const;
        bool isDoubleList() const;
        bool isStringList() const;
        bool isDateList() const;
        bool isTimeList() const;
        bool isDateTimeList() const;
        bool isUrlList() const;
        bool isResourceList() const;

        QVariant variant() const;

        int toInt() const;
        qlonglong toInt64() const;
        uint toUnsignedInt() const;
        qulonglong toUnsignedInt64() const;

        bool toBool() const;
        double toDouble() const;

        /**
         * The toString() method is a little more powerful than other
         * toXXX methods since it actually converts all values to string.
         * Thus, toString should work always (even list variants are converted 
         * to a comma-separated list)
         *
         * Resources are converted to a string representation of their URI.
         */
        QString toString() const;
        QDate toDate() const;
        QTime toTime() const;
        QDateTime toDateTime() const;
        QUrl toUrl() const;
        Resource toResource() const;

        QList<int> toIntList() const;
        QList<qlonglong> toInt64List() const;
        QList<uint> toUnsignedIntList() const;
        QList<qulonglong> toUnsignedInt64List() const;
        QList<bool> toBoolList() const;
        QList<double> toDoubleList() const;

        /**
         * Just like the toString method toStringList is able to convert all
         * supported types into a list of strings.
         */
        QStringList toStringList() const;
        QList<QDate> toDateList() const;
        QList<QTime> toTimeList() const;
        QList<QDateTime> toDateTimeList() const;
        QList<QUrl> toUrlList() const;
        QList<Resource> toResourceList() const;

        /**
         * Create a Variant object by parsing string \a value based on \a type.
         * If \a type is unknown a simple string Variant object is returned
         * containing the plain string \a value.
         */
        static Variant fromString( const QString& value, int type );

    private:
        class Private;
        Private* const d;
    };
}


NEPOMUK_EXPORT QDebug operator<<( QDebug dbg, const Nepomuk::Variant& );

Q_DECLARE_METATYPE(Nepomuk::Resource)
Q_DECLARE_METATYPE(QList<Nepomuk::Resource>)
Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QList<qlonglong>)
Q_DECLARE_METATYPE(QList<uint>)
Q_DECLARE_METATYPE(QList<qulonglong>)
Q_DECLARE_METATYPE(QList<double>)
Q_DECLARE_METATYPE(QList<bool>)
Q_DECLARE_METATYPE(QList<QDate>)
Q_DECLARE_METATYPE(QList<QTime>)
Q_DECLARE_METATYPE(QList<QDateTime>)
Q_DECLARE_METATYPE(QList<QUrl>)

#endif
