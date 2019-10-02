/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2007-2013 Broadcom Corporation.
 *
 * Eric Davis        <edavis@broadcom.com>
 * David Christensen <davidch@broadcom.com>
 * Gary Zambrano     <zambrano@broadcom.com>
 *
 * Copyright (c) 2014-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef ECORE_HSI_H
#define ECORE_HSI_H

#include "ecore_fw_defs.h"
#include "ecore_mfw_req.h"
#include "bnx2x_osal.h"

#define FW_ENCODE_32BIT_PATTERN         0x1e1e1e1e

struct license_key {
	uint32_t reserved[6];

	uint32_t max_iscsi_conn;
#define ECORE_MAX_ISCSI_TRGT_CONN_MASK	0xFFFF
#define ECORE_MAX_ISCSI_TRGT_CONN_SHIFT	0
#define ECORE_MAX_ISCSI_INIT_CONN_MASK	0xFFFF0000
#define ECORE_MAX_ISCSI_INIT_CONN_SHIFT	16

	uint32_t reserved_a;

	uint32_t max_fcoe_conn;
#define ECORE_MAX_FCOE_TRGT_CONN_MASK	0xFFFF
#define ECORE_MAX_FCOE_TRGT_CONN_SHIFT	0
#define ECORE_MAX_FCOE_INIT_CONN_MASK	0xFFFF0000
#define ECORE_MAX_FCOE_INIT_CONN_SHIFT	16

	uint32_t reserved_b[4];
};



/****************************************************************************
 * Shared HW configuration                                                  *
 ****************************************************************************/
#define PIN_CFG_NA                          0x00000000
#define PIN_CFG_GPIO0_P0                    0x00000001
#define PIN_CFG_GPIO1_P0                    0x00000002
#define PIN_CFG_GPIO2_P0                    0x00000003
#define PIN_CFG_GPIO3_P0                    0x00000004
#define PIN_CFG_GPIO0_P1                    0x00000005
#define PIN_CFG_GPIO1_P1                    0x00000006
#define PIN_CFG_GPIO2_P1                    0x00000007
#define PIN_CFG_GPIO3_P1                    0x00000008
#define PIN_CFG_EPIO0                       0x00000009
#define PIN_CFG_EPIO1                       0x0000000a
#define PIN_CFG_EPIO2                       0x0000000b
#define PIN_CFG_EPIO3                       0x0000000c
#define PIN_CFG_EPIO4                       0x0000000d
#define PIN_CFG_EPIO5                       0x0000000e
#define PIN_CFG_EPIO6                       0x0000000f
#define PIN_CFG_EPIO7                       0x00000010
#define PIN_CFG_EPIO8                       0x00000011
#define PIN_CFG_EPIO9                       0x00000012
#define PIN_CFG_EPIO10                      0x00000013
#define PIN_CFG_EPIO11                      0x00000014
#define PIN_CFG_EPIO12                      0x00000015
#define PIN_CFG_EPIO13                      0x00000016
#define PIN_CFG_EPIO14                      0x00000017
#define PIN_CFG_EPIO15                      0x00000018
#define PIN_CFG_EPIO16                      0x00000019
#define PIN_CFG_EPIO17                      0x0000001a
#define PIN_CFG_EPIO18                      0x0000001b
#define PIN_CFG_EPIO19                      0x0000001c
#define PIN_CFG_EPIO20                      0x0000001d
#define PIN_CFG_EPIO21                      0x0000001e
#define PIN_CFG_EPIO22                      0x0000001f
#define PIN_CFG_EPIO23                      0x00000020
#define PIN_CFG_EPIO24                      0x00000021
#define PIN_CFG_EPIO25                      0x00000022
#define PIN_CFG_EPIO26                      0x00000023
#define PIN_CFG_EPIO27                      0x00000024
#define PIN_CFG_EPIO28                      0x00000025
#define PIN_CFG_EPIO29                      0x00000026
#define PIN_CFG_EPIO30                      0x00000027
#define PIN_CFG_EPIO31                      0x00000028

/* EPIO definition */
#define EPIO_CFG_NA                         0x00000000
#define EPIO_CFG_EPIO0                      0x00000001
#define EPIO_CFG_EPIO1                      0x00000002
#define EPIO_CFG_EPIO2                      0x00000003
#define EPIO_CFG_EPIO3                      0x00000004
#define EPIO_CFG_EPIO4                      0x00000005
#define EPIO_CFG_EPIO5                      0x00000006
#define EPIO_CFG_EPIO6                      0x00000007
#define EPIO_CFG_EPIO7                      0x00000008
#define EPIO_CFG_EPIO8                      0x00000009
#define EPIO_CFG_EPIO9                      0x0000000a
#define EPIO_CFG_EPIO10                     0x0000000b
#define EPIO_CFG_EPIO11                     0x0000000c
#define EPIO_CFG_EPIO12                     0x0000000d
#define EPIO_CFG_EPIO13                     0x0000000e
#define EPIO_CFG_EPIO14                     0x0000000f
#define EPIO_CFG_EPIO15                     0x00000010
#define EPIO_CFG_EPIO16                     0x00000011
#define EPIO_CFG_EPIO17                     0x00000012
#define EPIO_CFG_EPIO18                     0x00000013
#define EPIO_CFG_EPIO19                     0x00000014
#define EPIO_CFG_EPIO20                     0x00000015
#define EPIO_CFG_EPIO21                     0x00000016
#define EPIO_CFG_EPIO22                     0x00000017
#define EPIO_CFG_EPIO23                     0x00000018
#define EPIO_CFG_EPIO24                     0x00000019
#define EPIO_CFG_EPIO25                     0x0000001a
#define EPIO_CFG_EPIO26                     0x0000001b
#define EPIO_CFG_EPIO27                     0x0000001c
#define EPIO_CFG_EPIO28                     0x0000001d
#define EPIO_CFG_EPIO29                     0x0000001e
#define EPIO_CFG_EPIO30                     0x0000001f
#define EPIO_CFG_EPIO31                     0x00000020

struct mac_addr {
	uint32_t upper;
	uint32_t lower;
};


struct shared_hw_cfg {			 /* NVRAM Offset */
	/* Up to 16 bytes of NULL-terminated string */
	uint8_t  part_num[16];		    /* 0x104 */

	uint32_t config;			/* 0x114 */
	#define SHARED_HW_CFG_MDIO_VOLTAGE_MASK             0x00000001
		#define SHARED_HW_CFG_MDIO_VOLTAGE_SHIFT             0
		#define SHARED_HW_CFG_MDIO_VOLTAGE_1_2V              0x00000000
		#define SHARED_HW_CFG_MDIO_VOLTAGE_2_5V              0x00000001

	#define SHARED_HW_CFG_PORT_SWAP                     0x00000004

	    #define SHARED_HW_CFG_BEACON_WOL_EN                  0x00000008

	    #define SHARED_HW_CFG_PCIE_GEN3_DISABLED            0x00000000
	    #define SHARED_HW_CFG_PCIE_GEN3_ENABLED             0x00000010

	#define SHARED_HW_CFG_MFW_SELECT_MASK               0x00000700
		#define SHARED_HW_CFG_MFW_SELECT_SHIFT               8
	/* Whatever MFW found in NVM
	   (if multiple found, priority order is: NC-SI, UMP, IPMI) */
		#define SHARED_HW_CFG_MFW_SELECT_DEFAULT             0x00000000
		#define SHARED_HW_CFG_MFW_SELECT_NC_SI               0x00000100
		#define SHARED_HW_CFG_MFW_SELECT_UMP                 0x00000200
		#define SHARED_HW_CFG_MFW_SELECT_IPMI                0x00000300
	/* Use SPIO4 as an arbiter between: 0-NC_SI, 1-IPMI
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_NC_SI_IPMI    0x00000400
	/* Use SPIO4 as an arbiter between: 0-UMP, 1-IPMI
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_UMP_IPMI      0x00000500
	/* Use SPIO4 as an arbiter between: 0-NC-SI, 1-UMP
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_NC_SI_UMP     0x00000600

	/* Adjust the PCIe G2 Tx amplitude driver for all Tx lanes. For
	   backwards compatibility, value of 0 is disabling this feature.
	    That means that though 0 is a valid value, it cannot be
	    configured. */
	#define SHARED_HW_CFG_G2_TX_DRIVE_MASK                        0x0000F000
	#define SHARED_HW_CFG_G2_TX_DRIVE_SHIFT                       12

	#define SHARED_HW_CFG_LED_MODE_MASK                 0x000F0000
		#define SHARED_HW_CFG_LED_MODE_SHIFT                 16
		#define SHARED_HW_CFG_LED_MAC1                       0x00000000
		#define SHARED_HW_CFG_LED_PHY1                       0x00010000
		#define SHARED_HW_CFG_LED_PHY2                       0x00020000
		#define SHARED_HW_CFG_LED_PHY3                       0x00030000
		#define SHARED_HW_CFG_LED_MAC2                       0x00040000
		#define SHARED_HW_CFG_LED_PHY4                       0x00050000
		#define SHARED_HW_CFG_LED_PHY5                       0x00060000
		#define SHARED_HW_CFG_LED_PHY6                       0x00070000
		#define SHARED_HW_CFG_LED_MAC3                       0x00080000
		#define SHARED_HW_CFG_LED_PHY7                       0x00090000
		#define SHARED_HW_CFG_LED_PHY9                       0x000a0000
		#define SHARED_HW_CFG_LED_PHY11                      0x000b0000
		#define SHARED_HW_CFG_LED_MAC4                       0x000c0000
		#define SHARED_HW_CFG_LED_PHY8                       0x000d0000
		#define SHARED_HW_CFG_LED_EXTPHY1                    0x000e0000
		#define SHARED_HW_CFG_LED_EXTPHY2                    0x000f0000

    #define SHARED_HW_CFG_SRIOV_MASK                    0x40000000
		#define SHARED_HW_CFG_SRIOV_DISABLED                 0x00000000
		#define SHARED_HW_CFG_SRIOV_ENABLED                  0x40000000

	#define SHARED_HW_CFG_ATC_MASK                      0x80000000
		#define SHARED_HW_CFG_ATC_DISABLED                   0x00000000
		#define SHARED_HW_CFG_ATC_ENABLED                    0x80000000

	uint32_t config2;			    /* 0x118 */

	#define SHARED_HW_CFG_PCIE_GEN2_MASK                0x00000100
	    #define SHARED_HW_CFG_PCIE_GEN2_SHIFT                8
	    #define SHARED_HW_CFG_PCIE_GEN2_DISABLED             0x00000000
	#define SHARED_HW_CFG_PCIE_GEN2_ENABLED              0x00000100

	#define SHARED_HW_CFG_SMBUS_TIMING_MASK             0x00001000
		#define SHARED_HW_CFG_SMBUS_TIMING_100KHZ            0x00000000
		#define SHARED_HW_CFG_SMBUS_TIMING_400KHZ            0x00001000

	#define SHARED_HW_CFG_HIDE_PORT1                    0x00002000


		/* Output low when PERST is asserted */
	#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_MASK       0x00008000
		#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_DISABLED    0x00000000
		#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_ENABLED     0x00008000

	#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_MASK    0x00070000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_SHIFT    16
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_HW       0x00000000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_0DB      0x00010000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_3_5DB    0x00020000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_6_0DB    0x00030000

	/*  The fan failure mechanism is usually related to the PHY type
	      since the power consumption of the board is determined by the PHY.
	      Currently, fan is required for most designs with SFX7101, BNX2X8727
	      and BNX2X8481. If a fan is not required for a board which uses one
	      of those PHYs, this field should be set to "Disabled". If a fan is
	      required for a different PHY type, this option should be set to
	      "Enabled". The fan failure indication is expected on SPIO5 */
	#define SHARED_HW_CFG_FAN_FAILURE_MASK              0x00180000
		#define SHARED_HW_CFG_FAN_FAILURE_SHIFT              19
		#define SHARED_HW_CFG_FAN_FAILURE_PHY_TYPE           0x00000000
		#define SHARED_HW_CFG_FAN_FAILURE_DISABLED           0x00080000
		#define SHARED_HW_CFG_FAN_FAILURE_ENABLED            0x00100000

		/* ASPM Power Management support */
	#define SHARED_HW_CFG_ASPM_SUPPORT_MASK             0x00600000
		#define SHARED_HW_CFG_ASPM_SUPPORT_SHIFT             21
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_L1_ENABLED    0x00000000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_DISABLED      0x00200000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L1_DISABLED       0x00400000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_L1_DISABLED   0x00600000

	/* The value of PM_TL_IGNORE_REQS (bit0) in PCI register
	   tl_control_0 (register 0x2800) */
	#define SHARED_HW_CFG_PREVENT_L1_ENTRY_MASK         0x00800000
		#define SHARED_HW_CFG_PREVENT_L1_ENTRY_DISABLED      0x00000000
		#define SHARED_HW_CFG_PREVENT_L1_ENTRY_ENABLED       0x00800000


	/*  Set the MDC/MDIO access for the first external phy */
	#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_MASK         0x1C000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_SHIFT         26
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_PHY_TYPE      0x00000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_EMAC0         0x04000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_EMAC1         0x08000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_BOTH          0x0c000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_SWAPPED       0x10000000

	/*  Set the MDC/MDIO access for the second external phy */
	#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_MASK         0xE0000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_SHIFT         29
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_PHY_TYPE      0x00000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_EMAC0         0x20000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_EMAC1         0x40000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_BOTH          0x60000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_SWAPPED       0x80000000

	/*  Max number of PF MSIX vectors */
	uint32_t config_3;                                       /* 0x11C */
	#define SHARED_HW_CFG_PF_MSIX_MAX_NUM_MASK                    0x0000007F
	#define SHARED_HW_CFG_PF_MSIX_MAX_NUM_SHIFT                   0

	/* This field extends the mf mode chosen in nvm cfg #73 (as we ran
	 * out of bits)
	 */
	#define SHARED_HW_CFG_EXTENDED_MF_MODE_MASK         0x00000F00
		#define SHARED_HW_CFG_EXTENDED_MF_MODE_SHIFT          8
		#define SHARED_HW_CFG_EXTENDED_MF_MODE_NPAR1_DOT_5    0x00000000
		#define SHARED_HW_CFG_EXTENDED_MF_MODE_NPAR2_DOT_0    0x00000100

	uint32_t ump_nc_si_config;			/* 0x120 */
	#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MASK       0x00000003
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_SHIFT       0
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MAC         0x00000000
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_PHY         0x00000001
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MII         0x00000000
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_RMII        0x00000002

	/* Reserved bits: 226-230 */

	/*  The output pin template BSC_SEL which selects the I2C for this
	port in the I2C Mux */
	uint32_t board;			/* 0x124 */
	#define SHARED_HW_CFG_E3_I2C_MUX0_MASK              0x0000003F
	    #define SHARED_HW_CFG_E3_I2C_MUX0_SHIFT              0

	#define SHARED_HW_CFG_E3_I2C_MUX1_MASK              0x00000FC0
	#define SHARED_HW_CFG_E3_I2C_MUX1_SHIFT                      6
	/* Use the PIN_CFG_XXX defines on top */
	#define SHARED_HW_CFG_BOARD_REV_MASK                0x00FF0000
	#define SHARED_HW_CFG_BOARD_REV_SHIFT                        16

	#define SHARED_HW_CFG_BOARD_MAJOR_VER_MASK          0x0F000000
	#define SHARED_HW_CFG_BOARD_MAJOR_VER_SHIFT                  24

	#define SHARED_HW_CFG_BOARD_MINOR_VER_MASK          0xF0000000
	#define SHARED_HW_CFG_BOARD_MINOR_VER_SHIFT                  28

	uint32_t wc_lane_config;				    /* 0x128 */
	#define SHARED_HW_CFG_LANE_SWAP_CFG_MASK            0x0000FFFF
		#define SHARED_HW_CFG_LANE_SWAP_CFG_SHIFT            0
		#define SHARED_HW_CFG_LANE_SWAP_CFG_32103210         0x00001b1b
		#define SHARED_HW_CFG_LANE_SWAP_CFG_32100123         0x00001be4
		#define SHARED_HW_CFG_LANE_SWAP_CFG_31200213         0x000027d8
		#define SHARED_HW_CFG_LANE_SWAP_CFG_02133120         0x0000d827
		#define SHARED_HW_CFG_LANE_SWAP_CFG_01233210         0x0000e41b
		#define SHARED_HW_CFG_LANE_SWAP_CFG_01230123         0x0000e4e4
	#define SHARED_HW_CFG_LANE_SWAP_CFG_TX_MASK         0x000000FF
	#define SHARED_HW_CFG_LANE_SWAP_CFG_TX_SHIFT                 0
	#define SHARED_HW_CFG_LANE_SWAP_CFG_RX_MASK         0x0000FF00
	#define SHARED_HW_CFG_LANE_SWAP_CFG_RX_SHIFT                 8

	/* TX lane Polarity swap */
	#define SHARED_HW_CFG_TX_LANE0_POL_FLIP_ENABLED     0x00010000
	#define SHARED_HW_CFG_TX_LANE1_POL_FLIP_ENABLED     0x00020000
	#define SHARED_HW_CFG_TX_LANE2_POL_FLIP_ENABLED     0x00040000
	#define SHARED_HW_CFG_TX_LANE3_POL_FLIP_ENABLED     0x00080000
	/* TX lane Polarity swap */
	#define SHARED_HW_CFG_RX_LANE0_POL_FLIP_ENABLED     0x00100000
	#define SHARED_HW_CFG_RX_LANE1_POL_FLIP_ENABLED     0x00200000
	#define SHARED_HW_CFG_RX_LANE2_POL_FLIP_ENABLED     0x00400000
	#define SHARED_HW_CFG_RX_LANE3_POL_FLIP_ENABLED     0x00800000

	/*  Selects the port layout of the board */
	#define SHARED_HW_CFG_E3_PORT_LAYOUT_MASK           0x0F000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_SHIFT           24
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_2P_01           0x00000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_2P_10           0x01000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_0123         0x02000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_1032         0x03000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_2301         0x04000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_3210         0x05000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_2P_01_SIG       0x06000000
};


/****************************************************************************
 * Port HW configuration                                                    *
 ****************************************************************************/
struct port_hw_cfg {		    /* port 0: 0x12c  port 1: 0x2bc */

	uint32_t pci_id;
	#define PORT_HW_CFG_PCI_DEVICE_ID_MASK              0x0000FFFF
	#define PORT_HW_CFG_PCI_DEVICE_ID_SHIFT             0

	#define PORT_HW_CFG_PCI_VENDOR_ID_MASK              0xFFFF0000
	#define PORT_HW_CFG_PCI_VENDOR_ID_SHIFT             16

	uint32_t pci_sub_id;
	#define PORT_HW_CFG_PCI_SUBSYS_VENDOR_ID_MASK       0x0000FFFF
	#define PORT_HW_CFG_PCI_SUBSYS_VENDOR_ID_SHIFT      0

	#define PORT_HW_CFG_PCI_SUBSYS_DEVICE_ID_MASK       0xFFFF0000
	#define PORT_HW_CFG_PCI_SUBSYS_DEVICE_ID_SHIFT      16

	uint32_t power_dissipated;
	#define PORT_HW_CFG_POWER_DIS_D0_MASK               0x000000FF
	#define PORT_HW_CFG_POWER_DIS_D0_SHIFT                       0
	#define PORT_HW_CFG_POWER_DIS_D1_MASK               0x0000FF00
	#define PORT_HW_CFG_POWER_DIS_D1_SHIFT                       8
	#define PORT_HW_CFG_POWER_DIS_D2_MASK               0x00FF0000
	#define PORT_HW_CFG_POWER_DIS_D2_SHIFT                       16
	#define PORT_HW_CFG_POWER_DIS_D3_MASK               0xFF000000
	#define PORT_HW_CFG_POWER_DIS_D3_SHIFT                       24

	uint32_t power_consumed;
	#define PORT_HW_CFG_POWER_CONS_D0_MASK              0x000000FF
	#define PORT_HW_CFG_POWER_CONS_D0_SHIFT                      0
	#define PORT_HW_CFG_POWER_CONS_D1_MASK              0x0000FF00
	#define PORT_HW_CFG_POWER_CONS_D1_SHIFT                      8
	#define PORT_HW_CFG_POWER_CONS_D2_MASK              0x00FF0000
	#define PORT_HW_CFG_POWER_CONS_D2_SHIFT                      16
	#define PORT_HW_CFG_POWER_CONS_D3_MASK              0xFF000000
	#define PORT_HW_CFG_POWER_CONS_D3_SHIFT                      24

	uint32_t mac_upper;
	uint32_t mac_lower;                                      /* 0x140 */
	#define PORT_HW_CFG_UPPERMAC_MASK                   0x0000FFFF
	#define PORT_HW_CFG_UPPERMAC_SHIFT                           0


	uint32_t iscsi_mac_upper;  /* Upper 16 bits are always zeroes */
	uint32_t iscsi_mac_lower;

	uint32_t rdma_mac_upper;   /* Upper 16 bits are always zeroes */
	uint32_t rdma_mac_lower;

	uint32_t serdes_config;
	#define PORT_HW_CFG_SERDES_TX_DRV_PRE_EMPHASIS_MASK 0x0000FFFF
	#define PORT_HW_CFG_SERDES_TX_DRV_PRE_EMPHASIS_SHIFT         0

	#define PORT_HW_CFG_SERDES_RX_DRV_EQUALIZER_MASK    0xFFFF0000
	#define PORT_HW_CFG_SERDES_RX_DRV_EQUALIZER_SHIFT            16


	/*  Default values: 2P-64, 4P-32 */
	uint32_t reserved;

	uint32_t vf_config;					    /* 0x15C */
	#define PORT_HW_CFG_VF_PCI_DEVICE_ID_MASK           0xFFFF0000
	#define PORT_HW_CFG_VF_PCI_DEVICE_ID_SHIFT                   16

	uint32_t mf_pci_id;					    /* 0x160 */
	#define PORT_HW_CFG_MF_PCI_DEVICE_ID_MASK           0x0000FFFF
	#define PORT_HW_CFG_MF_PCI_DEVICE_ID_SHIFT                   0

	/*  Controls the TX laser of the SFP+ module */
	uint32_t sfp_ctrl;					    /* 0x164 */
	#define PORT_HW_CFG_TX_LASER_MASK                   0x000000FF
		#define PORT_HW_CFG_TX_LASER_SHIFT                   0
		#define PORT_HW_CFG_TX_LASER_MDIO                    0x00000000
		#define PORT_HW_CFG_TX_LASER_GPIO0                   0x00000001
		#define PORT_HW_CFG_TX_LASER_GPIO1                   0x00000002
		#define PORT_HW_CFG_TX_LASER_GPIO2                   0x00000003
		#define PORT_HW_CFG_TX_LASER_GPIO3                   0x00000004

	/*  Controls the fault module LED of the SFP+ */
	#define PORT_HW_CFG_FAULT_MODULE_LED_MASK           0x0000FF00
		#define PORT_HW_CFG_FAULT_MODULE_LED_SHIFT           8
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO0           0x00000000
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO1           0x00000100
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO2           0x00000200
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO3           0x00000300
		#define PORT_HW_CFG_FAULT_MODULE_LED_DISABLED        0x00000400

	/*  The output pin TX_DIS that controls the TX laser of the SFP+
	  module. Use the PIN_CFG_XXX defines on top */
	uint32_t e3_sfp_ctrl;				    /* 0x168 */
	#define PORT_HW_CFG_E3_TX_LASER_MASK                0x000000FF
	#define PORT_HW_CFG_E3_TX_LASER_SHIFT                        0

	/*  The output pin for SFPP_TYPE which turns on the Fault module LED */
	#define PORT_HW_CFG_E3_FAULT_MDL_LED_MASK           0x0000FF00
	#define PORT_HW_CFG_E3_FAULT_MDL_LED_SHIFT                   8

	/*  The input pin MOD_ABS that indicates whether SFP+ module is
	  present or not. Use the PIN_CFG_XXX defines on top */
	#define PORT_HW_CFG_E3_MOD_ABS_MASK                 0x00FF0000
	#define PORT_HW_CFG_E3_MOD_ABS_SHIFT                         16

	/*  The output pin PWRDIS_SFP_X which disable the power of the SFP+
	  module. Use the PIN_CFG_XXX defines on top */
	#define PORT_HW_CFG_E3_PWR_DIS_MASK                 0xFF000000
	#define PORT_HW_CFG_E3_PWR_DIS_SHIFT                         24

	/*
	 * The input pin which signals module transmit fault. Use the
	 * PIN_CFG_XXX defines on top
	 */
	uint32_t e3_cmn_pin_cfg;				    /* 0x16C */
	#define PORT_HW_CFG_E3_TX_FAULT_MASK                0x000000FF
	#define PORT_HW_CFG_E3_TX_FAULT_SHIFT                        0

	/*  The output pin which reset the PHY. Use the PIN_CFG_XXX defines on
	 top */
	#define PORT_HW_CFG_E3_PHY_RESET_MASK               0x0000FF00
	#define PORT_HW_CFG_E3_PHY_RESET_SHIFT                       8

	/*
	 * The output pin which powers down the PHY. Use the PIN_CFG_XXX
	 * defines on top
	 */
	#define PORT_HW_CFG_E3_PWR_DOWN_MASK                0x00FF0000
	#define PORT_HW_CFG_E3_PWR_DOWN_SHIFT                        16

	/*  The output pin values BSC_SEL which selects the I2C for this port
	  in the I2C Mux */
	#define PORT_HW_CFG_E3_I2C_MUX0_MASK                0x01000000
	#define PORT_HW_CFG_E3_I2C_MUX1_MASK                0x02000000


	/*
	 * The input pin I_FAULT which indicate over-current has occurred.
	 * Use the PIN_CFG_XXX defines on top
	 */
	uint32_t e3_cmn_pin_cfg1;				    /* 0x170 */
	#define PORT_HW_CFG_E3_OVER_CURRENT_MASK            0x000000FF
	#define PORT_HW_CFG_E3_OVER_CURRENT_SHIFT                    0

	/*  pause on host ring */
	uint32_t generic_features;                               /* 0x174 */
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_MASK                   0x00000001
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_SHIFT                  0
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_DISABLED               0x00000000
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_ENABLED                0x00000001

	/* SFP+ Tx Equalization: NIC recommended and tested value is 0xBEB2
	 * LOM recommended and tested value is 0xBEB2. Using a different
	 * value means using a value not tested by BRCM
	 */
	uint32_t sfi_tap_values;                                 /* 0x178 */
	#define PORT_HW_CFG_TX_EQUALIZATION_MASK                      0x0000FFFF
	#define PORT_HW_CFG_TX_EQUALIZATION_SHIFT                     0

	/* SFP+ Tx driver broadcast IDRIVER: NIC recommended and tested
	 * value is 0x2. LOM recommended and tested value is 0x2. Using a
	 * different value means using a value not tested by BRCM
	 */
	#define PORT_HW_CFG_TX_DRV_BROADCAST_MASK                     0x000F0000
	#define PORT_HW_CFG_TX_DRV_BROADCAST_SHIFT                    16
	/*  Set non-default values for TXFIR in SFP mode. */
	#define PORT_HW_CFG_TX_DRV_IFIR_MASK                          0x00F00000
	#define PORT_HW_CFG_TX_DRV_IFIR_SHIFT                         20

	/*  Set non-default values for IPREDRIVER in SFP mode. */
	#define PORT_HW_CFG_TX_DRV_IPREDRIVER_MASK                    0x0F000000
	#define PORT_HW_CFG_TX_DRV_IPREDRIVER_SHIFT                   24

	/*  Set non-default values for POST2 in SFP mode. */
	#define PORT_HW_CFG_TX_DRV_POST2_MASK                         0xF0000000
	#define PORT_HW_CFG_TX_DRV_POST2_SHIFT                        28

	uint32_t reserved0[5];				    /* 0x17c */

	uint32_t aeu_int_mask;				    /* 0x190 */

	uint32_t media_type;					    /* 0x194 */
	#define PORT_HW_CFG_MEDIA_TYPE_PHY0_MASK            0x000000FF
	#define PORT_HW_CFG_MEDIA_TYPE_PHY0_SHIFT                    0

	#define PORT_HW_CFG_MEDIA_TYPE_PHY1_MASK            0x0000FF00
	#define PORT_HW_CFG_MEDIA_TYPE_PHY1_SHIFT                    8

	#define PORT_HW_CFG_MEDIA_TYPE_PHY2_MASK            0x00FF0000
	#define PORT_HW_CFG_MEDIA_TYPE_PHY2_SHIFT                    16

	/*  4 times 16 bits for all 4 lanes. In case external PHY is present
	      (not direct mode), those values will not take effect on the 4 XGXS
	      lanes. For some external PHYs (such as 8706 and 8726) the values
	      will be used to configure the external PHY  in those cases, not
	      all 4 values are needed. */
	uint16_t xgxs_config_rx[4];			/* 0x198 */
	uint16_t xgxs_config_tx[4];			/* 0x1A0 */


	/* For storing FCOE mac on shared memory */
	uint32_t fcoe_fip_mac_upper;
	#define PORT_HW_CFG_FCOE_UPPERMAC_MASK              0x0000ffff
	#define PORT_HW_CFG_FCOE_UPPERMAC_SHIFT                      0
	uint32_t fcoe_fip_mac_lower;

	uint32_t fcoe_wwn_port_name_upper;
	uint32_t fcoe_wwn_port_name_lower;

	uint32_t fcoe_wwn_node_name_upper;
	uint32_t fcoe_wwn_node_name_lower;

	/*  wwpn for npiv enabled */
	uint32_t wwpn_for_npiv_config;                           /* 0x1C0 */
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_MASK                0x00000001
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_SHIFT               0
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_DISABLED            0x00000000
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_ENABLED             0x00000001

	/*  wwpn for npiv valid addresses */
	uint32_t wwpn_for_npiv_valid_addresses;                  /* 0x1C4 */
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ADDRESS_BITMAP_MASK         0x0000FFFF
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ADDRESS_BITMAP_SHIFT        0

	struct mac_addr wwpn_for_niv_macs[16];

	/* Reserved bits: 2272-2336 For storing FCOE mac on shared memory */
	uint32_t Reserved1[14];

	uint32_t pf_allocation;                                  /* 0x280 */
	/* number of vfs per PF, if 0 - sriov disabled */
	#define PORT_HW_CFG_NUMBER_OF_VFS_MASK                        0x000000FF
	#define PORT_HW_CFG_NUMBER_OF_VFS_SHIFT                       0

	/*  Enable RJ45 magjack pair swapping on 10GBase-T PHY (0=default),
	      84833 only */
	uint32_t xgbt_phy_cfg;				    /* 0x284 */
	#define PORT_HW_CFG_RJ45_PAIR_SWAP_MASK             0x000000FF
	#define PORT_HW_CFG_RJ45_PAIR_SWAP_SHIFT                     0

		uint32_t default_cfg;			    /* 0x288 */
	#define PORT_HW_CFG_GPIO0_CONFIG_MASK               0x00000003
		#define PORT_HW_CFG_GPIO0_CONFIG_SHIFT               0
		#define PORT_HW_CFG_GPIO0_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO0_CONFIG_LOW                 0x00000001
		#define PORT_HW_CFG_GPIO0_CONFIG_HIGH                0x00000002
		#define PORT_HW_CFG_GPIO0_CONFIG_INPUT               0x00000003

	#define PORT_HW_CFG_GPIO1_CONFIG_MASK               0x0000000C
		#define PORT_HW_CFG_GPIO1_CONFIG_SHIFT               2
		#define PORT_HW_CFG_GPIO1_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO1_CONFIG_LOW                 0x00000004
		#define PORT_HW_CFG_GPIO1_CONFIG_HIGH                0x00000008
		#define PORT_HW_CFG_GPIO1_CONFIG_INPUT               0x0000000c

	#define PORT_HW_CFG_GPIO2_CONFIG_MASK               0x00000030
		#define PORT_HW_CFG_GPIO2_CONFIG_SHIFT               4
		#define PORT_HW_CFG_GPIO2_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO2_CONFIG_LOW                 0x00000010
		#define PORT_HW_CFG_GPIO2_CONFIG_HIGH                0x00000020
		#define PORT_HW_CFG_GPIO2_CONFIG_INPUT               0x00000030

	#define PORT_HW_CFG_GPIO3_CONFIG_MASK               0x000000C0
		#define PORT_HW_CFG_GPIO3_CONFIG_SHIFT               6
		#define PORT_HW_CFG_GPIO3_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO3_CONFIG_LOW                 0x00000040
		#define PORT_HW_CFG_GPIO3_CONFIG_HIGH                0x00000080
		#define PORT_HW_CFG_GPIO3_CONFIG_INPUT               0x000000c0

	/*  When KR link is required to be set to force which is not
	      KR-compliant, this parameter determine what is the trigger for it.
	      When GPIO is selected, low input will force the speed. Currently
	      default speed is 1G. In the future, it may be widen to select the
	      forced speed in with another parameter. Note when force-1G is
	      enabled, it override option 56: Link Speed option. */
	#define PORT_HW_CFG_FORCE_KR_ENABLER_MASK           0x00000F00
		#define PORT_HW_CFG_FORCE_KR_ENABLER_SHIFT           8
		#define PORT_HW_CFG_FORCE_KR_ENABLER_NOT_FORCED      0x00000000
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO0_P0        0x00000100
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO1_P0        0x00000200
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO2_P0        0x00000300
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO3_P0        0x00000400
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO0_P1        0x00000500
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO1_P1        0x00000600
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO2_P1        0x00000700
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO3_P1        0x00000800
		#define PORT_HW_CFG_FORCE_KR_ENABLER_FORCED          0x00000900
	/*  Enable to determine with which GPIO to reset the external phy */
	#define PORT_HW_CFG_EXT_PHY_GPIO_RST_MASK           0x000F0000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_SHIFT           16
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_PHY_TYPE        0x00000000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO0_P0        0x00010000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO1_P0        0x00020000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO2_P0        0x00030000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO3_P0        0x00040000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO0_P1        0x00050000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO1_P1        0x00060000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO2_P1        0x00070000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO3_P1        0x00080000

	/*  Enable BAM on KR */
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_MASK           0x00100000
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_SHIFT                   20
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_DISABLED                0x00000000
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_ENABLED                 0x00100000

	/*  Enable Common Mode Sense */
	#define PORT_HW_CFG_ENABLE_CMS_MASK                 0x00200000
	#define PORT_HW_CFG_ENABLE_CMS_SHIFT                         21
	#define PORT_HW_CFG_ENABLE_CMS_DISABLED                      0x00000000
	#define PORT_HW_CFG_ENABLE_CMS_ENABLED                       0x00200000

