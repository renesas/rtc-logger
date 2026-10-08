project(
  'rtc_event_logger',
  'c',
  version: '1.0.0',
  default_options: [
    'c_std=c99',
    'warning_level=2',
  ]
)

# ------------------------------------------------------------
# Pure formatting engine — no FSP / FreeRTOS dependency.
# Compiles on host (used by host unit tests) and on target.
# ------------------------------------------------------------

rtc_format_sources = files(
  'src/rs_rtc_logger/rs_rtc_logger_format.c',
)

rtc_src_inc = include_directories('src')

# Stub include dir for host unit tests
stub_inc = include_directories('tests/stubs')

# Host-testable dep: format engine + stubs (no HAL, no RTOS)
rtc_format_host_dep = declare_dependency(
  sources: rtc_format_sources,
  include_directories: [rtc_src_inc, stub_inc],
)

# Full source set for firmware builds (core + HAL; board config added per-board)
rtc_core_sources = files(
  'src/rs_rtc_logger/rs_rtc_logger.c',
  'src/rs_rtc_logger/rs_rtc_logger_format.c',
  'src/rs_rtc_logger/rs_rtc_logger_hal.c',
  'src/rs_rtc_logger/rs_rtc_logger_stdio_console.c',
  'src/rs_rtc_logger/rs_rtc_logger_rtt_console.c',
)

if get_option('rtt_console_enable') == 1
  rtc_core_sources += files('src/SEGGER_RTT/SEGGER_RTT.c')
endif

# ------------------------------------------------------------
# Optional firmware build (cross builds only; not subproject)
# ------------------------------------------------------------

if (meson.is_cross_build()) and (false == meson.is_subproject())
  subdir('firmware')
endif

# ------------------------------------------------------------
# Tests (host only; cross builds skip inside subdir)
# ------------------------------------------------------------

subdir('tests')
