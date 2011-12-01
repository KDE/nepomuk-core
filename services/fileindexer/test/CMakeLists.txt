set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${NEPOMUK_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

kde4_add_unit_test(fileindexerconfigtest
  fileindexerconfigtest.cpp
  ${nepomukcommon_SOURCE_DIR}/regexpcache.cpp
  ${nepomukcommon_SOURCE_DIR}/fileexcludefilters.cpp
  ../fileindexerconfig.cpp)
target_link_libraries(fileindexerconfigtest
  ${QT_QTTEST_LIBRARY}
  ${KDE4_KDECORE_LIBS})

kde4_add_unit_test(indexcleanertest
  indexcleanertest.cpp
  ${nepomukcommon_SOURCE_DIR}/regexpcache.cpp
  ${nepomukcommon_SOURCE_DIR}/fileexcludefilters.cpp
  ../fileindexerconfig.cpp
  ../indexcleaner.cpp
  ../util.cpp)
target_link_libraries(indexcleanertest
  ${QT_QTTEST_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  nepomuktestcore)
