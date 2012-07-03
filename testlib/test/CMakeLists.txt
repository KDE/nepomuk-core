project(nepomuk_tests)

find_package(KDE4 REQUIRED)
find_package(NepomukCore REQUIRED)

include(NepomukTestLibMacros)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${CMAKE_SOURCE_DIR}
  ${SOPRANO_INCLUDE_DIR}
  ${NEPOMUK_CORE_INCLUDE_DIR}
)

include (KDE4Defaults)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

#
# Individual Tests
#

# Resource
set( RESOURCE_TESTS_SRC resourcetests.cpp )

kde4_add_executable(resourcetest ${RESOURCE_TESTS_SRC})
target_link_libraries(resourcetest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${NEPOMUK_CORE_LIBRARY}
  kdecore
  nepomuktest
)

add_nepomuk_test(NepomukResourceTest ${CMAKE_CURRENT_BINARY_DIR}/resourcetest)



