/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdint.h>

#include "dlb_priv.h"

/* DLB PMD Internal interface function pointers.
 * If VDEV (bifurcated PMD),  these will resolve to functions that issue ioctls
 * serviced by DLB kernel module.
 * If PCI (PF PMD),  these will be implemented locally in user mode.
 */

void (*dlb_iface_low_level_io_init)(struct dlb_eventdev *dlb);

int (*dlb_iface_open)(struct dlb_hw_dev *handle, const char *name);

void (*dlb_iface_domain_close)(struct dlb_eventdev *dlb);

int (*dlb_iface_get_device_version)(struct dlb_hw_dev *handle,
				    uint8_t *revision);

int (*dlb_iface_get_num_resources)(struct dlb_hw_dev *handle,
				   struct dlb_get_num_resources_args *rsrcs);

int (*dlb_iface_sched_domain_create)(struct dlb_hw_dev *handle,
				     struct dlb_create_sched_domain_args *args);

int (*dlb_iface_ldb_credit_pool_create)(struct dlb_hw_dev *handle,
					struct dlb_create_ldb_pool_args *cfg);

int (*dlb_iface_dir_credit_pool_create)(struct dlb_hw_dev *handle,
					struct dlb_create_dir_pool_args *cfg);

int (*dlb_iface_dir_queue_create)(struct dlb_hw_dev *handle,
				  struct dlb_create_dir_queue_args *cfg);

int (*dlb_iface_ldb_queue_create)(struct dlb_hw_dev *handle,
				  struct dlb_create_ldb_queue_args *cfg);

int (*dlb_iface_ldb_port_create)(struct dlb_hw_dev *handle,
				 struct dlb_create_ldb_port_args *cfg,
				 enum dlb_cq_poll_modes poll_mode);

int (*dlb_iface_dir_port_create)(struct dlb_hw_dev *handle,
				 struct dlb_create_dir_port_args *cfg,
				 enum dlb_cq_poll_modes poll_mode);

int (*dlb_iface_map_qid)(struct dlb_hw_dev *handle,
			 struct dlb_map_qid_args *cfg);

int (*dlb_iface_unmap_qid)(struct dlb_hw_dev *handle,
			   struct dlb_unmap_qid_args *cfg);

int (*dlb_iface_sched_domain_start)(struct dlb_hw_dev *handle,
				    struct dlb_start_domain_args *cfg);

int (*dlb_iface_pending_port_unmaps)(struct dlb_hw_dev *handle,
				     struct dlb_pending_port_unmaps_args *args);

int (*dlb_iface_get_cq_poll_mode)(struct dlb_hw_dev *handle,
				  enum dlb_cq_poll_modes *mode);

int (*dlb_iface_get_sn_allocation)(struct dlb_hw_dev *handle,
				   struct dlb_get_sn_allocation_args *args);

int (*dlb_iface_set_sn_allocation)(struct dlb_hw_dev *handle,
				   struct dlb_set_sn_allocation_args *args);

int (*dlb_iface_get_sn_occupancy)(struct dlb_hw_dev *handle,
				  struct dlb_get_sn_occupancy_args *args);

int (*dlb_iface_get_ldb_queue_depth)(struct dlb_hw_dev *handle,
				     struct dlb_get_ldb_queue_depth_args *args);

int (*dlb_iface_get_dir_queue_depth)(struct dlb_hw_dev *handle,
				     struct dlb_get_dir_queue_depth_args *args);

