project('gooroom-usb-control', 'c')

cc = meson.get_compiler('c')

gnome = import('gnome')
i18n = import('i18n')
    
gtk_dep = dependency('gtk+-3.0')
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
json_dep = dependency('json-c')

gooroom_usbctl_sources = [
  'src/gooroom-usbctl-application.c',
  'src/gooroom-usbctl-window.c',
  'src/gooroom-usbctl-util.c',
  'src/gooroom-usbctl-main.c'
]

gooroom_usbctl_sources += gnome.compile_resources(
  'gooroom-usbctl-resources',
  'resources/gooroom-usbctl.gresource.xml',
  source_dir: 'resources'
)

gooroom_usbctl = executable(
  'gooroom-usbctl',
  gooroom_usbctl_sources,
  dependencies: [gtk_dep, glib_dep, gio_dep, json_dep],
  install: true,
  install_dir: '/usr/bin'
)

po_dir = join_paths(meson.source_root(), 'po')

subdir('data')
subdir('po')
