# Release Notes and Adopting a Newer Release

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Current Version](#1-current-version)
2. [Adopting a Newer Release](#2-adopting-a-newer-release)
3. [What Typically Changes Between Releases](#3-what-typically-changes-between-releases)
4. [Recommended Merge Process](#4-recommended-merge-process)

---

## 1. Current Version

| Field | Value |
|---|---|
| Version | 1.0.0 |
| FSP Baseline | v6.5.0 |
| Primary Target | FPB-RA4E2 |
| Secondary Target | FPB-RA6E2 |

Query the running version at any time (including CLOSED state) via `rs_rtc_logger_VersionGet()` — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#5-configuration-and-query-functions).

---

## 2. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

---

## 3. What Typically Changes Between Releases

- Core source files (`rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`) may receive bug fixes, new features, or FSP version compatibility updates.
- Public headers (`rs_rtc_logger.h`, `rs_rtc_logger_types.h`) may add new public functions or extend existing structures. Additions are generally backward-compatible; removals or signature changes are not.
- `rs_rtc_logger_cfg.h` constants may gain new `#ifndef`-guarded defaults. Your existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases or corrected for hardware errata.

---

## 4. Recommended Merge Process

1. Diff the updated core files against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications.
3. Review the updated `rs_rtc_logger.h` and `rs_rtc_logger_types.h` for interface changes; update call sites in your application accordingly.
4. Rebuild and retest on target hardware.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading. If the FSP version changes between your baseline and the update, rerun the FSP Stack Configurator to regenerate `ra_gen/` content for the new FSP version, or re-run the Meson `rasc_generate` target — see [BUILD.md](BUILD.md#-rasc-code-regeneration).

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