	/*  Determine the Serdes electrical interface   */
	#define PORT_HW_CFG_NET_SERDES_IF_MASK              0x0F000000
	#define PORT_HW_CFG_NET_SERDES_IF_SHIFT                      24
	#define PORT_HW_CFG_NET_SERDES_IF_SGMII                      0x00000000
	#define PORT_HW_CFG_NET_SERDES_IF_XFI                        0x01000000
	#define PORT_HW_CFG_NET_SERDES_IF_SFI                        0x02000000
	#define PORT_HW_CFG_NET_SERDES_IF_KR                         0x03000000
	#define PORT_HW_CFG_NET_SERDES_IF_DXGXS                      0x04000000
	#define PORT_HW_CFG_NET_SERDES_IF_KR2                        0x05000000

	/*  SFP+ main TAP and post TAP volumes */
	#define PORT_HW_CFG_TAP_LEVELS_MASK                           0x70000000
	#define PORT_HW_CFG_TAP_LEVELS_SHIFT                          28
	#define PORT_HW_CFG_TAP_LEVELS_POST_15_MAIN_43                0x00000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_14_MAIN_44                0x10000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_13_MAIN_45                0x20000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_12_MAIN_46                0x30000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_11_MAIN_47                0x40000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_10_MAIN_48                0x50000000

	uint32_t speed_capability_mask2;			    /* 0x28C */
	#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_MASK       0x0000FFFF
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_SHIFT       0
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10M_FULL    0x00000001
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10M_HALF    0x00000002
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_100M_HALF   0x00000004
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_100M_FULL   0x00000008
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_1G          0x00000010
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_2_5G        0x00000020
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10G         0x00000040
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_20G         0x00000080

	#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_MASK       0xFFFF0000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_SHIFT       16
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10M_FULL    0x00010000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10M_HALF    0x00020000
	    #define PORT_HW_CFG_SPEED_CAPABILITY2_D0_100M_HALF   0x00040000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_100M_FULL   0x00080000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_1G          0x00100000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_2_5G        0x00200000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10G         0x00400000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_20G         0x00800000


	/*  In the case where two media types (e.g. copper and fiber) are
	      present and electrically active at the same time, PHY Selection
	      will determine which of the two PHYs will be designated as the
	      Active PHY and used for a connection to the network.  */
	uint32_t multi_phy_config;				    /* 0x290 */
	#define PORT_HW_CFG_PHY_SELECTION_MASK              0x00000007
		#define PORT_HW_CFG_PHY_SELECTION_SHIFT              0
		#define PORT_HW_CFG_PHY_SELECTION_HARDWARE_DEFAULT   0x00000000
		#define PORT_HW_CFG_PHY_SELECTION_FIRST_PHY          0x00000001
		#define PORT_HW_CFG_PHY_SELECTION_SECOND_PHY         0x00000002
		#define PORT_HW_CFG_PHY_SELECTION_FIRST_PHY_PRIORITY 0x00000003
		#define PORT_HW_CFG_PHY_SELECTION_SECOND_PHY_PRIORITY 0x00000004

	/*  When enabled, all second phy nvram parameters will be swapped
	      with the first phy parameters */
	#define PORT_HW_CFG_PHY_SWAPPED_MASK                0x00000008
		#define PORT_HW_CFG_PHY_SWAPPED_SHIFT                3
		#define PORT_HW_CFG_PHY_SWAPPED_DISABLED             0x00000000
		#define PORT_HW_CFG_PHY_SWAPPED_ENABLED              0x00000008


	/*  Address of the second external phy */
	uint32_t external_phy_config2;			    /* 0x294 */
	#define PORT_HW_CFG_XGXS_EXT_PHY2_ADDR_MASK         0x000000FF
	#define PORT_HW_CFG_XGXS_EXT_PHY2_ADDR_SHIFT                 0

	/*  The second XGXS external PHY type */
	#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_MASK         0x0000FF00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_SHIFT         8
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_DIRECT        0x00000000
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8071       0x00000100
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8072       0x00000200
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8073       0x00000300
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8705       0x00000400
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8706       0x00000500
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8726       0x00000600
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8481       0x00000700
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_SFX7101       0x00000800
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8727       0x00000900
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8727_NOC   0x00000a00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X84823      0x00000b00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X54640      0x00000c00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X84833      0x00000d00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X54618SE    0x00000e00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X8722       0x00000f00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X54616      0x00001000
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X84834      0x00001100
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BNX2X84858    0x00001200
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_FAILURE       0x0000fd00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_NOT_CONN      0x0000ff00


	/*  4 times 16 bits for all 4 lanes. For some external PHYs (such as
	      8706, 8726 and 8727) not all 4 values are needed. */
	uint16_t xgxs_config2_rx[4];				    /* 0x296 */
	uint16_t xgxs_config2_tx[4];				    /* 0x2A0 */

	uint32_t lane_config;
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASK              0x0000FFFF
		#define PORT_HW_CFG_LANE_SWAP_CFG_SHIFT              0
		/* AN and forced */
		#define PORT_HW_CFG_LANE_SWAP_CFG_01230123           0x00001b1b
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_01233210           0x00001be4
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_31203120           0x0000d8d8
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_32103210           0x0000e4e4
	#define PORT_HW_CFG_LANE_SWAP_CFG_TX_MASK           0x000000FF
	#define PORT_HW_CFG_LANE_SWAP_CFG_TX_SHIFT                   0
	#define PORT_HW_CFG_LANE_SWAP_CFG_RX_MASK           0x0000FF00
	#define PORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT                   8
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK       0x0000C000
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT               14

	/*  Indicate whether to swap the external phy polarity */
	#define PORT_HW_CFG_SWAP_PHY_POLARITY_MASK          0x00010000
		#define PORT_HW_CFG_SWAP_PHY_POLARITY_DISABLED       0x00000000
		#define PORT_HW_CFG_SWAP_PHY_POLARITY_ENABLED        0x00010000


	uint32_t external_phy_config;
	#define PORT_HW_CFG_XGXS_EXT_PHY_ADDR_MASK          0x000000FF
	#define PORT_HW_CFG_XGXS_EXT_PHY_ADDR_SHIFT                  0

	#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK          0x0000FF00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SHIFT          8
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT         0x00000000
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8071        0x00000100
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8072        0x00000200
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8073        0x00000300
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8705        0x00000400
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8706        0x00000500
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8726        0x00000600
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8481        0x00000700
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101        0x00000800
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8727        0x00000900
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8727_NOC    0x00000a00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X84823       0x00000b00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X54640       0x00000c00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X84833       0x00000d00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X54618SE     0x00000e00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X8722        0x00000f00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X54616       0x00001000
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X84834       0x00001100
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X84858     0x00001200
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT_WC      0x0000fc00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE        0x0000fd00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN       0x0000ff00

	#define PORT_HW_CFG_SERDES_EXT_PHY_ADDR_MASK        0x00FF0000
	#define PORT_HW_CFG_SERDES_EXT_PHY_ADDR_SHIFT                16

	#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_MASK        0xFF000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_SHIFT        24
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT       0x00000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BNX2X5482      0x01000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT_SD    0x02000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_NOT_CONN     0xff000000

	uint32_t speed_capability_mask;
	#define PORT_HW_CFG_SPEED_CAPABILITY_D3_MASK        0x0000FFFF
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_SHIFT        0
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10M_FULL     0x00000001
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10M_HALF     0x00000002
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_100M_HALF    0x00000004
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_100M_FULL    0x00000008
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_1G           0x00000010
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_2_5G         0x00000020
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10G          0x00000040
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_20G          0x00000080
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_RESERVED     0x0000f000

	#define PORT_HW_CFG_SPEED_CAPABILITY_D0_MASK        0xFFFF0000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_SHIFT        16
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL     0x00010000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_HALF     0x00020000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_HALF    0x00040000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_FULL    0x00080000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_1G           0x00100000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G         0x00200000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10G          0x00400000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_20G          0x00800000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_RESERVED     0xf0000000

	/*  A place to hold the original MAC address as a backup */
	uint32_t backup_mac_upper;			/* 0x2B4 */
	uint32_t backup_mac_lower;			/* 0x2B8 */

};


/****************************************************************************
 * Shared Feature configuration                                             *
 ****************************************************************************/
struct shared_feat_cfg {		 /* NVRAM Offset */

	uint32_t config;			/* 0x450 */
	#define SHARED_FEATURE_BMC_ECHO_MODE_EN             0x00000001

	/* Use NVRAM values instead of HW default values */
	#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_MASK \
							    0x00000002
		#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_DISABLED \
								     0x00000000
		#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_ENABLED \
								     0x00000002

	#define SHARED_FEAT_CFG_NCSI_ID_METHOD_MASK         0x00000008
		#define SHARED_FEAT_CFG_NCSI_ID_METHOD_SPIO          0x00000000
		#define SHARED_FEAT_CFG_NCSI_ID_METHOD_NVRAM         0x00000008

	#define SHARED_FEAT_CFG_NCSI_ID_MASK                0x00000030
	#define SHARED_FEAT_CFG_NCSI_ID_SHIFT                        4

	/*  Override the OTP back to single function mode. When using GPIO,
	      high means only SF, 0 is according to CLP configuration */
	#define SHARED_FEAT_CFG_FORCE_SF_MODE_MASK          0x00000700
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SHIFT          8
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_MF_ALLOWED     0x00000000
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_FORCED_SF      0x00000100
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SPIO4          0x00000200
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SWITCH_INDEPT  0x00000300
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_AFEX_MODE      0x00000400
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_BD_MODE        0x00000500
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_UFP_MODE       0x00000600
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_EXTENDED_MODE  0x00000700

	/*  Act as if the FCoE license is invalid */
	#define SHARED_FEAT_CFG_PREVENT_FCOE                0x00001000

    /*  Force FLR capability to all ports */
	#define SHARED_FEAT_CFG_FORCE_FLR_CAPABILITY        0x00002000

	/*  Act as if the iSCSI license is invalid */
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_MASK                    0x00004000
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_SHIFT                   14
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_DISABLED                0x00000000
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_ENABLED                 0x00004000

	/* The interval in seconds between sending LLDP packets. Set to zero
	   to disable the feature */
	#define SHARED_FEAT_CFG_LLDP_XMIT_INTERVAL_MASK     0x00FF0000
	#define SHARED_FEAT_CFG_LLDP_XMIT_INTERVAL_SHIFT             16

	/* The assigned device type ID for LLDP usage */
	#define SHARED_FEAT_CFG_LLDP_DEVICE_TYPE_ID_MASK    0xFF000000
	#define SHARED_FEAT_CFG_LLDP_DEVICE_TYPE_ID_SHIFT            24

};


/****************************************************************************
 * Port Feature configuration                                               *
 ****************************************************************************/
struct port_feat_cfg {		    /* port 0: 0x454  port 1: 0x4c8 */

	uint32_t config;
	#define PORT_FEAT_CFG_BAR1_SIZE_MASK                 0x0000000F
		#define PORT_FEAT_CFG_BAR1_SIZE_SHIFT                 0
		#define PORT_FEAT_CFG_BAR1_SIZE_DISABLED              0x00000000
		#define PORT_FEAT_CFG_BAR1_SIZE_64K                   0x00000001
		#define PORT_FEAT_CFG_BAR1_SIZE_128K                  0x00000002
		#define PORT_FEAT_CFG_BAR1_SIZE_256K                  0x00000003
		#define PORT_FEAT_CFG_BAR1_SIZE_512K                  0x00000004
		#define PORT_FEAT_CFG_BAR1_SIZE_1M                    0x00000005
		#define PORT_FEAT_CFG_BAR1_SIZE_2M                    0x00000006
		#define PORT_FEAT_CFG_BAR1_SIZE_4M                    0x00000007
		#define PORT_FEAT_CFG_BAR1_SIZE_8M                    0x00000008
		#define PORT_FEAT_CFG_BAR1_SIZE_16M                   0x00000009
		#define PORT_FEAT_CFG_BAR1_SIZE_32M                   0x0000000a
		#define PORT_FEAT_CFG_BAR1_SIZE_64M                   0x0000000b
		#define PORT_FEAT_CFG_BAR1_SIZE_128M                  0x0000000c
		#define PORT_FEAT_CFG_BAR1_SIZE_256M                  0x0000000d
		#define PORT_FEAT_CFG_BAR1_SIZE_512M                  0x0000000e
		#define PORT_FEAT_CFG_BAR1_SIZE_1G                    0x0000000f
	#define PORT_FEAT_CFG_BAR2_SIZE_MASK                 0x000000F0
		#define PORT_FEAT_CFG_BAR2_SIZE_SHIFT                 4
		#define PORT_FEAT_CFG_BAR2_SIZE_DISABLED              0x00000000
		#define PORT_FEAT_CFG_BAR2_SIZE_64K                   0x00000010
		#define PORT_FEAT_CFG_BAR2_SIZE_128K                  0x00000020
		#define PORT_FEAT_CFG_BAR2_SIZE_256K                  0x00000030
		#define PORT_FEAT_CFG_BAR2_SIZE_512K                  0x00000040
		#define PORT_FEAT_CFG_BAR2_SIZE_1M                    0x00000050
		#define PORT_FEAT_CFG_BAR2_SIZE_2M                    0x00000060
		#define PORT_FEAT_CFG_BAR2_SIZE_4M                    0x00000070
		#define PORT_FEAT_CFG_BAR2_SIZE_8M                    0x00000080
		#define PORT_FEAT_CFG_BAR2_SIZE_16M                   0x00000090
		#define PORT_FEAT_CFG_BAR2_SIZE_32M                   0x000000a0
		#define PORT_FEAT_CFG_BAR2_SIZE_64M                   0x000000b0
		#define PORT_FEAT_CFG_BAR2_SIZE_128M                  0x000000c0
		#define PORT_FEAT_CFG_BAR2_SIZE_256M                  0x000000d0
		#define PORT_FEAT_CFG_BAR2_SIZE_512M                  0x000000e0
		#define PORT_FEAT_CFG_BAR2_SIZE_1G                    0x000000f0

	#define PORT_FEAT_CFG_DCBX_MASK                     0x00000100
		#define PORT_FEAT_CFG_DCBX_DISABLED                  0x00000000
		#define PORT_FEAT_CFG_DCBX_ENABLED                   0x00000100

    #define PORT_FEAT_CFG_AUTOGREEEN_MASK               0x00000200
	    #define PORT_FEAT_CFG_AUTOGREEEN_SHIFT               9
	    #define PORT_FEAT_CFG_AUTOGREEEN_DISABLED            0x00000000
	    #define PORT_FEAT_CFG_AUTOGREEEN_ENABLED             0x00000200

	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_MASK                0x00000C00
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_SHIFT               10
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_DEFAULT             0x00000000
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_FCOE                0x00000400
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_ISCSI               0x00000800
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_BOTH                0x00000c00

	#define PORT_FEAT_CFG_DCBX_SEL_MASK                           0x00003000
	#define PORT_FEAT_CFG_DCBX_SEL_SHIFT                          12
	#define PORT_FEAT_CFG_DCBX_SEL_CEE                            0x00000000
	#define PORT_FEAT_CFG_DCBX_SEL_IEEE                           0x00001000
	#define PORT_FEAT_CFG_DCBX_SEL_AUTO                           0x00002000

	#define PORT_FEATURE_EN_SIZE_MASK                   0x0f000000
	#define PORT_FEATURE_EN_SIZE_SHIFT                       24
	#define PORT_FEATURE_WOL_ENABLED                         0x01000000
	#define PORT_FEATURE_MBA_ENABLED                         0x02000000
	#define PORT_FEATURE_MFW_ENABLED                         0x04000000

	/* Advertise expansion ROM even if MBA is disabled */
	#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_MASK        0x08000000
		#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_DISABLED     0x00000000
		#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_ENABLED      0x08000000

	/* Check the optic vendor via i2c against a list of approved modules
	   in a separate nvram image */
	#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK         0xE0000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_SHIFT         29
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_NO_ENFORCEMENT \
								     0x00000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER \
								     0x20000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_WARNING_MSG   0x40000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_POWER_DOWN    0x60000000

	uint32_t wol_config;
	/* Default is used when driver sets to "auto" mode */
	#define PORT_FEATURE_WOL_ACPI_UPON_MGMT             0x00000010

	uint32_t mba_config;
	#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_MASK       0x00000007
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_SHIFT       0
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_PXE         0x00000000
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_RPL         0x00000001
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_BOOTP       0x00000002
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_ISCSIB      0x00000003
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_FCOE_BOOT   0x00000004
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_NONE        0x00000007

	#define PORT_FEATURE_MBA_BOOT_RETRY_MASK            0x00000038
	#define PORT_FEATURE_MBA_BOOT_RETRY_SHIFT                    3

    #define PORT_FEATURE_MBA_SETUP_PROMPT_ENABLE        0x00000400
	#define PORT_FEATURE_MBA_HOTKEY_MASK                0x00000800
		#define PORT_FEATURE_MBA_HOTKEY_CTRL_S               0x00000000
		#define PORT_FEATURE_MBA_HOTKEY_CTRL_B               0x00000800

	#define PORT_FEATURE_MBA_EXP_ROM_SIZE_MASK          0x000FF000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_SHIFT          12
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_DISABLED       0x00000000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_2K             0x00001000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_4K             0x00002000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_8K             0x00003000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_16K            0x00004000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_32K            0x00005000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_64K            0x00006000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_128K           0x00007000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_256K           0x00008000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_512K           0x00009000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_1M             0x0000a000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_2M             0x0000b000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_4M             0x0000c000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_8M             0x0000d000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_16M            0x0000e000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_32M            0x0000f000
	#define PORT_FEATURE_MBA_MSG_TIMEOUT_MASK           0x00F00000
	#define PORT_FEATURE_MBA_MSG_TIMEOUT_SHIFT                   20
	#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_MASK        0x03000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_SHIFT        24
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_AUTO         0x00000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_BBS          0x01000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_INT18H       0x02000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_INT19H       0x03000000
	#define PORT_FEATURE_MBA_LINK_SPEED_MASK            0x3C000000
		#define PORT_FEATURE_MBA_LINK_SPEED_SHIFT            26
		#define PORT_FEATURE_MBA_LINK_SPEED_AUTO             0x00000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10M_HALF         0x04000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10M_FULL         0x08000000
		#define PORT_FEATURE_MBA_LINK_SPEED_100M_HALF        0x0c000000
		#define PORT_FEATURE_MBA_LINK_SPEED_100M_FULL        0x10000000
		#define PORT_FEATURE_MBA_LINK_SPEED_1G               0x14000000
		#define PORT_FEATURE_MBA_LINK_SPEED_2_5G             0x18000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10G              0x1c000000
		#define PORT_FEATURE_MBA_LINK_SPEED_20G              0x20000000

	/* Secondary MBA configuration,
	 * see mba_config for the fileds defination.
	 */
	uint32_t mba_config2;

	uint32_t mba_vlan_cfg;
	#define PORT_FEATURE_MBA_VLAN_TAG_MASK              0x0000FFFF
	#define PORT_FEATURE_MBA_VLAN_TAG_SHIFT                      0
	#define PORT_FEATURE_MBA_VLAN_EN                    0x00010000
	#define PORT_FEATUTE_BOFM_CFGD_EN                   0x00020000
	#define PORT_FEATURE_BOFM_CFGD_FTGT                 0x00040000
	#define PORT_FEATURE_BOFM_CFGD_VEN                  0x00080000

	/* Secondary MBA configuration,
	 * see mba_vlan_cfg for the fileds defination.
	 */
	uint32_t mba_vlan_cfg2;

	uint32_t smbus_config;
	#define PORT_FEATURE_SMBUS_ADDR_MASK                0x000000fe
	#define PORT_FEATURE_SMBUS_ADDR_SHIFT                        1

	uint32_t vf_config;
	#define PORT_FEAT_CFG_VF_BAR2_SIZE_MASK             0x0000000F
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_SHIFT             0
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_DISABLED          0x00000000
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_4K                0x00000001
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_8K                0x00000002
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_16K               0x00000003
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_32K               0x00000004
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_64K               0x00000005
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_128K              0x00000006
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_256K              0x00000007
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_512K              0x00000008
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_1M                0x00000009
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_2M                0x0000000a
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_4M                0x0000000b
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_8M                0x0000000c
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_16M               0x0000000d
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_32M               0x0000000e
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_64M               0x0000000f

	uint32_t link_config;    /* Used as HW defaults for the driver */

    #define PORT_FEATURE_FLOW_CONTROL_MASK              0x00000700
		#define PORT_FEATURE_FLOW_CONTROL_SHIFT              8
		#define PORT_FEATURE_FLOW_CONTROL_AUTO               0x00000000
		#define PORT_FEATURE_FLOW_CONTROL_TX                 0x00000100
		#define PORT_FEATURE_FLOW_CONTROL_RX                 0x00000200
		#define PORT_FEATURE_FLOW_CONTROL_BOTH               0x00000300
		#define PORT_FEATURE_FLOW_CONTROL_NONE               0x00000400
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_RX            0x00000500
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_TX            0x00000600
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_BOTH          0x00000700

    #define PORT_FEATURE_LINK_SPEED_MASK                0x000F0000
		#define PORT_FEATURE_LINK_SPEED_SHIFT                16
		#define PORT_FEATURE_LINK_SPEED_AUTO                 0x00000000
		#define PORT_FEATURE_LINK_SPEED_10M_HALF             0x00010000
		#define PORT_FEATURE_LINK_SPEED_10M_FULL             0x00020000
		#define PORT_FEATURE_LINK_SPEED_100M_HALF            0x00030000
		#define PORT_FEATURE_LINK_SPEED_100M_FULL            0x00040000
		#define PORT_FEATURE_LINK_SPEED_1G                   0x00050000
		#define PORT_FEATURE_LINK_SPEED_2_5G                 0x00060000
		#define PORT_FEATURE_LINK_SPEED_10G_CX4              0x00070000
		#define PORT_FEATURE_LINK_SPEED_20G                  0x00080000

	#define PORT_FEATURE_CONNECTED_SWITCH_MASK          0x03000000
		#define PORT_FEATURE_CONNECTED_SWITCH_SHIFT          24
		/* (forced) low speed switch (< 10G) */
		#define PORT_FEATURE_CON_SWITCH_1G_SWITCH            0x00000000
		/* (forced) high speed switch (>= 10G) */
		#define PORT_FEATURE_CON_SWITCH_10G_SWITCH           0x01000000
		#define PORT_FEATURE_CON_SWITCH_AUTO_DETECT          0x02000000
		#define PORT_FEATURE_CON_SWITCH_ONE_TIME_DETECT      0x03000000


	/* The default for MCP link configuration,
	   uses the same defines as link_config */
	uint32_t mfw_wol_link_cfg;

	/* The default for the driver of the second external phy,
	   uses the same defines as link_config */
	uint32_t link_config2;				    /* 0x47C */

	/* The default for MCP of the second external phy,
	   uses the same defines as link_config */
	uint32_t mfw_wol_link_cfg2;				    /* 0x480 */


	/*  EEE power saving mode */
	uint32_t eee_power_mode;                                 /* 0x484 */
	#define PORT_FEAT_CFG_EEE_POWER_MODE_MASK                     0x000000FF
	#define PORT_FEAT_CFG_EEE_POWER_MODE_SHIFT                    0
	#define PORT_FEAT_CFG_EEE_POWER_MODE_DISABLED                 0x00000000
	#define PORT_FEAT_CFG_EEE_POWER_MODE_BALANCED                 0x00000001
	#define PORT_FEAT_CFG_EEE_POWER_MODE_AGGRESSIVE               0x00000002
	#define PORT_FEAT_CFG_EEE_POWER_MODE_LOW_LATENCY              0x00000003


	uint32_t Reserved2[16];                                  /* 0x48C */
};

/****************************************************************************
 * Device Information                                                       *
 ****************************************************************************/
struct shm_dev_info {				/* size */

	uint32_t    bc_rev; /* 8 bits each: major, minor, build */	       /* 4 */

	struct shared_hw_cfg     shared_hw_config;	      /* 40 */

	struct port_hw_cfg       port_hw_config[PORT_MAX];     /* 400*2=800 */

	struct shared_feat_cfg   shared_feature_config;		   /* 4 */

	struct port_feat_cfg     port_feature_config[PORT_MAX];/* 116*2=232 */

};

struct extended_dev_info_shared_cfg {             /* NVRAM OFFSET */

	/*  Threshold in celcius to start using the fan */
	uint32_t temperature_monitor1;                           /* 0x4000 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_THRESH_MASK     0x0000007F
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_THRESH_SHIFT    0

	/*  Threshold in celcius to shut down the board */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_THRESH_MASK    0x00007F00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_THRESH_SHIFT   8

	/*  EPIO of fan temperature status */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_MASK       0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_SHIFT      16
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_NA         0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO0      0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO1      0x00020000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO2      0x00030000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO3      0x00040000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO4      0x00050000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO5      0x00060000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO6      0x00070000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO7      0x00080000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO8      0x00090000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO9      0x000a0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO10     0x000b0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO11     0x000c0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO12     0x000d0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO13     0x000e0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO14     0x000f0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO15     0x00100000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO16     0x00110000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO17     0x00120000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO18     0x00130000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO19     0x00140000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO20     0x00150000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO21     0x00160000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO22     0x00170000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO23     0x00180000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO24     0x00190000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO25     0x001a0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO26     0x001b0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO27     0x001c0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO28     0x001d0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO29     0x001e0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO30     0x001f0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO31     0x00200000

	/*  EPIO of shut down temperature status */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_MASK      0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_SHIFT     24
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_NA        0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO0     0x01000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO1     0x02000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO2     0x03000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO3     0x04000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO4     0x05000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO5     0x06000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO6     0x07000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO7     0x08000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO8     0x09000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO9     0x0a000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO10    0x0b000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO11    0x0c000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO12    0x0d000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO13    0x0e000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO14    0x0f000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO15    0x10000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO16    0x11000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO17    0x12000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO18    0x13000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO19    0x14000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO20    0x15000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO21    0x16000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO22    0x17000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO23    0x18000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO24    0x19000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO25    0x1a000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO26    0x1b000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO27    0x1c000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO28    0x1d000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO29    0x1e000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO30    0x1f000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO31    0x20000000


	/*  EPIO of shut down temperature status */
	uint32_t temperature_monitor2;                           /* 0x4004 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_PERIOD_MASK         0x0000FFFF
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_PERIOD_SHIFT        0

	/*  Sensor interface - Disabled / BSC / In the future - SMBUS */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_INTERFACE_MASK    0x00030000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_INTERFACE_SHIFT   16
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_INTERFACE_DISABLED \
								      0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_INTERFACE_BSC     0x00010000

	/*  On Board Sensor Address */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_ADDR_MASK         0x03FC0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SENSOR_ADDR_SHIFT        18

	/*  MFW flavor to be used */
	uint32_t mfw_cfg;                                        /* 0x4008 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_MASK          0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_SHIFT         0
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_NA            0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_A             0x00000001

	/*  Should NIC data query remain enabled upon last drv unload */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_MASK     0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_SHIFT    8
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_DISABLED 0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_ENABLED  0x00000100

	/*  Prevent OCBB feature */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_PREVENT_MASK        0x00000200
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_PREVENT_SHIFT       9
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_PREVENT_DISABLED    0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_PREVENT_ENABLED     0x00000200

	/*  Enable DCi support */
	#define EXTENDED_DEV_INFO_SHARED_CFG_DCI_SUPPORT_MASK         0x00000400
	#define EXTENDED_DEV_INFO_SHARED_CFG_DCI_SUPPORT_SHIFT        10
	#define EXTENDED_DEV_INFO_SHARED_CFG_DCI_SUPPORT_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DCI_SUPPORT_ENABLED      0x00000400

	/*  Reserved bits: 75 */

	/*  PLDM support over MCTP */
	#define EXTENDED_DEV_INFO_SHARED_CFG_PLDM_ENABLE_MASK         0x00001000
	#define EXTENDED_DEV_INFO_SHARED_CFG_PLDM_ENABLE_SHIFT        12
	#define EXTENDED_DEV_INFO_SHARED_CFG_PLDM_ENABLE_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_PLDM_ENABLE_ENABLED      0x00001000

	/*  Option to Disable embedded LLDP, 0 - Off, 1 - On */
	#define EXTENDED_DEV_INFO_SHARED_CFG_LLDP_DISABLE_MASK        0x00002000
	#define EXTENDED_DEV_INFO_SHARED_CFG_LLDP_DISABLE_SHIFT       13
	#define EXTENDED_DEV_INFO_SHARED_CFG_LLDP_DISABLE_OFF         0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_LLDP_DISABLE_ON          0x00002000

	/*  Hide DCBX feature in CCM/BACS menus */
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_MASK      0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_SHIFT     16
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_DISABLED  0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_ENABLED   0x00010000

	uint32_t smbus_config;                                   /* 0x400C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SMBUS_ADDR_MASK          0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_SMBUS_ADDR_SHIFT         0

	/*  Switching regulator loop gain */
	uint32_t board_cfg;                                      /* 0x4010 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_MASK           0x0000000F
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_SHIFT          0
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_HW_DEFAULT     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X2             0x00000008
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X4             0x00000009
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X8             0x0000000a
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X16            0x0000000b
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV8           0x0000000c
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV4           0x0000000d
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV2           0x0000000e
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X1             0x0000000f

	/*  whether shadow swim feature is supported */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_MASK         0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_SHIFT        8
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_ENABLED      0x00000100

    /*  whether to show/hide SRIOV menu in CCM */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU_MASK     0x00000200
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU_SHIFT    9
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU          0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_HIDE_MENU          0x00000200

	/*  Override PCIE revision ID when enabled the,
	 *  revision ID will set to B1=='0x11'
	 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_MASK          0x00000400
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_SHIFT         10
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_DISABLED      0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_ENABLED       0x00000400

	/*  Bypass slicer offset tuning */
	#define EXTENDED_DEV_INFO_SHARED_CFG_BYPASS_SLICER_MASK       0x00000800
	#define EXTENDED_DEV_INFO_SHARED_CFG_BYPASS_SLICER_SHIFT      11
	#define EXTENDED_DEV_INFO_SHARED_CFG_BYPASS_SLICER_DISABLED   0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_BYPASS_SLICER_ENABLED    0x00000800
	/*  Control Revision ID */
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_MASK         0x00003000
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_SHIFT        12
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_PRESERVE     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_ACTUAL       0x00001000
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_FORCE_B0     0x00002000
	#define EXTENDED_DEV_INFO_SHARED_CFG_REV_ID_CTRL_FORCE_B1     0x00003000
	/*  Threshold in celcius for max continuous operation */
	uint32_t temperature_report;                             /* 0x4014 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_MCOT_MASK           0x0000007F
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_MCOT_SHIFT          0

	/*  Threshold in celcius for sensor caution */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SCT_MASK            0x00007F00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SCT_SHIFT           8

	/*  wwn node prefix to be used (unless value is 0) */
	uint32_t wwn_prefix;                                     /* 0x4018 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX0_MASK    0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX0_SHIFT   0

	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX1_MASK    0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX1_SHIFT   8

	/*  wwn port prefix to be used (unless value is 0) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX0_MASK    0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX0_SHIFT   16

	/*  wwn port prefix to be used (unless value is 0) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX1_MASK    0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX1_SHIFT   24

	/*  General debug nvm cfg */
	uint32_t dbg_cfg_flags;                                  /* 0x401C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_MASK                 0x000FFFFF
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SHIFT                0
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_ENABLE               0x00000001
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_EN_SIGDET_FILTER     0x00000002
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_LP_TX_PRESET7    0x00000004
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_TX_ANA_DEFAULT   0x00000008
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_PLL_ANA_DEFAULT  0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FORCE_G1PLL_RETUNE   0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_RX_ANA_DEFAULT   0x00000040
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FORCE_SERDES_RX_CLK  0x00000080
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_DIS_RX_LP_EIEOS      0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FINALIZE_UCODE       0x00000200
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_HOLDOFF_REQ          0x00000400
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_OVERRIDE   0x00000800
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_GP_PORG_UC_RESET     0x00001000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SUPPRESS_COMPEN_EVT  0x00002000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_ADJ_TXEQ_P0_P1       0x00004000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_G3_PLL_RETUNE        0x00008000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_MAC_PHY_CTL8     0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_DIS_MAC_G3_FRM_ERR   0x00020000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_INFERRED_EI          0x00040000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_GEN3_COMPLI_ENA      0x00080000

	/*  Override Rx signal detect threshold when enabled the threshold
	 * will be set staticaly
	 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_RX_SIG_MASK     0x00100000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_RX_SIG_SHIFT    20
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_RX_SIG_DISABLED 0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_RX_SIG_ENABLED  0x00100000

	/*  Debug signet rx threshold */
	uint32_t dbg_rx_sigdet_threshold;                        /* 0x4020 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_MASK       0x00000007
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_SHIFT      0

    /*  Enable IFFE feature */
	uint32_t iffe_features;                                  /* 0x4024 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_MASK         0x00000001
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_SHIFT        0
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_ENABLED      0x00000001

	/*  Allowable port enablement (bitmask for ports 3-1) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_PORT_MASK       0x0000000E
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_PORT_SHIFT      1

	/*  Allow iSCSI offload override */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_MASK      0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_SHIFT     4
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_DISABLED  0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_ENABLED   0x00000010

	/*  Allow FCoE offload override */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_MASK       0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_SHIFT      5
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_DISABLED   0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_ENABLED    0x00000020

	/*  Tie to adaptor */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_MASK         0x00008000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_SHIFT        15
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_ENABLED      0x00008000

	/*  Currently enabled port(s) (bitmask for ports 3-1) */
	uint32_t current_iffe_mask;                              /* 0x4028 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_CFG_MASK         0x0000000E
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_CFG_SHIFT        1

	/*  Current iSCSI offload  */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_MASK       0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_SHIFT      4
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_DISABLED   0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_ENABLED    0x00000010

	/*  Current FCoE offload  */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_MASK        0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_SHIFT       5
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_DISABLED    0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_ENABLED     0x00000020

