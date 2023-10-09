/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _CPFL_RULES_API_H_
#define _CPFL_RULES_API_H_

#include <base/idpf_controlq_api.h>
#include "cpfl_actions.h"
#include "cpfl_controlq.h"

/* Common Bit Mask Macros */
#define CPFL_BIT(b)			(1 << (b))

#define MAKE_MASK(type, mask, shift)	((u##type) (mask) << (shift))
#define SHIFT_VAL_LT(type, val, field)		\
		(((u##type)(val) << field##_S) & field##_M)
#define SHIFT_VAL_RT(type, val, field)		\
		(((u##type)(val) & field##_M) >> field##_S)

#define MAKE_MASK_VAL(type, bit_len)	(((u##type)0x01 << (bit_len)) - 1)
#define MAKE_MASK_VAL16(bit_len)	MAKE_MASK_VAL(16, bit_len)
#define MAKE_MASK_VAL64(bit_len)	MAKE_MASK_VAL(64, bit_len)

#define MAKE_MASK64(mask, shift)	MAKE_MASK(64, mask, shift)
#define MAKE_MASK16(mask, shift)	MAKE_MASK(16, mask, shift)
#define MAKE_MASK32(mask, shift)	MAKE_MASK(32, mask, shift)

/* Make masks with bit length and left-shifting count */
#define MAKE_SMASK(type, bits, shift)	\
	((((u##type)1 << (bits)) - 1) << (shift))
#define MAKE_SMASK64(bits, shift)	MAKE_SMASK(64, bits, shift)
#define MAKE_SMASK32(bits, shift)	MAKE_SMASK(32, bits, shift)
#define MAKE_SMASK16(bits, shift)	MAKE_SMASK(16, bits, shift)

#define SHIFT_VAL64(val, field)		SHIFT_VAL_LT(64, val, field)
#define SHIFT_VAL32(val, field)		SHIFT_VAL_LT(32, val, field)
#define SHIFT_VAL16(val, field)		SHIFT_VAL_LT(16, val, field)

/* Rule Config queue opcodes */
enum cpfl_ctlq_rule_cfg_opc {
	cpfl_ctlq_sem_add_rule				= 0x1303,
	cpfl_ctlq_sem_update_rule			= 0x1304,
	cpfl_ctlq_sem_del_rule				= 0x1305,
	cpfl_ctlq_sem_query_rule			= 0x1306,
	cpfl_ctlq_sem_query_rule_hash_addr		= 0x1307,
	cpfl_ctlq_sem_query_del_rule_hash_addr		= 0x1308,

	cpfl_ctlq_mod_add_update_rule			= 0x1360,
	cpfl_ctlq_mod_query_rule			= 0x1361,
};

enum cpfl_cfg_pkt_error_code {
	CPFL_CFG_PKT_ERR_OK = 0,
	CPFL_CFG_PKT_ERR_ESRCH = 1,     /* Bad opcode */
	CPFL_CFG_PKT_ERR_EEXIST = 2,    /* Entry Already exists */
	CPFL_CFG_PKT_ERR_ENOSPC = 4,    /* No space left in the table*/
	CPFL_CFG_PKT_ERR_ERANGE = 5,    /* Parameter out of range */
	CPFL_CFG_PKT_ERR_ESBCOMP = 6,   /* Completion Error */
	CPFL_CFG_PKT_ERR_ENOPIN = 7,    /* Entry cannot be pinned in cache */
	CPFL_CFG_PKT_ERR_ENOTFND = 8,   /* Entry Not exists */
	CPFL_CFG_PKT_ERR_EMAXCOL = 9    /* Max Hash Collision */
};

/* macros for creating context for rule descriptor */
#define MEV_RULE_VSI_ID_S		0
#define MEV_RULE_VSI_ID_M		\
		MAKE_MASK64(0x7FF, MEV_RULE_VSI_ID_S)

#define MEV_RULE_TIME_SEL_S		13
#define MEV_RULE_TIME_SEL_M		\
		MAKE_MASK64(0x3, MEV_RULE_TIME_SEL_S)

#define MEV_RULE_TIME_SEL_VAL_S		15
#define MEV_RULE_TIME_SEL_VAL_M		\
		MAKE_MASK64(0x1, MEV_RULE_TIME_SEL_VAL_S)

#define MEV_RULE_PORT_NUM_S		16
#define MEV_RULE_HOST_ID_S		18
#define MEV_RULE_PORT_NUM_M		\
		MAKE_MASK64(0x3, MEV_RULE_PORT_NUM_S)
#define MEV_RULE_HOST_ID_M		\
		MAKE_MASK64(0x7, MEV_RULE_HOST_ID_S)

#define MEV_RULE_CACHE_WR_THRU_S	21
#define MEV_RULE_CACHE_WR_THRU_M	\
		MAKE_MASK64(0x1, MEV_RULE_CACHE_WR_THRU_S)

#define MEV_RULE_RESP_REQ_S		22
#define MEV_RULE_RESP_REQ_M		\
		MAKE_MASK64(0x3, MEV_RULE_RESP_REQ_S)
#define MEV_RULE_OBJ_ADDR_S		24
#define MEV_RULE_OBJ_ADDR_M		\
		MAKE_MASK64(0x7FFFFFF, MEV_RULE_OBJ_ADDR_S)
#define MEV_RULE_OBJ_ID_S		59
#define MEV_RULE_OBJ_ID_M		\
		MAKE_MASK64((uint64_t)0x3, MEV_RULE_OBJ_ID_S)

/* macros for creating CFG_CTRL for sem/lem rule blob */
#define MEV_RULE_CFG_CTRL_PROF_ID_S			0
#define MEV_RULE_CFG_CTRL_PROF_ID_M			\
		MAKE_MASK16(0x7FF, MEV_RULE_CFG_CTRL_PROF_ID_S)

#define MEV_RULE_CFG_CTRL_SUB_PROF_ID_S		11
#define MEV_RULE_CFG_CTRL_SUB_PROF_ID_M		\
		MAKE_MASK16(0x3, MEV_RULE_CFG_CTRL_SUB_PROF_ID_S)
#define MEV_RULE_CFG_CTRL_PIN_CACHE_S		13
#define MEV_RULE_CFG_CTRL_PIN_CACHE_M		\
		MAKE_MASK16(0x1, MEV_RULE_CFG_CTRL_PIN_CACHE_S)
#define MEV_RULE_CFG_CTRL_CLEAR_MIRROR_S	14
#define MEV_RULE_CFG_CTRL_CLEAR_MIRROR_M	\
		MAKE_MASK16(0x1, MEV_RULE_CFG_CTRL_CLEAR_MIRROR_S)
#define MEV_RULE_CFG_CTRL_FIXED_FETCH_S		15
#define MEV_RULE_CFG_CTRL_FIXED_FETCH_M		\
		MAKE_MASK16(0x1, MEV_RULE_CFG_CTRL_FIXED_FETCH_S)

/**
 * macro to build the CFG_CTRL for rule packet data, which is one of
 * cpfl_prep_sem_rule_blob()'s input parameter.
 */
 /* build SEM CFG_CTRL*/
#define CPFL_GET_MEV_SEM_RULE_CFG_CTRL(prof_id, sub_prof_id,		       \
				       pin_to_cache, fixed_fetch)	       \
		(SHIFT_VAL16((prof_id), MEV_RULE_CFG_CTRL_PROF_ID)	     | \
		 SHIFT_VAL16((sub_prof_id), MEV_RULE_CFG_CTRL_SUB_PROF_ID)   | \
		 SHIFT_VAL16((pin_to_cache), MEV_RULE_CFG_CTRL_PIN_CACHE)    | \
		 SHIFT_VAL16((fixed_fetch), MEV_RULE_CFG_CTRL_FIXED_FETCH))

/* build LEM CFG_CTRL*/
#define CPFL_GET_MEV_LEM_RULE_CFG_CTRL(prof_id, pin_to_cache, clear_mirror)    \
		(SHIFT_VAL16(prof_id, MEV_RULE_CFG_CTRL_PROF_ID)             | \
		 SHIFT_VAL16(pin_to_cache, MEV_RULE_CFG_CTRL_PIN_CACHE)      | \
		 SHIFT_VAL16(clear_mirror, MEV_RULE_CFG_CTRL_CLEAR_MIRROR))

/* macros for creating mod content config packets */
#define MEV_RULE_MOD_INDEX_S		24
#define MEV_RULE_MOD_INDEX_M		\
		MAKE_MASK64(0xFFFFFFFF, MEV_RULE_MOD_INDEX_S)

#define MEV_RULE_PIN_MOD_CONTENT_S	62
#define MEV_RULE_PIN_MOD_CONTENT_M	\
		MAKE_MASK64((uint64_t)0x1, MEV_RULE_PIN_MOD_CONTENT_S)
#define MEV_RULE_MOD_OBJ_SIZE_S		63
#define MEV_RULE_MOD_OBJ_SIZE_M		\
		MAKE_MASK64((uint64_t)0x1, MEV_RULE_MOD_OBJ_SIZE_S)

/**
 * struct cpfl_sem_rule_cfg_pkt - Describes rule information for SEM
 * note: The key may be in mixed big/little endian format, the rest of members
 * are in little endian
 */
struct cpfl_sem_rule_cfg_pkt {
#define MEV_SEM_RULE_KEY_SIZE 128
	uint8_t key[MEV_SEM_RULE_KEY_SIZE];

#define MEV_SEM_RULE_ACT_SIZE 72
	uint8_t actions[MEV_SEM_RULE_ACT_SIZE];

	/* Bit(s):
	 * 10:0 : PROFILE_ID
	 * 12:11: SUB_PROF_ID (used for SEM only)
	 * 13   : pin the SEM key content into the cache
	 * 14   : Reserved
	 * 15   : Fixed_fetch
	 */
	uint8_t cfg_ctrl[2];

	/* Bit(s):
	 * 0:     valid
	 * 15:1:  Hints
	 * 26:16: PROFILE_ID, the profile associated with the entry
	 * 31:27: PF
	 * 55:32: FLOW ID (assigned by HW)
	 * 63:56: EPOCH
	 */
	uint8_t ctrl_word[8];
	uint8_t padding[46];
};

/**
 * union cpfl_rule_cfg_pkt_record - Describes rule data blob
 */
union cpfl_rule_cfg_pkt_record {
	struct cpfl_sem_rule_cfg_pkt sem_rule;
	uint8_t pkt_data[256];
	uint8_t mod_blob[256];
};

/**
 * cpfl_rule_query_addr - LEM/SEM Rule Query Address structure
 */
struct cpfl_rule_query_addr {
	uint8_t	obj_id;
	uint32_t	obj_addr;
};

/**
 * cpfl_rule_query_del_addr - Rule Query and Delete Address
 */
struct cpfl_rule_query_del_addr {
	uint8_t	obj_id;
	uint32_t	obj_addr;
};

/**
 * cpfl_rule_mod_content - MOD Rule Content
 */
struct cpfl_rule_mod_content {
	uint8_t	obj_size;
	uint8_t	pin_content;
	uint32_t	index;
};

/**
 * cpfl_rule_cfg_data_common - data struct for all rule opcodes
 *note: some rules may only require part of structure
 */
struct cpfl_rule_cfg_data_common {
	enum cpfl_ctlq_rule_cfg_opc opc;
	uint64_t	cookie;
	uint16_t	vsi_id;
	uint8_t	port_num;
	uint8_t	host_id;
	uint8_t	time_sel;
	uint8_t	time_sel_val;
	uint8_t	cache_wr_thru;
	uint8_t	resp_req;
	uint32_t	ret_val;
	uint16_t	buf_len;
	struct idpf_dma_mem *payload;
};

/**
 * cpfl_rule_cfg_data - rule config data
 * note: Before sending rule to HW, caller needs to fill
 *       in this struct then call cpfl_prep_rule_desc().
 */
struct cpfl_rule_cfg_data {
	struct cpfl_rule_cfg_data_common common;
	union {
		struct cpfl_rule_query_addr query_addr;
		struct cpfl_rule_query_del_addr query_del_addr;
		struct cpfl_rule_mod_content mod_content;
	} ext;
};

/**
 * cpfl_fill_rule_mod_content - fill info for mod content
 */
static inline void
cpfl_fill_rule_mod_content(uint8_t mod_obj_size,
			   uint8_t pin_mod_content,
			   uint32_t mod_index,
			   struct cpfl_rule_mod_content *mod_content)
{
	mod_content->obj_size = mod_obj_size;
	mod_content->pin_content = pin_mod_content;
	mod_content->index = mod_index;
}

/**
 * cpfl_fill_rule_cfg_data_common - fill in rule config data for all opcodes
 * note: call this function before calls cpfl_prep_rule_desc()
 */
static inline void
cpfl_fill_rule_cfg_data_common(enum cpfl_ctlq_rule_cfg_opc opc,
			       uint64_t cookie,
			       uint16_t vsi_id,
			       uint8_t port_num,
			       uint8_t host_id,
			       uint8_t time_sel,
			       uint8_t time_sel_val,
			       uint8_t cache_wr_thru,
			       uint8_t resp_req,
			       uint16_t payload_len,
			       struct idpf_dma_mem *payload,
			       struct cpfl_rule_cfg_data_common *cfg_cmn)
{
	cfg_cmn->opc = opc;
	cfg_cmn->cookie = cookie;
	cfg_cmn->vsi_id = vsi_id;
	cfg_cmn->port_num = port_num;
	cfg_cmn->resp_req = resp_req;
	cfg_cmn->ret_val = 0;
	cfg_cmn->host_id = host_id;
	cfg_cmn->time_sel = time_sel;
	cfg_cmn->time_sel_val = time_sel_val;
	cfg_cmn->cache_wr_thru = cache_wr_thru;

	cfg_cmn->buf_len = payload_len;
	cfg_cmn->payload = payload;
}

void
cpfl_prep_rule_desc(struct cpfl_rule_cfg_data *cfg_data,
		    struct idpf_ctlq_msg *ctlq_msg);

void
cpfl_prep_sem_rule_blob(const uint8_t *key,
			uint8_t key_byte_len,
			const uint8_t *act_bytes,
			uint8_t act_byte_len,
			uint16_t cfg_ctrl,
			union cpfl_rule_cfg_pkt_record *rule_blob);

#endif /* _CPFL_RULES_API_H_ */
