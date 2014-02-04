project(nepomuk-core)
# set_package_properties appeared in cmake 2.8.6
# TODO: Remove when kdelibs >= 4.10 will be required
cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

# CTest setup
# ===============================================================================================
include(CTest)
include(CTestConfig.cmake)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake COPYONLY)

# search required packages
# ===============================================================================================
find_package(KDE4 4.9.4 REQUIRED)
include(KDE4Defaults)

# TODO: Remove this block and the cmake directory when kdelibs >= 4.10 will be required
# if(KDE_VERSION VERSION_LESS 4.10.00)
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})
# endif()

find_package(SharedDesktopOntologies 0.10.51)
set_package_properties(SharedDesktopOntologies PROPERTIES DESCRIPTION "Shared Desktop Ontologies" URL "http://oscaf.sourceforge.net" TYPE REQUIRED PURPOSE "Ontologies necessary for the Nepomuk semantic desktop")

find_package(Soprano 2.9.3 COMPONENTS PLUGIN_RAPTORPARSER PLUGIN_REDLANDBACKEND PLUGIN_VIRTUOSOBACKEND)
set_package_properties(Soprano PROPERTIES DESCRIPTION "Qt-based RDF storage and parsing solution" URL "http://soprano.sourceforge.net" TYPE REQUIRED PURPOSE "Required for everything (storage and general data management)")

# Poppler
find_package(PopplerQt4 0.12.1)
set_package_properties(PopplerQt4 PROPERTIES DESCRIPTION "A PDF rendering library" URL "http://poppler.freedesktop.org" TYPE OPTIONAL PURPOSE "Support for PDF files")

find_package(Taglib)
set_package_properties(Taglib PROPERTIES DESCRIPTION "Id3 tag reader" URL "http://developer.kde.org/~wheeler/taglib.html" TYPE OPTIONAL PURPOSE "Support for music metadata")

find_package(Exiv2 0.21)
set_package_properties(Exiv2 PROPERTIES DESCRIPTION "Image Tag reader" URL "http://www.exiv2.org" TYPE OPTIONAL PURPOSE "Support for image metadata")

find_package(FFmpeg 1.0)
set_package_properties(FFmpeg PROPERTIES DESCRIPTION "Video Tag reader" URL "http://ffmpeg.org" TYPE OPTIONAL PURPOSE "Support for video metadata")

find_package(EPub)
set_package_properties(EPub PROPERTIES DESCRIPTION "Ebook epub reader" URL "http://sourceforge.net/projects/ebook-tools" TYPE OPTIONAL PURPOSE "Support for epub metadata")

find_package(QMobipocket)
set_package_properties(QMobipocket PROPERTIES DESCRIPTION "Mobipocket epub reader" URL "https://projects.kde.org/projects/kde/kdegraphics/kdegraphics-mobipocket" TYPE OPTIONAL PURPOSE "Support for mobi metadata")

find_package(KFileMetaData)
set_package_properties(KFileMetaData PROPERTIES DESCRIPTION "Library for extracting file metadata" URL "http://projects.kde.org/kfilemetadata" TYPE OPTIONAL)

find_package(Baloo)
set_package_properties(Baloo PROPERTIES DESCRIPTION "Baloo metadata search" URL "http://kde.org" TYPE OPTIONAL PURPOSE "Support for Baloo Migrator")

# set the version to be used by NepomukCoreConfig.cmake and the lib version
# ===============================================================================================
set(CMAKE_NEPOMUK_CORE_VERSION_MAJOR ${KDE_VERSION_MAJOR})
set(CMAKE_NEPOMUK_CORE_VERSION_MINOR ${KDE_VERSION_MINOR})
set(CMAKE_NEPOMUK_CORE_VERSION_RELEASE ${KDE_VERSION_RELEASE})
set(CMAKE_NEPOMUK_CORE_VERSION_STRING "${CMAKE_NEPOMUK_CORE_VERSION_MAJOR}.${CMAKE_NEPOMUK_CORE_VERSION_MINOR}.${CMAKE_NEPOMUK_CORE_VERSION_RELEASE}")

# Some definitions
# ===============================================================================================
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

# Add all the parts that make up nepomuk-core
# ===============================================================================================
include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/resource
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/service
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/query
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/types
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/datamanagement
  ${nepomuk-core_SOURCE_DIR}/libnepomukcore/misc
  ${nepomuk-core_SOURCE_DIR}/common
  ${nepomuk-core_BINARY_DIR}/common
  ${nepomuk-core_BINARY_DIR}/libnepomukcore
)

add_subdirectory(libnepomukcore)
add_subdirectory(rcgen)
add_subdirectory(ontologies)
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(services)
add_subdirectory(servicestub)
add_subdirectory(cleaner)
add_subdirectory(interfaces)
add_subdirectory(removed-services)
add_subdirectory(includes)
add_subdirectory(tools)

if(Baloo_FOUND AND KFileMetaData_FOUND)
  add_subdirectory(baloo)
endif()

if(KDE4_BUILD_TESTS)
  add_subdirectory(tests)
  add_subdirectory(autotests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

# generate NepomukCoreConfig.cmake for easy utilisation of the package by other cmake build systems
# ===============================================================================================

macro(MAKE_INSTALL_PATH_ABSOLUTE out in)
   if(IS_ABSOLUTE "${in}")
      set(${out} "${in}")
   else()
      set(${out} "\${CMAKE_INSTALL_PREFIX}/${in}")
   endif()
endmacro()

make_install_path_absolute(NEPOMUK_CORE_INCLUDE_DIR ${INCLUDE_INSTALL_DIR})
make_install_path_absolute(NEPOMUK_CORE_LIB_DIR ${LIB_INSTALL_DIR})
make_install_path_absolute(NEPOMUK_CORE_DBUS_INTERFACES_DIR ${DBUS_INTERFACES_INSTALL_DIR})

configure_file(NepomukCoreConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/NepomukCoreConfig.cmake @ONLY)

# Generate NepomukCoreConfigVersion.cmake
macro_write_basic_cmake_version_file(${nepomuk-core_BINARY_DIR}/NepomukCoreConfigVersion.cmake
                                     ${CMAKE_NEPOMUK_CORE_VERSION_MAJOR}
                                     ${CMAKE_NEPOMUK_CORE_VERSION_MINOR}
                                     ${CMAKE_NEPOMUK_CORE_VERSION_RELEASE}
                                     )

install(FILES
  Nepomuk2AddOntologyClasses.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/NepomukCore)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/NepomukCoreConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/NepomukCoreConfigVersion.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/NepomukCore)

# install the file with the exported targets
install(EXPORT NepomukCoreLibraryTargets
        DESTINATION ${LIB_INSTALL_DIR}/cmake/NepomukCore
        FILE NepomukCoreTargetsWithPrefix.cmake
        )