	/* FW set this pin to "0" (assert) these signal if either of its MAC
	 * or PHY specific threshold values is exceeded.
	 * Values are standard GPIO/EPIO pins.
	 */
	uint32_t threshold_pin;                                  /* 0x402C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCONTROL_PIN_MASK        0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCONTROL_PIN_SHIFT       0
	#define EXTENDED_DEV_INFO_SHARED_CFG_TWARNING_PIN_MASK        0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TWARNING_PIN_SHIFT       8
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCRITICAL_PIN_MASK       0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCRITICAL_PIN_SHIFT      16

	/* MAC die temperature threshold in Celsius. */
	uint32_t mac_threshold_val;                              /* 0x4030 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_MAC_THRESH_MASK  0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_MAC_THRESH_SHIFT 0
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_MAC_THRESH_MASK  0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_MAC_THRESH_SHIFT 8
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_MAC_THRESH_MASK 0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_MAC_THRESH_SHIFT 16

	/*  PHY die temperature threshold in Celsius. */
	uint32_t phy_threshold_val;                              /* 0x4034 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_PHY_THRESH_MASK  0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_PHY_THRESH_SHIFT 0
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_PHY_THRESH_MASK  0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_PHY_THRESH_SHIFT 8
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_PHY_THRESH_MASK 0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_PHY_THRESH_SHIFT 16

	/* External pins to communicate with host.
	 * Values are standard GPIO/EPIO pins.
	 */
	uint32_t host_pin;                                       /* 0x4038 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_I2C_ISOLATE_MASK         0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_I2C_ISOLATE_SHIFT        0
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_FAULT_MASK          0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_FAULT_SHIFT         8
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_VPD_UPDATE_MASK     0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_VPD_UPDATE_SHIFT    16
	#define EXTENDED_DEV_INFO_SHARED_CFG_VPD_CACHE_COMP_MASK      0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_VPD_CACHE_COMP_SHIFT     24

	/*  Manufacture kit version */
	uint32_t manufacture_ver;                                /* 0x403C */

	/*  Manufacture timestamp */
	uint32_t manufacture_data;                               /* 0x4040 */

	/*  Number of ISCSI/FCOE cfg images */
	#define EXTENDED_DEV_INFO_SHARED_CFG_NUM_ISCSI_FCOE_CFGS_MASK 0x00040000
	#define EXTENDED_DEV_INFO_SHARED_CFG_NUM_ISCSI_FCOE_CFGS_SHIFT18
	#define EXTENDED_DEV_INFO_SHARED_CFG_NUM_ISCSI_FCOE_CFGS_2    0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_NUM_ISCSI_FCOE_CFGS_4    0x00040000

	/*  MCP crash dump trigger */
	uint32_t mcp_crash_dump;                                 /* 0x4044 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRASH_DUMP_MASK          0x7FFFFFFF
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRASH_DUMP_SHIFT         0
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRASH_DUMP_DISABLED      0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRASH_DUMP_ENABLED       0x00000001

	/*  MBI version */
	uint32_t mbi_version;                                    /* 0x4048 */

	/*  MBI date */
	uint32_t mbi_date;                                       /* 0x404C */
};


#if !defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
	#error "Missing either LITTLE_ENDIAN or BIG_ENDIAN definition."
#endif

#define FUNC_0              0
#define FUNC_1              1
#define FUNC_2              2
#define FUNC_3              3
#define FUNC_4              4
#define FUNC_5              5
#define FUNC_6              6
#define FUNC_7              7
#define E1_FUNC_MAX         2
#define E1H_FUNC_MAX            8
#define E2_FUNC_MAX         4   /* per path */

#define VN_0                0
#define VN_1                1
#define VN_2                2
#define VN_3                3
#define E1VN_MAX            1
#define E1HVN_MAX           4

#define E2_VF_MAX           64  /* HC_REG_VF_CONFIGURATION_SIZE */
/* This value (in milliseconds) determines the frequency of the driver
 * issuing the PULSE message code.  The firmware monitors this periodic
 * pulse to determine when to switch to an OS-absent mode. */
#define DRV_PULSE_PERIOD_MS     250

/* This value (in milliseconds) determines how long the driver should
 * wait for an acknowledgement from the firmware before timing out.  Once
 * the firmware has timed out, the driver will assume there is no firmware
 * running and there won't be any firmware-driver synchronization during a
 * driver reset. */
#define FW_ACK_TIME_OUT_MS      5000

#define FW_ACK_POLL_TIME_MS     1

#define FW_ACK_NUM_OF_POLL  (FW_ACK_TIME_OUT_MS/FW_ACK_POLL_TIME_MS)

#define MFW_TRACE_SIGNATURE     0x54524342

/****************************************************************************
 * Driver <-> FW Mailbox                                                    *
 ****************************************************************************/
struct drv_port_mb {

	uint32_t link_status;
	/* Driver should update this field on any link change event */

	#define LINK_STATUS_NONE				(0<<0)
	#define LINK_STATUS_LINK_FLAG_MASK			0x00000001
	#define LINK_STATUS_LINK_UP				0x00000001
	#define LINK_STATUS_SPEED_AND_DUPLEX_MASK		0x0000001E
	#define LINK_STATUS_SPEED_AND_DUPLEX_AN_NOT_COMPLETE	(0<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10THD		(1<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10TFD		(2<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100TXHD		(3<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100T4		(4<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100TXFD		(5<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000THD		(6<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000TFD		(7<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000XFD		(7<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500THD		(8<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500TFD		(9<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500XFD		(9<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10GTFD		(10<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10GXFD		(10<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_20GTFD		(11<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_20GXFD		(11<<1)

	#define LINK_STATUS_AUTO_NEGOTIATE_FLAG_MASK		0x00000020
	#define LINK_STATUS_AUTO_NEGOTIATE_ENABLED		0x00000020

	#define LINK_STATUS_AUTO_NEGOTIATE_COMPLETE		0x00000040
	#define LINK_STATUS_PARALLEL_DETECTION_FLAG_MASK	0x00000080
	#define LINK_STATUS_PARALLEL_DETECTION_USED		0x00000080

	#define LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE	0x00000200
	#define LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE	0x00000400
	#define LINK_STATUS_LINK_PARTNER_100T4_CAPABLE		0x00000800
	#define LINK_STATUS_LINK_PARTNER_100TXFD_CAPABLE	0x00001000
	#define LINK_STATUS_LINK_PARTNER_100TXHD_CAPABLE	0x00002000
	#define LINK_STATUS_LINK_PARTNER_10TFD_CAPABLE		0x00004000
	#define LINK_STATUS_LINK_PARTNER_10THD_CAPABLE		0x00008000

	#define LINK_STATUS_TX_FLOW_CONTROL_FLAG_MASK		0x00010000
	#define LINK_STATUS_TX_FLOW_CONTROL_ENABLED		0x00010000

	#define LINK_STATUS_RX_FLOW_CONTROL_FLAG_MASK		0x00020000
	#define LINK_STATUS_RX_FLOW_CONTROL_ENABLED		0x00020000

	#define LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK	0x000C0000
	#define LINK_STATUS_LINK_PARTNER_NOT_PAUSE_CAPABLE	(0<<18)
	#define LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE	(1<<18)
	#define LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE	(2<<18)
	#define LINK_STATUS_LINK_PARTNER_BOTH_PAUSE		(3<<18)

	#define LINK_STATUS_SERDES_LINK				0x00100000

	#define LINK_STATUS_LINK_PARTNER_2500XFD_CAPABLE	0x00200000
	#define LINK_STATUS_LINK_PARTNER_2500XHD_CAPABLE	0x00400000
	#define LINK_STATUS_LINK_PARTNER_10GXFD_CAPABLE		0x00800000
	#define LINK_STATUS_LINK_PARTNER_20GXFD_CAPABLE		0x10000000

	#define LINK_STATUS_PFC_ENABLED				0x20000000

	#define LINK_STATUS_PHYSICAL_LINK_FLAG			0x40000000
	#define LINK_STATUS_SFP_TX_FAULT			0x80000000

	uint32_t port_stx;

	uint32_t stat_nig_timer;

	/* MCP firmware does not use this field */
	uint32_t ext_phy_fw_version;

};


struct drv_func_mb {

	uint32_t drv_mb_header;
	#define DRV_MSG_CODE_MASK                       0xffff0000
	#define DRV_MSG_CODE_LOAD_REQ                   0x10000000
	#define DRV_MSG_CODE_LOAD_DONE                  0x11000000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_EN          0x20000000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS         0x20010000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP         0x20020000
	#define DRV_MSG_CODE_UNLOAD_DONE                0x21000000
	#define DRV_MSG_CODE_DCC_OK                     0x30000000
	#define DRV_MSG_CODE_DCC_FAILURE                0x31000000
	#define DRV_MSG_CODE_DIAG_ENTER_REQ             0x50000000
	#define DRV_MSG_CODE_DIAG_EXIT_REQ              0x60000000
	#define DRV_MSG_CODE_VALIDATE_KEY               0x70000000
	#define DRV_MSG_CODE_GET_CURR_KEY               0x80000000
	#define DRV_MSG_CODE_GET_UPGRADE_KEY            0x81000000
	#define DRV_MSG_CODE_GET_MANUF_KEY              0x82000000
	#define DRV_MSG_CODE_LOAD_L2B_PRAM              0x90000000
	#define DRV_MSG_CODE_OEM_OK			0x00010000
	#define DRV_MSG_CODE_OEM_FAILURE		0x00020000
	#define DRV_MSG_CODE_OEM_UPDATE_SVID_OK		0x00030000
	#define DRV_MSG_CODE_OEM_UPDATE_SVID_FAILURE	0x00040000

	/*
	 * The optic module verification command requires bootcode
	 * v5.0.6 or later, te specific optic module verification command
	 * requires bootcode v5.2.12 or later
	 */
	#define DRV_MSG_CODE_VRFY_FIRST_PHY_OPT_MDL     0xa0000000
	#define REQ_BC_VER_4_VRFY_FIRST_PHY_OPT_MDL     0x00050006
	#define DRV_MSG_CODE_VRFY_SPECIFIC_PHY_OPT_MDL  0xa1000000
	#define REQ_BC_VER_4_VRFY_SPECIFIC_PHY_OPT_MDL  0x00050234
	#define DRV_MSG_CODE_VRFY_AFEX_SUPPORTED        0xa2000000
	#define REQ_BC_VER_4_VRFY_AFEX_SUPPORTED        0x00070002
	#define REQ_BC_VER_4_SFP_TX_DISABLE_SUPPORTED   0x00070014
	#define REQ_BC_VER_4_MT_SUPPORTED               0x00070201
	#define REQ_BC_VER_4_PFC_STATS_SUPPORTED        0x00070201
	#define REQ_BC_VER_4_FCOE_FEATURES              0x00070209

	#define DRV_MSG_CODE_DCBX_ADMIN_PMF_MSG         0xb0000000
	#define DRV_MSG_CODE_DCBX_PMF_DRV_OK            0xb2000000
	#define REQ_BC_VER_4_DCBX_ADMIN_MSG_NON_PMF     0x00070401

	#define DRV_MSG_CODE_VF_DISABLED_DONE           0xc0000000

	#define DRV_MSG_CODE_AFEX_DRIVER_SETMAC         0xd0000000
	#define DRV_MSG_CODE_AFEX_LISTGET_ACK           0xd1000000
	#define DRV_MSG_CODE_AFEX_LISTSET_ACK           0xd2000000
	#define DRV_MSG_CODE_AFEX_STATSGET_ACK          0xd3000000
	#define DRV_MSG_CODE_AFEX_VIFSET_ACK            0xd4000000

	#define DRV_MSG_CODE_DRV_INFO_ACK               0xd8000000
	#define DRV_MSG_CODE_DRV_INFO_NACK              0xd9000000

	#define DRV_MSG_CODE_EEE_RESULTS_ACK            0xda000000

	#define DRV_MSG_CODE_RMMOD                      0xdb000000
	#define REQ_BC_VER_4_RMMOD_CMD                  0x0007080f

	#define DRV_MSG_CODE_SET_MF_BW                  0xe0000000
	#define REQ_BC_VER_4_SET_MF_BW                  0x00060202
	#define DRV_MSG_CODE_SET_MF_BW_ACK              0xe1000000

	#define DRV_MSG_CODE_LINK_STATUS_CHANGED        0x01000000

	#define DRV_MSG_CODE_INITIATE_FLR               0x02000000
	#define REQ_BC_VER_4_INITIATE_FLR               0x00070213

	#define BIOS_MSG_CODE_LIC_CHALLENGE             0xff010000
	#define BIOS_MSG_CODE_LIC_RESPONSE              0xff020000
	#define BIOS_MSG_CODE_VIRT_MAC_PRIM             0xff030000
	#define BIOS_MSG_CODE_VIRT_MAC_ISCSI            0xff040000

	#define DRV_MSG_CODE_IMG_OFFSET_REQ             0xe2000000
	#define DRV_MSG_CODE_IMG_SIZE_REQ               0xe3000000

	#define DRV_MSG_CODE_UFP_CONFIG_ACK             0xe4000000

	#define DRV_MSG_SEQ_NUMBER_MASK                 0x0000ffff

	#define DRV_MSG_CODE_CONFIG_CHANGE              0xC1000000

	#define DRV_MSG_CODE_UPDATE_DRIVER_STATE        0xC2000000
	#define REQ_BC_VER_4_UPDATE_DRIVER_STATE        0x00070f35

	uint32_t drv_mb_param;
	#define DRV_MSG_CODE_SET_MF_BW_MIN_MASK         0x00ff0000
	#define DRV_MSG_CODE_SET_MF_BW_MAX_MASK         0xff000000

	#define DRV_MSG_CODE_UNLOAD_NON_D3_POWER        0x00000001
	#define DRV_MSG_CODE_UNLOAD_SKIP_LINK_RESET     0x00000002

	#define DRV_MSG_CODE_LOAD_REQ_WITH_LFA          0x0000100a
	#define DRV_MSG_CODE_LOAD_REQ_FORCE_LFA         0x00002000

	#define DRV_MSG_CODE_USR_BLK_IMAGE_REQ          0x00000001
	#define DRV_MSG_CODE_ISCSI_PERS_IMAGE_REQ       0x00000002
	#define DRV_MSG_CODE_VPD_IMAGE_REQ              0x00000003
	#define DRV_MSG_CODE_VLAN_TABLE_IMAGE_REQ       0x00000004

	#define DRV_MSG_CODE_CONFIG_CHANGE_MTU_SIZE     0x00000001
	#define DRV_MSG_CODE_CONFIG_CHANGE_MAC_ADD      0x00000002
	#define DRV_MSG_CODE_CONFIG_CHANGE_WOL_ENA      0x00000003
	#define DRV_MSG_CODE_CONFIG_CHANGE_ISCI_BOOT    0x00000004
	#define DRV_MSG_CODE_CONFIG_CHANGE_FCOE_BOOT    0x00000005
	#define DRV_MSG_CODE_CONFIG_CHANGE_RST2DFT      0x00000006

	#define DRV_MSG_CODE_DRIVER_STATE_UNKNOWN       0x00000001
	#define DRV_MSG_CODE_DRIVER_STATE_NOT_LOADED    0x00000002
	#define DRV_MSG_CODE_DRIVER_STATE_LOADING       0x00000003
	#define DRV_MSG_CODE_DRIVER_STATE_DISABLED      0x00000004
	#define DRV_MSG_CODE_DRIVER_STATE_ACTIVE        0x00000005

	uint32_t fw_mb_header;
	#define FW_MSG_CODE_MASK                        0xffff0000
	#define FW_MSG_CODE_DRV_LOAD_COMMON             0x10100000
	#define FW_MSG_CODE_DRV_LOAD_PORT               0x10110000
	#define FW_MSG_CODE_DRV_LOAD_FUNCTION           0x10120000
	/* Load common chip is supported from bc 6.0.0  */
	#define REQ_BC_VER_4_DRV_LOAD_COMMON_CHIP       0x00060000
	#define FW_MSG_CODE_DRV_LOAD_COMMON_CHIP        0x10130000

	#define FW_MSG_CODE_DRV_LOAD_REFUSED            0x10200000
	#define FW_MSG_CODE_DRV_LOAD_DONE               0x11100000
	#define FW_MSG_CODE_DRV_UNLOAD_COMMON           0x20100000
	#define FW_MSG_CODE_DRV_UNLOAD_PORT             0x20110000
	#define FW_MSG_CODE_DRV_UNLOAD_FUNCTION         0x20120000
	#define FW_MSG_CODE_DRV_UNLOAD_DONE             0x21100000
	#define FW_MSG_CODE_DCC_DONE                    0x30100000
	#define FW_MSG_CODE_LLDP_DONE                   0x40100000
	#define FW_MSG_CODE_DIAG_ENTER_DONE             0x50100000
	#define FW_MSG_CODE_DIAG_REFUSE                 0x50200000
	#define FW_MSG_CODE_DIAG_EXIT_DONE              0x60100000
	#define FW_MSG_CODE_VALIDATE_KEY_SUCCESS        0x70100000
	#define FW_MSG_CODE_VALIDATE_KEY_FAILURE        0x70200000
	#define FW_MSG_CODE_GET_KEY_DONE                0x80100000
	#define FW_MSG_CODE_NO_KEY                      0x80f00000
	#define FW_MSG_CODE_LIC_INFO_NOT_READY          0x80f80000
	#define FW_MSG_CODE_L2B_PRAM_LOADED             0x90100000
	#define FW_MSG_CODE_L2B_PRAM_T_LOAD_FAILURE     0x90210000
	#define FW_MSG_CODE_L2B_PRAM_C_LOAD_FAILURE     0x90220000
	#define FW_MSG_CODE_L2B_PRAM_X_LOAD_FAILURE     0x90230000
	#define FW_MSG_CODE_L2B_PRAM_U_LOAD_FAILURE     0x90240000
	#define FW_MSG_CODE_VRFY_OPT_MDL_SUCCESS        0xa0100000
	#define FW_MSG_CODE_VRFY_OPT_MDL_INVLD_IMG      0xa0200000
	#define FW_MSG_CODE_VRFY_OPT_MDL_UNAPPROVED     0xa0300000
	#define FW_MSG_CODE_VF_DISABLED_DONE            0xb0000000
	#define FW_MSG_CODE_HW_SET_INVALID_IMAGE        0xb0100000

	#define FW_MSG_CODE_AFEX_DRIVER_SETMAC_DONE     0xd0100000
	#define FW_MSG_CODE_AFEX_LISTGET_ACK            0xd1100000
	#define FW_MSG_CODE_AFEX_LISTSET_ACK            0xd2100000
	#define FW_MSG_CODE_AFEX_STATSGET_ACK           0xd3100000
	#define FW_MSG_CODE_AFEX_VIFSET_ACK             0xd4100000

	#define FW_MSG_CODE_DRV_INFO_ACK                0xd8100000
	#define FW_MSG_CODE_DRV_INFO_NACK               0xd9100000

	#define FW_MSG_CODE_EEE_RESULS_ACK              0xda100000

	#define FW_MSG_CODE_RMMOD_ACK                   0xdb100000

	#define FW_MSG_CODE_SET_MF_BW_SENT              0xe0000000
	#define FW_MSG_CODE_SET_MF_BW_DONE              0xe1000000

	#define FW_MSG_CODE_LINK_CHANGED_ACK            0x01100000

	#define FW_MSG_CODE_FLR_ACK                     0x02000000
	#define FW_MSG_CODE_FLR_NACK                    0x02100000

	#define FW_MSG_CODE_LIC_CHALLENGE               0xff010000
	#define FW_MSG_CODE_LIC_RESPONSE                0xff020000
	#define FW_MSG_CODE_VIRT_MAC_PRIM               0xff030000
	#define FW_MSG_CODE_VIRT_MAC_ISCSI              0xff040000

	#define FW_MSG_CODE_IMG_OFFSET_RESPONSE         0xe2100000
	#define FW_MSG_CODE_IMG_SIZE_RESPONSE           0xe3100000

	#define FW_MSG_CODE_OEM_ACK			0x00010000
	#define DRV_MSG_CODE_OEM_UPDATE_SVID_ACK	0x00020000

	#define FW_MSG_CODE_CONFIG_CHANGE_DONE          0xC2000000

	#define FW_MSG_CODE_UPDATE_DRIVER_STATE_DONE    0xC3000000

	#define FW_MSG_SEQ_NUMBER_MASK                  0x0000ffff

	uint32_t fw_mb_param;

	#define FW_PARAM_INVALID_IMG                    0xffffffff

	uint32_t drv_pulse_mb;
	#define DRV_PULSE_SEQ_MASK                      0x00007fff
	#define DRV_PULSE_SYSTEM_TIME_MASK              0xffff0000
	/*
	 * The system time is in the format of
	 * (year-2001)*12*32 + month*32 + day.
	 */
	#define DRV_PULSE_ALWAYS_ALIVE                  0x00008000
	/*
	 * Indicate to the firmware not to go into the
	 * OS-absent when it is not getting driver pulse.
	 * This is used for debugging as well for PXE(MBA).
	 */

	uint32_t mcp_pulse_mb;
	#define MCP_PULSE_SEQ_MASK                      0x00007fff
	#define MCP_PULSE_ALWAYS_ALIVE                  0x00008000
	/* Indicates to the driver not to assert due to lack
	 * of MCP response */
	#define MCP_EVENT_MASK                          0xffff0000
	#define MCP_EVENT_OTHER_DRIVER_RESET_REQ        0x00010000

	uint32_t iscsi_boot_signature;
	uint32_t iscsi_boot_block_offset;

	uint32_t drv_status;
	#define DRV_STATUS_PMF                          0x00000001
	#define DRV_STATUS_VF_DISABLED                  0x00000002
	#define DRV_STATUS_SET_MF_BW                    0x00000004
	#define DRV_STATUS_LINK_EVENT                   0x00000008

	#define DRV_STATUS_OEM_EVENT_MASK               0x00000070
	#define DRV_STATUS_OEM_DISABLE_ENABLE_PF        0x00000010
	#define DRV_STATUS_OEM_BANDWIDTH_ALLOCATION     0x00000020
	#define DRV_STATUS_OEM_FC_NPIV_UPDATE           0x00000040

	#define DRV_STATUS_OEM_UPDATE_SVID              0x00000080

	#define DRV_STATUS_DCC_EVENT_MASK               0x0000ff00
	#define DRV_STATUS_DCC_DISABLE_ENABLE_PF        0x00000100
	#define DRV_STATUS_DCC_BANDWIDTH_ALLOCATION     0x00000200
	#define DRV_STATUS_DCC_CHANGE_MAC_ADDRESS       0x00000400
	#define DRV_STATUS_DCC_RESERVED1                0x00000800
	#define DRV_STATUS_DCC_SET_PROTOCOL             0x00001000
	#define DRV_STATUS_DCC_SET_PRIORITY             0x00002000

	#define DRV_STATUS_DCBX_EVENT_MASK              0x000f0000
	#define DRV_STATUS_DCBX_NEGOTIATION_RESULTS     0x00010000
	#define DRV_STATUS_AFEX_EVENT_MASK              0x03f00000
	#define DRV_STATUS_AFEX_LISTGET_REQ             0x00100000
	#define DRV_STATUS_AFEX_LISTSET_REQ             0x00200000
	#define DRV_STATUS_AFEX_STATSGET_REQ            0x00400000
	#define DRV_STATUS_AFEX_VIFSET_REQ              0x00800000

	#define DRV_STATUS_DRV_INFO_REQ                 0x04000000

	#define DRV_STATUS_EEE_NEGOTIATION_RESULTS      0x08000000

	uint32_t virt_mac_upper;
	#define VIRT_MAC_SIGN_MASK                      0xffff0000
	#define VIRT_MAC_SIGNATURE                      0x564d0000
	uint32_t virt_mac_lower;

};


/****************************************************************************
 * Management firmware state                                                *
 ****************************************************************************/
/* Allocate 440 bytes for management firmware */
#define MGMTFW_STATE_WORD_SIZE                          110

struct mgmtfw_state {
	uint32_t opaque[MGMTFW_STATE_WORD_SIZE];
};


/****************************************************************************
 * Multi-Function configuration                                             *
 ****************************************************************************/
struct shared_mf_cfg {

	uint32_t clp_mb;
	#define SHARED_MF_CLP_SET_DEFAULT               0x00000000
	/* set by CLP */
	#define SHARED_MF_CLP_EXIT                      0x00000001
	/* set by MCP */
	#define SHARED_MF_CLP_EXIT_DONE                 0x00010000

};

struct port_mf_cfg {

	uint32_t dynamic_cfg;    /* device control channel */
	#define PORT_MF_CFG_E1HOV_TAG_MASK              0x0000ffff
	#define PORT_MF_CFG_E1HOV_TAG_SHIFT             0
	#define PORT_MF_CFG_E1HOV_TAG_DEFAULT         PORT_MF_CFG_E1HOV_TAG_MASK

	uint32_t reserved[1];

};

struct func_mf_cfg {

	uint32_t config;
	/* E/R/I/D */
	/* function 0 of each port cannot be hidden */
	#define FUNC_MF_CFG_FUNC_HIDE                   0x00000001

	#define FUNC_MF_CFG_PROTOCOL_MASK               0x00000006
	#define FUNC_MF_CFG_PROTOCOL_FCOE               0x00000000
	#define FUNC_MF_CFG_PROTOCOL_ETHERNET           0x00000002
	#define FUNC_MF_CFG_PROTOCOL_ETHERNET_WITH_RDMA 0x00000004
	#define FUNC_MF_CFG_PROTOCOL_ISCSI              0x00000006
	#define FUNC_MF_CFG_PROTOCOL_DEFAULT \
				FUNC_MF_CFG_PROTOCOL_ETHERNET_WITH_RDMA

	#define FUNC_MF_CFG_FUNC_DISABLED               0x00000008
	#define FUNC_MF_CFG_FUNC_DELETED                0x00000010

	#define FUNC_MF_CFG_FUNC_BOOT_MASK              0x00000060
	#define FUNC_MF_CFG_FUNC_BOOT_BIOS_CTRL         0x00000000
	#define FUNC_MF_CFG_FUNC_BOOT_VCM_DISABLED      0x00000020
	#define FUNC_MF_CFG_FUNC_BOOT_VCM_ENABLED       0x00000040

	/* PRI */
	/* 0 - low priority, 3 - high priority */
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_MASK      0x00000300
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_SHIFT     8
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_DEFAULT   0x00000000

	/* MINBW, MAXBW */
	/* value range - 0..100, increments in 100Mbps */
	#define FUNC_MF_CFG_MIN_BW_MASK                 0x00ff0000
	#define FUNC_MF_CFG_MIN_BW_SHIFT                16
	#define FUNC_MF_CFG_MIN_BW_DEFAULT              0x00000000
	#define FUNC_MF_CFG_MAX_BW_MASK                 0xff000000
	#define FUNC_MF_CFG_MAX_BW_SHIFT                24
	#define FUNC_MF_CFG_MAX_BW_DEFAULT              0x64000000

	uint32_t mac_upper;	    /* MAC */
	#define FUNC_MF_CFG_UPPERMAC_MASK               0x0000ffff
	#define FUNC_MF_CFG_UPPERMAC_SHIFT              0
	#define FUNC_MF_CFG_UPPERMAC_DEFAULT           FUNC_MF_CFG_UPPERMAC_MASK
	uint32_t mac_lower;
	#define FUNC_MF_CFG_LOWERMAC_DEFAULT            0xffffffff

	uint32_t e1hov_tag;	/* VNI */
	#define FUNC_MF_CFG_E1HOV_TAG_MASK              0x0000ffff
	#define FUNC_MF_CFG_E1HOV_TAG_SHIFT             0
	#define FUNC_MF_CFG_E1HOV_TAG_DEFAULT         FUNC_MF_CFG_E1HOV_TAG_MASK

	/* afex default VLAN ID - 12 bits */
	#define FUNC_MF_CFG_AFEX_VLAN_MASK              0x0fff0000
	#define FUNC_MF_CFG_AFEX_VLAN_SHIFT             16

	uint32_t afex_config;
	#define FUNC_MF_CFG_AFEX_COS_FILTER_MASK                     0x000000ff
	#define FUNC_MF_CFG_AFEX_COS_FILTER_SHIFT                    0
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_MASK                    0x0000ff00
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_SHIFT                   8
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_VAL                     0x00000100
	#define FUNC_MF_CFG_AFEX_VLAN_MODE_MASK                      0x000f0000
	#define FUNC_MF_CFG_AFEX_VLAN_MODE_SHIFT                     16

	uint32_t pf_allocation;
	/* number of vfs in function, if 0 - sriov disabled */
	#define FUNC_MF_CFG_NUMBER_OF_VFS_MASK                      0x000000FF
	#define FUNC_MF_CFG_NUMBER_OF_VFS_SHIFT                     0
};

enum mf_cfg_afex_vlan_mode {
	FUNC_MF_CFG_AFEX_VLAN_TRUNK_MODE = 0,
	FUNC_MF_CFG_AFEX_VLAN_ACCESS_MODE,
	FUNC_MF_CFG_AFEX_VLAN_TRUNK_TAG_NATIVE_MODE
};

/* This structure is not applicable and should not be accessed on 57711 */
struct func_ext_cfg {
	uint32_t func_cfg;
	#define MACP_FUNC_CFG_FLAGS_MASK                0x0000007F
	#define MACP_FUNC_CFG_FLAGS_SHIFT               0
	#define MACP_FUNC_CFG_FLAGS_ENABLED             0x00000001
	#define MACP_FUNC_CFG_FLAGS_ETHERNET            0x00000002
	#define MACP_FUNC_CFG_FLAGS_ISCSI_OFFLOAD       0x00000004
	#define MACP_FUNC_CFG_FLAGS_FCOE_OFFLOAD        0x00000008
    #define MACP_FUNC_CFG_PAUSE_ON_HOST_RING        0x00000080

	uint32_t iscsi_mac_addr_upper;
	uint32_t iscsi_mac_addr_lower;

	uint32_t fcoe_mac_addr_upper;
	uint32_t fcoe_mac_addr_lower;

	uint32_t fcoe_wwn_port_name_upper;
	uint32_t fcoe_wwn_port_name_lower;

	uint32_t fcoe_wwn_node_name_upper;
	uint32_t fcoe_wwn_node_name_lower;

	uint32_t preserve_data;
	#define MF_FUNC_CFG_PRESERVE_L2_MAC             (1<<0)
	#define MF_FUNC_CFG_PRESERVE_ISCSI_MAC          (1<<1)
	#define MF_FUNC_CFG_PRESERVE_FCOE_MAC           (1<<2)
	#define MF_FUNC_CFG_PRESERVE_FCOE_WWN_P         (1<<3)
	#define MF_FUNC_CFG_PRESERVE_FCOE_WWN_N         (1<<4)
	#define MF_FUNC_CFG_PRESERVE_TX_BW              (1<<5)
};

struct mf_cfg {

	struct shared_mf_cfg    shared_mf_config;       /* 0x4 */
	struct port_mf_cfg  port_mf_config[NVM_PATH_MAX][PORT_MAX];
    /* 0x10*2=0x20 */
	/* for all chips, there are 8 mf functions */
	struct func_mf_cfg  func_mf_config[E1H_FUNC_MAX]; /* 0x18 * 8 = 0xc0 */
	/*
	 * Extended configuration per function  - this array does not exist and
	 * should not be accessed on 57711
	 */
	struct func_ext_cfg func_ext_config[E1H_FUNC_MAX]; /* 0x28 * 8 = 0x140*/
}; /* 0x224 */

/****************************************************************************
 * Shared Memory Region                                                     *
 ****************************************************************************/
struct shmem_region {		       /*   SharedMem Offset (size) */

	uint32_t         validity_map[PORT_MAX];  /* 0x0 (4*2 = 0x8) */
	#define SHR_MEM_FORMAT_REV_MASK                     0xff000000
	#define SHR_MEM_FORMAT_REV_ID                       ('A'<<24)
	/* validity bits */
	#define SHR_MEM_VALIDITY_PCI_CFG                    0x00100000
	#define SHR_MEM_VALIDITY_MB                         0x00200000
	#define SHR_MEM_VALIDITY_DEV_INFO                   0x00400000
	#define SHR_MEM_VALIDITY_RESERVED                   0x00000007
	/* One licensing bit should be set */
	#define SHR_MEM_VALIDITY_LIC_KEY_IN_EFFECT_MASK     0x00000038
	#define SHR_MEM_VALIDITY_LIC_MANUF_KEY_IN_EFFECT    0x00000008
	#define SHR_MEM_VALIDITY_LIC_UPGRADE_KEY_IN_EFFECT  0x00000010
	#define SHR_MEM_VALIDITY_LIC_NO_KEY_IN_EFFECT       0x00000020
	/* Active MFW */
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_UNKNOWN         0x00000000
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_MASK            0x000001c0
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_IPMI            0x00000040
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_UMP             0x00000080
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_NCSI            0x000000c0
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_NONE            0x000001c0

	struct shm_dev_info dev_info;	     /* 0x8     (0x438) */

	struct license_key    drv_lic_key[PORT_MAX]; /* 0x440 (52 * 2 = 0x68) */

	/* FW information (for internal FW use) */
	uint32_t         fw_info_fio_offset;		/* 0x4a8       (0x4) */
	struct mgmtfw_state mgmtfw_state;	/* 0x4ac     (0x1b8) */

	struct drv_port_mb  port_mb[PORT_MAX];	/* 0x664 (16*2=0x20) */


#ifdef BMAPI
	/* This is a variable length array */
	/* the number of function depends on the chip type */
	struct drv_func_mb func_mb[1];	/* 0x684 (44*2/4/8=0x58/0xb0/0x160) */
#else
	/* the number of function depends on the chip type */
	struct drv_func_mb  func_mb[];	/* 0x684 (44*2/4/8=0x58/0xb0/0x160) */
#endif /* BMAPI */

}; /* 57710 = 0x6dc | 57711 = 0x7E4 | 57712 = 0x734 */

/****************************************************************************
 * Shared Memory 2 Region                                                   *
 ****************************************************************************/
/* The fw_flr_ack is actually built in the following way:                   */
/* 8 bit:  PF ack                                                           */
/* 64 bit: VF ack                                                           */
/* 8 bit:  ios_dis_ack                                                      */
/* In order to maintain endianity in the mailbox hsi, we want to keep using */
/* uint32_t. The fw must have the VF right after the PF since this is how it     */
/* access arrays(it expects always the VF to reside after the PF, and that  */
/* makes the calculation much easier for it. )                              */
/* In order to answer both limitations, and keep the struct small, the code */
/* will abuse the structure defined here to achieve the actual partition    */
/* above                                                                    */
/****************************************************************************/
struct fw_flr_ack {
	uint32_t         pf_ack;
	uint32_t         vf_ack;
	uint32_t         iov_dis_ack;
};

struct fw_flr_mb {
	uint32_t         aggint;
	uint32_t         opgen_addr;
	struct fw_flr_ack ack;
};

struct eee_remote_vals {
	uint32_t         tx_tw;
	uint32_t         rx_tw;
};

/**** SUPPORT FOR SHMEM ARRRAYS ***
 * The SHMEM HSI is aligned on 32 bit boundaries which makes it difficult to
 * define arrays with storage types smaller then unsigned dwords.
 * The macros below add generic support for SHMEM arrays with numeric elements
 * that can span 2,4,8 or 16 bits. The array underlying type is a 32 bit dword
 * array with individual bit-filed elements accessed using shifts and masks.
 *
 */

