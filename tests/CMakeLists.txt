set( querytester_SRCS
  querytester.cpp
)

kde4_add_ui_files(querytester_SRCS querytester.ui)

kde4_add_executable(nepomukquerytester TEST ${querytester_SRCS})

target_link_libraries(nepomukquerytester
  ${QT_QTGUI_LIBRARY}
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
)
