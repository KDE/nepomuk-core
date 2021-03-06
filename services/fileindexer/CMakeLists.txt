project(fileindexerservice)

include(SopranoAddOntology)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300106)

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  )

set(nepomukfileindexer_SRCS
  fileindexer.cpp
  indexscheduler.cpp
  indexingqueue.cpp
  basicindexingqueue.cpp
  fileindexingqueue.cpp
  fileindexingjob.cpp
  indexcleaner.cpp
  fileindexerconfig.cpp
  indexer/simpleindexer.cpp
  eventmonitor.cpp
  util.cpp
  )

soprano_add_ontology(nepomukfileindexer_SRCS ${nepomuk_ontologies_SOURCE_DIR}/kext.trig "KExt" "Nepomuk2::Vocabulary" "trig")

qt4_add_dbus_adaptor(nepomukfileindexer_SRCS ../../interfaces/org.kde.nepomuk.FileIndexer.xml fileindexer.h Nepomuk2::FileIndexer )

qt4_add_dbus_interface(nepomukfileindexer_SRCS ../../interfaces/org.kde.nepomuk.FileWatch.xml filewatchserviceinterface)

kde4_add_executable(nepomukfileindexer ${nepomukfileindexer_SRCS})

target_link_libraries(nepomukfileindexer
  nepomukcommon
  nepomukcore
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  ${KDE4_KIDLETIME_LIBS}
  ${SOPRANO_LIBRARIES}
)

install(
  FILES nepomukfileindexer.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})

install(
  FILES nepomukfileindexer.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/fileindexerservice)

install(
  TARGETS nepomukfileindexer
  DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(indexer)
add_subdirectory(test)
# -----------------------------
