# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += 'bus_vdev'
sources = files('null_crypto_pmd.c', 'null_crypto_pmd_ops.c')
require_iova_in_mbuf = false
