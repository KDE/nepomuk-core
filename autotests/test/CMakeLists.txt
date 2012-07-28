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
  kdecore
  nepomukcore
  nepomuktestlib
)

add_nepomuk_test(NepomukResourceTest ${CMAKE_CURRENT_BINARY_DIR}/resourcetest)


#
# Query Update Tests
#

set( QUERY_UPDATE_TESTS_SRC queryupdatetests.cpp )

kde4_add_executable(queryupdatetests ${QUERY_UPDATE_TESTS_SRC})

target_link_libraries(queryupdatetests
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  kdecore
  nepomukcore
  nepomuktestlib
)

add_nepomuk_test(QueryUpdateTests ${CMAKE_CURRENT_BINARY_DIR}/queryupdatetests)

