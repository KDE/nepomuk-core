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

#include "queryparser.h"
#include "patternmatcher.h"
#include "completionproposal.h"
#include "utils.h"

#include "pass_splitunits.h"
#include "pass_numbers.h"
#include "pass_filenames.h"
#include "pass_filesize.h"
#include "pass_typehints.h"
#include "pass_properties.h"
#include "pass_dateperiods.h"
#include "pass_datevalues.h"
#include "pass_periodnames.h"
#include "pass_subqueries.h"
#include "pass_comparators.h"

#include "literalterm.h"
#include "property.h"
#include "nfo.h"
#include "nmo.h"
#include "nie.h"

#include <soprano/literalvalue.h>
#include <soprano/nao.h>
#include <klocale.h>
#include <kcalendarsystem.h>
#include <klocalizedstring.h>

#include <QtCore/QList>

using namespace Nepomuk2::Query;

struct Field {
    enum Flags {
        Unset = 0,
        Absolute,
        Relative
    };

    int value;
    Flags flags;

    void reset()
    {
        value = 0;
        flags = Unset;
    }
};

struct DateTimeSpec {
    Field fields[PassDatePeriods::MaxPeriod];

    void reset()
    {
        for (int i=0; i<int(PassDatePeriods::MaxPeriod); ++i) {
            fields[i].reset();
        }
    }
};

struct QueryParser::Private
{
    Private()
    : separators(i18nc(
        "Characters that are kept in the query for further processing but are considered word boundaries",
        ",;:!?()[]{}<>=#+-"))
    {}

    QStringList split(const QString &query, bool split_separators, QList<int> *positions = NULL);

    template<typename T>
    void runPass(const T &pass,
                 int cursor_position,
                 const QString &pattern,
                 const KLocalizedString &description = KLocalizedString(),
                 CompletionProposal::Type type = Nepomuk2::Query::CompletionProposal::NoType);
    void foldDateTimes();
    void handleDateTimeComparison(DateTimeSpec &spec, const ComparisonTerm &term);

    QueryParser *parser;
    QList<Term> terms;
    QList<CompletionProposal *> proposals;

    // Parsing passes (they cache translations, queries, etc)
    PassSplitUnits pass_splitunits;
    PassNumbers pass_numbers;
    PassFileNames pass_filenames;
    PassFileSize pass_filesize;
    PassTypeHints pass_typehints;
    PassComparators pass_comparators;
    PassProperties pass_properties;
    PassDatePeriods pass_dateperiods;
    PassDateValues pass_datevalues;
    PassPeriodNames pass_periodnames;
    PassSubqueries pass_subqueries;

    // Locale-specific
    QString separators;
};

QueryParser::QueryParser()
: d(new Private)
{
    d->parser = this;
}

QueryParser::~QueryParser()
{
    qDeleteAll(d->proposals);
    delete d;
}

Query QueryParser::parse(const QString &query) const
{
    return parse(query, NoParserFlags);
}

Query QueryParser::parse(const QString &query, ParserFlags flags) const
{
    return parse(query, flags, -1);
}

Query QueryParser::parseQuery(const QString &query, ParserFlags flags)
{
    return QueryParser().parse(query, flags);
}

Query QueryParser::parseQuery(const QString &query)
{
    return QueryParser().parse(query);
}

QList<Nepomuk2::Types::Property> QueryParser::matchProperty( const QString& fieldName ) const
{
    Q_UNUSED(fieldName)

    return QList<Nepomuk2::Types::Property>();
}


