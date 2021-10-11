/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_REPR_PROXY_API_H
#define _SFC_REPR_PROXY_API_H

#include <stdint.h>

#include <rte_ring.h>
#include <rte_mempool.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

int sfc_repr_proxy_add_port(uint16_t pf_port_id, uint16_t repr_id,
			    uint16_t rte_port_id,
			    const efx_mport_sel_t *mport_set);
int sfc_repr_proxy_del_port(uint16_t pf_port_id, uint16_t repr_id);

int sfc_repr_proxy_add_rxq(uint16_t pf_port_id, uint16_t repr_id,
			   uint16_t queue_id, struct rte_ring *rx_ring,
			   struct rte_mempool *mp);
void sfc_repr_proxy_del_rxq(uint16_t pf_port_id, uint16_t repr_id,
			    uint16_t queue_id);

int sfc_repr_proxy_add_txq(uint16_t pf_port_id, uint16_t repr_id,
			   uint16_t queue_id, struct rte_ring *tx_ring,
			   efx_mport_id_t *egress_mport);
void sfc_repr_proxy_del_txq(uint16_t pf_port_id, uint16_t repr_id,
			    uint16_t queue_id);

int sfc_repr_proxy_start_repr(uint16_t pf_port_id, uint16_t repr_id);
int sfc_repr_proxy_stop_repr(uint16_t pf_port_id, uint16_t repr_id);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_REPR_PROXY_API_H */
