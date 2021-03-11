/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_SRIOV_H
#define _SFC_SRIOV_H

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_sriov {
	/** Number of enabled virtual functions */
	unsigned int			num_vfs;
	/** PF and VFs vPorts configuration */
	efx_vport_config_t		*vport_config;
	/** vSwitch handle */
	efx_vswitch_t			*vswitch;
};

struct sfc_adapter;

int sfc_sriov_attach(struct sfc_adapter *sa);
void sfc_sriov_detach(struct sfc_adapter *sa);

int sfc_sriov_vswitch_create(struct sfc_adapter *sa);
void sfc_sriov_vswitch_destroy(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_SRIOV_H */
