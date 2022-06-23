# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
reason = 'only supported on x86'

test_avx2_code = '''
#include <x86intrin.h>
__attribute__((target("avx2")))
__m256i fn(void *x) { return _mm256_loadu_si256(x); }
'''
if build and not cc.compiles(test_avx2_code, args:machine_args)
    build = false
    reason = 'missing support for AVX2 function attribute'
    subdir_done()
endif

deps += ['bus_pci']
sources = files(
        'idxd_common.c',
        'idxd_pci.c',
)

if is_linux
    sources += files('idxd_bus.c')
endif
