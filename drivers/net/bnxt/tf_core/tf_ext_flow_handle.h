/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EXT_FLOW_HANDLE_H_
#define _TF_EXT_FLOW_HANDLE_H_

#define TF_NUM_KEY_ENTRIES_FLOW_HANDLE_MASK	0x00000000F0000000ULL
#define TF_NUM_KEY_ENTRIES_FLOW_HANDLE_SFT	28
#define TF_FLOW_TYPE_FLOW_HANDLE_MASK		0x00000000000000F0ULL
#define TF_FLOW_TYPE_FLOW_HANDLE_SFT		4
#define TF_FLAGS_FLOW_HANDLE_MASK		0x000000000000000FULL
#define TF_FLAGS_FLOW_HANDLE_SFT		0
#define TF_INDEX_FLOW_HANDLE_MASK		0xFFFFFFF000000000ULL
#define TF_INDEX_FLOW_HANDLE_SFT		36
#define TF_ENTRY_NUM_FLOW_HANDLE_MASK		0x0000000E00000000ULL
#define TF_ENTRY_NUM_FLOW_HANDLE_SFT		33
#define TF_HASH_TYPE_FLOW_HANDLE_MASK		0x0000000100000000ULL
#define TF_HASH_TYPE_FLOW_HANDLE_SFT		32

#define TF_FLOW_HANDLE_MASK (TF_NUM_KEY_ENTRIES_FLOW_HANDLE_MASK |	\
				TF_FLOW_TYPE_FLOW_HANDLE_MASK |		\
				TF_FLAGS_FLOW_HANDLE_MASK |		\
				TF_INDEX_FLOW_HANDLE_MASK |		\
				TF_ENTRY_NUM_FLOW_HANDLE_MASK |		\
				TF_HASH_TYPE_FLOW_HANDLE_MASK)

#define TF_GET_FIELDS_FROM_FLOW_HANDLE(flow_handle,			\
				       num_key_entries,			\
				       flow_type,			\
				       flags,				\
				       index,				\
				       entry_num,			\
				       hash_type)			\
do {									\
	(num_key_entries) = \
		(((flow_handle) & TF_NUM_KEY_ENTRIES_FLOW_HANDLE_MASK) >> \
		 TF_NUM_KEY_ENTRIES_FLOW_HANDLE_SFT);			\
	(flow_type) = (((flow_handle) & TF_FLOW_TYPE_FLOW_HANDLE_MASK) >> \
		     TF_FLOW_TYPE_FLOW_HANDLE_SFT);			\
	(flags) = (((flow_handle) & TF_FLAGS_FLOW_HANDLE_MASK) >>	\
		     TF_FLAGS_FLOW_HANDLE_SFT);				\
	(index) = (((flow_handle) & TF_INDEX_FLOW_HANDLE_MASK) >>	\
		     TF_INDEX_FLOW_HANDLE_SFT);				\
	(entry_num) = (((flow_handle) & TF_ENTRY_NUM_FLOW_HANDLE_MASK) >> \
		     TF_ENTRY_NUM_FLOW_HANDLE_SFT);			\
	(hash_type) = (((flow_handle) & TF_HASH_TYPE_FLOW_HANDLE_MASK) >> \
		     TF_HASH_TYPE_FLOW_HANDLE_SFT);			\
} while (0)

#define TF_SET_FIELDS_IN_FLOW_HANDLE(flow_handle,			\
				     num_key_entries,			\
				     flow_type,				\
				     flags,				\
				     index,				\
				     entry_num,				\
				     hash_type)				\
do {									\
	(flow_handle) &= ~TF_FLOW_HANDLE_MASK;				\
	(flow_handle) |= \
		(((num_key_entries) << TF_NUM_KEY_ENTRIES_FLOW_HANDLE_SFT) & \
		 TF_NUM_KEY_ENTRIES_FLOW_HANDLE_MASK);			\
	(flow_handle) |= (((flow_type) << TF_FLOW_TYPE_FLOW_HANDLE_SFT) & \
			TF_FLOW_TYPE_FLOW_HANDLE_MASK);			\
	(flow_handle) |= (((flags) << TF_FLAGS_FLOW_HANDLE_SFT) &	\
			TF_FLAGS_FLOW_HANDLE_MASK);			\
	(flow_handle) |= ((((uint64_t)index) << TF_INDEX_FLOW_HANDLE_SFT) & \
			TF_INDEX_FLOW_HANDLE_MASK);			\
	(flow_handle) |=						\
		((((uint64_t)entry_num) << TF_ENTRY_NUM_FLOW_HANDLE_SFT) & \
		 TF_ENTRY_NUM_FLOW_HANDLE_MASK);			\
	(flow_handle) |=						\
		((((uint64_t)hash_type) << TF_HASH_TYPE_FLOW_HANDLE_SFT) & \
		 TF_HASH_TYPE_FLOW_HANDLE_MASK);			\
} while (0)
#define TF_SET_FIELDS_IN_WH_FLOW_HANDLE TF_SET_FIELDS_IN_FLOW_HANDLE

