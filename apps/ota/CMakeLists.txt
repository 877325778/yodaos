cmake_minimum_required(VERSION 3.0)
project(yodaos-ota-app CXX)
set(CMAKE_CXX_STANDARD 11)

# MARK: - ota
set(APP_ROOT_DIR "/opt/apps/ota")
set(APP_INSTALL_DIR "${CMAKE_INSTALL_DIR}${APP_ROOT_DIR}")

file(GLOB OTA_APP_SRC *.js *.json)

install(FILES ${OTA_APP_SRC}
  DESTINATION ${APP_INSTALL_DIR}
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
install(DIRECTORY ./script
  DESTINATION ${APP_INSTALL_DIR}
  USE_SOURCE_PERMISSIONS)
