# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2020 Broadcom

cflags_options = [
        '-DCFA_BLD_PRINT_OBJ=1',
]

foreach option:cflags_options
        if cc.has_argument(option)
                cflags += option
        endif
endforeach

#Include the folder for headers
includes += include_directories('./')
includes += include_directories('./include')
includes += include_directories('./include/platform/dpdk/')
includes += include_directories('./bld/p70')
includes += include_directories('./bld/p70/host')
includes += include_directories('./bld/include')
includes += include_directories('./bld/include/host')
includes += include_directories('./bld/include/p70')
includes += include_directories('./mm/include')
includes += include_directories('./tim/include')
includes += include_directories('./tpm/include')

#Add the source files
sources += files(
        'bld/host/cfa_bld_mpc.c',
        'bld/p70/cfa_bld_p70_mpc.c',
        'bld/p70/host/cfa_bld_p70_host_mpc_wrapper.c',
        'bld/p70/host/cfa_bld_p70_mpcops.c',
        'mm/cfa_mm.c',
        'tim/cfa_tim.c',
        'tpm/cfa_tpm.c')
