/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_REPR_H
#define _SFC_REPR_H

#include <stdint.h>

#include <rte_ethdev.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Max count of the representor Rx queues */
#define SFC_REPR_RXQ_MAX	1

/** Max count of the representor Tx queues */
#define SFC_REPR_TXQ_MAX	1

struct sfc_repr_entity_info {
	enum rte_eth_representor_type type;
	efx_pcie_interface_t intf;
	uint16_t pf;
	uint16_t vf;
};

int sfc_repr_create(struct rte_eth_dev *parent,
		    struct sfc_repr_entity_info *entity,
		    uint16_t switch_domain_id,
		    const efx_mport_sel_t *mport_sel);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_REPR_H */
