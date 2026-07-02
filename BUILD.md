# Build & Test Instructions — RTC Event Logger (Meson + LLVM/Clang)

Supports:
- ✅ Host-based **unit tests** (Windows + Linux, no hardware)
- ✅ Embedded firmware for **FPB-RA4E2** (Cortex-M33, R7FA4E2B93CFM)
- ✅ Embedded firmware for **FPB-RA6E2** (Cortex-M33, R7FA6E2BB3CFM)
- ✅ On-target Unity test firmware for both boards
- ✅ Flash deployment via **rfp-cli** (JLink/SWD)

---

## 📁 Project Structure

```
RTC_Event_Logger_Meson/
│   BUILD.md
│   meson.build
│   meson_options.txt
│
├── firmware/
│   │   meson.build
│   ├── generated/
│   │       meson.build         ← RASC custom_target + rasc_gen[] index
│   ├── ra4e2-fpb/
│   │       configuration.xml   ← R7FA4E2B93CFM (checked in)
│   └── ra6e2-fpb/
│           configuration.xml   ← R7FA6E2BB3CFM (checked in)
│
├── meson/
│       llvm-clangcl-msvc.ini          ← Windows host (unit tests)
│       llvm-clangcl-ubuntu2404-ci.ini ← Linux host (unit tests / CI)
│       llvm-cortex-m33.ini            ← Cortex-M33 cross (both boards)
│
├── scripts/
│       env.ps1                 ← Windows PATH setup (edit before use)
│       run_rasc.py             ← headless RASC + output copy
│       run_rasc_bundle.py      ← smart bundle for TrustZone MCUs
│       UartLibrary.py          ← optional VCOM helper (Robot Framework)
│
├── src/
│   │   blinky_thread_entry_ra4e2.c   ← FreeRTOS entry — RA4E2
│   │   blinky_thread_entry_ra6e2.c   ← FreeRTOS entry — RA6E2
│   │   crt0_stub.c                   ← weak _start stub (picolibc compat)
│   │   fault_handler.c
│   │   hal_entry.c                   ← FSP v6.5.0: no longer RASC-generated
│   │   hal_warmstart.c / .h          ← warm-reset seq-count preservation
│   │   rs_rtc_logger_demo.c          ← reference demo (board-agnostic)
│   └── rs_rtc_logger/
│       │   rs_rtc_logger.c           ← core (v18 — TestIssue_02 bug fix)
│       │   rs_rtc_logger_format.c    ← pure formatting engine
│       │   rs_rtc_logger_hal.c       ← HAL shim
│       │   rs_rtc_logger.h / _cfg.h / _format.h / _hal.h / _types.h
│       └── config/
│               rs_rtc_logger_data_fpb_ra4e2.c/.h
│               rs_rtc_logger_data_fpb_ra6e2.c/.h
│
├── subprojects/
│       unity.wrap / fsp.wrap / cmsis.wrap
│       packagefiles/           ← Meson patch overlays
│
└── tests/
    │   meson.build
    │   test_main.c                         ← host runner
    │   test_main_firmware.c                ← on-target runner
    │   test_rs_rtc_logger_format.c         ← RtcFormat + RtcFormatLine
    │   test_rs_rtc_logger_core.c           ← RtcLoggerLifecycle + RtcLoggerBuffer
    ├── stubs/
    │       rs_rtc_logger_test_types.h      ← FSP type stubs for host build
    │       FreeRTOS.h                      ← FreeRTOS stubs for host build
    │       task.h                          ← FreeRTOS task stubs for host build
    └── target/
            unity_config.h                  ← Unity output macro
            unity_port_sci_uart.c           ← unity_putc() via SCI UART
            syscalls_sci_uart.c             ← newlib _write() on target
```

---

## 🛠️ Tools to Install

### Windows — Install Order

All installers require **Administrator mode** except `pip install` which runs
as a normal user once Python is installed.

**1. MSVC Build Tools**
Required by `clang-cl` for Windows headers and the linker.
Download: https://visualstudio.microsoft.com/downloads/
Scroll to "Build Tools for Visual Studio" and install:
- Desktop development with C++
- MSVC toolchain
- Windows SDK (latest)

**2. LLVM generic**
Used for host unit tests (`clang-cl`, `x86_64-pc-windows-msvc` target).
Download: https://releases.llvm.org/
Install to the default path `C:\Program Files\LLVM`.

