project(fileindexerservice)

include(SopranoAddOntology)

# Check if strigiconfig.h has version functions
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${STRIGI_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${STRIGI_STREAMS_LIBRARY})
CHECK_CXX_SOURCE_COMPILES(
  "#include <strigi/strigiconfig.h>\nint main(int ac, char** av) { Strigi::versionString(); }"
  STRIGI_HAS_VERSION_HEADER
)
if(${STRIGI_HAS_VERSION_HEADER})
  add_definitions(-DSTRIGI_HAS_VERSION_HEADER)
endif(${STRIGI_HAS_VERSION_HEADER})

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300106)

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  )

set(nepomukfileindexer_SRCS
  fileindexer.cpp
  indexscheduler.cpp
  indexcleaner.cpp
  fileindexerconfig.cpp
  eventmonitor.cpp
  util.cpp
  nepomukindexer.cpp
  )

qt4_add_dbus_adaptor(nepomukfileindexer_SRCS ../../interfaces/org.kde.nepomuk.FileIndexer.xml fileindexer.h Nepomuk2::FileIndexer )

qt4_add_dbus_interface(nepomukfileindexer_SRCS ../../interfaces/org.kde.nepomuk.FileWatch.xml filewatchserviceinterface)

kde4_add_plugin(nepomukfileindexer ${nepomukfileindexer_SRCS})

set(fileindexer_LIBS
  nepomukcommon
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  ${KDE4_KIDLETIME_LIBS}
  nepomukcore
  ${SOPRANO_LIBRARIES}
)

if(${STRIGI_HAS_VERSION_HEADER})
    set(fileindexer_LIBS ${fileindexer_LIBS} ${STRIGI_STREAMS_LIBRARY} )
endif(${STRIGI_HAS_VERSION_HEADER})

target_link_libraries(nepomukfileindexer
  ${fileindexer_LIBS}
)

install(
  FILES nepomukfileindexer.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})

install(
  FILES nepomukfileindexer.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/fileindexerservice)

install(
  TARGETS nepomukfileindexer
  DESTINATION ${PLUGIN_INSTALL_DIR})

add_subdirectory(indexer)
add_subdirectory(test)
# -----------------------------
