# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# Defines the order of dependencies evaluation
subdirs = [
        'common',
        'bus',
        'common/cnxk',    # depends on bus.
        'common/mlx5',    # depends on bus.
        'common/qat',     # depends on bus.
        'common/sfc_efx', # depends on bus.
        'mempool',        # depends on common and bus.
        'net',            # depends on common, bus, mempool
        'raw',            # depends on common, bus and net.
        'crypto',         # depends on common, bus and mempool (net in future).
        'compress',       # depends on common, bus, mempool.
        'regex',          # depends on common, bus, regexdev.
        'vdpa',           # depends on common, bus and mempool.
        'event',          # depends on common, bus, mempool and net.
        'baseband',       # depends on common and bus.
]

if meson.is_cross_build()
    disable_drivers += ',' + meson.get_cross_property('disable_drivers', '')
    enable_drivers += ',' + meson.get_cross_property('enable_drivers', '')
endif

# add cmdline disabled drivers and meson disabled drivers together
disable_drivers += ',' + get_option('disable_drivers')
disable_drivers = run_command(list_dir_globs, disable_drivers, check: true).stdout().split()

# add cmdline enabled drivers and meson enabled drivers together
enable_drivers = ',' + get_option('enable_drivers')
enable_drivers = run_command(list_dir_globs, enable_drivers, check: true).stdout().split()
if enable_drivers.length() == 0
    enable_drivers = run_command(list_dir_globs, '*/*', check: true).stdout().split()
endif

# these drivers must always be enabled, otherwise the build breaks
always_enable = ['bus/pci', 'bus/vdev']
enable_drivers += always_enable

default_cflags = machine_args
default_cflags += ['-DALLOW_EXPERIMENTAL_API']
default_cflags += ['-DALLOW_INTERNAL_API']

if cc.has_argument('-Wno-format-truncation')
    default_cflags += '-Wno-format-truncation'
endif

