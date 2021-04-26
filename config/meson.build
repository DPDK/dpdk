# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# check the OS is supported, rather than going any further
supported_exec_envs = ['freebsd', 'linux', 'windows']
exec_env = host_machine.system()
if not supported_exec_envs.contains(exec_env)
	error('unsupported system type "@0@"'.format(exec_env))
endif

# define a handy variable for checking which OS we have.
# gives us "is_windows", "is_freebsd" and "is_linux"
foreach env:supported_exec_envs
	set_variable('is_' + env, exec_env == env)
endforeach

# MS linker requires special treatment.
# TODO: use cc.get_linker_id() with Meson >= 0.54
is_ms_linker = is_windows and (cc.get_id() == 'clang')

# set the major version, which might be used by drivers and libraries
# depending on the configuration options
pver = meson.project_version().split('.')
major_version = '@0@.@1@'.format(pver.get(0), pver.get(1))
abi_version = run_command(find_program('cat', 'more'),
	abi_version_file).stdout().strip()

# Libraries have the abi_version as the filename extension
# and have the soname be all but the final part of the abi_version.
# e.g. v20.1 => librte_foo.so.20.1
#    sonames => librte_foo.so.20
so_version = abi_version.split('.')[0]

# extract all version information into the build configuration
dpdk_conf.set('RTE_VER_YEAR', pver.get(0).to_int())
dpdk_conf.set('RTE_VER_MONTH', pver.get(1).to_int())
if pver.get(2).contains('-rc')
	rc_ver = pver.get(2).split('-rc')
	dpdk_conf.set('RTE_VER_MINOR', rc_ver.get(0).to_int())
	dpdk_conf.set_quoted('RTE_VER_SUFFIX', '-rc')
	dpdk_conf.set('RTE_VER_RELEASE', rc_ver.get(1).to_int())
else
	dpdk_conf.set('RTE_VER_MINOR', pver.get(2).to_int())
	dpdk_conf.set_quoted('RTE_VER_SUFFIX', '')
# for actual, non-rc releases, set the release value to 99 to ensure releases
# have higher version numbers than their respective release candidates
	dpdk_conf.set('RTE_VER_RELEASE', 99)
endif

pmd_subdir_opt = get_option('drivers_install_subdir')
if pmd_subdir_opt.contains('<VERSION>')
	pmd_subdir_opt = abi_version.join(pmd_subdir_opt.split('<VERSION>'))
endif
driver_install_path = join_paths(get_option('libdir'), pmd_subdir_opt)
eal_pmd_path = join_paths(get_option('prefix'), driver_install_path)

# driver .so files often depend upon the bus drivers for their connect bus,
# e.g. ixgbe depends on librte_bus_pci. This means that the bus drivers need
# to be in the library path, so symlink the drivers from the main lib directory.
if not is_windows
	meson.add_install_script('../buildtools/symlink-drivers-solibs.sh',
			get_option('libdir'),
			pmd_subdir_opt)
elif meson.version().version_compare('>=0.55.0')
	# 0.55.0 is required to use external program with add_install_script
	meson.add_install_script(py3, '../buildtools/symlink-drivers-solibs.py',
			get_option('libdir'), pmd_subdir_opt, get_option('bindir'))
endif

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

dpdk_conf.set('RTE_ARCH_64', cc.sizeof('void *') == 8)
dpdk_conf.set('RTE_ARCH_32', cc.sizeof('void *') == 4)

if not is_windows
	add_project_link_arguments('-Wl,--no-as-needed', language: 'c')
endif

# use pthreads if available for the platform
if not is_windows
	add_project_link_arguments('-pthread', language: 'c')
	dpdk_extra_ldflags += '-pthread'
endif

# on some OS, maths functions are in a separate library
if cc.find_library('m', required : false).found()
	# some libs depend on maths lib
	add_project_link_arguments('-lm', language: 'c')
	dpdk_extra_ldflags += '-lm'
endif

if is_linux
	link_lib = 'dl'
else
	link_lib = ''
endif

# if link_lib is empty, do not add it to project properties
if link_lib != ''
	add_project_link_arguments('-l' + link_lib, language: 'c')
	dpdk_extra_ldflags += '-l' + link_lib
endif

# check for libraries used in multiple places in DPDK
has_libnuma = 0
numa_dep = cc.find_library('numa', required: false)
if numa_dep.found() and cc.has_header('numaif.h')
	dpdk_conf.set10('RTE_HAS_LIBNUMA', true)
	has_libnuma = 1
	add_project_link_arguments('-lnuma', language: 'c')
	dpdk_extra_ldflags += '-lnuma'
endif

has_libfdt = 0
fdt_dep = cc.find_library('libfdt', required: false)
if fdt_dep.found() and cc.has_header('fdt.h')
	dpdk_conf.set10('RTE_HAS_LIBFDT', true)
	has_libfdt = 1
	add_project_link_arguments('-lfdt', language: 'c')
	dpdk_extra_ldflags += '-lfdt'
endif

libexecinfo = cc.find_library('libexecinfo', required: false)
if libexecinfo.found() and cc.has_header('execinfo.h')
	add_project_link_arguments('-lexecinfo', language: 'c')
	dpdk_extra_ldflags += '-lexecinfo'
endif

# check for libbsd
libbsd = dependency('libbsd', required: false, method: 'pkg-config')
if libbsd.found()
	dpdk_conf.set('RTE_USE_LIBBSD', 1)
endif

# check for pcap
pcap_dep = dependency('libpcap', required: false, method: 'pkg-config')
if not pcap_dep.found()
	# pcap got a pkg-config file only in 1.9.0
	pcap_dep = cc.find_library('pcap', required: false)
