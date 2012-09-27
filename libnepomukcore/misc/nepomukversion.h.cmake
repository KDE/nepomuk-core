/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011-2012 Sebastian Trueg <trueg@kde.org>

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

#ifndef _NEPOMUK2_CORE_VERSION_H_
#define _NEPOMUK2_CORE_VERSION_H_

#include "nepomuk_export.h"

/// @brief Nepomuk version as string at compile time.
#define NEPOMUK_VERSION_STRING "${CMAKE_NEPOMUK_CORE_VERSION_STRING}"

/// @brief The major Nepomuk version number at compile time
#define NEPOMUK_VERSION_MAJOR ${CMAKE_NEPOMUK_CORE_VERSION_MAJOR}

/// @brief The minor Nepomuk version number at compile time
#define NEPOMUK_VERSION_MINOR ${CMAKE_NEPOMUK_CORE_VERSION_MINOR}

/// @brief The Nepomuk release version number at compile time
#define NEPOMUK_VERSION_RELEASE ${CMAKE_NEPOMUK_CORE_VERSION_RELEASE}

/**
 * \brief Create a unique number from the major, minor and release number of a %NEPOMUK version
 *
 * This function can be used for preprocessing. For version information at runtime
 * use the version methods in the Nepomuk namespace.
 */
#define NEPOMUK_MAKE_VERSION( a,b,c ) (((a) << 16) | ((b) << 8) | (c))

/**
 * \brief %Nepomuk Version as a unique number at compile time
 *
 * This macro calculates the %Nepomuk version into a number. It is mainly used
 * through NEPOMUK_IS_VERSION in preprocessing. For version information at runtime
 * use the version methods in the Nepomuk namespace.
 */
#define NEPOMUK_VERSION \
    NEPOMUK_MAKE_VERSION(NEPOMUK_VERSION_MAJOR,NEPOMUK_VERSION_MINOR,NEPOMUK_VERSION_RELEASE)

/**
 * \brief Check if the %Nepomuk version matches a certain version or is higher
 *
 * This macro is typically used to compile conditionally a part of code:
 * \code
 * #if NEPOMUK_IS_VERSION(2,1)
 * // Code for Nepomuk 2.1
 * #else
 * // Code for Nepomuk 2.0
 * #endif
 * \endcode
 *
 * For version information at runtime
 * use the version methods in the Nepomuk namespace.
 */
#define NEPOMUK_IS_VERSION(a,b,c) ( NEPOMUK_VERSION >= NEPOMUK_MAKE_VERSION(a,b,c) )


namespace Nepomuk2 {
    /**
     * @brief Returns the major number of %Nepomuk's version, e.g.
     * 1 for %Nepomuk 1.0.2.
     * @return the major version number at runtime.
     */
    NEPOMUK_EXPORT unsigned int versionMajor();

    /**
     * @brief Returns the minor number of %Nepomuk's version, e.g.
     * 0 for %Nepomuk 1.0.2.
     * @return the minor version number at runtime.
     */
    NEPOMUK_EXPORT unsigned int versionMinor();

    /**
     * @brief Returns the release of %Nepomuk's version, e.g.
     * 2 for %Nepomuk 1.0.2.
     * @return the release number at runtime.
     */
    NEPOMUK_EXPORT unsigned int versionRelease();

    /**
     * @brief Returns the %Nepomuk version as string, e.g. "1.0.2".
     *
     * On contrary to the macro NEPOMUK_CORE_VERSION_STRING this function returns
     * the version number of %Nepomuk at runtime.
     * @return the %Nepomuk version. You can keep the string forever
     */
    NEPOMUK_EXPORT const char* versionString();
}

#endif
