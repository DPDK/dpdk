/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

/* ACC101 PCI vendor & device IDs */
#define ACC101_VENDOR_ID           (0x8086)
#define ACC101_PF_DEVICE_ID        (0x57c4)
#define ACC101_VF_DEVICE_ID        (0x57c5)

/* Number of Virtual Functions ACC101 supports */
#define ACC101_NUM_VFS                  16
#define ACC101_NUM_QGRPS                8
#define ACC101_NUM_AQS                  16

#define ACC101_WORDS_IN_ARAM_SIZE (128 * 1024 / 4)

/* Mapping of signals for the available engines */
#define ACC101_SIG_UL_5G      0
#define ACC101_SIG_UL_5G_LAST 8
#define ACC101_SIG_DL_5G      13
#define ACC101_SIG_DL_5G_LAST 15
#define ACC101_SIG_UL_4G      16
#define ACC101_SIG_UL_4G_LAST 19
#define ACC101_SIG_DL_4G      27
#define ACC101_SIG_DL_4G_LAST 31
#define ACC101_NUM_ACCS       5

/* ACC101 Configuration */
#define ACC101_CFG_DMA_ERROR    0x3D7
#define ACC101_CFG_AXI_CACHE    0x11
#define ACC101_CFG_QMGR_HI_P    0x0F0F
#define ACC101_CFG_PCI_AXI      0xC003
#define ACC101_CFG_PCI_BRIDGE   0x40006033
#define ACC101_GPEX_AXIMAP_NUM  17
#define ACC101_CLOCK_GATING_EN  0x30000
#define ACC101_DMA_INBOUND      0x104
/* DDR Size per VF - 512MB by default
 * Can be increased up to 4 GB with single PF/VF
 */
#define ACC101_HARQ_DDR         (512 * 1)
