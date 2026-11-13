# Project-local backport of espressif/esp-idf 83ede087025eb3edc5ccad83bfa4cf064ca6b8cc.
# The upstream change updates only the ESP32 controller archive (IDFGH-17125).
# Run after project(): the SDK's GLOBAL imported target must already exist.
if(NOT CONFIG_IDF_TARGET_ESP32 OR NOT TARGET bt_btdm_app)
    message(FATAL_ERROR "The controller compatibility backport requires ESP32 / bt_btdm_app")
endif()

idf_build_get_property(retro_idf_path IDF_PATH)
file(STRINGS "${retro_idf_path}/version.txt" retro_idf_version LIMIT_COUNT 1)
if(NOT retro_idf_version STREQUAL "6.0.1")
    message(FATAL_ERROR "Review the BT controller backport before changing ESP-IDF ${retro_idf_version}")
endif()

get_target_property(retro_original_controller bt_btdm_app IMPORTED_LOCATION)
file(SHA256 "${retro_original_controller}" retro_original_sha256)
if(NOT retro_original_sha256 STREQUAL "9552f29e7e58823d3d62ecf6310d89c110239da41655d0783be80ac77829e7db")
    message(FATAL_ERROR "Unexpected original ESP32 controller archive; review compatibility before linking")
endif()

set(retro_fixed_controller
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/esp32-bt-lib/10f4e9ad6260788765863ed7f05e0e649b288e36/libbtdm_app.a")
get_filename_component(retro_fixed_controller "${retro_fixed_controller}" ABSOLUTE)
file(SHA256 "${retro_fixed_controller}" retro_fixed_sha256)
if(NOT retro_fixed_sha256 STREQUAL "dc4d03d1ace55a3399cb3104284b3d1c5b76fc0931212ed999d4d8f59414bb19")
    message(FATAL_ERROR "Unexpected project-local ESP32 controller archive SHA256")
endif()

# Do not copy over or modify the user's shared PlatformIO framework package.
set_property(TARGET bt_btdm_app PROPERTY IMPORTED_LOCATION "${retro_fixed_controller}")
message(STATUS "RetroTerminal: ESP32 controller fix #18145, lib commit 10f4e9ad6260")
