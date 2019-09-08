
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

# in, out, name
file_list = [
  ['db.sql', 'sql.h', 'sql'],

  ['tmpl/entry.tmpl', 'entry_tmpl.h', 'entry_tmpl'],
  ['tmpl/feed.tmpl', 'feed_tmpl.h', 'feed_tmpl'],
  ['tmpl/footer.tmpl', 'footer_tmpl.h', 'footer_tmpl'],
  ['tmpl/header.tmpl', 'header_tmpl.h', 'header_tmpl'],
  ['tmpl/index.tmpl', 'index_tmpl.h', 'index_tmpl'],
  ['tmpl/list.tmpl', 'list_tmpl.h', 'list_tmpl'],
  ['tmpl/page.tmpl', 'page_tmpl.h', 'page_tmpl'],
]
foreach p : file_list
  file_in = join_paths(meson.current_source_dir(), p[0])
  configure_file(
	output: p[1],
	command: [prog_python, bin2h_py, '-i', file_in, '-o', p[1], '-n', p[2]],
  )
endforeach


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

