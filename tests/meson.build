# tests/meson.build

# Unity subproject — used for both host and firmware builds
# '-Dnoreturn=' redefines the C11 noreturn macro to nothing, preventing a
# clash between LLVM 22's stdnoreturn.h (#define noreturn _Noreturn) and the
# Windows SDK stdlib.h (__declspec(noreturn)) when building with clang-cl.
unity_sp = subproject('unity', default_options: [
  'extension_fixture=true',
  'werror=false',
])

unity_dep = unity_sp.get_variable('unity_dep')
unity_inc = unity_sp.get_variable('unity_inc')
unity_src = unity_sp.get_variable('unity_src')

# Host unit tests only — skip for cross (firmware) builds
if meson.is_cross_build()
  subdir_done()
endif

# ── Host-build compile flags ────────────────────────────────────────────────
# RS_RTC_LOGGER_TEST_HOST_BUILD=1 is set globally in the native file so it
# reaches every translation unit including subprojects.
#
# RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE is NOT overridden — the default of 1
# (Example Project mode) in rs_rtc_logger_cfg.h is used.  FreeRTOS symbols
# are satisfied by tests/stubs/FreeRTOS.h and tests/stubs/task.h.
host_test_cargs = []

# ── Source groups ──────────────────────────────────────────────────────────

# Pure formatting engine — no FSP / FreeRTOS dependency
format_sources = files(
  '../src/rs_rtc_logger/rs_rtc_logger_format.c',
)

# Core logger — HAL calls satisfied by mock implementations in test_rs_rtc_logger_core.c
core_sources = files(
  '../src/rs_rtc_logger/rs_rtc_logger.c',
)

# Test files
test_format_sources = files(
  'test_main.c',
  'test_rs_rtc_logger_format.c',
  'test_rs_rtc_logger_core.c',
)

# ── Host test executable ──────────────────────────────────────────────────

unittest_on_pc_exe = executable(
  'unittest_on_pc',
  native: true,
  sources: [
    test_format_sources,
    format_sources,
    core_sources,
    # Note: rs_rtc_logger_hal.c is NOT included here; test_rs_rtc_logger_core.c
    # provides mock implementations of every rs_rtc_logger_hal_* function.
  ],
  include_directories: [
    unity_inc,
    include_directories('../src'),
    include_directories('stubs'),
  ],
  c_args: host_test_cargs,
  dependencies: [
    unity_dep,
  ],
)

test(
  meson.project_name() + '_unittests',
  unittest_on_pc_exe,
)
