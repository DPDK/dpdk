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
cflags += '-DALLOW_EXPERIMENTAL_API'
cflags += '-DALLOW_INTERNAL_API'

sources = files('main.c')
sources += gen_c_files.process(dpdk_chkinc_headers)

# some driver SDK headers depend on these two buses, which are mandatory in build
# so we always include them in deps list
deps = [get_variable('shared_rte_bus_vdev'), get_variable('shared_rte_bus_pci')]
# add the rest of the libs to the dependencies
foreach l:enabled_libs
    deps += get_variable('shared_rte_' + l)
endforeach

executable('chkincs', sources,
        c_args: cflags,
        include_directories: includes,
        dependencies: deps,
        install: false)

# run tests for c++ builds also
if not add_languages('cpp', required: false)
    subdir_done()
endif

# check for extern C in files, since this is not detected as an error by the compiler
grep = find_program('grep', required: false)
if grep.found()
    errlist = run_command([grep, '--files-without-match', '^extern "C"', dpdk_chkinc_headers],
            check: false, capture: true).stdout().split()
    if errlist != []
        error('Files missing C++ \'extern "C"\' guards:\n- ' + '\n- '.join(errlist))
    endif
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
