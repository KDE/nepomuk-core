project(nepomuk-core)


# set the version to be used by NepomukCoreConfig.cmake and the lib version
# ===============================================================================================
set(CMAKE_NEPOMUK_CORE_VERSION_MAJOR 0)
set(CMAKE_NEPOMUK_CORE_VERSION_MINOR 1)
set(CMAKE_NEPOMUK_CORE_VERSION_RELEASE 0)
set(CMAKE_NEPOMUK_CORE_VERSION_STRING "${CMAKE_NEPOMUK_CORE_VERSION_MAJOR}.${CMAKE_NEPOMUK_CORE_VERSION_MINOR}.${CMAKE_NEPOMUK_CORE_VERSION_RELEASE}")

# CTest setup
# ===============================================================================================
include(CTest)
include(CTestConfig.cmake)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake COPYONLY)


# search required packages
# ===============================================================================================
set(KDE_MIN_VERSION "4.7.00")
find_package(KDE4 4.7.00 REQUIRED)
include(KDE4Defaults)

set(SDO_MIN_VERSION "0.10")
find_package(SharedDesktopOntologies)
set(SOPRANO_MIN_VERSION "2.9.0")
find_package(Soprano)

macro_log_feature(SOPRANO_FOUND "Soprano" "Soprano is the Qt-based RDF storage and parsing solution" "http://soprano.sourceforge.net" TRUE ${SOPRANO_MIN_VERSION} "Required for everything (storage and general data management)")
macro_log_feature(SOPRANO_PLUGIN_VIRTUOSOBACKEND_FOUND "Soprano Virtuoso Backend" "Virtuoso storage backend for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano Virtuoso backend is required to run the Nepomuk semantic desktop system.")
macro_log_feature(SOPRANO_PLUGIN_RAPTORPARSER_FOUND "Soprano Raptor Parser" "RDF parser plugin for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano raptor parser plugin is required to build the Nepomuk semantic desktop system.")
macro_log_feature(SOPRANO_PLUGIN_REDLANDBACKEND_FOUND "Soprano Redland Backend" "Redland storage backend for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano redland backend is required to build the Nepomuk semantic desktop system.")
macro_log_feature(SHAREDDESKTOPONTOLOGIES_FOUND "Shared desktop ontologies" "Desktop ontologies" "http://oscaf.sourceforge.net" TRUE ${SDO_MIN_VERSION} "Ontologies necessary for the Nepomuk semantic desktop.")

# Poppler
macro_optional_find_package(PopplerQt4)
macro_log_feature(POPPLER_QT4_FOUND "Poppler-Qt4" "A PDF rendering library" "http://poppler.freedesktop.org" FALSE "0.12.1" "Support for PDF files.")

macro_optional_find_package(Taglib)
macro_log_feature(TAGLIB_FOUND "Taglib" "Id3 tag reader" "taglib-website" FALSE "" "Support for music metadata")

macro_optional_find_package(Exiv2)
macro_log_feature(EXIV2_FOUND "Exiv2" "Image Tag reader" "http://www.exiv2.org" FALSE "0.21" "Support for image metadata")

macro_optional_find_package(FFmpeg)
macro_log_feature(FFMPEG_FOUND "FFmpeg" "Video Tag reader" "http://ffmpeg.org" FALSE "" "Support for video metadata")

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
if(KDE4_BUILD_TESTS)
  add_subdirectory(tests)
  add_subdirectory(autotests)
endif()

macro_display_feature_log()

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
