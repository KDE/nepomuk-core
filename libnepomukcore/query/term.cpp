/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include "term.h"
#include "term_p.h"
#include "literalterm.h"
#include "resourceterm.h"
#include "andterm.h"
#include "orterm.h"
#include "negationterm.h"
#include "optionalterm.h"
#include "comparisonterm.h"
#include "resourcetypeterm.h"
#include "literalterm_p.h"
#include "resourceterm_p.h"
#include "andterm_p.h"
#include "orterm_p.h"
#include "negationterm_p.h"
#include "optionalterm_p.h"
#include "comparisonterm_p.h"
#include "resourcetypeterm_p.h"
#include "queryserializer.h"

#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QDebug>

#include "property.h"
#include "variant.h"


Nepomuk2::Query::Term::Term()
    : d_ptr( new TermPrivate() )
{
}


Nepomuk2::Query::Term::Term( const Term& other )
    : d_ptr( other.d_ptr )
{
}


Nepomuk2::Query::Term::Term( TermPrivate* d )
    : d_ptr( d )
{
}


Nepomuk2::Query::Term::~Term()
{
}


Nepomuk2::Query::Term& Nepomuk2::Query::Term::operator=( const Term& other )
{
    d_ptr = other.d_ptr;
    return *this;
}


bool Nepomuk2::Query::Term::isValid() const
{
    return d_ptr->isValid();
}


Nepomuk2::Query::Term::Type Nepomuk2::Query::Term::type() const
{
    return d_ptr->m_type;
}


Nepomuk2::Query::Term Nepomuk2::Query::Term::optimized() const
{
    switch( type() ) {
    case Nepomuk2::Query::Term::And:
    case Nepomuk2::Query::Term::Or: {
        QList<Nepomuk2::Query::Term> subTerms = static_cast<const Nepomuk2::Query::GroupTerm&>( *this ).subTerms();
        QList<Nepomuk2::Query::Term> newSubTerms;
        QList<Nepomuk2::Query::Term>::const_iterator end( subTerms.constEnd() );
        for ( QList<Nepomuk2::Query::Term>::const_iterator it = subTerms.constBegin();
              it != end; ++it ) {
            const Nepomuk2::Query::Term& t = *it;
            Nepomuk2::Query::Term ot = t.optimized();
            QList<Nepomuk2::Query::Term> terms;
            if ( ot.type() == type() ) {
                terms = static_cast<const Nepomuk2::Query::GroupTerm&>( ot ).subTerms();
            }
            else if( ot.isValid() ) {
                terms += ot;
            }
            Q_FOREACH( const Nepomuk2::Query::Term& t, terms ) {
                if( !newSubTerms.contains( t ) )
                    newSubTerms += t;
            }
        }
        if ( newSubTerms.count() == 0 )
            return Nepomuk2::Query::Term();
        else if ( newSubTerms.count() == 1 )
            return *newSubTerms.begin();
        else if ( isAndTerm() )
            return Nepomuk2::Query::AndTerm( newSubTerms );
        else
            return Nepomuk2::Query::OrTerm( newSubTerms );
    }

    case Nepomuk2::Query::Term::Negation: {
        Nepomuk2::Query::NegationTerm nt = toNegationTerm();
        // a negation in a negation
        if( nt.subTerm().isNegationTerm() )
            return nt.subTerm().toNegationTerm().subTerm().optimized();
        else
            return Nepomuk2::Query::NegationTerm::negateTerm( nt.subTerm().optimized() );
    }

    case Nepomuk2::Query::Term::Optional: {
        Nepomuk2::Query::OptionalTerm ot = toOptionalTerm();
        // remove duplicate optional terms
        if( ot.subTerm().isOptionalTerm() )
            return ot.subTerm().optimized();
        else
            return Nepomuk2::Query::OptionalTerm::optionalizeTerm( ot.subTerm().optimized() );
    }

    case Nepomuk2::Query::Term::Comparison: {
        Nepomuk2::Query::ComparisonTerm ct( toComparisonTerm() );
        ct.setSubTerm( ct.subTerm().optimized() );
        return ct;
    }

    default:
        return *this;
    }
}


bool Nepomuk2::Query::Term::isLiteralTerm() const
{
    return type() == Literal;
}


bool Nepomuk2::Query::Term::isResourceTerm() const
{
    return type() == Resource;
}


bool Nepomuk2::Query::Term::isNegationTerm() const
{
    return type() == Negation;
}


bool Nepomuk2::Query::Term::isOptionalTerm() const
{
    return type() == Optional;
}


bool Nepomuk2::Query::Term::isAndTerm() const
{
    return type() == And;
}


