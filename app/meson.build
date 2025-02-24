# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

disable_apps = ',' + get_option('disable_apps')
disable_apps = run_command(list_dir_globs, disable_apps, check: true).stdout().split()

enable_apps = ',' + get_option('enable_apps')
enable_apps = run_command(list_dir_globs, enable_apps, check: true).stdout().split()
require_apps = true
if enable_apps.length() == 0
    require_apps = false
    enable_apps = run_command(list_dir_globs, '*', check: true).stdout().split()
endif

apps = [
        'dumpcap',
        'graph',
        'pdump',
        'proc-info',
        'test-acl',
        'test-bbdev',
        'test-cmdline',
        'test-compress-perf',
        'test-crypto-perf',
        'test-dma-perf',
        'test-eventdev',
        'test-fib',
        'test-flow-perf',
        'test-gpudev',
        'test-mldev',
        'test-pipeline',
        'test-pmd',
        'test-regex',
        'test-sad',
        'test-security-perf',
]

if get_option('tests')
# build the auto test app if enabled.
    apps += 'test'
endif

default_cflags = machine_args + ['-DALLOW_EXPERIMENTAL_API']
default_ldflags = []
if get_option('default_library') == 'static' and not is_windows
    default_ldflags += ['-Wl,--export-dynamic']
endif

foreach app:apps
    name = app
    build = true
    reason = '<unknown reason>' # set if build == false to explain
    sources = []
    resources = []
    includes = []
    cflags = default_cflags
    ldflags = default_ldflags
    objs = [] # other object files to link against, used e.g. for
              # instruction-set optimized versions of code

    # use "deps" for internal DPDK dependencies, and "ext_deps" for
    # external package/library requirements
    ext_deps = []
    deps = []

    if not enable_apps.contains(app)
        build = false
        reason = 'not in enabled apps build config'
    elif disable_apps.contains(app)
        build = false
        reason = 'explicitly disabled via build config'
    endif

    if build
        subdir(name)
        if not build and require_apps
            error('Cannot build explicitly requested app "@0@".\n'.format(name)
                  + '\tReason: ' + reason)
        endif
    endif

    if build
        dep_objs = []
        foreach d:deps
            var_name = get_option('default_library') + '_rte_' + d
            if not is_variable(var_name)
                build = false
                reason = 'missing internal dependency, "@0@"'.format(d)
                message('Missing dependency "@0@" for app "@1@"'.format(d, name))
                if require_apps
                    error('Cannot build explicitly requested app "@0@".\n'.format(name)
                          + '\tPlease add missing dependency "@0@" to "enable_libs" option'.format(d))
                endif
                break
            endif
            dep_objs += [get_variable(var_name)]
        endforeach
    endif

    if not build
        if reason != ''
            dpdk_apps_disabled += app
            set_variable('app_' + app.underscorify() + '_disable_reason', reason)
        endif
        continue
    endif

    dpdk_apps_enabled += app
    link_libs = []
    if get_option('default_library') == 'static'
        link_libs = dpdk_static_libraries + dpdk_drivers
    endif

    exec = executable('dpdk-' + name,
            [ sources, resources ],
            c_args: cflags,
            link_args: ldflags,
            link_whole: link_libs,
            dependencies: ext_deps + dep_objs,
            include_directories: includes,
            install_rpath: join_paths(get_option('prefix'), driver_install_path),
            install: true)
    if name == 'test'
        dpdk_test = exec
        autotest_sources = sources
        subdir('test/suites')  # define the pre-canned test suites
    endif
endforeach
