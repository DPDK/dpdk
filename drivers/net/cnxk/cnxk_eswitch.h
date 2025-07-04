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
#define CNXK_ESWITCH_VLAN_TPID		ROC_ESWITCH_VLAN_TPID
#define CNXK_REP_ESWITCH_DEV_MZ		"cnxk_eswitch_dev"
#define CNXK_ESWITCH_MAX_TXQ		256
#define CNXK_ESWITCH_MAX_RXQ		256
#define CNXK_ESWITCH_VFPF_SHIFT		8

#define CNXK_ESWITCH_QUEUE_STATE_RELEASED   0
#define CNXK_ESWITCH_QUEUE_STATE_CONFIGURED 1
#define CNXK_ESWITCH_QUEUE_STATE_STARTED    2
#define CNXK_ESWITCH_QUEUE_STATE_STOPPED    3

TAILQ_HEAD(eswitch_flow_list, roc_npc_flow);
enum cnxk_esw_da_pattern_type {
	CNXK_ESW_DA_TYPE_LIST = 0,
	CNXK_ESW_DA_TYPE_PFVF,
};

struct cnxk_esw_repte_msg {
	struct roc_eswitch_repte_notify_msg *notify_msg;

	TAILQ_ENTRY(cnxk_esw_repte_msg) next;
};

struct cnxk_esw_repte_msg_proc {
	bool start_thread;
	uint8_t msg_avail;
	rte_thread_t repte_msg_thread;
	pthread_cond_t repte_msg_cond;
	pthread_mutex_t mutex;

	TAILQ_HEAD(esw_repte_msg_list, cnxk_esw_repte_msg) msg_list;
};

struct cnxk_esw_repr_hw_info {
	/* Representee pcifunc value */
	uint16_t hw_func;
	/* rep id in sync with kernel */
	uint16_t rep_id;
	/* pf or vf id */
	uint16_t pfvf;
	/* representor port id assigned to representee */
	uint16_t port_id;
	uint16_t num_flow_entries;

	TAILQ_HEAD(flow_list, roc_npc_flow) repr_flow_list;
};

/* Structure representing per devarg information - this can be per representee
 * or range of representee
 */
struct cnxk_eswitch_devargs {
	/* Devargs populated */
	struct rte_eth_devargs da;
	/* HW info of representee */
	struct cnxk_esw_repr_hw_info *repr_hw_info;
	/* No of representor ports */
	uint16_t nb_repr_ports;
	/* Devargs pattern type */
	enum cnxk_esw_da_pattern_type type;
};

struct cnxk_eswitch_repr_cnt {
	/* Max possible representors */
	uint16_t max_repr;
	/* Representors to be created as per devargs passed */
	uint16_t nb_repr_created;
	/* Representors probed successfully */
	uint16_t nb_repr_probed;
	/* Representors started representing a representee */
	uint16_t nb_repr_started;
};

struct cnxk_eswitch_switch_domain {
	uint16_t switch_domain_id;
	uint16_t pf;
};

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

	/* NPC rxtx rules */
	struct flow_list esw_flow_list;
	uint16_t num_entries;
	bool eswitch_vf_rules_setup;
	uint16_t esw_pf_entry;
	uint16_t esw_vf_entry;

	/* Eswitch Representors Devargs */
	uint16_t nb_esw_da;
	uint16_t last_probed;
	struct cnxk_eswitch_devargs esw_da[RTE_MAX_ETHPORTS];

	/* No of representors */
	struct cnxk_eswitch_repr_cnt repr_cnt;

	/* Representor control channel field */
	bool start_ctrl_msg_thrd;
	rte_thread_t rep_ctrl_msg_thread;
	bool client_connected;
	int sock_fd;

	/* Representee notification */
	struct cnxk_esw_repte_msg_proc repte_msg_proc;

	/* Port representor fields */
	rte_spinlock_t rep_lock;
	uint16_t nb_switch_domain;
	struct cnxk_eswitch_switch_domain sw_dom[RTE_MAX_ETHPORTS];
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

/* HW Resources */
int cnxk_eswitch_nix_rsrc_start(struct cnxk_eswitch_dev *eswitch_dev);
int cnxk_eswitch_representor_id(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func,
				uint16_t *rep_id);
struct cnxk_esw_repr_hw_info *cnxk_eswitch_representor_hw_info(struct cnxk_eswitch_dev *eswitch_dev,
							       uint16_t hw_func);
int cnxk_eswitch_repr_devargs(struct rte_pci_device *pci_dev, struct cnxk_eswitch_dev *eswitch_dev);
int cnxk_eswitch_representor_info_get(struct cnxk_eswitch_dev *eswitch_dev,
				      struct rte_eth_representor_info *info);
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
/* Flow Rules */
int cnxk_eswitch_flow_rules_install(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func);
int cnxk_eswitch_flow_rules_delete(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func);
int cnxk_eswitch_pfvf_flow_rules_install(struct cnxk_eswitch_dev *eswitch_dev, bool is_vf);
int cnxk_eswitch_flow_rule_shift(uint16_t hw_func, uint16_t *new_entry);
int cnxk_eswitch_flow_rules_remove_list(struct cnxk_eswitch_dev *eswitch_dev,
					struct flow_list *list, uint16_t hw_func);
/* RX TX fastpath routines */
uint16_t cnxk_eswitch_dev_tx_burst(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid,
				   struct rte_mbuf **pkts, uint16_t nb_tx, const uint16_t flags);
uint16_t cnxk_eswitch_dev_rx_burst(struct cnxk_eswitch_dev *eswitch_dev, uint16_t qid,
				   struct rte_mbuf **pkts, uint16_t nb_pkts);
#endif /* __CNXK_ESWITCH_H__ */
