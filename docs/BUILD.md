
# Recommended Workflow

1. Develop and validate logic using **native unit tests**
2. Perform cross build to generate firmware
3. Flash and validate on hardware

***

# 🧪 Native Build (MSVC) + Unit Tests

Used for **fast validation of logic** without hardware.

***

## Prerequisites

* Visual Studio (C++ workload)
* Meson + Ninja

***

## Step 1: Open Developer Prompt

Use:

```
x64 Native Tools Command Prompt for VS
```

***

## Step 2: Configure native build

```bash
meson setup --wipe build-msvc \
  --buildtype=debug
```

> Note: No cross-file is used for native builds.

***

## Step 3: Build

```bash
meson compile -C build-msvc
```

***

## Step 4: Run tests

```bash
meson test -C build-msvc --print-errorlogs
```

***

## What is being tested

*
*

***

## Test environment

| Component | Used                      |
| --------- | ------------------------- |
| FSP       | Not used                  |
| libc      | Host                      |

***

# 🔁 Switching Between Builds

| Build Type        | Command                                    |
| ----------------- | ------------------------------------------ |
| Native (MSVC)     | `meson setup build-msvc`                   |
| Cross (RA target) | `meson setup build-ra0l1 --cross-file=...` |

> Always use separate build directories.

***

## Key Differences

| Feature   | Native          | Cross           |
| --------- | --------------- | --------------- |
| Compiler  | MSVC (`cl.exe`) | Clang (ARM)     |
| Execution | Host PC         | MCU             |
| UART      | Fake            | Real FSP        |
| Output    | Test exe        | `.elf`, `.srec` |
| Purpose   | Validation      | Deployment      |

***

# 🚀 Cross Build Instructions (LLVM Embedded Toolchain)

This project uses Meson to build firmware for Renesas RA MCUs using the LLVM Embedded Toolchain and e²studio-generated sources.

***

## Prerequisites

* LLVM Embedded Toolchain installed (e.g., LLVM-ET-Arm)
* Meson ≥ 0.61
* Ninja
* Valid cross file (e.g., `toolchains/llvm-cortex-m23.ini`)
* e²studio-generated project present under:
  ```
  examples/e2studio/<example>
  ```

***

## Step 1: Configure the build

Use your exact command:

```bash
meson setup --wipe build-ra0l1 \
  --cross-file toolchains/llvm-cortex-m23.ini \
  -Dmcu=ra0l1 \
  -Dboard=fpb \
  -Dide=e2studio \
  -Dexample=stdio_uart
```

***

## Explanation of options

| Option                 | Meaning                                                                   |
| ---------------------- | ------------------------------------------------------------------------- |
| `--wipe`               | Ensures a clean configuration (removes previous build state)              |
| `--cross-file`         | Specifies toolchain (CPU, compiler, linker flags)                         |
| `-Dmcu=ra0l1`          | Selects MCU family                                                        |
| `-Dboard=fpb`          | Selects target board (combined internally to `ra0l1_fpb`)                 |
| `-Dide=e2studio`       | Selects project layout style                                              |
| `-Dexample=stdio_uart` | Selects example → resolves to `examples/e2studio/stdio_uart_on_fpb_ra0l1` |

***

## Step 2: Build the firmware

```bash
meson compile -C build-ra0l1
```

***

## Step 3: Generated outputs

After build completes:

| Artifact | Description                                    |
| -------- | ---------------------------------------------- |
| `*.elf`  | Firmware image (debuggable)                    |
| `*.srec` | Flashable image (generated via `llvm-objcopy`) |
| `*.map`  | Linker map file for symbol analysis            |

***

## Example output path

```
build-ra0l1/examples/e2studio/RTC_Event_Logger_FPB_RA4E2/
```

***

## Step 4: Flashing (external step)

Flash using one of:

* Renesas Flash Programmer (RFP CLI)
* J-Link
* e²studio debugger

***
