project(
  'picolibc_stdio',
  'c',
  default_options: [
    'c_std=c11',
    'warning_level=2'
  ]
)

# -------------------------------------------------------------------
# ByteQ dependency (from subproject)
# -------------------------------------------------------------------

byteq_dep = dependency('data_queues', fallback: ['pes-queues', 'byteq_dep'])
byteq_host_dep = dependency('data_queues', fallback: ['pes-queues', 'byteq_host_dep'])
# -------------------------------------------------------------------
# Common sources (your module only)
# -------------------------------------------------------------------

stdio_to_uart_src = files(
  'src/syscalls_picolib_stdio_to_uart.c'
)

stdio_to_uart_dep = declare_dependency(
  sources: stdio_to_uart_src,
  dependencies: byteq_dep
)

# -------------------------------------------------------------------
# Build example (cross only)
# -------------------------------------------------------------------

if meson.is_cross_build()
  fs = import('fs')

  # -------------------------------------------------------------------
  # Options
  # -------------------------------------------------------------------

  mcu      = get_option('mcu')
  board    = mcu + '_' + get_option('board')
  example  = get_option('example')
  ide = get_option('ide')
  example_name = example + '_on_' + get_option('board') + '_' + mcu

  if mcu == '' or board == '' or example == ''
    error('You must set -Dmcu= -Dboard= -Dexample=')
  endif

  # -------------------------------------------------------------------
  # Resolve project path
  # -------------------------------------------------------------------

  example_dir = join_paths(
    'examples',
    ide,
    example_name
  )

  if not fs.exists(example_dir)
    error('Example not found: ' + example_dir)
  endif

  subdir('tests')
  subdir(example_dir)
else
  subdir('tests')

  run_target(
    'dev_flow',
    command: [
      find_program('python'),
      join_paths(meson.project_source_root(), 'scripts','dev_flow.py'),
      join_paths(meson.project_source_root(), 'build-msvc'),
      join_paths(meson.project_source_root(), 'build-ra0l1'),
    ]
  )

endif
