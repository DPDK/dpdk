# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 HiSilicon Limited.

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files('rte_dmadev.c', 'rte_dmadev_trace_points.c')
headers = files('rte_dmadev.h')
indirect_headers += files('rte_dmadev_core.h', 'rte_dmadev_trace_fp.h')
driver_sdk_headers += files('rte_dmadev_pmd.h')

deps += ['telemetry']
