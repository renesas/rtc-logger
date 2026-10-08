# 📘 Developer Contribution Guide

This repository provides a reusable implementation of **picolibc stdio over UART** for Renesas RA devices, along with:

* ✅ e²studio example projects
* ✅ Host-based unit tests (Unity + stubs)
* ✅ Meson-based build system
* ✅ Reusable queue library (`pes-queues`)

This guide describes how to contribute in two primary areas:

1. ➕ Adding new example projects
2. 🔧 Adding new syscall implementations

## ✅ One-line Summary

> Contributors should develop in `src/`, validate with host tests, and propagate changes to example projects, ensuring both **unit correctness and target integration**.

***

# 🧭 Repository Model (Mental Model)

```
src/       → source of truth (shared logic)
tests/     → host validation (Unity + fakes)
examples/  → target-specific integration (e²studio)
subprojects/ → external reusable components
```

> **Key principle:**  
> Changes should originate in `src/`, validated in `tests/`, then propagated to `examples/`.

***

# ✅ Prerequisites

Before contributing:

* Meson + Ninja installed
* Python 3.x available
* LLVM Embedded Toolchain (for cross builds)
* Visual Studio (for host/MSVC testing)

***

# 🔄 Standard Developer Workflow

1. Modify source in `src/`
2. Run unit tests (host)
3. Sync to example projects
4. Build or validate example(s)

Use:

```bash
python scripts/dev_flow.py
```

***

# 🧪 Adding a New Syscall Implementation

This applies when adding a new feature such as:

* UART variants
* USB CDC
* semihosting
* buffered I/O enhancements

***

## ✅ Step 1 — Add your implementation

Create or extend:

```
src/syscalls_<feature>.c
```

Example:

```
src/syscalls_picolib_stdio_to_usb.c
```

***

## ✅ Step 2 — Design Guidelines

Follow these rules:

### ✅ Required

* Must be **platform-agnostic where possible**
* Must use **FSP APIs only at boundary layer**
* Must support:
  * `putc`
  * `getc`
  * `flush`

***

### ✅ Strongly recommended

* Reuse ByteQ (`r_byteq`) for buffering
* Avoid static global side-effects unless test-reset capable
* Provide:

```c
void <module>_test_reset(void);
```

***

## ✅ Step 3 — Add unit tests

Add:

```
tests/test_<feature>.c
```

Use:

* `fake_uart.c` (or create a new stub)
* Unity framework
* Direct inclusion of source file:

```c
#include "syscalls_<feature>.c"
```

***

## ✅ Step 4 — Update Meson tests

Modify:

```
tests/meson.build
```

Add your test file to the list.

***

## ✅ Step 5 — Run tests

```bash
meson test -C build-msvc --print-errorlogs
```

✅ All tests must pass before proceeding.

***

## ✅ Step 6 — Integrate with examples

Use the dev flow script:

```bash
python scripts/dev_flow.py
```

This copies:

* `syscalls_*.c`
* `r_byteq.c/h`

into example projects.

***

## ✅ Step 7 — Validate on hardware

Perform a cross build and run on target.

***

# ➕ Adding a New Example Project

Examples represent **real integration targets**.

***

## ✅ Step 1 — Create e²studio project

Under:

```
examples/e2studio/
```

Create:

```
<feature>_on_<board>_<mcu>
```

Example:

```
stdio_uart_on_fpb_ra4m1/
```

***

## ✅ Required contents

Your project must include:

* `ra_gen/` (generated code)
* `ra_cfg/`
* `src/`
* `ra/fsp/`
* `script/fsp.lld`

***

## ✅ Step 2 — Add `meson.build`

Each example must have its own `meson.build`.

✅ Use an existing example as a template  
✅ Keep structure consistent

***

## ✅ Step 3 — Update include paths

Ensure:

```meson
include_directories(
  ...
  'src',
  'ra_gen',
  ...
)
```

***

## ✅ Step 4 — Use shared logic

All examples must depend on:

```meson
dependencies: common_dep
```

This injects:

* `syscalls_*`
* ByteQ (`pes-queues`)

***

## ✅ Step 5 — Sync shared sources

Run:

```bash
python scripts/dev_flow.py
```

This ensures example contains:

* `syscalls_*.c`
* `r_byteq.c/h`

> e²studio projects require physical files → this step is intentional

***

## ✅ Step 6 — Validate build

```bash
meson setup build-<target> ...
meson compile -C build-<target>
```

***

# 🧱 Coding Guidelines

***

## ✅ General

* Use **C99/C11**
* Follow existing naming style:
  * `uart_*`
  * `jlob_*`
* Avoid dynamic allocation unless justified

***

## ✅ Error Handling

* Always return `fsp_err_t`
* Validate inputs:

```c
if (NULL == ...)
{
    return FSP_ERR_ASSERTION;
}
```

***

## ✅ Testability

Every module must:

* Be testable via host build
* Provide reset function
* Avoid hidden state

***

# 🧪 Testing Philosophy

***

## ✅ Host Testing (primary)

Uses:

* `fake_uart.c`
* Unity
* ByteQ in host mode

Goal:

> Validate logic independent of hardware

***

## ✅ Cross Testing (secondary)

* Validate integration with:
  * UART
  * interrupts
  * real timing

***

# 🔁 Subprojects

Located under:

```
subprojects/
```

| Component  | Purpose          |
| ---------- | ---------------- |
| cmsis      | ARM headers      |
| fsp        | Renesas platform |
| pes-queues | queue library    |
| unity      | test framework   |

***

## ✅ Rules

* Do not modify subprojects directly unless necessary
* Prefer wrapping via Meson dependencies

***

# ✅ Pull Request Checklist

Before submitting:

* [ ] Unit tests pass (`meson test`)
* [ ] No warnings introduced
* [ ] Example builds successfully
* [ ] `dev_flow.py` completes successfully
* [ ] Code follows repository conventions
* [ ] Documentation updated (if needed)

***


