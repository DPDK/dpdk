/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_FLOW_HANDLE_H_
#define _TFC_FLOW_HANDLE_H_

#define TFC_POOL_TSID_FLOW_HANDLE_MASK		  0x0F80000000000000ULL
#define TFC_POOL_TSID_FLOW_HANDLE_SFT		  55
#define TFC_RECORD_SIZE_FLOW_HANDLE_MASK	  0x0070000000000000ULL
#define TFC_RECORD_SIZE_FLOW_HANDLE_SFT		  52
#define TFC_EM_REC_OFFSET_FLOW_HANDLE_MASK	  0x000FFFFFFC000000ULL
#define TFC_EM_REC_OFFSET_FLOW_HANDLE_SFT	  26
#define TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_MASK 0x0000000003FFFFFFULL
#define TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_SFT  0

#define TFC_FLOW_HANDLE_MASK ( \
			      TFC_POOL_TSID_FLOW_HANDLE_MASK |	\
			      TFC_RECORD_SIZE_FLOW_HANDLE_MASK |	\
			      TFC_EM_REC_OFFSET_FLOW_HANDLE_MASK |	\
			      TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_MASK)

static inline void tfc_get_fields_from_flow_handle(uint64_t *flow_handle,
					    uint8_t *tsid,
					    uint32_t *record_size,
					    uint32_t *em_record_offset,
					    uint32_t *static_bucket_offset)
{
	*tsid = (uint8_t)((*flow_handle & TFC_POOL_TSID_FLOW_HANDLE_MASK) >>
		 TFC_POOL_TSID_FLOW_HANDLE_SFT);
	*record_size =
		(uint32_t)((*flow_handle & TFC_RECORD_SIZE_FLOW_HANDLE_MASK) >>
		 TFC_RECORD_SIZE_FLOW_HANDLE_SFT);
	*em_record_offset =
		(uint32_t)((*flow_handle & TFC_EM_REC_OFFSET_FLOW_HANDLE_MASK) >>
		 TFC_EM_REC_OFFSET_FLOW_HANDLE_SFT);
	*static_bucket_offset =
		(uint32_t)((*flow_handle & TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_MASK) >>
		 TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_SFT);
}

static inline uint64_t tfc_create_flow_handle(uint32_t tsid,
					      uint32_t record_size,
					      uint32_t em_record_offset,
					      uint32_t static_bucket_offset)
{
	uint64_t flow_handle = 0ULL;

	flow_handle |=
		((((uint64_t)tsid) << TFC_POOL_TSID_FLOW_HANDLE_SFT) &
		 TFC_POOL_TSID_FLOW_HANDLE_MASK);
	flow_handle |=
		((((uint64_t)record_size) << TFC_RECORD_SIZE_FLOW_HANDLE_SFT) &
		 TFC_RECORD_SIZE_FLOW_HANDLE_MASK);
	flow_handle |=
		((((uint64_t)em_record_offset) << TFC_EM_REC_OFFSET_FLOW_HANDLE_SFT) &
		 TFC_EM_REC_OFFSET_FLOW_HANDLE_MASK);
	flow_handle |=
		(((static_bucket_offset) << TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_SFT) &
		 TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_MASK);

	return flow_handle;
}

static inline uint64_t tfc_create_flow_handle2(uint64_t partial_flow_handle,
					       uint32_t static_bucket_offset)
{
	uint64_t flow_handle = partial_flow_handle;

	flow_handle |=
		(((static_bucket_offset) << TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_SFT) &
		 TFC_STATIC_BUCKET_OFFSET_FLOW_HANDLE_MASK);

	return flow_handle;
}

#define TFC_FLOW_GET_POOL_ID(em_record_offset, pool_sz_exp) \
	((em_record_offset) >> (pool_sz_exp))

#endif /* _TFC_FLOW_HANDLE_H_ */
