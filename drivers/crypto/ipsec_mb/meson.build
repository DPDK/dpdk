# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

IMB_required_ver = '1.0.0'
IMB_header = '#include<intel-ipsec-mb.h>'
if arch_subdir == 'arm'
    IMB_header = '#include<ipsec-mb.h>'
endif
lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
    build = false
    reason = 'missing dependency, "libIPSec_MB"'
else
    ext_deps += lib

    # version comes with quotes, so we split based on " and take the middle
    imb_ver = cc.get_define('IMB_VERSION_STR',
        prefix : IMB_header).split('"')[1]

    if (imb_ver == '') or (imb_ver.version_compare('<' + IMB_required_ver))
        reason = 'IPSec_MB version >= @0@ is required, found version @1@'.format(
            IMB_required_ver, imb_ver)
        build = false
    endif
endif

sources = files(
        'ipsec_mb_private.c',
        'ipsec_mb_ops.c',
        'pmd_aesni_mb.c',
        'pmd_aesni_gcm.c',
        'pmd_chacha_poly.c',
        'pmd_kasumi.c',
        'pmd_snow3g.c',
        'pmd_zuc.c',
)
deps += ['bus_vdev', 'net', 'security']
require_iova_in_mbuf = false
