# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation


# process all libraries equally, as far as possible
# "core" libs first, then others alphabetically as far as possible
# NOTE: for speed of meson runs, the dependencies in the subdirectories
# sometimes skip deps that would be implied by others, e.g. if mempool is
# given as a dep, no need to mention ring. This is especially true for the
# core libs which are widely reused, so their deps are kept to a minimum.
libraries = [
        'log',
        'kvargs', # eal depends on kvargs
        'argparse',
        'telemetry', # basic info querying
        'pmu',
        'eal', # everything depends on eal
        'ptr_compress',
        'ring',
        'rcu', # rcu depends on ring
        'mempool',
        'mbuf',
        'net',
        'meter',
        'ethdev',
        'pci', # core
        'cmdline',
        'metrics', # bitrate/latency stats depends on this
        'hash',    # efd depends on this
        'timer',   # eventdev depends on this
        'acl',
        'bbdev',
        'bitratestats',
        'bpf',
        'cfgfile',
        'compressdev',
        'cryptodev',
        'distributor',
        'dmadev',  # eventdev depends on this
        'efd',
        'eventdev',
        'dispatcher', # dispatcher depends on eventdev
        'gpudev',
        'gro',
        'gso',
        'ip_frag',
        'jobstats',
        'latencystats',
        'lpm',
        'member',
        'pcapng',
        'power',
        'rawdev',
        'regexdev',
        'mldev',
        'rib',
        'reorder',
        'sched',
        'security',
        'stack',
        'vhost',
        'ipsec', # ipsec lib depends on net, crypto and security
        'pdcp', # pdcp lib depends on crypto and security
        'fib', #fib lib depends on rib
        'port', # pkt framework libs which use other libs from above
        'pdump', # pdump lib depends on bpf
        'table',
        'pipeline',
        'graph',
        'node',
]

always_enable = [
        'cmdline',
        'eal',
        'ethdev',
        'hash',
        'kvargs',
        'log',
        'mbuf',
        'mempool',
        'meter',
        'net',
        'pci',
        'rcu',
        'ring',
        'stack',
        'telemetry',
]

enable_deprecated_libs = []
foreach l:run_command(list_dir_globs, get_option('enable_deprecated_libs'),
        check: true).stdout().split()
    if not dpdk_libs_deprecated.contains(l)
        continue
    endif
    enable_deprecated_libs += l
endforeach

disable_libs = run_command(list_dir_globs, get_option('disable_libs'), check: true).stdout().split()

enable_libs = run_command(list_dir_globs, get_option('enable_libs'), check: true).stdout().split()
require_libs = true
if enable_libs.length() == 0
    require_libs = false
    enable_libs = libraries
else
    enable_libs += always_enable
endif

default_cflags = machine_args
default_cflags += ['-DALLOW_EXPERIMENTAL_API']
default_cflags += ['-DALLOW_INTERNAL_API']

if cc.has_argument('-Wno-format-truncation')
    default_cflags += '-Wno-format-truncation'
endif

