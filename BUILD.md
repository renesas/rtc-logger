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
│   │   fault_handler.c
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

**2. LLVM (Arm Embedded Toolchain variant)**
Used for both host unit tests (`clang-cl`) and cross firmware builds
(`clang` targeting Cortex-M33). Use the Arm LLVM-ET-Arm build, not the
generic LLVM installer — it includes the bare-metal Arm libraries.
Download: https://github.com/ARM-software/LLVM-Embedded-Toolchain-for-Arm/releases
Pick: `LLVM-ET-Arm-<version>-Windows-x86_64.exe`
During install, add to PATH when prompted (or add manually afterwards):
`C:\Program Files\LLVM\bin`

Verify:
```powershell
clang --version
clang-cl --version
```

**3. Python 3**
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

**4. Git**
Required by Meson to fetch the `unity`, `fsp`, and `cmsis` subprojects
from GitHub on first build.
Download: https://git-scm.com/download/win

**5. RASC / e2studio** *(firmware builds only — not needed for unit tests)*
RASC is bundled inside e2studio and invoked headlessly by `scripts/run_rasc.py`
to regenerate `ra_gen` / `ra_cfg` from `configuration.xml`.
Download: https://www.renesas.com/en/software-tool/e2studio

The FSP+e2studio bundle installs `e2studio.exe` — there is no separate
`rasc.exe` in this package. The project uses `find_program('e2studio')` and
passes the same `--generate` headless arguments to it.

The expected install path is:
`C:\Users\<username>\AppData\Local\Programs\Renesas\RA\e2studio_v2025-12_fsp_v6.4.0\eclipse`

Update `scripts/env.ps1` with this path (replacing `MyPC` with your username).

Note: A standalone `rasc.exe` is available via the separate RASC installer on
the FSP GitHub releases page (`setup_fsp_v6_4_0_rasc_v2025-12.exe`). If you
install that instead, change `find_program('e2studio')` back to
`find_program('rasc')` in `firmware/generated/meson.build`.

**6. Renesas Flash Programmer V3** *(flashing only — not needed for building)*
Provides `rfp-cli.exe` used by the `rfp_program` and `rfp_program_ut` Meson
targets to flash firmware over JLink/SWD.
Download: https://www.renesas.com/en/software-tool/renesas-flash-programmer-programming-gui
Update `scripts/env.ps1` with the install path.

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

Verify all tools are on PATH:
```powershell
clang --version
clang-cl --version
meson --version
python --version
rasc --version        # firmware builds only
rfp-cli --version     # flashing only
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

```bash
meson subprojects download
```

Reset if corrupted:

```bash
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
| Clean rebuild | `meson setup build-dir --wipe ...` |

---

## 📝 Version Notes

### v18 — rs_rtc_logger.c (TestIssue_02 fix)

The RA6E2 project (v18) fixed a race condition in both operating modes:

An `EventTrigger()` ISR notification could wake `ulTaskNotifyTake()` before
the UART TX was complete, causing `line_buf` to be overwritten mid-transmission
and producing corrupted output with silently dropped events.

The consumer task now loops on `ulTaskNotifyTake()` until `p_ctrl->tx_complete`
is set by the UART HAL callback. This fix applies to both boards.

---

## ✅ Summary

| Build | Native/cross file | `-Dmcu` | `-Ddevice` | Mode |
|-------|-------------------|---------|-----------|------|
| Host unit tests | `llvm-clangcl-msvc.ini` | n/a | n/a | Example Project (default) |
| FPB-RA4E2 firmware | `llvm-cortex-m33.ini` | `ra4e2` | `RA4E2` | Example Project (default) |
| FPB-RA6E2 firmware | `llvm-cortex-m33.ini` | `ra6e2` | `RA6E2` | Example Project (default) |
