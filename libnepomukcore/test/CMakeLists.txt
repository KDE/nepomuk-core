project(libnepomukcore_tests)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

add_definitions(-DBUILDING_NEPOMUK_TESTS)

# Query tests
# --------------------------------------------
set(queryparsertest_SRC queryparsertest.cpp)
kde4_add_unit_test(queryparsertest TESTNAME nepomuk-queryparsertest NOGUI ${queryparsertest_SRC})
target_link_libraries(queryparsertest nepomukcore
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

set(querytest_SRC querytest.cpp)
kde4_add_unit_test(querytest TESTNAME nepomuk-querytest NOGUI ${querytest_SRC})
target_link_libraries(querytest nepomukcore
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

