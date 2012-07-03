project(filewatch_tests)

find_package(KDE4 REQUIRED)
find_package(Nepomuk REQUIRED)

include( NepomukTestLibMacros )

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${CMAKE_SOURCE_DIR}
  ${SOPRANO_INCLUDE_DIR}
  ${NEPOMUK_INCLUDE_DIR}
)

include (KDE4Defaults)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

add_definitions(-DDISABLE_NEPOMUK_LEGACY=1)

set(filewatch_tests_SRCS filewatch.cpp )

kde4_add_executable( filewatch_tests ${filewatch_tests_SRCS} )
add_nepomuk_test(filewatch_tests ${CMAKE_CURRENT_BINARY_DIR}/filewatch_tests)

target_link_libraries(filewatch_tests
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  kdecore
  nepomuk
  nepomuktest
)



