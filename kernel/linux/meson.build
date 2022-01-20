# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdirs = ['kni']

kernel_build_dir = get_option('kernel_dir')
kernel_source_dir = get_option('kernel_dir')
kernel_install_dir = ''
install = not meson.is_cross_build()
cross_args = []

if not meson.is_cross_build()
    # native build
    kernel_version = run_command('uname', '-r', check: true).stdout().strip()
    kernel_install_dir = '/lib/modules/' + kernel_version + '/extra/dpdk'
    if kernel_build_dir == ''
        # use default path for native builds
        kernel_build_dir = '/lib/modules/' + kernel_version + '/build'
    endif
    if kernel_source_dir == ''
        # use default path for native builds
        kernel_source_dir = '/lib/modules/' + kernel_version + '/source'
    endif

    # test running make in kernel directory, using "make kernelversion"
    make_returncode = run_command('make', '-sC', kernel_build_dir,
            'kernelversion', check: true).returncode()
    if make_returncode != 0
        # backward compatibility:
        # the headers could still be in the 'build' subdir
        if not kernel_build_dir.endswith('build') and not kernel_build_dir.endswith('build/')
            kernel_build_dir = join_paths(kernel_build_dir, 'build')
            make_returncode = run_command('make', '-sC', kernel_build_dir,
                    'kernelversion', check: true).returncode()
        endif
    endif

    if make_returncode != 0
        error('Cannot compile kernel modules as requested - are kernel headers installed?')
    endif

    # DO ACTUAL MODULE BUILDING
    foreach d:subdirs
        subdir(d)
    endforeach

    subdir_done()
endif

# cross build
# if we are cross-compiling we need kernel_build_dir specified
if kernel_build_dir == ''
    error('Need "kernel_dir" option for kmod compilation when cross-compiling')
endif
cross_compiler = find_program('c').path()
if cross_compiler.endswith('gcc')
    cross_prefix = run_command([py3, '-c', 'print("' + cross_compiler + '"[:-3])'],
            check: true).stdout().strip()
elif cross_compiler.endswith('clang')
    cross_prefix = ''
    found_target = false
    # search for '-target' and use the arg that follows
    # (i.e. the value of '-target') as cross_prefix
    foreach cross_c_arg : meson.get_cross_property('c_args')
        if found_target and cross_prefix == ''
            cross_prefix = cross_c_arg
        endif
        if cross_c_arg == '-target'
            found_target = true
        endif
    endforeach
    if cross_prefix == ''
        error('Did not find -target and its value in c_args in input cross-file.')
    endif
    linker = 'lld'
    foreach cross_c_link_arg : meson.get_cross_property('c_link_args')
        if cross_c_link_arg.startswith('-fuse-ld')
            linker = cross_c_link_arg.split('=')[1]
        endif
    endforeach
    cross_args += ['CC=@0@'.format(cross_compiler), 'LD=ld.@0@'.format(linker)]
else
    error('Unsupported cross compiler: @0@'.format(cross_compiler))
endif

cross_arch = host_machine.cpu_family()
if host_machine.cpu_family() == 'aarch64'
    cross_arch = 'arm64'
endif

cross_args += ['ARCH=@0@'.format(cross_arch),
        'CROSS_COMPILE=@0@'.format(cross_prefix)]

# DO ACTUAL MODULE BUILDING
foreach d:subdirs
    subdir(d)
endforeach
