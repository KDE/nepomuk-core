project(nepomukindexer)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  )

kde4_add_library(nepomukextractor SHARED extractorplugin.cpp)

target_link_libraries(nepomukextractor
  ${QT_QTCORE_LIBRARY}
  ${KDE4_KDECORE_LIBS}
  nepomukcore
)

install(TARGETS nepomukextractor EXPORT NepomukCoreLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

set(indexer_SRCS
  main.cpp
  indexer.cpp
  simpleindexer.cpp
  extractorpluginmanager.cpp
  ../util.cpp
  ../../../servicestub/priority.cpp
  )

soprano_add_ontology(indexer_SRCS ${nepomuk_ontologies_SOURCE_DIR}/kext.trig "KExt" "Nepomuk2::Vocabulary" "trig")

kde4_add_executable( nepomukindexer ${indexer_SRCS} )

target_link_libraries( nepomukindexer
  nepomukcommon
  nepomukcore
  nepomukextractor
  ${KDE4_KDECORE_LIBS}
  ${SOPRANO_LIBRARIES}
  )

install(
  TARGETS nepomukindexer
  DESTINATION ${BIN_INSTALL_DIR} )

install(
  FILES nepomukextractor.desktop
  DESTINATION ${SERVICETYPES_INSTALL_DIR})

# -----------------------------
# Extractors
#

if(POPPLER_QT4_FOUND)
    include_directories( ${POPPLER_QT4_INCLUDE_DIR} )

    kde4_add_plugin( nepomukpopplerextractor popplerextractor.cpp )

    target_link_libraries( nepomukpopplerextractor
        nepomukcore
        nepomukextractor
        ${KDE4_KIO_LIBS}
        ${POPPLER_QT4_LIBRARIES}
    )

    install(
    FILES nepomukpopplerextractor.desktop
    DESTINATION ${SERVICES_INSTALL_DIR})

    install(
    TARGETS nepomukpopplerextractor
    DESTINATION ${PLUGIN_INSTALL_DIR})

endif(POPPLER_QT4_FOUND)

if(TAGLIB_FOUND)
    include_directories( ${TAGLIB_INCLUDES} )

    kde4_add_plugin( nepomuktaglibextractor taglibextractor.cpp )

    target_link_libraries( nepomuktaglibextractor
        nepomukcore
        nepomukextractor
        ${KDE4_KIO_LIBS}
        ${TAGLIB_LIBRARIES}
    )

    install(
    FILES nepomuktaglibextractor.desktop
    DESTINATION ${SERVICES_INSTALL_DIR})

    install(
    TARGETS nepomuktaglibextractor
    DESTINATION ${PLUGIN_INSTALL_DIR})

endif(TAGLIB_FOUND)

if(EXIV2_FOUND)
    include_directories( ${EXIV2_INCLUDE_DIR} )

    set_source_files_properties( exiv2extractor.cpp PROPERTIES
        COMPILE_FLAGS "${KDE4_ENABLE_EXCEPTIONS}"
        )

    kde4_add_plugin( nepomukexiv2extractor exiv2extractor.cpp )

    target_link_libraries( nepomukexiv2extractor
        nepomukcore
        nepomukextractor
        ${KDE4_KIO_LIBS}
        ${EXIV2_LIBRARIES}
    )

    install(
    FILES nepomukexiv2extractor.desktop
    DESTINATION ${SERVICES_INSTALL_DIR})

    install(
    TARGETS nepomukexiv2extractor
    DESTINATION ${PLUGIN_INSTALL_DIR})

endif(EXIV2_FOUND)

if(FFMPEG_FOUND)
    include_directories( ${FFMPEG_INCLUDE_DIRS} )

    kde4_add_plugin( nepomukffmpegextractor ffmpegextractor.cpp )

    target_link_libraries( nepomukffmpegextractor
        nepomukcore
        nepomukextractor
        ${KDE4_KIO_LIBS}
        ${FFMPEG_LIBRARIES}
    )

    install(
    FILES nepomukffmpegextractor.desktop
    DESTINATION ${SERVICES_INSTALL_DIR})

    install(
    TARGETS nepomukffmpegextractor
    DESTINATION ${PLUGIN_INSTALL_DIR})

endif(FFMPEG_FOUND)


kde4_add_plugin( nepomukplaintextextractor plaintextextractor.cpp )

target_link_libraries( nepomukplaintextextractor
    nepomukcore
    nepomukextractor
    ${KDE4_KIO_LIBS}
    ${FFMPEG_LIBRARIES}
)

install(
FILES nepomukplaintextextractor.desktop
DESTINATION ${SERVICES_INSTALL_DIR})

install(
TARGETS nepomukplaintextextractor
DESTINATION ${PLUGIN_INSTALL_DIR})
