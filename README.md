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
├── src
│
│
├── subprojects
│   ├── cmsis.wrap
│   │   └── Wrap file for CMSIS dependency (headers and core support)
│   │
    ├── pes-rtc-event-logger.wrap
│   │   └── Wrap file for RTC Event Logger
│   │
│   └── unity.wrap
│       └── Wrap file for Unity unit test framework
│
├── tests
│   ├── meson.build
│   │   └── Test build configuration (host vs target tests, dependencies)
│   │
│   ├── stubs
│   │   ├── bsp_api.h
│   │       └── Minimal BSP stub definitions for host testing   
│   │
│   ├── test_main.c
│       └── Entry point for host-based Unity test execution
│
│
├── toolchains
│   ├── llvm-cortex-m23.ini
│   │   └── Meson cross file for Cortex-M23 (RA0 series)
│   │
│   └── llvm-cortex-m33.ini
│       └── Meson cross file for Cortex-M33 (RA2/RA4/RA6 series)
│
└── tree.txt
    └── Generated directory tree snapshot (for documentation/reference)

```

