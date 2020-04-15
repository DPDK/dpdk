/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

/*
 * date: Mon Mar  9 02:37:53 2020
 * version: 0.0
 */

#include "ulp_template_db.h"
#include "ulp_template_struct.h"

struct bnxt_ulp_device_params ulp_device_params[] = {
	[BNXT_ULP_DEVICE_ID_WH_PLUS] = {
		.global_fid_enable       = BNXT_ULP_SYM_YES,
		.byte_order              = (enum bnxt_ulp_byte_order)
						BNXT_ULP_SYM_LITTLE_ENDIAN,
		.encap_byte_swap         = 1,
		.lfid_entries            = 16384,
		.lfid_entry_size         = 4,
		.gfid_entries            = 65536,
		.gfid_entry_size         = 4,
		.num_flows               = 32768,
		.num_resources_per_flow  = 8
	}
};
