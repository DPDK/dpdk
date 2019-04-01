/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(c) 2010-2014 Intel Corporation.
 */

#ifndef _KNI_DEV_H_
#define _KNI_DEV_H_

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "compat.h"

#include <linux/if.h>
#include <linux/wait.h>
#ifdef HAVE_SIGNAL_FUNCTIONS_OWN_HEADER
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <rte_kni_common.h>
#define KNI_KTHREAD_RESCHEDULE_INTERVAL 5 /* us */

#define MBUF_BURST_SZ 32

/* Default carrier state for created KNI network interfaces */
extern uint32_t dflt_carrier;

/**
 * A structure describing the private information for a kni device.
 */
struct kni_dev {
	/* kni list */
	struct list_head list;

	struct net_device_stats stats;
	int status;
	uint16_t group_id;           /* Group ID of a group of KNI devices */
	uint32_t core_id;            /* Core ID to bind */
	char name[RTE_KNI_NAMESIZE]; /* Network device name */
	struct task_struct *pthread;

	/* wait queue for req/resp */
	wait_queue_head_t wq;
	struct mutex sync_lock;

	/* PCI device id */
	uint16_t device_id;

	/* kni device */
	struct net_device *net_dev;
	struct net_device *lad_dev;
	struct pci_dev *pci_dev;

	/* queue for packets to be sent out */
	void *tx_q;

	/* queue for the packets received */
	void *rx_q;

	/* queue for the allocated mbufs those can be used to save sk buffs */
	void *alloc_q;

	/* free queue for the mbufs to be freed */
	void *free_q;

	/* request queue */
	void *req_q;

	/* response queue */
	void *resp_q;

	void *sync_kva;
	void *sync_va;

	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	uint32_t mbuf_size;

	/* synchro for request processing */
	unsigned long synchro;

	/* buffers */
	void *pa[MBUF_BURST_SZ];
	void *va[MBUF_BURST_SZ];
	void *alloc_pa[MBUF_BURST_SZ];
	void *alloc_va[MBUF_BURST_SZ];
};

void kni_net_release_fifo_phy(struct kni_dev *kni);
void kni_net_rx(struct kni_dev *kni);
void kni_net_init(struct net_device *dev);
void kni_net_config_lo_mode(char *lo_str);
void kni_net_poll_resp(struct kni_dev *kni);
void kni_set_ethtool_ops(struct net_device *netdev);

int ixgbe_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
void ixgbe_kni_remove(struct pci_dev *pdev);
int igb_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
void igb_kni_remove(struct pci_dev *pdev);

#endif
