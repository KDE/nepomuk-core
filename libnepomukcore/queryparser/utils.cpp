/* This file is part of the Nepomuk query parser
   Copyright (c) 2013 Denis Steckelmacher <steckdenis@yahoo.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2.1 as published by the Free Software Foundation,
   or any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "utils.h"
#include "pass_dateperiods.h"

#include "literalterm.h"
#include "andterm.h"
#include "orterm.h"
#include "negationterm.h"
#include "comparisonterm.h"
#include "resourcetypeterm.h"
#include "property.h"
#include "nie.h"
#include "nmo.h"
#include "nfo.h"

#include <soprano/literalvalue.h>
#include <klocale.h>
#include <kcalendarsystem.h>
#include <klocalizedstring.h>

QString termStringValue(const Nepomuk2::Query::Term &term)
{
    if (!term.isLiteralTerm()) {
        return QString();
    }

    Soprano::LiteralValue value = term.toLiteralTerm().value();

    if (value.isString()) {
        return value.toString();
    } else {
        return QString();
    }
}

bool termIntValue(const Nepomuk2::Query::Term& term, int &value)
{
    if (!term.isLiteralTerm()) {
        return false;
    }

    Soprano::LiteralValue v = term.toLiteralTerm().value();

    if (!v.isInt() && !v.isInt64()) {
        return false;
    }

    value = v.toInt();
    return true;
}

static Nepomuk2::Query::AndTerm intervalComparison(const Nepomuk2::Types::Property &prop,
                                                   const Nepomuk2::Query::LiteralTerm &min,
                                                   const Nepomuk2::Query::LiteralTerm &max)
{
    int start_position = qMin(min.position(), max.position());
    int end_position = qMax(min.position() + min.length(), max.position() + max.length());

    Nepomuk2::Query::ComparisonTerm greater(prop, min, Nepomuk2::Query::ComparisonTerm::GreaterOrEqual);
    Nepomuk2::Query::ComparisonTerm smaller(prop, max, Nepomuk2::Query::ComparisonTerm::SmallerOrEqual);

    greater.setPosition(start_position, end_position - start_position);
    smaller.setPosition(greater);

    Nepomuk2::Query::AndTerm total(greater, smaller);
    total.setPosition(greater);

    return total;
}

static Nepomuk2::Query::AndTerm dateTimeComparison(const Nepomuk2::Types::Property &prop,
                                                   const Nepomuk2::Query::LiteralTerm &term)
{
    KCalendarSystem *cal = KCalendarSystem::create(KGlobal::locale()->calendarSystem());
    QDateTime start_date_time = term.value().toDateTime().toLocalTime();

    QDate start_date(start_date_time.date());
    QTime start_time(start_date_time.time());
    QDate end_date(start_date);
    QTime end_time(start_time);
    PassDatePeriods::Period last_defined_period = (PassDatePeriods::Period)(start_time.msec());

    switch (last_defined_period) {
    case PassDatePeriods::Year:
        end_date = cal->addYears(start_date, 1);
        break;
    case PassDatePeriods::Month:
        end_date = cal->addMonths(start_date, 1);
        break;
    case PassDatePeriods::Week:
        end_date = cal->addDays(start_date, cal->dayOfWeek(end_date));
        break;
    case PassDatePeriods::DayOfWeek:
    case PassDatePeriods::Day:
        end_date = cal->addDays(start_date, 1);
        break;
    default:
        break;
    }

    QDateTime datetime(end_date, end_time);

    switch (last_defined_period) {
    case PassDatePeriods::Hour:
        datetime = datetime.addSecs(60 * 60);
        break;
    case PassDatePeriods::Minute:
        datetime = datetime.addSecs(60);
        break;
    case PassDatePeriods::Second:
        datetime = datetime.addSecs(1);
        break;
    default:
        break;
    }

    Nepomuk2::Query::LiteralTerm end_term(datetime);
    end_term.setPosition(term);

    return intervalComparison(
        prop,
        term,
        end_term
    );
}

Nepomuk2::Query::Term fuseTerms(const QList<Nepomuk2::Query::Term> &terms, int first_term_index, int& end_term_index)
{
    Nepomuk2::Query::Term fused_term;
    bool build_and = true;
    bool build_not = false;

    QUrl default_filesize_property = Nepomuk2::Vocabulary::NIE::byteSize();
    QUrl default_datetime_property = Nepomuk2::Vocabulary::NIE::created();

    QString and_string = i18n("and");
    QString or_string = i18n("or");
    QString not_string = i18n("not");

    // Fuse terms in nested AND and OR terms. "a AND b OR c" is fused as
    // "(a AND b) OR c"
    for (end_term_index=first_term_index; end_term_index<terms.size(); ++end_term_index) {
        Nepomuk2::Query::Term term = terms.at(end_term_index);

        if (term.isComparisonTerm()) {
            Nepomuk2::Query::ComparisonTerm comparison = term.toComparisonTerm();

            if (comparison.comparator() == Nepomuk2::Query::ComparisonTerm::Equal &&
                comparison.subTerm().isLiteralTerm() &&
                comparison.subTerm().toLiteralTerm().value().isDateTime())
            {
                // We try to compare exactly with a date-time, which is impossible
                // (except if you want to find documents edited precisely at
                // the millisecond you want)
                // Build a comparison against an interval
                term = dateTimeComparison(comparison.property(), comparison.subTerm().toLiteralTerm());

                // Only comparison has information about the position of the whole
                // comparison, as the date-time comparison was built with only the
                // literal terms
                Nepomuk2::Query::Term min_term, max_term;

                min_term = term.toAndTerm().subTerms().at(0);
                max_term = term.toAndTerm().subTerms().at(1);

                term.setPosition(comparison);
                min_term.setPosition(comparison);
                max_term.setPosition(comparison);

                term.toAndTerm().setSubTerms(QList<Nepomuk2::Query::Term>() << min_term << max_term);
            }
        } else if (term.isResourceTypeTerm()) {
            QUrl uri = term.toResourceTypeTerm().type().uri();

            if (uri == Nepomuk2::Vocabulary::NMO::Message()) {
                default_datetime_property = Nepomuk2::Vocabulary::NMO::receivedDate();
                default_filesize_property = Nepomuk2::Vocabulary::NIE::contentSize();
            } else if (uri == Nepomuk2::Vocabulary::NFO::FileDataObject() ||
                       uri == Nepomuk2::Vocabulary::NFO::Document()) {
                default_datetime_property = Nepomuk2::Vocabulary::NFO::fileLastModified();
                default_filesize_property = Nepomuk2::Vocabulary::NFO::fileSize();
            }
        } else if (term.isLiteralTerm()) {
            Soprano::LiteralValue value = term.toLiteralTerm().value();

            if (value.isDateTime()) {
                // Default property for date-times
                term = dateTimeComparison(
                    default_datetime_property,
                    term.toLiteralTerm()
                );
            } else if (value.isInt() || value.isInt64()) {
                long long int v = value.toInt64();
                Nepomuk2::Query::LiteralTerm min(v * 80LL / 100LL);
                Nepomuk2::Query::LiteralTerm max(v * 120LL / 100LL);

                min.setPosition(term);
                max.setPosition(term);

                term = intervalComparison(default_filesize_property, min, max);
            } else if (value.isString()) {
                QString content = value.toString().toLower();

                if (content == or_string) {
                    // Consume the OR term, the next term will be ORed with the previous
                    build_and = false;
                    continue;
                } else if (content == and_string ||
                           content == QLatin1String("+")) {
                    // Consume the AND term
                    build_and = true;
                    continue;
                } else if (content == QLatin1String("!") ||
                           content == not_string ||
                           content == QLatin1String("-")) {
                    // Consume the negation
                    build_not = true;
                    continue;
                } else if (content == QLatin1String("(")) {
                    // Fuse the nested query
                    term = fuseTerms(terms, end_term_index + 1, end_term_index);
                } else if (content == QLatin1String(")")) {
                    // Done
                    return fused_term;
                } else if (content.size() <= 2) {
                    // Ignore small terms, they are typically "to", "a", etc.
                    // NOTE: Some locales may want to have this filter removed
                    continue;
                }
            }
        }

        // Negate the term if needed
        if (build_not) {
            Nepomuk2::Query::Term negated =
                Nepomuk2::Query::NegationTerm::negateTerm(term);

            negated.setPosition(term);

            term = negated;
        }

        // Add term to the fused term
        if (!fused_term.isValid()) {
            fused_term = term;
        } else if (build_and) {
            if (fused_term.isAndTerm()) {
                fused_term.toAndTerm().addSubTerm(term);
            } else {
                fused_term = Nepomuk2::Query::AndTerm(fused_term, term);
            }
        } else {
            if (fused_term.isOrTerm()) {
                fused_term.toOrTerm().addSubTerm(term);
            } else {
                fused_term = Nepomuk2::Query::OrTerm(fused_term, term);
            }
        }

        // Position information
        int start_position = qMin(fused_term.position(), term.position());
        int end_position = qMax(fused_term.position() + fused_term.length(),
                                term.position() + term.length());

        fused_term.setPosition(
            start_position,
            end_position - start_position
        );

        // Default to AND, and don't invert terms
        build_and = true;
        build_not = false;
    }

    return fused_term;
}