Query QueryParser::parse(const QString &query, ParserFlags flags, int cursor_position) const
{
    qDeleteAll(d->proposals);

    d->terms.clear();
    d->proposals.clear();

    // Split the query into terms
    QList<int> positions;
    QStringList parts = d->split(query, true, &positions);

    for (int i=0; i<parts.count(); ++i) {
        const QString &part = parts.at(i);
        int position = positions.at(i);

        LiteralTerm term(part);
        term.setPosition(position, part.size());

        d->terms.append(term);
    }

    // Prepare literal values
    d->runPass(d->pass_splitunits, cursor_position, "%1");
    d->runPass(d->pass_numbers, cursor_position, "%1");
    d->runPass(d->pass_filesize, cursor_position, "%1 %2");
    d->runPass(d->pass_typehints, cursor_position, "%1");

    if (flags & DetectFilenamePattern) {
        d->runPass(d->pass_filenames, cursor_position, "%1");
    }

    // Date-time periods
    d->runPass(d->pass_periodnames, cursor_position, "%1");

    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Adding an offset to a period of time (%1=period, %2=offset)", "in %2 %1"));

    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::InvertedOffset);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Removing an offset from a period of time (%1=period, %2=offset)", "%2 %1 ago"));

    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset, 1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Adding 1 to a period of time", "next %1"),
        ki18n("Next week, month, day, ..."));

    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset, -1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Removing 1 to a period of time", "last %1"),
        ki18n("Previous week, month, day, ..."));

    d->pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, 1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("In one day", "tomorrow"),
        ki18n("Tomorrow"));
    d->pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, -1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("One day ago", "yesterday"),
        ki18n("Yesterday"));
    d->pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, 0);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("The current day", "today"),
        ki18n("Today"));

    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value, 1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("First period (first day, month, etc)", "first %1"),
        ki18n("First week, month, day, ..."));
    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value, -1);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Last period (last day, month, etc)", "last %1"),
        ki18n("Last week, month, day, ..."));
    d->pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value);
    d->runPass(d->pass_dateperiods, cursor_position,
        i18nc("Setting the value of a period, as in 'third week' (%1=period, %2=value)", "%2 %1"));

    // Setting values of date-time periods (14:30, June 6, etc)
    d->pass_datevalues.setPm(true);
    d->runPass(d->pass_datevalues, cursor_position,
        i18nc("An hour (%5) and an optional minute (%6), PM", "at %5 : %6 pm;at %5 h pm;at %5 pm;%5 : %6 pm;%5 h pm;%5 pm"),
        ki18n("A time after midday"));
    d->pass_datevalues.setPm(false);
    d->runPass(d->pass_datevalues, cursor_position,
        i18nc("An hour (%5) and an optional minute (%6), AM", "at %5 : %6 am;at %5 h am;at %5 am;at %5;%5 : %6 am;%5 : %6 : %7;%5 : %6;%5 h am;%5 h;%5 am"),
        ki18n("A time"));

    d->runPass(d->pass_datevalues, cursor_position, i18nc(
        "A year (%1), month (%2), day (%3), day of week (%4), hour (%5), "
            "minute (%6), second (%7), in every combination supported by your language",
        "%3 of %2 %1;%3 st|nd|rd|th %2 %1;%3 st|nd|rd|th of %2 %1;"
        "%3 of %2;%3 st|nd|rd|th %2;%3 st|nd|rd|th of %2;of %2 %1;%2 %3 st|nd|rd|th;%2 %3;%2 %1;"
        "%1 - %2 - %3;%1 - %2;%3 / %2 / %1;%3 / %2;"
        "in %2 %1; in %1;, %1;"
    ));

    // Fold date-time properties into real DateTime values
    d->foldDateTimes();

    // Comparators
    d->pass_comparators.setComparator(ComparisonTerm::Contains);
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("Equality", "contains|containing %1"),
        ki18n("Containing"));
    d->pass_comparators.setComparator(ComparisonTerm::Greater);
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("Strictly greater", "greater|bigger|more than %1;at least %1;\\> %1"),
        ki18n("Greater than"));
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("After in time", "after|since %1"),
        ki18n("After"));
    d->pass_comparators.setComparator(ComparisonTerm::Smaller);
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("Strictly smaller", "smaller|less|lesser than %1;at most %1;\\< %1"),
        ki18n("Smaller than"), CompletionProposal::DateTime);
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("Before in time", "before|until %1"),
        ki18n("Before"), CompletionProposal::DateTime);
    d->pass_comparators.setComparator(ComparisonTerm::Equal);
    d->runPass(d->pass_comparators, cursor_position,
        i18nc("Equality", "equal|equals|= %1;equal to %1"),
        ki18n("Equal to"));

    // Email-related properties
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::messageFrom(), PassProperties::String);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Sender of an e-mail", "sent by %1;from %1;sender is %1;sender %1"),
        ki18n("Sender of an e-mail"), CompletionProposal::Contact);
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::messageSubject(), PassProperties::String);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Title of an e-mail", "title %1;titled %1"),
        ki18n("Title of an e-mail"));
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::messageRecipient(), PassProperties::String);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Recipient of an e-mail", "sent to %1;to %1;recipient is %1;recipient %1"),
        ki18n("Recipient of an e-mail"), CompletionProposal::Contact);
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::sentDate(), PassProperties::DateTime);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Sending date-time", "sent at|on %1;sent %1"),
        ki18n("Date of sending"), CompletionProposal::DateTime);
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::receivedDate(), PassProperties::DateTime);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Receiving date-time", "received at|on %1;received %1"),
        ki18n("Date of reception"), CompletionProposal::DateTime);

    // File-related properties
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NFO::fileSize(), PassProperties::IntegerOrDouble);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Size of a file", "size is %1;size %1;being %1 large;%1 large"),
        ki18n("File size"));
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NFO::fileName(), PassProperties::String);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Name of a file", "name %1;named %1"),
        ki18n("File name"));
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NIE::created(), PassProperties::DateTime);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Date of creation", "created at|on|in %1;created %1"),
        ki18n("Date of creation"), CompletionProposal::DateTime);
    d->pass_properties.setProperty(Nepomuk2::Vocabulary::NIE::lastModified(), PassProperties::DateTime);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("Date of last modification", "modified|edited|dated at|on|of %1;modified|edited|dated %1"),
        ki18n("Date of last modification"), CompletionProposal::DateTime);

    // Properties having a resource range (hasTag, messageFrom, etc)
    d->pass_properties.setProperty(Soprano::Vocabulary::NAO::hasTag(), PassProperties::Tag);
    d->runPass(d->pass_properties, cursor_position,
        i18nc("A document is associated with a tag", "tagged as %1;has tag %1;tag is %1;# %1"),
        ki18n("Tag name"), CompletionProposal::Tag);

    // Different kinds of properties that need subqueries
    d->pass_subqueries.setProperty(Nepomuk2::Vocabulary::NIE::relatedTo());
    d->runPass(d->pass_subqueries, cursor_position,
        i18nc("Related to a subquery", "related to ... ,"),
        ki18n("Match items related to others"));

    // Fuse the terms into a big AND term and produce the query
    int end_index;
    Term final_term = fuseTerms(d->terms, 0, end_index);

    return Query(final_term);
}

