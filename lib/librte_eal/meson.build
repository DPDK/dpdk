# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# Custom EAL processing. EAL is complicated enough that it can't just
# have a straight list of headers and source files.
# Initially pull in common settings
eal_inc = [global_inc]
subdir('common') # defines common_sources, common_objs, etc.

# Now do OS/exec-env specific settings, including building kernel modules
# The <exec-env>/eal/meson.build file should define env_sources, etc.
if host_machine.system() == 'linux'
	dpdk_conf.set('RTE_EXEC_ENV_LINUXAPP', 1)
	subdir('linuxapp/eal')
	subdir('linuxapp/igb_uio')

elif host_machine.system() == 'freebsd'
	dpdk_conf.set('RTE_EXEC_ENV_BSDAPP', 1)
	subdir('bsdapp/eal')
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
	foreach k:kmods
		subdir(join_paths('bsdapp', k))
		custom_target(k,
			input: [files('bsdapp/BSDmakefile.meson'), sources],
			output: k + '.ko',
			command: ['make', '-f', '@INPUT0@',
				'KMOD_SRC=@INPUT1@',
				'KMOD=' + k,
				'KMOD_CFLAGS=' + ' '.join(kmod_cflags)],
			build_by_default: get_option('enable_kmods'))
	endforeach
else
	error('unsupported system type @0@'.format(hostmachine.system()))
endif

version = 6  # the version of the EAL API
allow_experimental_apis = true
deps += 'compat'
cflags += '-D_GNU_SOURCE'
sources = common_sources + env_sources
objs = common_objs + env_objs
headers = common_headers + env_headers
includes = eal_inc
