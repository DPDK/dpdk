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

exec_envs = {'freebsd': 0, 'linux': 1, 'windows': 2}
foreach env, id:exec_envs
    dpdk_conf.set('RTE_ENV_' + env.to_upper(), id)
    dpdk_conf.set10('RTE_EXEC_ENV_IS_' + env.to_upper(), (exec_env == env))
endforeach
dpdk_conf.set('RTE_EXEC_ENV', exec_envs[exec_env])
dpdk_conf.set('RTE_EXEC_ENV_' + exec_env.to_upper(), 1)

# MS linker requires special treatment.
# TODO: use cc.get_linker_id() with Meson >= 0.54
is_ms_compiler = is_windows and (cc.get_id() == 'msvc')
is_ms_linker = is_windows and (cc.get_id() == 'clang' or is_ms_compiler)

if is_ms_compiler
    # force the use of intrinsics the MSVC compiler (except x86)
    # does not support inline assembly
    dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)

    # suppress warnings raised for using standard library functions
    # the MSVC compiler regards as unsafe but are used by DPDK
    dpdk_conf.set('_CRT_SECURE_NO_WARNINGS', 1)

    # temporarily disable msvc specific warnings
    # - 4244 compiler detected conversion of integer to smaller type
    # - 4267 compiler detected conversion of size_t to smaller type
    add_project_arguments('/wd4244', '/wd4267', language: 'c')

    # enable non-locking atomic operations
    add_project_arguments('/experimental:c11atomics', language: 'c')

    # enable typeof operator
    add_project_arguments('/d1experimental:typeof', language: 'c')
endif

# set the major version, which might be used by drivers and libraries
# depending on the configuration options
pver = meson.project_version().split('.')
major_version = '@0@.@1@'.format(pver.get(0), pver.get(1))
abi_version = run_command(find_program('cat', 'more'), abi_version_file,
        check: true).stdout().strip()

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
            get_option('libdir'), pmd_subdir_opt)
elif meson.version().version_compare('>=0.55.0')
    # 0.55.0 is required to use external program with add_install_script
    meson.add_install_script(py3,
            files('../buildtools/symlink-drivers-solibs.py'),
            get_option('libdir'), pmd_subdir_opt, get_option('bindir'))
endif

# init disable/enable driver lists that will be populated in different places
disable_drivers = ''
enable_drivers = ''

platform = get_option('platform')

# set the cpu_instruction_set and cflags for it
if meson.is_cross_build()
    cpu_instruction_set = host_machine.cpu()
else
    cpu_instruction_set = get_option('cpu_instruction_set')
    machine = get_option('machine')
    if machine != 'auto'
        warning('The "machine" option is deprecated. ' +
                'Please use "cpu_instruction_set" instead.')
        if cpu_instruction_set != 'auto'
            error('Setting both "machine" and ' +
                '"cpu_instruction_set" is unsupported.')
        endif
        cpu_instruction_set = machine
        if cpu_instruction_set == 'default'
            cpu_instruction_set = 'generic'
        endif
    endif
    if platform == 'native'
        if cpu_instruction_set == 'auto'
            cpu_instruction_set = 'native'
        endif
    endif
endif

if platform == 'generic'
    if cpu_instruction_set == 'auto'
        cpu_instruction_set = 'generic'
    endif
endif

# cpu_instruction_set 'generic' is special, it selects the per arch agreed
# common minimal baseline needed for DPDK. cpu_instruction_set 'default' is
# also supported with the same meaning for backwards compatibility.
# That might not be the most optimized, but the most portable version while
# still being able to support the CPU features required for DPDK.
# This can be bumped up by the DPDK project, but it can never be an
# invariant like 'native'
if cpu_instruction_set == 'generic'
    if host_machine.cpu_family().startswith('x86')
        # matches the old pre-meson build systems generic cpu_instruction_set
        cpu_instruction_set = 'corei7'
    elif host_machine.cpu_family().startswith('arm')
        cpu_instruction_set = 'armv7-a'
    elif host_machine.cpu_family().startswith('aarch')
        # arm64 manages generic config in config/arm/meson.build
        cpu_instruction_set = 'generic'
    elif host_machine.cpu_family().startswith('ppc')
        cpu_instruction_set = 'power8'
    elif host_machine.cpu_family().startswith('riscv')
        cpu_instruction_set = 'rv64gc'
    endif
endif

dpdk_conf.set('RTE_MACHINE', cpu_instruction_set)
machine_args = []

if not is_ms_compiler
    # ppc64 does not support -march= at all, use -mcpu and -mtune for that
    if host_machine.cpu_family().startswith('ppc')
        machine_args += '-mcpu=' + cpu_instruction_set
        machine_args += '-mtune=' + cpu_instruction_set
        compiler_arch_support = cc.has_argument('-mcpu=' + cpu_instruction_set)
    else
        machine_args += '-march=' + cpu_instruction_set
        # arm manages generic/auto config in config/arm/meson.build
        if cpu_instruction_set != 'generic' and cpu_instruction_set != 'auto'
            compiler_arch_support = cc.has_argument('-march=' + cpu_instruction_set)
        else
            compiler_arch_support = true
        endif
    endif
    if not compiler_arch_support
        error('Compiler does not support "@0@" arch flag.'.format(cpu_instruction_set))
    endif