QList<CompletionProposal *> QueryParser::completionProposals() const
{
    return d->proposals;
}

void QueryParser::addCompletionProposal(CompletionProposal *proposal)
{
    d->proposals.append(proposal);
}

QStringList QueryParser::allTags() const
{
    return d->pass_properties.tags().keys();
}

QStringList QueryParser::Private::split(const QString &query, bool split_separators, QList<int> *positions)
{
    QStringList parts;
    QString part;
    int size = query.size();
    bool between_quotes = false;

    for (int i=0; i<size; ++i) {
        QChar c = query.at(i);

        if (!between_quotes && (c.isSpace() || (split_separators && separators.contains(c)))) {
            // A part may be empty if more than one space are found in block in the input
            if (part.size() > 0) {
                parts.append(part);
                part.clear();
            }

            // Add a separator, if any
            if (split_separators && separators.contains(c)) {
                if (positions) {
                    positions->append(i);
                }

                parts.append(QString(c));
            }
        } else if (c == '"') {
            between_quotes = !between_quotes;
        } else {
            if (positions && part.size() == 0) {
                // Start of a new part, save its position in the stream
                positions->append(i);
            }

            part.append(c);
        }
    }

    if (part.size() > 0) {
        parts.append(part);
    }

    return parts;
}

template<typename T>
void QueryParser::Private::runPass(const T &pass,
                                   int cursor_position,
                                   const QString &pattern,
                                   const KLocalizedString &description,
                                   CompletionProposal::Type type)
{
    // Split the pattern at ";" characters, as a locale can have more than one
    // pattern that can be used for a given rule
    QStringList rules = pattern.split(QLatin1Char(';'));

    Q_FOREACH(const QString &rule, rules) {
        // Split the rule into parts that have to be matched
        QStringList parts = split(rule, false);
        PatternMatcher matcher(parser, terms, cursor_position, parts, type, description);

        matcher.runPass(pass);
    }
}