bool Nepomuk2::Query::Term::isOrTerm() const
{
    return type() == Or;
}


bool Nepomuk2::Query::Term::isComparisonTerm() const
{
    return type() == Comparison;
}


bool Nepomuk2::Query::Term::isResourceTypeTerm() const
{
    return type() == ResourceType;
}


Nepomuk2::Query::LiteralTerm Nepomuk2::Query::Term::toLiteralTerm() const
{
    if ( isLiteralTerm() ) {
        return *static_cast<const LiteralTerm*>( this );
    }
    else
        return LiteralTerm();
}


Nepomuk2::Query::ResourceTerm Nepomuk2::Query::Term::toResourceTerm() const
{
    if ( isResourceTerm() )
        return *static_cast<const ResourceTerm*>( this );
    else
        return ResourceTerm();
}


Nepomuk2::Query::NegationTerm Nepomuk2::Query::Term::toNegationTerm() const
{
    if ( isNegationTerm() )
        return *static_cast<const NegationTerm*>( this );
    else
        return NegationTerm();
}


Nepomuk2::Query::OptionalTerm Nepomuk2::Query::Term::toOptionalTerm() const
{
    if ( isOptionalTerm() )
        return *static_cast<const OptionalTerm*>( this );
    else
        return OptionalTerm();
}


Nepomuk2::Query::AndTerm Nepomuk2::Query::Term::toAndTerm() const
{
    if ( isAndTerm() )
        return *static_cast<const AndTerm*>( this );
    else
        return AndTerm();
}


