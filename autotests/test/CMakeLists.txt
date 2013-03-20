include(${NEPOMUK_TESTLIB_CMAKE_DIR}/NepomukTestLibMacros.cmake)

#
# Resource Tests
#

set( RESOURCE_TESTS_SRC resourcetests.cpp )

# we are linking a static nepomuktestlib, no need for exports then

add_definitions(-DNEPOMUKTEST_EXPORT=)

kde4_add_executable(resourcetest ${RESOURCE_TESTS_SRC})

target_link_libraries(resourcetest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomukcore
  nepomuktestlib
)

add_nepomuk_test(NepomukResourceTest ${CMAKE_CURRENT_BINARY_DIR}/resourcetest)

#
# Query Tests
#

set( QUERY_TESTS_SRC querytests.cpp )

kde4_add_executable(querytests ${QUERY_TESTS_SRC})

target_link_libraries(querytests
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomukcore
  nepomuktestlib
)

add_nepomuk_test(QueryTests ${CMAKE_CURRENT_BINARY_DIR}/querytests)


#
# Query Service Tests
#

set( QUERY_SERVICE_TESTS_SRC queryservicetest.cpp )

kde4_add_executable(queryservicetest ${QUERY_SERVICE_TESTS_SRC})

target_link_libraries(queryservicetest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomuktestlib
  nepomukcore
)

add_nepomuk_test(QueryServiceTest ${CMAKE_CURRENT_BINARY_DIR}/queryservicetest)

#
# Backup Tests
#

set( BACKUP_TESTS_SRC backuptests.cpp )

set_source_files_properties(
  ../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QList")
set_source_files_properties(
  ../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QString")

qt4_add_dbus_interface( BACKUP_TESTS_SRC
                        ../../interfaces/org.kde.nepomuk.BackupManager.xml
                        backupmanagerinterface )

kde4_add_executable(backuptests ${BACKUP_TESTS_SRC})

target_link_libraries(backuptests
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomuktestlib
  nepomukcore
)

add_nepomuk_test(BackupTests ${CMAKE_CURRENT_BINARY_DIR}/backuptests)

#
# Async Tests
#
kde4_add_executable(asyncclientapitest 
    asyncclientapitest.cpp
)

target_link_libraries(asyncclientapitest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomuktestlib
  nepomukcore
)

add_nepomuk_test(AsyncClientApiTest ${CMAKE_CURRENT_BINARY_DIR}/asyncclientapitest)

#
# Normal Tests ( Do not require the testing environment )
#

# Query tests
# --------------------------------------------
set(queryparsertest_SRC queryparsertest.cpp)
kde4_add_unit_test(queryparsertest TESTNAME nepomuk-queryparsertest NOGUI ${queryparsertest_SRC})
target_link_libraries(queryparsertest nepomukcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

set(querylibtest_SRC querylibtest.cpp)
kde4_add_unit_test(querylibtest TESTNAME nepomuk-querylibtest NOGUI ${querylibtest_SRC})
target_link_libraries(querylibtest nepomukcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

set(queryserializertest_SRC queryserializertest.cpp)
kde4_add_unit_test(queryserializertest TESTNAME nepomuk-queryserializertest NOGUI ${queryserializertest_SRC})
target_link_libraries(queryserializertest nepomukcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

# DMS Tests
# -------------------------------------------

kde4_add_unit_test(simpleresourceclassestest
  simpleresourceclassestest.cpp
)
target_link_libraries(simpleresourceclassestest
  nepomukcore
  ${QT_QTTEST_LIBRARY}
  ${QT_QTCORE_LIBRARY}
)

#
# Store Resources Benchmark
#

set( STORE_RESOURCES_BENCHMARK_SRC storeresourcesbenchmark.cpp )

kde4_add_executable(storeresourcesbenchmark ${STORE_RESOURCES_BENCHMARK_SRC})

target_link_libraries(storeresourcesbenchmark
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomukcore
  nepomuktestlib
)

add_nepomuk_test(StoreResourcesBenchmark ${CMAKE_CURRENT_BINARY_DIR}/storeresourcesbenchmark)

