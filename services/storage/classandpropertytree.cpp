/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-2011 Sebastian Trueg <trueg@kde.org>

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

#include "classandpropertytree.h"
#include "simpleresource.h"
#include "simpleresourcegraph.h"

#include <QtCore/QSet>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QMutexLocker>

#include <Soprano/Node>
#include <Soprano/LiteralValue>
#include <Soprano/QueryResultIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/XMLSchema>

#include <KDebug>

using namespace Soprano;
using namespace Soprano::Vocabulary;

Nepomuk2::ClassAndPropertyTree* Nepomuk2::ClassAndPropertyTree::s_self = 0;

class Nepomuk2::ClassAndPropertyTree::ClassOrProperty
{
public:
    ClassOrProperty()
        : isProperty(false),
          maxCardinality(0),
          defining(0),
          hasRdfsLiteralRange(false) {
    }

    /// true if this is a property, for classes this is false
    bool isProperty;

    /// the uri of the class or property
    QUrl uri;

    /// the direct parents, ie. those for which a rdfs relations exists
    QSet<QUrl> directParents;

    /// includes all parents, even grand-parents and further up
    QSet<QUrl> allParents;

    /// the max cardinality if this is a property with a max cardinality set, 0 otherwise
    int maxCardinality;

    /// 0 - undecided, 1 - defining, -1 - non-defining
    int defining;

    /// only valid for properties
    QUrl domain;
    QUrl range;

    /// only valid for properties with literal range
    QVariant::Type literalType;
    bool hasRdfsLiteralRange;
};

Nepomuk2::ClassAndPropertyTree::ClassAndPropertyTree(QObject *parent)
    : QObject(parent),
      m_mutex(QMutex::Recursive)
{
    Q_ASSERT(s_self == 0);
    s_self = this;
}

Nepomuk2::ClassAndPropertyTree::~ClassAndPropertyTree()
{
    qDeleteAll(m_tree);
    s_self = 0;
}

bool Nepomuk2::ClassAndPropertyTree::isKnownClass(const QUrl &uri) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return !cop->isProperty;
    else
        return false;
}

QSet<QUrl> Nepomuk2::ClassAndPropertyTree::allParents(const QUrl &uri) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return cop->allParents;
    else
        return QSet<QUrl>();
}

QSet<QUrl> Nepomuk2::ClassAndPropertyTree::allParents(const QList< QUrl >& types) const
{
    QSet<QUrl> all;
    foreach(const QUrl& uri, types) {
        all.unite( allParents(uri) );
        all << uri;
    }

    return all;
}


bool Nepomuk2::ClassAndPropertyTree::isChildOf(const QUrl &type, const QUrl &superClass) const
{
    if( type == superClass )
        return true;

    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(type))
        return cop->allParents.contains(superClass);
    else
        return 0;
}

bool Nepomuk2::ClassAndPropertyTree::isChildOf(const QList< QUrl >& types, const QUrl& superClass) const
{
    if(superClass == RDFS::Resource()) {
        return true;
    }

    foreach( const QUrl & type, types ) {
        if( isChildOf( type, superClass ) )
            return true;
    }
    return false;
}

int Nepomuk2::ClassAndPropertyTree::maxCardinality(const QUrl &type) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(type))
        return cop->maxCardinality;
    else
        return 0;
}

QUrl Nepomuk2::ClassAndPropertyTree::propertyDomain(const QUrl &uri) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return cop->domain;
    else
        return QUrl();
}

QUrl Nepomuk2::ClassAndPropertyTree::propertyRange(const QUrl &uri) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return cop->range;
    else
        return QUrl();
}

bool Nepomuk2::ClassAndPropertyTree::hasLiteralRange(const QUrl &uri) const
{
    // TODO: this is a rather crappy check for literal range
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return (cop->range.toString().startsWith(XMLSchema::xsdNamespace().toString() ) ||
                cop->range == RDFS::Literal());
    else
        return false;
}

bool Nepomuk2::ClassAndPropertyTree::isDefiningProperty(const QUrl &uri) const
{
    QMutexLocker lock(&m_mutex);
    if(const ClassOrProperty* cop = findClassOrProperty(uri))
        return cop->defining == 1;
    else
        return true; // we default to true for unknown properties to ensure that we never perform invalid merges
}

Soprano::Node Nepomuk2::ClassAndPropertyTree::variantToNode(const QVariant &value, const QUrl &property) const
{
    QSet<Soprano::Node> nodes = variantListToNodeSet(QVariantList() << value, property);
    if(nodes.isEmpty())
        return Soprano::Node();
    else
        return *nodes.begin();
}

