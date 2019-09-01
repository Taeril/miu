project('miu', 'cpp',
  version: '0.1.0',
  license: 'MIT', 
  default_options: ['warning_level=3', 'cpp_std=c++17']
)

cpp = meson.get_compiler('cpp')
if cpp.get_argument_syntax() == 'gcc'
  add_project_arguments('-Wconversion', language: 'cpp')

  # TODO: handle linking requirements 
  # GNU implementation requires linking with -lstdc++fs
  add_project_link_arguments('-lstdc++fs', language : 'cpp')
  # LLVM implementation requires linking with -lc++fs
  #add_project_arguments(['-stdlib=libc++'], language : 'cpp')
  #add_project_link_arguments(['-stdlib=libc++','-lstdc++fs'], language : 'cpp')
endif

#fmt_dep = dependency('fmt')
# mkdir subprojects && meson wrap install fmt
fmt_dep = dependency('fmt', fallback: ['fmt', 'fmt_dep'])

kvc_dep = dependency('kvc', fallback: ['kvc', 'kvc_dep'])

mkd_dep = dependency('mkd', fallback: ['mkd', 'mkd_dep'])

tmpl_dep = dependency('tmpl', fallback: ['tmpl', 'tmpl_dep'])

message('libdir: ' + get_option('libdir'))

subdir('src')

