# Overview
This repo contains multiple instances of standard input output stream handlers as explained briefly in sections below.

***

# Buffered I/O over UART

[syscalls_picolib_stdio_to_uart.c](docs/README-UART.md) module provides a **clean stdio abstraction** over FSP UART using:

* ByteQ buffering
* ISR-driven communication
* Minimal integration effort

It is best suited for:

* Debug console
* Development phase instrumentation

For production-grade systems, consider:

* Removing heap usage
* Adding non-blocking APIs
* RTOS integration

***

# Project Structure

```
.
├── README.md
│   └── High-level overview of the project, usage, and build instructions
│
├── docs
│   └── README-UART.md
│       └── Detailed documentation for UART stdio implementation and design notes
│
├── examples
│   └── e2studio
│       ├── stdio_uart_on_fpb_ra0l1
│       │   └── e²studio project for RA0L1 FPB demonstrating UART-backed stdio
│       │
│       └── stdio_uart_on_fpb_ra2e3
│           └── e²studio project for RA2E3 FPB demonstrating UART-backed stdio
│
├── meson.build
│   └── Top-level Meson build definition (options, subprojects, dependency wiring)
│
├── meson_options.txt
│   └── User-selectable build options (MCU, board, example, toolchain)
│
├── src
│   ├── reviews
│   │   └── copilot.md
│   │       └── Design reviews, notes, and analysis artifacts
│   │
│   └── syscalls_picolib_stdio_to_uart.c
│       └── Core implementation: redirects picolibc stdio to UART using FSP APIs
│
├── subprojects
│   ├── cmsis.wrap
│   │   └── Wrap file for CMSIS dependency (headers and core support)
│   │
│   ├── fsp.wrap
│   │   └── Wrap file for Renesas FSP (if used externally)
│   │
│   ├── pes-queues.wrap
│   │   └── Wrap file for ByteQ (data_queues) reusable queue library
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
│   │   │   └── Minimal BSP stub definitions for host testing
│   │   │
│   │   ├── fake_uart.c / fake_uart.h
│   │   │   └── UART test double (simulates FSP UART behavior)
│   │   │
│   │   └── r_uart_api.h
│   │       └── Stubbed UART API interface for unit tests
│   │
│   ├── test_main.c
│   │   └── Entry point for host-based Unity test execution
│   │
│   ├── test_main_firmware.c
│   │   └── Entry point for on-target Unity test execution (firmware mode)
│   │
│   └── test_picolibc_stdio_to_uart.c
│       └── Unit tests for stdio-to-UART implementation (init, RX, TX, flush)
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

