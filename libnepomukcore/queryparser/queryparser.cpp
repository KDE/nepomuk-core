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
#include "comparisonterm.h"
#include "andterm.h"
#include "orterm.h"
#include "resourcetypeterm.h"
#include "negationterm.h"
#include "property.h"
#include "nfo.h"
#include "nmo.h"
#include "nmm.h"
#include "nie.h"
#include "nco.h"
#include "ncal.h"

#include <soprano/literalvalue.h>
#include <soprano/nao.h>
#include <soprano/rdfs.h>
#include <klocale.h>
#include <kcalendarsystem.h>
#include <klocalizedstring.h>

#include <QtCore/QList>

#define _P(x) QUrl(QLatin1String("property://" x "/"))
#define PROPERTY_AUTHOR     _P("author")
#define PROPERTY_TITLE      _P("title")
#define PROPERTY_SIZE       _P("size")
#define PROPERTY_NAME       _P("name")
#define PROPERTY_CREATED    _P("created")
#define PROPERTY_MODIFIED   _P("modified")

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
    {
        // Default property types (NOTE: Ensure that any property used in runPasses
        // is represented here)
        QHash<QUrl, QUrl> general;

        general.insert(PROPERTY_CREATED, Nepomuk2::Vocabulary::NIE::created());
        general.insert(PROPERTY_AUTHOR, Nepomuk2::Vocabulary::NFO::fileOwner());
        general.insert(PROPERTY_TITLE, Nepomuk2::Vocabulary::NIE::title());
        general.insert(PROPERTY_SIZE, Nepomuk2::Vocabulary::NIE::contentSize());
        general.insert(PROPERTY_NAME, Soprano::Vocabulary::RDFS::label());
        general.insert(PROPERTY_CREATED, Nepomuk2::Vocabulary::NIE::created());
        general.insert(PROPERTY_MODIFIED, Nepomuk2::Vocabulary::NIE::lastModified());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NIE::InformationElement(), general);

        // Files
        QHash<QUrl, QUrl> files(general);

        files.insert(PROPERTY_SIZE, Nepomuk2::Vocabulary::NFO::fileSize());
        files.insert(PROPERTY_NAME, Nepomuk2::Vocabulary::NFO::fileName());
        files.insert(PROPERTY_CREATED, Nepomuk2::Vocabulary::NFO::fileCreated());
        files.insert(PROPERTY_MODIFIED, Nepomuk2::Vocabulary::NFO::fileLastModified());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NFO::FileDataObject(), files);

        // Images, videos, documents use the same properties as plain files
        resourcetype_properties.insert(Nepomuk2::Vocabulary::NFO::Image(), files);
        resourcetype_properties.insert(Nepomuk2::Vocabulary::NFO::Video(), files);
        resourcetype_properties.insert(Nepomuk2::Vocabulary::NFO::Document(), files);

        // Music
        QHash<QUrl, QUrl> musics(files);

        musics.insert(PROPERTY_AUTHOR, Nepomuk2::Vocabulary::NMM::performer());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NFO::Audio(), musics);

        // Messages
        QHash<QUrl, QUrl> messages(general);

        messages.insert(PROPERTY_TITLE, Nepomuk2::Vocabulary::NMO::messageSubject());
        messages.insert(PROPERTY_CREATED, Nepomuk2::Vocabulary::NMO::receivedDate());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NMO::Message(), messages);

        // Contacts
        QHash<QUrl, QUrl> contacts(general);

        contacts.insert(PROPERTY_TITLE, Nepomuk2::Vocabulary::NCO::fullname());
        contacts.insert(PROPERTY_NAME, Nepomuk2::Vocabulary::NCO::fullname());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NCO::Contact(), contacts);

        // Events
        QHash<QUrl, QUrl> events(general);

        events.insert(PROPERTY_NAME, Nepomuk2::Vocabulary::NCAL::summary());
        events.insert(PROPERTY_TITLE, Nepomuk2::Vocabulary::NCAL::description());

        resourcetype_properties.insert(Nepomuk2::Vocabulary::NCAL::Event(), events);
    }

    QStringList split(const QString &query, bool is_user_query, QList<int> *positions = NULL);

    void runPasses(int cursor_position, QueryParser::ParserFlags flags);
    template<typename T>
    void runPass(const T &pass,
                 int cursor_position,
                 const QString &pattern,
                 const KLocalizedString &description = KLocalizedString(),
                 CompletionProposal::Type type = CompletionProposal::NoType);
    void foldDateTimes();
    void handleDateTimeComparison(DateTimeSpec &spec, const ComparisonTerm &term);

    AndTerm intervalComparison(const Nepomuk2::Types::Property &prop,
                               const LiteralTerm &min,
                               const LiteralTerm &max);
    AndTerm dateTimeComparison(const Nepomuk2::Types::Property &prop,
                               const LiteralTerm &term);
    Term tuneTerm(Term term);

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

    // Real properties depending the type of a resource
    QUrl resourcetype;
    QHash<QUrl, QHash<QUrl, QUrl> > resourcetype_properties;
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
        int length = part.length();

        if (position > 0 &&
            query.at(position - 1) == QLatin1Char('"')) {
            // Absorb the starting quote into the term's position
            --position;
            ++length;
        }
        if (position + length < query.length() &&
            query.at(position + length) == QLatin1Char('"')) {
            // Absorb the ending quote into the term's position
            ++length;
        }

        LiteralTerm term(part);
        term.setPosition(position, length);

        d->terms.append(term);
    }

    // Run the parsing passes
    d->runPasses(cursor_position, flags);

    // Fuse the terms into a big AND term and produce the query
    d->resourcetype = Nepomuk2::Vocabulary::NIE::InformationElement();

    int end_index;
    Term final_term = d->tuneTerm(fuseTerms(d->terms, 0, end_index));

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