/* eb is the bitwidth of a single element */
#define SHMEM_ARRAY_MASK(eb)		((1<<(eb))-1)
#define SHMEM_ARRAY_ENTRY(i, eb)	((i)/(32/(eb)))

/* the bit-position macro allows the used to flip the order of the arrays
 * elements on a per byte or word boundary.
 *
 * example: an array with 8 entries each 4 bit wide. This array will fit into
 * a single dword. The diagrmas below show the array order of the nibbles.
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 4) defines the stadard ordering:
 *
 *                |                |                |               |
 *   0    |   1   |   2    |   3   |   4    |   5   |   6   |   7   |
 *                |                |                |               |
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 8) defines a flip ordering per byte:
 *
 *                |                |                |               |
 *   1   |   0    |   3    |   2   |   5    |   4   |   7   |   6   |
 *                |                |                |               |
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 16) defines a flip ordering per word:
 *
 *                |                |                |               |
 *   3   |   2    |   1   |   0    |   7   |   6    |   5   |   4   |
 *                |                |                |               |
 */
#define SHMEM_ARRAY_BITPOS(i, eb, fb)	\
	((((32/(fb)) - 1 - ((i)/((fb)/(eb))) % (32/(fb))) * (fb)) + \
	(((i)%((fb)/(eb))) * (eb)))

#define SHMEM_ARRAY_GET(a, i, eb, fb)					\
	((a[SHMEM_ARRAY_ENTRY(i, eb)] >> SHMEM_ARRAY_BITPOS(i, eb, fb)) &  \
	SHMEM_ARRAY_MASK(eb))

#define SHMEM_ARRAY_SET(a, i, eb, fb, val)				\
do {									   \
	a[SHMEM_ARRAY_ENTRY(i, eb)] &= ~(SHMEM_ARRAY_MASK(eb) <<	   \
	SHMEM_ARRAY_BITPOS(i, eb, fb));					   \
	a[SHMEM_ARRAY_ENTRY(i, eb)] |= (((val) & SHMEM_ARRAY_MASK(eb)) <<  \
	SHMEM_ARRAY_BITPOS(i, eb, fb));					   \
} while (0)


/****START OF DCBX STRUCTURES DECLARATIONS****/
#define DCBX_MAX_NUM_PRI_PG_ENTRIES	8
#define DCBX_PRI_PG_BITWIDTH		4
#define DCBX_PRI_PG_FBITS		8
#define DCBX_PRI_PG_GET(a, i)		\
	SHMEM_ARRAY_GET(a, i, DCBX_PRI_PG_BITWIDTH, DCBX_PRI_PG_FBITS)
#define DCBX_PRI_PG_SET(a, i, val)	\
	SHMEM_ARRAY_SET(a, i, DCBX_PRI_PG_BITWIDTH, DCBX_PRI_PG_FBITS, val)
#define DCBX_MAX_NUM_PG_BW_ENTRIES	8
#define DCBX_BW_PG_BITWIDTH		8
#define DCBX_PG_BW_GET(a, i)		\
	SHMEM_ARRAY_GET(a, i, DCBX_BW_PG_BITWIDTH, DCBX_BW_PG_BITWIDTH)
#define DCBX_PG_BW_SET(a, i, val)	\
	SHMEM_ARRAY_SET(a, i, DCBX_BW_PG_BITWIDTH, DCBX_BW_PG_BITWIDTH, val)
#define DCBX_STRICT_PRI_PG		15
#define DCBX_MAX_APP_PROTOCOL		16
#define DCBX_MAX_APP_LOCAL	    32
#define FCOE_APP_IDX			0
#define ISCSI_APP_IDX			1
#define PREDEFINED_APP_IDX_MAX		2


/* Big/Little endian have the same representation. */
struct dcbx_ets_feature {
	/*
	 * For Admin MIB - is this feature supported by the
	 * driver | For Local MIB - should this feature be enabled.
	 */
	uint32_t enabled;
	uint32_t  pg_bw_tbl[2];
	uint32_t  pri_pg_tbl[1];
};

/* Driver structure in LE */
struct dcbx_pfc_feature {
#ifdef __BIG_ENDIAN
	uint8_t pri_en_bitmap;
	#define DCBX_PFC_PRI_0 0x01
	#define DCBX_PFC_PRI_1 0x02
	#define DCBX_PFC_PRI_2 0x04
	#define DCBX_PFC_PRI_3 0x08
	#define DCBX_PFC_PRI_4 0x10
	#define DCBX_PFC_PRI_5 0x20
	#define DCBX_PFC_PRI_6 0x40
	#define DCBX_PFC_PRI_7 0x80
	uint8_t pfc_caps;
	uint8_t reserved;
	uint8_t enabled;
#elif defined(__LITTLE_ENDIAN)
	uint8_t enabled;
	uint8_t reserved;
	uint8_t pfc_caps;
	uint8_t pri_en_bitmap;
	#define DCBX_PFC_PRI_0 0x01
	#define DCBX_PFC_PRI_1 0x02
	#define DCBX_PFC_PRI_2 0x04
	#define DCBX_PFC_PRI_3 0x08
	#define DCBX_PFC_PRI_4 0x10
	#define DCBX_PFC_PRI_5 0x20
	#define DCBX_PFC_PRI_6 0x40
	#define DCBX_PFC_PRI_7 0x80
#endif
};

struct dcbx_app_priority_entry {
#ifdef __BIG_ENDIAN
	uint16_t  app_id;
	uint8_t  pri_bitmap;
	uint8_t  appBitfield;
	#define DCBX_APP_ENTRY_VALID         0x01
	#define DCBX_APP_ENTRY_SF_MASK       0xF0
	#define DCBX_APP_ENTRY_SF_SHIFT      4
	#define DCBX_APP_SF_ETH_TYPE         0x10
	#define DCBX_APP_SF_PORT             0x20 /* TCP */
	#define DCBX_APP_SF_UDP              0x40 /* UDP */
	#define DCBX_APP_SF_DEFAULT          0x80
	#define DCBX_APP_PRI_0               0x01
	#define DCBX_APP_PRI_1               0x02
	#define DCBX_APP_PRI_2               0x04
	#define DCBX_APP_PRI_3               0x08
	#define DCBX_APP_PRI_4               0x10
	#define DCBX_APP_PRI_5               0x20
	#define DCBX_APP_PRI_6               0x40
	#define DCBX_APP_PRI_7               0x80
#elif defined(__LITTLE_ENDIAN)
	uint8_t appBitfield;
	#define DCBX_APP_ENTRY_VALID         0x01
	#define DCBX_APP_ENTRY_SF_MASK       0xF0
	#define DCBX_APP_ENTRY_SF_SHIFT      4
	#define DCBX_APP_ENTRY_VALID         0x01
	#define DCBX_APP_SF_ETH_TYPE         0x10
	#define DCBX_APP_SF_PORT             0x20 /* TCP */
	#define DCBX_APP_SF_UDP              0x40 /* UDP */
	#define DCBX_APP_SF_DEFAULT          0x80
	uint8_t  pri_bitmap;
	uint16_t  app_id;
#endif
};


/* FW structure in BE */
struct dcbx_app_priority_feature {
#ifdef __BIG_ENDIAN
	uint8_t reserved;
	uint8_t default_pri;
	uint8_t tc_supported;
	uint8_t enabled;
#elif defined(__LITTLE_ENDIAN)
	uint8_t enabled;
	uint8_t tc_supported;
	uint8_t default_pri;
	uint8_t reserved;
#endif
	struct dcbx_app_priority_entry  app_pri_tbl[DCBX_MAX_APP_PROTOCOL];
};

/* FW structure in BE */
struct dcbx_features {
	/* PG feature */
	struct dcbx_ets_feature ets;
	/* PFC feature */
	struct dcbx_pfc_feature pfc;
	/* APP feature */
	struct dcbx_app_priority_feature app;
};

/* LLDP protocol parameters */
/* FW structure in BE */
struct lldp_params {
#ifdef __BIG_ENDIAN
	uint8_t  msg_fast_tx_interval;
	uint8_t  msg_tx_hold;
	uint8_t  msg_tx_interval;
	uint8_t  admin_status;
	#define LLDP_TX_ONLY  0x01
	#define LLDP_RX_ONLY  0x02
	#define LLDP_TX_RX    0x03
	#define LLDP_DISABLED 0x04
	uint8_t  reserved1;
	uint8_t  tx_fast;
	uint8_t  tx_crd_max;
	uint8_t  tx_crd;
#elif defined(__LITTLE_ENDIAN)
	uint8_t  admin_status;
	#define LLDP_TX_ONLY  0x01
	#define LLDP_RX_ONLY  0x02
	#define LLDP_TX_RX    0x03
	#define LLDP_DISABLED 0x04
	uint8_t  msg_tx_interval;
	uint8_t  msg_tx_hold;
	uint8_t  msg_fast_tx_interval;
	uint8_t  tx_crd;
	uint8_t  tx_crd_max;
	uint8_t  tx_fast;
	uint8_t  reserved1;
#endif
	#define REM_CHASSIS_ID_STAT_LEN 4
	#define REM_PORT_ID_STAT_LEN 4
	/* Holds remote Chassis ID TLV header, subtype and 9B of payload. */
	uint32_t peer_chassis_id[REM_CHASSIS_ID_STAT_LEN];
	/* Holds remote Port ID TLV header, subtype and 9B of payload. */
	uint32_t peer_port_id[REM_PORT_ID_STAT_LEN];
};

struct lldp_dcbx_stat {
	#define LOCAL_CHASSIS_ID_STAT_LEN 2
	#define LOCAL_PORT_ID_STAT_LEN 2
	/* Holds local Chassis ID 8B payload of constant subtype 4. */
	uint32_t local_chassis_id[LOCAL_CHASSIS_ID_STAT_LEN];
	/* Holds local Port ID 8B payload of constant subtype 3. */
	uint32_t local_port_id[LOCAL_PORT_ID_STAT_LEN];
	/* Number of DCBX frames transmitted. */
	uint32_t num_tx_dcbx_pkts;
	/* Number of DCBX frames received. */
	uint32_t num_rx_dcbx_pkts;
};

/* ADMIN MIB - DCBX local machine default configuration. */
struct lldp_admin_mib {
	uint32_t     ver_cfg_flags;
	#define DCBX_ETS_CONFIG_TX_ENABLED       0x00000001
	#define DCBX_PFC_CONFIG_TX_ENABLED       0x00000002
	#define DCBX_APP_CONFIG_TX_ENABLED       0x00000004
	#define DCBX_ETS_RECO_TX_ENABLED         0x00000008
	#define DCBX_ETS_RECO_VALID              0x00000010
	#define DCBX_ETS_WILLING                 0x00000020
	#define DCBX_PFC_WILLING                 0x00000040
	#define DCBX_APP_WILLING                 0x00000080
	#define DCBX_VERSION_CEE                 0x00000100
	#define DCBX_VERSION_IEEE                0x00000200
	#define DCBX_DCBX_ENABLED                0x00000400
	#define DCBX_CEE_VERSION_MASK            0x0000f000
	#define DCBX_CEE_VERSION_SHIFT           12
	#define DCBX_CEE_MAX_VERSION_MASK        0x000f0000
	#define DCBX_CEE_MAX_VERSION_SHIFT       16
	struct dcbx_features     features;
};

/* REMOTE MIB - remote machine DCBX configuration. */
struct lldp_remote_mib {
	uint32_t prefix_seq_num;
	uint32_t flags;
	#define DCBX_ETS_TLV_RX                  0x00000001
	#define DCBX_PFC_TLV_RX                  0x00000002
	#define DCBX_APP_TLV_RX                  0x00000004
	#define DCBX_ETS_RX_ERROR                0x00000010
	#define DCBX_PFC_RX_ERROR                0x00000020
	#define DCBX_APP_RX_ERROR                0x00000040
	#define DCBX_ETS_REM_WILLING             0x00000100
	#define DCBX_PFC_REM_WILLING             0x00000200
	#define DCBX_APP_REM_WILLING             0x00000400
	#define DCBX_REMOTE_ETS_RECO_VALID       0x00001000
	#define DCBX_REMOTE_MIB_VALID            0x00002000
	struct dcbx_features features;
	uint32_t suffix_seq_num;
};

/* LOCAL MIB - operational DCBX configuration - transmitted on Tx LLDPDU. */
struct lldp_local_mib {
	uint32_t prefix_seq_num;
	/* Indicates if there is mismatch with negotiation results. */
	uint32_t error;
	#define DCBX_LOCAL_ETS_ERROR             0x00000001
	#define DCBX_LOCAL_PFC_ERROR             0x00000002
	#define DCBX_LOCAL_APP_ERROR             0x00000004
	#define DCBX_LOCAL_PFC_MISMATCH          0x00000010
	#define DCBX_LOCAL_APP_MISMATCH          0x00000020
	#define DCBX_REMOTE_MIB_ERROR            0x00000040
	#define DCBX_REMOTE_ETS_TLV_NOT_FOUND    0x00000080
	#define DCBX_REMOTE_PFC_TLV_NOT_FOUND    0x00000100
	#define DCBX_REMOTE_APP_TLV_NOT_FOUND    0x00000200
	struct dcbx_features   features;
	uint32_t suffix_seq_num;
};

struct lldp_local_mib_ext {
	uint32_t prefix_seq_num;
	/* APP TLV extension - 16 more entries for negotiation results*/
	struct dcbx_app_priority_entry  app_pri_tbl_ext[DCBX_MAX_APP_PROTOCOL];
	uint32_t suffix_seq_num;
};
/***END OF DCBX STRUCTURES DECLARATIONS***/

/***********************************************************/
/*                         Elink section                   */
/***********************************************************/
#define SHMEM_LINK_CONFIG_SIZE 2
struct shmem_lfa {
	uint32_t req_duplex;
	#define REQ_DUPLEX_PHY0_MASK        0x0000ffff
	#define REQ_DUPLEX_PHY0_SHIFT       0
	#define REQ_DUPLEX_PHY1_MASK        0xffff0000
	#define REQ_DUPLEX_PHY1_SHIFT       16
	uint32_t req_flow_ctrl;
	#define REQ_FLOW_CTRL_PHY0_MASK     0x0000ffff
	#define REQ_FLOW_CTRL_PHY0_SHIFT    0
	#define REQ_FLOW_CTRL_PHY1_MASK     0xffff0000
	#define REQ_FLOW_CTRL_PHY1_SHIFT    16
	uint32_t req_line_speed; /* Also determine AutoNeg */
	#define REQ_LINE_SPD_PHY0_MASK      0x0000ffff
	#define REQ_LINE_SPD_PHY0_SHIFT     0
	#define REQ_LINE_SPD_PHY1_MASK      0xffff0000
	#define REQ_LINE_SPD_PHY1_SHIFT     16
	uint32_t speed_cap_mask[SHMEM_LINK_CONFIG_SIZE];
	uint32_t additional_config;
	#define REQ_FC_AUTO_ADV_MASK        0x0000ffff
	#define REQ_FC_AUTO_ADV0_SHIFT      0
	#define NO_LFA_DUE_TO_DCC_MASK      0x00010000
	uint32_t lfa_sts;
	#define LFA_LINK_FLAP_REASON_OFFSET		0
	#define LFA_LINK_FLAP_REASON_MASK		0x000000ff
		#define LFA_LINK_DOWN			    0x1
		#define LFA_LOOPBACK_ENABLED		0x2
		#define LFA_DUPLEX_MISMATCH		    0x3
		#define LFA_MFW_IS_TOO_OLD		    0x4
		#define LFA_LINK_SPEED_MISMATCH		0x5
		#define LFA_FLOW_CTRL_MISMATCH		0x6
		#define LFA_SPEED_CAP_MISMATCH		0x7
		#define LFA_DCC_LFA_DISABLED		0x8
		#define LFA_EEE_MISMATCH		0x9

	#define LINK_FLAP_AVOIDANCE_COUNT_OFFSET	8
	#define LINK_FLAP_AVOIDANCE_COUNT_MASK		0x0000ff00

	#define LINK_FLAP_COUNT_OFFSET			16
	#define LINK_FLAP_COUNT_MASK			0x00ff0000

	#define LFA_FLAGS_MASK				0xff000000
	#define SHMEM_LFA_DONT_CLEAR_STAT		(1<<24)

};

/*
 * Used to suppoert NSCI get OS driver version
 * On driver load the version value will be set
 * On driver unload driver value of 0x0 will be set
 */
struct os_drv_ver {
	#define DRV_VER_NOT_LOADED                      0
	/*personalites orrder is importent */
	#define DRV_PERS_ETHERNET                       0
	#define DRV_PERS_ISCSI                          1
	#define DRV_PERS_FCOE                           2
	/*shmem2 struct is constatnt can't add more personalites here*/
	#define MAX_DRV_PERS                            3
	uint32_t  versions[MAX_DRV_PERS];
};

#define OEM_I2C_UUID_STR_ADDR 0x9f
#define OEM_I2C_CARD_SKU_STR_ADDR 0x3c
#define OEM_I2C_CARD_FN_STR_ADDR 0x48
#define OEM_I2C_CARD_NAME_STR_ADDR 0x10e

#define OEM_I2C_UUID_STR_LEN 16
#define OEM_I2C_CARD_SKU_STR_LEN 12
#define OEM_I2C_CARD_FN_STR_LEN 12
#define OEM_I2C_CARD_NAME_STR_LEN 128
#define OEM_I2C_CARD_VERSION_STR_LEN 36

struct oem_i2c_data_t {
	uint32_t size;
	uint8_t uuid[OEM_I2C_UUID_STR_LEN];
	uint8_t card_sku[OEM_I2C_CARD_SKU_STR_LEN];
	uint8_t card_name[OEM_I2C_CARD_NAME_STR_LEN];
	uint8_t card_ver[OEM_I2C_CARD_VERSION_STR_LEN];
	uint8_t card_fn[OEM_I2C_CARD_FN_STR_LEN];
};

enum curr_cfg_method_e {
	CURR_CFG_MET_NONE = 0,  /* default config */
	CURR_CFG_MET_OS = 1,
	CURR_CFG_MET_VENDOR_SPEC = 2,/* e.g. Option ROM, NPAR, O/S Cfg Utils */
	CURR_CFG_MET_HP_OTHER = 3,
	CURR_CFG_MET_VC_CLP = 4,  /* C-Class SM-CLP */
	CURR_CFG_MET_HP_CNU = 5,  /*  Converged Network Utility */
	CURR_CFG_MET_HP_DCI = 6,  /* DCi (BD) changes */
};

#define FC_NPIV_WWPN_SIZE 8
#define FC_NPIV_WWNN_SIZE 8
struct bdn_npiv_settings {
	uint8_t npiv_wwpn[FC_NPIV_WWPN_SIZE];
	uint8_t npiv_wwnn[FC_NPIV_WWNN_SIZE];
};

struct bdn_fc_npiv_cfg {
	/* hdr used internally by the MFW */
	uint32_t hdr;
	uint32_t num_of_npiv;
};

#define MAX_NUMBER_NPIV 64
struct bdn_fc_npiv_tbl {
	struct bdn_fc_npiv_cfg fc_npiv_cfg;
	struct bdn_npiv_settings settings[MAX_NUMBER_NPIV];
};

struct mdump_driver_info {
	uint32_t epoc;
	uint32_t drv_ver;
	uint32_t fw_ver;

	uint32_t valid_dump;
	#define FIRST_DUMP_VALID        (1 << 0)
	#define SECOND_DUMP_VALID       (1 << 1)

	uint32_t flags;
	#define ENABLE_ALL_TRIGGERS     (0x7fffffff)
	#define TRIGGER_MDUMP_ONCE      (1 << 31)
};

struct shmem2_region {

	uint32_t size;					/* 0x0000 */

	uint32_t dcc_support;				/* 0x0004 */
	#define SHMEM_DCC_SUPPORT_NONE                      0x00000000
	#define SHMEM_DCC_SUPPORT_DISABLE_ENABLE_PF_TLV     0x00000001
	#define SHMEM_DCC_SUPPORT_BANDWIDTH_ALLOCATION_TLV  0x00000004
	#define SHMEM_DCC_SUPPORT_CHANGE_MAC_ADDRESS_TLV    0x00000008
	#define SHMEM_DCC_SUPPORT_SET_PROTOCOL_TLV          0x00000040
	#define SHMEM_DCC_SUPPORT_SET_PRIORITY_TLV          0x00000080

	uint32_t ext_phy_fw_version2[PORT_MAX];		/* 0x0008 */
	/*
	 * For backwards compatibility, if the mf_cfg_addr does not exist
	 * (the size filed is smaller than 0xc) the mf_cfg resides at the
	 * end of struct shmem_region
	 */
	uint32_t mf_cfg_addr;				/* 0x0010 */
	#define SHMEM_MF_CFG_ADDR_NONE                  0x00000000

	struct fw_flr_mb flr_mb;			/* 0x0014 */
	uint32_t dcbx_lldp_params_offset;			/* 0x0028 */
	#define SHMEM_LLDP_DCBX_PARAMS_NONE             0x00000000
	uint32_t dcbx_neg_res_offset;			/* 0x002c */
	#define SHMEM_DCBX_NEG_RES_NONE			0x00000000
	uint32_t dcbx_remote_mib_offset;			/* 0x0030 */
	#define SHMEM_DCBX_REMOTE_MIB_NONE              0x00000000
	/*
	 * The other shmemX_base_addr holds the other path's shmem address
	 * required for example in case of common phy init, or for path1 to know
	 * the address of mcp debug trace which is located in offset from shmem
	 * of path0
	 */
	uint32_t other_shmem_base_addr;			/* 0x0034 */
	uint32_t other_shmem2_base_addr;			/* 0x0038 */
	/*
	 * mcp_vf_disabled is set by the MCP to indicate the driver about VFs
	 * which were disabled/flred
	 */
	uint32_t mcp_vf_disabled[E2_VF_MAX / 32];		/* 0x003c */

	/*
	 * drv_ack_vf_disabled is set by the PF driver to ack handled disabled
	 * VFs
	 */
	uint32_t drv_ack_vf_disabled[E2_FUNC_MAX][E2_VF_MAX / 32]; /* 0x0044 */

	uint32_t dcbx_lldp_dcbx_stat_offset;			/* 0x0064 */
	#define SHMEM_LLDP_DCBX_STAT_NONE               0x00000000

	/*
	 * edebug_driver_if field is used to transfer messages between edebug
	 * app to the driver through shmem2.
	 *
	 * message format:
	 * bits 0-2 -  function number / instance of driver to perform request
	 * bits 3-5 -  op code / is_ack?
	 * bits 6-63 - data
	 */
	uint32_t edebug_driver_if[2];			/* 0x0068 */
	#define EDEBUG_DRIVER_IF_OP_CODE_GET_PHYS_ADDR  1
	#define EDEBUG_DRIVER_IF_OP_CODE_GET_BUS_ADDR   2
	#define EDEBUG_DRIVER_IF_OP_CODE_DISABLE_STAT   3

	uint32_t nvm_retain_bitmap_addr;			/* 0x0070 */

	/* afex support of that driver */
	uint32_t afex_driver_support;			/* 0x0074 */
	#define SHMEM_AFEX_VERSION_MASK                  0x100f
	#define SHMEM_AFEX_SUPPORTED_VERSION_ONE         0x1001
	#define SHMEM_AFEX_REDUCED_DRV_LOADED            0x8000

	/* driver receives addr in scratchpad to which it should respond */
	uint32_t afex_scratchpad_addr_to_write[E2_FUNC_MAX];

	/*
	 * generic params from MCP to driver (value depends on the msg sent
	 * to driver
	 */
	uint32_t afex_param1_to_driver[E2_FUNC_MAX];		/* 0x0088 */
	uint32_t afex_param2_to_driver[E2_FUNC_MAX];		/* 0x0098 */

	uint32_t swim_base_addr;				/* 0x00a8 */
	uint32_t swim_funcs;					/* 0x00ac */
	uint32_t swim_main_cb;				/* 0x00b0 */

	/*
	 * bitmap notifying which VIF profiles stored in nvram are enabled by
	 * switch
	 */
	uint32_t afex_profiles_enabled[2];			/* 0x00b4 */

	/* generic flags controlled by the driver */
	uint32_t drv_flags;					/* 0x00bc */
	#define DRV_FLAGS_DCB_CONFIGURED		0x0
	#define DRV_FLAGS_DCB_CONFIGURATION_ABORTED	0x1
	#define DRV_FLAGS_DCB_MFW_CONFIGURED	0x2

    #define DRV_FLAGS_PORT_MASK	((1 << DRV_FLAGS_DCB_CONFIGURED) | \
			(1 << DRV_FLAGS_DCB_CONFIGURATION_ABORTED) | \
			(1 << DRV_FLAGS_DCB_MFW_CONFIGURED))
	/* Port offset*/
	#define DRV_FLAGS_P0_OFFSET		0
	#define DRV_FLAGS_P1_OFFSET		16
	#define DRV_FLAGS_GET_PORT_OFFSET(_port)	((0 == _port) ? \
						DRV_FLAGS_P0_OFFSET : \
						DRV_FLAGS_P1_OFFSET)

	#define DRV_FLAGS_GET_PORT_MASK(_port)	(DRV_FLAGS_PORT_MASK << \
	DRV_FLAGS_GET_PORT_OFFSET(_port))

	#define DRV_FLAGS_FILED_BY_PORT(_field_bit, _port)	(1 << ( \
	(_field_bit) + DRV_FLAGS_GET_PORT_OFFSET(_port)))

	/* pointer to extended dev_info shared data copied from nvm image */
	uint32_t extended_dev_info_shared_addr;		/* 0x00c0 */
	uint32_t ncsi_oem_data_addr;				/* 0x00c4 */

	uint32_t sensor_data_addr;				/* 0x00c8 */
	uint32_t buffer_block_addr;				/* 0x00cc */
	uint32_t sensor_data_req_update_interval;		/* 0x00d0 */
	uint32_t temperature_in_half_celsius;		/* 0x00d4 */
	uint32_t glob_struct_in_host;			/* 0x00d8 */

	uint32_t dcbx_neg_res_ext_offset;			/* 0x00dc */
	#define SHMEM_DCBX_NEG_RES_EXT_NONE			0x00000000

	uint32_t drv_capabilities_flag[E2_FUNC_MAX];		/* 0x00e0 */
	#define DRV_FLAGS_CAPABILITIES_LOADED_SUPPORTED 0x00000001
	#define DRV_FLAGS_CAPABILITIES_LOADED_L2        0x00000002
	#define DRV_FLAGS_CAPABILITIES_LOADED_FCOE      0x00000004
	#define DRV_FLAGS_CAPABILITIES_LOADED_ISCSI     0x00000008
	#define DRV_FLAGS_MTU_MASK			0xffff0000
	#define DRV_FLAGS_MTU_SHIFT				16

	uint32_t extended_dev_info_shared_cfg_size;		/* 0x00f0 */

	uint32_t dcbx_en[PORT_MAX];				/* 0x00f4 */

	/* The offset points to the multi threaded meta structure */
	uint32_t multi_thread_data_offset;			/* 0x00fc */

	/* address of DMAable host address holding values from the drivers */
	uint32_t drv_info_host_addr_lo;			/* 0x0100 */
	uint32_t drv_info_host_addr_hi;			/* 0x0104 */

	/* general values written by the MFW (such as current version) */
	uint32_t drv_info_control;				/* 0x0108 */
	#define DRV_INFO_CONTROL_VER_MASK          0x000000ff
	#define DRV_INFO_CONTROL_VER_SHIFT         0
	#define DRV_INFO_CONTROL_OP_CODE_MASK      0x0000ff00
	#define DRV_INFO_CONTROL_OP_CODE_SHIFT     8
	uint32_t ibft_host_addr; /* initialized by option ROM */    /* 0x010c */

	struct eee_remote_vals eee_remote_vals[PORT_MAX];	/* 0x0110 */
	uint32_t pf_allocation[E2_FUNC_MAX];			/* 0x0120 */
	#define PF_ALLOACTION_MSIX_VECTORS_MASK    0x000000ff /* real value, as PCI config space can show only maximum of 64 vectors */
	#define PF_ALLOACTION_MSIX_VECTORS_SHIFT   0

	/* the status of EEE auto-negotiation
	 * bits 15:0 the configured tx-lpi entry timer value. Depends on bit 31.
	 * bits 19:16 the supported modes for EEE.
	 * bits 23:20 the speeds advertised for EEE.
	 * bits 27:24 the speeds the Link partner advertised for EEE.
	 * The supported/adv. modes in bits 27:19 originate from the
	 * SHMEM_EEE_XXX_ADV definitions (where XXX is replaced by speed).
	 * bit 28 when 1'b1 EEE was requested.
	 * bit 29 when 1'b1 tx lpi was requested.
	 * bit 30 when 1'b1 EEE was negotiated. Tx lpi will be asserted if
	 * 30:29 are 2'b11.
	 * bit 31 when 1'b0 bits 15:0 contain a PORT_FEAT_CFG_EEE_ define as
	 * value. When 1'b1 those bits contains a value times 16 microseconds.
	 */
	uint32_t eee_status[PORT_MAX];				/* 0x0130 */
	#define SHMEM_EEE_TIMER_MASK		   0x0000ffff
	#define SHMEM_EEE_SUPPORTED_MASK	   0x000f0000
	#define SHMEM_EEE_SUPPORTED_SHIFT	   16
	#define SHMEM_EEE_ADV_STATUS_MASK	   0x00f00000
		#define SHMEM_EEE_100M_ADV	   (1<<0)
		#define SHMEM_EEE_1G_ADV	   (1 << 1)
		#define SHMEM_EEE_10G_ADV	   (1<<2)
	#define SHMEM_EEE_ADV_STATUS_SHIFT	   20
	#define	SHMEM_EEE_LP_ADV_STATUS_MASK	   0x0f000000
	#define SHMEM_EEE_LP_ADV_STATUS_SHIFT	   24
	#define SHMEM_EEE_REQUESTED_BIT		   0x10000000
	#define SHMEM_EEE_LPI_REQUESTED_BIT	   0x20000000
	#define SHMEM_EEE_ACTIVE_BIT		   0x40000000
	#define SHMEM_EEE_TIME_OUTPUT_BIT	   0x80000000

	uint32_t sizeof_port_stats;				/* 0x0138 */

	/* Link Flap Avoidance */
	uint32_t lfa_host_addr[PORT_MAX];			/* 0x013c */

    /* External PHY temperature in deg C. */
	uint32_t extphy_temps_in_celsius;			/* 0x0144 */
	#define EXTPHY1_TEMP_MASK                  0x0000ffff
	#define EXTPHY1_TEMP_SHIFT                 0
	#define ON_BOARD_TEMP_MASK                 0xffff0000
	#define ON_BOARD_TEMP_SHIFT                16

	uint32_t ocdata_info_addr;			/* Offset 0x148 */
	uint32_t drv_func_info_addr;			/* Offset 0x14C */
	uint32_t drv_func_info_size;			/* Offset 0x150 */
	uint32_t link_attr_sync[PORT_MAX];		/* Offset 0x154 */
	#define LINK_ATTR_SYNC_KR2_ENABLE	0x00000001
	#define LINK_ATTR_84858			0x00000002
	#define LINK_SFP_EEPROM_COMP_CODE_MASK	0x0000ff00
	#define LINK_SFP_EEPROM_COMP_CODE_SHIFT		 8
	#define LINK_SFP_EEPROM_COMP_CODE_SR	0x00001000
	#define LINK_SFP_EEPROM_COMP_CODE_LR	0x00002000
	#define LINK_SFP_EEPROM_COMP_CODE_LRM	0x00004000

	uint32_t ibft_host_addr_hi;  /* Initialize by uEFI ROM Offset 0x158 */
	uint32_t fcode_ver;                          /* Offset 0x15c */
	uint32_t link_change_count[PORT_MAX];        /* Offset 0x160-0x164 */
	#define LINK_CHANGE_COUNT_MASK 0xff     /* Offset 0x168 */
	/* driver version for each personality*/
	struct os_drv_ver func_os_drv_ver[E2_FUNC_MAX]; /* Offset 0x16c */

	/* Flag to the driver that PF's drv_info_host_addr buffer was read */
	uint32_t mfw_drv_indication;			/* Offset 0x19c */

	/* We use inidcation for each PF (0..3) */
	#define MFW_DRV_IND_READ_DONE_OFFSET(_pf_)  (1 << (_pf_))

	union { /* For various OEMs */			/* Offset 0x1a0 */
		uint8_t storage_boot_prog[E2_FUNC_MAX];
	#define STORAGE_BOOT_PROG_MASK				0x000000FF
	#define STORAGE_BOOT_PROG_NONE				0x00000000
	#define STORAGE_BOOT_PROG_ISCSI_IP_ACQUIRED		0x00000002
	#define STORAGE_BOOT_PROG_FCOE_FABRIC_LOGIN_SUCCESS	0x00000002
	#define STORAGE_BOOT_PROG_TARGET_FOUND			0x00000004
	#define STORAGE_BOOT_PROG_ISCSI_CHAP_SUCCESS		0x00000008
	#define STORAGE_BOOT_PROG_FCOE_LUN_FOUND		0x00000008
	#define STORAGE_BOOT_PROG_LOGGED_INTO_TGT		0x00000010
	#define STORAGE_BOOT_PROG_IMG_DOWNLOADED		0x00000020
	#define STORAGE_BOOT_PROG_OS_HANDOFF			0x00000040
	#define STORAGE_BOOT_PROG_COMPLETED			0x00000080

		uint32_t oem_i2c_data_addr;
	};

	/* 9 entries for the C2S PCP map for each inner VLAN PCP + 1 default */
	/* For PCP values 0-3 use the map lower */
	/* 0xFF000000 - PCP 0, 0x00FF0000 - PCP 1,
	 * 0x0000FF00 - PCP 2, 0x000000FF PCP 3
	 */
	uint32_t c2s_pcp_map_lower[E2_FUNC_MAX];		/* 0x1a4 */

	/* For PCP values 4-7 use the map upper */
	/* 0xFF000000 - PCP 4, 0x00FF0000 - PCP 5,
	 * 0x0000FF00 - PCP 6, 0x000000FF PCP 7
	 */
	uint32_t c2s_pcp_map_upper[E2_FUNC_MAX];		/* 0x1b4 */

	/* For PCP default value get the MSB byte of the map default */
	uint32_t c2s_pcp_map_default[E2_FUNC_MAX];		/* 0x1c4 */

	/* FC_NPIV table offset in NVRAM */
	uint32_t fc_npiv_nvram_tbl_addr[PORT_MAX];		/* 0x1d4 */

	/* Shows last method that changed configuration of this device */
	enum curr_cfg_method_e curr_cfg;			/* 0x1dc */

	/* Storm FW version, shold be kept in the format 0xMMmmbbdd:
	 * MM - Major, mm - Minor, bb - Build ,dd - Drop
	 */
	uint32_t netproc_fw_ver;				/* 0x1e0 */

	/* Option ROM SMASH CLP version */
	uint32_t clp_ver;					/* 0x1e4 */

	uint32_t pcie_bus_num;					/* 0x1e8 */

	uint32_t sriov_switch_mode;				/* 0x1ec */
	#define SRIOV_SWITCH_MODE_NONE		0x0
	#define SRIOV_SWITCH_MODE_VEB		0x1
	#define SRIOV_SWITCH_MODE_VEPA		0x2

	uint8_t  rsrv2[E2_FUNC_MAX];				/* 0x1f0 */

	uint32_t img_inv_table_addr;	/* Address to INV_TABLE_P */ /* 0x1f4 */

	uint32_t mtu_size[E2_FUNC_MAX];				/* 0x1f8 */

	uint32_t os_driver_state[E2_FUNC_MAX];			/* 0x208 */
	#define OS_DRIVER_STATE_NOT_LOADED	0 /* not installed */
	#define OS_DRIVER_STATE_LOADING		1 /* transition state */
	#define OS_DRIVER_STATE_DISABLED	2 /* installed but disabled */
	#define OS_DRIVER_STATE_ACTIVE		3 /* installed and active */

	/* mini dump driver info */
	struct mdump_driver_info drv_info;			/* 0x218 */

	/* written by mfw, read by driver, eg. feature capability support */
	uint32_t mfw_flags;					/* 0x22c */
	#define DISABLE_EMBEDDED_LLDP_SUPPORT	0x00000001
};