foreach l:libraries
    build = true
    reason = '<unknown reason>' # set if build == false to explain why
    name = l
    use_function_versioning = false
    annotate_locks = true
    sources = []
    sources_avx2 = []
    sources_avx512 = []
    cflags_avx512 = [] # extra cflags for the avx512 code, e.g. extra avx512 feature flags
    headers = []
    indirect_headers = [] # public headers not directly included by apps
    driver_sdk_headers = [] # public headers included by drivers
    includes = []
    cflags = default_cflags
    objs = [] # other object files to link against, used e.g. for
              # instruction-set optimized versions of code

    # use "deps" for internal DPDK dependencies, and "ext_deps" for
    # external package/library requirements
    ext_deps = []
    deps = []
    if dpdk_conf.has('RTE_LIB_EAL')
        # eal is standard dependency once built
        deps += ['eal']
    else
        # otherwise, make private headers available (like eal_export.h)
        includes += include_directories('eal/common')
    endif

    if dpdk_libs_deprecated.contains(l)
        if not enable_deprecated_libs.contains(l)
            build = false
            reason = 'not in enabled deprecated libraries build config'
        else
            warning('Enabling deprecated library, "@0@"'.format(l))
        endif
    elif not enable_libs.contains(l)
        build = false
        reason = 'not in enabled libraries build config'
    elif disable_libs.contains(l)
        if always_enable.contains(l)
            warning('Cannot disable mandatory library "@0@"'.format(l))
        else
            build = false
            reason = 'explicitly disabled via build config'
        endif
    endif

    if build
        subdir(l)
        if not build and require_libs
            error('Cannot build explicitly requested lib "@0@".\n'.format(name)
                    +'\tReason: ' + reason)
        endif
    endif
    if name != l
        warning('Library name, "@0@", and directory name, "@1@", do not match'.format(name, l))
    endif

    shared_deps = ext_deps
    static_deps = ext_deps
    foreach d:deps
        if not build
            break
        endif
        if not is_variable('shared_rte_' + d)
            build = false
            reason = 'missing internal dependency, "@0@"'.format(d)
            if dpdk_libs_deprecated.contains(d)
                reason += ' (deprecated lib)'
            endif
            message('Disabling @1@ [@2@]: missing internal dependency "@0@"'
                    .format(d, name, 'lib/' + l))
            if require_libs
                error('Cannot build explicitly requested lib "@0@".\n'.format(name)
                        + '\tPlease add missing dependency "@0@" to "enable_libs" option'.format(d))
            endif
        else
            shared_deps += [get_variable('shared_rte_' + d)]
            static_deps += [get_variable('static_rte_' + d)]
        endif
    endforeach

    if not build
        dpdk_libs_disabled += name
        set_variable('lib_' + name.underscorify() + '_disable_reason', reason)
        continue
    endif

    dpdk_libs_enabled += name
    dpdk_conf.set('RTE_LIB_' + name.to_upper(), 1)
    install_headers(headers)
    install_headers(indirect_headers)
    if get_option('enable_driver_sdk')
        install_headers(driver_sdk_headers)
    endif
    dpdk_chkinc_headers += headers
    dpdk_chkinc_headers += driver_sdk_headers

    libname = 'rte_' + name
    includes += include_directories(l)
    dpdk_includes += include_directories(l)

    # special case for header only libraries
    if sources.length() == 0
        shared_dep = declare_dependency(include_directories: includes,
                dependencies: shared_deps)
        static_dep = declare_dependency(include_directories: includes,
                dependencies: static_deps)
        set_variable('shared_rte_' + name, shared_dep)
        set_variable('static_rte_' + name, static_dep)
        dpdk_shared_lib_deps += shared_dep
        dpdk_static_lib_deps += static_dep
        if developer_mode
            message('lib/@0@: Defining dependency "@1@"'.format(l, name))
        endif
        continue
    endif

    if developer_mode and is_windows and use_function_versioning
        message('@0@: Function versioning is not supported by Windows.'.format(name))
    endif

    if use_function_versioning
        cflags += '-DRTE_USE_FUNCTION_VERSIONING'
    endif
    cflags += '-DRTE_LOG_DEFAULT_LOGTYPE=lib.' + l
    if annotate_locks and cc.get_id() == 'clang' and cc.version().version_compare('>=3.5.0')
        cflags += '-DRTE_ANNOTATE_LOCKS'
        cflags += '-Wthread-safety'
    endif

    # handle avx2 and avx512 source files
    if arch_subdir == 'x86'
        if sources_avx2.length() > 0
            avx2_lib = static_library(libname + '_avx2_lib',
                    sources_avx2,
                    dependencies: static_deps,
                    include_directories: includes,
                    c_args: [cflags, cc_avx2_flags])
            objs += avx2_lib.extract_objects(sources_avx2)
        endif
        if sources_avx512.length() > 0 and cc_has_avx512
            cflags += '-DCC_AVX512_SUPPORT'
            avx512_args = [cflags, cflags_avx512, cc_avx512_flags]
            if not target_has_avx512 and cc.has_argument('-march=skylake-avx512')
                avx512_args += '-march=skylake-avx512'
                if cc.has_argument('-Wno-overriding-option')
                    avx512_args += '-Wno-overriding-option'
                endif
            endif
            avx512_lib = static_library(libname + '_avx512_lib',
                    sources_avx512,
                    dependencies: static_deps,
                    include_directories: includes,
                    c_args: avx512_args)
            objs += avx512_lib.extract_objects(sources_avx512)
        endif
    endif

    # build static lib
    static_lib = static_library(libname,
            sources,
            objects: objs,
            c_args: cflags,
            dependencies: static_deps,
            include_directories: includes,
            install: true)
    static_dep = declare_dependency(
            include_directories: includes,
            dependencies: static_deps)

    if is_ms_linker
        link_mode = 'mslinker'
    elif is_windows
        link_mode = 'mingw'
    else
        link_mode = 'gnu'
    endif
    version_map = custom_target(libname + '_map',
            command: [gen_version_map, '--linker', link_mode,
                      '--abi-version', abi_version_file, '--output', '@OUTPUT@',
                      '--source', '@INPUT@'],
            input: sources,
            output: '_'.join([name, 'exports.map']))
    lk_deps = [version_map]

    if is_ms_linker and is_ms_compiler
        lk_args = ['/def:' + version_map.full_path()]
    elif is_ms_linker
        lk_args = ['-Wl,/def:' + version_map.full_path()]
    else
        lk_args = ['-Wl,--version-script=' + version_map.full_path()]
    endif

    if developer_mode and not is_windows
        # on unix systems check the output of the
        # check-symbols.sh script, using it as a
        # dependency of the .so build
        lk_deps += custom_target(name + '.sym_chk',
                command: [check_symbols, version_map.full_path(), '@INPUT@'],
                capture: true,
                input: static_lib,
                output: name + '.sym_chk',
                depends: [version_map])
    endif

    if not use_function_versioning or is_windows
        # use pre-build objects to build shared lib
        sources = []
        objs += static_lib.extract_all_objects(recursive: false)
    else
        # for compat we need to rebuild with
        # RTE_BUILD_SHARED_LIB defined
        cflags += '-DRTE_BUILD_SHARED_LIB'
    endif

    shared_lib = shared_library(libname,
            sources,
            objects: objs,
            c_args: cflags,
            dependencies: shared_deps,
            include_directories: includes,
            link_args: lk_args,
            link_depends: lk_deps,
            version: abi_version,
            soversion: so_version,
            install: true)
    shared_dep = declare_dependency(link_with: shared_lib,
            include_directories: includes,
            dependencies: shared_deps)

    dpdk_libraries = [shared_lib] + dpdk_libraries
    dpdk_static_libraries = [static_lib] + dpdk_static_libraries

    set_variable('shared_rte_' + name, shared_dep)
    set_variable('static_rte_' + name, static_dep)
    dpdk_shared_lib_deps += shared_dep
    dpdk_static_lib_deps += static_dep
    if developer_mode
        message('lib/@0@: Defining dependency "@1@"'.format(l, name))
    endif
endforeach
