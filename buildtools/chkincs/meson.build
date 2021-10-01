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

sources = files('main.c')
sources += gen_c_files.process(dpdk_chkinc_headers)

deps = []
foreach l:enabled_libs
    deps += get_variable('static_rte_' + l)
endforeach

executable('chkincs', sources,
        c_args: cflags,
        include_directories: includes,
        dependencies: deps,
        link_whole: dpdk_static_libraries + dpdk_drivers,
        install: false)
