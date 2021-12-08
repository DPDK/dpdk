# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdirs = ['kni']

# if we are cross-compiling we need kernel_dir specified
if get_option('kernel_dir') == '' and meson.is_cross_build()
	error('Need "kernel_dir" option for kmod compilation when cross-compiling')
endif

kernel_dir = get_option('kernel_dir')
if kernel_dir == ''
	# use default path for native builds
	kernel_version = run_command('uname', '-r').stdout().strip()
	kernel_dir = '/lib/modules/' + kernel_version
endif

kernel_source_dir = get_option('kernel_dir')
if kernel_source_dir == ''
	# use default path for native builds
	kernel_source_dir = '/lib/modules/' + kernel_version + '/source'
endif

# test running make in kernel directory, using "make kernelversion"
make_returncode = run_command('make', '-sC', kernel_dir + '/build',
		'kernelversion').returncode()
if make_returncode != 0
	error('Cannot compile kernel modules as requested - are kernel headers installed?')
endif

# DO ACTUAL MODULE BUILDING
foreach d:subdirs
	subdir(d)
endforeach
