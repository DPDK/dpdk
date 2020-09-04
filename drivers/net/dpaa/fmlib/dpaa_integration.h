/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2009-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
 */

#ifndef __DPAA_INTEGRATION_H
#define __DPAA_INTEGRATION_H

#include "ncsw_ext.h"

#define DPAA_VERSION	11

#define BM_MAX_NUM_OF_POOLS	64	/**< Number of buffers pools */

#define INTG_MAX_NUM_OF_FM	2

/* Ports defines */
#define FM_MAX_NUM_OF_1G_MACS	6
#define FM_MAX_NUM_OF_10G_MACS	2
#define FM_MAX_NUM_OF_MACS	(FM_MAX_NUM_OF_1G_MACS + FM_MAX_NUM_OF_10G_MACS)
#define FM_MAX_NUM_OF_OH_PORTS	6

#define FM_MAX_NUM_OF_1G_RX_PORTS   FM_MAX_NUM_OF_1G_MACS
#define FM_MAX_NUM_OF_10G_RX_PORTS  FM_MAX_NUM_OF_10G_MACS
#define FM_MAX_NUM_OF_RX_PORTS	\
	(FM_MAX_NUM_OF_10G_RX_PORTS + FM_MAX_NUM_OF_1G_RX_PORTS)

#define FM_MAX_NUM_OF_1G_TX_PORTS   FM_MAX_NUM_OF_1G_MACS
#define FM_MAX_NUM_OF_10G_TX_PORTS  FM_MAX_NUM_OF_10G_MACS
#define FM_MAX_NUM_OF_TX_PORTS	\
	(FM_MAX_NUM_OF_10G_TX_PORTS + FM_MAX_NUM_OF_1G_TX_PORTS)

#define FM_PORT_MAX_NUM_OF_EXT_POOLS		4
	/**< Number of external BM pools per Rx port */
#define FM_NUM_CONG_GRPS		256
	/**< Total number of congestion groups in QM */
#define FM_MAX_NUM_OF_SUB_PORTALS		16
#define FM_PORT_MAX_NUM_OF_OBSERVED_EXT_POOLS   0

/* PCD defines */
#define FM_PCD_PLCR_NUM_ENTRIES		256
		/**< Total number of policer profiles */
#define FM_PCD_KG_NUM_OF_SCHEMES	32
		/**< Total number of KG schemes */
#define FM_PCD_MAX_NUM_OF_CLS_PLANS	256
		/**< Number of classification plan entries. */

#define FM_MAX_PFC_PRIO		8

#endif /* __DPAA_INTEGRATION_H */
