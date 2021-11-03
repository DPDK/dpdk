/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_VDPA_OPS_H
#define _SFC_VDPA_OPS_H

#include <rte_vdpa.h>

#define SFC_VDPA_MAX_QUEUE_PAIRS		1

enum sfc_vdpa_context {
	SFC_VDPA_AS_VF
};

enum sfc_vdpa_state {
	SFC_VDPA_STATE_UNINITIALIZED = 0,
	SFC_VDPA_STATE_INITIALIZED,
};

struct sfc_vdpa_ops_data {
	void				*dev_handle;
	struct rte_vdpa_device		*vdpa_dev;
	enum sfc_vdpa_context		vdpa_context;
	enum sfc_vdpa_state		state;

	uint64_t			dev_features;
	uint64_t			drv_features;
};

struct sfc_vdpa_ops_data *
sfc_vdpa_device_init(void *adapter, enum sfc_vdpa_context context);
void
sfc_vdpa_device_fini(struct sfc_vdpa_ops_data *ops_data);

#endif /* _SFC_VDPA_OPS_H */