endif

toolchain = cc.get_id()
dpdk_conf.set_quoted('RTE_TOOLCHAIN', toolchain)
dpdk_conf.set('RTE_TOOLCHAIN_' + toolchain.to_upper().underscorify(), 1)

dpdk_conf.set('RTE_ARCH_64', cc.sizeof('void *') == 8)
dpdk_conf.set('RTE_ARCH_32', cc.sizeof('void *') == 4)

if not is_windows
    add_project_link_arguments('-Wl,--no-as-needed', language: 'c')
    if cc.has_link_argument('-Wl,--undefined-version')
        add_project_link_arguments('-Wl,--undefined-version', language: 'c')
    endif
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
has_libnuma = false
find_libnuma = true
if meson.is_cross_build() and not meson.get_cross_property('numa', true)
    # don't look for libnuma if explicitly disabled in cross build
    find_libnuma = false
endif
if find_libnuma
    numa_dep = cc.find_library('numa', required: false)
    if numa_dep.found() and cc.has_header('numaif.h')
        dpdk_conf.set10('RTE_HAS_LIBNUMA', true)
        has_libnuma = true
        add_project_link_arguments('-lnuma', language: 'c')
        dpdk_extra_ldflags += '-lnuma'
    endif
endif

has_libfdt = false
fdt_dep = cc.find_library('fdt', required: false)
if fdt_dep.found() and cc.has_header('fdt.h')
    dpdk_conf.set10('RTE_HAS_LIBFDT', true)
    has_libfdt = true
    add_project_link_arguments('-lfdt', language: 'c')
    dpdk_extra_ldflags += '-lfdt'
endif

libexecinfo = cc.find_library('execinfo', required: false)
if libexecinfo.found()
    add_project_link_arguments('-lexecinfo', language: 'c')
    dpdk_extra_ldflags += '-lexecinfo'
endif
dpdk_conf.set('RTE_BACKTRACE', cc.has_header('execinfo.h') or is_windows)

libarchive = dependency('libarchive', required: false, method: 'pkg-config')
if libarchive.found()
    dpdk_conf.set('RTE_HAS_LIBARCHIVE', 1)
endif

# check for libbsd
libbsd = dependency('libbsd', required: false, method: 'pkg-config')
if libbsd.found()
    dpdk_conf.set('RTE_USE_LIBBSD', 1)
endif

jansson_dep = dependency('jansson', required: false, method: 'pkg-config')
if jansson_dep.found()
    dpdk_conf.set('RTE_HAS_JANSSON', 1)
endif

# check for OpenSSL
openssl_dep = dependency('openssl', required: false, method: 'pkg-config')
if openssl_dep.found()
    dpdk_conf.set('RTE_HAS_OPENSSL', 1)
endif

# check for pcap
pcap_dep = dependency('libpcap', required: false, method: 'pkg-config')
pcap_lib = is_windows ? 'wpcap' : 'pcap'
if not pcap_dep.found()
    # pcap got a pkg-config file only in 1.9.0
    pcap_dep = cc.find_library(pcap_lib, required: false)
endif
if pcap_dep.found() and cc.has_header('pcap.h', dependencies: pcap_dep)
    dpdk_conf.set('RTE_HAS_LIBPCAP', 1)
    dpdk_extra_ldflags += '-l@0@'.format(pcap_lib)
endif

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang' and dpdk_conf.get('RTE_ARCH_64') == false
    atomic_dep = cc.find_library('atomic', required: true)
    add_project_link_arguments('-latomic', language: 'c')
    dpdk_extra_ldflags += '-latomic'
endif

# add -include rte_config to cflags
if is_ms_compiler
    add_project_arguments('/FI', 'rte_config.h', language: 'c')
else
    add_project_arguments('-include', 'rte_config.h', language: 'c')
endif

