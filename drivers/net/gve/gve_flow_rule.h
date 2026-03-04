/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Google LLC
 */

#ifndef _GVE_FLOW_RULE_H_
#define _GVE_FLOW_RULE_H_

#include "base/gve_osdep.h"

enum gve_adminq_flow_rule_cfg_opcode {
	GVE_FLOW_RULE_CFG_ADD	= 0,
	GVE_FLOW_RULE_CFG_DEL	= 1,
	GVE_FLOW_RULE_CFG_RESET	= 2,
};

enum gve_adminq_flow_type {
	GVE_FLOW_TYPE_TCPV4,
	GVE_FLOW_TYPE_UDPV4,
	GVE_FLOW_TYPE_SCTPV4,
	GVE_FLOW_TYPE_AHV4,
	GVE_FLOW_TYPE_ESPV4,
	GVE_FLOW_TYPE_TCPV6,
	GVE_FLOW_TYPE_UDPV6,
	GVE_FLOW_TYPE_SCTPV6,
	GVE_FLOW_TYPE_AHV6,
	GVE_FLOW_TYPE_ESPV6,
};

struct gve_flow_spec {
	__be32 src_ip[4];
	__be32 dst_ip[4];
	union {
		struct {
			__be16 src_port;
			__be16 dst_port;
		};
		__be32 spi;
	};
	union {
		u8 tos;
		u8 tclass;
	};
};

/* Flow rule parameters using mixed endianness.
 * - flow_type and action are guest endian.
 * - key and mask are in network byte order (big endian), matching rte_flow.
 * This struct is used by the driver when validating and creating flow rules;
 * guest endian fields are only converted to network byte order within admin
 * queue functions.
 */
struct gve_flow_rule_params {
	u16 flow_type;
	u16 action; /* RX queue id */
	struct gve_flow_spec key;
	struct gve_flow_spec mask;
};

struct gve_priv;

int gve_flow_init_bmp(struct gve_priv *priv);
void gve_flow_free_bmp(struct gve_priv *priv);
int gve_free_flow_rules(struct gve_priv *priv);

#endif /* _GVE_FLOW_RULE_H_ */
