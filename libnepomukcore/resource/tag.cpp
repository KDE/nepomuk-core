/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2007-2011 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "tag.h"

#include "tools.h"
#include "variant.h"
#include "resourcemanager.h"
#include "resource.h"

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Model>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>

using namespace Soprano::Vocabulary;

Nepomuk2::Tag::Tag()
    : Resource()
{
}


Nepomuk2::Tag::Tag( const Tag& res )
    : Resource( res )
{
}


Nepomuk2::Tag::Tag( const Nepomuk2::Resource& res )
    : Resource( res )
{
}


Nepomuk2::Tag::Tag( const QString& uri )
    : Resource( uri, Soprano::Vocabulary::NAO::Tag() )
{
}


Nepomuk2::Tag::Tag( const QUrl& uri )
    : Resource( uri, Soprano::Vocabulary::NAO::Tag() )
{
}


Nepomuk2::Tag::Tag( const QString& uri, const QUrl& type )
    : Resource( uri, type )
{
}


Nepomuk2::Tag::Tag( const QUrl& uri, const QUrl& type )
    : Resource( uri, type )
{
}


Nepomuk2::Tag::~Tag()
{
}


Nepomuk2::Tag& Nepomuk2::Tag::operator=( const Tag& res )
{
    Resource::operator=( res );
    return *this;
}


QList<Nepomuk2::Resource> Nepomuk2::Tag::tagOf() const
{
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QList<Soprano::Node> list = model->listStatements( Soprano::Node(), NAO::hasTag(), uri() ).iterateSubjects().allNodes();
    QList<Resource> resources;
    foreach(const Soprano::Node& node, list)
        resources << node.uri();
    return resources;
}

QList< Nepomuk2::Tag > Nepomuk2::Tag::allTags()
{
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QList<Soprano::Node> list = model->listStatements( Soprano::Node(), RDF::type(), NAO::Tag() ).iterateSubjects().allNodes();
    QList<Tag> tags;
    foreach(const Soprano::Node& node, list)
        tags << node.uri();
    return tags;
}
