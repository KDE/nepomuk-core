project(nepomuksync_tests)

find_package(KDE4 REQUIRED)
find_package(Nepomuk REQUIRED)

include( NepomukTestLibMacros )

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${NEPOMUK_INCLUDE_DIR}
  ${libnepomuksync_SOURCE_DIR}
)

include (KDE4Defaults)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

add_definitions(-DDISABLE_NEPOMUK_LEGACY=1)

set(nepomuksynctests_SRCS identificationtest.cpp )

kde4_add_executable( nepomuksynctest ${nepomuksynctests_SRCS} )
add_nepomuk_test(nepomuksynctest ${CMAKE_CURRENT_BINARY_DIR}/nepomuksynctest)

target_link_libraries(nepomuksynctest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  kdecore
  nepomuktest
  nepomuk
)



