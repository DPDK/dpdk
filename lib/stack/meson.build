# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files('rte_stack.c', 'rte_stack_std.c', 'rte_stack_lf.c')
headers = files('rte_stack.h')
# subheaders, not for direct inclusion by apps
indirect_headers += files(
        'rte_stack_std.h',
        'rte_stack_lf.h',
        'rte_stack_lf_generic.h',
        'rte_stack_lf_c11.h',
        'rte_stack_lf_stubs.h',
)
