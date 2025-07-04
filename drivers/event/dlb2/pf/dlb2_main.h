/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_MAIN_H
#define __DLB2_MAIN_H

#include <rte_debug.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_eal_paging.h>

#include "base/dlb2_hw_types.h"
#include "../dlb2_user.h"

#define DLB2_EAL_PROBE_CORE 2
#define DLB2_NUM_PROBE_ENQS 1000
#define DLB2_HCW_MEM_SIZE 8
#define DLB2_HCW_64B_OFF 4
#define DLB2_HCW_ALIGN_MASK 0x3F

struct dlb2_dev;

struct dlb2_port_memory {
	struct dlb2_list_head list;
	void *cq_base;
	bool valid;
};

struct dlb2_dev {
	struct rte_pci_device *pdev;
	struct dlb2_hw hw;
	/* struct list_head list; */
	struct device *dlb2_device;
	bool domain_reset_failed;
	/* The enqueue_four function enqueues four HCWs (one cache-line worth)
	 * to the HQM, using whichever mechanism is supported by the platform
	 * on which this driver is running.
	 */
	void (*enqueue_four)(void *qe4, void *pp_addr);
	/* The resource mutex serializes access to driver data structures and
	 * hardware registers.
	 */
	rte_spinlock_t resource_mutex;
	bool worker_launched;
	u8 revision;
	u8 version;
};

struct dlb2_pp_thread_data {
	struct dlb2_hw *hw;
	int pp;
	bool is_ldb;
	int cycles;
};

struct dlb2_dev *dlb2_probe(struct rte_pci_device *pdev, const void *probe_args);


int dlb2_pf_reset(struct dlb2_dev *dlb2_dev);
int dlb2_pf_create_sched_domain(struct dlb2_hw *hw,
				struct dlb2_create_sched_domain_args *args,
				struct dlb2_cmd_response *resp);
int dlb2_pf_create_ldb_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_ldb_queue_args *args,
			     struct dlb2_cmd_response *resp);
int dlb2_pf_create_dir_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_dir_queue_args *args,
			     struct dlb2_cmd_response *resp);
int dlb2_pf_create_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_ldb_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp);
int dlb2_pf_create_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_dir_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp);
int dlb2_pf_start_domain(struct dlb2_hw *hw,
			 u32 domain_id,
			 struct dlb2_start_domain_args *args,
			 struct dlb2_cmd_response *resp);
int dlb2_pf_enable_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_ldb_port_args *args,
			    struct dlb2_cmd_response *resp);
int dlb2_pf_disable_ldb_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_ldb_port_args *args,
			     struct dlb2_cmd_response *resp);
int dlb2_pf_enable_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_dir_port_args *args,
			    struct dlb2_cmd_response *resp);
int dlb2_pf_disable_dir_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_dir_port_args *args,
			     struct dlb2_cmd_response *resp);
int dlb2_pf_reset_domain(struct dlb2_hw *hw, u32 domain_id);
int dlb2_pf_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				     u32 domain_id,
				     u32 port_id);
int dlb2_pf_dir_port_owned_by_domain(struct dlb2_hw *hw,
				     u32 domain_id,
				     u32 port_id);

#endif /* __DLB2_MAIN_H */