/*
 * Datetime-folding
 */
void QueryParser::Private::handleDateTimeComparison(DateTimeSpec &spec, const ComparisonTerm &term)
{
    QUrl property_url = term.property().uri(); // URL like date://<property>/<offset|value>
    int value = term.subTerm().toLiteralTerm().value().toInt();

    // Populate the field corresponding to the property being compared to
    Field &field = spec.fields[pass_dateperiods.periodFromName(property_url.host())];

    field.value = value;
    field.flags =
        (property_url.path() == QLatin1String("/offset") ? Field::Relative : Field::Absolute);
}

static int fieldIsRelative(const Field &field, int if_yes, int if_no)
{
    return (field.flags == Field::Relative ? if_yes : if_no);
}

static int fieldValue(const Field &field, bool in_defined_period, int now_value, int null_value)
{
    switch (field.flags) {
    case Field::Unset:
        return (in_defined_period ? now_value : null_value);
    case Field::Absolute:
        return field.value;
    case Field::Relative:
        return now_value;
    }

    return 0;
}

static LiteralTerm buildDateTimeLiteral(const DateTimeSpec &spec)
{
    KCalendarSystem *calendar = KCalendarSystem::create(KGlobal::locale()->calendarSystem());
    QDate cdate = QDate::currentDate();
    QTime ctime = QTime::currentTime();

    const Field &year = spec.fields[PassDatePeriods::Year];
    const Field &month = spec.fields[PassDatePeriods::Month];
    const Field &week = spec.fields[PassDatePeriods::Week];
    const Field &dayofweek = spec.fields[PassDatePeriods::DayOfWeek];
    const Field &day = spec.fields[PassDatePeriods::Day];
    const Field &hour = spec.fields[PassDatePeriods::Hour];
    const Field &minute = spec.fields[PassDatePeriods::Minute];
    const Field &second = spec.fields[PassDatePeriods::Second];

    // Last defined period
    PassDatePeriods::Period last_defined_date = PassDatePeriods::Day;   // If no date is given, use the current date-time
    PassDatePeriods::Period last_defined_time = PassDatePeriods::Year;  // If no time is given, use 00:00:00

    if (day.flags != Field::Unset) {
        last_defined_date = PassDatePeriods::Day;
    } else if (dayofweek.flags != Field::Unset) {
        last_defined_date = PassDatePeriods::DayOfWeek;
    } else if (week.flags != Field::Unset) {
        last_defined_date = PassDatePeriods::Week;
    } else if (month.flags != Field::Unset) {
        last_defined_date = PassDatePeriods::Month;
    } else if (year.flags != Field::Unset) {
        last_defined_date = PassDatePeriods::Year;
    }

    if (second.flags != Field::Unset) {
        last_defined_time = PassDatePeriods::Second;
    } else if (minute.flags != Field::Unset) {
        last_defined_time = PassDatePeriods::Minute;
    } else if (hour.flags != Field::Unset) {
        last_defined_time = PassDatePeriods::Hour;
    }

    // Absolute year, month, day of month
    QDate date;

    if (month.flags != Field::Unset)
    {
        // Month set, day of month
        calendar->setDate(
            date,
            fieldValue(year, last_defined_date >= PassDatePeriods::Year, calendar->year(cdate), 1),
            fieldValue(month, last_defined_date >= PassDatePeriods::Month, calendar->month(cdate), 1),
            fieldValue(day, last_defined_date >= PassDatePeriods::Day, calendar->day(cdate), 1)
        );
    } else {
        calendar->setDate(
            date,
            fieldValue(year, last_defined_date >= PassDatePeriods::Year, calendar->year(cdate), 1),
            fieldValue(day, last_defined_date >= PassDatePeriods::Week, calendar->dayOfYear(cdate), 1)
        );
    }

    // Absolute week and day of week
    int isoyear;
    int isoweek = calendar->week(date, KLocale::IsoWeekNumber, &isoyear);
    int isoday = calendar->dayOfWeek(date);

    calendar->setDateIsoWeek(
        date,
        isoyear,
        (week.flags == Field::Absolute && month.flags != Field::Unset) ?
            // Week of month, isoweek is the first week of the month
            isoweek + (week.value - 1) :
            // Week of year (or no week at all)
            fieldValue(week, last_defined_date >= PassDatePeriods::Week, isoweek, isoweek),
        fieldValue(dayofweek, last_defined_date >= PassDatePeriods::DayOfWeek, isoday, 1)
    );

    // Relative year, month, week, day of month
    if (year.flags == Field::Relative) {
        date = calendar->addYears(date, year.value);
    }
    if (month.flags == Field::Relative) {
        date = calendar->addMonths(date, month.value);
    }
    if (week.flags == Field::Relative) {
        date = calendar->addDays(date, week.value * calendar->daysInWeek(date));
    }
    if (day.flags == Field::Relative) {
        date = calendar->addDays(date, day.value);
    }

    // Absolute time
    QTime time = QTime(
        fieldValue(hour, last_defined_time >= PassDatePeriods::Hour, ctime.hour(), 0),
        fieldValue(minute, last_defined_time >= PassDatePeriods::Minute, ctime.minute(), 0),
        fieldValue(second, last_defined_time >= PassDatePeriods::Second, ctime.second(), 0)
    );

    // Relative time
    QDateTime rs(date, time);

    rs = rs.addSecs(
        fieldIsRelative(hour, hour.value * 60 * 60, 0) +
        fieldIsRelative(minute, minute.value * 60, 0) +
        fieldIsRelative(second, second.value, 0)
    );

    // Store the last defined period in the millisecond part of the date-time.
    // This way, equality comparisons with a date-time can be changed to comparisons
    // against an interval whose size is defined by the last defined period.
    rs = rs.addMSecs(
        qMax(last_defined_date, last_defined_time)
    );

    delete calendar;
    return LiteralTerm(rs);
}

