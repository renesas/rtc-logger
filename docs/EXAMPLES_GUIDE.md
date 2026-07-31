# Running the Project and Expected Output

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).
> For building from the command line with Meson/LLVM instead of e2studio, see [BUILD.md](BUILD.md).

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Importing and Building](#2-importing-and-building)
3. [Expected Output — Example Project Mode (cold start)](#3-expected-output--example-project-mode-cold-start)
4. [Expected Output — Expert Mode (cold start)](#4-expected-output--expert-mode-cold-start)
5. [Expected Output — Warm Reset (both modes)](#5-expected-output--warm-reset-both-modes)
6. [Interaction Notes](#6-interaction-notes)

---

## 1. Prerequisites

- FPB-RA4E2 board connected via USB (provides both power and J-Link virtual COM)
- e2studio IDE with FSP v6.5.0 installed
- A serial terminal connected to the J-Link virtual COM port at **115200 baud, 8N1**

## 2. Importing and Building

1. Import the project into e2studio using **File → Import → Existing Projects into Workspace** and select the `RTC_Event_Logger_FPB_RA4E2` project directory (or `RTC_Event_Logger_FPB_RA6E2` for the secondary target).
2. The project is fully functional upon import. The `configuration.xml` is pre-configured; no FSP Stack Configurator re-run is required for the default preset path.
3. Build the project (**Project → Build All**). The default build target is **Example Project mode** (`RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1`).
4. Flash and debug using **Run → Debug As → Renesas GDB Hardware Debugging**.

To switch to **Expert mode**, set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` to `0` in `rs_rtc_logger_cfg.h` (or as a project-level compiler define `-DRS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0`) and rebuild.

> **Note:** If you download firmware via the e2studio debugger, see the e2studio reset-configuration section in [BUILD.md](BUILD.md#-e2studio-debugger--reset-configuration-for-rtc) — the default debugger reset behavior freezes the RTC counter, which affects timestamps shown below.

## 3. Expected Output — Example Project Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Example Project
RTC Event Logger control block: 992 bytes.
Creating RTC Event Logger consumer task...
Consumer task ready.
Triggering RTC Event Logger auto-init...
Waiting for RTC Event Logger auto-init to complete...
[0000-00-00 00:00:00] INIT #1
RTC Event Logger auto-init complete.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2026-07-30 10:38:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: 3
LOCO calibration set: trim +3, RFRL = 258.
Ready. Press S1 to log an event.
[2026-07-30 10:38:06] Event Detected #2
[2026-07-30 10:38:07] Event Detected #3
```

## 4. Expected Output — Expert Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Expert
Opening RTC Event Logger...
RTC Event Logger control block: 984 bytes.
RTC Event Logger open OK.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2026-07-30 10:54:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: 3
LOCO calibration set: trim +3, RFRL = 258.
Starting RTC Event Logger...
RTC Event Logger started.
Ready. Press S1 to log an event.
[2026-07-30 10:54:05] Event Detected #1
[2026-07-30 10:54:07] Event Detected #2
```

## 5. Expected Output — Warm Reset (both modes)

After pressing the reset button while USB power remains:

```
=========================================
  RTC Event Logger - FPB-RA4E2
  FSP v6.5.0 / FreeRTOS
=========================================
UART OK. Starting ...

*** RTC Event Logger Demo ***
Mode: Example Project
RTC Event Logger control block: 992 bytes.
Creating RTC Event Logger consumer task...
Consumer task ready.
Triggering RTC Event Logger auto-init...
Waiting for RTC Event Logger auto-init to complete...
[0000-00-00 00:00:00] INIT #1
RTC Event Logger auto-init complete.
Warm reset: seq counter restored.
Warm reset — RTC time retained.
Ready. Press S1 to log an event.
[2026-07-30 10:49:37] Event Detected #3
```

> Note: "Warm reset: seq counter restored." appears only when a valid sequence-counter
> backup exists from the previous session (i.e. at least one event was logged before the reset).

The sequence counter resumes from its pre-reset value and the RTC continues from its running time with no re-prompting for date/time.

## 6. Interaction Notes

- **Date/time prompt:** Enter in `YYYY-MM-DD HH:MM:SS` format. Press Enter with no input to skip; events will display `[0000-00-00 00:00:00]` and you will be re-prompted after each event.
- **LOCO trim prompt:** Enter a signed integer (e.g. `+17`, `-8`, `0`) to adjust the RFRL register. Press Enter with no input to skip and use the default RFRL = 255.
- **S1 button:** Press User Switch S1 on the FPB-RA4E2 board to trigger an event. A software debounce (default 150 ms, `RS_RTC_LOGGER_CFG_DEBOUNCE_MS`) suppresses spurious re-triggers from mechanical switch bounce — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application). Presses spaced closer than the debounce window will not produce a second logged event.

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
