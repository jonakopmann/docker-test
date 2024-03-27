project('ufo-test', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++2a'])


deps = [
  dependency('ufo'),
  dependency('OpenCL'),
]

executable('ufo-test',
           'main.cpp',
           dependencies : deps,
           install : true)