endif
if pcap_dep.found() and cc.has_header('pcap.h', dependencies: pcap_dep)
	dpdk_conf.set('RTE_PORT_PCAP', 1)
	dpdk_extra_ldflags += '-lpcap'
endif

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang' and dpdk_conf.get('RTE_ARCH_64') == false
	atomic_dep = cc.find_library('atomic', required: true)
	add_project_link_arguments('-latomic', language: 'c')
	dpdk_extra_ldflags += '-latomic'
endif

# add -include rte_config to cflags
add_project_arguments('-include', 'rte_config.h', language: 'c')

# enable extra warnings and disable any unwanted warnings
warning_flags = [
	# -Wall is added by meson by default, so add -Wextra only
	'-Wextra',

	# additional warnings in alphabetical order
	'-Wcast-qual',
	'-Wdeprecated',
	'-Wformat',
	'-Wformat-nonliteral',
	'-Wformat-security',
	'-Wmissing-declarations',
	'-Wmissing-prototypes',
	'-Wnested-externs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Wsign-compare',
	'-Wstrict-prototypes',
	'-Wundef',
	'-Wwrite-strings',

	# globally disabled warnings
	'-Wno-address-of-packed-member',
	'-Wno-packed-not-aligned',
	'-Wno-missing-field-initializers'
]
if cc.get_id() == 'gcc' and cc.version().version_compare('>=10.0')
# FIXME: Bugzilla 396
	warning_flags += '-Wno-zero-length-bounds'
endif
if not dpdk_conf.get('RTE_ARCH_64')
# for 32-bit, don't warn about casting a 32-bit pointer to 64-bit int - it's fine!!
	warning_flags += '-Wno-pointer-to-int-cast'
endif
if cc.get_id() == 'intel'
	warning_ids = [181, 188, 2203, 2279, 2557, 3179, 3656]
	foreach i:warning_ids
		warning_flags += '-diag-disable=@0@'.format(i)
	endforeach
endif
foreach arg: warning_flags
	if cc.has_argument(arg)
		add_project_arguments(arg, language: 'c')
	endif
endforeach

# set other values pulled from the build options
dpdk_conf.set('RTE_MAX_LCORE', get_option('max_lcores'))
dpdk_conf.set('RTE_MAX_NUMA_NODES', get_option('max_numa_nodes'))
dpdk_conf.set('RTE_MAX_ETHPORTS', get_option('max_ethports'))
dpdk_conf.set('RTE_LIBEAL_USE_HPET', get_option('use_hpet'))
dpdk_conf.set('RTE_ENABLE_TRACE_FP', get_option('enable_trace_fp'))
# values which have defaults which may be overridden
dpdk_conf.set('RTE_MAX_VFIO_GROUPS', 64)
dpdk_conf.set('RTE_DRIVER_MEMPOOL_BUCKET_SIZE_KB', 64)
dpdk_conf.set('RTE_LIBRTE_DPAA2_USE_PHYS_IOVA', true)
if dpdk_conf.get('RTE_ARCH_64')
	dpdk_conf.set('RTE_MAX_MEM_MB', 524288)
else # for 32-bit we need smaller reserved memory areas
	dpdk_conf.set('RTE_MAX_MEM_MB', 2048)
endif


compile_time_cpuflags = []
subdir(arch_subdir)
dpdk_conf.set('RTE_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# set the install path for the drivers
dpdk_conf.set_quoted('RTE_EAL_PMD_PATH', eal_pmd_path)

install_headers(['rte_config.h', 'rte_compatibility_defines.h'],
		subdir: get_option('include_subdir_arch'))

# enable VFIO only if it is linux OS
dpdk_conf.set('RTE_EAL_VFIO', is_linux)

# specify -D_GNU_SOURCE unconditionally
add_project_arguments('-D_GNU_SOURCE', language: 'c')

# specify -D__BSD_VISIBLE for FreeBSD
if is_freebsd
	add_project_arguments('-D__BSD_VISIBLE', language: 'c')
endif

if is_windows
	# VirtualAlloc2() is available since Windows 10 / Server 2016.
	add_project_arguments('-D_WIN32_WINNT=0x0A00', language: 'c')

	# Use MinGW-w64 stdio, because DPDK assumes ANSI-compliant formatting.
	if cc.get_id() == 'gcc'
		add_project_arguments('-D__USE_MINGW_ANSI_STDIO', language: 'c')
	endif

	add_project_link_arguments('-lws2_32', language: 'c')

	# Contrary to docs, VirtualAlloc2() is exported by mincore.lib
	# in Windows SDK, while MinGW exports it by advapi32.a.
	if is_ms_linker
		add_project_link_arguments('-lmincore', language: 'c')
	endif

	add_project_link_arguments('-ladvapi32', '-lsetupapi', language: 'c')
	add_project_link_arguments('-ldbghelp', language: 'c')
endif

if get_option('b_lto')
	if cc.has_argument('-ffat-lto-objects')
		add_project_arguments('-ffat-lto-objects', language: 'c')
	else
		error('compiler does not support fat LTO objects - please turn LTO off')
	endif
	# workaround for gcc bug 81440
	if cc.get_id() == 'gcc' and cc.version().version_compare('<8.0')
		add_project_arguments('-Wno-lto-type-mismatch', language: 'c')
		add_project_link_arguments('-Wno-lto-type-mismatch', language: 'c')
	endif
endif

if get_option('default_library') == 'both'
	error( '''
    Unsupported value "both" for "default_library" option.

    NOTE: DPDK always builds both shared and static libraries.  Please set
    "default_library" to either "static" or "shared" to select default linkage
    for apps and any examples.''')
endif