# enable extra warnings and disable any unwanted warnings
# -Wall is added by default at warning level 1, and -Wextra
# at warning level 2 (DPDK default)
warning_flags = [
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
        '-Wno-missing-field-initializers',
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
dpdk_conf.set('RTE_MAX_ETHPORTS', get_option('max_ethports'))
dpdk_conf.set('RTE_LIBEAL_USE_HPET', get_option('use_hpet'))
dpdk_conf.set('RTE_ENABLE_STDATOMIC', get_option('enable_stdatomic'))
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
if get_option('mbuf_refcnt_atomic')
    dpdk_conf.set('RTE_MBUF_REFCNT_ATOMIC', true)
endif
dpdk_conf.set10('RTE_IOVA_IN_MBUF', get_option('enable_iova_as_pa'))

compile_time_cpuflags = []
subdir(arch_subdir)
dpdk_conf.set('RTE_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# apply cross-specific options
if meson.is_cross_build()
    # configure RTE_MAX_LCORE and RTE_MAX_NUMA_NODES from cross file
    cross_max_lcores = meson.get_cross_property('max_lcores', 0)
    if cross_max_lcores != 0
        message('Setting RTE_MAX_LCORE from cross file')
        dpdk_conf.set('RTE_MAX_LCORE', cross_max_lcores)
    endif
    cross_max_numa_nodes = meson.get_cross_property('max_numa_nodes', 0)
    if cross_max_numa_nodes != 0
        message('Setting RTE_MAX_NUMA_NODES from cross file')
        dpdk_conf.set('RTE_MAX_NUMA_NODES', cross_max_numa_nodes)
    endif
endif

max_lcores = get_option('max_lcores')
if max_lcores == 'detect'
    # discovery makes sense only for non-cross builds
    if meson.is_cross_build()
        error('Discovery of max_lcores is not supported for cross-compilation.')
    endif
    # overwrite the default value with discovered values
    max_lcores = run_command(get_cpu_count_cmd, check: true).stdout().to_int()
    min_lcores = 2
    # DPDK must be built for at least 2 cores
    if max_lcores < min_lcores
        message('Found less than @0@ cores, building for @0@ cores'.format(min_lcores))
        max_lcores = min_lcores
    else
        message('Found @0@ cores'.format(max_lcores))
    endif
    dpdk_conf.set('RTE_MAX_LCORE', max_lcores)
elif max_lcores != 'default'
    # overwrite the default value from arch_subdir with user input
    dpdk_conf.set('RTE_MAX_LCORE', max_lcores.to_int())
endif

max_numa_nodes = get_option('max_numa_nodes')
if max_numa_nodes == 'detect'
    # discovery makes sense only for non-cross builds
    if meson.is_cross_build()
        error('Discovery of max_numa_nodes not supported for cross-compilation.')
    endif
    # overwrite the default value with discovered values
    max_numa_nodes = run_command(get_numa_count_cmd, check: true).stdout().to_int()
    message('Found @0@ numa nodes'.format(max_numa_nodes))
    dpdk_conf.set('RTE_MAX_NUMA_NODES', max_numa_nodes)
elif max_numa_nodes != 'default'
    # overwrite the default value from arch_subdir with user input
    dpdk_conf.set('RTE_MAX_NUMA_NODES', max_numa_nodes.to_int())
endif

# check that CPU and NUMA counts are set
if not dpdk_conf.has('RTE_MAX_LCORE')
    error('Number of CPU cores not specified.')
endif
if not dpdk_conf.has('RTE_MAX_NUMA_NODES')
    error('Number of NUMA nodes not specified.')
endif
if (is_linux and
        dpdk_conf.get('RTE_MAX_NUMA_NODES') > 1 and
        not meson.is_cross_build() and
        not has_libnuma)
    error('''
No NUMA library (development package) found, yet DPDK configured for multiple NUMA nodes.
Please install libnuma, or set 'max_numa_nodes' option to '1' to build without NUMA support.
''')
endif

# set the install path for the drivers
dpdk_conf.set_quoted('RTE_EAL_PMD_PATH', eal_pmd_path)

install_headers(['rte_config.h'],
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
    # VirtualAlloc2() is available since Windows 10 / Server 2019.
    # It's essential for EAL, so we don't support older versions.
    add_project_arguments('-D_WIN32_WINNT=0x0A00', language: 'c')

    # Use MinGW-w64 stdio, because DPDK assumes ANSI-compliant formatting.
    if cc.get_id() == 'gcc'
        add_project_arguments('-D__USE_MINGW_ANSI_STDIO', language: 'c')
    endif

    # Disable secure CRT deprecated warnings for clang
    if cc.get_id() == 'clang'
        add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'c')
    endif
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

if get_option('b_sanitize') == 'address' or get_option('b_sanitize') == 'address,undefined'
    if is_windows
        error('ASan is not supported on windows')
    endif

    if cc.get_id() == 'gcc'
        asan_dep = cc.find_library('asan', required: true)
        if (not cc.links('int main(int argc, char *argv[]) { return 0; }',
                dependencies: asan_dep))
            error('broken dependency, "libasan"')
        endif
        add_project_link_arguments('-lasan', language: 'c')
        dpdk_extra_ldflags += '-lasan'
    endif

    if is_linux and dpdk_conf.get('RTE_ARCH_64')
        dpdk_conf.set10('RTE_MALLOC_ASAN', true)
    endif
endif

if get_option('default_library') == 'both'
    error( '''
 Unsupported value "both" for "default_library" option.

 NOTE: DPDK always builds both shared and static libraries.  Please set
 "default_library" to either "static" or "shared" to select default linkage
 for apps and any examples.''')
endif
