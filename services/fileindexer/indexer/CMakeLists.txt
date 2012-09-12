project(nepomukindexer)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${POPPLER_QT4_INCLUDE_DIR}
  ${TAGLIB_INCLUDES}
  ${EXIV2_INCLUDE_DIR}
  ${FFMPEG_INCLUDE_DIRS}
  )

set(indexer_SRCS
  main.cpp
  indexer.cpp
  simpleindexer.cpp
  extractor.cpp
  popplerextractor.cpp
  taglibextractor.cpp
  exiv2extractor.cpp
  ffmpegextractor.cpp
  ../util.cpp
  ../../../servicestub/priority.cpp
  )

soprano_add_ontology(indexer_SRCS ${nepomuk_ontologies_SOURCE_DIR}/kext.trig "KExt" "Nepomuk2::Vocabulary" "trig")

kde4_add_executable( nepomukindexer ${indexer_SRCS} )

target_link_libraries( nepomukindexer
  nepomukcommon
  nepomukcore
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  ${POPPLER_QT4_LIBRARIES}
  ${TAGLIB_LIBRARIES}
  ${EXIV2_LIBRARIES}
  ${FFMPEG_LIBRARIES}
  )

install(
  TARGETS nepomukindexer
  DESTINATION ${BIN_INSTALL_DIR} )
# -----------------------------
