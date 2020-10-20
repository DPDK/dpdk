/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_SWITCH_H
#define _SFC_SWITCH_H

#include <stdint.h>

#include "efx.h"

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Options for MAE switch port type */
enum sfc_mae_switch_port_type {
	/**
	 * The switch port is operated by a self-sufficient RTE ethdev
	 * and thus refers to its underlying PCIe function
	 */
	SFC_MAE_SWITCH_PORT_INDEPENDENT = 0,
};

struct sfc_mae_switch_port_request {
	enum sfc_mae_switch_port_type		type;
	const efx_mport_sel_t			*entity_mportp;
	const efx_mport_sel_t			*ethdev_mportp;
	uint16_t				ethdev_port_id;
};

int sfc_mae_assign_switch_domain(struct sfc_adapter *sa,
				 uint16_t *switch_domain_id);

int sfc_mae_assign_switch_port(uint16_t switch_domain_id,
			       const struct sfc_mae_switch_port_request *req,
			       uint16_t *switch_port_id);

int sfc_mae_switch_port_by_ethdev(uint16_t switch_domain_id,
				  uint16_t ethdev_port_id,
				  efx_mport_sel_t *mport_sel);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_SWITCH_H */
