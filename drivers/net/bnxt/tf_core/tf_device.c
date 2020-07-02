/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include "tf_device.h"
#include "tf_device_p4.h"
#include "tfp.h"
#include "bnxt.h"

struct tf;

/**
 * Device specific bind function
 */
static int
dev_bind_p4(struct tf *tfp __rte_unused,
	    struct tf_session_resources *resources __rte_unused,
	    struct tf_dev_info *dev_info)
{
	/* Initialize the modules */

	dev_info->ops = &tf_dev_ops_p4;
	return 0;
}

int
dev_bind(struct tf *tfp __rte_unused,
	 enum tf_device_type type,
	 struct tf_session_resources *resources,
	 struct tf_dev_info *dev_info)
{
	switch (type) {
	case TF_DEVICE_TYPE_WH:
		return dev_bind_p4(tfp,
				   resources,
				   dev_info);
	default:
		TFP_DRV_LOG(ERR,
			    "Device type not supported\n");
		return -ENOTSUP;
	}
}

int
dev_unbind(struct tf *tfp __rte_unused,
	   struct tf_dev_info *dev_handle __rte_unused)
{
	return 0;
}