QStringList QueryParser::allContacts() const
{
    return d->pass_properties.contacts().keys();
}

QStringList QueryParser::allEmailAddresses() const
{
    return d->pass_properties.emailAddresses().keys();
}

QStringList QueryParser::Private::split(const QString &query, bool is_user_query, QList<int> *positions)
{
    QStringList parts;
    QString part;
    int size = query.size();
    bool between_quotes = false;
    bool split_at_every_char =
        i18nc("Are words of your language separated by spaces (Y/N) ?", "Y") != QLatin1String("Y");

    for (int i=0; i<size; ++i) {
        QChar c = query.at(i);

        if (!between_quotes && (is_user_query || part != QLatin1String("%")) &&
            (split_at_every_char || c.isSpace() || (is_user_query && separators.contains(c)))) {
            // If there is a cluster of several spaces in the input, part may be empty
            if (part.size() > 0) {
                parts.append(part);
                part.clear();
            }

            // Add a separator, if any
            if (!c.isSpace()) {
                if (positions) {
                    positions->append(i);
                }

                part.append(c);
            }
        } else if (c == '"') {
            between_quotes = !between_quotes;
        } else {
            if (is_user_query && part.length() == 1 && separators.contains(part.at(0))) {
                // The part contains only a separator, split "-KMail" to "-", "KMail"
                parts.append(part);
                part.clear();
            }

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

void QueryParser::Private::runPasses(int cursor_position, QueryParser::ParserFlags flags)
{
    // Prepare literal values
    runPass(pass_splitunits, cursor_position, "%1");
    runPass(pass_numbers, cursor_position, "%1");
    runPass(pass_filesize, cursor_position, "%1 %2");
    runPass(pass_typehints, cursor_position, "%1");

    if (flags & DetectFilenamePattern) {
        runPass(pass_filenames, cursor_position, "%1");
    }

    // Date-time periods
    runPass(pass_periodnames, cursor_position, "%1");

    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Adding an offset to a period of time (%1=period, %2=offset)", "in %2 %1"));
    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::InvertedOffset);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Removing an offset from a period of time (%1=period, %2=offset)", "%2 %1 ago"));

    pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, 1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("In one day", "tomorrow"),
        ki18n("Tomorrow"));
    pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, -1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("One day ago", "yesterday"),
        ki18n("Yesterday"));
    pass_dateperiods.setKind(PassDatePeriods::Day, PassDatePeriods::Offset, 0);
    runPass(pass_dateperiods, cursor_position,
        i18nc("The current day", "today"),
        ki18n("Today"));

    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value, 1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("First period (first day, month, etc)", "first %1"),
        ki18n("First week, month, day, ..."));
    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value, -1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Last period (last day, month, etc)", "last %1 of"),
        ki18n("Last week, month, day, ..."));
    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Value);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Setting the value of a period, as in 'third week' (%1=period, %2=value)", "%2 %1"));

    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset, 1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Adding 1 to a period of time", "next %1"),
        ki18n("Next week, month, day, ..."));
    pass_dateperiods.setKind(PassDatePeriods::VariablePeriod, PassDatePeriods::Offset, -1);
    runPass(pass_dateperiods, cursor_position,
        i18nc("Removing 1 to a period of time", "last %1"),
        ki18n("Previous week, month, day, ..."));

    // Setting values of date-time periods (14:30, June 6, etc)
    pass_datevalues.setPm(true);
    runPass(pass_datevalues, cursor_position,
        i18nc("An hour (%5) and an optional minute (%6), PM", "at %5 : %6 pm;at %5 h pm;at %5 pm;%5 : %6 pm;%5 h pm;%5 pm"),
        ki18n("A time after midday"));
    pass_datevalues.setPm(false);
    runPass(pass_datevalues, cursor_position,
        i18nc("An hour (%5) and an optional minute (%6), AM", "at %5 : %6 am;at %5 h am;at %5 am;at %5;%5 : %6 am;%5 : %6 : %7;%5 : %6;%5 h am;%5 h;%5 am"),
        ki18n("A time"));

    runPass(pass_datevalues, cursor_position, i18nc(
        "A year (%1), month (%2), day (%3), day of week (%4), hour (%5), "
            "minute (%6), second (%7), in every combination supported by your language",
        "%3 of %2 %1;%3 st|nd|rd|th %2 %1;%3 st|nd|rd|th of %2 %1;"
        "%3 of %2;%3 st|nd|rd|th %2;%3 st|nd|rd|th of %2;%2 %3 st|nd|rd|th;%2 %3;%2 %1;"
        "%1 - %2 - %3;%1 - %2;%3 / %2 / %1;%3 / %2;"
        "in %2 %1; in %1;, %1"
    ));

    // Fold date-time properties into real DateTime values
    foldDateTimes();

    // Comparators
    pass_comparators.setComparator(ComparisonTerm::Contains);
    runPass(pass_comparators, cursor_position,
        i18nc("Equality", "contains|containing %1"),
        ki18n("Containing"));
    pass_comparators.setComparator(ComparisonTerm::Greater);
    runPass(pass_comparators, cursor_position,
        i18nc("Strictly greater", "greater|bigger|more than %1;at least %1;> %1"),
        ki18n("Greater than"));
    runPass(pass_comparators, cursor_position,
        i18nc("After in time", "after|since %1"),
        ki18n("After"), CompletionProposal::DateTime);
    pass_comparators.setComparator(ComparisonTerm::Smaller);
    runPass(pass_comparators, cursor_position,
        i18nc("Strictly smaller", "smaller|less|lesser than %1;at most %1;< %1"),
        ki18n("Smaller than"));
    runPass(pass_comparators, cursor_position,
        i18nc("Before in time", "before|until %1"),
        ki18n("Before"), CompletionProposal::DateTime);
    pass_comparators.setComparator(ComparisonTerm::Equal);
    runPass(pass_comparators, cursor_position,
        i18nc("Equality", "equal|equals|= %1;equal to %1"),
        ki18n("Equal to"));

    // Properties associated with any Nepomuk resource
    pass_properties.setProperty(Soprano::Vocabulary::NAO::numericRating(), PassProperties::Integer);
    runPass(pass_properties, cursor_position,
        i18nc("Numeric rating of a resource", "rated as %1;rated %1;score is %1;score|scored %1;having %1 stars|star"),
        ki18n("Rating (0 to 10)"), CompletionProposal::NoType);
    pass_properties.setProperty(Soprano::Vocabulary::NAO::description(), PassProperties::String);
    runPass(pass_properties, cursor_position,
        i18nc("Description of a resource", "described as %1;description|comment is %1;described|description|comment %1"),
        ki18n("Comment or description"), CompletionProposal::NoType);

    // Email-related properties
    pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::messageFrom(), PassProperties::EmailAddress);
    runPass(pass_properties, cursor_position,
        i18nc("Sender of an e-mail", "sent by %1;from %1;sender is %1;sender %1"),
        ki18n("Sender of an e-mail"), CompletionProposal::Email);
    pass_properties.setProperty(PROPERTY_TITLE, PassProperties::String);
    runPass(pass_properties, cursor_position,
        i18nc("Title of an e-mail or document", "title %1;titled %1"),
        ki18n("Title"));
    pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::messageRecipient(), PassProperties::EmailAddress);
    runPass(pass_properties, cursor_position,
        i18nc("Recipient of an e-mail", "sent to %1;to %1;recipient is %1;recipient %1"),
        ki18n("Recipient of an e-mail"), CompletionProposal::Email);
    pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::sentDate(), PassProperties::DateTime);
    runPass(pass_properties, cursor_position,
        i18nc("Sending date-time", "sent at|on %1;sent %1"),
        ki18n("Date of sending"), CompletionProposal::DateTime);
    pass_properties.setProperty(Nepomuk2::Vocabulary::NMO::receivedDate(), PassProperties::DateTime);
    runPass(pass_properties, cursor_position,
        i18nc("Receiving date-time", "received at|on %1;received %1;reception is %1"),
        ki18n("Date of reception"), CompletionProposal::DateTime);

    // File-related properties
    pass_properties.setProperty(PROPERTY_AUTHOR, PassProperties::Contact);
    runPass(pass_properties, cursor_position,
        i18nc("Author of a document", "written|created|composed by %1;author is %1;by %1"),
        ki18n("Author"), CompletionProposal::Contact);
    pass_properties.setProperty(PROPERTY_SIZE, PassProperties::IntegerOrDouble);
    runPass(pass_properties, cursor_position,
        i18nc("Size of a file", "size is %1;size %1;being %1 large;%1 large"),
        ki18n("Size"));
    pass_properties.setProperty(PROPERTY_NAME, PassProperties::String);
    runPass(pass_properties, cursor_position,
        i18nc("Name of a file or contact", "name is %1;name %1;named %1"),
        ki18n("Name"));
    pass_properties.setProperty(PROPERTY_CREATED, PassProperties::DateTime);
    runPass(pass_properties, cursor_position,
        i18nc("Date of creation", "created|dated at|on|in|of %1;created|dated %1;creation date|time|datetime is %1"),
        ki18n("Date of creation"), CompletionProposal::DateTime);
    pass_properties.setProperty(PROPERTY_MODIFIED, PassProperties::DateTime);
    runPass(pass_properties, cursor_position,
        i18nc("Date of last modification", "modified|edited at|on %1;modified|edited %1;modification|edition date|time|datetime is %1"),
        ki18n("Date of last modification"), CompletionProposal::DateTime);

    // Properties having a resource range (hasTag, messageFrom, etc)
    pass_properties.setProperty(Soprano::Vocabulary::NAO::hasTag(), PassProperties::Tag);
    runPass(pass_properties, cursor_position,
        i18nc("A document is associated with a tag", "tagged as %1;has tag %1;tag is %1;# %1"),
        ki18n("Tag name"), CompletionProposal::Tag);

    // Different kinds of properties that need subqueries
    pass_subqueries.setProperty(Nepomuk2::Vocabulary::NIE::relatedTo());
    runPass(pass_subqueries, cursor_position,
        i18nc("Related to a subquery", "related to %% ,"),
        ki18n("Match items related to others"));
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
 * Term tuning (setting the right properties of comparisons, etc)
 */
