/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_ETHDEV_H__
#define __CN10K_ETHDEV_H__

#include <cnxk_ethdev.h>
#include <cnxk_security.h>
#include <cn10k_rxtx.h>

/* Private data in sw rsvd area of struct roc_ot_ipsec_outb_sa */
struct cn10k_outb_priv_data {
	void *userdata;
	/* Rlen computation data */
	struct cnxk_ipsec_outb_rlens rlens;
	/* Back pointer to eth sec session */
	struct cnxk_eth_sec_sess *eth_sec;
	/* SA index */
	uint32_t sa_idx;
};

/* Rx and Tx routines */
void cn10k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn10k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

/* Security context setup */
void cn10k_eth_sec_ops_override(void);

/* SSO Work callback */
void cn10k_eth_sec_sso_work_cb(uint64_t *gw, void *args,
			       uint32_t soft_exp_event);

#endif /* __CN10K_ETHDEV_H__ */
