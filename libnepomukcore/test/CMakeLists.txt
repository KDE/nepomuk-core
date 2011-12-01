project(libnepomukcore_tests)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

add_definitions(-DBUILDING_NEPOMUK_TESTS)

# The static nepomukcore lib to avoid installation for testing
kde4_add_library(nepomuktestcore STATIC ${nepomuk_LIB_SRCS})
target_link_libraries(nepomuktestcore
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_CLIENT_LIBRARIES}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  kdecore
  kdeui
)

# Query tests
# --------------------------------------------
set(queryparsertest_SRC queryparsertest.cpp)
kde4_add_unit_test(queryparsertest TESTNAME nepomuk-queryparsertest NOGUI ${queryparsertest_SRC})
target_link_libraries(queryparsertest nepomuktestcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

set(querytest_SRC querytest.cpp)
kde4_add_unit_test(querytest TESTNAME nepomuk-querytest NOGUI ${querytest_SRC})
target_link_libraries(querytest nepomuktestcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

set(queryserializertest_SRC queryserializertest.cpp)
kde4_add_unit_test(queryserializertest TESTNAME nepomuk-queryserializertest NOGUI ${queryserializertest_SRC})
target_link_libraries(queryserializertest nepomuktestcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)



# Resource tests
# --------------------------------------------

# The basis of all the tests handling statements
kde4_add_library(nepomuk_testbase STATIC testbase.cpp)
target_link_libraries(nepomuk_testbase nepomuktestcore ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY})

set(resourcetest_SRCS resourcetest.cpp)
kde4_add_unit_test(resourcetest TESTNAME nepomuk-resourcetest ${resourcetest_SRCS})
target_link_libraries(resourcetest
  nepomuk_testbase
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${KDE4_KDECORE_LIBS}
  ${SOPRANO_LIBRARIES}
  kdecore)