**3. e2studio with ATfE toolchain**
The e2studio FSP bundle includes ATfE (Arm Toolchain for Embedded, LLVM
variant) which provides `clang.exe` targeting `arm-none-eabi` with the
correct bare-metal sysroot headers (`setjmp.h`, `string.h`, etc.) for
Cortex-M33 firmware cross builds.
Download: https://www.renesas.com/en/software-tool/e2studio
Default install path:
`C:\Users\<username>\AppData\Local\Programs\Renesas\RA\e2studio_v2025-12_fsp_v6.4.0\`

ATfE `clang.exe` is at:
`...\toolchains\llvm_arm\ATfE-21.1.1-Windows-x86_64\bin\clang.exe`

> **PATH order matters:** Generic LLVM must appear **after** ATfE in
> `env.ps1` so that `clang-cl.exe` resolves to the generic LLVM
> (`x86_64-pc-windows-msvc`) and `clang.exe` resolves to ATfE
> (`arm-none-eabi`). See `scripts/env.ps1` for the correct order.

**4. Python 3**
Required to run Meson, Ninja, and the RASC helper scripts.
Download: https://www.python.org/downloads/windows/
During install tick **"Add Python to PATH"**.

Then install Meson and Ninja (no admin needed):
```powershell
pip install meson ninja
```

Verify:
```powershell
meson --version
ninja --version
```

**5. Git**
Required by Meson to fetch the `unity`, `fsp`, and `cmsis` subprojects
from GitHub on first build.
Download: https://git-scm.com/download/win

**6. RASC standalone** *(firmware builds only — not needed for unit tests)*
The standalone RASC installer provides `rasc.exe` for headless FSP code
generation. This is separate from e2studio — `e2studioc.exe` bundled with
e2studio cannot run `--generate` headlessly (it waits for an Eclipse socket
connection). The standalone installer is required.

Download: https://github.com/renesas/fsp/releases
Look for: `setup_fsp_v6_5_0_rasc_v2026-04.2.exe`
Default install path: `C:\Renesas\RA\sc_v2026-04.2_fsp_v6.5.0\eclipse\rasc.exe`

Verify:
```powershell
rasc --version
```

**7. Renesas Flash Programmer V3** *(flashing only — not needed for building)*
Provides `rfp-cli.exe` used by the `rfp_program` and `rfp_program_ut` Meson
targets to flash firmware over JLink/SWD.
Download: https://www.renesas.com/en/software-tool/renesas-flash-programmer-programming-gui
Update `scripts/env.ps1` with the install path.

### Tool install paths (update env.ps1 if different)

```
C:\Program Files\LLVM\bin\                          # generic LLVM (clang-cl)
C:\Users\MyPC\AppData\Local\Programs\Renesas\RA\
  e2studio_v2025-12_fsp_v6.4.0\toolchains\
  llvm_arm\ATfE-21.1.1-Windows-x86_64\bin\          # ATfE (clang)
C:\Users\MyPC\AppData\Local\Programs\Python\Python311\
C:\Renesas\RA\sc_v2026-04.2_fsp_v6.5.0\eclipse\    # RASC v6.5.0
```

### Admin requirement summary

| Step | Admin needed? |
|------|--------------|
| MSVC Build Tools installer | ✅ Yes |
| LLVM installer | ✅ Yes |
| Python installer (system-wide) | ✅ Yes |
| `pip install meson ninja` | ❌ No |
| Git installer | ✅ Yes |
| e2studio / RASC installer | ✅ Yes |
| Renesas Flash Programmer | ✅ Yes |
| `meson setup / compile / test` | ❌ No |

Install the tools once in an admin PowerShell, then do all builds in a
normal (non-admin) PowerShell session.

### First-time environment setup (Windows)

Edit `scripts/env.ps1` to match your installation paths, then run it at
the start of every build session:

```powershell
. .\scripts\env.ps1
```

If scripts are blocked by execution policy, run once:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
Unblock-File -Path .\scripts\env.ps1
```

Verify all tools are on PATH:
```powershell
clang --version       # should show ATfE arm-none-eabi target
clang-cl --version    # should show x86_64-pc-windows-msvc target
meson --version
python --version
rasc --version        # firmware builds only
```

