project(
	'wf-sound-control',
    'c',
	'cpp',
	version: '0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'cpp_std=c++11',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayland_client = dependency('wayland-client')
gtkmm          = dependency('gtkmm-3.0')
alsa           = dependency('alsa')
wayland_protos = dependency('wayland-protocols')


wayland_scanner = find_program('wayland-scanner')
wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)


wl_protos_src = []
wl_protos_client_src = []
wl_protos_headers = []

wf_shell = 'proto/wayfire-shell.xml'
wf_shell_src = wayland_scanner_code.process(wf_shell)
wf_shell_header = wayland_scanner_client.process(wf_shell)


add_project_arguments(['-w'], language: 'cpp')
executable('wf-sound-control', [wf_shell_src, wf_shell_header, 'main.cpp'],
        dependencies: [wayland_client, gtkmm, alsa],
        install: true)
