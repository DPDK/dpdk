/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_SLC_LR_V2_H_
#define _HW_MOD_SLC_LR_V2_H_

#include <stdint.h>

struct slc_lr_v2_rcp_s {
	uint32_t head_slc_en;
	uint32_t head_dyn;
	int32_t head_ofs;
	uint32_t tail_slc_en;
	uint32_t tail_dyn;
	int32_t tail_ofs;
	uint32_t pcap;
};

struct hw_mod_slc_lr_v2_s {
	struct slc_lr_v2_rcp_s *rcp;
};

#endif	/* _HW_MOD_SLC_V2_H_ */
