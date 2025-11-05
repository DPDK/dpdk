/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_ETHDEV_H__
#define __CN20K_ETHDEV_H__

#include <cn20k_rxtx.h>
#include <cnxk_ethdev.h>
#include <cnxk_security.h>

/* Private data in sw rsvd area of struct roc_ow_ipsec_outb_sa */
struct cn20k_outb_priv_data {
	void *userdata;
	/* Rlen computation data */
	struct cnxk_ipsec_outb_rlens rlens;
	/* Back pointer to eth sec session */
	struct cnxk_eth_sec_sess *eth_sec;
	/* SA index */
	uint32_t sa_idx;
};

/* Rx and Tx routines */
void cn20k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn20k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

/* Security context setup */
void cn20k_eth_sec_ops_override(void);

/* SSO Work callback */
void cn20k_eth_sec_sso_work_cb(uint64_t *gw, void *args, enum nix_inl_event_type type, void *cq_s,
			       uint32_t port_id);

#endif /* __CN20K_ETHDEV_H__ */
