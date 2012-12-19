/*-
 * GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 * 
 *   This program is free software; you can redistribute it and/or modify 
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 * 
 *   This program is distributed in the hope that it will be useful, but 
 *   WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *   General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program; if not, write to the Free Software 
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution 
 *   in the file called LICENSE.GPL.
 * 
 *   Contact Information:
 *   Intel Corporation
 * 
 */

#ifndef _KNI_DEV_H_
#define _KNI_DEV_H_

#include <linux/if.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>

#define KNI_KTHREAD_RESCHEDULE_INTERVAL 10 /* us */

/**
 * A structure describing the private information for a kni device.
 */
struct kni_dev {
	struct net_device_stats stats;
	int status;
	int idx;

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

	void * sync_kva;
	void *sync_va;

	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	unsigned mbuf_size;

	/* synchro for request processing */
	unsigned long synchro;
};

#define DEBUG_KNI

#define KNI_ERR(args...) printk(KERN_DEBUG "KNI: Error: " args)
#define KNI_PRINT(args...) printk(KERN_DEBUG "KNI: " args)
#ifdef DEBUG_KNI
	#define KNI_DBG(args...) printk(KERN_DEBUG "KNI: " args)
#else
	#define KNI_DBG(args...)
#endif

#endif
