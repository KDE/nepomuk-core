/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2008 Sebastian Trueg <trueg@kde.org>
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

#ifndef _NEPOMUK2_TOOLS_H_
#define _NEPOMUK2_TOOLS_H_

#include <QtCore/QString>
#include <QtCore/QList>

#include "variant.h"
#include "nepomuk_export.h"

#include <Soprano/Node>

namespace Nepomuk2 {

    /**
     * Used internally by Resource.
     * Convert a list of resources to a list of Ts where T needs to be a subclass
     * of Resource.
     *
     * \return A list containing all resources in \p l represented as a T.
     */
    template<typename T> QList<T> convertResourceList( const QList<Resource>& l ) {
        QList<T> rl;
        for( QList<Resource>::const_iterator it = l.constBegin();
             it != l.constEnd(); ++it )
            rl.append( T( *it ) );
        return rl;
    }

    /**
     * Used internally by Resource.
     * Convert a list of Ts to a list of Resources where T needs to be a subclass
     * of Resource.
     *
     * \return A list containing all resources in \p l.
     */
    template<typename T> QList<Resource> convertResourceList( const QList<T>& l ) {
        QList<Resource> rl;
        Q_FOREACH( const T& r, l )
            rl.append( Resource( r ) );
        return rl;
    }
}

#endif
