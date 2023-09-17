vteditor_sources = [
  'main.c',
  'vteditor-application.c',
  'vteditor-window.c',
]

vteditor_deps = [
  dependency('gtk4'),
  libvtf_dep,
]

vteditor_sources += gnome.compile_resources('vteditor-resources',
  'vteditor.gresource.xml',
  c_name: 'vteditor'
)

executable('vteditor', vteditor_sources,
  dependencies: vteditor_deps,
       install: true,
)
