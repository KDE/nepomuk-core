project(nepomukfilewatch)

include(SopranoAddOntology)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300101)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../fileindexer
  )

set(SRCS
  nepomukfilewatch.cpp
  metadatamover.cpp
  updaterequest.cpp
  ../fileindexer/fileindexerconfig.cpp
  removabledeviceindexnotification.cpp
  removablemediadatamigrator.cpp
  activefilequeue.cpp
  )

qt4_add_dbus_interface(SRCS ../../interfaces/org.kde.nepomuk.FileIndexer.xml fileindexerinterface)
qt4_add_dbus_interface(SRCS ../../interfaces/org.kde.nepomuk.FileWatch.xml filewatchinterface)
qt4_add_dbus_adaptor(SRCS ../../interfaces/org.kde.nepomuk.FileWatch.xml nepomukfilewatch.h Nepomuk2::FileWatch )

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(SRCS
    ${SRCS}
    kinotify.cpp
)
add_definitions(-DBUILD_KINOTIFY)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

kde4_add_executable(nepomukfilewatch ${SRCS})

target_link_libraries(nepomukfilewatch
  nepomukcommon
  nepomukcore
  ${SOPRANO_CLIENT_LIBRARIES}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${KDE4_SOLID_LIBS}
  )

install(
  FILES nepomukfilewatch.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})
install(
  TARGETS nepomukfilewatch
  DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})
install(
  FILES nepomukfilewatch.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/nepomukfilewatch)

add_subdirectory(test)
