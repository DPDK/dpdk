# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# set the machine type and cflags for it
if meson.is_cross_build()
	machine = host_machine.cpu()
else
	machine = get_option('machine')
endif

# machine type 'default' is special, it defaults to the per arch agreed common
# minimal baseline needed for DPDK.
# That might not be the most optimized, but the most portable version while
# still being able to support the CPU features required for DPDK.
# This can be bumped up by the DPDK project, but it can never be an
# invariant like 'native'
if machine == 'default'
	if host_machine.cpu_family().startswith('x86')
		# matches the old pre-meson build systems default
		machine = 'corei7'
	elif host_machine.cpu_family().startswith('arm')
		machine = 'armv7-a'
	elif host_machine.cpu_family().startswith('aarch')
		# arm64 manages defaults in config/arm/meson.build
		machine = 'default'
	elif host_machine.cpu_family().startswith('ppc')
		machine = 'power8'
	endif
endif

dpdk_conf.set('RTE_MACHINE', machine)
machine_args = []

# ppc64 does not support -march= at all, use -mcpu and -mtune for that
if host_machine.cpu_family().startswith('ppc')
	machine_args += '-mcpu=' + machine
	machine_args += '-mtune=' + machine
else
	machine_args += '-march=' + machine
endif

toolchain = cc.get_id()
dpdk_conf.set_quoted('RTE_TOOLCHAIN', toolchain)
dpdk_conf.set('RTE_TOOLCHAIN_' + toolchain.to_upper(), 1)

add_project_link_arguments('-Wl,--no-as-needed', language: 'c')
dpdk_extra_ldflags += '-Wl,--no-as-needed'

# use pthreads
add_project_link_arguments('-pthread', language: 'c')
dpdk_extra_ldflags += '-pthread'

# some libs depend on maths lib
add_project_link_arguments('-lm', language: 'c')
dpdk_extra_ldflags += '-lm'

# for linux link against dl, for bsd execinfo
if host_machine.system() == 'linux'
	link_lib = 'dl'
else
	link_lib = 'execinfo'
endif
add_project_link_arguments('-l' + link_lib, language: 'c')
dpdk_extra_ldflags += '-l' + link_lib

# check for libraries used in multiple places in DPDK
has_libnuma = 0
numa_dep = cc.find_library('numa', required: false)
if numa_dep.found() and cc.has_header('numaif.h')
	dpdk_conf.set10('RTE_HAS_LIBNUMA', true)
	has_libnuma = 1
	add_project_link_arguments('-lnuma', language: 'c')
	dpdk_extra_ldflags += '-lnuma'
endif

# check for strlcpy
if host_machine.system() == 'linux' and cc.find_library('bsd',
		required: false).found() and cc.has_header('bsd/string.h')
	dpdk_conf.set('RTE_USE_LIBBSD', 1)
	add_project_link_arguments('-lbsd', language: 'c')
	dpdk_extra_ldflags += '-lbsd'
endif

# add -include rte_config to cflags
add_project_arguments('-include', 'rte_config.h', language: 'c')

# enable extra warnings and disable any unwanted warnings
warning_flags = [
	'-Wsign-compare',
	'-Wcast-qual',
	'-Wno-address-of-packed-member'
]
if cc.sizeof('void *') == 4
# for 32-bit, don't warn about casting a 32-bit pointer to 64-bit int - it's fine!!
	warning_flags += '-Wno-pointer-to-int-cast'
endif
foreach arg: warning_flags
	if cc.has_argument(arg)
		add_project_arguments(arg, language: 'c')
	endif
endforeach

# set other values pulled from the build options
dpdk_conf.set('RTE_MAX_LCORE', get_option('max_lcores'))
dpdk_conf.set('RTE_MAX_NUMA_NODES', get_option('max_numa_nodes'))
dpdk_conf.set('RTE_LIBEAL_USE_HPET', get_option('use_hpet'))
dpdk_conf.set('RTE_EAL_ALLOW_INV_SOCKET_ID', get_option('allow_invalid_socket_id'))
# values which have defaults which may be overridden
dpdk_conf.set('RTE_MAX_VFIO_GROUPS', 64)
dpdk_conf.set('RTE_DRIVER_MEMPOOL_BUCKET_SIZE_KB', 64)
dpdk_conf.set('RTE_LIBRTE_DPAA2_USE_PHYS_IOVA', true)

compile_time_cpuflags = []
if host_machine.cpu_family().startswith('x86')
	arch_subdir = 'x86'
elif host_machine.cpu_family().startswith('arm') or host_machine.cpu_family().startswith('aarch')
	arch_subdir = 'arm'
elif host_machine.cpu_family().startswith('ppc')
	arch_subdir = 'ppc_64'
endif
subdir(arch_subdir)
dpdk_conf.set('RTE_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# set the install path for the drivers
dpdk_conf.set_quoted('RTE_EAL_PMD_PATH', eal_pmd_path)

install_headers('rte_config.h', subdir: get_option('include_subdir_arch'))

# enable VFIO only if it is linux OS
dpdk_conf.set('RTE_EAL_VFIO', host_machine.system() == 'linux')
