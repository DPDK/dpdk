# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

if not get_option('check_includes')
    build = false
    subdir_done()
endif

gen_c_file_for_header = find_program('gen_c_file_for_header.py')
gen_c_files = generator(gen_c_file_for_header,
        output: '@BASENAME@.c',
        arguments: ['@INPUT@', '@OUTPUT@'])

cflags = machine_args
cflags += no_wvla_cflag

sources = files('main.c')
sources += gen_c_files.process(dpdk_chkinc_headers)

# some driver SDK headers depend on these two buses, which are mandatory in build
# so we always include them in deps list
deps = [get_variable('shared_rte_bus_vdev'), get_variable('shared_rte_bus_pci')]
if dpdk_conf.has('RTE_BUS_VMBUS')
    deps += get_variable('shared_rte_bus_vmbus')
endif
# add the rest of the libs to the dependencies
foreach l:dpdk_libs_enabled
    deps += get_variable('shared_rte_' + l)
endforeach

executable('chkincs', sources,
        c_args: cflags,
        include_directories: includes,
        dependencies: deps,
        install: false)

executable('chkincs-exp', sources,
        c_args: [cflags, '-DALLOW_EXPERIMENTAL_API'],
        include_directories: includes,
        dependencies: deps,
        install: false)

executable('chkincs-all', sources,
        c_args: [cflags, '-DALLOW_EXPERIMENTAL_API', '-DALLOW_INTERNAL_API'],
        include_directories: includes,
        dependencies: deps,
        install: false)

# run tests for c++ builds also
if not add_languages('cpp', required: false)
    subdir_done()
endif

gen_cpp_files = generator(gen_c_file_for_header,
        output: '@BASENAME@.cpp',
        arguments: ['@INPUT@', '@OUTPUT@'])

cpp_sources = files('main.cpp')
cpp_sources += gen_cpp_files.process(dpdk_chkinc_headers)

executable('chkincs-cpp', cpp_sources,
        cpp_args: ['-include', 'rte_config.h', cflags],
        include_directories: includes,
        dependencies: deps,
        install: false)

executable('chkincs-cpp-exp', cpp_sources,
        cpp_args: ['-include', 'rte_config.h', cflags, '-DALLOW_EXPERIMENTAL_API'],
        include_directories: includes,
        dependencies: deps,
        install: false)

executable('chkincs-cpp-all', cpp_sources,
        cpp_args: ['-include', 'rte_config.h', cflags, '-DALLOW_EXPERIMENTAL_API',
                   '-DALLOW_INTERNAL_API'],
        include_directories: includes,
        dependencies: deps,
        install: false)
