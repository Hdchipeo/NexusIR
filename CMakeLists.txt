cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_LIST_DIR}/external/esp-homekit-sdk/components/homekit
)

# set(EXCLUDE_COMPONENTS button)
# set project version
set(PROJECT_VERSION "1.5.8")
add_compile_definitions(PROJECT_VERSION=\"${PROJECT_VERSION}\")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nexus-ir VERSION ${PROJECT_VERSION})

spiffs_create_partition_image(storage components/svc_web_server/www FLASH_IN_PROJECT)