foreach subpath:subdirs
    drivers = []
    std_deps = []
    log_prefix = ''

    # subpath can be either "class" or "class/driver"
    if subpath.contains('/')
        driver_path = subpath.split('/')
        class = driver_path[0]
        drivers += driver_path[1]
    else
        class = subpath
        subdir(class)
    endif

    # save class name on first occurrence
    if not dpdk_driver_classes.contains(class)
        dpdk_driver_classes += class
    endif
    # get already enabled drivers of the same class
    enabled_drivers = get_variable(class + '_drivers', [])

    # default log prefix can be defined per class
    if log_prefix == ''
        # default log name is pmd.class.driver
        log_prefix = 'pmd.' + class
    endif

    foreach drv:drivers
        drv_path = join_paths(class, drv)

        # set up empty variables used for build
        build = true # set to false to disable, e.g. missing deps
        reason = '<unknown reason>' # set if build == false to explain
        name = drv
        sources = []
        headers = []
        objs = []
        cflags = default_cflags
        includes = [include_directories(drv_path)]
        # set up internal deps. Drivers can append/override as necessary
        deps = std_deps
        # ext_deps: Stores external library dependency got
        # using dependency() (preferred) or find_library().
        # For the find_library() case (but not with dependency()) we also
        # need to specify the "-l" flags in pkgconfig_extra_libs variable
        # too, so that it can be reflected in the pkgconfig output for
        # static builds.
        ext_deps = []
        pkgconfig_extra_libs = []

        if not enable_drivers.contains(drv_path)
            build = false
            reason = 'not in enabled drivers build config'
        elif disable_drivers.contains(drv_path)
            if always_enable.contains(drv_path)
                message('Driver @0@ cannot be disabled, not disabling.'.format(drv_path))
            else
                build = false
                reason = 'explicitly disabled via build config'
            endif
        endif

        if build
            # pull in driver directory which should update all the local variables
            subdir(drv_path)

            # get dependency objs from strings
            shared_deps = ext_deps
            static_deps = ext_deps
            foreach d:deps
                if not build
                    break
                endif
                if not is_variable('shared_rte_' + d)
                    build = false
                    reason = 'missing internal dependency, "@0@"'.format(d)
                    message('Disabling @1@ [@2@]: missing internal dependency "@0@"'
                            .format(d, name, 'drivers/' + drv_path))
                else
                    shared_deps += [get_variable('shared_rte_' + d)]
                    static_deps += [get_variable('static_rte_' + d)]
                endif
            endforeach
        endif

        if not build
            # some driver directories are placeholders which
            # are never built, so we allow suppression of the
            # component disable printout in those cases
            if reason != ''
                dpdk_drvs_disabled += drv_path
                set_variable(drv_path.underscorify() + '_disable_reason', reason)
            endif
            continue
        endif

        enabled_drivers += name
        lib_name = '_'.join(['rte', class, name])
        cflags += '-DRTE_LOG_DEFAULT_LOGTYPE=' + '.'.join([log_prefix, name])
        dpdk_conf.set(lib_name.to_upper(), 1)

        dpdk_extra_ldflags += pkgconfig_extra_libs

        install_headers(headers)

        # generate pmdinfo sources by building a temporary
        # lib and then running pmdinfogen on the contents of
        # that lib. The final lib reuses the object files and
        # adds in the new source file.
        out_filename = lib_name + '.pmd.c'
        tmp_lib = static_library('tmp_' + lib_name, sources,
                include_directories: includes,
                dependencies: static_deps,
                c_args: cflags)
        objs += tmp_lib.extract_all_objects(recursive: true)
        sources = custom_target(out_filename,
                command: [pmdinfo, tmp_lib.full_path(), '@OUTPUT@', pmdinfogen],
                output: out_filename,
                depends: [tmp_lib])

        # now build the static driver
        static_lib = static_library(lib_name,
                sources,
                objects: objs,
                include_directories: includes,
                dependencies: static_deps,
                c_args: cflags,
                install: true)

        # now build the shared driver
        version_map = '@0@/@1@/version.map'.format(meson.current_source_dir(), drv_path)
        implib = 'lib' + lib_name + '.dll.a'

        def_file = custom_target(lib_name + '_def',
                command: [map_to_win_cmd, '@INPUT@', '@OUTPUT@'],
                input: version_map,
                output: '@0@_exports.def'.format(lib_name))

        mingw_map = custom_target(lib_name + '_mingw',
                command: [map_to_win_cmd, '@INPUT@', '@OUTPUT@'],
                input: version_map,
                output: '@0@_mingw.map'.format(lib_name))

        lk_deps = [version_map, def_file, mingw_map]
        if is_windows
            if is_ms_linker
                lk_args = ['-Wl,/def:' + def_file.full_path()]
                if meson.version().version_compare('<0.54.0')
                    lk_args += ['-Wl,/implib:drivers\\' + implib]
                endif
            else
                lk_args = ['-Wl,--version-script=' + mingw_map.full_path()]
            endif
        else
            lk_args = ['-Wl,--version-script=' + version_map]
            if developer_mode
                # on unix systems check the output of the
                # check-symbols.sh script, using it as a
                # dependency of the .so build
                lk_deps += custom_target(lib_name + '.sym_chk',
                        command: [check_symbols, version_map, '@INPUT@'],
                        capture: true,
                        input: static_lib,
                        output: lib_name + '.sym_chk')
            endif
        endif

        shared_lib = shared_library(lib_name, sources,
                objects: objs,
                include_directories: includes,
                dependencies: shared_deps,
                c_args: cflags,
                link_args: lk_args,
                link_depends: lk_deps,
                version: abi_version,
                soversion: so_version,
                install: true,
                install_dir: driver_install_path)

        # create a dependency object and add it to the global dictionary so
        # testpmd or other built-in apps can find it if necessary
        shared_dep = declare_dependency(link_with: shared_lib,
                include_directories: includes,
                dependencies: shared_deps)
        static_dep = declare_dependency(
                include_directories: includes,
                dependencies: static_deps)

        dpdk_drivers += static_lib

        set_variable('shared_@0@'.format(lib_name), shared_dep)
        set_variable('static_@0@'.format(lib_name), static_dep)
        dependency_name = ''.join(lib_name.split('rte_'))
        if developer_mode
            message('drivers/@0@: Defining dependency "@1@"'.format(
                    drv_path, dependency_name))
        endif
    endforeach

    set_variable(class + '_drivers', enabled_drivers)
endforeach
