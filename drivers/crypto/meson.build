# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    subdir_done()
endif

drivers = [
        'aesni_gcm',
        'aesni_mb',
        'armv8',
        'bcmfs',
        'caam_jr',
        'ccp',
        'cnxk',
        'dpaa_sec',
        'dpaa2_sec',
        'kasumi',
        'mlx5',
        'mvsam',
        'nitrox',
        'null',
        'octeontx',
        'octeontx2',
        'openssl',
        'scheduler',
        'snow3g',
        'virtio',
        'zuc',
]

std_deps = ['cryptodev'] # cryptodev pulls in all other needed deps