#define TF_GET_INDEX_FROM_FLOW_HANDLE(flow_handle,			\
				      index)				\
do {									\
	index = (((flow_handle) & TF_INDEX_FLOW_HANDLE_MASK) >>		\
		     TF_INDEX_FLOW_HANDLE_SFT);				\
} while (0)

#define TF_GET_HASH_TYPE_FROM_FLOW_HANDLE(flow_handle,			\
					  hash_type)			\
do {									\
	hash_type = (((flow_handle) & TF_HASH_TYPE_FLOW_HANDLE_MASK) >>	\
		     TF_HASH_TYPE_FLOW_HANDLE_SFT);			\
} while (0)

#define TF_GET_NUM_KEY_ENTRIES_FROM_FLOW_HANDLE(flow_handle,		\
					  num_key_entries)		\
	(num_key_entries =						\
		(((flow_handle) & TF_NUM_KEY_ENTRIES_FLOW_HANDLE_MASK) >> \
		     TF_NUM_KEY_ENTRIES_FLOW_HANDLE_SFT))		\

#define TF_GET_ENTRY_NUM_FROM_FLOW_HANDLE(flow_handle,		\
					  entry_num)		\
	(entry_num =						\
		(((flow_handle) & TF_ENTRY_NUM_FLOW_HANDLE_MASK) >> \
		     TF_ENTRY_NUM_FLOW_HANDLE_SFT))		\

/*
 * 32 bit Flow ID handlers
 */
#define TF_GFID_FLOW_ID_MASK		0xFFFFFFF0UL
#define TF_GFID_FLOW_ID_SFT		4
#define TF_FLAG_FLOW_ID_MASK		0x00000002UL
#define TF_FLAG_FLOW_ID_SFT		1
#define TF_DIR_FLOW_ID_MASK		0x00000001UL
#define TF_DIR_FLOW_ID_SFT		0

#define TF_SET_FLOW_ID(flow_id, gfid, flag, dir)			\
do {									\
	(flow_id) &= ~(TF_GFID_FLOW_ID_MASK |				\
		     TF_FLAG_FLOW_ID_MASK |				\
		     TF_DIR_FLOW_ID_MASK);				\
	(flow_id) |= (((gfid) << TF_GFID_FLOW_ID_SFT) &			\
		    TF_GFID_FLOW_ID_MASK) |				\
		(((flag) << TF_FLAG_FLOW_ID_SFT) &			\
		 TF_FLAG_FLOW_ID_MASK) |				\
		(((dir) << TF_DIR_FLOW_ID_SFT) &			\
		 TF_DIR_FLOW_ID_MASK);					\
} while (0)

#define TF_GET_GFID_FROM_FLOW_ID(flow_id, gfid)				\
do {									\
	gfid = (((flow_id) & TF_GFID_FLOW_ID_MASK) >>			\
		TF_GFID_FLOW_ID_SFT);					\
} while (0)

#define TF_GET_DIR_FROM_FLOW_ID(flow_id, dir)				\
do {									\
	dir = (((flow_id) & TF_DIR_FLOW_ID_MASK) >>			\
		TF_DIR_FLOW_ID_SFT);					\
} while (0)

#define TF_GET_FLAG_FROM_FLOW_ID(flow_id, flag)				\
do {									\
	flag = (((flow_id) & TF_FLAG_FLOW_ID_MASK) >>			\
		TF_FLAG_FLOW_ID_SFT);					\
} while (0)

/*
 * 32 bit GFID handlers
 */
#define TF_HASH_INDEX_GFID_MASK	0x07FFFFFFUL
#define TF_HASH_INDEX_GFID_SFT	0
#define TF_HASH_TYPE_GFID_MASK	0x08000000UL
#define TF_HASH_TYPE_GFID_SFT	27

#define TF_GFID_TABLE_INTERNAL 0
#define TF_GFID_TABLE_EXTERNAL 1

#define TF_SET_GFID(gfid, index, type)					\
do {									\
	gfid = (((index) << TF_HASH_INDEX_GFID_SFT) &			\
		TF_HASH_INDEX_GFID_MASK) |				\
		(((type) << TF_HASH_TYPE_GFID_SFT) &			\
		 TF_HASH_TYPE_GFID_MASK);				\
} while (0)

#define TF_GET_HASH_INDEX_FROM_GFID(gfid, index)			\
do {									\
	index = (((gfid) & TF_HASH_INDEX_GFID_MASK) >>			\
		TF_HASH_INDEX_GFID_SFT);				\
} while (0)

#define TF_GET_HASH_TYPE_FROM_GFID(gfid, type)				\
do {									\
	type = (((gfid) & TF_HASH_TYPE_GFID_MASK) >>			\
		TF_HASH_TYPE_GFID_SFT);					\
} while (0)


#endif /* _TF_EXT_FLOW_HANDLE_H_ */