AndTerm QueryParser::Private::intervalComparison(const Nepomuk2::Types::Property &prop,
                                                 const LiteralTerm &min,
                                                 const LiteralTerm &max)
{
    int start_position = qMin(min.position(), max.position());
    int end_position = qMax(min.position() + min.length(), max.position() + max.length());

    ComparisonTerm greater(prop, min, ComparisonTerm::GreaterOrEqual);
    ComparisonTerm smaller(prop, max, ComparisonTerm::SmallerOrEqual);

    greater.setPosition(start_position, end_position - start_position);
    smaller.setPosition(greater);

    AndTerm total(greater, smaller);
    total.setPosition(greater);

    return total;
}

AndTerm QueryParser::Private::dateTimeComparison(const Nepomuk2::Types::Property &prop,
                                                 const LiteralTerm &term)
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
        end_date = cal->addDays(start_date, cal->daysInWeek(end_date));
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

Term QueryParser::Private::tuneTerm(Term term)
{
    // Recurse in And, Or and Not terms
    if (term.isNegationTerm()) {
        Term rs = NegationTerm::negateTerm(
            tuneTerm(term.toNegationTerm().subTerm())
        );

        rs.setPosition(term);
        return rs;
    } else if (term.isAndTerm() || term.isOrTerm()) {
        QList<Term> terms = (term.isAndTerm() ? term.toAndTerm().subTerms() : term.toOrTerm().subTerms());

        // Tune the sub-terms. Some of them may change this->resourcetype, so save
        // it and restore it after the tuning
        QUrl old_resourcetype = resourcetype;

        for (int i=0; i<terms.count(); ++i) {
            terms[i] = tuneTerm(terms.at(i));
        }

        resourcetype = old_resourcetype;

        // Create a shiny new And or Or term
        Term rs = (term.isAndTerm() ? (Term)AndTerm(terms) : (Term)OrTerm(terms));
        rs.setPosition(term);

        return rs;
    } else if (term.isComparisonTerm() &&
        term.toComparisonTerm().property().uri() == Nepomuk2::Vocabulary::NIE::relatedTo()) {
        // Related to is a nested comparison, tune its sub-term
        term.toComparisonTerm().setSubTerm(tuneTerm(term.toComparisonTerm().subTerm()));
    }

    // Resource type terms change the current resource type used for properties
    if (term.isResourceTypeTerm()) {
        resourcetype = term.toResourceTypeTerm().type().uri();
    }

    // Change literal terms to comparisons against a default property
    if (term.isLiteralTerm()) {
        Soprano::LiteralValue value = term.toLiteralTerm().value();

        if (value.isDateTime() || value.isInt() || value.isInt64()) {
            Term t = ComparisonTerm(
                value.isDateTime() ? PROPERTY_CREATED : PROPERTY_SIZE,
                term,
                ComparisonTerm::Equal
            );

            t.setPosition(term);
            term = t;
        }
    }

    // Handle comparisons
    if (term.isComparisonTerm()) {
        ComparisonTerm &comparison = term.toComparisonTerm();

        // Change comparisons against a default property to actual comparisons
        QUrl property_uri = comparison.property().uri();

        if (property_uri.scheme() == QLatin1String("property")) {
            term.toComparisonTerm().setProperty(
                resourcetype_properties.value(resourcetype).value(property_uri)
            );
        }

        // Change strict equalities to interval comparisons
        if (comparison.comparator() == ComparisonTerm::Equal &&
            comparison.subTerm().isLiteralTerm()) {
            Soprano::LiteralValue value = comparison.subTerm().toLiteralTerm().value();

            if (value.isDateTime()) {
                // The width of the interval is encoded into the millisecond part
                // of the date-time
                AndTerm datetime_comparison = dateTimeComparison(
                    comparison.property(),
                    comparison.subTerm().toLiteralTerm()
                );

                // Only comparison has information about the position of the whole
                // comparison, as the date-time comparison was built with only the
                // literal terms
                Nepomuk2::Query::Term min_term, max_term;

                min_term = datetime_comparison.subTerms().at(0);
                max_term = datetime_comparison.subTerms().at(1);

                datetime_comparison.setPosition(comparison);
                min_term.setPosition(comparison);
                max_term.setPosition(comparison);

                datetime_comparison.setSubTerms(QList<Nepomuk2::Query::Term>() << min_term << max_term);
                term = datetime_comparison;
            } else if (value.isInt() || value.isInt64()) {
                // Compare with the value +- 20%
                long long int v = value.toInt64();
                Nepomuk2::Query::LiteralTerm min(v * 80LL / 100LL);
                Nepomuk2::Query::LiteralTerm max(v * 120LL / 100LL);

                min.setPosition(term);
                max.setPosition(term);

                term = intervalComparison(
                    PROPERTY_SIZE,
                    min,
                    max
                );
            }
        }
    }

    // The term is now okay
    return term;
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

    // Week (absolute or relative, it is easy as the date is currently at the beginning
    // of a year or a month)
    if (week.flags == Field::Absolute) {
        date = calendar->addDays(date, (week.value - 1) * calendar->daysInWeek(date));
    } else if (week.flags == Field::Relative) {
        date = calendar->addDays(date, week.value * calendar->daysInWeek(date));
    }

    // Day of week
    int isoyear;
    int isoweek = calendar->week(date, KLocale::IsoWeekNumber, &isoyear);
    int isoday = calendar->dayOfWeek(date);

    calendar->setDateIsoWeek(
        date,
        isoyear,
        isoweek,
        fieldValue(dayofweek, last_defined_date >= PassDatePeriods::DayOfWeek, isoday, 1)
    );

    // Relative year, month, day of month
    if (year.flags == Field::Relative) {
        date = calendar->addYears(date, year.value);
    }
    if (month.flags == Field::Relative) {
        date = calendar->addMonths(date, month.value);
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
        bool end_of_cluster = true;

        if (term.isComparisonTerm()) {
            ComparisonTerm comparison = term.toComparisonTerm();

            if (comparison.property().uri().scheme() == QLatin1String("date")) {
                handleDateTimeComparison(spec, comparison);

                spec_contains_interesting_data = true;
                end_of_cluster = false;

                start_position = qMin(start_position, term.position());
                end_position = qMax(end_position, term.position() + term.length());
            }
        } else if (spec_contains_interesting_data &&
                   term.isLiteralTerm()) {
            QString value = termStringValue(term);

            if (value.length() == 2 || (value.length() == 1 && !separators.contains(value.at(0)))) {
                end_of_cluster = false;
            }
        }

        if (end_of_cluster) {
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