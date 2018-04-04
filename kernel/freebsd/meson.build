# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

kmods = ['contigmem', 'nic_uio']

# for building kernel modules, we use kernel build system using make, as
# with Linux. We have a skeleton BSDmakefile, which pulls many of its
# values from the environment. Each module only has a single source file
# right now, which allows us to simplify things. We pull in the sourcer
# files from the individual meson.build files, and then use a custom
# target to call make, passing in the values as env parameters.
kmod_cflags = ['-I' + meson.build_root(),
		'-I' + join_paths(meson.source_root(), 'config'),
		'-include rte_config.h']

# to avoid warnings due to race conditions with creating the dev_if.h, etc.
# files, serialize the kernel module builds. Each module will depend on
# previous ones
built_kmods = []
foreach k:kmods
	subdir(k)
	built_kmods += custom_target(k,
		input: [files('BSDmakefile.meson'), sources],
		output: k + '.ko',
		command: ['make', '-f', '@INPUT0@',
			'KMOD_OBJDIR=@OUTDIR@',
			'KMOD_SRC=@INPUT1@',
			'KMOD=' + k,
			'KMOD_CFLAGS=' + ' '.join(kmod_cflags)],
		depends: built_kmods, # make each module depend on prev
		build_by_default: get_option('enable_kmods'))
endforeach
