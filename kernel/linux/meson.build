# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdirs = ['igb_uio', 'kni']

# if we are cross-compiling we need kernel_dir specified
if get_option('kernel_dir') == '' and meson.is_cross_build()
	warning('Need "kernel_dir" option for kmod compilation when cross-compiling')
	subdir_done()
endif

kernel_dir = get_option('kernel_dir')
if kernel_dir == ''
	# use default path for native builds
	kernel_version = run_command('uname', '-r').stdout().strip()
	kernel_dir = '/lib/modules/' + kernel_version + '/build'
endif

# test running make in kernel directory, using "make kernelversion"
make_returncode = run_command('make', '-sC', kernel_dir,
		'kernelversion').returncode()
if make_returncode != 0
	warning('Cannot compile kernel modules as requested - are kernel headers installed?')
	subdir_done()
endif

# DO ACTUAL MODULE BUILDING
foreach d:subdirs
	subdir(d)
endforeach
