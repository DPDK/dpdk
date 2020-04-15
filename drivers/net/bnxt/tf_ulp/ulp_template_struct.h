/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TEMPLATE_STRUCT_H_
#define _ULP_TEMPLATE_STRUCT_H_

#include <stdint.h>
#include "rte_ether.h"
#include "rte_icmp.h"
#include "rte_ip.h"
#include "rte_tcp.h"
#include "rte_udp.h"
#include "rte_esp.h"
#include "rte_sctp.h"
#include "rte_flow.h"
#include "tf_core.h"

/* Device specific parameters. */
struct bnxt_ulp_device_params {
	uint8_t				description[16];
	uint32_t			global_fid_enable;
	enum bnxt_ulp_byte_order	byte_order;
	uint8_t				encap_byte_swap;
	uint32_t			lfid_entries;
	uint32_t			lfid_entry_size;
	uint64_t			gfid_entries;
	uint32_t			gfid_entry_size;
	uint64_t			num_flows;
	uint32_t			num_resources_per_flow;
};

/*
 * The ulp_device_params is indexed by the dev_id.
 * This table maintains the device specific parameters.
 */
extern struct bnxt_ulp_device_params ulp_device_params[];

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
