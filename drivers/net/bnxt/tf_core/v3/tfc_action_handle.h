/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_ACTION_HANDLE_H_
#define _TFC_ACTION_HANDLE_H_

#define TFC_POOL_TSID_ACTION_HANDLE_MASK	  0x0000003F000000000ULL
#define TFC_POOL_TSID_ACTION_HANDLE_SFT		  36
#define TFC_RECORD_SIZE_ACTION_HANDLE_MASK	  0x00000000F00000000ULL
#define TFC_RECORD_SIZE_ACTION_HANDLE_SFT	  32
#define TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK	  0x00000000007FFFFFFULL
#define TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT	  0

#define TFC_ACTION_HANDLE_MASK ( \
	TFC_POOL_TSID_ACTION_HANDLE_MASK | \
	TFC_RECORD_SIZE_ACTION_HANDLE_MASK | \
	TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK)

static inline void tfc_get_fields_from_action_handle(const uint64_t *act_handle,
						     uint8_t *tsid,
						     uint32_t *record_size,
						     uint32_t *action_offset)
{
	*tsid = (uint8_t)((*act_handle & TFC_POOL_TSID_ACTION_HANDLE_MASK) >>
		 TFC_POOL_TSID_ACTION_HANDLE_SFT);
	*record_size =
		(uint32_t)((*act_handle & TFC_RECORD_SIZE_ACTION_HANDLE_MASK) >>
		 TFC_RECORD_SIZE_ACTION_HANDLE_SFT);
	*action_offset =
		(uint32_t)((*act_handle & TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK) >>
		 TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT);
}

static inline uint64_t tfc_create_action_handle(uint8_t tsid,
						uint32_t record_size,
						uint32_t action_offset)
{
	uint64_t act_handle = 0ULL;

	act_handle |=
		((((uint64_t)tsid) << TFC_POOL_TSID_ACTION_HANDLE_SFT) &
		TFC_POOL_TSID_ACTION_HANDLE_MASK);
	act_handle |=
		((((uint64_t)record_size) << TFC_RECORD_SIZE_ACTION_HANDLE_SFT) &
		TFC_RECORD_SIZE_ACTION_HANDLE_MASK);
	act_handle |=
		((((uint64_t)action_offset) << TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT) &
		TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK);

	return act_handle;
}

#define TFC_ACTION_GET_POOL_ID(action_offset, pool_sz_exp) \
	((action_offset) >> (pool_sz_exp))

#define TFC_GET_32B_OFFSET_ACT_HANDLE(act_32byte_offset, act_handle)     \
	{                                                                \
		(act_32byte_offset) = (uint32_t)((*(act_handle) &	 \
				TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK) >> \
				TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT);    \
	}

#define TFC_GET_8B_OFFSET(act_8byte_offset, act_32byte_offset) \
	{ (act_8byte_offset) = ((act_32byte_offset) << 2); }

#endif /* _TFC_ACTION_HANDLE_H_ */
