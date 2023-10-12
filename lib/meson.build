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
        'telemetry', # basic info querying
        'eal', # everything depends on eal
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

if is_ms_compiler
    libraries = [
            'log',
            'kvargs',
            'telemetry',
    ]
endif

optional_libs = [
        'bbdev',
        'bitratestats',
        'bpf',
        'cfgfile',
        'compressdev',
        'cryptodev',
        'dispatcher',
        'distributor',
        'dmadev',
        'efd',
        'eventdev',
        'gpudev',
        'graph',
        'gro',
        'gso',
        'ip_frag',
        'ipsec',
        'jobstats',
        'latencystats',
        'member',
        'metrics',
        'mldev',
        'node',
        'pcapng',
        'pdcp',
        'pdump',
        'pipeline',
        'port',
        'power',
        'rawdev',
        'regexdev',
        'reorder',
        'sched',
        'security',
        'table',
        'vhost',
]

always_enable = []
foreach l:libraries
    if not optional_libs.contains(l)
        always_enable += l
    endif
endforeach

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
    enable_libs += optional_libs
endif
enable_libs += always_enable

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
    # eal is standard dependency once built
    if dpdk_conf.has('RTE_LIB_EAL')
        deps += ['eal']
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
        set_variable(name.underscorify() + '_disable_reason', reason)
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

    # first build static lib
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

    if not use_function_versioning or is_windows
        # use pre-build objects to build shared lib
        sources = []
        objs += static_lib.extract_all_objects(recursive: false)
    else
        # for compat we need to rebuild with
        # RTE_BUILD_SHARED_LIB defined
        cflags += '-DRTE_BUILD_SHARED_LIB'
    endif

    version_map = '@0@/@1@/version.map'.format(meson.current_source_dir(), l)
    lk_deps = [version_map]

    if is_ms_linker
        def_file = custom_target(libname + '_def',
                command: [map_to_win_cmd, '@INPUT@', '@OUTPUT@'],
                input: version_map,
                output: '@0@_exports.def'.format(libname))
        lk_deps += [def_file]

        if is_ms_compiler
            lk_args = ['/def:' + def_file.full_path()]
            if meson.version().version_compare('<0.54.0')
                lk_args += ['/implib:lib\\librte_' + l + '.dll.a']
            endif
        else
            lk_args = ['-Wl,/def:' + def_file.full_path()]
            if meson.version().version_compare('<0.54.0')
                lk_args += ['-Wl,/implib:lib\\librte_' + l + '.dll.a']
            endif
        endif
    else
        if is_windows
            mingw_map = custom_target(libname + '_mingw',
                    command: [map_to_win_cmd, '@INPUT@', '@OUTPUT@'],
                    input: version_map,
                    output: '@0@_mingw.map'.format(libname))
            lk_deps += [mingw_map]

            lk_args = ['-Wl,--version-script=' + mingw_map.full_path()]
        else
            lk_args = ['-Wl,--version-script=' + version_map]
        endif
    endif

    if developer_mode and not is_windows
        # on unix systems check the output of the
        # check-symbols.sh script, using it as a
        # dependency of the .so build
        lk_deps += custom_target(name + '.sym_chk',
                command: [check_symbols,
                    version_map, '@INPUT@'],
                capture: true,
                input: static_lib,
                output: name + '.sym_chk')
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
