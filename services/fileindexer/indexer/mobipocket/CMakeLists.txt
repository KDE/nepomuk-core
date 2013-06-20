kde4_add_plugin( nepomukmobiextractor mobiextractor.cpp mobipocket.cpp decompressor.cpp )

target_link_libraries( nepomukmobiextractor
    nepomukcore
    nepomukextractor
    ${KDE4_KIO_LIBS}
)

install(
FILES nepomukmobiextractor.desktop
DESTINATION ${SERVICES_INSTALL_DIR})

install(
TARGETS nepomukmobiextractor
DESTINATION ${PLUGIN_INSTALL_DIR})