#define VLAN_BITMAP_SIZE                        512
#define VLAN_PF_NUM_MAX                         8

struct pf_vlan_table {
	uint16_t pvid;
	uint8_t pcp;
	uint8_t rsvd;
	uint8_t trunk_vlan_bitmap[VLAN_BITMAP_SIZE];
	uint32_t rsvd1[4];
};

struct vlan_table_s {
	uint32_t version;
	#define VLAN_TABLE_IMAGE_VERSION_1      1
	uint8_t vlan_mode[NVM_PATH_MAX][PORT_MAX];
	#define VLAN_MODE_NORMAL                0
	#define VLAN_MODE_FILTER                1
	#define VLAN_MODE_QINQ                  2
	struct pf_vlan_table pf_vlans[VLAN_PF_NUM_MAX];
	uint32_t rsvd2[8];
};

/* The VLAN table Image is stored in Big Endian format */
struct nvm_vlan_table_image {
	struct vlan_table_s vlan_table;
	uint32_t crc;
};


struct emac_stats {
	uint32_t     rx_stat_ifhcinoctets;
	uint32_t     rx_stat_ifhcinbadoctets;
	uint32_t     rx_stat_etherstatsfragments;
	uint32_t     rx_stat_ifhcinucastpkts;
	uint32_t     rx_stat_ifhcinmulticastpkts;
	uint32_t     rx_stat_ifhcinbroadcastpkts;
	uint32_t     rx_stat_dot3statsfcserrors;
	uint32_t     rx_stat_dot3statsalignmenterrors;
	uint32_t     rx_stat_dot3statscarriersenseerrors;
	uint32_t     rx_stat_xonpauseframesreceived;
	uint32_t     rx_stat_xoffpauseframesreceived;
	uint32_t     rx_stat_maccontrolframesreceived;
	uint32_t     rx_stat_xoffstateentered;
	uint32_t     rx_stat_dot3statsframestoolong;
	uint32_t     rx_stat_etherstatsjabbers;
	uint32_t     rx_stat_etherstatsundersizepkts;
	uint32_t     rx_stat_etherstatspkts64octets;
	uint32_t     rx_stat_etherstatspkts65octetsto127octets;
	uint32_t     rx_stat_etherstatspkts128octetsto255octets;
	uint32_t     rx_stat_etherstatspkts256octetsto511octets;
	uint32_t     rx_stat_etherstatspkts512octetsto1023octets;
	uint32_t     rx_stat_etherstatspkts1024octetsto1522octets;
	uint32_t     rx_stat_etherstatspktsover1522octets;

	uint32_t     rx_stat_falsecarriererrors;

	uint32_t     tx_stat_ifhcoutoctets;
	uint32_t     tx_stat_ifhcoutbadoctets;
	uint32_t     tx_stat_etherstatscollisions;
	uint32_t     tx_stat_outxonsent;
	uint32_t     tx_stat_outxoffsent;
	uint32_t     tx_stat_flowcontroldone;
	uint32_t     tx_stat_dot3statssinglecollisionframes;
	uint32_t     tx_stat_dot3statsmultiplecollisionframes;
	uint32_t     tx_stat_dot3statsdeferredtransmissions;
	uint32_t     tx_stat_dot3statsexcessivecollisions;
	uint32_t     tx_stat_dot3statslatecollisions;
	uint32_t     tx_stat_ifhcoutucastpkts;
	uint32_t     tx_stat_ifhcoutmulticastpkts;
	uint32_t     tx_stat_ifhcoutbroadcastpkts;
	uint32_t     tx_stat_etherstatspkts64octets;
	uint32_t     tx_stat_etherstatspkts65octetsto127octets;
	uint32_t     tx_stat_etherstatspkts128octetsto255octets;
	uint32_t     tx_stat_etherstatspkts256octetsto511octets;
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets;
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets;
	uint32_t     tx_stat_etherstatspktsover1522octets;
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors;
};


struct bmac1_stats {
	uint32_t	tx_stat_gtpkt_lo;
	uint32_t	tx_stat_gtpkt_hi;
	uint32_t	tx_stat_gtxpf_lo;
	uint32_t	tx_stat_gtxpf_hi;
	uint32_t	tx_stat_gtfcs_lo;
	uint32_t	tx_stat_gtfcs_hi;
	uint32_t	tx_stat_gtmca_lo;
	uint32_t	tx_stat_gtmca_hi;
	uint32_t	tx_stat_gtbca_lo;
	uint32_t	tx_stat_gtbca_hi;
	uint32_t	tx_stat_gtfrg_lo;
	uint32_t	tx_stat_gtfrg_hi;
	uint32_t	tx_stat_gtovr_lo;
	uint32_t	tx_stat_gtovr_hi;
	uint32_t	tx_stat_gt64_lo;
	uint32_t	tx_stat_gt64_hi;
	uint32_t	tx_stat_gt127_lo;
	uint32_t	tx_stat_gt127_hi;
	uint32_t	tx_stat_gt255_lo;
	uint32_t	tx_stat_gt255_hi;
	uint32_t	tx_stat_gt511_lo;
	uint32_t	tx_stat_gt511_hi;
	uint32_t	tx_stat_gt1023_lo;
	uint32_t	tx_stat_gt1023_hi;
	uint32_t	tx_stat_gt1518_lo;
	uint32_t	tx_stat_gt1518_hi;
	uint32_t	tx_stat_gt2047_lo;
	uint32_t	tx_stat_gt2047_hi;
	uint32_t	tx_stat_gt4095_lo;
	uint32_t	tx_stat_gt4095_hi;
	uint32_t	tx_stat_gt9216_lo;
	uint32_t	tx_stat_gt9216_hi;
	uint32_t	tx_stat_gt16383_lo;
	uint32_t	tx_stat_gt16383_hi;
	uint32_t	tx_stat_gtmax_lo;
	uint32_t	tx_stat_gtmax_hi;
	uint32_t	tx_stat_gtufl_lo;
	uint32_t	tx_stat_gtufl_hi;
	uint32_t	tx_stat_gterr_lo;
	uint32_t	tx_stat_gterr_hi;
	uint32_t	tx_stat_gtbyt_lo;
	uint32_t	tx_stat_gtbyt_hi;

	uint32_t	rx_stat_gr64_lo;
	uint32_t	rx_stat_gr64_hi;
	uint32_t	rx_stat_gr127_lo;
	uint32_t	rx_stat_gr127_hi;
	uint32_t	rx_stat_gr255_lo;
	uint32_t	rx_stat_gr255_hi;
	uint32_t	rx_stat_gr511_lo;
	uint32_t	rx_stat_gr511_hi;
	uint32_t	rx_stat_gr1023_lo;
	uint32_t	rx_stat_gr1023_hi;
	uint32_t	rx_stat_gr1518_lo;
	uint32_t	rx_stat_gr1518_hi;
	uint32_t	rx_stat_gr2047_lo;
	uint32_t	rx_stat_gr2047_hi;
	uint32_t	rx_stat_gr4095_lo;
	uint32_t	rx_stat_gr4095_hi;
	uint32_t	rx_stat_gr9216_lo;
	uint32_t	rx_stat_gr9216_hi;
	uint32_t	rx_stat_gr16383_lo;
	uint32_t	rx_stat_gr16383_hi;
	uint32_t	rx_stat_grmax_lo;
	uint32_t	rx_stat_grmax_hi;
	uint32_t	rx_stat_grpkt_lo;
	uint32_t	rx_stat_grpkt_hi;
	uint32_t	rx_stat_grfcs_lo;
	uint32_t	rx_stat_grfcs_hi;
	uint32_t	rx_stat_grmca_lo;
	uint32_t	rx_stat_grmca_hi;
	uint32_t	rx_stat_grbca_lo;
	uint32_t	rx_stat_grbca_hi;
	uint32_t	rx_stat_grxcf_lo;
	uint32_t	rx_stat_grxcf_hi;
	uint32_t	rx_stat_grxpf_lo;
	uint32_t	rx_stat_grxpf_hi;
	uint32_t	rx_stat_grxuo_lo;
	uint32_t	rx_stat_grxuo_hi;
	uint32_t	rx_stat_grjbr_lo;
	uint32_t	rx_stat_grjbr_hi;
	uint32_t	rx_stat_grovr_lo;
	uint32_t	rx_stat_grovr_hi;
	uint32_t	rx_stat_grflr_lo;
	uint32_t	rx_stat_grflr_hi;
	uint32_t	rx_stat_grmeg_lo;
	uint32_t	rx_stat_grmeg_hi;
	uint32_t	rx_stat_grmeb_lo;
	uint32_t	rx_stat_grmeb_hi;
	uint32_t	rx_stat_grbyt_lo;
	uint32_t	rx_stat_grbyt_hi;
	uint32_t	rx_stat_grund_lo;
	uint32_t	rx_stat_grund_hi;
	uint32_t	rx_stat_grfrg_lo;
	uint32_t	rx_stat_grfrg_hi;
	uint32_t	rx_stat_grerb_lo;
	uint32_t	rx_stat_grerb_hi;
	uint32_t	rx_stat_grfre_lo;
	uint32_t	rx_stat_grfre_hi;
	uint32_t	rx_stat_gripj_lo;
	uint32_t	rx_stat_gripj_hi;
};

struct bmac2_stats {
	uint32_t	tx_stat_gtpk_lo; /* gtpok */
	uint32_t	tx_stat_gtpk_hi; /* gtpok */
	uint32_t	tx_stat_gtxpf_lo; /* gtpf */
	uint32_t	tx_stat_gtxpf_hi; /* gtpf */
	uint32_t	tx_stat_gtpp_lo; /* NEW BMAC2 */
	uint32_t	tx_stat_gtpp_hi; /* NEW BMAC2 */
	uint32_t	tx_stat_gtfcs_lo;
	uint32_t	tx_stat_gtfcs_hi;
	uint32_t	tx_stat_gtuca_lo; /* NEW BMAC2 */
	uint32_t	tx_stat_gtuca_hi; /* NEW BMAC2 */
	uint32_t	tx_stat_gtmca_lo;
	uint32_t	tx_stat_gtmca_hi;
	uint32_t	tx_stat_gtbca_lo;
	uint32_t	tx_stat_gtbca_hi;
	uint32_t	tx_stat_gtovr_lo;
	uint32_t	tx_stat_gtovr_hi;
	uint32_t	tx_stat_gtfrg_lo;
	uint32_t	tx_stat_gtfrg_hi;
	uint32_t	tx_stat_gtpkt1_lo; /* gtpkt */
	uint32_t	tx_stat_gtpkt1_hi; /* gtpkt */
	uint32_t	tx_stat_gt64_lo;
	uint32_t	tx_stat_gt64_hi;
	uint32_t	tx_stat_gt127_lo;
	uint32_t	tx_stat_gt127_hi;
	uint32_t	tx_stat_gt255_lo;
	uint32_t	tx_stat_gt255_hi;
	uint32_t	tx_stat_gt511_lo;
	uint32_t	tx_stat_gt511_hi;
	uint32_t	tx_stat_gt1023_lo;
	uint32_t	tx_stat_gt1023_hi;
	uint32_t	tx_stat_gt1518_lo;
	uint32_t	tx_stat_gt1518_hi;
	uint32_t	tx_stat_gt2047_lo;
	uint32_t	tx_stat_gt2047_hi;
	uint32_t	tx_stat_gt4095_lo;
	uint32_t	tx_stat_gt4095_hi;
	uint32_t	tx_stat_gt9216_lo;
	uint32_t	tx_stat_gt9216_hi;
	uint32_t	tx_stat_gt16383_lo;
	uint32_t	tx_stat_gt16383_hi;
	uint32_t	tx_stat_gtmax_lo;
	uint32_t	tx_stat_gtmax_hi;
	uint32_t	tx_stat_gtufl_lo;
	uint32_t	tx_stat_gtufl_hi;
	uint32_t	tx_stat_gterr_lo;
	uint32_t	tx_stat_gterr_hi;
	uint32_t	tx_stat_gtbyt_lo;
	uint32_t	tx_stat_gtbyt_hi;

	uint32_t	rx_stat_gr64_lo;
	uint32_t	rx_stat_gr64_hi;
	uint32_t	rx_stat_gr127_lo;
	uint32_t	rx_stat_gr127_hi;
	uint32_t	rx_stat_gr255_lo;
	uint32_t	rx_stat_gr255_hi;
	uint32_t	rx_stat_gr511_lo;
	uint32_t	rx_stat_gr511_hi;
	uint32_t	rx_stat_gr1023_lo;
	uint32_t	rx_stat_gr1023_hi;
	uint32_t	rx_stat_gr1518_lo;
	uint32_t	rx_stat_gr1518_hi;
	uint32_t	rx_stat_gr2047_lo;
	uint32_t	rx_stat_gr2047_hi;
	uint32_t	rx_stat_gr4095_lo;
	uint32_t	rx_stat_gr4095_hi;
	uint32_t	rx_stat_gr9216_lo;
	uint32_t	rx_stat_gr9216_hi;
	uint32_t	rx_stat_gr16383_lo;
	uint32_t	rx_stat_gr16383_hi;
	uint32_t	rx_stat_grmax_lo;
	uint32_t	rx_stat_grmax_hi;
	uint32_t	rx_stat_grpkt_lo;
	uint32_t	rx_stat_grpkt_hi;
	uint32_t	rx_stat_grfcs_lo;
	uint32_t	rx_stat_grfcs_hi;
	uint32_t	rx_stat_gruca_lo;
	uint32_t	rx_stat_gruca_hi;
	uint32_t	rx_stat_grmca_lo;
	uint32_t	rx_stat_grmca_hi;
	uint32_t	rx_stat_grbca_lo;
	uint32_t	rx_stat_grbca_hi;
	uint32_t	rx_stat_grxpf_lo; /* grpf */
	uint32_t	rx_stat_grxpf_hi; /* grpf */
	uint32_t	rx_stat_grpp_lo;
	uint32_t	rx_stat_grpp_hi;
	uint32_t	rx_stat_grxuo_lo; /* gruo */
	uint32_t	rx_stat_grxuo_hi; /* gruo */
	uint32_t	rx_stat_grjbr_lo;
	uint32_t	rx_stat_grjbr_hi;
	uint32_t	rx_stat_grovr_lo;
	uint32_t	rx_stat_grovr_hi;
	uint32_t	rx_stat_grxcf_lo; /* grcf */
	uint32_t	rx_stat_grxcf_hi; /* grcf */
	uint32_t	rx_stat_grflr_lo;
	uint32_t	rx_stat_grflr_hi;
	uint32_t	rx_stat_grpok_lo;
	uint32_t	rx_stat_grpok_hi;
	uint32_t	rx_stat_grmeg_lo;
	uint32_t	rx_stat_grmeg_hi;
	uint32_t	rx_stat_grmeb_lo;
	uint32_t	rx_stat_grmeb_hi;
	uint32_t	rx_stat_grbyt_lo;
	uint32_t	rx_stat_grbyt_hi;
	uint32_t	rx_stat_grund_lo;
	uint32_t	rx_stat_grund_hi;
	uint32_t	rx_stat_grfrg_lo;
	uint32_t	rx_stat_grfrg_hi;
	uint32_t	rx_stat_grerb_lo; /* grerrbyt */
	uint32_t	rx_stat_grerb_hi; /* grerrbyt */
	uint32_t	rx_stat_grfre_lo; /* grfrerr */
	uint32_t	rx_stat_grfre_hi; /* grfrerr */
	uint32_t	rx_stat_gripj_lo;
	uint32_t	rx_stat_gripj_hi;
};

struct mstat_stats {
	struct {
		/* OTE MSTAT on E3 has a bug where this register's contents are
		 * actually tx_gtxpok + tx_gtxpf + (possibly)tx_gtxpp
		 */
		uint32_t tx_gtxpok_lo;
		uint32_t tx_gtxpok_hi;
		uint32_t tx_gtxpf_lo;
		uint32_t tx_gtxpf_hi;
		uint32_t tx_gtxpp_lo;
		uint32_t tx_gtxpp_hi;
		uint32_t tx_gtfcs_lo;
		uint32_t tx_gtfcs_hi;
		uint32_t tx_gtuca_lo;
		uint32_t tx_gtuca_hi;
		uint32_t tx_gtmca_lo;
		uint32_t tx_gtmca_hi;
		uint32_t tx_gtgca_lo;
		uint32_t tx_gtgca_hi;
		uint32_t tx_gtpkt_lo;
		uint32_t tx_gtpkt_hi;
		uint32_t tx_gt64_lo;
		uint32_t tx_gt64_hi;
		uint32_t tx_gt127_lo;
		uint32_t tx_gt127_hi;
		uint32_t tx_gt255_lo;
		uint32_t tx_gt255_hi;
		uint32_t tx_gt511_lo;
		uint32_t tx_gt511_hi;
		uint32_t tx_gt1023_lo;
		uint32_t tx_gt1023_hi;
		uint32_t tx_gt1518_lo;
		uint32_t tx_gt1518_hi;
		uint32_t tx_gt2047_lo;
		uint32_t tx_gt2047_hi;
		uint32_t tx_gt4095_lo;
		uint32_t tx_gt4095_hi;
		uint32_t tx_gt9216_lo;
		uint32_t tx_gt9216_hi;
		uint32_t tx_gt16383_lo;
		uint32_t tx_gt16383_hi;
		uint32_t tx_gtufl_lo;
		uint32_t tx_gtufl_hi;
		uint32_t tx_gterr_lo;
		uint32_t tx_gterr_hi;
		uint32_t tx_gtbyt_lo;
		uint32_t tx_gtbyt_hi;
		uint32_t tx_collisions_lo;
		uint32_t tx_collisions_hi;
		uint32_t tx_singlecollision_lo;
		uint32_t tx_singlecollision_hi;
		uint32_t tx_multiplecollisions_lo;
		uint32_t tx_multiplecollisions_hi;
		uint32_t tx_deferred_lo;
		uint32_t tx_deferred_hi;
		uint32_t tx_excessivecollisions_lo;
		uint32_t tx_excessivecollisions_hi;
		uint32_t tx_latecollisions_lo;
		uint32_t tx_latecollisions_hi;
	} stats_tx;

	struct {
		uint32_t rx_gr64_lo;
		uint32_t rx_gr64_hi;
		uint32_t rx_gr127_lo;
		uint32_t rx_gr127_hi;
		uint32_t rx_gr255_lo;
		uint32_t rx_gr255_hi;
		uint32_t rx_gr511_lo;
		uint32_t rx_gr511_hi;
		uint32_t rx_gr1023_lo;
		uint32_t rx_gr1023_hi;
		uint32_t rx_gr1518_lo;
		uint32_t rx_gr1518_hi;
		uint32_t rx_gr2047_lo;
		uint32_t rx_gr2047_hi;
		uint32_t rx_gr4095_lo;
		uint32_t rx_gr4095_hi;
		uint32_t rx_gr9216_lo;
		uint32_t rx_gr9216_hi;
		uint32_t rx_gr16383_lo;
		uint32_t rx_gr16383_hi;
		uint32_t rx_grpkt_lo;
		uint32_t rx_grpkt_hi;
		uint32_t rx_grfcs_lo;
		uint32_t rx_grfcs_hi;
		uint32_t rx_gruca_lo;
		uint32_t rx_gruca_hi;
		uint32_t rx_grmca_lo;
		uint32_t rx_grmca_hi;
		uint32_t rx_grbca_lo;
		uint32_t rx_grbca_hi;
		uint32_t rx_grxpf_lo;
		uint32_t rx_grxpf_hi;
		uint32_t rx_grxpp_lo;
		uint32_t rx_grxpp_hi;
		uint32_t rx_grxuo_lo;
		uint32_t rx_grxuo_hi;
		uint32_t rx_grovr_lo;
		uint32_t rx_grovr_hi;
		uint32_t rx_grxcf_lo;
		uint32_t rx_grxcf_hi;
		uint32_t rx_grflr_lo;
		uint32_t rx_grflr_hi;
		uint32_t rx_grpok_lo;
		uint32_t rx_grpok_hi;
		uint32_t rx_grbyt_lo;
		uint32_t rx_grbyt_hi;
		uint32_t rx_grund_lo;
		uint32_t rx_grund_hi;
		uint32_t rx_grfrg_lo;
		uint32_t rx_grfrg_hi;
		uint32_t rx_grerb_lo;
		uint32_t rx_grerb_hi;
		uint32_t rx_grfre_lo;
		uint32_t rx_grfre_hi;

		uint32_t rx_alignmenterrors_lo;
		uint32_t rx_alignmenterrors_hi;
		uint32_t rx_falsecarrier_lo;
		uint32_t rx_falsecarrier_hi;
		uint32_t rx_llfcmsgcnt_lo;
		uint32_t rx_llfcmsgcnt_hi;
	} stats_rx;
};

union mac_stats {
	struct emac_stats	emac_stats;
	struct bmac1_stats	bmac1_stats;
	struct bmac2_stats	bmac2_stats;
	struct mstat_stats	mstat_stats;
};


struct mac_stx {
	/* in_bad_octets */
	uint32_t     rx_stat_ifhcinbadoctets_hi;
	uint32_t     rx_stat_ifhcinbadoctets_lo;

	/* out_bad_octets */
	uint32_t     tx_stat_ifhcoutbadoctets_hi;
	uint32_t     tx_stat_ifhcoutbadoctets_lo;

	/* crc_receive_errors */
	uint32_t     rx_stat_dot3statsfcserrors_hi;
	uint32_t     rx_stat_dot3statsfcserrors_lo;
	/* alignment_errors */
	uint32_t     rx_stat_dot3statsalignmenterrors_hi;
	uint32_t     rx_stat_dot3statsalignmenterrors_lo;
	/* carrier_sense_errors */
	uint32_t     rx_stat_dot3statscarriersenseerrors_hi;
	uint32_t     rx_stat_dot3statscarriersenseerrors_lo;
	/* false_carrier_detections */
	uint32_t     rx_stat_falsecarriererrors_hi;
	uint32_t     rx_stat_falsecarriererrors_lo;

	/* runt_packets_received */
	uint32_t     rx_stat_etherstatsundersizepkts_hi;
	uint32_t     rx_stat_etherstatsundersizepkts_lo;
	/* jabber_packets_received */
	uint32_t     rx_stat_dot3statsframestoolong_hi;
	uint32_t     rx_stat_dot3statsframestoolong_lo;

	/* error_runt_packets_received */
	uint32_t     rx_stat_etherstatsfragments_hi;
	uint32_t     rx_stat_etherstatsfragments_lo;
	/* error_jabber_packets_received */
	uint32_t     rx_stat_etherstatsjabbers_hi;
	uint32_t     rx_stat_etherstatsjabbers_lo;

	/* control_frames_received */
	uint32_t     rx_stat_maccontrolframesreceived_hi;
	uint32_t     rx_stat_maccontrolframesreceived_lo;
	uint32_t     rx_stat_mac_xpf_hi;
	uint32_t     rx_stat_mac_xpf_lo;
	uint32_t     rx_stat_mac_xcf_hi;
	uint32_t     rx_stat_mac_xcf_lo;

	/* xoff_state_entered */
	uint32_t     rx_stat_xoffstateentered_hi;
	uint32_t     rx_stat_xoffstateentered_lo;
	/* pause_xon_frames_received */
	uint32_t     rx_stat_xonpauseframesreceived_hi;
	uint32_t     rx_stat_xonpauseframesreceived_lo;
	/* pause_xoff_frames_received */
	uint32_t     rx_stat_xoffpauseframesreceived_hi;
	uint32_t     rx_stat_xoffpauseframesreceived_lo;
	/* pause_xon_frames_transmitted */
	uint32_t     tx_stat_outxonsent_hi;
	uint32_t     tx_stat_outxonsent_lo;
	/* pause_xoff_frames_transmitted */
	uint32_t     tx_stat_outxoffsent_hi;
	uint32_t     tx_stat_outxoffsent_lo;
	/* flow_control_done */
	uint32_t     tx_stat_flowcontroldone_hi;
	uint32_t     tx_stat_flowcontroldone_lo;

	/* ether_stats_collisions */
	uint32_t     tx_stat_etherstatscollisions_hi;
	uint32_t     tx_stat_etherstatscollisions_lo;
	/* single_collision_transmit_frames */
	uint32_t     tx_stat_dot3statssinglecollisionframes_hi;
	uint32_t     tx_stat_dot3statssinglecollisionframes_lo;
	/* multiple_collision_transmit_frames */
	uint32_t     tx_stat_dot3statsmultiplecollisionframes_hi;
	uint32_t     tx_stat_dot3statsmultiplecollisionframes_lo;
	/* deferred_transmissions */
	uint32_t     tx_stat_dot3statsdeferredtransmissions_hi;
	uint32_t     tx_stat_dot3statsdeferredtransmissions_lo;
	/* excessive_collision_frames */
	uint32_t     tx_stat_dot3statsexcessivecollisions_hi;
	uint32_t     tx_stat_dot3statsexcessivecollisions_lo;
	/* late_collision_frames */
	uint32_t     tx_stat_dot3statslatecollisions_hi;
	uint32_t     tx_stat_dot3statslatecollisions_lo;

	/* frames_transmitted_64_bytes */
	uint32_t     tx_stat_etherstatspkts64octets_hi;
	uint32_t     tx_stat_etherstatspkts64octets_lo;
	/* frames_transmitted_65_127_bytes */
	uint32_t     tx_stat_etherstatspkts65octetsto127octets_hi;
	uint32_t     tx_stat_etherstatspkts65octetsto127octets_lo;
	/* frames_transmitted_128_255_bytes */
	uint32_t     tx_stat_etherstatspkts128octetsto255octets_hi;
	uint32_t     tx_stat_etherstatspkts128octetsto255octets_lo;
	/* frames_transmitted_256_511_bytes */
	uint32_t     tx_stat_etherstatspkts256octetsto511octets_hi;
	uint32_t     tx_stat_etherstatspkts256octetsto511octets_lo;
	/* frames_transmitted_512_1023_bytes */
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets_hi;
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets_lo;
	/* frames_transmitted_1024_1522_bytes */
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets_hi;
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets_lo;
	/* frames_transmitted_1523_9022_bytes */
	uint32_t     tx_stat_etherstatspktsover1522octets_hi;
	uint32_t     tx_stat_etherstatspktsover1522octets_lo;
	uint32_t     tx_stat_mac_2047_hi;
	uint32_t     tx_stat_mac_2047_lo;
	uint32_t     tx_stat_mac_4095_hi;
	uint32_t     tx_stat_mac_4095_lo;
	uint32_t     tx_stat_mac_9216_hi;
	uint32_t     tx_stat_mac_9216_lo;
	uint32_t     tx_stat_mac_16383_hi;
	uint32_t     tx_stat_mac_16383_lo;

	/* internal_mac_transmit_errors */
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors_hi;
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors_lo;

	/* if_out_discards */
	uint32_t     tx_stat_mac_ufl_hi;
	uint32_t     tx_stat_mac_ufl_lo;
};


#define MAC_STX_IDX_MAX                     2

struct host_port_stats {
	uint32_t            host_port_stats_counter;

	struct mac_stx mac_stx[MAC_STX_IDX_MAX];

	uint32_t            brb_drop_hi;
	uint32_t            brb_drop_lo;

	uint32_t            not_used; /* obsolete as of MFW 7.2.1 */

	uint32_t            pfc_frames_tx_hi;
	uint32_t            pfc_frames_tx_lo;
	uint32_t            pfc_frames_rx_hi;
	uint32_t            pfc_frames_rx_lo;

	uint32_t            eee_lpi_count_hi;
	uint32_t            eee_lpi_count_lo;
};


struct host_func_stats {
	uint32_t     host_func_stats_start;

	uint32_t     total_bytes_received_hi;
	uint32_t     total_bytes_received_lo;

	uint32_t     total_bytes_transmitted_hi;
	uint32_t     total_bytes_transmitted_lo;

	uint32_t     total_unicast_packets_received_hi;
	uint32_t     total_unicast_packets_received_lo;

	uint32_t     total_multicast_packets_received_hi;
	uint32_t     total_multicast_packets_received_lo;

	uint32_t     total_broadcast_packets_received_hi;
	uint32_t     total_broadcast_packets_received_lo;

	uint32_t     total_unicast_packets_transmitted_hi;
	uint32_t     total_unicast_packets_transmitted_lo;

	uint32_t     total_multicast_packets_transmitted_hi;
	uint32_t     total_multicast_packets_transmitted_lo;

	uint32_t     total_broadcast_packets_transmitted_hi;
	uint32_t     total_broadcast_packets_transmitted_lo;

	uint32_t     valid_bytes_received_hi;
	uint32_t     valid_bytes_received_lo;

	uint32_t     host_func_stats_end;
};

/* VIC definitions */
#define VICSTATST_UIF_INDEX 2

/*
 * stats collected for afex.
 * NOTE: structure is exactly as expected to be received by the switch.
 *       order must remain exactly as is unless protocol changes !
 */
struct afex_stats {
	uint32_t tx_unicast_frames_hi;
	uint32_t tx_unicast_frames_lo;
	uint32_t tx_unicast_bytes_hi;
	uint32_t tx_unicast_bytes_lo;
	uint32_t tx_multicast_frames_hi;
	uint32_t tx_multicast_frames_lo;
	uint32_t tx_multicast_bytes_hi;
	uint32_t tx_multicast_bytes_lo;
	uint32_t tx_broadcast_frames_hi;
	uint32_t tx_broadcast_frames_lo;
	uint32_t tx_broadcast_bytes_hi;
	uint32_t tx_broadcast_bytes_lo;
	uint32_t tx_frames_discarded_hi;
	uint32_t tx_frames_discarded_lo;
	uint32_t tx_frames_dropped_hi;
	uint32_t tx_frames_dropped_lo;

	uint32_t rx_unicast_frames_hi;
	uint32_t rx_unicast_frames_lo;
	uint32_t rx_unicast_bytes_hi;
	uint32_t rx_unicast_bytes_lo;
	uint32_t rx_multicast_frames_hi;
	uint32_t rx_multicast_frames_lo;
	uint32_t rx_multicast_bytes_hi;
	uint32_t rx_multicast_bytes_lo;
	uint32_t rx_broadcast_frames_hi;
	uint32_t rx_broadcast_frames_lo;
	uint32_t rx_broadcast_bytes_hi;
	uint32_t rx_broadcast_bytes_lo;
	uint32_t rx_frames_discarded_hi;
	uint32_t rx_frames_discarded_lo;
	uint32_t rx_frames_dropped_hi;
	uint32_t rx_frames_dropped_lo;
};

/* To maintain backward compatibility between FW and drivers, new elements */
/* should be added to the end of the structure. */

/* Per  Port Statistics    */
struct port_info {
	uint32_t size; /* size of this structure (i.e. sizeof(port_info))  */
	uint32_t enabled;      /* 0 =Disabled, 1= Enabled */
	uint32_t link_speed;   /* multiplier of 100Mb */
	uint32_t wol_support;  /* WoL Support (i.e. Non-Zero if WOL supported ) */
	uint32_t flow_control; /* 802.3X Flow Ctrl. 0=off 1=RX 2=TX 3=RX&TX.*/
	uint32_t flex10;     /* Flex10 mode enabled. non zero = yes */
	uint32_t rx_drops;  /* RX Discards. Counters roll over, never reset */
	uint32_t rx_errors; /* RX Errors. Physical Port Stats L95, All PFs and NC-SI.
				   This is flagged by Consumer as an error. */
	uint32_t rx_uncast_lo;   /* RX Unicast Packets. Free running counters: */
	uint32_t rx_uncast_hi;   /* RX Unicast Packets. Free running counters: */
	uint32_t rx_mcast_lo;    /* RX Multicast Packets  */
	uint32_t rx_mcast_hi;    /* RX Multicast Packets  */
	uint32_t rx_bcast_lo;    /* RX Broadcast Packets  */
	uint32_t rx_bcast_hi;    /* RX Broadcast Packets  */
	uint32_t tx_uncast_lo;   /* TX Unicast Packets   */
	uint32_t tx_uncast_hi;   /* TX Unicast Packets   */
	uint32_t tx_mcast_lo;    /* TX Multicast Packets  */
	uint32_t tx_mcast_hi;    /* TX Multicast Packets  */
	uint32_t tx_bcast_lo;    /* TX Broadcast Packets  */
	uint32_t tx_bcast_hi;    /* TX Broadcast Packets  */
	uint32_t tx_errors;      /* TX Errors              */
	uint32_t tx_discards;    /* TX Discards          */
	uint32_t rx_frames_lo;   /* RX Frames received  */
	uint32_t rx_frames_hi;   /* RX Frames received  */
	uint32_t rx_bytes_lo;    /* RX Bytes received    */
	uint32_t rx_bytes_hi;    /* RX Bytes received    */
	uint32_t tx_frames_lo;   /* TX Frames sent      */
	uint32_t tx_frames_hi;   /* TX Frames sent      */
	uint32_t tx_bytes_lo;    /* TX Bytes sent        */
	uint32_t tx_bytes_hi;    /* TX Bytes sent        */
	uint32_t link_status;  /* Port P Link Status. 1:0 bit for port enabled.
				1:1 bit for link good,
				2:1 Set if link changed between last poll. */
	uint32_t tx_pfc_frames_lo;   /* PFC Frames sent.    */
	uint32_t tx_pfc_frames_hi;   /* PFC Frames sent.    */
	uint32_t rx_pfc_frames_lo;   /* PFC Frames Received. */
	uint32_t rx_pfc_frames_hi;   /* PFC Frames Received. */
};