### Linux

```bash
sudo apt install -y clang meson ninja-build git
```

No additional tools are needed for host unit tests on Linux. For firmware
builds on Linux, install RASC via the e2studio Linux package and add
`rfp-cli` from the Renesas Flash Programmer Linux distribution.

---

## ⚙️ Example Project Mode vs. Expert Mode

The PES supports two operating modes selected at build time by
`RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` in `rs_rtc_logger_cfg.h`.

| Mode | Value | Description |
|------|-------|-------------|
| Example Project | `1` (default) | PES auto-manages Open()/Start(); owns an internal FreeRTOS consumer task |
| Expert | `0` | Application owns the full lifecycle: Open() → Start() → EventTrigger() → Write() → Stop() → Close() |

The default in `rs_rtc_logger_cfg.h` is `1` (Example Project mode). This is
the correct production setting and is used by both firmware builds and host
unit tests without any override.

To switch the **host unit tests** to Expert mode, add
`'-DRS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0'` to the `c_args` line in
`meson/llvm-clangcl-msvc.ini`. Do not change `rs_rtc_logger_cfg.h` itself.

---

## ✅ Host Unit Tests

The host test build uses two isolation macros set globally in
`meson/llvm-clangcl-msvc.ini`:

- `RS_RTC_LOGGER_TEST_HOST_BUILD=1` — redirects `rs_rtc_logger_types.h` to
  use `tests/stubs/` headers instead of FSP and FreeRTOS headers
- `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` — not overridden; defaults to `1`
  (Example Project mode), matching the firmware

`rs_rtc_logger_hal.c` is excluded from the host build. Mock implementations
in `test_rs_rtc_logger_core.c` satisfy all HAL symbols at link time.

### Test groups

| Group | Tests | What is covered |
|-------|-------|----------------|
| `RtcFormat` | 14 | `format_timestamp()` — all 4 formats, brackets, boundaries, NULL/small-buffer guards |
| `RtcFormatLine` | 10 | `format_line()` — all terminators, time_valid=false, empty payload, truncation |
| `RtcLoggerLifecycle` | 14 | Open/Start/Stop/Close state machine, all error paths |
| `RtcLoggerBuffer` | 13 | EventTrigger/Dequeue, overflow policies, seq counter, STOPPED buffering, diagnostics |

### Windows

```powershell
meson setup build-clangcl --wipe --native-file .\meson\llvm-clangcl-msvc.ini
meson compile -C build-clangcl
meson test -C build-clangcl -v
```

### Linux

```bash
meson setup build-clang --wipe \
  --native-file ./meson/llvm-clangcl-ubuntu2404-ci.ini
meson compile -C build-clang
meson test -C build-clang -v
```

### Expected output

```
Unity test run 1 of 1
...................................................
-----------------------
51 Tests 0 Failures 0 Ignored
OK
```

---

## ✅ Fetch Subproject Dependencies

Run once before any firmware build (requires Git and internet access):

```powershell
meson subprojects download
```

Reset if corrupted:

```powershell
meson subprojects purge --confirm
meson subprojects download
```

---

## ✅ FPB-RA4E2 Firmware Build (Cortex-M33, R7FA4E2B93CFM)

```powershell
meson setup build-ra4e2 --wipe `
  --cross-file meson/llvm-cortex-m33.ini `
  -Dgen_board=ra4e2-fpb `
  -Dboard=ra4e2_fpb `
  -Dmcu=ra4e2 `
  -Dra_core=CM33 `
  -Dra_ordinal=1 `
  -Ddevice=RA4E2

meson compile -C build-ra4e2
```

### Outputs

```
build-ra4e2/firmware/rtc_event_logger_on_ra4e2_fpb_clang.elf
build-ra4e2/firmware/rtc_event_logger_on_ra4e2_fpb_clang.srec
build-ra4e2/firmware/rtc_event_logger_unittest_on_ra4e2_fpb_clang.elf
build-ra4e2/firmware/rtc_event_logger_unittest_on_ra4e2_fpb_clang.srec
```

---

## ✅ FPB-RA6E2 Firmware Build (Cortex-M33 + TrustZone, R7FA6E2BB3CFM)

Both RA4E2 and RA6E2 use the same Cortex-M33 cross file and flat-memory
SREC programming (no TrustZone bundle required).

```powershell
meson setup build-ra6e2 --wipe `
  --cross-file meson/llvm-cortex-m33.ini `
  -Dgen_board=ra6e2-fpb `
  -Dboard=ra6e2_fpb `
  -Dmcu=ra6e2 `
  -Dra_core=CM33 `
  -Dra_ordinal=1 `
  -Ddevice=RA6E2

meson compile -C build-ra6e2
```

