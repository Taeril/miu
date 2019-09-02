
ver_arr = meson.project_version().split('.')
ver_data = configuration_data({
  'VERSION': meson.project_version(),
  'VERSION_MAJOR': ver_arr[0],
  'VERSION_MINOR': ver_arr[1],
  'VERSION_PATCH': ver_arr[2],
})
configure_file(
  input : 'version.hpp.in',
  output : 'version.hpp',
  configuration : ver_data
)


prog_python = import('python').find_installation('python3')
bin2h_py = join_paths(meson.current_source_dir(), 'bin2h.py')
db_sql = join_paths(meson.current_source_dir(), 'db.sql')
configure_file(
  output: 'sql.h',
  command: [prog_python, bin2h_py, '-i', db_sql, '-o', 'sql.h', '-n', 'sql'],
)


sources = files([
  'cache.cpp',
  'config.cpp',
  'app.cpp',
  'main.cpp',
])

miu_exe = executable('miu', sources,
  install : true,
  gnu_symbol_visibility : 'hidden',
  dependencies: [fmt_dep, kvc_dep, mkd_dep, tmpl_dep, sqlite_dep],
  include_directories: '.',
)

