# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Advanced Micro Devices, Inc.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

ESMI_header = '#include<e_smi/e_smi.h>'
lib = cc.find_library('e_smi64', required: false)
if not lib.found()
    build = false
    reason = 'missing dependency, "libe_smi"'
else
    ext_deps += lib
endif

sources = files('amd_uncore.c')
deps += ['power']