### Outputs

```
build-ra6e2/firmware/rtc_event_logger_on_ra6e2_fpb_clang.elf
build-ra6e2/firmware/rtc_event_logger_on_ra6e2_fpb_clang.srec
build-ra6e2/firmware/rtc_event_logger_unittest_on_ra6e2_fpb_clang.elf
build-ra6e2/firmware/rtc_event_logger_unittest_on_ra6e2_fpb_clang.srec
```

---

## 🎯 Firmware build outputs explained

Each firmware build produces two independent SREC images. Flash one or the
other depending on what you want to run on the board.

**`rtc_event_logger_on_<board>_clang.srec` — Application firmware**
The full RTC Event Logger demo. Runs the FreeRTOS blinky thread, initialises
the RTC and UART via FSP drivers, and executes the `rs_rtc_logger_demo`
example. Flash this for normal operation or demonstration.

**`rtc_event_logger_unittest_on_<board>_clang.srec` — On-target Unity tests**
A separate firmware image that replaces `main.c` and `blinky_thread_entry`
with a Unity test runner. On boot it runs the `test_rs_rtc_logger_format` and
`test_rs_rtc_logger_core` test suites directly on the Cortex-M33 using the
real FSP RTC and UART drivers, then streams Unity pass/fail output over UART.
Flash this to verify the `rs_rtc_logger` library on real silicon.

### TDD workflow

```
1. meson test -C build-clangcl        # host tests: fast, no hardware
2. flash unittest SREC → read UART    # on-target tests: real drivers, real silicon
3. flash app SREC                     # demo / production firmware
```

---

## ✅ Flash Programming (rfp-cli)

### Program application firmware

```powershell
meson compile -C build-ra4e2 rfp_program   # RA4E2
meson compile -C build-ra6e2 rfp_program   # RA6E2
```

### Program Unity test firmware

```powershell
meson compile -C build-ra4e2 rfp_program_ut
meson compile -C build-ra6e2 rfp_program_ut
```

After flashing the UT image, open a serial terminal at **115200 8N1** on the
USB-VCOM port. Unity prints one line per test case:

```
===== UNITY LOG BEGIN ======
[PASS] RtcFormat.ISO8601WithBrackets
...
[PASS] RtcLoggerBuffer.SeqCountSetRestoresCounter
===== UNITY LOG END =====
```

### Utility targets

| Target | Effect |
|--------|--------|
| `rfp_reset` | Reset the target MCU |
| `rfp_erase_halt` | Erase all Flash, leave CPU halted |
| `rfp_check_blank` | Verify Flash is blank |

---

## ✅ RASC Code Regeneration

To force RASC to re-run without a full firmware rebuild:

```powershell
meson compile -C build-ra4e2 rasc_generate
meson compile -C build-ra6e2 rasc_generate
```

---

## ✅ Linker Behavior

The firmware uses `fsp.lld` (generated by RASC into `firmware/generated/`),
which internally includes `fsp_gen.lld` and `memory_regions.lld`.

⚠️ Pass only `fsp.lld` to the linker — do **not** pass the sub-files individually.

---

