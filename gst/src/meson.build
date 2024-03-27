project('gst-test', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

deps = [
  dependency('gstreamer-1.0'),
  dependency('gstreamer-video-1.0'),
]

executable('gst-test',
           'main.cpp',
           dependencies : deps,
           install : true)
