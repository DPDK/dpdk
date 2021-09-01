# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Marvell, Inc
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

config_flag_fmt = 'RTE_LIBRTE_@0@_COMMON'
deps = ['eal', 'pci', 'bus_pci', 'mbuf', 'security']
sources = files(
        'roc_ae.c',
        'roc_ae_fpm_tables.c',
        'roc_bphy.c',
        'roc_bphy_cgx.c',
        'roc_bphy_irq.c',
        'roc_cpt.c',
        'roc_cpt_debug.c',
        'roc_dev.c',
        'roc_hash.c',
        'roc_idev.c',
        'roc_irq.c',
        'roc_mbox.c',
        'roc_model.c',
        'roc_nix.c',
        'roc_nix_debug.c',
        'roc_nix_fc.c',
        'roc_nix_irq.c',
        'roc_nix_mac.c',
        'roc_nix_mcast.c',
        'roc_nix_npc.c',
        'roc_nix_ops.c',
        'roc_nix_ptp.c',
        'roc_nix_queue.c',
        'roc_nix_rss.c',
        'roc_nix_stats.c',
        'roc_nix_tm.c',
        'roc_nix_tm_ops.c',
        'roc_nix_tm_utils.c',
        'roc_nix_vlan.c',
        'roc_npa.c',
        'roc_npa_debug.c',
        'roc_npa_irq.c',
        'roc_npc.c',
        'roc_npc_mcam.c',
        'roc_npc_mcam_dump.c',
        'roc_npc_parse.c',
        'roc_npc_utils.c',
        'roc_platform.c',
        'roc_se.c',
        'roc_sso.c',
        'roc_sso_debug.c',
        'roc_sso_irq.c',
        'roc_tim.c',
        'roc_tim_irq.c',
        'roc_utils.c',
)

# Security common code
sources += files('cnxk_security.c')

includes += include_directories('../../bus/pci')
includes += include_directories('../../../lib/net')
