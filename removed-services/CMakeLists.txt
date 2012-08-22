project(storage)

install(
  FILES
  nepomukbackupsync.desktop
  nepomukactivitiesservice.desktop
  nepomukontologyloader.desktop
  nepomukremovablestorageservice.desktop
  nepomukstrigiservice.desktop
  nepomukqueryservice.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})

