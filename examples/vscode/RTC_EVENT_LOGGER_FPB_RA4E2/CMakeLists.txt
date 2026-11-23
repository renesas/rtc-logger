# Please see README.md for build instructions

cmake_minimum_required(VERSION 3.16.4)

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_SYSTEM_PROCESSOR arm)

# Use add_library() with the STATIC option to name the source file in the generated project.
# This avoids running the linker and is intended for use with cross-compiling toolchains that
# cannot link without custom flags or linker scripts
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)

if(BUILD_KIT)
	message(NOTICE "Build Kit is defined as ${BUILD_KIT}")
endif()

message(NOTICE "CMAKE_CURRENT_BINARY_DIR is defined as ${CMAKE_CURRENT_BINARY_DIR}")

get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(isMultiConfig)
    message(NOTICE "Multi-config generator(${CMAKE_GENERATOR}) is active")
else()
    message(NOTICE "Single-config generator(${CMAKE_GENERATOR}) is active")
	if(DEFINED CMAKE_BUILD_TYPE)
	 	message(NOTICE "CMAKE_BUILD_TYPE is defined as ${CMAKE_BUILD_TYPE}")
	elseif(DEFINED BUILD_TYPE)
		set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
		message(NOTICE "CMAKE_BUILD_TYPE is set to BUILD_TYPE ENV ${CMAKE_BUILD_TYPE}")
	else()
		message(FATAL_ERROR "Please specify the CMAKE_BUILD_TYPE with -DCMAKE_BUILD_TYPE=")
	endif()
    #set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
	message(NOTICE "CMAKE_RUNTIME_OUTPUT_DIRECTORY is defined as ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

cmake_minimum_required(VERSION 3.16.4)

# Project configuration
project(RTC_Event_Logger_FPB_RA4E2_cmake
	VERSION 1.0.0
	LANGUAGES C CXX ASM)

set(CMAKE_CROSSCOMPILING "TRUE")
set(CMAKE_EXPORT_COMPILE_COMMANDS "TRUE")

include(${CMAKE_CURRENT_LIST_DIR}/Config.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/GeneratedCfg.cmake)

# If required user can amend RASC generated flags here, e.g. list(REMOVE_ITEM RASC_CMAKE_C_FLAGS "-Wall")

include(${CMAKE_CURRENT_LIST_DIR}/cmake/GeneratedSrc.cmake)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE $<$<CONFIG:Debug>:
    BSP_CFG_RTC_USED=1>)

# ─── Console transport backend ─────────────────────────────────────────────────
# Set USE_RTT_CONSOLE=ON to build with SEGGER RTT instead of the Buffered I/O
# UART driver.  Only the selected backend's source files are compiled — the
# other backend's files (and, for RTT, the vendored UART driver) are excluded
# from the build entirely, saving Flash/RAM neither backend actually needs.
#
#   cmake -DUSE_RTT_CONSOLE=ON ...
#   cmake -DUSE_RTT_CONSOLE=OFF ...   (default — UART backend)
# ───────────────────────────────────────────────────────────────────────────────
option(USE_RTT_CONSOLE "Use SEGGER RTT as the console transport (backend 1)" OFF)

get_target_property(_all_src ${PROJECT_NAME}.elf SOURCES)

if(USE_RTT_CONSOLE)
    target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
        RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE=1)

    # Drop the vendored Buffered I/O UART driver sources — not needed for RTT.
    list(FILTER _all_src EXCLUDE REGEX "rs_stdio_uart_buffer_io")

    target_include_directories(${PROJECT_NAME}.elf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/SEGGER_RTT)

    message(STATUS "Console backend: SEGGER RTT  (rs_stdio_uart_buffer_io excluded)")
else()
    # Drop the SEGGER RTT sources — not needed for the default UART backend.
    list(FILTER _all_src EXCLUDE REGEX "SEGGER_RTT|rs_rtc_logger_rtt_console")

    message(STATUS "Console backend: Buffered I/O UART  (default, SEGGER RTT excluded)")
endif()

set_property(TARGET ${PROJECT_NAME}.elf PROPERTY SOURCES ${_all_src})

target_include_directories(${PROJECT_NAME}.elf
    BEFORE
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)



target_include_directories(${PROJECT_NAME}.elf
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/.
	${CMAKE_CURRENT_SOURCE_DIR}/src/rtc_event_logger
	${CMAKE_CURRENT_SOURCE_DIR}/src/rtc_event_logger/config
)

target_link_options(${PROJECT_NAME}.elf
    PRIVATE
        $<$<CONFIG:Debug>:
            "SHELL:-Wl,-z,norelro"
        >
)

target_link_options(${PROJECT_NAME}.elf
    PRIVATE
        $<$<CONFIG:Release>:
            "SHELL:-Wl,-z,norelro"
        >
)


target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    crt0
    c++abi
)

