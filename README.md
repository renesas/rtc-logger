# Overview
This repo contains the RTC Event Logger

***

# RTC Internal Timestamp Event Logger

***

# Project Structure

```
.
├── README.md
│   └── High-level overview of the project, usage, and build instructions
│
├── docs
│   └── README-RTC_EVENT_LOGGER.md
│       └── Detailed documentation for RTC Event Logger implementation and design notes
│
├── examples
│   └── e2studio
│       ├── RTC_EVENT_LOGGER_FPB_RA4E2
│       │   └── e²studio project for FPB-RA4E2 demonstrating RTC Event Logger
│       │
│       └── RTC_EVENT_LOGGER_FPB_RA6E2
│           └── e²studio project for FPB-RA6E2 demonstrating RTC Event Logger
│
├── meson.build
│   └── Top-level Meson build definition (options, subprojects, dependency wiring)
│
├── meson_options.txt
│   └── User-selectable build options (MCU, board, example, toolchain)
│
├── firmware
│   └── Meson based Build for Firmware Targets - Builds ELF targets per Board
│
├── src
│   └── PES RTC Event Logger Source and Demo
│
├── subprojects
│   ├── packagefiles
│   │   └── 
│   ├── unity
│   ├── cmsis 
│   ├── fsp
│   ├── rm_comms_lock.patch
│   └── unity.wrap
│       └── Wrap file for Unity unit test framework
│
├── tests
│   ├── meson.build
│   │   └── Test build configuration (host vs target tests, dependencies)
│   ├── run_ut.robot
│   │   └── Run On-Target Unity Firmware Tests
│   │
│   ├── stubs
│   │   ├── FreeRTOS.h
│   │   ├── rs_rtc_logger_test_types.h
│   │   ├── task.h
│   │
│   ├── target
│   │   ├── syscalls_sci_uart.c
│   │   ├── unity_config.h
│   │   ├── unity_port_sci_uart.c
│   │
│   ├── test_main.c
│   │   └── Entry point for host-based Unity test execution
│   ├── test_main_firmware.c
│   │   └── Entry point for On-Target Unity test execution
│   ├── test_rs_rtc_logger_core.c
│   │   └── Unit Tests for Lifecycle state-machine transistions and EventTrigger enqueue, dequeue, overflow
│   ├── test_rs_rtc_logger_format.c      
│   │   └── Unit Tests for RTC Event Logger formatting engine
│
├── toolchains
│   ├── llvm-cortex-m23.ini
│   │   └── Meson cross file for Cortex-M23 (RA0 series)
│   │
│   └── llvm-cortex-m33.ini
│       └── Meson cross file for Cortex-M33 (RA2/RA4/RA6 series)

```