## 🔧 Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Malformed machine file` on setup | Check `c_args` in `llvm-clangcl-msvc.ini` is on a single line — the Meson ini parser does not accept multi-line arrays |
| `TaskHandle_t: unknown type name` | Ensure `tests/stubs/FreeRTOS.h` exists and `tests/stubs` is in `include_directories` in `tests/meson.build` |
| `__declspec(noreturn)` errors in Unity | Ensure `/clang:-D_Noreturn=` is present in `c_args` in `llvm-clangcl-msvc.ini` |
| `rs_rtc_logger_test_types.h: No such file` | Check `RS_RTC_LOGGER_TEST_HOST_BUILD=1` is in `c_args` in the native file |
| `bsp_cfg.h: No such file` (firmware) | Run `meson compile -C build-ra4e2 rasc_generate` first |
| `undefined ref to rs_rtc_logger_hal_rtc_open` (host) | Ensure `rs_rtc_logger_hal.c` is excluded from host test sources in `tests/meson.build` |
| `fsp.lld not found` | Run `rasc_generate` target before `meson compile` |
| Corrupted UART output / dropped events | Ensure v18+ of `rs_rtc_logger.c` is in use (TestIssue_02 fix) |
| `clang-cl cannot compile programs` on setup | Wrong `clang-cl.exe` on PATH — ensure generic LLVM appears last in `env.ps1` so it wins over ATfE for `clang-cl` |
| `cannot open crt0.o` at link time | `--sysroot` must not appear in `c_link_args` — see FSP v6.5.0 deviation note 6 |
| FreeRTOS symbols undefined at link | FSP subproject needs re-overlay — run `(Get-Content .\subprojects\packagefiles\fsp\meson.build) \| Set-Content .\subprojects\fsp\meson.build` |
| Clean rebuild | `meson setup build-dir --wipe ...` |

---

## FSP v6.5.0 migration — deviations from e2studio example projects

This section documents every structural difference discovered when migrating the
e2studio example projects (`RTC_Event_Logger_FPB_RA4E2_v15` and
`RTC_Event_Logger_FPB_RA6E2_v18`) to FSP v6.5.0 with the Meson build system.
It exists so that future changes to the e2studio reference projects can be
compared against this list to identify what needs updating here.

### 1. `hal_entry.c` no longer generated by RASC

**e2studio reference project (FSP v6.4.0):** RASC generated `hal_entry.c` into
`ra_gen/` as a thin wrapper calling `g_hal_init()` and starting the FreeRTOS
scheduler.

**FSP v6.5.0 behaviour:** RASC no longer generates `hal_entry.c`. The function
is declared in `ra_gen/hal_data.h` and must be provided by the application.

**This project's fix:** `src/hal_entry.c` was added to the project source tree.
It provides an empty `hal_entry()` body — the generated `main.c` calls
`g_hal_init()` and `vTaskStartScheduler()` directly, so `hal_entry()` is an
application hook that does nothing for this project.

### 2. `hal_warmstart.c` is now RASC-generated

**e2studio reference project (FSP v6.4.0):** `hal_warmstart.c` was a project
source file in `src/`, hand-edited by the developer.

**FSP v6.5.0 behaviour:** RASC now generates `hal_warmstart.c` into the project
directory (`src/hal_warmstart.c` inside the RASC projectdir). The content
matches the original project version.

**This project's fix:** `src/hal_warmstart.c` is no longer compiled from the
project `src/` tree. The RASC-generated copy is compiled instead (it is copied
to `firmware/generated/hal_warmstart.c` by `run_rasc.py`). Including both would
cause a duplicate `R_BSP_WarmStart` symbol at link time.

### 3. `blinky_thread.c` is a new RASC-generated file

**e2studio reference project (FSP v6.4.0):** No `blinky_thread.c` was generated
by RASC. The FreeRTOS thread was registered manually.

**FSP v6.5.0 behaviour:** RASC generates `ra_gen/blinky_thread.c` and
`ra_gen/blinky_thread.h`. `blinky_thread.c` defines `blinky_thread_create()`
which is called by the generated `main.c`. Without it the firmware fails to link
with `undefined symbol: blinky_thread_create`.

**This project's fix:** `blinky_thread.c` is copied from the RASC projectdir
(`ra_gen/blinky_thread.c`) to `firmware/generated/blinky_thread.c` by
`run_rasc.py` and declared as a `custom_target` output so Meson compiles it.

### 4. FreeRTOS kernel sources ship in the RASC projectdir, not the FSP git repo

**e2studio reference project:** FreeRTOS kernel `.c` files were compiled from
the FSP git repo at `ra/aws/FreeRTOS/FreeRTOS/Source/`. The FSP git tag
`v6.4.0` included them.

**FSP v6.5.0 behaviour:** The FSP v6.5.0 git repo (`github.com/renesas/fsp`) no
longer includes FreeRTOS source files. Instead, RASC copies them from the RASC
installer bundle into the `--projectdir` at `ra/aws/FreeRTOS/FreeRTOS/Source/`
when `--generate` is run headlessly.

