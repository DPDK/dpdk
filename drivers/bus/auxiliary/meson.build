# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 NVIDIA Corporation & Affiliates

headers = files(
        'rte_bus_auxiliary.h',
)
sources = files(
        'auxiliary_common.c',
        'auxiliary_params.c',
)
if is_linux
    sources += files(
            'linux/auxiliary.c',
    )
endif
deps += ['kvargs']
