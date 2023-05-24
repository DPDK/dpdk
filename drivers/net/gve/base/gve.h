/* SPDX-License-Identifier: MIT
 * Google Virtual Ethernet (gve) driver
 * Copyright (C) 2015-2022 Google, Inc.
 */

#ifndef _GVE_H_
#define _GVE_H_

#include "gve_desc.h"
#include "gve_desc_dqo.h"

#ifndef GOOGLE_VENDOR_ID
#define GOOGLE_VENDOR_ID	0x1ae0
#endif

#define GVE_DEV_ID		0x0042

#define GVE_REG_BAR		0
#define GVE_DB_BAR		2

/* 1 for management, 1 for rx, 1 for tx */
#define GVE_MIN_MSIX		3

/* PTYPEs are always 10 bits. */
#define GVE_NUM_PTYPES		1024

struct gve_irq_db {
	rte_be32_t id;
} ____cacheline_aligned;

struct gve_ptype {
	uint8_t l3_type;  /* `gve_l3_type` in gve_adminq.h */
	uint8_t l4_type;  /* `gve_l4_type` in gve_adminq.h */
};

struct gve_ptype_lut {
	struct gve_ptype ptypes[GVE_NUM_PTYPES];
};

enum gve_queue_format {
	GVE_QUEUE_FORMAT_UNSPECIFIED = 0x0, /* default unspecified */
	GVE_GQI_RDA_FORMAT	     = 0x1, /* GQI Raw Addressing */
	GVE_GQI_QPL_FORMAT	     = 0x2, /* GQI Queue Page List */
	GVE_DQO_RDA_FORMAT	     = 0x3, /* DQO Raw Addressing */
};

enum gve_state_flags_bit {
	GVE_PRIV_FLAGS_ADMIN_QUEUE_OK		= 1,
	GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK	= 2,
	GVE_PRIV_FLAGS_DEVICE_RINGS_OK		= 3,
	GVE_PRIV_FLAGS_NAPI_ENABLED		= 4,
};

#endif /* _GVE_H_ */