QSet<Soprano::Node> Nepomuk2::ClassAndPropertyTree::variantListToNodeSet(const QVariantList &vl, const QUrl &property) const
{
    clearError();

    QSet<Soprano::Node> nodes;
    nodes.reserve( vl.size() );

    QUrl range;
    QVariant::Type literalType;
    bool hasRdfsLiteralRange = false;

    // Temporary scope for the locker
    {
        QMutexLocker lock(&m_mutex);
        const ClassOrProperty* propertyNode = findClassOrProperty( property );
        if( !propertyNode ) {
            setError( QString::fromLatin1("Cannot set values for abstract property '%1'.")
                    .arg( Soprano::Node::resourceToN3( property ) ) );
            return QSet<Soprano::Node>();
        }

        range = propertyNode->range;
        literalType = propertyNode->literalType;
        hasRdfsLiteralRange = propertyNode->hasRdfsLiteralRange;
    }

    //
    // Special case: abstract properties - we do not allow setting them
    //
    if(range.isEmpty()) {
        setError(QString::fromLatin1("Cannot set values for abstract property '%1'.").arg(property.toString()), Soprano::Error::ErrorInvalidArgument);
        return QSet<Soprano::Node>();
    }

    //
    // Special case: rdfs:Literal
    //
    if( hasRdfsLiteralRange ) {
        Q_FOREACH(const QVariant& value, vl) {
            nodes.insert(Soprano::LiteralValue::createPlainLiteral(value.toString()));
        }
        return nodes;
    }

    //
    // Invalid Range - Probably a resource
    //
    if(literalType == QVariant::Invalid) {
        Q_FOREACH(const QVariant& value, vl) {
            // treat as a resource range for now
            if(value.type() == QVariant::Url) {
                QUrl url = value.toUrl();
                if( url.scheme().isEmpty() && !url.toString().startsWith("_:") )
                    url.setScheme("file");
                nodes.insert(url);
            }
            else if(value.type() == QVariant::String) {
                QString s = value.toString();
                if(!s.isEmpty()) {
                    // for convinience we support local file paths
                    if(s[0] == QDir::separator() && QFile::exists(s)) {
                        nodes.insert(QUrl::fromLocalFile(s));
                    }
                    else {
                        // treat it as a URI
                        nodes.insert(QUrl(s));
                    }
                }
                else {
                    // empty string
                    setError(QString::fromLatin1("Encountered an empty string where a resource URI was expected."), Soprano::Error::ErrorInvalidArgument);
                    return QSet<Soprano::Node>();
                }
            }
            else {
                // invalid type
                setError(QString::fromLatin1("Encountered '%1' where a resource URI was expected.").arg(value.toString()), Soprano::Error::ErrorInvalidArgument);
                return QSet<Soprano::Node>();
            }
        }

        return nodes;
    }

    //
    // The standard case
    //
    Q_FOREACH(const QVariant& value, vl) {
        //
        // Exiv data often contains floating point values encoded as a fraction
        //
        if((range == XMLSchema::xsdFloat() || range == XMLSchema::xsdDouble())
                && value.type() == QVariant::String) {
            int x = 0;
            int y = 0;
            if ( sscanf( value.toString().toLatin1().data(), "%d/%d", &x, &y ) == 2 && y != 0 ) {
                const double v = double( x )/double( y );
                nodes.insert(LiteralValue::fromVariant(v, range));
                continue;
            }
        }

        //
        // ID3 tags sometimes only contain the year of publication. We cover this
        // special case here with a very dumb heuristic
        //
        else if(range == XMLSchema::dateTime()
                && value.canConvert(QVariant::UInt)) {
            bool ok = false;
            const int t = value.toInt(&ok);
            if(ok && t > 0 && t <= 9999) {
                nodes.insert(LiteralValue(QDateTime(QDate(t, 1, 1), QTime(0, 0), Qt::UTC)));
                continue;
            }
        }

        Soprano::LiteralValue v = Soprano::LiteralValue::fromVariant(value, range);
        if(v.isValid()) {
            nodes.insert(v);
        }
        else {
            // failed literal conversion
            setError(QString::fromLatin1("Failed to convert '%1' to literal of type '%2'.").arg(value.toString(), range.toString()), Soprano::Error::ErrorInvalidArgument);
            return QSet<Soprano::Node>();
        }
    }

    return nodes;
}


namespace Soprano {
    namespace Vocabulary {
        namespace XMLSchema {
            QUrl xsdDuration() {
                return QUrl( Soprano::Vocabulary::XMLSchema::xsdNamespace().toString() +
                             QLatin1String("duration") );
            }
        }
    }
}

