# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.

if is_windows
        build = false
        reason = 'only supported on Linux'
        subdir_done()
endif

cflags += ['-g']

deps += ['common_sxe2', 'hash','cryptodev','security']

includes += include_directories('../../common/sxe2')

if arch_subdir == 'x86'
        sources += files('sxe2_txrx_vec_sse.c')

        sxe2_avx512_cpu_support =(
                cc.get_define('__AVX512F__', args: machine_args) != '' and
                cc.get_define('__AVX512BW__', args: machine_args) != '')

        sxe2_avx512_cc_support = (
                not machine_args.contains('-mno-avx512f') and
                cc.has_argument('-mavx512f') and
                cc.has_argument('-mavx512bw'))

        if sxe2_avx512_cpu_support == true or sxe2_avx512_cc_support == true
                cflags += ['-DCC_AVX512_SUPPORT']
                avx512_args = [cflags, '-mavx512f', '-mavx512bw']
                if cc.has_argument('-march=skylake-avx512')
                        avx512_args += '-march=skylake-avx512'
                endif
                sxe2_avx512_lib = static_library('sxe2_avx512_lib', 'sxe2_txrx_vec_avx512.c',
                        dependencies: [static_rte_ethdev,
                        static_rte_kvargs, static_rte_hash,
                        static_rte_security, static_rte_cryptodev, static_rte_bus_pci],
                        include_directories: includes,
                        c_args: avx512_args)
                objs += sxe2_avx512_lib.extract_objects('sxe2_txrx_vec_avx512.c')
        endif
endif

sources += files(
        'sxe2_ethdev.c',
        'sxe2_cmd_chnl.c',
        'sxe2_vsi.c',
        'sxe2_queue.c',
        'sxe2_tx.c',
        'sxe2_rx.c',
        'sxe2_txrx_poll.c',
        'sxe2_txrx.c',
        'sxe2_txrx_vec.c',
)
