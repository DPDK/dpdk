# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdirs = ['igb_uio']

WARN_CROSS_COMPILE='Need "kernel_dir" option for kmod compilation when cross-compiling'
WARN_NO_HEADERS='Cannot compile kernel modules as requested - are kernel headers installed?'

# if we are cross-compiling we need kernel_dir specified
# NOTE: warning() function only available from version 0.44 onwards
if get_option('kernel_dir') == '' and meson.is_cross_build()
	if meson.version().version_compare('>=0.44')
		warning(WARN_CROSS_COMPILE)
	else
		message('WARNING: ' + WARN_CROSS_COMPILE)
	endif
else

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
		if meson.version().version_compare('>=0.44')
			warning(WARN_NO_HEADERS)
		else
			message('WARNING: ' + WARN_NO_HEADERS)
		endif
	else # returncode == 0

# DO ACTUAL MODULE BUILDING
		foreach d:subdirs
			subdir(d)
		endforeach
	endif
endif