void Nepomuk2::ClassAndPropertyTree::rebuildTree(Soprano::Model* model)
{
    QMutexLocker lock(&m_mutex);

    qDeleteAll(m_tree);
    m_tree.clear();

    QString query
            = QString::fromLatin1("select distinct ?r ?p ?v ?mc ?c ?domain ?range ?ct ?pt "
                                  "where { "
                                  "{ ?r a ?ct . FILTER(?ct=rdfs:Class) . "
                                  "OPTIONAL { ?r rdfs:subClassOf ?p . ?p a rdfs:Class . } . } "
                                  "UNION "
                                  "{ ?r a ?pt . FILTER(?pt=rdf:Property) . "
                                  "OPTIONAL { ?r rdfs:subPropertyOf ?p . ?p a rdf:Property . } . } "
                                  "OPTIONAL { ?r %1 ?mc . } . "
                                  "OPTIONAL { ?r %2 ?c . } . "
                                  "OPTIONAL { ?r %3 ?domain . } . "
                                  "OPTIONAL { ?r %4 ?range . } . "
                                  "FILTER(?r!=%5) . "
                                  "}" )
            .arg(Soprano::Node::resourceToN3(NRL::maxCardinality()),
                 Soprano::Node::resourceToN3(NRL::cardinality()),
                 Soprano::Node::resourceToN3(RDFS::domain()),
                 Soprano::Node::resourceToN3(RDFS::range()),
                 Soprano::Node::resourceToN3(RDFS::Resource()));
//    kDebug() << query;
    Soprano::QueryResultIterator it
            = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        const QUrl r = it["r"].uri();
        const Soprano::Node p = it["p"];
        const Soprano::Node v = it["v"];
        int mc = it["mc"].literal().toInt();
        int c = it["c"].literal().toInt();
        const QUrl domain = it["domain"].uri();
        const QUrl range = it["range"].uri();

        ClassOrProperty* r_cop = 0;
        QHash<QUrl, ClassOrProperty*>::iterator copIt = m_tree.find(r);
        if(copIt != m_tree.end()) {
            r_cop = copIt.value();
        }
        else {
            r_cop = new ClassOrProperty;
            r_cop->uri = r;
            m_tree.insert( r, r_cop );
        }

        r_cop->isProperty = it["pt"].isValid();

        if(mc > 0 || c > 0) {
            r_cop->maxCardinality = qMax(mc, c);
        }

        if(!domain.isEmpty()) {
            r_cop->domain = domain;
        }

        if(!range.isEmpty()) {
            r_cop->range = range;

            // Special handling for xsd:duration - Soprano doesn't handle it natively
            if( range == XMLSchema::xsdDuration() )
                r_cop->literalType = QVariant::UInt;
            else if( range == RDFS::Literal() ) {
                r_cop->literalType = QVariant::UserType;
                r_cop->hasRdfsLiteralRange = true;
            }
            else
                r_cop->literalType = Soprano::LiteralValue::typeFromDataTypeUri( range );
        }
        else {
            // no range -> resource range
            r_cop->defining = -1;
        }

        if ( p.isResource() &&
                p.uri() != r &&
                p.uri() != RDFS::Resource() ) {
            ClassOrProperty* p_cop = 0;
            if ( !m_tree.contains( p.uri() ) ) {
                p_cop = new ClassOrProperty;
                p_cop->uri = p.uri();
                m_tree.insert( p.uri(), p_cop );
            }
            r_cop->directParents.insert(p.uri());
        }
    }

    // although nao:identifier is actually an abstract property Nepomuk has been using
    // it for very long to store string identifiers (instead of nao:personalIdentifier).
    // Thus, we force its range to xsd:string for correct conversion in variantListToNodeSet()
    if(m_tree.contains(NAO::identifier())) {
        ClassOrProperty* cop = m_tree[NAO::identifier()];
        cop->range = XMLSchema::string();
        cop->literalType = QVariant::String;
    }

    // add rdfs:Resource as parent for all top-level classes
    ClassOrProperty* rdfsResourceNode = 0;
    QHash<QUrl, ClassOrProperty*>::iterator rdfsResourceIt = m_tree.find(RDFS::Resource());
    if( rdfsResourceIt == m_tree.end() ) {
        rdfsResourceNode = new ClassOrProperty;
        rdfsResourceNode->uri = RDFS::Resource();
        m_tree.insert( RDFS::Resource(), rdfsResourceNode );
    }
    else {
        rdfsResourceNode = rdfsResourceIt.value();
    }
    for ( QHash<QUrl, ClassOrProperty*>::iterator it = m_tree.begin();
          it != m_tree.end(); ++it ) {
        if( it.value() != rdfsResourceNode && it.value()->directParents.isEmpty() ) {
            it.value()->directParents.insert( RDFS::Resource() );
        }
    }

    // complete the allParents lists
    for ( QHash<QUrl, ClassOrProperty*>::iterator it = m_tree.begin();
          it != m_tree.end(); ++it ) {
        QSet<QUrl> visitedNodes;
        getAllParents( it.value(), visitedNodes );
    }

    // update all defining and non-defining properties
    // by default all properties with a literal range are defining
    // and all properties with a resource range are non-defining
    query = QString::fromLatin1("select ?p ?t where { "
                                "?p a rdf:Property . "
                                "?p a ?t . FILTER(?t!=rdf:Property) . }");
    it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        const QUrl p = it["p"].uri();
        const QUrl t = it["t"].uri();

        if(t == NRL::DefiningProperty()) {
            m_tree[p]->defining = 1;
        }
        else if(t == NRL::NonDefiningProperty()) {
            m_tree[p]->defining = -1;
        }
    }

    // rdf:type is defining by default
    if(m_tree.contains(RDF::type()))
        m_tree[RDF::type()]->defining = 1;

    // nao:hasSubResource is defining by default
    if(m_tree.contains(NAO::hasSubResource()))
        m_tree[NAO::hasSubResource()]->defining = 1;

    for ( QHash<QUrl, ClassOrProperty*>::iterator it = m_tree.begin();
          it != m_tree.end(); ++it ) {
        if(it.value()->isProperty) {
            QSet<QUrl> visitedNodes;
            updateDefining( it.value(), visitedNodes );
        }
    }
}

