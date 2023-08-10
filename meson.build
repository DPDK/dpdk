# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

project('DPDK', 'c',
        # Get version number from file.
        # Fallback to "more" for Windows compatibility.
        version: run_command(find_program('cat', 'more'),
            files('VERSION'), check: true).stdout().strip(),
        license: 'BSD',
        default_options: [
            'buildtype=release',
            'c_std=c11',
            'default_library=static',
            'warning_level=2',
        ],
        meson_version: '>= 0.53.2'
)

# check for developer mode
developer_mode = false
if get_option('developer_mode').auto()
    fs = import('fs')
    developer_mode = fs.exists('.git')
else
    developer_mode = get_option('developer_mode').enabled()
endif
if developer_mode
    message('## Building in Developer Mode ##')
endif

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
dpdk_source_root = meson.current_source_dir()
dpdk_build_root = meson.current_build_dir()
dpdk_conf = configuration_data()
dpdk_includes = []
dpdk_libraries = []
dpdk_static_libraries = []
dpdk_shared_lib_deps = []
dpdk_static_lib_deps = []
dpdk_chkinc_headers = []
dpdk_driver_classes = []
dpdk_drivers = []
dpdk_extra_ldflags = []
dpdk_libs_deprecated = []
dpdk_apps_disabled = []
dpdk_apps_enabled = []
dpdk_libs_disabled = []
dpdk_libs_enabled = []
dpdk_drvs_disabled = []
testpmd_drivers_sources = []
testpmd_drivers_deps = []
abi_version_file = files('ABI_VERSION')

if host_machine.cpu_family().startswith('x86')
    arch_subdir = 'x86'
elif host_machine.cpu_family().startswith('arm') or host_machine.cpu_family().startswith('aarch')
    arch_subdir = 'arm'
elif host_machine.cpu_family().startswith('loongarch')
    arch_subdir = 'loongarch'
elif host_machine.cpu_family().startswith('ppc')
    arch_subdir = 'ppc'
elif host_machine.cpu_family().startswith('riscv')
    arch_subdir = 'riscv'
endif

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.', 'config',
    'lib/eal/include',
    'lib/eal/@0@/include'.format(host_machine.system()),
    'lib/eal/@0@/include'.format(arch_subdir),
)

# do configuration and get tool paths
subdir('buildtools')
subdir('config')

# build libs and drivers
subdir('lib')
subdir('drivers')

# build binaries and installable tools
subdir('usertools')
subdir('app')

# build docs
subdir('doc')

# build any examples explicitly requested - useful for developers - and
# install any example code into the appropriate install path
subdir('examples')
install_subdir('examples',
        install_dir: get_option('datadir') + '/dpdk',
        exclude_files: ex_file_excludes)

# build kernel modules
subdir('kernel')

# check header includes if requested
if get_option('check_includes')
    subdir('buildtools/chkincs')
endif

# write the build config
build_cfg = 'rte_build_config.h'
configure_file(output: build_cfg,
        configuration: dpdk_conf,
        install_dir: join_paths(get_option('includedir'),
            get_option('include_subdir_arch')))

# build pkg-config files for dpdk
subdir('buildtools/pkg-config')

if meson.is_subproject()
    subdir('buildtools/subproject')
endif

# Final output, list all the parts to be built.
# This does not affect any part of the build, for information only.
output_message = '\n=================\nApplications Enabled\n=================\n'
output_message += '\napps:\n\t'
output_count = 0
foreach app:dpdk_apps_enabled
    output_message += app + ', '
    output_count += 1
    if output_count == 8
        output_message += '\n\t'
        output_count = 0
    endif
endforeach
message(output_message + '\n')

output_message = '\n=================\nLibraries Enabled\n=================\n'
output_message += '\nlibs:\n\t'
output_count = 0
foreach lib:dpdk_libs_enabled
    output_message += lib + ', '
    output_count += 1
    if output_count == 8
        output_message += '\n\t'
        output_count = 0
    endif
endforeach
message(output_message + '\n')

output_message = '\n===============\nDrivers Enabled\n===============\n'
foreach class:dpdk_driver_classes
    class_drivers = get_variable(class + '_drivers')
    output_message += '\n' + class + ':\n\t'
    output_count = 0
    foreach drv:class_drivers
        output_message += drv + ', '
        output_count += 1
        if output_count == 8
            output_message += '\n\t'
            output_count = 0
        endif
    endforeach
endforeach
message(output_message + '\n')

output_message = '\n=================\nContent Skipped\n=================\n'
output_message += '\napps:\n\t'
foreach app:dpdk_apps_disabled
    reason = get_variable(app.underscorify() + '_disable_reason')
    output_message += app + ':\t' + reason + '\n\t'
endforeach
output_message += '\nlibs:\n\t'
foreach lib:dpdk_libs_disabled
    reason = get_variable(lib.underscorify() + '_disable_reason')
    output_message += lib + ':\t' + reason + '\n\t'
endforeach
output_message += '\ndrivers:\n\t'
foreach drv:dpdk_drvs_disabled
    reason = get_variable(drv.underscorify() + '_disable_reason')
    output_message += drv + ':\t' + reason + '\n\t'
endforeach
message(output_message + '\n')
