# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2022 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

qat_crypto = true
qat_crypto_path = 'crypto/qat'
qat_crypto_relpath = '../../' + qat_crypto_path
qat_compress = true
qat_compress_path = 'compress/qat'
qat_compress_relpath = '../../' + qat_compress_path

if disable_drivers.contains(qat_crypto_path)
    qat_crypto = false
    dpdk_drvs_disabled += qat_crypto_path
    set_variable('drv_' + qat_crypto_path.underscorify() + '_disable_reason',
            'Explicitly disabled via build config')
endif
if disable_drivers.contains(qat_compress_path)
    qat_compress = false
    dpdk_drvs_disabled += qat_compress_path
    set_variable('drv_' + qat_compress_path.underscorify() + '_disable_reason',
            'Explicitly disabled via build config')
endif

libcrypto = dependency('libcrypto', required: false, method: 'pkg-config')

if arch_subdir == 'arm'
    if libcrypto.found()
        ext_deps += libcrypto
        dpdk_conf.set('RTE_QAT_OPENSSL', true)
    else
        qat_crypto = false
        dpdk_drvs_disabled += qat_crypto_path
        set_variable('drv_' + qat_crypto_path.underscorify() + '_disable_reason',
        'missing dependency for Arm, libcrypto')
    endif
else
    IMB_required_ver = '1.4.0'
    IMB_header = '#include<intel-ipsec-mb.h>'
    libipsecmb = cc.find_library('IPSec_MB', required: false)
    if libipsecmb.found() and meson.version().version_compare('>=0.60') and cc.links(
            'int main(void) {return 0;}', dependencies: libipsecmb)
        # version comes with quotes, so we split based on " and take the middle
        imb_ver = cc.get_define('IMB_VERSION_STR',
            prefix : IMB_header).split('"')[1]

        if (imb_ver.version_compare('>=' + IMB_required_ver))
            ext_deps += libipsecmb
        elif libcrypto.found()
            ext_deps += libcrypto
            dpdk_conf.set('RTE_QAT_OPENSSL', true)
        else
            qat_crypto = false
            dpdk_drvs_disabled += qat_crypto_path
            set_variable('drv_' + qat_crypto_path.underscorify() + '_disable_reason',
                'missing dependency, libipsecmb or libcrypto')
        endif
    elif libcrypto.found()
        ext_deps += libcrypto
        dpdk_conf.set('RTE_QAT_OPENSSL', true)
    else
        qat_crypto = false
        dpdk_drvs_disabled += qat_crypto_path
        set_variable('drv_' + qat_crypto_path.underscorify() + '_disable_reason',
            'missing dependency, libipsecmb or libcrypto')
    endif
endif

deps += ['bus_pci', 'cryptodev', 'net', 'compressdev']
sources += files(
        'qat_common.c',
        'qat_qp.c',
        'qat_device.c',
        'qat_logs.c',
        'qat_pf2vf.c',
        'dev/qat_dev_gen1.c',
        'dev/qat_dev_gen2.c',
        'dev/qat_dev_gen3.c',
        'dev/qat_dev_gen4.c',
)
includes += include_directories(
        'qat_adf',
        qat_crypto_relpath,
        qat_compress_relpath,
)

if qat_compress
    foreach f: ['qat_comp_pmd.c', 'qat_comp.c',
            'dev/qat_comp_pmd_gen1.c',
            'dev/qat_comp_pmd_gen2.c',
            'dev/qat_comp_pmd_gen3.c',
            'dev/qat_comp_pmd_gen4.c',
        ]
        sources += files(join_paths(qat_compress_relpath, f))
    endforeach
endif

if qat_crypto
    foreach f: ['qat_sym.c', 'qat_sym_session.c',
            'qat_asym.c', 'qat_crypto.c',
            'dev/qat_sym_pmd_gen1.c',
            'dev/qat_asym_pmd_gen1.c',
            'dev/qat_crypto_pmd_gen2.c',
            'dev/qat_crypto_pmd_gen3.c',
            'dev/qat_crypto_pmd_gen4.c',
        ]
        sources += files(join_paths(qat_crypto_relpath, f))
    endforeach
    deps += ['security']
    cflags += ['-DBUILD_QAT_SYM', '-DBUILD_QAT_ASYM']
endif