const Nepomuk2::ClassAndPropertyTree::ClassOrProperty * Nepomuk2::ClassAndPropertyTree::findClassOrProperty(const QUrl &uri) const
{
    QHash<QUrl, ClassOrProperty*>::const_iterator it = m_tree.constFind(uri);
    if(it == m_tree.constEnd())
        return 0;
    else
        return it.value();
}

bool Nepomuk2::ClassAndPropertyTree::contains(const QUrl& uri) const
{
    return m_tree.contains(uri);
}


/**
 * Set the value of defining.
 * An defining property has at least one defining direct parent property.
 */
int Nepomuk2::ClassAndPropertyTree::updateDefining( ClassOrProperty* cop, QSet<QUrl>& definingNodes )
{
    if ( cop->defining != 0 ) {
        return cop->defining;
    }
    else {
        for ( QSet<QUrl>::iterator it = cop->directParents.begin();
             it != cop->directParents.end(); ++it ) {
            // avoid endless loops
            if( definingNodes.contains(*it) )
                continue;
            definingNodes.insert(*it);
            if ( updateDefining( m_tree[*it], definingNodes ) == 1 ) {
                cop->defining = 1;
                break;
            }
        }
        if ( cop->defining == 0 ) {
            // properties with a literal range default to defining
            cop->defining = hasLiteralRange(cop->uri) ? 1 : -1;
        }
        //kDebug() << "Setting defining of" << cop->uri.toString() << ( cop->defining == 1 );
        return cop->defining;
    }
}

QSet<QUrl> Nepomuk2::ClassAndPropertyTree::getAllParents(ClassOrProperty* cop, QSet<QUrl>& visitedNodes)
{
    if(cop->allParents.isEmpty()) {
        for ( QSet<QUrl>::iterator it = cop->directParents.begin();
             it != cop->directParents.end(); ++it ) {
            // avoid endless loops
            if( visitedNodes.contains(*it) )
                continue;
            visitedNodes.insert( *it );
            cop->allParents += getAllParents(m_tree[*it], visitedNodes);
        }
        cop->allParents += cop->directParents;

        // some cleanup to fix inheritance loops
        cop->allParents << RDFS::Resource();
        cop->allParents.remove(cop->uri);
    }
    return cop->allParents;
}


namespace {
    Soprano::Node convertIfBlankNode( const Soprano::Node & n ) {
        if( n.isResource() ) {
            const QString uriString = n.uri().toString();
            if( uriString.startsWith("_:") ) {
                return Soprano::Node( uriString.mid(2) ); // "_:" take 2 characters
            }
        }
        return n;
    }
}

Nepomuk2::ClassAndPropertyTree * Nepomuk2::ClassAndPropertyTree::self()
{
    return s_self;
}

#include "classandpropertytree.moc"