**This project's fix:** `run_rasc.py` copies the six FreeRTOS kernel `.c` files
(`tasks.c`, `queue.c`, `list.c`, `timers.c`, `event_groups.c`,
`stream_buffer.c`) from the RASC projectdir to `firmware/generated/` with
renamed filenames (`ra_freertos_tasks.c` etc.) so they can be declared as
`custom_target` outputs and compiled by Meson without triggering setup-time
path validation.

### 5. `rm_comms_lock` and `rm_comms_uart` removed from FSP `src/`

**e2studio reference project (FSP v6.4.0):** `rm_comms_lock.c` and
`rm_comms_uart.c` existed under `ra/fsp/src/` and were compiled as part of the
FSP middleware layer.

**FSP v6.5.0 behaviour:** These source files were removed from `src/`. The
`rm_comms_lock.h` stub in `src/rm_comms_lock/` now contains a `#error` directive
instructing users to use the `inc/instances/` version instead. Attempting to
compile `rm_comms_lock.c` produces a hard error.

**This project's fix:** Both files are commented out in
`subprojects/packagefiles/fsp/meson.build` with a version note.

### 6. picolibc linker script conflict with FSP linker script

**e2studio reference project:** e2studio links with a custom GCC or LLVM
invocation that does not use the picolibc sysroot at link time.

**This project's issue:** The ATfE toolchain's `--sysroot` flag, when passed to
the linker, causes `ld.lld` to auto-include `picolibc.ld` from the sysroot's
multilib directory. This linker script contains `ENTRY(_start)` and references
`crt0.o` directly. The FSP already provides `Reset_Handler` as the entry point
via `startup.c` and its own linker script (`fsp_gen.lld`), making `picolibc.ld`
and `crt0.o` both redundant and conflicting.

**This project's fix:** `--sysroot` is present in `c_args` (needed at compile
time for headers such as `setjmp.h`, `string.h`) but is omitted from
`c_link_args`. The linker instead receives `-Wl,-L<multilib_lib_dir>` pointing
directly at the correct ATfE multilib directory
(`armv8m.main_hard_fp_unaligned_size/lib`) so `libc.a` and
`libclang_rt.builtins.a` can be found without triggering `picolibc.ld`
auto-inclusion. A weak `_start` stub in `src/crt0_stub.c` satisfies any
residual reference from `libc.a`.

### 7. RASC headless invocation: standalone `rasc.exe` required

**e2studio reference project:** Code generation is performed interactively
inside e2studio's GUI.

**This project's requirement:** Headless code generation via `--generate` is
only supported by the standalone RASC installer (`setup_fsp_v6_5_0_rasc_v2026-04.2.exe`).
The `e2studioc.exe` bundled with e2studio enters an interactive Eclipse socket
wait mode when given `--generate` and cannot run headlessly. The two
executables accept the same command-line arguments but behave differently.

### 8. `configuration.xml` must be migrated to FSP v6.5.0 before headless builds

RASC v6.5.0 will generate content for a v6.4.0 `configuration.xml` but exits
with a non-zero code and may produce incomplete output. The fix is to open each
board's `configuration.xml` in the RASC v6.5.0 GUI, accept the migration prompt,
click **Generate Project Content**, and save. The migrated files checked in to
`firmware/ra4e2-fpb/configuration.xml` and `firmware/ra6e2-fpb/configuration.xml`
were produced this way.

---

## Subproject management

After editing `subprojects/packagefiles/fsp/meson.build`, the change only takes
effect after a purge + re-download:

```powershell
meson subprojects purge --confirm fsp
meson subprojects download fsp
```

Then copy the packagefiles content to the live subproject:

```powershell
(Get-Content .\subprojects\packagefiles\fsp\meson.build) | Set-Content .\subprojects\fsp\meson.build
```

---

## ✅ Summary

| Build | Native/cross file | `-Dmcu` | `-Ddevice` | Mode |
|-------|-------------------|---------|-----------|------|
| Host unit tests | `llvm-clangcl-msvc.ini` | n/a | n/a | Example Project (default) |
| FPB-RA4E2 firmware | `llvm-cortex-m33.ini` | `ra4e2` | `RA4E2` | Example Project (default) |
| FPB-RA6E2 firmware | `llvm-cortex-m33.ini` | `ra6e2` | `RA6E2` | Example Project (default) |
