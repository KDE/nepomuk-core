set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${libnepomukcore_BINARY_DIR}
  )

set( DMTEST
  ../classandpropertytree.cpp
  ../datamanagementmodel.cpp
  ../resourcewatchermanager.cpp
  ../resourcewatcherconnection.cpp
  ../datamanagementadaptor.cpp
  ../datamanagementcommand.cpp
  ../resourcemerger.cpp
  ../resourceidentifier.cpp
  ../virtuosoinferencemodel.cpp
  ../syncresource.cpp
  ../syncresourceidentifier.cpp
  ../typecache.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/dbustypes.cpp
  qtest_dms.cpp
)

qt4_add_dbus_adaptor(DMTEST
  ../../../interfaces/org.kde.nepomuk.ResourceWatcherConnection.xml
  resourcewatcherconnection.h
  Nepomuk2::ResourceWatcherConnection)


kde4_add_library( datamanagementtestlib STATIC ${DMTEST} )

target_link_libraries( datamanagementtestlib
  ${SOPRANO_LIBRARIES}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
)

kde4_add_unit_test(classandpropertytreetest
  classandpropertytreetest.cpp
)
target_link_libraries(classandpropertytreetest
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  datamanagementtestlib
)

kde4_add_unit_test(datamanagementmodeltest
  datamanagementmodeltest.cpp
)

target_link_libraries(datamanagementmodeltest
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
  datamanagementtestlib
)

kde4_add_unit_test(datamanagementmodelbenchmark
  datamanagementmodelbenchmark.cpp
)

target_link_libraries(datamanagementmodelbenchmark
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
  datamanagementtestlib
)

kde4_add_unit_test(datamanagementadaptortest
  datamanagementadaptortest.cpp
)

target_link_libraries(datamanagementadaptortest
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
  datamanagementtestlib
)


kde4_add_executable(resourcewatchertest
  resourcewatchertest.cpp
)

target_link_libraries(resourcewatchertest
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_SERVER_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore

  datamanagementtestlib
)
