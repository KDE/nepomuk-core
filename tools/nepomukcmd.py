#!/usr/bin/env python
# -*- coding: utf-8 -*-

## This file is part of the Nepomuk KDE project.
## Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
##
## This library is free software; you can redistribute it and/or
## modify it under the terms of the GNU Lesser General Public
## License as published by the Free Software Foundation; either
## version 2.1 of the License, or (at your option) version 3, or any
## later version accepted by the membership of KDE e.V. (or its
## successor approved by the membership of KDE e.V.), which shall
## act as a proxy defined in Section 6 of version 3 of the license.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this library.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import sys
import os, errno
from PyKDE4.soprano import Soprano
from PyKDE4.nepomuk import Nepomuk
from PyKDE4.kdecore import KUrl
from PyQt4 import QtCore

def extractExistingResourcesFromVariant(v):
    "Extracts all existing resources from v and returns a set."
    rl = set()
    if v.isList():
        for vv in v.toVariantList():
            rl = rl | extractExistingResourcesFromVariant(vv)
    elif v.isResource():
        res = v.toResource()
        if res.exists():
            rl.add(res)
    return rl
        

def getRelatedResources(resources):
    "Retrieve all related resources and return them in a set."
    relatedResources = set()
    for res in resources:
        for x in [extractExistingResourcesFromVariant(v) for v in res.properties().values()]:
            relatedResources = relatedResources | x
    return relatedResources
        
        
def getAllRelatedResources(resources, depth):
    "Retrieve all related resources up to a certain depth."
    if depth > 0:
        related = getRelatedResources(resources)
        related = related | getAllRelatedResources(related, depth-1)
        return related
    else:
        return set()


def variantToStringList(v):
    "Convert a Nepomuk::Variant into a list of N3 strings"
    sl = []
    if v.isList():
        for vv in v.toVariantList():
            sl.extend(variantToStringList(vv))
    elif v.isResource():
        sl.append(Soprano.Node.resourceToN3(v.toUrl()))
    else:
        sl.append(Soprano.Node.literalToN3(Soprano.LiteralValue(v.variant())))
        # todo: abbreviate xsd ns
    return sl


def printResource(res):
    "Prints a single resource"
    print "Resource <%s>" % res.resourceUri().toString()
    properties = {}
    propLen = 1
    # create a new dict with all prop labels and the values
    # this is mainly for getting the max len of the props
    for prop, value in res.properties().items():
        label = prop.toString()
        # todo: abbreviate prop ns
        propLen = max(propLen, len(label))
        properties[label] = variantToStringList(value)

    # actually print the values
    for prop, values in properties.items():
        print '    %s:%s%s' % (prop, ' '.rjust(propLen-prop.length()+1), values[0])
        for prop in values[1:]:
            print '    %s%s' % (' '.rjust(propLen+2), prop)

    
def dumpRes(args):
    # build initial resource set
    allResources = set()
    for uri in args.resources:
        res = Nepomuk.Resource(KUrl(uri))
        if not res.exists():
            print "Resource <%s> does not exist" % uri
        else:
            allResources.add(res)

    # get all related resources
    allResources = allResources | getAllRelatedResources(allResources, args.depth)

    # finally dump all the resources
    for res in allResources:
        printResource(res)


def main():
    optparser = argparse.ArgumentParser(description="")
    cmdParsers = optparser.add_subparsers(title='commands', description='The following commands are supported.')

    # command dump
    dumpParser = cmdParsers.add_parser('dump')
    dumpParser.add_argument('--depth', type=int, default=0, help='The graph-traversal depth. A depth higher than 1 will result in related resources to be dumped, too.')
    dumpParser.add_argument("resources", type=str, nargs='+', metavar="resource", help="The resources to dump.")
    dumpParser.set_defaults(func=dumpRes)
    
    args = optparser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
