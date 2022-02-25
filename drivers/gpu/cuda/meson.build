# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 NVIDIA Corporation & Affiliates

if not is_linux
        build = false
        reason = 'only supported on Linux'
        subdir_done()
endif
if not cc.has_header('cuda.h')
        build = false
        reason = 'missing dependency, "cuda.h"'
        subdir_done()
endif
if not cc.has_header('cudaTypedefs.h')
        build = false
        reason = 'missing dependency, "cudaTypedefs.h"'
        subdir_done()
endif

if cc.has_header('gdrapi.h')
        dpdk_conf.set('DRIVERS_GPU_CUDA_GDRCOPY_H', 1)
endif

deps += ['gpudev', 'pci', 'bus_pci']
sources = files('cuda.c', 'gdrcopy.c')
