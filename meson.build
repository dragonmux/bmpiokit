# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2024 1BitSquared <info@1bitsquared.com>
# SPDX-FileContributor: Written by Rachel Mant <git@dragonmux.network>

project(
	'bmpiokit',
	'c',
	default_options: [
		'c_std=c11',
		'warning_level=3',
		'buildtype=release',
		'strip=true',
		'b_ndebug=if-release',
		'b_lto=true',
	],
	version: '0.0.1',
	meson_version: '>= 0.63',
	subproject_dir: 'deps',
)

cc = meson.get_compiler('c')

if get_option('b_lto') and host_machine.system() == 'darwin' and cc.get_id() == 'gcc'
	error(
		'GCC on macOS does not correctly implement support for LTO, please pass `-Db_lto=false` to Meson\n',
		'-> See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=111635 for details'
	)
endif


extendedWarnings = [
	'-Wdouble-promotion',
	'-Wformat=2',
	'-Wformat-overflow=2',
	'-Wformat-signedness',
	'-Wformat-truncation',
	'-Wnull-dereference',
	'-Wmissing-attributes',
	'-Wmissing-braces',
	'-Wsequence-point',
	'-Wreturn-type',
	'-Wunused',
	'-Wunused-local-typedefs',
	'-Wunused-const-variable=2',
	'-Wmaybe-uninitialized',
	'-Wunknown-pragmas',
	'-Wstrict-aliasing',
	'-Wstrict-overflow=3',
	'-Wstring-compare',
	'-Wstringop-overflow',
	'-Warith-conversion',
	'-Wvla-parameter',
	'-Wduplicated-branches',
	'-Wshadow=local',
	'-Wunsafe-loop-optimizations',
	'-Wbad-function-cast',
	'-Wcast-qual',
	'-Wcast-align=strict',
	'-Wcast-function-type',
	'-Wconversion',
	'-Wdangling-else',
	'-Wsign-conversion',
	'-Wfloat-conversion',
#	'-Wpacked',
#	'-Wpadded',
	'-Wredundant-decls',
#	'-Winline',
	'-Wvla',
	'-Wstack-protector',
	'-Wunsuffixed-float-constant',
	'-Wimplicit-fallthrough',
	'-Wundef',
	'-Wignored-qualifiers',
	'-Wshadow',
	'-Wswitch-enum',
	'-Wdefaulted-function-deleted',
	'-Wdeprecated-copy',
	'-ftrapv',
]

add_project_arguments(
	cc.get_supported_arguments(extendedWarnings),
	language: 'c'
)

dependencies = [
	dependency('appleframeworks', modules: ['IOKit', 'CoreFoundation'])
]

bmpiokitSrc = [
	'bmpiokit.c',
	'unicode.c',
]

executable(
	'bmpiokit',
	bmpiokitSrc,
	dependencies: dependencies,
	gnu_symbol_visibility: 'inlineshidden',
)
