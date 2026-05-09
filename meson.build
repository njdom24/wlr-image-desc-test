project('wayland-test', 'c', default_options: ['c_std=gnu11'])

src = files(
  'src/main.c',
  'src/cm.c',
)

wl_dep = dependency('wayland-client')
wl_mod = import('wayland')

wp_dep = dependency('wayland-protocols')
wp_dir = wp_dep.get_variable('pkgdatadir')
color_management_xml = files(wp_dir / 'staging/color-management/color-management-v1.xml')
color_management = wl_mod.scan_xml(color_management_xml)

executable('main', src, color_management, dependencies: [wl_dep, meson.get_compiler('c').find_library('m')])