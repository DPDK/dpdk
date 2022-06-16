/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdint.h>

#include "dlb2_priv.h"

/* DLB2 PMD Internal interface function pointers.
 * If VDEV (bifurcated PMD),  these will resolve to functions that issue ioctls
 * serviced by DLB kernel module.
 * If PCI (PF PMD),  these will be implemented locally in user mode.
 */

void (*dlb2_iface_low_level_io_init)(void);

int (*dlb2_iface_open)(struct dlb2_hw_dev *handle, const char *name);

int (*dlb2_iface_get_device_version)(struct dlb2_hw_dev *handle,
				     uint8_t *revision);

void (*dlb2_iface_hardware_init)(struct dlb2_hw_dev *handle);

int (*dlb2_iface_get_cq_poll_mode)(struct dlb2_hw_dev *handle,
				   enum dlb2_cq_poll_modes *mode);

int (*dlb2_iface_get_num_resources)(struct dlb2_hw_dev *handle,
				struct dlb2_get_num_resources_args *rsrcs);

int (*dlb2_iface_sched_domain_create)(struct dlb2_hw_dev *handle,
				struct dlb2_create_sched_domain_args *args);

void (*dlb2_iface_domain_reset)(struct dlb2_eventdev *dlb2);

int (*dlb2_iface_ldb_queue_create)(struct dlb2_hw_dev *handle,
				   struct dlb2_create_ldb_queue_args *cfg);

int (*dlb2_iface_get_sn_allocation)(struct dlb2_hw_dev *handle,
				    struct dlb2_get_sn_allocation_args *args);

int (*dlb2_iface_set_sn_allocation)(struct dlb2_hw_dev *handle,
				    struct dlb2_set_sn_allocation_args *args);

int (*dlb2_iface_get_sn_occupancy)(struct dlb2_hw_dev *handle,
				   struct dlb2_get_sn_occupancy_args *args);

int (*dlb2_iface_ldb_port_create)(struct dlb2_hw_dev *handle,
				  struct dlb2_create_ldb_port_args *cfg,
				  enum dlb2_cq_poll_modes poll_mode);

int (*dlb2_iface_dir_port_create)(struct dlb2_hw_dev *handle,
				  struct dlb2_create_dir_port_args *cfg,
				  enum dlb2_cq_poll_modes poll_mode);

int (*dlb2_iface_dir_queue_create)(struct dlb2_hw_dev *handle,
				   struct dlb2_create_dir_queue_args *cfg);

int (*dlb2_iface_map_qid)(struct dlb2_hw_dev *handle,
			  struct dlb2_map_qid_args *cfg);

int (*dlb2_iface_unmap_qid)(struct dlb2_hw_dev *handle,
			    struct dlb2_unmap_qid_args *cfg);

int (*dlb2_iface_pending_port_unmaps)(struct dlb2_hw_dev *handle,
				struct dlb2_pending_port_unmaps_args *args);

int (*dlb2_iface_sched_domain_start)(struct dlb2_hw_dev *handle,
				     struct dlb2_start_domain_args *cfg);

int (*dlb2_iface_get_ldb_queue_depth)(struct dlb2_hw_dev *handle,
				struct dlb2_get_ldb_queue_depth_args *args);

int (*dlb2_iface_get_dir_queue_depth)(struct dlb2_hw_dev *handle,
				struct dlb2_get_dir_queue_depth_args *args);


int (*dlb2_iface_enable_cq_weight)(struct dlb2_hw_dev *handle,
				   struct dlb2_enable_cq_weight_args *args);

int (*dlb2_iface_set_cos_bw)(struct dlb2_hw_dev *handle,
			     struct dlb2_set_cos_bw_args *args);
