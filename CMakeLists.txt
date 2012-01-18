project(nepomuk-core)


# set the version to be used by NepomukExtrasConfig.cmake and the lib version
# ===============================================================================================
set(CMAKE_NEPOMUK_VERSION_MAJOR 0)
set(CMAKE_NEPOMUK_VERSION_MINOR 1)
set(CMAKE_NEPOMUK_VERSION_RELEASE 0)
set(CMAKE_NEPOMUK_VERSION_STRING "${CMAKE_NEPOMUK_VERSION_MAJOR}.${CMAKE_NEPOMUK_VERSION_MINOR}.${CMAKE_NEPOMUK_VERSION_RELEASE}")


# search required packages
# ===============================================================================================
set(KDE_MIN_VERSION "4.7.00")
find_package(KDE4 4.7.00 REQUIRED)
include(KDE4Defaults)
find_package(KDE4Workspace)
include(MacroLibrary)

set(SDO_MIN_VERSION "0.7")
find_package(SharedDesktopOntologies)
set(SOPRANO_MIN_VERSION "2.7.53")
find_package(Soprano)
set(STRIGI_MIN_VERSION "0.7.3")
find_package(Strigi)

macro_log_feature(SOPRANO_FOUND "Soprano" "Soprano is the Qt-based RDF storage and parsing solution" "http://soprano.sourceforge.net" TRUE ${SOPRANO_MIN_VERSION} "Required for everything (storage and general data management)")
macro_log_feature(SOPRANO_PLUGIN_VIRTUOSOBACKEND_FOUND "Soprano Virtuoso Backend" "Virtuoso storage backend for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano Virtuoso backend is required to run the Nepomuk semantic desktop system.")
macro_log_feature(SOPRANO_PLUGIN_RAPTORPARSER_FOUND "Soprano Raptor Parser" "RDF parser plugin for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano raptor parser plugin is required to build the Nepomuk semantic desktop system.")
macro_log_feature(SOPRANO_PLUGIN_REDLANDBACKEND_FOUND "Soprano Redland Backend" "Redland storage backend for Soprano" "http://soprano.sourceforge.net" TRUE "" "The Soprano redland backend is required to build the Nepomuk semantic desktop system.")
macro_log_feature(SHAREDDESKTOPONTOLOGIES_FOUND "Shared desktop ontologies" "Desktop ontologies" "http://oscaf.sourceforge.net" TRUE ${SDO_MIN_VERSION} "Ontologies necessary for the Nepomuk semantic desktop.")
macro_log_feature(STRIGI_FOUND "Strigi" "Strigi is a fast and light-weight file indexing solution" "http://strigi.sourceforge.net" TRUE ${STRIGI_MIN_VERSION} "Required for file indexing (only libstreamanalyzer is required actually)")


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
  ${STRIGI_INCLUDE_DIR}
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
add_subdirectory(interfaces)
add_subdirectory(removed-services)

macro_display_feature_log()
