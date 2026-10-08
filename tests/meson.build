# Unity for host tests
unity_sp = subproject('unity', default_options: [
  'extension_fixture=true',
  'werror=false',
])

unity_dep = unity_sp.get_variable('unity_dep')
unity_inc = unity_sp.get_variable('unity_inc')

# -----------------------------------------------------------------------------
# Unity (assumed subproject)
# -----------------------------------------------------------------------------

unity_sp = subproject('unity')
unity_dep = unity_sp.get_variable('unity_dep')

# -----------------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------------

test_inc = include_directories(
  '.',
  'stubs',
  '..'
)

test_inc_cross = include_directories(
  '.',
  'stubs',
)

# -----------------------------------------------------------------------------
# Stub / fake sources
# -----------------------------------------------------------------------------

fake_srcs = files(
  'stubs/fake_uart.c'
)

# -----------------------------------------------------------------------------
# Test executables
# -----------------------------------------------------------------------------

test_sources_host = files(
  'test_main.c',
  'test_picolibc_stdio_to_uart.c'
)

test_cross_sources = files(
  'test_main_firmware.c',
  'test_picolibc_stdio_to_uart.c'
)

test_cross_deps = declare_dependency(
  include_directories: test_inc_cross,
  sources: [test_cross_sources,fake_srcs],
  dependencies: [unity_dep, byteq_dep]
)

# Host unit tests only. Do not build tests in cross builds.
if meson.is_cross_build()
  subdir_done()
endif

test_exe = executable(
  meson.project_name() + '_unittests',
  sources : [fake_srcs,test_sources_host],
  include_directories : [
    test_inc,
    include_directories('../src')
  ],
  # IMPORTANT: Do NOT link byteq here
  dependencies : [
    unity_dep,
    byteq_host_dep,
  ],
)

test(
  meson.project_name() + '_unittests',
  test_exe,
)