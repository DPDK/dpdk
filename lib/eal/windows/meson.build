# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

subdir('include')

sources += files(
        'eal.c',
        'eal_alarm.c',
        'eal_debug.c',
        'eal_dev.c',
        'eal_file.c',
        'eal_hugepages.c',
        'eal_interrupts.c',
        'eal_lcore.c',
        'eal_memalloc.c',
        'eal_memory.c',
        'eal_mp.c',
        'eal_thread.c',
        'eal_timer.c',
        'getline.c',
        'getopt.c',
        'rte_basename.c',
        'rte_thread.c',
)

dpdk_conf.set10('RTE_EAL_NUMA_AWARE_HUGEPAGES', true)

dbghelp_dep = cc.find_library('dbghelp')
if not cc.links(min_c_code, dependencies: dbghelp_dep)
    error('broken dependency, "dbghelp"')
endif
setupapi_dep = cc.find_library('setupapi')
if not cc.links(min_c_code, dependencies: setupapi_dep)
    error('broken dependency, "setupapi"')
endif
ws2_32_dep = cc.find_library('ws2_32')
if not cc.links(min_c_code, dependencies: ws2_32_dep)
    error('broken dependency, "ws2_32"')
endif
ext_deps += [dbghelp_dep, setupapi_dep, ws2_32_dep]
if is_ms_linker
        # Contrary to docs, VirtualAlloc2() is exported by mincore.lib.
        mincore_dep = cc.find_library('mincore')
        if not cc.links(min_c_code, dependencies: mincore_dep)
            error('broken dependency, "mincore"')
        endif
        ext_deps += mincore_dep
endif
