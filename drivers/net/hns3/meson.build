# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2021 Hisilicon Limited

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

if arch_subdir != 'x86' and arch_subdir != 'arm' or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on x86_64 and aarch64'
    subdir_done()
endif

sources = files(
        'hns3_cmd.c',
        'hns3_dcb.c',
        'hns3_intr.c',
        'hns3_ethdev.c',
        'hns3_ethdev_vf.c',
        'hns3_fdir.c',
        'hns3_flow.c',
        'hns3_mbx.c',
        'hns3_regs.c',
        'hns3_rss.c',
        'hns3_rxtx.c',
        'hns3_stats.c',
        'hns3_mp.c',
        'hns3_tm.c',
        'hns3_ptp.c',
        'hns3_common.c',
        'hns3_ethdev_dump.c',
)

deps += ['hash']

if arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('hns3_rxtx_vec.c')

    # compile SVE when:
    # a. support SVE in minimum instruction set baseline
    # b. it's not minimum instruction set, but compiler support
    if dpdk_conf.has('RTE_HAS_SVE_ACLE')
        sources += files('hns3_rxtx_vec_sve.c')
    elif cc.has_argument('-march=armv8.2-a+sve') and cc.check_header('arm_sve.h')
        cflags += ['-DRTE_HAS_SVE_ACLE=1']
        sve_cflags = []
        foreach flag: cflags
            if not (flag.startswith('-march=') or flag.startswith('-mcpu=') or flag.startswith('-mtune='))
                sve_cflags += flag
            endif
        endforeach
        hns3_sve_lib = static_library('hns3_sve_lib',
                        'hns3_rxtx_vec_sve.c',
                        dependencies: [static_rte_ethdev],
                        include_directories: includes,
                        c_args: [sve_cflags, '-march=armv8.2-a+sve'])
        objs += hns3_sve_lib.extract_objects('hns3_rxtx_vec_sve.c')
    endif
endif
