project(
	'wav',
	'c',
	license: 'MIT',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_global_arguments(['-Wno-unused-parameter'], language: 'c')

cc = meson.get_compiler('c')
fftw = dependency('fftw3f')
math = cc.find_library('m')
pulse = dependency('libpulse')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')

subdir('protocol')

include_files = include_directories('include')
source_files = files(
	'src/audio.c',
	'src/buffer.c',
	'src/config.c',
	'src/event-loop.c',
	'src/main.c',
	'src/output.c',
	'src/render.c',
	'src/wayland.c'
)
executable(
	'wav',
	source_files,
	include_directories: include_files,
	dependencies: [
		client_protos,
		fftw,
		math,
		pulse,
		wayland_client
	]
)
