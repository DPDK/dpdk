/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#ifndef _ETH_PRIVATE_H_
#define _ETH_PRIVATE_H_

#include <sys/queue.h>

#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_os_shim.h>

#include "rte_ethdev.h"

struct eth_dev_shared {
	uint64_t allocated_owners;
	uint64_t next_owner_id;
	uint64_t allocated_ports;
	struct rte_eth_dev_data data[RTE_MAX_ETHPORTS];
};

/* Shared memory between primary and secondary processes. */
extern struct eth_dev_shared *eth_dev_shared_data
	__rte_guarded_by(rte_mcfg_ethdev_get_lock());

/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_eth_dev_callback {
	TAILQ_ENTRY(rte_eth_dev_callback) next; /**< Callbacks list */
	rte_eth_dev_cb_fn cb_fn;                /**< Callback address */
	void *cb_arg;                           /**< Parameter for callback */
	void *ret_param;                        /**< Return parameter */
	enum rte_eth_event_type event;          /**< Interrupt event type */
	uint32_t active;                        /**< Callback is executing */
};

extern rte_spinlock_t eth_dev_cb_lock;

/* Convert all error to -EIO if device is removed. */
int eth_err(uint16_t port_id, int ret);

/*
 * Convert rte_eth_dev pointer to port ID.
 * NULL will be translated to RTE_MAX_ETHPORTS.
 */
uint16_t eth_dev_to_id(const struct rte_eth_dev *dev);

/* Generic rte_eth_dev comparison function. */
typedef int (*rte_eth_cmp_t)(const struct rte_eth_dev *, const void *);

/* Generic rte_eth_dev iterator. */
struct rte_eth_dev *
eth_find_device(const struct rte_eth_dev *_start, rte_eth_cmp_t cmp,
		const void *data);

/* Parse devargs value for representor parameter. */
int rte_eth_devargs_parse_representor_ports(char *str, void *data);

/* reset eth fast-path API to dummy values */
void eth_dev_fp_ops_reset(struct rte_eth_fp_ops *fpo);

/* setup eth fast-path API to ethdev values */
void eth_dev_fp_ops_setup(struct rte_eth_fp_ops *fpo,
		const struct rte_eth_dev *dev);


void *eth_dev_shared_data_prepare(void)
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock());
void eth_dev_shared_data_release(void)
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock());

void eth_dev_rxq_release(struct rte_eth_dev *dev, uint16_t qid);
void eth_dev_txq_release(struct rte_eth_dev *dev, uint16_t qid);
int eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues);
int eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues);

#endif /* _ETH_PRIVATE_H_ */
