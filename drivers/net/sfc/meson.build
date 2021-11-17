# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2019-2021 Xilinx, Inc.
# Copyright(c) 2016-2019 Solarflare Communications Inc.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if (arch_subdir != 'x86' and arch_subdir != 'arm') or (not dpdk_conf.get('RTE_ARCH_64'))
    build = false
    reason = 'only supported on x86_64 and aarch64'
    subdir_done()
endif

extra_flags = []

# Strict-aliasing rules are violated by rte_eth_link to uint64_t casts
extra_flags += '-Wno-strict-aliasing'

# Enable more warnings
extra_flags += [
        '-Wdisabled-optimization',
]

# Compiler and version dependent flags
extra_flags += [
        '-Waggregate-return',
        '-Wbad-function-cast',
]

foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

# for gcc and old Clang compiles we need -latomic for 128-bit atomic ops
atomic_check_code = '''
int main(void)
{
    __int128 a = 0;
    __int128 b;

    b = __atomic_load_n(&a, __ATOMIC_RELAXED);
    __atomic_store(&b, &a, __ATOMIC_RELAXED);
    __atomic_store_n(&b, a, __ATOMIC_RELAXED);
    return 0;
}
'''
if not cc.links(atomic_check_code)
    libatomic_dep = cc.find_library('atomic', required: false)
    if not libatomic_dep.found()
        build = false
        reason = 'missing dependency, "libatomic"'
        subdir_done()
    endif

    # libatomic could be half-installed when above check finds it but
    # linkage fails
    if not cc.links(atomic_check_code, dependencies: libatomic_dep)
        build = false
        reason = 'broken dependency, "libatomic"'
        subdir_done()
    endif
    ext_deps += libatomic_dep
endif

deps += ['common_sfc_efx', 'bus_pci']
sources = files(
        'sfc_ethdev.c',
        'sfc_kvargs.c',
        'sfc.c',
        'sfc_mcdi.c',
        'sfc_sriov.c',
        'sfc_sw_stats.c',
        'sfc_intr.c',
        'sfc_ev.c',
        'sfc_port.c',
        'sfc_rx.c',
        'sfc_tx.c',
        'sfc_tso.c',
        'sfc_filter.c',
        'sfc_switch.c',
        'sfc_mae.c',
        'sfc_mae_counter.c',
        'sfc_flow.c',
        'sfc_flow_tunnel.c',
        'sfc_dp.c',
        'sfc_ef10_rx.c',
        'sfc_ef10_essb_rx.c',
        'sfc_ef10_tx.c',
        'sfc_ef100_rx.c',
        'sfc_ef100_tx.c',
        'sfc_service.c',
        'sfc_repr_proxy.c',
        'sfc_repr.c',
        'sfc_nic_dma.c',
)
