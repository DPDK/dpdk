/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CNXK_ESWITCH_H__
#define __CNXK_ESWITCH_H__

#include <sys/socket.h>
#include <sys/un.h>

#include <cnxk_ethdev.h>

#include "cn10k_tx.h"

#define CNXK_ESWITCH_CTRL_MSG_SOCK_PATH "/tmp/cxk_rep_ctrl_msg_sock"
#define CNXK_REP_ESWITCH_DEV_MZ		"cnxk_eswitch_dev"
#define CNXK_ESWITCH_VLAN_TPID		0x8100
#define CNXK_ESWITCH_MAX_TXQ		256
#define CNXK_ESWITCH_MAX_RXQ		256
#define CNXK_ESWITCH_LBK_CHAN		63
#define CNXK_ESWITCH_VFPF_SHIFT		8

#define CNXK_ESWITCH_QUEUE_STATE_RELEASED   0
#define CNXK_ESWITCH_QUEUE_STATE_CONFIGURED 1
#define CNXK_ESWITCH_QUEUE_STATE_STARTED    2
#define CNXK_ESWITCH_QUEUE_STATE_STOPPED    3

struct cnxk_rep_info {
	struct rte_eth_dev *rep_eth_dev;
};

struct cnxk_eswitch_txq {
	struct roc_nix_sq sqs;
	uint8_t state;
};

struct cnxk_eswitch_rxq {
	struct roc_nix_rq rqs;
	uint8_t state;
};

struct cnxk_eswitch_cxq {
	struct roc_nix_cq cqs;
	uint8_t state;
};

TAILQ_HEAD(eswitch_flow_list, roc_npc_flow);
struct cnxk_eswitch_dev {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	/* ROC NIX */
	struct roc_nix nix;

	/* ROC NPC */
	struct roc_npc npc;

	/* ROC NPA */
	struct rte_mempool *ctrl_chan_pool;
	const struct plt_memzone *pktmem_mz;
	uint64_t pkt_aura;

	/* Eswitch RQs, SQs and CQs */
	struct cnxk_eswitch_txq *txq;
	struct cnxk_eswitch_rxq *rxq;
	struct cnxk_eswitch_cxq *cxq;

	/* Configured queue count */
	uint16_t nb_rxq;
	uint16_t nb_txq;
	uint16_t rep_cnt;
	uint8_t configured;

	/* Port representor fields */
	rte_spinlock_t rep_lock;
	uint16_t switch_domain_id;
	uint16_t eswitch_vdev;
	struct cnxk_rep_info *rep_info;
};

static inline struct cnxk_eswitch_dev *
cnxk_eswitch_pmd_priv(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(CNXK_REP_ESWITCH_DEV_MZ);
	if (!mz)
		return NULL;

	return mz->addr;
}

int cnxk_eswitch_nix_rsrc_start(struct cnxk_eswitch_dev *eswitch_dev);
int cnxk_eswitch_txq_setup(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid, uint16_t nb_desc,
			   const struct rte_eth_txconf *tx_conf);
int cnxk_eswitch_txq_release(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
int cnxk_eswitch_rxq_setup(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid, uint16_t nb_desc,
			   const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp);
int cnxk_eswitch_rxq_release(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
int cnxk_eswitch_rxq_start(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
int cnxk_eswitch_rxq_stop(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
int cnxk_eswitch_txq_start(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
int cnxk_eswitch_txq_stop(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid);
#endif /* __CNXK_ESWITCH_H__ */