#define BNX2X_5710_FW_MAJOR_VERSION			7
#define BNX2X_5710_FW_MINOR_VERSION			13
#define BNX2X_5710_FW_REVISION_VERSION		11
#define BNX2X_5710_FW_ENGINEERING_VERSION		0
#define BNX2X_5710_FW_COMPILE_FLAGS			1


/*
 * attention bits
 */
struct atten_sp_status_block {
	__le32 attn_bits;
	__le32 attn_bits_ack;
	uint8_t status_block_id;
	uint8_t reserved0;
	__le16 attn_bits_index;
	__le32 reserved1;
};


/*
 * The eth aggregative context of Cstorm
 */
struct cstorm_eth_ag_context {
	uint32_t __reserved0[10];
};


/*
 * dmae command structure
 */
struct dmae_command {
	uint32_t opcode;
#define DMAE_COMMAND_SRC (0x1 << 0)
#define DMAE_COMMAND_SRC_SHIFT 0
#define DMAE_COMMAND_DST (0x3 << 1)
#define DMAE_COMMAND_DST_SHIFT 1
#define DMAE_COMMAND_C_DST (0x1 << 3)
#define DMAE_COMMAND_C_DST_SHIFT 3
#define DMAE_COMMAND_C_TYPE_ENABLE (0x1 << 4)
#define DMAE_COMMAND_C_TYPE_ENABLE_SHIFT 4
#define DMAE_COMMAND_C_TYPE_CRC_ENABLE (0x1 << 5)
#define DMAE_COMMAND_C_TYPE_CRC_ENABLE_SHIFT 5
#define DMAE_COMMAND_C_TYPE_CRC_OFFSET (0x7 << 6)
#define DMAE_COMMAND_C_TYPE_CRC_OFFSET_SHIFT 6
#define DMAE_COMMAND_ENDIANITY (0x3 << 9)
#define DMAE_COMMAND_ENDIANITY_SHIFT 9
#define DMAE_COMMAND_PORT (0x1 << 11)
#define DMAE_COMMAND_PORT_SHIFT 11
#define DMAE_COMMAND_CRC_RESET (0x1 << 12)
#define DMAE_COMMAND_CRC_RESET_SHIFT 12
#define DMAE_COMMAND_SRC_RESET (0x1 << 13)
#define DMAE_COMMAND_SRC_RESET_SHIFT 13
#define DMAE_COMMAND_DST_RESET (0x1 << 14)
#define DMAE_COMMAND_DST_RESET_SHIFT 14
#define DMAE_COMMAND_E1HVN (0x3 << 15)
#define DMAE_COMMAND_E1HVN_SHIFT 15
#define DMAE_COMMAND_DST_VN (0x3 << 17)
#define DMAE_COMMAND_DST_VN_SHIFT 17
#define DMAE_COMMAND_C_FUNC (0x1 << 19)
#define DMAE_COMMAND_C_FUNC_SHIFT 19
#define DMAE_COMMAND_ERR_POLICY (0x3 << 20)
#define DMAE_COMMAND_ERR_POLICY_SHIFT 20
#define DMAE_COMMAND_RESERVED0 (0x3FF << 22)
#define DMAE_COMMAND_RESERVED0_SHIFT 22
	uint32_t src_addr_lo;
	uint32_t src_addr_hi;
	uint32_t dst_addr_lo;
	uint32_t dst_addr_hi;
#if defined(__BIG_ENDIAN)
	uint16_t opcode_iov;
#define DMAE_COMMAND_SRC_VFID (0x3F << 0)
#define DMAE_COMMAND_SRC_VFID_SHIFT 0
#define DMAE_COMMAND_SRC_VFPF (0x1 << 6)
#define DMAE_COMMAND_SRC_VFPF_SHIFT 6
#define DMAE_COMMAND_RESERVED1 (0x1 << 7)
#define DMAE_COMMAND_RESERVED1_SHIFT 7
#define DMAE_COMMAND_DST_VFID (0x3F << 8)
#define DMAE_COMMAND_DST_VFID_SHIFT 8
#define DMAE_COMMAND_DST_VFPF (0x1 << 14)
#define DMAE_COMMAND_DST_VFPF_SHIFT 14
#define DMAE_COMMAND_RESERVED2 (0x1 << 15)
#define DMAE_COMMAND_RESERVED2_SHIFT 15
	uint16_t len;
#elif defined(__LITTLE_ENDIAN)
	uint16_t len;
	uint16_t opcode_iov;
#define DMAE_COMMAND_SRC_VFID (0x3F << 0)
#define DMAE_COMMAND_SRC_VFID_SHIFT 0
#define DMAE_COMMAND_SRC_VFPF (0x1 << 6)
#define DMAE_COMMAND_SRC_VFPF_SHIFT 6
#define DMAE_COMMAND_RESERVED1 (0x1 << 7)
#define DMAE_COMMAND_RESERVED1_SHIFT 7
#define DMAE_COMMAND_DST_VFID (0x3F << 8)
#define DMAE_COMMAND_DST_VFID_SHIFT 8
#define DMAE_COMMAND_DST_VFPF (0x1 << 14)
#define DMAE_COMMAND_DST_VFPF_SHIFT 14
#define DMAE_COMMAND_RESERVED2 (0x1 << 15)
#define DMAE_COMMAND_RESERVED2_SHIFT 15
#endif
	uint32_t comp_addr_lo;
	uint32_t comp_addr_hi;
	uint32_t comp_val;
	uint32_t crc32;
	uint32_t crc32_c;
#if defined(__BIG_ENDIAN)
	uint16_t crc16_c;
	uint16_t crc16;
#elif defined(__LITTLE_ENDIAN)
	uint16_t crc16;
	uint16_t crc16_c;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved3;
	uint16_t crc_t10;
#elif defined(__LITTLE_ENDIAN)
	uint16_t crc_t10;
	uint16_t reserved3;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t xsum8;
	uint16_t xsum16;
#elif defined(__LITTLE_ENDIAN)
	uint16_t xsum16;
	uint16_t xsum8;
#endif
};


/*
 * common data for all protocols
 */
struct doorbell_hdr {
	uint8_t header;
#define DOORBELL_HDR_RX (0x1 << 0)
#define DOORBELL_HDR_RX_SHIFT 0
#define DOORBELL_HDR_DB_TYPE (0x1 << 1)
#define DOORBELL_HDR_DB_TYPE_SHIFT 1
#define DOORBELL_HDR_DPM_SIZE (0x3 << 2)
#define DOORBELL_HDR_DPM_SIZE_SHIFT 2
#define DOORBELL_HDR_CONN_TYPE (0xF << 4)
#define DOORBELL_HDR_CONN_TYPE_SHIFT 4
};

/*
 * Ethernet doorbell
 */
struct eth_tx_doorbell {
#if defined(__BIG_ENDIAN)
	uint16_t npackets;
	uint8_t params;
#define ETH_TX_DOORBELL_NUM_BDS (0x3F << 0)
#define ETH_TX_DOORBELL_NUM_BDS_SHIFT 0
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG (0x1 << 6)
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG_SHIFT 6
#define ETH_TX_DOORBELL_SPARE (0x1 << 7)
#define ETH_TX_DOORBELL_SPARE_SHIFT 7
	struct doorbell_hdr hdr;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr hdr;
	uint8_t params;
#define ETH_TX_DOORBELL_NUM_BDS (0x3F << 0)
#define ETH_TX_DOORBELL_NUM_BDS_SHIFT 0
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG (0x1 << 6)
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG_SHIFT 6
#define ETH_TX_DOORBELL_SPARE (0x1 << 7)
#define ETH_TX_DOORBELL_SPARE_SHIFT 7
	uint16_t npackets;
#endif
};


/*
 * 3 lines. status block
 */
struct hc_status_block_e1x {
	__le16 index_values[HC_SB_MAX_INDICES_E1X];
	__le16 running_index[HC_SB_MAX_SM];
	__le32 rsrv[11];
};

/*
 * host status block
 */
struct host_hc_status_block_e1x {
	struct hc_status_block_e1x sb;
};


/*
 * 3 lines. status block
 */
struct hc_status_block_e2 {
	__le16 index_values[HC_SB_MAX_INDICES_E2];
	__le16 running_index[HC_SB_MAX_SM];
	__le32 reserved[11];
};

/*
 * host status block
 */
struct host_hc_status_block_e2 {
	struct hc_status_block_e2 sb;
};


/*
 * 5 lines. slow-path status block
 */
struct hc_sp_status_block {
	__le16 index_values[HC_SP_SB_MAX_INDICES];
	__le16 running_index;
	__le16 rsrv;
	uint32_t rsrv1;
};

/*
 * host status block
 */
struct host_sp_status_block {
	struct atten_sp_status_block atten_status_block;
	struct hc_sp_status_block sp_sb;
};


/*
 * IGU driver acknowledgment register
 */
struct igu_ack_register {
#if defined(__BIG_ENDIAN)
	uint16_t sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F << 0)
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7 << 5)
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1 << 8)
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3 << 9)
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F << 11)
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
	uint16_t status_block_index;
#elif defined(__LITTLE_ENDIAN)
	uint16_t status_block_index;
	uint16_t sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F << 0)
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7 << 5)
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1 << 8)
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3 << 9)
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F << 11)
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
#endif
};


/*
 * IGU driver acknowledgement register
 */
struct igu_backward_compatible {
	uint32_t sb_id_and_flags;
#define IGU_BACKWARD_COMPATIBLE_SB_INDEX (0xFFFF << 0)
#define IGU_BACKWARD_COMPATIBLE_SB_INDEX_SHIFT 0
#define IGU_BACKWARD_COMPATIBLE_SB_SELECT (0x1F << 16)
#define IGU_BACKWARD_COMPATIBLE_SB_SELECT_SHIFT 16
#define IGU_BACKWARD_COMPATIBLE_SEGMENT_ACCESS (0x7 << 21)
#define IGU_BACKWARD_COMPATIBLE_SEGMENT_ACCESS_SHIFT 21
#define IGU_BACKWARD_COMPATIBLE_BUPDATE (0x1 << 24)
#define IGU_BACKWARD_COMPATIBLE_BUPDATE_SHIFT 24
#define IGU_BACKWARD_COMPATIBLE_ENABLE_INT (0x3 << 25)
#define IGU_BACKWARD_COMPATIBLE_ENABLE_INT_SHIFT 25
#define IGU_BACKWARD_COMPATIBLE_RESERVED_0 (0x1F << 27)
#define IGU_BACKWARD_COMPATIBLE_RESERVED_0_SHIFT 27
	uint32_t reserved_2;
};


/*
 * IGU driver acknowledgement register
 */
struct igu_regular {
	uint32_t sb_id_and_flags;
#define IGU_REGULAR_SB_INDEX (0xFFFFF << 0)
#define IGU_REGULAR_SB_INDEX_SHIFT 0
#define IGU_REGULAR_RESERVED0 (0x1 << 20)
#define IGU_REGULAR_RESERVED0_SHIFT 20
#define IGU_REGULAR_SEGMENT_ACCESS (0x7 << 21)
#define IGU_REGULAR_SEGMENT_ACCESS_SHIFT 21
#define IGU_REGULAR_BUPDATE (0x1 << 24)
#define IGU_REGULAR_BUPDATE_SHIFT 24
#define IGU_REGULAR_ENABLE_INT (0x3 << 25)
#define IGU_REGULAR_ENABLE_INT_SHIFT 25
#define IGU_REGULAR_RESERVED_1 (0x1 << 27)
#define IGU_REGULAR_RESERVED_1_SHIFT 27
#define IGU_REGULAR_CLEANUP_TYPE (0x3 << 28)
#define IGU_REGULAR_CLEANUP_TYPE_SHIFT 28
#define IGU_REGULAR_CLEANUP_SET (0x1 << 30)
#define IGU_REGULAR_CLEANUP_SET_SHIFT 30
#define IGU_REGULAR_BCLEANUP (0x1 << 31)
#define IGU_REGULAR_BCLEANUP_SHIFT 31
	uint32_t reserved_2;
};

/*
 * IGU driver acknowledgement register
 */
union igu_consprod_reg {
	struct igu_regular regular;
	struct igu_backward_compatible backward_compatible;
};


/*
 * Igu control commands
 */
enum igu_ctrl_cmd {
	IGU_CTRL_CMD_TYPE_RD,
	IGU_CTRL_CMD_TYPE_WR,
	MAX_IGU_CTRL_CMD};


/*
 * Control register for the IGU command register
 */
struct igu_ctrl_reg {
	uint32_t ctrl_data;
#define IGU_CTRL_REG_ADDRESS (0xFFF << 0)
#define IGU_CTRL_REG_ADDRESS_SHIFT 0
#define IGU_CTRL_REG_FID (0x7F << 12)
#define IGU_CTRL_REG_FID_SHIFT 12
#define IGU_CTRL_REG_RESERVED (0x1 << 19)
#define IGU_CTRL_REG_RESERVED_SHIFT 19
#define IGU_CTRL_REG_TYPE (0x1 << 20)
#define IGU_CTRL_REG_TYPE_SHIFT 20
#define IGU_CTRL_REG_UNUSED (0x7FF << 21)
#define IGU_CTRL_REG_UNUSED_SHIFT 21
};


/*
 * Igu interrupt command
 */
enum igu_int_cmd {
	IGU_INT_ENABLE,
	IGU_INT_DISABLE,
	IGU_INT_NOP,
	IGU_INT_NOP2,
	MAX_IGU_INT_CMD};


/*
 * Igu segments
 */
enum igu_seg_access {
	IGU_SEG_ACCESS_NORM,
	IGU_SEG_ACCESS_DEF,
	IGU_SEG_ACCESS_ATTN,
	MAX_IGU_SEG_ACCESS};


/*
 * Parser parsing flags field
 */
struct parsing_flags {
	__le16 flags;
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE (0x1 << 0)
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE_SHIFT 0
#define PARSING_FLAGS_VLAN (0x1 << 1)
#define PARSING_FLAGS_VLAN_SHIFT 1
#define PARSING_FLAGS_EXTRA_VLAN (0x1 << 2)
#define PARSING_FLAGS_EXTRA_VLAN_SHIFT 2
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL (0x3 << 3)
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT 3
#define PARSING_FLAGS_IP_OPTIONS (0x1 << 5)
#define PARSING_FLAGS_IP_OPTIONS_SHIFT 5
#define PARSING_FLAGS_FRAGMENTATION_STATUS (0x1 << 6)
#define PARSING_FLAGS_FRAGMENTATION_STATUS_SHIFT 6
#define PARSING_FLAGS_OVER_IP_PROTOCOL (0x3 << 7)
#define PARSING_FLAGS_OVER_IP_PROTOCOL_SHIFT 7
#define PARSING_FLAGS_PURE_ACK_INDICATION (0x1 << 9)
#define PARSING_FLAGS_PURE_ACK_INDICATION_SHIFT 9
#define PARSING_FLAGS_TCP_OPTIONS_EXIST (0x1 << 10)
#define PARSING_FLAGS_TCP_OPTIONS_EXIST_SHIFT 10
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG (0x1 << 11)
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG_SHIFT 11
#define PARSING_FLAGS_CONNECTION_MATCH (0x1 << 12)
#define PARSING_FLAGS_CONNECTION_MATCH_SHIFT 12
#define PARSING_FLAGS_LLC_SNAP (0x1 << 13)
#define PARSING_FLAGS_LLC_SNAP_SHIFT 13
#define PARSING_FLAGS_RESERVED0 (0x3 << 14)
#define PARSING_FLAGS_RESERVED0_SHIFT 14
};


/*
 * Parsing flags for TCP ACK type
 */
enum prs_flags_ack_type {
	PRS_FLAG_PUREACK_PIGGY,
	PRS_FLAG_PUREACK_PURE,
	MAX_PRS_FLAGS_ACK_TYPE};


/*
 * Parsing flags for Ethernet address type
 */
enum prs_flags_eth_addr_type {
	PRS_FLAG_ETHTYPE_NON_UNICAST,
	PRS_FLAG_ETHTYPE_UNICAST,
	MAX_PRS_FLAGS_ETH_ADDR_TYPE};


/*
 * Parsing flags for over-ethernet protocol
 */
enum prs_flags_over_eth {
	PRS_FLAG_OVERETH_UNKNOWN,
	PRS_FLAG_OVERETH_IPV4,
	PRS_FLAG_OVERETH_IPV6,
	PRS_FLAG_OVERETH_LLCSNAP_UNKNOWN,
	MAX_PRS_FLAGS_OVER_ETH};


/*
 * Parsing flags for over-IP protocol
 */
enum prs_flags_over_ip {
	PRS_FLAG_OVERIP_UNKNOWN,
	PRS_FLAG_OVERIP_TCP,
	PRS_FLAG_OVERIP_UDP,
	MAX_PRS_FLAGS_OVER_IP};


/*
 * SDM operation gen command (generate aggregative interrupt)
 */
struct sdm_op_gen {
	__le32 command;
#define SDM_OP_GEN_COMP_PARAM (0x1F << 0)
#define SDM_OP_GEN_COMP_PARAM_SHIFT 0
#define SDM_OP_GEN_COMP_TYPE (0x7 << 5)
#define SDM_OP_GEN_COMP_TYPE_SHIFT 5
#define SDM_OP_GEN_AGG_VECT_IDX (0xFF << 8)
#define SDM_OP_GEN_AGG_VECT_IDX_SHIFT 8
#define SDM_OP_GEN_AGG_VECT_IDX_VALID (0x1 << 16)
#define SDM_OP_GEN_AGG_VECT_IDX_VALID_SHIFT 16
#define SDM_OP_GEN_RESERVED (0x7FFF << 17)
#define SDM_OP_GEN_RESERVED_SHIFT 17
};


/*
 * Timers connection context
 */
struct timers_block_context {
	uint32_t __reserved_0;
	uint32_t __reserved_1;
	uint32_t __reserved_2;
	uint32_t flags;
#define __TIMERS_BLOCK_CONTEXT_NUM_OF_ACTIVE_TIMERS (0x3 << 0)
#define __TIMERS_BLOCK_CONTEXT_NUM_OF_ACTIVE_TIMERS_SHIFT 0
#define TIMERS_BLOCK_CONTEXT_CONN_VALID_FLG (0x1 << 2)
#define TIMERS_BLOCK_CONTEXT_CONN_VALID_FLG_SHIFT 2
#define __TIMERS_BLOCK_CONTEXT_RESERVED0 (0x1FFFFFFF << 3)
#define __TIMERS_BLOCK_CONTEXT_RESERVED0_SHIFT 3
};


/*
 * The eth aggregative context of Tstorm
 */
struct tstorm_eth_ag_context {
	uint32_t __reserved0[14];
};


/*
 * The eth aggregative context of Ustorm
 */
struct ustorm_eth_ag_context {
	uint32_t __reserved0;
#if defined(__BIG_ENDIAN)
	uint8_t cdu_usage;
	uint8_t __reserved2;
	uint16_t __reserved1;
#elif defined(__LITTLE_ENDIAN)
	uint16_t __reserved1;
	uint8_t __reserved2;
	uint8_t cdu_usage;
#endif
	uint32_t __reserved3[6];
};


/*
 * The eth aggregative context of Xstorm
 */
struct xstorm_eth_ag_context {
	uint32_t reserved0;
#if defined(__BIG_ENDIAN)
	uint8_t cdu_reserved;
	uint8_t reserved2;
	uint16_t reserved1;
#elif defined(__LITTLE_ENDIAN)
	uint16_t reserved1;
	uint8_t reserved2;
	uint8_t cdu_reserved;
#endif
	uint32_t reserved3[30];
};


/*
 * doorbell message sent to the chip
 */
struct doorbell {
#if defined(__BIG_ENDIAN)
	uint16_t zero_fill2;
	uint8_t zero_fill1;
	struct doorbell_hdr header;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr header;
	uint8_t zero_fill1;
	uint16_t zero_fill2;
#endif
};


/*
 * doorbell message sent to the chip
 */
struct doorbell_set_prod {
#if defined(__BIG_ENDIAN)
	uint16_t prod;
	uint8_t zero_fill1;
	struct doorbell_hdr header;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr header;
	uint8_t zero_fill1;
	uint16_t prod;
#endif
};


struct regpair {
	__le32 lo;
	__le32 hi;
};


struct regpair_native {
	uint32_t lo;
	uint32_t hi;
};


/*
 * Classify rule opcodes in E2/E3
 */
enum classify_rule {
	CLASSIFY_RULE_OPCODE_MAC,
	CLASSIFY_RULE_OPCODE_VLAN,
	CLASSIFY_RULE_OPCODE_PAIR,
	CLASSIFY_RULE_OPCODE_IMAC_VNI,
	MAX_CLASSIFY_RULE};


/*
 * Classify rule types in E2/E3
 */
enum classify_rule_action_type {
	CLASSIFY_RULE_REMOVE,
	CLASSIFY_RULE_ADD,
	MAX_CLASSIFY_RULE_ACTION_TYPE};


/*
 * client init ramrod data
 */
struct client_init_general_data {
	uint8_t client_id;
	uint8_t statistics_counter_id;
	uint8_t statistics_en_flg;
	uint8_t is_fcoe_flg;
	uint8_t activate_flg;
	uint8_t sp_client_id;
	__le16 mtu;
	uint8_t statistics_zero_flg;
	uint8_t func_id;
	uint8_t cos;
	uint8_t traffic_type;
	uint8_t fp_hsi_ver;
	uint8_t reserved0[3];
};


/*
 * client init rx data
 */
struct client_init_rx_data {
	uint8_t tpa_en;
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV4 (0x1 << 0)
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV4_SHIFT 0
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV6 (0x1 << 1)
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV6_SHIFT 1
#define CLIENT_INIT_RX_DATA_TPA_MODE (0x1 << 2)
#define CLIENT_INIT_RX_DATA_TPA_MODE_SHIFT 2
#define CLIENT_INIT_RX_DATA_TPA_OVER_VLAN_DISABLE (0x1 << 3)
#define CLIENT_INIT_RX_DATA_TPA_OVER_VLAN_DISABLE_SHIFT 3
#define CLIENT_INIT_RX_DATA_RESERVED5 (0xF << 4)
#define CLIENT_INIT_RX_DATA_RESERVED5_SHIFT 4
	uint8_t vmqueue_mode_en_flg;
	uint8_t extra_data_over_sgl_en_flg;
	uint8_t cache_line_alignment_log_size;
	uint8_t enable_dynamic_hc;
	uint8_t max_sges_for_packet;
	uint8_t client_qzone_id;
	uint8_t drop_ip_cs_err_flg;
	uint8_t drop_tcp_cs_err_flg;
	uint8_t drop_ttl0_flg;
	uint8_t drop_udp_cs_err_flg;
	uint8_t inner_vlan_removal_enable_flg;
	uint8_t outer_vlan_removal_enable_flg;
	uint8_t status_block_id;
	uint8_t rx_sb_index_number;
	uint8_t dont_verify_rings_pause_thr_flg;
	uint8_t max_tpa_queues;
	uint8_t silent_vlan_removal_flg;
	__le16 max_bytes_on_bd;
	__le16 sge_buff_size;
	uint8_t approx_mcast_engine_id;
	uint8_t rss_engine_id;
	struct regpair bd_page_base;
	struct regpair sge_page_base;
	struct regpair cqe_page_base;
	uint8_t is_leading_rss;
	uint8_t is_approx_mcast;
	__le16 max_agg_size;
	__le16 state;
#define CLIENT_INIT_RX_DATA_UCAST_DROP_ALL (0x1 << 0)
#define CLIENT_INIT_RX_DATA_UCAST_DROP_ALL_SHIFT 0
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_ALL (0x1 << 1)
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_ALL_SHIFT 1
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_UNMATCHED (0x1 << 2)
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_UNMATCHED_SHIFT 2
#define CLIENT_INIT_RX_DATA_MCAST_DROP_ALL (0x1 << 3)
#define CLIENT_INIT_RX_DATA_MCAST_DROP_ALL_SHIFT 3
#define CLIENT_INIT_RX_DATA_MCAST_ACCEPT_ALL (0x1 << 4)
#define CLIENT_INIT_RX_DATA_MCAST_ACCEPT_ALL_SHIFT 4
#define CLIENT_INIT_RX_DATA_BCAST_ACCEPT_ALL (0x1 << 5)
#define CLIENT_INIT_RX_DATA_BCAST_ACCEPT_ALL_SHIFT 5
#define CLIENT_INIT_RX_DATA_ACCEPT_ANY_VLAN (0x1 << 6)
#define CLIENT_INIT_RX_DATA_ACCEPT_ANY_VLAN_SHIFT 6
#define CLIENT_INIT_RX_DATA_RESERVED2 (0x1FF << 7)
#define CLIENT_INIT_RX_DATA_RESERVED2_SHIFT 7
	__le16 cqe_pause_thr_low;
	__le16 cqe_pause_thr_high;
	__le16 bd_pause_thr_low;
	__le16 bd_pause_thr_high;
	__le16 sge_pause_thr_low;
	__le16 sge_pause_thr_high;
	__le16 rx_cos_mask;
	__le16 silent_vlan_value;
	__le16 silent_vlan_mask;
	uint8_t handle_ptp_pkts_flg;
	uint8_t reserved6[3];
	__le32 reserved7;
};

/*
 * client init tx data
 */
struct client_init_tx_data {
	uint8_t enforce_security_flg;
	uint8_t tx_status_block_id;
	uint8_t tx_sb_index_number;
	uint8_t tss_leading_client_id;
	uint8_t tx_switching_flg;
	uint8_t anti_spoofing_flg;
	__le16 default_vlan;
	struct regpair tx_bd_page_base;
	__le16 state;
#define CLIENT_INIT_TX_DATA_UCAST_ACCEPT_ALL (0x1 << 0)
#define CLIENT_INIT_TX_DATA_UCAST_ACCEPT_ALL_SHIFT 0
#define CLIENT_INIT_TX_DATA_MCAST_ACCEPT_ALL (0x1 << 1)
#define CLIENT_INIT_TX_DATA_MCAST_ACCEPT_ALL_SHIFT 1
#define CLIENT_INIT_TX_DATA_BCAST_ACCEPT_ALL (0x1 << 2)
#define CLIENT_INIT_TX_DATA_BCAST_ACCEPT_ALL_SHIFT 2
#define CLIENT_INIT_TX_DATA_ACCEPT_ANY_VLAN (0x1 << 3)
#define CLIENT_INIT_TX_DATA_ACCEPT_ANY_VLAN_SHIFT 3
#define CLIENT_INIT_TX_DATA_RESERVED0 (0xFFF << 4)
#define CLIENT_INIT_TX_DATA_RESERVED0_SHIFT 4
	uint8_t default_vlan_flg;
	uint8_t force_default_pri_flg;
	uint8_t tunnel_lso_inc_ip_id;
	uint8_t refuse_outband_vlan_flg;
	uint8_t tunnel_non_lso_pcsum_location;
	uint8_t tunnel_non_lso_outer_ip_csum_location;
};

/*
 * client init ramrod data
 */
struct client_init_ramrod_data {
	struct client_init_general_data general;
	struct client_init_rx_data rx;
	struct client_init_tx_data tx;
};


/*
 * client update ramrod data
 */
struct client_update_ramrod_data {
	uint8_t client_id;
	uint8_t func_id;
	uint8_t inner_vlan_removal_enable_flg;
	uint8_t inner_vlan_removal_change_flg;
	uint8_t outer_vlan_removal_enable_flg;
	uint8_t outer_vlan_removal_change_flg;
	uint8_t anti_spoofing_enable_flg;
	uint8_t anti_spoofing_change_flg;
	uint8_t activate_flg;
	uint8_t activate_change_flg;
	__le16 default_vlan;
	uint8_t default_vlan_enable_flg;
	uint8_t default_vlan_change_flg;
	__le16 silent_vlan_value;
	__le16 silent_vlan_mask;
	uint8_t silent_vlan_removal_flg;
	uint8_t silent_vlan_change_flg;
	uint8_t refuse_outband_vlan_flg;
	uint8_t refuse_outband_vlan_change_flg;
	uint8_t tx_switching_flg;
	uint8_t tx_switching_change_flg;
	uint8_t handle_ptp_pkts_flg;
	uint8_t handle_ptp_pkts_change_flg;
	__le16 reserved1;
	__le32 echo;
};


/*
 * The eth storm context of Cstorm
 */
struct cstorm_eth_st_context {
	uint32_t __reserved0[4];
};


struct double_regpair {
	uint32_t regpair0_lo;
	uint32_t regpair0_hi;
	uint32_t regpair1_lo;
	uint32_t regpair1_hi;
};


/*
 * 2nd parse bd type used in ethernet tx BDs
 */
enum eth_2nd_parse_bd_type {
	ETH_2ND_PARSE_BD_TYPE_LSO_TUNNEL,
	MAX_ETH_2ND_PARSE_BD_TYPE};


/*
 * Ethernet address typesm used in ethernet tx BDs
 */
enum eth_addr_type {
	UNKNOWN_ADDRESS,
	UNICAST_ADDRESS,
	MULTICAST_ADDRESS,
	BROADCAST_ADDRESS,
	MAX_ETH_ADDR_TYPE};


/*
 *
 */
struct eth_classify_cmd_header {
	uint8_t cmd_general_data;
#define ETH_CLASSIFY_CMD_HEADER_RX_CMD (0x1 << 0)
#define ETH_CLASSIFY_CMD_HEADER_RX_CMD_SHIFT 0
#define ETH_CLASSIFY_CMD_HEADER_TX_CMD (0x1 << 1)
#define ETH_CLASSIFY_CMD_HEADER_TX_CMD_SHIFT 1
#define ETH_CLASSIFY_CMD_HEADER_OPCODE (0x3 << 2)
#define ETH_CLASSIFY_CMD_HEADER_OPCODE_SHIFT 2
#define ETH_CLASSIFY_CMD_HEADER_IS_ADD (0x1 << 4)
#define ETH_CLASSIFY_CMD_HEADER_IS_ADD_SHIFT 4
#define ETH_CLASSIFY_CMD_HEADER_RESERVED0 (0x7 << 5)
#define ETH_CLASSIFY_CMD_HEADER_RESERVED0_SHIFT 5
	uint8_t func_id;
	uint8_t client_id;
	uint8_t reserved1;
};


/*
 * header for eth classification config ramrod
 */
struct eth_classify_header {
	uint8_t rule_cnt;
	uint8_t warning_on_error;
	__le16 reserved1;
	__le32 echo;
};


/*
 * Command for adding/removing a Inner-MAC/VNI classification rule
 */
struct eth_classify_imac_vni_cmd {
	struct eth_classify_cmd_header header;
	__le32 vni;
	__le16 imac_lsb;
	__le16 imac_mid;
	__le16 imac_msb;
	__le16 reserved1;
};


/*
 * Command for adding/removing a MAC classification rule
 */
struct eth_classify_mac_cmd {
	struct eth_classify_cmd_header header;
	__le16 reserved0;
	__le16 inner_mac;
	__le16 mac_lsb;
	__le16 mac_mid;
	__le16 mac_msb;
	__le16 reserved1;
};


/*
 * Command for adding/removing a MAC-VLAN pair classification rule
 */
struct eth_classify_pair_cmd {
	struct eth_classify_cmd_header header;
	__le16 reserved0;
	__le16 inner_mac;
	__le16 mac_lsb;
	__le16 mac_mid;
	__le16 mac_msb;
	__le16 vlan;
};


/*
 * Command for adding/removing a VLAN classification rule
 */
struct eth_classify_vlan_cmd {
	struct eth_classify_cmd_header header;
	__le32 reserved0;
	__le32 reserved1;
	__le16 reserved2;
	__le16 vlan;
};

/*
 * union for eth classification rule
 */
union eth_classify_rule_cmd {
	struct eth_classify_mac_cmd mac;
	struct eth_classify_vlan_cmd vlan;
	struct eth_classify_pair_cmd pair;
	struct eth_classify_imac_vni_cmd imac_vni;
};

/*
 * parameters for eth classification configuration ramrod
 */
struct eth_classify_rules_ramrod_data {
	struct eth_classify_header header;
	union eth_classify_rule_cmd rules[CLASSIFY_RULES_COUNT];
};


/*
 * The data contain client ID need to the ramrod
 */
struct eth_common_ramrod_data {
	__le32 client_id;
	__le32 reserved1;
};


/*
 * The eth storm context of Ustorm
 */
struct ustorm_eth_st_context {
	uint32_t reserved0[52];
};

/*
 * The eth storm context of Tstorm
 */
struct tstorm_eth_st_context {
	uint32_t __reserved0[28];
};

/*
 * The eth storm context of Xstorm
 */
struct xstorm_eth_st_context {
	uint32_t reserved0[60];
};

/*
 * Ethernet connection context
 */
struct eth_context {
	struct ustorm_eth_st_context ustorm_st_context;
	struct tstorm_eth_st_context tstorm_st_context;
	struct xstorm_eth_ag_context xstorm_ag_context;
	struct tstorm_eth_ag_context tstorm_ag_context;
	struct cstorm_eth_ag_context cstorm_ag_context;
	struct ustorm_eth_ag_context ustorm_ag_context;
	struct timers_block_context timers_context;
	struct xstorm_eth_st_context xstorm_st_context;
	struct cstorm_eth_st_context cstorm_st_context;
};


/*
 * union for sgl and raw data.
 */
union eth_sgl_or_raw_data {
	__le16 sgl[8];
	uint32_t raw_data[4];
};

/*
 * eth FP end aggregation CQE parameters struct
 */
struct eth_end_agg_rx_cqe {
	uint8_t type_error_flags;
#define ETH_END_AGG_RX_CQE_TYPE (0x3 << 0)
#define ETH_END_AGG_RX_CQE_TYPE_SHIFT 0
#define ETH_END_AGG_RX_CQE_SGL_RAW_SEL (0x1 << 2)
#define ETH_END_AGG_RX_CQE_SGL_RAW_SEL_SHIFT 2
#define ETH_END_AGG_RX_CQE_RESERVED0 (0x1F << 3)
#define ETH_END_AGG_RX_CQE_RESERVED0_SHIFT 3
	uint8_t reserved1;
	uint8_t queue_index;
	uint8_t reserved2;
	__le32 timestamp_delta;
	__le16 num_of_coalesced_segs;
	__le16 pkt_len;
	uint8_t pure_ack_count;
	uint8_t reserved3;
	__le16 reserved4;
	union eth_sgl_or_raw_data sgl_or_raw_data;
	__le32 padding[8];
};

/*
 * Ethernet error code
 */
