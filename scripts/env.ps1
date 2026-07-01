# env.ps1 — Set up PATH for RTC Event Logger Meson builds on Windows.
# Run this at the start of every build session: . .\scripts\env.ps1

# PATH prepend order matters: last prepended = first searched.
# ATfE is prepended first (lower priority) so generic LLVM wins for clang-cl.exe.
# Generic LLVM is prepended last (higher priority) so clang-cl.exe resolves to
# x86_64-pc-windows-msvc (needed for host unit tests), not ATfE aarch64.

# LLVM ATfE (Arm Toolchain for Embedded) — bundled with e2studio FSP v6.4.0.
# Provides clang.exe + bare-metal sysroot for Cortex-M33 firmware cross builds.
# Added first so generic LLVM takes priority for clang-cl.exe.
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Renesas\RA\e2studio_v2025-12_fsp_v6.4.0\toolchains\llvm_arm\ATfE-21.1.1-Windows-x86_64\bin;" + $env:PATH

# LLVM generic — provides clang-cl.exe (x86_64) for host unit tests.
# Added last so it appears before ATfE in PATH and wins for clang-cl.exe.
$env:PATH = "C:\Program Files\LLVM\bin;" + $env:PATH

# Python Scripts — provides meson.exe and ninja.exe (installed via pip).
# The path matches the Python 3.11 installation seen in previous build logs.
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Python\Python311\Scripts;" + $env:PATH
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Python\Python311;" + $env:PATH

# e2studio — provides e2studioc.exe (for general use).
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Renesas\RA\e2studio_v2025-12_fsp_v6.4.0\eclipse;" + $env:PATH

# Standalone RASC — provides rasc.exe for headless FSP code generation.
# Installed from setup_fsp_v6_5_0_rasc_v2026-04.2.exe
$env:PATH = "C:\Renesas\RA\sc_v2026-04.2_fsp_v6.5.0\eclipse;" + $env:PATH

# Renesas Flash Programmer CLI — provides rfp-cli.exe for flashing over JLink/SWD.
# Download: https://www.renesas.com/en/software-tool/renesas-flash-programmer-programming-gui
# Update the path below to match your installation.
$env:PATH = "Path\to\Renesas Flash Programmer V3;" + $env:PATH
