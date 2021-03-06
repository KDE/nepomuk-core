/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2009-2010 Sebastian Trueg <trueg@kde.org>

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

#include "comparisonterm.h"
#include "comparisonterm_p.h"
#include "querybuilderdata_p.h"
#include "literalterm.h"
#include "literalterm_p.h"
#include "resourceterm.h"
#include "resource.h"
#include "query_p.h"

#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/Vocabulary/RDFS>

#include "literal.h"
#include "class.h"

#include <kdebug.h>


namespace {

    QString varInAggregateFunction( Nepomuk2::Query::ComparisonTerm::AggregateFunction f, const QString& varName )
    {
        switch( f ) {
        case Nepomuk2::Query::ComparisonTerm::Count:
            return QString::fromLatin1("count(%1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::DistinctCount:
            return QString::fromLatin1("count(distinct %1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::Max:
            return QString::fromLatin1("max(%1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::Min:
            return QString::fromLatin1("min(%1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::Sum:
            return QString::fromLatin1("sum(%1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::DistinctSum:
            return QString::fromLatin1("sum(distinct %1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::Average:
            return QString::fromLatin1("avg(%1)").arg(varName);
        case Nepomuk2::Query::ComparisonTerm::DistinctAverage:
            return QString::fromLatin1("avg(distinct %1)").arg(varName);
        default:
            return QString();
        }
    }
}

QString Nepomuk2::Query::comparatorToString( Nepomuk2::Query::ComparisonTerm::Comparator c )
{
    switch( c ) {
    case Nepomuk2::Query::ComparisonTerm::Contains:
        return QChar( ':' );
    case Nepomuk2::Query::ComparisonTerm::Equal:
        return QChar( '=' );
    case Nepomuk2::Query::ComparisonTerm::Regexp:
        return QLatin1String( "regex" );
    case Nepomuk2::Query::ComparisonTerm::Greater:
        return QChar( '>' );
    case Nepomuk2::Query::ComparisonTerm::Smaller:
        return QChar( '<' );
    case Nepomuk2::Query::ComparisonTerm::GreaterOrEqual:
        return QLatin1String( ">=" );
    case Nepomuk2::Query::ComparisonTerm::SmallerOrEqual:
        return QLatin1String( "<=" );
    default:
        return QString();
    }
}


Nepomuk2::Query::ComparisonTerm::Comparator Nepomuk2::Query::stringToComparator( const QStringRef& c )
{
    if( c == QChar( '=' ) )
        return Nepomuk2::Query::ComparisonTerm::Equal;
    else if( c == QLatin1String( "regex" ) )
        return Nepomuk2::Query::ComparisonTerm::Regexp;
    else if( c == QChar( '>' ) )
        return Nepomuk2::Query::ComparisonTerm::Greater;
    else if( c == QChar( '<' ) )
        return Nepomuk2::Query::ComparisonTerm::Smaller;
    else if( c == QLatin1String( ">=" ) )
        return Nepomuk2::Query::ComparisonTerm::GreaterOrEqual;
    else if( c == QLatin1String( "<=" ) )
        return Nepomuk2::Query::ComparisonTerm::SmallerOrEqual;
    else
        return Nepomuk2::Query::ComparisonTerm::Contains;
}

QString Nepomuk2::Query::ComparisonTermPrivate::toSparqlGraphPattern( const QString& resourceVarName, const TermPrivate* parentTerm, const QString &additionalFilters, QueryBuilderData *qbd ) const
{
    Q_UNUSED(parentTerm);

    //
    // 1. property range: literal
    // 1.1. operator =
    //      use a single pattern like: ?r <prop> "value"
    // 1.2. operator :
    //      use two patterns: ?r <prop> ?v . ?v bif:contains "'value'"
    // 1.3. operator <,>,<=,>=
    //      use two patterns: ?r <prop> ?v . FILTER(?v < value)
    // fail if subterm is not a literal term
    //
    // 2. property range is class
    // 2.1. operator =
    // 2.1.1. literal subterm
    //        use two patterns like: ?r <prop> ?v . ?v rdfs:label "value"
    // 2.1.2. resource subterm
    //        use one pattern: ?r <prop> <res>
    // 2.1.3. subterm type and, or, comparision
    //        use one pattern and the subpattern: ?r <prop> ?v . subpattern(?v)
    // 2.2. operator :
    // 2.2.1. literal subterm
    //        use 3 pattern: ?r <prop> ?v . ?v rdfs:label ?l . ?l bif:contains "'value'"
    // 2.2.2. resource subterm
    //        same as =
    // 2.2.3. subterm type and, or, comparision
    //        same as =
    // 2.3. operator <,>,<=,>=
    //      fail!
    //

    if ( !m_subTerm.isValid() ) {
        QString finalPattern;
        QString prop = propertyToString( qbd );
        bool firstUse = false;
        QString ov = getMainVariableName( qbd, &firstUse );
        if( m_inverted )
            finalPattern = QString::fromLatin1( "%1 %2 %3 . " )
                           .arg( ov, prop, resourceVarName );
        else if( firstUse )
            finalPattern = QString::fromLatin1( "%1 %2 %3 . " )
                           .arg( resourceVarName, prop, ov );
        else
            return QString();

        return finalPattern + additionalFilters;
    }

    // when using Regexp comparator with a resource range property we match the resource URI to the regular expression
    else if ( m_property.literalRangeType().isValid() || m_comparator == ComparisonTerm::Regexp ) {
        QString finalPattern;

        if( !m_subTerm.isLiteralTerm() )
            kDebug() << "Incompatible subterm type:" << m_subTerm.type();
        if ( m_comparator == ComparisonTerm::Equal ) {
            finalPattern = QString::fromLatin1( "%1 %2 %3 . " )
                           .arg( resourceVarName,
                                 propertyToString( qbd ),
                                 Soprano::Node::literalToN3( m_subTerm.toLiteralTerm().value() ) );
        }
        else if ( m_comparator == ComparisonTerm::Contains ) {
            bool firstUse = false;
            const QString v = getMainVariableName(qbd, &firstUse);
            finalPattern = LiteralTermPrivate::createContainsPattern( v, m_subTerm.toLiteralTerm().value().toString(), qbd );
            if(firstUse) {
                finalPattern.prepend(QString::fromLatin1( "%1 %2 %3 . " )
                                     .arg( resourceVarName,
                                           propertyToString( qbd ),
                                           v ));
            }
        }
        else if ( m_comparator == ComparisonTerm::Regexp ) {
            bool firstUse = false;
            const QString v = getMainVariableName(qbd, &firstUse);
            finalPattern = QString::fromLatin1( "FILTER(REGEX(STR(%1), '%2', 'i')) . " )
                           .arg( v, m_subTerm.toLiteralTerm().value().toString() );
            if( firstUse ) {
                finalPattern.prepend(QString::fromLatin1( "%1 %2 %3 . " )
                                     .arg( resourceVarName,
                                           propertyToString( qbd ),
                                           v ));
            }
        }
        else {
            bool firstUse = false;
            const QString v = getMainVariableName(qbd, &firstUse);
            finalPattern = QString::fromLatin1( "FILTER(%1%2%3) . " )
                           .arg( v,
                                 comparatorToString( m_comparator ),
                                 Soprano::Node::literalToN3(m_subTerm.toLiteralTerm().value()) );
            if( firstUse ) {
                finalPattern.prepend( QString::fromLatin1( "%1 %2 %3 . " )
                                      .arg( resourceVarName,
                                            propertyToString( qbd ),
                                            v ) );
            }
        }

        return finalPattern + additionalFilters;
    }

    else { // resource range
        if( !(m_comparator == ComparisonTerm::Equal ||
              m_comparator == ComparisonTerm::Contains ||
              m_comparator == ComparisonTerm::Regexp )) {
            kDebug() << "Incompatible property range:" << m_property.range().uri();
        }

        //
        // The core pattern is always the same: we match to resources that have a certain
        // property defined. The value of that property is filled in below.
        //
        QString corePattern;
        QString subject;
        QString object;
        if( m_inverted && !m_subTerm.isLiteralTerm() ) {
            subject = QLatin1String("%1"); // funny way to have a resulting string which takes only one arg
            object = resourceVarName;
        }
        else {
            subject = resourceVarName;
            object = QLatin1String("%1");
        }
        if( qbd->flags() & Query::HandleInverseProperties &&
            m_property.inverseProperty().isValid() ) {
            corePattern = QString::fromLatin1("{ %1 %2 %3 . %5} UNION { %3 %4 %1 . %5} . ")
                              .arg( subject,
                                    propertyToString( qbd ),
                                    object,
                                    Soprano::Node::resourceToN3( m_property.inverseProperty().uri() ),
                                    additionalFilters );
        }
        else {
            corePattern = QString::fromLatin1("%1 %2 %3 . %4")
                              .arg( subject,
                                    propertyToString( qbd ),
                                    object,
                                    additionalFilters );
        }

        if ( m_subTerm.isLiteralTerm() ) {
            //
            // the base of the pattern is always the same: match to resources related to X
            // which has a label that we compare somehow. This label's value will be filled below
            //
            bool firstUse = true;
            QString v1 = getMainVariableName(qbd, &firstUse);
            QString pattern = QString::fromLatin1( "%1%2 %4 %3 . " )
                    .arg( firstUse ? corePattern.arg(v1) : QString(),
                          v1,
                          QLatin1String("%1"), // funny way to have a resulting string which takes only one arg
// FIXME: Change back to rdfs:label when virtuoso inferencing bug is fixed
// BUG: 3591024 - https://sourceforge.net/tracker/?func=detail&aid=3591024&group_id=161622&atid=820574
                          qbd->uniqueVarName() ); // Soprano::Node::resourceToN3( Soprano::Vocabulary::RDFS::label() ) );

            if ( m_comparator == ComparisonTerm::Equal ) {
                return pattern.arg( Soprano::Node::literalToN3( m_subTerm.toLiteralTerm().value() ) );
            }
            else if ( m_comparator == ComparisonTerm::Contains ) {
                QString v3 = qbd->uniqueVarName();
                // since this is not a "real" full text search but rather a match on resource "names" we do not call QueryBuilderData::addFullTextSearchTerm
                return pattern.arg(v3)
                        + LiteralTermPrivate::createContainsPattern( v3,
                                                                     m_subTerm.toLiteralTerm().value().toString(),
                                                                     qbd );
            }
            else if ( m_comparator == ComparisonTerm::Regexp ) {
                QString v3 = qbd->uniqueVarName();
                return QString::fromLatin1( "%1FILTER(REGEX(STR(%2)), '%3', 'i') . " )
                    .arg( pattern.arg(v3),
                          v3,
                          m_subTerm.toLiteralTerm().value().toString() );
            }
            else {
                kDebug() << QString( "Invalid Term: comparator %1 cannot be used for matching to a resource!" ).arg( comparatorToString( m_comparator ) );
                return QString();
            }
        }
        else if ( m_subTerm.isResourceTerm() ) {
            // ?r <prop> <res>
            return corePattern.arg( Soprano::Node::resourceToN3(m_subTerm.toResourceTerm().resource().uri()) );
        }
        else {
            // ?r <prop> ?v1 . ?v1 ...
            bool firstUse = true;
            QString v = getMainVariableName(qbd, &firstUse);
            qbd->increaseDepth();
            QString subTermSparql = m_subTerm.d_ptr->toSparqlGraphPattern( v, this, QString(), qbd );
            qbd->decreaseDepth();
            if( firstUse )
                return corePattern.arg(v) + subTermSparql;
            else
                return subTermSparql;
        }
    }
}



bool Nepomuk2::Query::ComparisonTermPrivate::equals( const TermPrivate* other ) const
{
    if ( other->m_type == m_type ) {
        const ComparisonTermPrivate* ctp = static_cast<const ComparisonTermPrivate*>( other );
        return( ctp->m_property == m_property &&
                ctp->m_comparator == m_comparator &&
                ctp->m_subTerm == m_subTerm &&
                ctp->m_inverted == m_inverted &&
                ctp->m_sortOrder == m_sortOrder &&
                ctp->m_sortWeight == m_sortWeight &&
                ctp->m_variableName == m_variableName );
    }
    else {
        return false;
    }
}


bool Nepomuk2::Query::ComparisonTermPrivate::isValid() const
{
    // an invalid property will simply match all properties
    // and an invalid subterm is a wildcard, too
    // Thus, a ComparisonTerm is always valid
    return true;
}


//
// Determines the main variable name, i.e. the variable representing the compared value, i.e. the one
// that can be set vie setVariableName.
//
// Thus, the basic scheme is: if a variable name has been set via setVariableName, return that name,
// otherwise create a random new one.
//
// But this method also handles AggregateFunction and sorting. The latter is a bit hacky.
//
// There a quite a few cases that are handled:
//
// 1. No variable name set
// 1.1 no aggregate function set
// 1.1.1 no sorting weight set
//       -> return a new random variable
// 1.1.2 sorting weight set
//       -> determine a new random variable, use it as sorting var (via QueryBuilderData::addOrderVariable),
//          and return it
// 1.2 an aggregate function has been set
// 1.2.1 sorting weight set (no sorting weight is useless and behaves exactly as if no aggregate function was set)
//       -> embed a new random variable in the aggregate expression, add that as sort variable, and
//          return the new variable
// 2. A variable name has been set
// 2.1 no aggregate function set
// 2.1.1 no sorting weight set
//       -> add the variable name as custom variable via QueryBuilderData::addCustomVariable and return the variable name
// 2.1.2 sorting weight set
//       -> add the variable name as sort car via QueryBuilderData::addOrderVariable and return it
// 2.2 an aggregate function has been set
// 2.2.1 no sorting weight set
//       -> Create a new random variable, embed it in the aggregate expression with the set variable name,
//          use that expression as custom variable (this is the hacky part), and return the random one
// 2.2.2 sorting weight set
//       -> Do the same as above only also add the set variable name as sort variable via QueryBuilderData::addOrderVariable
//
QString Nepomuk2::Query::ComparisonTermPrivate::getMainVariableName( QueryBuilderData* qbd, bool* firstUse ) const
{
    QString v;
    QString sortVar;
    if( !m_variableName.isEmpty() ) {
        qbd->registerVarName( m_property, QLatin1String("?") + m_variableName );
        *firstUse = true;

        sortVar = QLatin1String("?") + m_variableName;
        if( m_aggregateFunction == ComparisonTerm::NoAggregateFunction ) {
            v = sortVar;
            qbd->addCustomVariable( v );
        }
        else {
            // this is a bit hacky as far as the method naming in QueryBuilderData is concerned.
            // we add a select statement as a variable name.
            v = qbd->uniqueVarName();
            QString selectVar = QString::fromLatin1( "%1 as ?%2")
                                .arg(varInAggregateFunction(m_aggregateFunction, v),
                                     m_variableName );
            qbd->addCustomVariable( selectVar );
        }
    }
    else {
        //
        // for inverted terms we do not perform variable sharing as described in QueryBuilderData::uniqueVarName
        // since we would have to check inverse properties and their cardinality and the former is rarely defined.
        //
        v = qbd->uniqueVarName( m_inverted ? Types::Property() : m_property, firstUse );
        if( m_aggregateFunction == ComparisonTerm::NoAggregateFunction )
            sortVar = v;
        else
            sortVar = varInAggregateFunction(m_aggregateFunction, v);
    }
    if( m_sortWeight != 0 ) {
        qbd->addOrderVariable( sortVar, m_sortWeight, m_sortOrder );

        // trueg: there seems to be a bug in Virtuoso which gives wrong search order if the sort variable is not in the select list.
        if( m_aggregateFunction == ComparisonTerm::NoAggregateFunction )
            qbd->addCustomVariable( sortVar );
    }
    return v;
}


QString Nepomuk2::Query::ComparisonTermPrivate::propertyToString( QueryBuilderData* qbd ) const
{
    if( m_property.isValid() )
        return Soprano::Node::resourceToN3( m_property.uri() );
    else
        return qbd->uniqueVarName();
}


Nepomuk2::Query::ComparisonTerm::ComparisonTerm()
    : SimpleTerm( new ComparisonTermPrivate() )
{
}


Nepomuk2::Query::ComparisonTerm::ComparisonTerm( const ComparisonTerm& term )
    : SimpleTerm( term )
{
}


Nepomuk2::Query::ComparisonTerm::ComparisonTerm( const Types::Property& property, const Term& term, Comparator comparator )
    : SimpleTerm( new ComparisonTermPrivate() )
{
    N_D( ComparisonTerm );
    d->m_property = property;
    d->m_subTerm = term;
    d->m_comparator = comparator;
}


Nepomuk2::Query::ComparisonTerm::~ComparisonTerm()
{
}


Nepomuk2::Query::ComparisonTerm& Nepomuk2::Query::ComparisonTerm::operator=( const ComparisonTerm& term )
{
    d_ptr = term.d_ptr;
    return *this;
}


Nepomuk2::Query::ComparisonTerm::Comparator Nepomuk2::Query::ComparisonTerm::comparator() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_comparator;
}


Nepomuk2::Types::Property Nepomuk2::Query::ComparisonTerm::property() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_property;
}


void Nepomuk2::Query::ComparisonTerm::setComparator( Comparator comparator )
{
    N_D( ComparisonTerm );
    d->m_comparator = comparator;
}


void Nepomuk2::Query::ComparisonTerm::setProperty( const Types::Property& property )
{
    N_D( ComparisonTerm );
    d->m_property = property;
}


void Nepomuk2::Query::ComparisonTerm::setVariableName( const QString& name )
{
    N_D( ComparisonTerm );
    d->m_variableName = name;
}


QString Nepomuk2::Query::ComparisonTerm::variableName() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_variableName;
}


void Nepomuk2::Query::ComparisonTerm::setAggregateFunction( AggregateFunction function )
{
    N_D( ComparisonTerm );
    d->m_aggregateFunction = function;
}


Nepomuk2::Query::ComparisonTerm::AggregateFunction Nepomuk2::Query::ComparisonTerm::aggregateFunction() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_aggregateFunction;
}


void Nepomuk2::Query::ComparisonTerm::setSortWeight( int weight, Qt::SortOrder sortOrder )
{
    N_D( ComparisonTerm );
    d->m_sortWeight = weight;
    d->m_sortOrder = sortOrder;
}


int Nepomuk2::Query::ComparisonTerm::sortWeight() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_sortWeight;
}


Qt::SortOrder Nepomuk2::Query::ComparisonTerm::sortOrder() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_sortOrder;
}


bool Nepomuk2::Query::ComparisonTerm::isInverted() const
{
    N_D_CONST( ComparisonTerm );
    return d->m_inverted;
}


void Nepomuk2::Query::ComparisonTerm::setInverted( bool invert )
{
    N_D( ComparisonTerm );
    d->m_inverted = invert;
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::ComparisonTerm::inverted() const
{
    ComparisonTerm ct( *this );
    ct.setInverted( !isInverted() );
    return ct;
}
