cmake_minimum_required(VERSION 3.16)

# Add only necessary components to the build
set(EXTRA_COMPONENT_DIRS 
    ../components/mgr_ac_logic
    ../components/mgr_ir_protocols
    ../components/drv_ir_rmt
    ../components/svc_logging
    ../components/sys_utils
    ../components/svc_nvs
    ../components/drv_led_strip
)

# Include the ESP-IDF project cmake file
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_app)