void QueryParser::Private::foldDateTimes()
{
    QList<Term> new_terms;

    DateTimeSpec spec;
    bool spec_contains_interesting_data = false;
    int start_position = 1 << 30;
    int end_position = 0;

    spec.reset();

    Q_FOREACH(const Term &term, terms) {
        bool comparison_encountered = false;

        if (term.isComparisonTerm()) {
            ComparisonTerm comparison = term.toComparisonTerm();

            if (comparison.property().uri().scheme() == QLatin1String("date")) {
                handleDateTimeComparison(spec, comparison);

                spec_contains_interesting_data = true;
                comparison_encountered = true;

                start_position = qMin(start_position, term.position());
                end_position = qMax(end_position, term.position() + term.length());
            }
        }

        if (!comparison_encountered) {
            if (spec_contains_interesting_data) {
                // End a date-time spec and emit its xsd:DateTime value
                new_terms.append(buildDateTimeLiteral(spec));
                new_terms.last().setPosition(start_position, end_position - start_position);

                spec.reset();
                spec_contains_interesting_data = false;
                start_position = 1 << 30;
                end_position = 0;
            }

            new_terms.append(term);     // Preserve non-datetime terms
        }
    }

    if (spec_contains_interesting_data) {
        // Query ending with a date-time, don't forget to build it
        new_terms.append(buildDateTimeLiteral(spec));
        new_terms.last().setPosition(start_position, end_position - start_position);
    }

    terms.swap(new_terms);
}