enum eth_error_code {
	ETH_OK = 0x00,
	ETH_RAMROD_DATA_READ_ERROR = 0x01,
	ETH_FILTERS_FUNC_NOT_ENABLED,
	ETH_FILTERS_MAC_ADD_FAIL_CAM_FULL,
	ETH_FILTERS_MAC_DEL_FAIL_NOF,
	ETH_FILTERS_PAIR_ADD_FAIL_CAM_FULL,
	ETH_FILTERS_PAIR_DEL_FAIL_NOF,
	ETH_FILTERS_VLAN_ADD_FAIL_CAM_FULL,
	ETH_FILTERS_VLAN_ADD_FAIL_DUP_TT,
	ETH_FILTERS_VLAN_DEL_FAIL_NOF,
	ETH_FILTERS_VLAN_DEL_FAIL_NOF_TT,
	ETH_FILTERS_VLAN_DEL_FAIL_NO_VLAN,
	ETH_FILTERS_IMAC_VNI_ADD_UNALLOWED_IN_TX,
	ETH_FILTERS_IMAC_VNI_DEL_UNALLOWED_IN_TX,
	ETH_FILTERS_IMAC_VNI_ADD_FAIL_CAM_FULL,
	ETH_FILTERS_IMAC_VNI_DEL_FAIL_NOF,
	MAX_ETH_ERROR_CODE};

/*
 * regular eth FP CQE parameters struct
 */
struct eth_fast_path_rx_cqe {
	uint8_t type_error_flags;
#define ETH_FAST_PATH_RX_CQE_TYPE (0x3 << 0)
#define ETH_FAST_PATH_RX_CQE_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL (0x1 << 2)
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL_SHIFT 2
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG (0x1 << 3)
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG (0x1 << 4)
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG (0x1 << 5)
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_PTP_PKT (0x1 << 6)
#define ETH_FAST_PATH_RX_CQE_PTP_PKT_SHIFT 6
#define ETH_FAST_PATH_RX_CQE_RESERVED0 (0x1 << 7)
#define ETH_FAST_PATH_RX_CQE_RESERVED0_SHIFT 7
	uint8_t status_flags;
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE (0x7 << 0)
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG (0x1 << 3)
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG (0x1 << 4)
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG (0x1 << 5)
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG (0x1 << 6)
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG_SHIFT 6
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG (0x1 << 7)
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG_SHIFT 7
	uint8_t queue_index;
	uint8_t placement_offset;
	__le32 rss_hash_result;
	__le16 vlan_tag;
	__le16 pkt_len_or_gro_seg_len;
	__le16 len_on_bd;
	struct parsing_flags pars_flags;
	union eth_sgl_or_raw_data sgl_or_raw_data;
	uint8_t tunn_type;
	uint8_t tunn_inner_hdrs_offset;
	__le16 reserved1;
	__le32 tunn_tenant_id;
	__le32 padding[5];
	__le32 marker;
};


/*
 * Command for setting classification flags for a client
 */
struct eth_filter_rules_cmd {
	uint8_t cmd_general_data;
#define ETH_FILTER_RULES_CMD_RX_CMD (0x1 << 0)
#define ETH_FILTER_RULES_CMD_RX_CMD_SHIFT 0
#define ETH_FILTER_RULES_CMD_TX_CMD (0x1 << 1)
#define ETH_FILTER_RULES_CMD_TX_CMD_SHIFT 1
#define ETH_FILTER_RULES_CMD_RESERVED0 (0x3F << 2)
#define ETH_FILTER_RULES_CMD_RESERVED0_SHIFT 2
	uint8_t func_id;
	uint8_t client_id;
	uint8_t reserved1;
	__le16 state;
#define ETH_FILTER_RULES_CMD_UCAST_DROP_ALL (0x1 << 0)
#define ETH_FILTER_RULES_CMD_UCAST_DROP_ALL_SHIFT 0
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL (0x1 << 1)
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL_SHIFT 1
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED (0x1 << 2)
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED_SHIFT 2
#define ETH_FILTER_RULES_CMD_MCAST_DROP_ALL (0x1 << 3)
#define ETH_FILTER_RULES_CMD_MCAST_DROP_ALL_SHIFT 3
#define ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL (0x1 << 4)
#define ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL_SHIFT 4
#define ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL (0x1 << 5)
#define ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL_SHIFT 5
#define ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN (0x1 << 6)
#define ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN_SHIFT 6
#define ETH_FILTER_RULES_CMD_RESERVED2 (0x1FF << 7)
#define ETH_FILTER_RULES_CMD_RESERVED2_SHIFT 7
	__le16 reserved3;
	struct regpair reserved4;
};


/*
 * parameters for eth classification filters ramrod
 */
struct eth_filter_rules_ramrod_data {
	struct eth_classify_header header;
	struct eth_filter_rules_cmd rules[FILTER_RULES_COUNT];
};


/*
 * Hsi version
 */
enum eth_fp_hsi_ver {
	ETH_FP_HSI_VER_0,
	ETH_FP_HSI_VER_1,
	ETH_FP_HSI_VER_2,
	MAX_ETH_FP_HSI_VER};


/*
 * parameters for eth classification configuration ramrod
 */
struct eth_general_rules_ramrod_data {
	struct eth_classify_header header;
	union eth_classify_rule_cmd rules[CLASSIFY_RULES_COUNT];
};


/*
 * The data for Halt ramrod
 */
struct eth_halt_ramrod_data {
	__le32 client_id;
	__le32 reserved0;
};


/*
 * destination and source mac address.
 */
struct eth_mac_addresses {
#if defined(__BIG_ENDIAN)
	__le16 dst_mid;
	__le16 dst_lo;
#elif defined(__LITTLE_ENDIAN)
	__le16 dst_lo;
	__le16 dst_mid;
#endif
#if defined(__BIG_ENDIAN)
	__le16 src_lo;
	__le16 dst_hi;
#elif defined(__LITTLE_ENDIAN)
	__le16 dst_hi;
	__le16 src_lo;
#endif
#if defined(__BIG_ENDIAN)
	__le16 src_hi;
	__le16 src_mid;
#elif defined(__LITTLE_ENDIAN)
	__le16 src_mid;
	__le16 src_hi;
#endif
};


/*
 * tunneling related data.
 */
struct eth_tunnel_data {
	__le16 dst_lo;
	__le16 dst_mid;
	__le16 dst_hi;
	__le16 fw_ip_hdr_csum;
	__le16 pseudo_csum;
	uint8_t ip_hdr_start_inner_w;
	uint8_t flags;
#define ETH_TUNNEL_DATA_IPV6_OUTER (0x1 << 0)
#define ETH_TUNNEL_DATA_IPV6_OUTER_SHIFT 0
#define ETH_TUNNEL_DATA_RESERVED (0x7F << 1)
#define ETH_TUNNEL_DATA_RESERVED_SHIFT 1
};

/*
 * union for mac addresses and for tunneling data. considered as tunneling data only if (tunnel_exist == 1).
 */
union eth_mac_addr_or_tunnel_data {
	struct eth_mac_addresses mac_addr;
	struct eth_tunnel_data tunnel_data;
};


/*
 * Command for setting multicast classification for a client
 */
struct eth_multicast_rules_cmd {
	uint8_t cmd_general_data;
#define ETH_MULTICAST_RULES_CMD_RX_CMD (0x1 << 0)
#define ETH_MULTICAST_RULES_CMD_RX_CMD_SHIFT 0
#define ETH_MULTICAST_RULES_CMD_TX_CMD (0x1 << 1)
#define ETH_MULTICAST_RULES_CMD_TX_CMD_SHIFT 1
#define ETH_MULTICAST_RULES_CMD_IS_ADD (0x1 << 2)
#define ETH_MULTICAST_RULES_CMD_IS_ADD_SHIFT 2
#define ETH_MULTICAST_RULES_CMD_RESERVED0 (0x1F << 3)
#define ETH_MULTICAST_RULES_CMD_RESERVED0_SHIFT 3
	uint8_t func_id;
	uint8_t bin_id;
	uint8_t engine_id;
	__le32 reserved2;
	struct regpair reserved3;
};


/*
 * parameters for multicast classification ramrod
 */
struct eth_multicast_rules_ramrod_data {
	struct eth_classify_header header;
	struct eth_multicast_rules_cmd rules[MULTICAST_RULES_COUNT];
};


/*
 * Place holder for ramrods protocol specific data
 */
struct ramrod_data {
	__le32 data_lo;
	__le32 data_hi;
};

/*
 * union for ramrod data for Ethernet protocol (CQE) (force size of 16 bits)
 */
union eth_ramrod_data {
	struct ramrod_data general;
};


/*
 * RSS toeplitz hash type, as reported in CQE
 */
enum eth_rss_hash_type {
	DEFAULT_HASH_TYPE,
	IPV4_HASH_TYPE,
	TCP_IPV4_HASH_TYPE,
	IPV6_HASH_TYPE,
	TCP_IPV6_HASH_TYPE,
	VLAN_PRI_HASH_TYPE,
	E1HOV_PRI_HASH_TYPE,
	DSCP_HASH_TYPE,
	MAX_ETH_RSS_HASH_TYPE};


/*
 * Ethernet RSS mode
 */
enum eth_rss_mode {
	ETH_RSS_MODE_DISABLED,
	ETH_RSS_MODE_REGULAR,
	ETH_RSS_MODE_ESX51,
	ETH_RSS_MODE_VLAN_PRI,
	ETH_RSS_MODE_E1HOV_PRI,
	ETH_RSS_MODE_IP_DSCP,
	MAX_ETH_RSS_MODE};


/*
 * parameters for RSS update ramrod (E2)
 */
struct eth_rss_update_ramrod_data {
	uint8_t rss_engine_id;
	uint8_t rss_mode;
	__le16 capabilities;
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_CAPABILITY (0x1 << 0)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_CAPABILITY_SHIFT 0
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_TCP_CAPABILITY (0x1 << 1)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_TCP_CAPABILITY_SHIFT 1
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_UDP_CAPABILITY (0x1 << 2)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_UDP_CAPABILITY_SHIFT 2
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_VXLAN_CAPABILITY (0x1 << 3)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_VXLAN_CAPABILITY_SHIFT 3
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_CAPABILITY (0x1 << 4)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_CAPABILITY_SHIFT 4
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_TCP_CAPABILITY (0x1 << 5)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_TCP_CAPABILITY_SHIFT 5
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_UDP_CAPABILITY (0x1 << 6)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_UDP_CAPABILITY_SHIFT 6
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_VXLAN_CAPABILITY (0x1 << 7)
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_VXLAN_CAPABILITY_SHIFT 7
#define ETH_RSS_UPDATE_RAMROD_DATA_TUNN_INNER_HDRS_CAPABILITY (0x1 << 8)
#define ETH_RSS_UPDATE_RAMROD_DATA_TUNN_INNER_HDRS_CAPABILITY_SHIFT 8
#define ETH_RSS_UPDATE_RAMROD_DATA_UPDATE_RSS_KEY (0x1 << 9)
#define ETH_RSS_UPDATE_RAMROD_DATA_UPDATE_RSS_KEY_SHIFT 9
#define ETH_RSS_UPDATE_RAMROD_DATA_RESERVED (0x3F << 10)
#define ETH_RSS_UPDATE_RAMROD_DATA_RESERVED_SHIFT 10
	uint8_t rss_result_mask;
	uint8_t reserved3;
	__le16 reserved4;
	uint8_t indirection_table[T_ETH_INDIRECTION_TABLE_SIZE];
	__le32 rss_key[T_ETH_RSS_KEY];
	__le32 echo;
	__le32 reserved5;
};


/*
 * The eth Rx Buffer Descriptor
 */
struct eth_rx_bd {
	__le32 addr_lo;
	__le32 addr_hi;
};


/*
 * Eth Rx Cqe structure- general structure for ramrods
 */
struct common_ramrod_eth_rx_cqe {
	uint8_t ramrod_type;
#define COMMON_RAMROD_ETH_RX_CQE_TYPE (0x3 << 0)
#define COMMON_RAMROD_ETH_RX_CQE_TYPE_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_ERROR (0x1 << 2)
#define COMMON_RAMROD_ETH_RX_CQE_ERROR_SHIFT 2
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0 (0x1F << 3)
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0_SHIFT 3
	uint8_t conn_type;
	__le16 reserved1;
	__le32 conn_and_cmd_data;
#define COMMON_RAMROD_ETH_RX_CQE_CID (0xFFFFFF << 0)
#define COMMON_RAMROD_ETH_RX_CQE_CID_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID (0xFF << 24)
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT 24
	struct ramrod_data protocol_data;
	__le32 echo;
	__le32 reserved2[11];
};

/*
 * Rx Last CQE in page (in ETH)
 */
struct eth_rx_cqe_next_page {
	__le32 addr_lo;
	__le32 addr_hi;
	__le32 reserved[14];
};

/*
 * union for all eth rx cqe types (fix their sizes)
 */
union eth_rx_cqe {
	struct eth_fast_path_rx_cqe fast_path_cqe;
	struct common_ramrod_eth_rx_cqe ramrod_cqe;
	struct eth_rx_cqe_next_page next_page_cqe;
	struct eth_end_agg_rx_cqe end_agg_cqe;
};


/*
 * Values for RX ETH CQE type field
 */
enum eth_rx_cqe_type {
	RX_ETH_CQE_TYPE_ETH_FASTPATH,
	RX_ETH_CQE_TYPE_ETH_RAMROD,
	RX_ETH_CQE_TYPE_ETH_START_AGG,
	RX_ETH_CQE_TYPE_ETH_STOP_AGG,
	MAX_ETH_RX_CQE_TYPE};


/*
 * Type of SGL/Raw field in ETH RX fast path CQE
 */
enum eth_rx_fp_sel {
	ETH_FP_CQE_REGULAR,
	ETH_FP_CQE_RAW,
	MAX_ETH_RX_FP_SEL};


/*
 * The eth Rx SGE Descriptor
 */
struct eth_rx_sge {
	__le32 addr_lo;
	__le32 addr_hi;
};


/*
 * common data for all protocols
 */
struct spe_hdr {
	__le32 conn_and_cmd_data;
#define SPE_HDR_CID (0xFFFFFF << 0)
#define SPE_HDR_CID_SHIFT 0
#define SPE_HDR_CMD_ID (0xFF << 24)
#define SPE_HDR_CMD_ID_SHIFT 24
	__le16 type;
#define SPE_HDR_CONN_TYPE (0xFF << 0)
#define SPE_HDR_CONN_TYPE_SHIFT 0
#define SPE_HDR_FUNCTION_ID (0xFF << 8)
#define SPE_HDR_FUNCTION_ID_SHIFT 8
	__le16 reserved1;
};

/*
 * specific data for ethernet slow path element
 */
union eth_specific_data {
	uint8_t protocol_data[8];
	struct regpair client_update_ramrod_data;
	struct regpair client_init_ramrod_init_data;
	struct eth_halt_ramrod_data halt_ramrod_data;
	struct regpair update_data_addr;
	struct eth_common_ramrod_data common_ramrod_data;
	struct regpair classify_cfg_addr;
	struct regpair filter_cfg_addr;
	struct regpair mcast_cfg_addr;
};

/*
 * Ethernet slow path element
 */
struct eth_spe {
	struct spe_hdr hdr;
	union eth_specific_data data;
};


/*
 * Ethernet command ID for slow path elements
 */
enum eth_spqe_cmd_id {
	RAMROD_CMD_ID_ETH_UNUSED,
	RAMROD_CMD_ID_ETH_CLIENT_SETUP,
	RAMROD_CMD_ID_ETH_HALT,
	RAMROD_CMD_ID_ETH_FORWARD_SETUP,
	RAMROD_CMD_ID_ETH_TX_QUEUE_SETUP,
	RAMROD_CMD_ID_ETH_CLIENT_UPDATE,
	RAMROD_CMD_ID_ETH_EMPTY,
	RAMROD_CMD_ID_ETH_TERMINATE,
	RAMROD_CMD_ID_ETH_TPA_UPDATE,
	RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES,
	RAMROD_CMD_ID_ETH_FILTER_RULES,
	RAMROD_CMD_ID_ETH_MULTICAST_RULES,
	RAMROD_CMD_ID_ETH_RSS_UPDATE,
	RAMROD_CMD_ID_ETH_SET_MAC,
	MAX_ETH_SPQE_CMD_ID};


/*
 * eth tpa update command
 */
enum eth_tpa_update_command {
	TPA_UPDATE_NONE_COMMAND,
	TPA_UPDATE_ENABLE_COMMAND,
	TPA_UPDATE_DISABLE_COMMAND,
	MAX_ETH_TPA_UPDATE_COMMAND};


/*
 * In case of LSO over IPv4 tunnel, whether to increment IP ID on external IP header or internal IP header
 */
enum eth_tunnel_lso_inc_ip_id {
	EXT_HEADER,
	INT_HEADER,
	MAX_ETH_TUNNEL_LSO_INC_IP_ID};


/*
 * In case tunnel exist and L4 checksum offload (or outer ip header checksum), the pseudo checksum location, on packet or on BD.
 */
enum eth_tunnel_non_lso_csum_location {
	CSUM_ON_PKT,
	CSUM_ON_BD,
	MAX_ETH_TUNNEL_NON_LSO_CSUM_LOCATION};


/*
 * Packet Tunneling Type
 */
enum eth_tunn_type {
	TUNN_TYPE_NONE,
	TUNN_TYPE_VXLAN,
	TUNN_TYPE_L2_GRE,
	TUNN_TYPE_IPV4_GRE,
	TUNN_TYPE_IPV6_GRE,
	TUNN_TYPE_L2_GENEVE,
	TUNN_TYPE_IPV4_GENEVE,
	TUNN_TYPE_IPV6_GENEVE,
	MAX_ETH_TUNN_TYPE};


/*
 * Tx regular BD structure
 */
struct eth_tx_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	__le16 total_pkt_bytes;
	__le16 nbytes;
	uint8_t reserved[4];
};


/*
 * structure for easy accessibility to assembler
 */
struct eth_tx_bd_flags {
	uint8_t as_bitfield;
#define ETH_TX_BD_FLAGS_IP_CSUM (0x1 << 0)
#define ETH_TX_BD_FLAGS_IP_CSUM_SHIFT 0
#define ETH_TX_BD_FLAGS_L4_CSUM (0x1 << 1)
#define ETH_TX_BD_FLAGS_L4_CSUM_SHIFT 1
#define ETH_TX_BD_FLAGS_VLAN_MODE (0x3 << 2)
#define ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT 2
#define ETH_TX_BD_FLAGS_START_BD (0x1 << 4)
#define ETH_TX_BD_FLAGS_START_BD_SHIFT 4
#define ETH_TX_BD_FLAGS_IS_UDP (0x1 << 5)
#define ETH_TX_BD_FLAGS_IS_UDP_SHIFT 5
#define ETH_TX_BD_FLAGS_SW_LSO (0x1 << 6)
#define ETH_TX_BD_FLAGS_SW_LSO_SHIFT 6
#define ETH_TX_BD_FLAGS_IPV6 (0x1 << 7)
#define ETH_TX_BD_FLAGS_IPV6_SHIFT 7
};

/*
 * The eth Tx Buffer Descriptor
 */
struct eth_tx_start_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	__le16 nbd;
	__le16 nbytes;
	__le16 vlan_or_ethertype;
	struct eth_tx_bd_flags bd_flags;
	uint8_t general_data;
#define ETH_TX_START_BD_HDR_NBDS (0x7 << 0)
#define ETH_TX_START_BD_HDR_NBDS_SHIFT 0
#define ETH_TX_START_BD_NO_ADDED_TAGS (0x1 << 3)
#define ETH_TX_START_BD_NO_ADDED_TAGS_SHIFT 3
#define ETH_TX_START_BD_FORCE_VLAN_MODE (0x1 << 4)
#define ETH_TX_START_BD_FORCE_VLAN_MODE_SHIFT 4
#define ETH_TX_START_BD_PARSE_NBDS (0x3 << 5)
#define ETH_TX_START_BD_PARSE_NBDS_SHIFT 5
#define ETH_TX_START_BD_TUNNEL_EXIST (0x1 << 7)
#define ETH_TX_START_BD_TUNNEL_EXIST_SHIFT 7
};

/*
 * Tx parsing BD structure for ETH E1/E1h
 */
struct eth_tx_parse_bd_e1x {
	__le16 global_data;
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W (0xF << 0)
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_ETH_ADDR_TYPE (0x3 << 4)
#define ETH_TX_PARSE_BD_E1X_ETH_ADDR_TYPE_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN (0x1 << 6)
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN (0x1 << 7)
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN_SHIFT 7
#define ETH_TX_PARSE_BD_E1X_NS_FLG (0x1 << 8)
#define ETH_TX_PARSE_BD_E1X_NS_FLG_SHIFT 8
#define ETH_TX_PARSE_BD_E1X_RESERVED0 (0x7F << 9)
#define ETH_TX_PARSE_BD_E1X_RESERVED0_SHIFT 9
	uint8_t tcp_flags;
#define ETH_TX_PARSE_BD_E1X_FIN_FLG (0x1 << 0)
#define ETH_TX_PARSE_BD_E1X_FIN_FLG_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_SYN_FLG (0x1 << 1)
#define ETH_TX_PARSE_BD_E1X_SYN_FLG_SHIFT 1
#define ETH_TX_PARSE_BD_E1X_RST_FLG (0x1 << 2)
#define ETH_TX_PARSE_BD_E1X_RST_FLG_SHIFT 2
#define ETH_TX_PARSE_BD_E1X_PSH_FLG (0x1 << 3)
#define ETH_TX_PARSE_BD_E1X_PSH_FLG_SHIFT 3
#define ETH_TX_PARSE_BD_E1X_ACK_FLG (0x1 << 4)
#define ETH_TX_PARSE_BD_E1X_ACK_FLG_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_URG_FLG (0x1 << 5)
#define ETH_TX_PARSE_BD_E1X_URG_FLG_SHIFT 5
#define ETH_TX_PARSE_BD_E1X_ECE_FLG (0x1 << 6)
#define ETH_TX_PARSE_BD_E1X_ECE_FLG_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_CWR_FLG (0x1 << 7)
#define ETH_TX_PARSE_BD_E1X_CWR_FLG_SHIFT 7
	uint8_t ip_hlen_w;
	__le16 total_hlen_w;
	__le16 tcp_pseudo_csum;
	__le16 lso_mss;
	__le16 ip_id;
	__le32 tcp_send_seq;
};

/*
 * Tx parsing BD structure for ETH E2
 */
struct eth_tx_parse_bd_e2 {
	union eth_mac_addr_or_tunnel_data data;
	__le32 parsing_data;
#define ETH_TX_PARSE_BD_E2_L4_HDR_START_OFFSET_W (0x7FF << 0)
#define ETH_TX_PARSE_BD_E2_L4_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW (0xF << 11)
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW_SHIFT 11
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR (0x1 << 15)
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR_SHIFT 15
#define ETH_TX_PARSE_BD_E2_LSO_MSS (0x3FFF << 16)
#define ETH_TX_PARSE_BD_E2_LSO_MSS_SHIFT 16
#define ETH_TX_PARSE_BD_E2_ETH_ADDR_TYPE (0x3 << 30)
#define ETH_TX_PARSE_BD_E2_ETH_ADDR_TYPE_SHIFT 30
};

/*
 * Tx 2nd parsing BD structure for ETH packet
 */
struct eth_tx_parse_2nd_bd {
	__le16 global_data;
#define ETH_TX_PARSE_2ND_BD_IP_HDR_START_OUTER_W (0xF << 0)
#define ETH_TX_PARSE_2ND_BD_IP_HDR_START_OUTER_W_SHIFT 0
#define ETH_TX_PARSE_2ND_BD_RESERVED0 (0x1 << 4)
#define ETH_TX_PARSE_2ND_BD_RESERVED0_SHIFT 4
#define ETH_TX_PARSE_2ND_BD_LLC_SNAP_EN (0x1 << 5)
#define ETH_TX_PARSE_2ND_BD_LLC_SNAP_EN_SHIFT 5
#define ETH_TX_PARSE_2ND_BD_NS_FLG (0x1 << 6)
#define ETH_TX_PARSE_2ND_BD_NS_FLG_SHIFT 6
#define ETH_TX_PARSE_2ND_BD_TUNNEL_UDP_EXIST (0x1 << 7)
#define ETH_TX_PARSE_2ND_BD_TUNNEL_UDP_EXIST_SHIFT 7
#define ETH_TX_PARSE_2ND_BD_IP_HDR_LEN_OUTER_W (0x1F << 8)
#define ETH_TX_PARSE_2ND_BD_IP_HDR_LEN_OUTER_W_SHIFT 8
#define ETH_TX_PARSE_2ND_BD_RESERVED1 (0x7 << 13)
#define ETH_TX_PARSE_2ND_BD_RESERVED1_SHIFT 13
	uint8_t bd_type;
#define ETH_TX_PARSE_2ND_BD_TYPE (0xF << 0)
#define ETH_TX_PARSE_2ND_BD_TYPE_SHIFT 0
#define ETH_TX_PARSE_2ND_BD_RESERVED2 (0xF << 4)
#define ETH_TX_PARSE_2ND_BD_RESERVED2_SHIFT 4
	uint8_t reserved3;
	uint8_t tcp_flags;
#define ETH_TX_PARSE_2ND_BD_FIN_FLG (0x1 << 0)
#define ETH_TX_PARSE_2ND_BD_FIN_FLG_SHIFT 0
#define ETH_TX_PARSE_2ND_BD_SYN_FLG (0x1 << 1)
#define ETH_TX_PARSE_2ND_BD_SYN_FLG_SHIFT 1
#define ETH_TX_PARSE_2ND_BD_RST_FLG (0x1 << 2)
#define ETH_TX_PARSE_2ND_BD_RST_FLG_SHIFT 2
#define ETH_TX_PARSE_2ND_BD_PSH_FLG (0x1 << 3)
#define ETH_TX_PARSE_2ND_BD_PSH_FLG_SHIFT 3
#define ETH_TX_PARSE_2ND_BD_ACK_FLG (0x1 << 4)
#define ETH_TX_PARSE_2ND_BD_ACK_FLG_SHIFT 4
#define ETH_TX_PARSE_2ND_BD_URG_FLG (0x1 << 5)
#define ETH_TX_PARSE_2ND_BD_URG_FLG_SHIFT 5
#define ETH_TX_PARSE_2ND_BD_ECE_FLG (0x1 << 6)
#define ETH_TX_PARSE_2ND_BD_ECE_FLG_SHIFT 6
#define ETH_TX_PARSE_2ND_BD_CWR_FLG (0x1 << 7)
#define ETH_TX_PARSE_2ND_BD_CWR_FLG_SHIFT 7
	uint8_t reserved4;
	uint8_t tunnel_udp_hdr_start_w;
	uint8_t fw_ip_hdr_to_payload_w;
	__le16 fw_ip_csum_wo_len_flags_frag;
	__le16 hw_ip_id;
	__le32 tcp_send_seq;
};

/*
 * The last BD in the BD memory will hold a pointer to the next BD memory
 */
struct eth_tx_next_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	uint8_t reserved[8];
};

/*
 * union for 4 Bd types
 */
union eth_tx_bd_types {
	struct eth_tx_start_bd start_bd;
	struct eth_tx_bd reg_bd;
	struct eth_tx_parse_bd_e1x parse_bd_e1x;
	struct eth_tx_parse_bd_e2 parse_bd_e2;
	struct eth_tx_parse_2nd_bd parse_2nd_bd;
	struct eth_tx_next_bd next_bd;
};

/*
 * array of 13 bds as appears in the eth xstorm context
 */
struct eth_tx_bds_array {
	union eth_tx_bd_types bds[13];
};


/*
 * VLAN mode on TX BDs
 */
enum eth_tx_vlan_type {
	X_ETH_NO_VLAN,
	X_ETH_OUTBAND_VLAN,
	X_ETH_INBAND_VLAN,
	X_ETH_FW_ADDED_VLAN,
	MAX_ETH_TX_VLAN_TYPE};


/*
 * Ethernet VLAN filtering mode in E1x
 */
enum eth_vlan_filter_mode {
	ETH_VLAN_FILTER_ANY_VLAN,
	ETH_VLAN_FILTER_SPECIFIC_VLAN,
	ETH_VLAN_FILTER_CLASSIFY,
	MAX_ETH_VLAN_FILTER_MODE};


/*
 * MAC filtering configuration command header
 */
struct mac_configuration_hdr {
	uint8_t length;
	uint8_t offset;
	__le16 client_id;
	__le32 echo;
};

/*
 * MAC address in list for ramrod
 */
struct mac_configuration_entry {
	__le16 lsb_mac_addr;
	__le16 middle_mac_addr;
	__le16 msb_mac_addr;
	__le16 vlan_id;
	uint8_t pf_id;
	uint8_t flags;
#define MAC_CONFIGURATION_ENTRY_ACTION_TYPE (0x1 << 0)
#define MAC_CONFIGURATION_ENTRY_ACTION_TYPE_SHIFT 0
#define MAC_CONFIGURATION_ENTRY_RDMA_MAC (0x1 << 1)
#define MAC_CONFIGURATION_ENTRY_RDMA_MAC_SHIFT 1
#define MAC_CONFIGURATION_ENTRY_VLAN_FILTERING_MODE (0x3 << 2)
#define MAC_CONFIGURATION_ENTRY_VLAN_FILTERING_MODE_SHIFT 2
#define MAC_CONFIGURATION_ENTRY_OVERRIDE_VLAN_REMOVAL (0x1 << 4)
#define MAC_CONFIGURATION_ENTRY_OVERRIDE_VLAN_REMOVAL_SHIFT 4
#define MAC_CONFIGURATION_ENTRY_BROADCAST (0x1 << 5)
#define MAC_CONFIGURATION_ENTRY_BROADCAST_SHIFT 5
#define MAC_CONFIGURATION_ENTRY_RESERVED1 (0x3 << 6)
#define MAC_CONFIGURATION_ENTRY_RESERVED1_SHIFT 6
	__le16 reserved0;
	__le32 clients_bit_vector;
};

/*
 * MAC filtering configuration command
 */
struct mac_configuration_cmd {
	struct mac_configuration_hdr hdr;
	struct mac_configuration_entry config_table[64];
};


/*
 * Set-MAC command type (in E1x)
 */
enum set_mac_action_type {
	T_ETH_MAC_COMMAND_INVALIDATE,
	T_ETH_MAC_COMMAND_SET,
	MAX_SET_MAC_ACTION_TYPE};


/*
 * Ethernet TPA Modes
 */
enum tpa_mode {
	TPA_LRO,
	TPA_GRO,
	MAX_TPA_MODE};


/*
 * tpa update ramrod data
 */
struct tpa_update_ramrod_data {
	uint8_t update_ipv4;
	uint8_t update_ipv6;
	uint8_t client_id;
	uint8_t max_tpa_queues;
	uint8_t max_sges_for_packet;
	uint8_t complete_on_both_clients;
	uint8_t dont_verify_rings_pause_thr_flg;
	uint8_t tpa_mode;
	__le16 sge_buff_size;
	__le16 max_agg_size;
	__le32 sge_page_base_lo;
	__le32 sge_page_base_hi;
	__le16 sge_pause_thr_low;
	__le16 sge_pause_thr_high;
	uint8_t tpa_over_vlan_disable;
	uint8_t reserved[7];
};


/*
 * approximate-match multicast filtering for E1H per function in Tstorm
 */
struct tstorm_eth_approximate_match_multicast_filtering {
	uint32_t mcast_add_hash_bit_array[8];
};


/*
 * Common configuration parameters per function in Tstorm
 */
struct tstorm_eth_function_common_config {
	__le16 config_flags;
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY (0x1 << 0)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY_SHIFT 0
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY (0x1 << 1)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY_SHIFT 1
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY (0x1 << 2)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY_SHIFT 2
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY (0x1 << 3)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY_SHIFT 3
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE (0x7 << 4)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE_SHIFT 4
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_VLAN_FILTERING_ENABLE (0x1 << 7)
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_VLAN_FILTERING_ENABLE_SHIFT 7
#define __TSTORM_ETH_FUNCTION_COMMON_CONFIG_RESERVED0 (0xFF << 8)
#define __TSTORM_ETH_FUNCTION_COMMON_CONFIG_RESERVED0_SHIFT 8
	uint8_t rss_result_mask;
	uint8_t reserved1;
	__le16 vlan_id[2];
};


/*
 * MAC filtering configuration parameters per port in Tstorm
 */
struct tstorm_eth_mac_filter_config {
	uint32_t ucast_drop_all;
	uint32_t ucast_accept_all;
	uint32_t mcast_drop_all;
	uint32_t mcast_accept_all;
	uint32_t bcast_accept_all;
	uint32_t vlan_filter[2];
	uint32_t unmatched_unicast;
};


/*
 * tx only queue init ramrod data
 */
struct tx_queue_init_ramrod_data {
	struct client_init_general_data general;
	struct client_init_tx_data tx;
};


/*
 * Three RX producers for ETH
 */
struct ustorm_eth_rx_producers {
#if defined(__BIG_ENDIAN)
	uint16_t bd_prod;
	uint16_t cqe_prod;
#elif defined(__LITTLE_ENDIAN)
	uint16_t cqe_prod;
	uint16_t bd_prod;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved;
	uint16_t sge_prod;
#elif defined(__LITTLE_ENDIAN)
	uint16_t sge_prod;
	uint16_t reserved;
#endif
};


/*
 * FCoE RX statistics parameters section#0
 */
struct fcoe_rx_stat_params_section0 {
	__le32 fcoe_rx_pkt_cnt;
	__le32 fcoe_rx_byte_cnt;
};


/*
 * FCoE RX statistics parameters section#1
 */
struct fcoe_rx_stat_params_section1 {
	__le32 fcoe_ver_cnt;
	__le32 fcoe_rx_drop_pkt_cnt;
};


/*
 * FCoE RX statistics parameters section#2
 */
struct fcoe_rx_stat_params_section2 {
	__le32 fc_crc_cnt;
	__le32 eofa_del_cnt;
	__le32 miss_frame_cnt;
	__le32 seq_timeout_cnt;
	__le32 drop_seq_cnt;
	__le32 fcoe_rx_drop_pkt_cnt;
	__le32 fcp_rx_pkt_cnt;
	__le32 reserved0;
};


/*
 * FCoE TX statistics parameters
 */
struct fcoe_tx_stat_params {
	__le32 fcoe_tx_pkt_cnt;
	__le32 fcoe_tx_byte_cnt;
	__le32 fcp_tx_pkt_cnt;
	__le32 reserved0;
};

/*
 * FCoE statistics parameters
 */