Nepomuk2::Query::OrTerm Nepomuk2::Query::Term::toOrTerm() const
{
    if ( isOrTerm() )
        return *static_cast<const OrTerm*>( this );
    else
        return OrTerm();
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::Term::toComparisonTerm() const
{
    if ( isComparisonTerm() )
        return *static_cast<const ComparisonTerm*>( this );
    else
        return ComparisonTerm();
}


Nepomuk2::Query::ResourceTypeTerm Nepomuk2::Query::Term::toResourceTypeTerm() const
{
    if ( isResourceTypeTerm() )
        return *static_cast<const ResourceTypeTerm*>( this );
    else
        return ResourceTypeTerm();
}


#define CONVERT_AND_RETURN( Class ) \
    if ( !is##Class() )                                       \
        d_ptr = new Class##Private();                         \
    return *static_cast<Class*>( this )

Nepomuk2::Query::LiteralTerm& Nepomuk2::Query::Term::toLiteralTerm()
{
    CONVERT_AND_RETURN( LiteralTerm );
}


Nepomuk2::Query::ResourceTerm& Nepomuk2::Query::Term::toResourceTerm()
{
    CONVERT_AND_RETURN( ResourceTerm );
}


Nepomuk2::Query::NegationTerm& Nepomuk2::Query::Term::toNegationTerm()
{
    CONVERT_AND_RETURN( NegationTerm );
}


Nepomuk2::Query::OptionalTerm& Nepomuk2::Query::Term::toOptionalTerm()
{
    CONVERT_AND_RETURN( OptionalTerm );
}


Nepomuk2::Query::AndTerm& Nepomuk2::Query::Term::toAndTerm()
{
    CONVERT_AND_RETURN( AndTerm );
}


Nepomuk2::Query::OrTerm& Nepomuk2::Query::Term::toOrTerm()
{
    CONVERT_AND_RETURN( OrTerm );
}


Nepomuk2::Query::ComparisonTerm& Nepomuk2::Query::Term::toComparisonTerm()
{
    CONVERT_AND_RETURN( ComparisonTerm );
}


Nepomuk2::Query::ResourceTypeTerm& Nepomuk2::Query::Term::toResourceTypeTerm()
{
    CONVERT_AND_RETURN( ResourceTypeTerm );
}


QString Nepomuk2::Query::Term::toString() const
{
    return Nepomuk2::Query::serializeTerm( *this );
}


// static
Nepomuk2::Query::Term Nepomuk2::Query::Term::fromString( const QString& s )
{
    return Nepomuk2::Query::parseTerm( s );
}


// static
Nepomuk2::Query::Term Nepomuk2::Query::Term::fromVariant( const Variant& variant )
{
    if( variant.isResource() ) {
        return ResourceTerm( variant.toResource() );
    }
    else if( !variant.isList() ) {
        Soprano::LiteralValue v( variant.variant() );
        if( v.isValid() ) {
            return LiteralTerm( v );
        }
    }

    // fallback: invalid term
    return Term();
}


// static
Nepomuk2::Query::Term Nepomuk2::Query::Term::fromProperty( const Nepomuk2::Types::Property& property, const Nepomuk2::Variant& variant )
{
    if( variant.isList() ) {
        AndTerm andTerm;
        Q_FOREACH( const Variant& v, variant.toVariantList() ) {
            andTerm.addSubTerm( fromProperty(property, v) );
        }
        return andTerm;
    }
    else {
        return ComparisonTerm( property, Term::fromVariant(variant), ComparisonTerm::Equal );
    }
}


bool Nepomuk2::Query::Term::operator==( const Term& other ) const
{
    return d_ptr->equals( other.d_ptr );
}


bool Nepomuk2::Query::Term::operator!=( const Term& other ) const
{
    return !d_ptr->equals( other.d_ptr );
}


QDebug operator<<( QDebug dbg, const Nepomuk2::Query::Term& term )
{
    return term.operator<<( dbg );
}


Nepomuk2::Query::Term Nepomuk2::Query::operator&&( const Term& term1, const Term& term2 )
{
    QList<Term> terms;
    if( term1.isAndTerm() )
        terms << term1.toAndTerm().subTerms();
    else if( term1.isValid() )
        terms << term1;
    if( term2.isAndTerm() )
        terms << term2.toAndTerm().subTerms();
    else if( term2.isValid() )
        terms << term2;

    if( terms.count() == 1 )
        return terms.first();
    else if( terms.count() > 1 )
        return AndTerm( terms );
    else
        return Term();
}


Nepomuk2::Query::Term Nepomuk2::Query::operator||( const Term& term1, const Term& term2 )
{
    QList<Term> terms;
    if( term1.isOrTerm() )
        terms << term1.toOrTerm().subTerms();
    else if( term1.isValid() )
        terms << term1;
    if( term2.isOrTerm() )
        terms << term2.toOrTerm().subTerms();
    else if( term2.isValid() )
        terms << term2;

    if( terms.count() == 1 )
        return terms.first();
    else if( terms.count() > 1 )
        return OrTerm( terms );
    else
        return Term();
}


Nepomuk2::Query::Term Nepomuk2::Query::operator!( const Nepomuk2::Query::Term& term )
{
    return NegationTerm::negateTerm( term );
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::operator<( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return ComparisonTerm( property, term, ComparisonTerm::Smaller );
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::operator>( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return ComparisonTerm( property, term, ComparisonTerm::Greater );
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::operator<=( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return ComparisonTerm( property, term, ComparisonTerm::SmallerOrEqual );
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::operator>=( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return ComparisonTerm( property, term, ComparisonTerm::GreaterOrEqual );
}


Nepomuk2::Query::ComparisonTerm Nepomuk2::Query::operator==( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return ComparisonTerm( property, term, ComparisonTerm::Equal );
}


Nepomuk2::Query::Term Nepomuk2::Query::operator!=( const Nepomuk2::Types::Property& property, const Nepomuk2::Query::Term& term )
{
    return !( property == term );
}


uint Nepomuk2::Query::qHash( const Nepomuk2::Query::Term& term )
{
    switch( term.type() ) {
    case Nepomuk2::Query::Term::Literal:
        return( qHash( term.toLiteralTerm().value().toString() ) );

    case Nepomuk2::Query::Term::Comparison:
        return( qHash( term.toComparisonTerm().property().uri().toString() )<<24 |
                qHash( term.toComparisonTerm().subTerm() )<<16 |
                ( uint )term.toComparisonTerm().comparator()<<8 );

    case Nepomuk2::Query::Term::Negation:
        return qHash(term.toNegationTerm().subTerm());

    case Nepomuk2::Query::Term::Optional:
        return qHash(term.toOptionalTerm().subTerm());

    case Nepomuk2::Query::Term::Resource:
        return qHash( term.toResourceTerm().resource().uri() );

    case Nepomuk2::Query::Term::ResourceType:
        return qHash( term.toResourceTypeTerm().type().uri() );

    case Nepomuk2::Query::Term::And:
    case Nepomuk2::Query::Term::Or: {
        uint h = ( uint )term.type();
        QList<Nepomuk2::Query::Term> subTerms = static_cast<const GroupTerm&>( term ).subTerms();
        for ( int i = 0; i < subTerms.count(); ++i ) {
            h |= ( qHash( subTerms[i] )<<i );
        }
        return h;
    }

    default:
        return 0;
    }
}


/// We need to overload QSharedDataPointer::clone to make sure the correct subclasses are created
/// when detaching. The default implementation would always call TermPrivate( const TermPrivate& )
template<> Nepomuk2::Query::TermPrivate* QSharedDataPointer<Nepomuk2::Query::TermPrivate>::clone()
{
    return d->clone();
}


QDebug Nepomuk2::Query::Term::operator<<( QDebug dbg ) const
{
    return dbg << toString();
}
