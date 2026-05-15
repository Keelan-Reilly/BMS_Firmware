cmake_minimum_required(VERSION 3.20)
project(BmsTests C)
set(CMAKE_C_STANDARD 11)

enable_testing()

# ── Unity test framework ───────────────────────────────────────────────────────
add_library(unity STATIC
    vendor/unity/unity.c
)
target_include_directories(unity PUBLIC vendor/unity)

# ── Mock BSP ──────────────────────────────────────────────────────────────────
add_library(mock_bsp STATIC
    mock_bsp/mock_board_spi.c
    mock_bsp/mock_board_uart.c
    mock_bsp/mock_board_outputs.c
    mock_bsp/mock_board_clock.c
    mock_bsp/mock_board_flash.c
)
target_include_directories(mock_bsp PUBLIC
    mock_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/board
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src
)

# ── Firmware source (host build, no hardware) ─────────────────────────────────
set(FIRMWARE_SOURCES_HOST
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms/bms_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms/bms_faults.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms/bms_outputs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms/bms_balance.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/drivers/isospi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/drivers/ltc6812.c
)

set(BOOTLOADER_SOURCES_HOST
    ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/src/bl_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/src/bl_jump.c
)

# Helper to add a test executable
function(add_bms_test target src)
    add_executable(${target} ${src} ${FIRMWARE_SOURCES_HOST} ${ARGN})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/board
        ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/drivers
        ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/bms
        ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/src
    )
    target_link_libraries(${target} unity mock_bsp)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        # Suppress some warnings that arise from host-building firmware code
        -DBMS_HOST_BUILD=1
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ── Test targets ──────────────────────────────────────────────────────────────
add_bms_test(test_pec15          unit/test_pec15.c)
add_bms_test(test_protocol_crc   unit/test_protocol_crc.c)
add_bms_test(test_config_validate unit/test_config_validate.c)
add_bms_test(test_config_masks   unit/test_config_masks.c)
add_bms_test(test_bms_outputs    unit/test_bms_outputs.c)
add_bms_test(test_flash_layout   unit/test_flash_layout.c)
add_bms_test(test_bootloader_validate
    unit/test_bootloader_validate.c
    ${BOOTLOADER_SOURCES_HOST}
)