struct fcoe_statistics_params {
	struct fcoe_tx_stat_params tx_stat;
	struct fcoe_rx_stat_params_section0 rx_stat0;
	struct fcoe_rx_stat_params_section1 rx_stat1;
	struct fcoe_rx_stat_params_section2 rx_stat2;
};


/*
 * The data afex vif list ramrod need
 */
struct afex_vif_list_ramrod_data {
	uint8_t afex_vif_list_command;
	uint8_t func_bit_map;
	__le16 vif_list_index;
	uint8_t func_to_clear;
	uint8_t echo;
	__le16 reserved1;
};


/*
 *
 */
struct c2s_pri_trans_table_entry {
	uint8_t val[MAX_VLAN_PRIORITIES];
};


/*
 * cfc delete event data
 */
struct cfc_del_event_data {
	__le32 cid;
	__le32 reserved0;
	__le32 reserved1;
};


/*
 * per-port SAFC demo variables
 */
struct cmng_flags_per_port {
	uint32_t cmng_enables;
#define CMNG_FLAGS_PER_PORT_FAIRNESS_VN (0x1 << 0)
#define CMNG_FLAGS_PER_PORT_FAIRNESS_VN_SHIFT 0
#define CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN (0x1 << 1)
#define CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN_SHIFT 1
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS (0x1 << 2)
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_SHIFT 2
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_MODE (0x1 << 3)
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_MODE_SHIFT 3
#define __CMNG_FLAGS_PER_PORT_RESERVED0 (0xFFFFFFF << 4)
#define __CMNG_FLAGS_PER_PORT_RESERVED0_SHIFT 4
	uint32_t __reserved1;
};


/*
 * per-port rate shaping variables
 */
struct rate_shaping_vars_per_port {
	uint32_t rs_periodic_timeout;
	uint32_t rs_threshold;
};

/*
 * per-port fairness variables
 */
struct fairness_vars_per_port {
	uint32_t upper_bound;
	uint32_t fair_threshold;
	uint32_t fairness_timeout;
	uint32_t size_thr;
};

/*
 * per-port SAFC variables
 */
struct safc_struct_per_port {
#if defined(__BIG_ENDIAN)
	uint16_t __reserved1;
	uint8_t __reserved0;
	uint8_t safc_timeout_usec;
#elif defined(__LITTLE_ENDIAN)
	uint8_t safc_timeout_usec;
	uint8_t __reserved0;
	uint16_t __reserved1;
#endif
	uint8_t cos_to_traffic_types[MAX_COS_NUMBER];
	uint16_t cos_to_pause_mask[NUM_OF_SAFC_BITS];
};

/*
 * Per-port congestion management variables
 */
struct cmng_struct_per_port {
	struct rate_shaping_vars_per_port rs_vars;
	struct fairness_vars_per_port fair_vars;
	struct safc_struct_per_port safc_vars;
	struct cmng_flags_per_port flags;
};

/*
 * a single rate shaping counter. can be used as protocol or vnic counter
 */
struct rate_shaping_counter {
	uint32_t quota;
#if defined(__BIG_ENDIAN)
	uint16_t __reserved0;
	uint16_t rate;
#elif defined(__LITTLE_ENDIAN)
	uint16_t rate;
	uint16_t __reserved0;
#endif
};

/*
 * per-vnic rate shaping variables
 */
struct rate_shaping_vars_per_vn {
	struct rate_shaping_counter vn_counter;
};

/*
 * per-vnic fairness variables
 */
struct fairness_vars_per_vn {
	uint32_t cos_credit_delta[MAX_COS_NUMBER];
	uint32_t vn_credit_delta;
	uint32_t __reserved0;
};

/*
 * cmng port init state
 */
struct cmng_vnic {
	struct rate_shaping_vars_per_vn vnic_max_rate[4];
	struct fairness_vars_per_vn vnic_min_rate[4];
};

/*
 * cmng port init state
 */
struct cmng_init {
	struct cmng_struct_per_port port;
	struct cmng_vnic vnic;
};


/*
 * driver parameters for congestion management init, all rates are in Mbps
 */
struct cmng_init_input {
	uint32_t port_rate;
	uint32_t size_thr;
	uint32_t fairness_thr;
	uint16_t vnic_min_rate[4];
	uint16_t vnic_max_rate[4];
	uint16_t cos_min_rate[MAX_COS_NUMBER];
	uint16_t cos_to_pause_mask[MAX_COS_NUMBER];
	struct cmng_flags_per_port flags;
};


/*
 * Protocol-common command ID for slow path elements
 */
enum common_spqe_cmd_id {
	RAMROD_CMD_ID_COMMON_UNUSED,
	RAMROD_CMD_ID_COMMON_FUNCTION_START,
	RAMROD_CMD_ID_COMMON_FUNCTION_STOP,
	RAMROD_CMD_ID_COMMON_FUNCTION_UPDATE,
	RAMROD_CMD_ID_COMMON_CFC_DEL,
	RAMROD_CMD_ID_COMMON_CFC_DEL_WB,
	RAMROD_CMD_ID_COMMON_STAT_QUERY,
	RAMROD_CMD_ID_COMMON_STOP_TRAFFIC,
	RAMROD_CMD_ID_COMMON_START_TRAFFIC,
	RAMROD_CMD_ID_COMMON_AFEX_VIF_LISTS,
	RAMROD_CMD_ID_COMMON_SET_TIMESYNC,
	MAX_COMMON_SPQE_CMD_ID};


/*
 * Per-protocol connection types
 */
enum connection_type {
	ETH_CONNECTION_TYPE,
	TOE_CONNECTION_TYPE,
	RDMA_CONNECTION_TYPE,
	ISCSI_CONNECTION_TYPE,
	FCOE_CONNECTION_TYPE,
	RESERVED_CONNECTION_TYPE_0,
	RESERVED_CONNECTION_TYPE_1,
	RESERVED_CONNECTION_TYPE_2,
	NONE_CONNECTION_TYPE,
	MAX_CONNECTION_TYPE};


/*
 * Cos modes
 */
enum cos_mode {
	OVERRIDE_COS,
	STATIC_COS,
	FW_WRR,
	MAX_COS_MODE};


/*
 * Dynamic HC counters set by the driver
 */
struct hc_dynamic_drv_counter {
	uint32_t val[HC_SB_MAX_DYNAMIC_INDICES];
};

/*
 * zone A per-queue data
 */
struct cstorm_queue_zone_data {
	struct hc_dynamic_drv_counter hc_dyn_drv_cnt;
	struct regpair reserved[2];
};


/*
 * Vf-PF channel data in cstorm ram (non-triggered zone)
 */
struct vf_pf_channel_zone_data {
	uint32_t msg_addr_lo;
	uint32_t msg_addr_hi;
};

/*
 * zone for VF non-triggered data
 */
struct non_trigger_vf_zone {
	struct vf_pf_channel_zone_data vf_pf_channel;
};

/*
 * Vf-PF channel trigger zone in cstorm ram
 */
struct vf_pf_channel_zone_trigger {
	uint8_t addr_valid;
};

/*
 * zone that triggers the in-bound interrupt
 */
struct trigger_vf_zone {
	struct vf_pf_channel_zone_trigger vf_pf_channel;
	uint8_t reserved0;
	uint16_t reserved1;
	uint32_t reserved2;
};

/*
 * zone B per-VF data
 */
struct cstorm_vf_zone_data {
	struct non_trigger_vf_zone non_trigger;
	struct trigger_vf_zone trigger;
};


/*
 * Dynamic host coalescing init parameters, per state machine
 */
struct dynamic_hc_sm_config {
	uint32_t threshold[3];
	uint8_t shift_per_protocol[HC_SB_MAX_DYNAMIC_INDICES];
	uint8_t hc_timeout0[HC_SB_MAX_DYNAMIC_INDICES];
	uint8_t hc_timeout1[HC_SB_MAX_DYNAMIC_INDICES];
	uint8_t hc_timeout2[HC_SB_MAX_DYNAMIC_INDICES];
	uint8_t hc_timeout3[HC_SB_MAX_DYNAMIC_INDICES];
};

/*
 * Dynamic host coalescing init parameters
 */
struct dynamic_hc_config {
	struct dynamic_hc_sm_config sm_config[HC_SB_MAX_SM];
};


struct e2_integ_data {
#if defined(__BIG_ENDIAN)
	uint8_t flags;
#define E2_INTEG_DATA_TESTING_EN (0x1 << 0)
#define E2_INTEG_DATA_TESTING_EN_SHIFT 0
#define E2_INTEG_DATA_LB_TX (0x1 << 1)
#define E2_INTEG_DATA_LB_TX_SHIFT 1
#define E2_INTEG_DATA_COS_TX (0x1 << 2)
#define E2_INTEG_DATA_COS_TX_SHIFT 2
#define E2_INTEG_DATA_OPPORTUNISTICQM (0x1 << 3)
#define E2_INTEG_DATA_OPPORTUNISTICQM_SHIFT 3
#define E2_INTEG_DATA_DPMTESTRELEASEDQ (0x1 << 4)
#define E2_INTEG_DATA_DPMTESTRELEASEDQ_SHIFT 4
#define E2_INTEG_DATA_RESERVED (0x7 << 5)
#define E2_INTEG_DATA_RESERVED_SHIFT 5
	uint8_t cos;
	uint8_t voq;
	uint8_t pbf_queue;
#elif defined(__LITTLE_ENDIAN)
	uint8_t pbf_queue;
	uint8_t voq;
	uint8_t cos;
	uint8_t flags;
#define E2_INTEG_DATA_TESTING_EN (0x1 << 0)
#define E2_INTEG_DATA_TESTING_EN_SHIFT 0
#define E2_INTEG_DATA_LB_TX (0x1 << 1)
#define E2_INTEG_DATA_LB_TX_SHIFT 1
#define E2_INTEG_DATA_COS_TX (0x1 << 2)
#define E2_INTEG_DATA_COS_TX_SHIFT 2
#define E2_INTEG_DATA_OPPORTUNISTICQM (0x1 << 3)
#define E2_INTEG_DATA_OPPORTUNISTICQM_SHIFT 3
#define E2_INTEG_DATA_DPMTESTRELEASEDQ (0x1 << 4)
#define E2_INTEG_DATA_DPMTESTRELEASEDQ_SHIFT 4
#define E2_INTEG_DATA_RESERVED (0x7 << 5)
#define E2_INTEG_DATA_RESERVED_SHIFT 5
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved3;
	uint8_t reserved2;
	uint8_t ramEn;
#elif defined(__LITTLE_ENDIAN)
	uint8_t ramEn;
	uint8_t reserved2;
	uint16_t reserved3;
#endif
};


/*
 * set mac event data
 */
struct eth_event_data {
	__le32 echo;
	__le32 reserved0;
	__le32 reserved1;
};


/*
 * pf-vf event data
 */
struct vf_pf_event_data {
	uint8_t vf_id;
	uint8_t reserved0;
	__le16 reserved1;
	__le32 msg_addr_lo;
	__le32 msg_addr_hi;
};

/*
 * VF FLR event data
 */
struct vf_flr_event_data {
	uint8_t vf_id;
	uint8_t reserved0;
	__le16 reserved1;
	__le32 reserved2;
	__le32 reserved3;
};

/*
 * malicious VF event data
 */
struct malicious_vf_event_data {
	uint8_t vf_id;
	uint8_t err_id;
	__le16 reserved1;
	__le32 reserved2;
	__le32 reserved3;
};

/*
 * vif list event data
 */
struct vif_list_event_data {
	uint8_t func_bit_map;
	uint8_t echo;
	__le16 reserved0;
	__le32 reserved1;
	__le32 reserved2;
};

/*
 * function update event data
 */
struct function_update_event_data {
	uint8_t echo;
	uint8_t reserved;
	__le16 reserved0;
	__le32 reserved1;
	__le32 reserved2;
};

/*
 * union for all event ring message types
 */
union event_data {
	struct vf_pf_event_data vf_pf_event;
	struct eth_event_data eth_event;
	struct cfc_del_event_data cfc_del_event;
	struct vf_flr_event_data vf_flr_event;
	struct malicious_vf_event_data malicious_vf_event;
	struct vif_list_event_data vif_list_event;
	struct function_update_event_data function_update_event;
};


/*
 * per PF event ring data
 */
struct event_ring_data {
	struct regpair_native base_addr;
#if defined(__BIG_ENDIAN)
	uint8_t index_id;
	uint8_t sb_id;
	uint16_t producer;
#elif defined(__LITTLE_ENDIAN)
	uint16_t producer;
	uint8_t sb_id;
	uint8_t index_id;
#endif
	uint32_t reserved0;
};


/*
 * event ring message element (each element is 128 bits)
 */
struct event_ring_msg {
	uint8_t opcode;
	uint8_t error;
	uint16_t reserved1;
	union event_data data;
};

/*
 * event ring next page element (128 bits)
 */
struct event_ring_next {
	struct regpair addr;
	uint32_t reserved[2];
};

/*
 * union for event ring element types (each element is 128 bits)
 */
union event_ring_elem {
	struct event_ring_msg message;
	struct event_ring_next next_page;
};


/*
 * Common event ring opcodes
 */
enum event_ring_opcode {
	EVENT_RING_OPCODE_VF_PF_CHANNEL,
	EVENT_RING_OPCODE_FUNCTION_START,
	EVENT_RING_OPCODE_FUNCTION_STOP,
	EVENT_RING_OPCODE_CFC_DEL,
	EVENT_RING_OPCODE_CFC_DEL_WB,
	EVENT_RING_OPCODE_STAT_QUERY,
	EVENT_RING_OPCODE_STOP_TRAFFIC,
	EVENT_RING_OPCODE_START_TRAFFIC,
	EVENT_RING_OPCODE_VF_FLR,
	EVENT_RING_OPCODE_MALICIOUS_VF,
	EVENT_RING_OPCODE_FORWARD_SETUP,
	EVENT_RING_OPCODE_RSS_UPDATE_RULES,
	EVENT_RING_OPCODE_FUNCTION_UPDATE,
	EVENT_RING_OPCODE_AFEX_VIF_LISTS,
	EVENT_RING_OPCODE_SET_MAC,
	EVENT_RING_OPCODE_CLASSIFICATION_RULES,
	EVENT_RING_OPCODE_FILTERS_RULES,
	EVENT_RING_OPCODE_MULTICAST_RULES,
	EVENT_RING_OPCODE_SET_TIMESYNC,
	MAX_EVENT_RING_OPCODE};


/*
 * Modes for fairness algorithm
 */
enum fairness_mode {
	FAIRNESS_COS_WRR_MODE,
	FAIRNESS_COS_ETS_MODE,
	MAX_FAIRNESS_MODE};


/*
 * Priority and cos
 */
struct priority_cos {
	uint8_t priority;
	uint8_t cos;
	__le16 reserved1;
};

/*
 * The data for flow control configuration
 */
struct flow_control_configuration {
	struct priority_cos traffic_type_to_priority_cos[MAX_TRAFFIC_TYPES];
	uint8_t dcb_enabled;
	uint8_t dcb_version;
	uint8_t dont_add_pri_0_en;
	uint8_t reserved1;
	__le32 reserved2;
	uint8_t dcb_outer_pri[MAX_TRAFFIC_TYPES];
};


/*
 *
 */
struct function_start_data {
	uint8_t function_mode;
	uint8_t allow_npar_tx_switching;
	__le16 sd_vlan_tag;
	__le16 vif_id;
	uint8_t path_id;
	uint8_t network_cos_mode;
	uint8_t dmae_cmd_id;
	uint8_t no_added_tags;
	__le16 reserved0;
	__le32 reserved1;
	uint8_t inner_clss_vxlan;
	uint8_t inner_clss_l2gre;
	uint8_t inner_clss_l2geneve;
	uint8_t inner_rss;
	__le16 vxlan_dst_port;
	__le16 geneve_dst_port;
	uint8_t sd_accept_mf_clss_fail;
	uint8_t sd_accept_mf_clss_fail_match_ethtype;
	__le16 sd_accept_mf_clss_fail_ethtype;
	__le16 sd_vlan_eth_type;
	uint8_t sd_vlan_force_pri_flg;
	uint8_t sd_vlan_force_pri_val;
	uint8_t c2s_pri_tt_valid;
	uint8_t c2s_pri_default;
	uint8_t tx_vlan_filtering_enable;
	uint8_t tx_vlan_filtering_use_pvid;
	uint8_t reserved2[4];
	struct c2s_pri_trans_table_entry c2s_pri_trans_table;
};


/*
 *
 */
struct function_update_data {
	uint8_t vif_id_change_flg;
	uint8_t afex_default_vlan_change_flg;
	uint8_t allowed_priorities_change_flg;
	uint8_t network_cos_mode_change_flg;
	__le16 vif_id;
	__le16 afex_default_vlan;
	uint8_t allowed_priorities;
	uint8_t network_cos_mode;
	uint8_t lb_mode_en_change_flg;
	uint8_t lb_mode_en;
	uint8_t tx_switch_suspend_change_flg;
	uint8_t tx_switch_suspend;
	uint8_t echo;
	uint8_t update_tunn_cfg_flg;
	uint8_t inner_clss_vxlan;
	uint8_t inner_clss_l2gre;
	uint8_t inner_clss_l2geneve;
	uint8_t inner_rss;
	__le16 vxlan_dst_port;
	__le16 geneve_dst_port;
	uint8_t sd_vlan_force_pri_change_flg;
	uint8_t sd_vlan_force_pri_flg;
	uint8_t sd_vlan_force_pri_val;
	uint8_t sd_vlan_tag_change_flg;
	uint8_t sd_vlan_eth_type_change_flg;
	uint8_t reserved1;
	__le16 sd_vlan_tag;
	__le16 sd_vlan_eth_type;
	uint8_t tx_vlan_filtering_pvid_change_flg;
	uint8_t reserved0;
	__le32 reserved2;
};


/*
 * FW version stored in the Xstorm RAM
 */
struct fw_version {
#if defined(__BIG_ENDIAN)
	uint8_t engineering;
	uint8_t revision;
	uint8_t minor;
	uint8_t major;
#elif defined(__LITTLE_ENDIAN)
	uint8_t major;
	uint8_t minor;
	uint8_t revision;
	uint8_t engineering;
#endif
	uint32_t flags;
#define FW_VERSION_OPTIMIZED (0x1 << 0)
#define FW_VERSION_OPTIMIZED_SHIFT 0
#define FW_VERSION_BIG_ENDIEN (0x1 << 1)
#define FW_VERSION_BIG_ENDIEN_SHIFT 1
#define FW_VERSION_CHIP_VERSION (0x3 << 2)
#define FW_VERSION_CHIP_VERSION_SHIFT 2
#define __FW_VERSION_RESERVED (0xFFFFFFF << 4)
#define __FW_VERSION_RESERVED_SHIFT 4
};


/*
 * Dynamic Host-Coalescing - Driver(host) counters
 */
struct hc_dynamic_sb_drv_counters {
	uint32_t dynamic_hc_drv_counter[HC_SB_MAX_DYNAMIC_INDICES];
};


/*
 * 2 bytes. configuration/state parameters for a single protocol index
 */
struct hc_index_data {
#if defined(__BIG_ENDIAN)
	uint8_t flags;
#define HC_INDEX_DATA_SM_ID (0x1 << 0)
#define HC_INDEX_DATA_SM_ID_SHIFT 0
#define HC_INDEX_DATA_HC_ENABLED (0x1 << 1)
#define HC_INDEX_DATA_HC_ENABLED_SHIFT 1
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED (0x1 << 2)
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED_SHIFT 2
#define HC_INDEX_DATA_RESERVE (0x1F << 3)
#define HC_INDEX_DATA_RESERVE_SHIFT 3
	uint8_t timeout;
#elif defined(__LITTLE_ENDIAN)
	uint8_t timeout;
	uint8_t flags;
#define HC_INDEX_DATA_SM_ID (0x1 << 0)
#define HC_INDEX_DATA_SM_ID_SHIFT 0
#define HC_INDEX_DATA_HC_ENABLED (0x1 << 1)
#define HC_INDEX_DATA_HC_ENABLED_SHIFT 1
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED (0x1 << 2)
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED_SHIFT 2
#define HC_INDEX_DATA_RESERVE (0x1F << 3)
#define HC_INDEX_DATA_RESERVE_SHIFT 3
#endif
};


/*
 * HC state-machine
 */
struct hc_status_block_sm {
#if defined(__BIG_ENDIAN)
	uint8_t igu_seg_id;
	uint8_t igu_sb_id;
	uint8_t timer_value;
	uint8_t __flags;
#elif defined(__LITTLE_ENDIAN)
	uint8_t __flags;
	uint8_t timer_value;
	uint8_t igu_sb_id;
	uint8_t igu_seg_id;
#endif
	uint32_t time_to_expire;
};

/*
 * hold PCI identification variables- used in various places in firmware
 */
struct pci_entity {
#if defined(__BIG_ENDIAN)
	uint8_t vf_valid;
	uint8_t vf_id;
	uint8_t vnic_id;
	uint8_t pf_id;
#elif defined(__LITTLE_ENDIAN)
	uint8_t pf_id;
	uint8_t vnic_id;
	uint8_t vf_id;
	uint8_t vf_valid;
#endif
};

/*
 * The fast-path status block meta-data, common to all chips
 */
struct hc_sb_data {
	struct regpair_native host_sb_addr;
	struct hc_status_block_sm state_machine[HC_SB_MAX_SM];
	struct pci_entity p_func;
#if defined(__BIG_ENDIAN)
	uint8_t rsrv0;
	uint8_t state;
	uint8_t dhc_qzone_id;
	uint8_t same_igu_sb_1b;
#elif defined(__LITTLE_ENDIAN)
	uint8_t same_igu_sb_1b;
	uint8_t dhc_qzone_id;
	uint8_t state;
	uint8_t rsrv0;
#endif
	struct regpair_native rsrv1[2];
};


/*
 * Segment types for host coaslescing
 */
enum hc_segment {
	HC_REGULAR_SEGMENT,
	HC_DEFAULT_SEGMENT,
	MAX_HC_SEGMENT};


/*
 * The fast-path status block meta-data
 */
struct hc_sp_status_block_data {
	struct regpair_native host_sb_addr;
#if defined(__BIG_ENDIAN)
	uint8_t rsrv1;
	uint8_t state;
	uint8_t igu_seg_id;
	uint8_t igu_sb_id;
#elif defined(__LITTLE_ENDIAN)
	uint8_t igu_sb_id;
	uint8_t igu_seg_id;
	uint8_t state;
	uint8_t rsrv1;
#endif
	struct pci_entity p_func;
};


/*
 * The fast-path status block meta-data
 */
struct hc_status_block_data_e1x {
	struct hc_index_data index_data[HC_SB_MAX_INDICES_E1X];
	struct hc_sb_data common;
};


/*
 * The fast-path status block meta-data
 */
struct hc_status_block_data_e2 {
	struct hc_index_data index_data[HC_SB_MAX_INDICES_E2];
	struct hc_sb_data common;
};


/*
 * IGU block operartion modes (in Everest2)
 */
enum igu_mode {
	HC_IGU_BC_MODE,
	HC_IGU_NBC_MODE,
	MAX_IGU_MODE};


/*
 * Inner Headers Classification Type
 */
enum inner_clss_type {
	INNER_CLSS_DISABLED,
	INNER_CLSS_USE_VLAN,
	INNER_CLSS_USE_VNI,
	MAX_INNER_CLSS_TYPE};


/*
 * IP versions
 */
enum ip_ver {
	IP_V4,
	IP_V6,
	MAX_IP_VER};


/*
 * Malicious VF error ID
 */
enum malicious_vf_error_id {
	MALICIOUS_VF_NO_ERROR,
	VF_PF_CHANNEL_NOT_READY,
	ETH_ILLEGAL_BD_LENGTHS,
	ETH_PACKET_TOO_SHORT,
	ETH_PAYLOAD_TOO_BIG,
	ETH_ILLEGAL_ETH_TYPE,
	ETH_ILLEGAL_LSO_HDR_LEN,
	ETH_TOO_MANY_BDS,
	ETH_ZERO_HDR_NBDS,
	ETH_START_BD_NOT_SET,
	ETH_ILLEGAL_PARSE_NBDS,
	ETH_IPV6_AND_CHECKSUM,
	ETH_VLAN_FLG_INCORRECT,
	ETH_ILLEGAL_LSO_MSS,
	ETH_TUNNEL_NOT_SUPPORTED,
	MAX_MALICIOUS_VF_ERROR_ID};


/*
 * Multi-function modes
 */
enum mf_mode {
	SINGLE_FUNCTION,
	MULTI_FUNCTION_SD,
	MULTI_FUNCTION_SI,
	MULTI_FUNCTION_AFEX,
	MAX_MF_MODE};


/*
 * Protocol-common statistics collected by the Tstorm (per pf)
 */
struct tstorm_per_pf_stats {
	struct regpair rcv_error_bytes;
};

/*
 *
 */
struct per_pf_stats {
	struct tstorm_per_pf_stats tstorm_pf_statistics;
};


/*
 * Protocol-common statistics collected by the Tstorm (per port)
 */
struct tstorm_per_port_stats {
	__le32 mac_discard;
	__le32 mac_filter_discard;
	__le32 brb_truncate_discard;
	__le32 mf_tag_discard;
	__le32 packet_drop;
	__le32 reserved;
};

/*
 *
 */
struct per_port_stats {
	struct tstorm_per_port_stats tstorm_port_statistics;
};


/*
 * Protocol-common statistics collected by the Tstorm (per client)
 */
struct tstorm_per_queue_stats {
	struct regpair rcv_ucast_bytes;
	__le32 rcv_ucast_pkts;
	__le32 checksum_discard;
	struct regpair rcv_bcast_bytes;
	__le32 rcv_bcast_pkts;
	__le32 pkts_too_big_discard;
	struct regpair rcv_mcast_bytes;
	__le32 rcv_mcast_pkts;
	__le32 ttl0_discard;
	__le16 no_buff_discard;
	__le16 reserved0;
	__le32 reserved1;
};

/*
 * Protocol-common statistics collected by the Ustorm (per client)
 */
struct ustorm_per_queue_stats {
	struct regpair ucast_no_buff_bytes;
	struct regpair mcast_no_buff_bytes;
	struct regpair bcast_no_buff_bytes;
	__le32 ucast_no_buff_pkts;
	__le32 mcast_no_buff_pkts;
	__le32 bcast_no_buff_pkts;
	__le32 coalesced_pkts;
	struct regpair coalesced_bytes;
	__le32 coalesced_events;
	__le32 coalesced_aborts;
};

/*
 * Protocol-common statistics collected by the Xstorm (per client)
 */
struct xstorm_per_queue_stats {
	struct regpair ucast_bytes_sent;
	struct regpair mcast_bytes_sent;
	struct regpair bcast_bytes_sent;
	__le32 ucast_pkts_sent;
	__le32 mcast_pkts_sent;
	__le32 bcast_pkts_sent;
	__le32 error_drop_pkts;
};

/*
 *
 */
struct per_queue_stats {
	struct tstorm_per_queue_stats tstorm_queue_statistics;
	struct ustorm_per_queue_stats ustorm_queue_statistics;
	struct xstorm_per_queue_stats xstorm_queue_statistics;
};


/*
 * FW version stored in first line of pram
 */
struct pram_fw_version {
	uint8_t major;
	uint8_t minor;
	uint8_t revision;
	uint8_t engineering;
	uint8_t flags;
#define PRAM_FW_VERSION_OPTIMIZED (0x1 << 0)
#define PRAM_FW_VERSION_OPTIMIZED_SHIFT 0
#define PRAM_FW_VERSION_STORM_ID (0x3 << 1)
#define PRAM_FW_VERSION_STORM_ID_SHIFT 1
#define PRAM_FW_VERSION_BIG_ENDIEN (0x1 << 3)
#define PRAM_FW_VERSION_BIG_ENDIEN_SHIFT 3
#define PRAM_FW_VERSION_CHIP_VERSION (0x3 << 4)
#define PRAM_FW_VERSION_CHIP_VERSION_SHIFT 4
#define __PRAM_FW_VERSION_RESERVED0 (0x3 << 6)
#define __PRAM_FW_VERSION_RESERVED0_SHIFT 6
};


/*
 * Ethernet slow path element
 */
union protocol_common_specific_data {
	uint8_t protocol_data[8];
	struct regpair phy_address;
	struct regpair mac_config_addr;
	struct afex_vif_list_ramrod_data afex_vif_list_data;
};

/*
 * The send queue element
 */
struct protocol_common_spe {
	struct spe_hdr hdr;
	union protocol_common_specific_data data;
};


/*
 * The data for the Set Timesync Ramrod
 */
struct set_timesync_ramrod_data {
	uint8_t drift_adjust_cmd;
	uint8_t offset_cmd;
	uint8_t add_sub_drift_adjust_value;
	uint8_t drift_adjust_value;
	uint32_t drift_adjust_period;
	struct regpair offset_delta;
};


/*
 * The send queue element
 */
struct slow_path_element {
	struct spe_hdr hdr;
	struct regpair protocol_data;
};


/*
 * Protocol-common statistics counter
 */
struct stats_counter {
	__le16 xstats_counter;
	__le16 reserved0;
	__le32 reserved1;
	__le16 tstats_counter;
	__le16 reserved2;
	__le32 reserved3;
	__le16 ustats_counter;
	__le16 reserved4;
	__le32 reserved5;
	__le16 cstats_counter;
	__le16 reserved6;
	__le32 reserved7;
};


/*
 *
 */
struct stats_query_entry {
	uint8_t kind;
	uint8_t index;
	__le16 funcID;
	__le32 reserved;
	struct regpair address;
};

/*
 * statistic command
 */
struct stats_query_cmd_group {
	struct stats_query_entry query[STATS_QUERY_CMD_COUNT];
};


/*
 * statistic command header
 */
struct stats_query_header {
	uint8_t cmd_num;
	uint8_t reserved0;
	__le16 drv_stats_counter;
	__le32 reserved1;
	struct regpair stats_counters_addrs;
};


/*
 * Types of statistcis query entry
 */
enum stats_query_type {
	STATS_TYPE_QUEUE,
	STATS_TYPE_PORT,
	STATS_TYPE_PF,
	STATS_TYPE_TOE,
	STATS_TYPE_FCOE,
	MAX_STATS_QUERY_TYPE};


/*
 * Indicate of the function status block state
 */
enum status_block_state {
	SB_DISABLED,
	SB_ENABLED,
	SB_CLEANED,
	MAX_STATUS_BLOCK_STATE};


/*
 * Storm IDs (including attentions for IGU related enums)
 */
enum storm_id {
	USTORM_ID,
	CSTORM_ID,
	XSTORM_ID,
	TSTORM_ID,
	ATTENTION_ID,
	MAX_STORM_ID};


/*
 * Taffic types used in ETS and flow control algorithms
 */
enum traffic_type {
	LLFC_TRAFFIC_TYPE_NW,
	LLFC_TRAFFIC_TYPE_FCOE,
	LLFC_TRAFFIC_TYPE_ISCSI,
	MAX_TRAFFIC_TYPE};


/*
 * zone A per-queue data
 */
struct tstorm_queue_zone_data {
	struct regpair reserved[4];
};


/*
 * zone B per-VF data
 */
struct tstorm_vf_zone_data {
	struct regpair reserved;
};


/*
 * Add or Subtract Value for Set Timesync Ramrod
 */
enum ts_add_sub_value {
	TS_SUB_VALUE,
	TS_ADD_VALUE,
	MAX_TS_ADD_SUB_VALUE};


/*
 * Drift-Adjust Commands for Set Timesync Ramrod
 */
enum ts_drift_adjust_cmd {
	TS_DRIFT_ADJUST_KEEP,
	TS_DRIFT_ADJUST_SET,
	TS_DRIFT_ADJUST_RESET,
	MAX_TS_DRIFT_ADJUST_CMD};


/*
 * Offset Commands for Set Timesync Ramrod
 */
enum ts_offset_cmd {
	TS_OFFSET_KEEP,
	TS_OFFSET_INC,
	TS_OFFSET_DEC,
	MAX_TS_OFFSET_CMD};


/*
 * Input for measuring Pci Latency
 */
struct t_measure_pci_latency_ctrl {
	struct regpair read_addr;
#if defined(__BIG_ENDIAN)
	uint8_t sleep;
	uint8_t enable;
	uint8_t func_id;
	uint8_t read_size;
#elif defined(__LITTLE_ENDIAN)
	uint8_t read_size;
	uint8_t func_id;
	uint8_t enable;
	uint8_t sleep;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t num_meas;
	uint8_t reserved;
	uint8_t period_10us;
#elif defined(__LITTLE_ENDIAN)
	uint8_t period_10us;
	uint8_t reserved;
	uint16_t num_meas;
#endif
};


/*
 * Input for measuring Pci Latency
 */
struct t_measure_pci_latency_data {
#if defined(__BIG_ENDIAN)
	uint16_t max_time_ns;
	uint16_t min_time_ns;
#elif defined(__LITTLE_ENDIAN)
	uint16_t min_time_ns;
	uint16_t max_time_ns;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved;
	uint16_t num_reads;
#elif defined(__LITTLE_ENDIAN)
	uint16_t num_reads;
	uint16_t reserved;
#endif
	struct regpair sum_time_ns;
};


/*
 * zone A per-queue data
 */
struct ustorm_queue_zone_data {
	struct ustorm_eth_rx_producers eth_rx_producers;
	struct regpair reserved[3];
};


/*
 * zone B per-VF data
 */
struct ustorm_vf_zone_data {
	struct regpair reserved;
};


/*
 * data per VF-PF channel
 */
struct vf_pf_channel_data {
#if defined(__BIG_ENDIAN)
	uint16_t reserved0;
	uint8_t valid;
	uint8_t state;
#elif defined(__LITTLE_ENDIAN)
	uint8_t state;
	uint8_t valid;
	uint16_t reserved0;
#endif
	uint32_t reserved1;
};


/*
 * State of VF-PF channel
 */
enum vf_pf_channel_state {
	VF_PF_CHANNEL_STATE_READY,
	VF_PF_CHANNEL_STATE_WAITING_FOR_ACK,
	MAX_VF_PF_CHANNEL_STATE};


/*
 * vif_list_rule_kind
 */
enum vif_list_rule_kind {
	VIF_LIST_RULE_SET,
	VIF_LIST_RULE_GET,
	VIF_LIST_RULE_CLEAR_ALL,
	VIF_LIST_RULE_CLEAR_FUNC,
	MAX_VIF_LIST_RULE_KIND};


/*
 * zone A per-queue data
 */
struct xstorm_queue_zone_data {
	struct regpair reserved[4];
};


/*
 * zone B per-VF data
 */
struct xstorm_vf_zone_data {
	struct regpair reserved;
};

#endif /* ECORE_HSI_H */
