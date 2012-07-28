project(nepomuksync_tests)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${libnepomuksync_SOURCE_DIR}
)

include (KDE4Defaults)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

set(nepomuksynctests_SRCS testbase.cpp nepomuksynctests.cpp )

kde4_add_unit_test(nepomuksynctest ${nepomuksynctests_SRCS})

target_link_libraries(nepomuksynctest
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomuksync
)
