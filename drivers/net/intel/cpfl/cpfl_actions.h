/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _CPFL_ACTIONS_H_
#define _CPFL_ACTIONS_H_

#include "base/idpf_osdep.h"

#pragma pack(1)

union cpfl_action_set {
	uint32_t data;

	struct {
		uint32_t val : 24;
		uint32_t idx : 4;
		uint32_t tag : 1;
		uint32_t prec : 3;
	} set_24b_a;

	struct {
		uint32_t val : 24;
		uint32_t idx : 3;
		uint32_t tag : 2;
		uint32_t prec : 3;
	} set_24b_b;

	struct {
		uint32_t val : 16;
		uint32_t idx : 4;
		uint32_t unused : 6;
		uint32_t tag : 3;
		uint32_t prec : 3;
	} set_16b;

	struct {
		uint32_t val_a : 8;
		uint32_t val_b : 8;
		uint32_t idx_a : 4;
		uint32_t idx_b : 4;
		uint32_t tag : 5;
		uint32_t prec : 3;
	} set_8b;

	struct {
		uint32_t val : 10;
		uint32_t ena : 10;
		uint32_t idx : 4;
		uint32_t tag : 5;
		uint32_t prec : 3;
	} set_1b;

	struct {
		uint32_t val : 24;
		uint32_t tag : 5;
		uint32_t prec : 3;
	} nop;

	struct {
		uint32_t val : 24;
		uint32_t tag : 5;
		uint32_t prec : 3;
	} chained_24b;

	struct {
		uint32_t val : 24;
		uint32_t tag : 5;
		uint32_t prec : 3;
	} aux_flags;
};

struct cpfl_action_set_ext {
#define CPFL_ACTION_SET_EXT_CNT 2
	union cpfl_action_set acts[CPFL_ACTION_SET_EXT_CNT];
};

#pragma pack()

/**
 * cpfl_act_nop - Encode a NOP action
 */
static inline union cpfl_action_set
cpfl_act_nop(void)
{
	union cpfl_action_set act;

	act.data = 0;
	return act;
}

/**
 * cpfl_is_nop_action - Indicate if an action set is a NOP
 */
static inline bool
cpfl_is_nop_action(union cpfl_action_set *act)
{
	return act->data == cpfl_act_nop().data;
}

#define CPFL_MAKE_MASK32(b, s)	((((uint32_t)1 << (b)) - 1) << (s))

#define CPFL_ACT_PREC_MAX	7
#define CPFL_ACT_PREC_S		29
#define CPFL_ACT_PREC_M		CPFL_MAKE_MASK32(3, CPFL_ACT_PREC_S)
#define CPFL_ACT_PREC_SET(p)	\
	(((uint32_t)(p) << CPFL_ACT_PREC_S) & CPFL_ACT_PREC_M)
#define CPFL_ACT_PREC_CHECK(p)	((p) > 0 && (p) <= CPFL_ACT_PREC_MAX)

#define CPFL_METADATA_ID_CNT		32	/* Max number of metadata IDs */
#define CPFL_METADATA_STRUCT_MAX_SZ	128	/* Max metadata size per ID */

/*******************************************************************************
 * 1-Bit Actions
 ******************************************************************************/
#define CPFL_ACT_1B_OP_S	24
#define CPFL_ACT_1B_OP_M	CPFL_MAKE_MASK32(5, CPFL_ACT_1B_OP_S)
#define CPFL_ACT_1B_OP		((uint32_t)(0x01) << CPFL_ACT_1B_OP_S)

#define CPFL_ACT_1B_VAL_S	0
#define CPFL_ACT_1B_VAL_M	CPFL_MAKE_MASK32(10, CPFL_ACT_1B_VAL_S)
#define CPFL_ACT_1B_EN_S	10
#define CPFL_ACT_1B_EN_M	CPFL_MAKE_MASK32(10, CPFL_ACT_1B_EN_S)
#define CPFL_ACT_1B_INDEX_S	20
#define CPFL_ACT_1B_INDEX_M	CPFL_MAKE_MASK32(4, CPFL_ACT_1B_INDEX_S)

/* 1-bit actions currently uses only INDEX of 0 */
#define CPFL_ACT_MAKE_1B(prec, en, val) \
	((CPFL_ACT_PREC_SET(prec)) | CPFL_ACT_1B_OP | \
	 ((((uint32_t)0) << CPFL_ACT_1B_INDEX_S) & CPFL_ACT_1B_INDEX_M) | \
	 (((uint32_t)(en) << CPFL_ACT_1B_EN_S) & CPFL_ACT_1B_EN_M) | \
	 (((uint32_t)(val) << CPFL_ACT_1B_VAL_S) & CPFL_ACT_1B_VAL_M))

enum cpfl_act_1b_op {
	CPFL_ACT_1B_OP_DROP		= 0x01,
	CPFL_ACT_1B_OP_HDR_SPLIT	= 0x02,
	CPFL_ACT_1B_OP_DIR_CHANGE	= 0x04,
	CPFL_ACT_1B_OP_DEFER_DROP	= 0x08,
	CPFL_ACT_1B_OP_ORIG_MIR_MD	= 0x80
};

#define CPFL_ACT_1B_COMMIT_MODE_S	4
#define CPFL_ACT_1B_COMMIT_MODE_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_1B_COMMIT_MODE_S)

/**
 * cpfl_act_commit_mode - action commit mode for certain action classes
 */
enum cpfl_act_commit_mode {
	/* Action processing for the initial classification pass */
	CPFL_ACT_COMMIT_ALL		= 0, /* Commit all actions */
	CPFL_ACT_COMMIT_PRE_MOD		= 1, /* Commit only pre-modify actions*/
	CPFL_ACT_COMMIT_NONE		= 2, /* Commit no action */
	/* Action processing for deferred actions in a recirculation pass */
	CPFL_ACT_COMMIT_RECIR_ALL	= 4, /* Commit all actions */
	CPFL_ACT_COMMIT_RECIR_PRE_MOD	= 5, /* Commit only pre-modify actions*/
	CPFL_ACT_COMMIT_RECIR_NONE	= 6  /* Commit no action */
};

/*******************************************************************************
 * 8-Bit Actions
 ******************************************************************************/
#define CPFL_ACT_OP_8B_S	24
#define CPFL_ACT_OP_8B_M	CPFL_MAKE_MASK32(5, CPFL_ACT_OP_8B_S)
#define CPFL_ACT_OP_8B		((uint32_t)(0x02) << CPFL_ACT_OP_8B_S)

#define CPFL_ACT_8B_A_VAL_S	0
#define CPFL_ACT_8B_A_VAL_M	CPFL_MAKE_MASK32(8, CPFL_ACT_8B_A_VAL_S)
#define CPFL_ACT_8B_A_INDEX_S	16
#define CPFL_ACT_8B_A_INDEX_M	CPFL_MAKE_MASK32(4, CPFL_ACT_8B_A_INDEX_S)

#define CPFL_ACT_8B_B_VAL_S	8
#define CPFL_ACT_8B_B_VAL_M	CPFL_MAKE_MASK32(8, CPFL_ACT_8B_B_VAL_S)
#define CPFL_ACT_8B_B_INDEX_S	20
#define CPFL_ACT_8B_B_INDEX_M	CPFL_MAKE_MASK32(4, CPFL_ACT_8B_B_INDEX_S)

/* Unless combining two 8-bit actions into an action set, both A and B fields
 * must be the same,
 */
#define CPFL_ACT_MAKE_8B(prec, idx, val) \
	((CPFL_ACT_PREC_SET(prec)) | CPFL_ACT_OP_8B | \
	 (((idx) << CPFL_ACT_8B_A_INDEX_S) & CPFL_ACT_8B_A_INDEX_M) | \
	 (((idx) << CPFL_ACT_8B_B_INDEX_S) & CPFL_ACT_8B_B_INDEX_M) | \
	 (((val) << CPFL_ACT_8B_A_VAL_S) & CPFL_ACT_8B_A_VAL_M) | \
	 (((val) << CPFL_ACT_8B_B_VAL_S) & CPFL_ACT_8B_B_VAL_M))

/* 8-Bit Action Indices */
#define CPFL_ACT_8B_INDEX_MOD_META		9

/* 8-Bit Action Miscellaneous */
#define CPFL_ACT_8B_MOD_META_PROF_CNT		16
#define CPFL_ACT_8B_MOD_META_VALID		0x80

/*******************************************************************************
 * 16-Bit Actions
 ******************************************************************************/
#define CPFL_ACT_OP_16B_S	26
#define CPFL_ACT_OP_16B_M	CPFL_MAKE_MASK32(3, CPFL_ACT_OP_16B_S)
#define CPFL_ACT_OP_16B		((uint32_t)0x1 << CPFL_ACT_OP_16B_S)

#define CPFL_ACT_16B_INDEX_S	16
#define CPFL_ACT_16B_INDEX_M	CPFL_MAKE_MASK32(4, CPFL_ACT_16B_INDEX_S)
#define CPFL_ACT_16B_VAL_S	0
#define CPFL_ACT_16B_VAL_M	CPFL_MAKE_MASK32(16, CPFL_ACT_16B_VAL_S)

#define CPFL_ACT_MAKE_16B(prec, idx, val) \
	((CPFL_ACT_PREC_SET(prec)) | CPFL_ACT_OP_16B | \
	 (((uint32_t)(idx) << CPFL_ACT_16B_INDEX_S) & CPFL_ACT_16B_INDEX_M) | \
	 (((uint32_t)(val) << CPFL_ACT_16B_VAL_S) & CPFL_ACT_16B_VAL_M))

/* 16-Bit Action Indices */
#define CPFL_ACT_16B_INDEX_COUNT_SET		0
#define CPFL_ACT_16B_INDEX_SET_MCAST_IDX	1
#define CPFL_ACT_16B_INDEX_SET_VSI		2
#define CPFL_ACT_16B_INDEX_DEL_MD		4
#define CPFL_ACT_16B_INDEX_MOD_VSI_LIST		5

/* 16-Bit Action Miscellaneous */
#define CPFL_ACT_16B_COUNT_SET_CNT		2048 /* TODO: Value from NSL */
#define CPFL_ACT_16B_SET_VSI_SLOTS		2
#define CPFL_ACT_16B_FWD_VSI_CNT		1032 /* TODO: Value from NSL */
#define CPFL_ACT_16B_FWD_VSI_LIST_CNT		256
#define CPFL_ACT_16B_MOD_VSI_LIST_CNT		1024
#define CPFL_ACT_16B_FWD_PORT_CNT		4
#define CPFL_ACT_16B_DEL_MD_MID_CNT		32
#define CPFL_ACT_16B_MOD_VSI_LIST_SLOTS		4

/* 16-Bit SET_MCAST_IDX Action */
#define CPFL_ACT_16B_SET_MCAST_VALID	((uint32_t)1 << 15)

/* 16-Bit SET_VSI Action Variants */
#define CPFL_ACT_16B_SET_VSI_VAL_S		0
#define CPFL_ACT_16B_SET_VSI_VAL_M		\
	CPFL_MAKE_MASK32(11, CPFL_ACT_16B_SET_VSI_VAL_S)
#define CPFL_ACT_16B_SET_VSI_PE_S		11
#define CPFL_ACT_16B_SET_VSI_PE_M		\
	CPFL_MAKE_MASK32(2, CPFL_ACT_16B_SET_VSI_PE_S)
#define CPFL_ACT_16B_SET_VSI_TYPE_S		14
#define CPFL_ACT_16B_SET_VSI_TYPE_M		\
	CPFL_MAKE_MASK32(2, CPFL_ACT_16B_SET_VSI_TYPE_S)

/* 16-Bit DEL_MD Action */
#define CPFL_ACT_16B_DEL_MD_0_S		0
#define CPFL_ACT_16B_DEL_MD_1_S		5

/* 16-Bit MOD_VSI_LIST Actions */
#define CPFL_ACT_16B_MOD_VSI_LIST_ID_S	0
#define CPFL_ACT_16B_MOD_VSI_LIST_ID_M	\
	CPFL_MAKE_MASK32(10, CPFL_ACT_16B_MOD_VSI_LIST_ID_S)
#define CPFL_ACT_16B_MOD_VSI_LIST_OP_S	14
#define CPFL_ACT_16B_MOD_VSI_LIST_OP_M	\
	CPFL_MAKE_MASK32(2, CPFL_ACT_16B_MOD_VSI_LIST_OP_S)
#define CPFL_MAKE_16B_MOD_VSI_LIST(op, id) \
	((((uint32_t)(op) << CPFL_ACT_16B_MOD_VSI_LIST_OP_S) & \
		CPFL_ACT_16B_MOD_VSI_LIST_OP_M) | \
	 (((uint32_t)(id) << CPFL_ACT_16B_MOD_VSI_LIST_ID_S) & \
		CPFL_ACT_16B_MOD_VSI_LIST_ID_M))

#define CPFL_ACT_16B_MAKE_SET_VSI(type, pe, val) \
	((((uint32_t)(type) << CPFL_ACT_16B_SET_VSI_TYPE_S) & \
		CPFL_ACT_16B_SET_VSI_TYPE_M) | \
	 (((uint32_t)(pe) << CPFL_ACT_16B_SET_VSI_PE_S) & \
		CPFL_ACT_16B_SET_VSI_PE_M) | \
	 (((uint32_t)(val) << CPFL_ACT_16B_SET_VSI_VAL_S) & \
		CPFL_ACT_16B_SET_VSI_VAL_M))

enum cpfl_prot_eng {
	CPFL_PE_LAN = 0,
	CPFL_PE_RDMA,
	CPFL_PE_CRT
};

enum cpfl_act_fwd_type {
	CPFL_ACT_FWD_VSI,
	CPFL_ACT_FWD_VSI_LIST,
	CPFL_ACT_FWD_PORT
};

/*******************************************************************************
 * 24-Bit Actions
 ******************************************************************************/
/* Group A */
#define CPFL_ACT_OP_24B_A_S	28
#define CPFL_ACT_OP_24B_A_M	CPFL_MAKE_MASK32(1, CPFL_ACT_OP_24B_A_S)
#define CPFL_ACT_24B_A_INDEX_S	24
#define CPFL_ACT_24B_A_INDEX_M	CPFL_MAKE_MASK32(4, CPFL_ACT_24B_A_INDEX_S)
#define CPFL_ACT_24B_A_VAL_S	0
#define CPFL_ACT_24B_A_VAL_M	CPFL_MAKE_MASK32(24, CPFL_ACT_24B_A_VAL_S)

#define CPFL_ACT_OP_24B_A	((uint32_t)1 << CPFL_ACT_OP_24B_A_S)

#define CPFL_ACT_MAKE_24B_A(prec, idx, val) \
	((CPFL_ACT_PREC_SET(prec)) | CPFL_ACT_OP_24B_A | \
	 (((uint32_t)(idx) << CPFL_ACT_24B_A_INDEX_S) & CPFL_ACT_24B_A_INDEX_M) | \
	 (((uint32_t)(val) << CPFL_ACT_24B_A_VAL_S) & CPFL_ACT_24B_A_VAL_M))

#define CPFL_ACT_24B_INDEX_MOD_ADDR	0
#define CPFL_ACT_24B_INDEX_MIRROR_FIRST	1
#define CPFL_ACT_24B_INDEX_COUNT	2
#define CPFL_ACT_24B_INDEX_SET_Q	8
#define CPFL_ACT_24B_INDEX_MOD_PROFILE	9
#define CPFL_ACT_24B_INDEX_METER	10

#define CPFL_ACT_24B_COUNT_SLOTS	6
#define CPFL_ACT_24B_METER_SLOTS	6

#define CPFL_ACT_24B_MOD_ADDR_CNT	(16 * 1024 * 1024)
#define CPFL_ACT_24B_COUNT_ID_CNT	((uint32_t)1 << 24)
#define CPFL_ACT_24B_SET_Q_CNT		(12 * 1024)
#define CPFL_ACT_24B_SET_Q_Q_RGN_BITS	3

/* 24-Bit SET_Q Action */
#define CPFL_ACT_24B_SET_Q_Q_S		0
#define CPFL_ACT_24B_SET_Q_Q_M		\
	CPFL_MAKE_MASK32(14, CPFL_ACT_24B_SET_Q_Q_S)
#define CPFL_ACT_24B_SET_Q_Q_RGN_S	14
#define CPFL_ACT_24B_SET_Q_Q_RGN_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_SET_Q_Q_RGN_S)
#define CPFL_ACT_24B_SET_Q_IMPLICIT_VSI_DIS	CPFL_MAKE_MASK32(1, 17)
#define CPFL_ACT_24B_SET_Q_DST_PE_S	21
#define CPFL_ACT_24B_SET_Q_DST_PE_M	\
	CPFL_MAKE_MASK32(2, CPFL_ACT_24B_SET_Q_DST_PE_S)
#define CPFL_ACT_24B_SET_Q_VALID	CPFL_MAKE_MASK32(1, 23)

/* 24-Bit MOD_PROFILE Action */
enum cpfl_act_mod_profile_hint {
	CPFL_ACT_MOD_PROFILE_NO_ADDR = 0, /* No associated MOD_ADDR action */
	CPFL_ACT_MOD_PROFILE_PREFETCH_128B, /* Prefetch 128B using MOD_ADDR */
	CPFL_ACT_MOD_PROFILE_PREFETCH_256B, /* Prefetch 256B using MOD_ADDR */
};

#define CPFL_ACT_24B_MOD_PROFILE_PROF_S		0
#define CPFL_ACT_24B_MOD_PROFILE_PROF_M		\
	CPFL_MAKE_MASK32(11, CPFL_ACT_24B_MOD_PROFILE_PROF_S)
#define CPFL_ACT_24B_MOD_PROFILE_XTLN_IDX_S	12
#define CPFL_ACT_24B_MOD_PROFILE_XTLN_IDX_M	\
	CPFL_MAKE_MASK32(2, CPFL_ACT_24B_MOD_PROFILE_XTLN_IDX_S)
#define CPFL_ACT_24B_MOD_PROFILE_HINT_S		14
#define CPFL_ACT_24B_MOD_PROFILE_HINT_M		\
	CPFL_MAKE_MASK32(2, CPFL_ACT_24B_MOD_PROFILE_HINT_S)
#define CPFL_ACT_24B_MOD_PROFILE_APPEND_ACT_BUS		((uint32_t)1 << 16)
#define CPFL_ACT_24B_MOD_PROFILE_SET_MISS_PREPEND	((uint32_t)1 << 17)
#define CPFL_ACT_24B_MOD_PROFILE_VALID			((uint32_t)1 << 23)

#define CPFL_ACT_24B_MOD_PROFILE_PTYPE_XLTN_INDEXES	4
#define CPFL_ACT_24B_MOD_PROFILE_PROF_CNT		2048

/* 24-Bit METER Actions */
#define CPFL_ACT_24B_METER_INDEX_S	0
#define CPFL_ACT_24B_METER_INDEX_M	\
	CPFL_MAKE_MASK32(20, CPFL_ACT_24B_METER_INDEX_S)
#define CPFL_ACT_24B_METER_BANK_S	20
#define CPFL_ACT_24B_METER_BANK_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_METER_BANK_S)
#define CPFL_ACT_24B_METER_VALID	((uint32_t)1 << 23)

#define CPFL_ACT_24B_METER_BANK_CNT	6
#define CPFL_ACT_24B_METER_INDEX_CNT	((uint32_t)1 << 20)

/* Group B */
#define CPFL_ACT_OP_24B_B_S	27
#define CPFL_ACT_OP_24B_B_M	CPFL_MAKE_MASK32(2, CPFL_ACT_OP_24B_B_S)
#define CPFL_ACT_24B_B_INDEX_S	24
#define CPFL_ACT_24B_B_INDEX_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_B_INDEX_S)
#define CPFL_ACT_24B_B_VAL_S	0
#define CPFL_ACT_24B_B_VAL_M	CPFL_MAKE_MASK32(24, CPFL_ACT_24B_B_VAL_S)

#define CPFL_ACT_OP_24B_B	((uint32_t)1 << CPFL_ACT_OP_24B_B_S)

#define CPFL_ACT_MAKE_24B_B(prec, idx, val) \
	((CPFL_ACT_PREC_SET(prec)) | CPFL_ACT_OP_24B_B | \
	 (((uint32_t)(idx) << CPFL_ACT_24B_B_INDEX_S) & CPFL_ACT_24B_B_INDEX_M) | \
	 (((uint32_t)(val) << CPFL_ACT_24B_B_VAL_S) & CPFL_ACT_24B_B_VAL_M))

#define CPFL_ACT_24B_INDEX_SET_MD	0
#define CPFL_ACT_24B_INDEX_RANGE_CHECK	6
#define CPFL_ACT_24B_SET_MD_SLOTS	6

/* Set/Add/Delete Metadata Actions - SET_MD[0-5], DEL_MD */
/* 8-Bit SET_MD */
#define CPFL_ACT_24B_SET_MD8_VAL_S	0
#define CPFL_ACT_24B_SET_MD8_VAL_M	\
	CPFL_MAKE_MASK32(8, CPFL_ACT_24B_SET_MD8_VAL_S)
#define CPFL_ACT_24B_SET_MD8_MASK_S	8
#define CPFL_ACT_24B_SET_MD8_MASK_M	\
	CPFL_MAKE_MASK32(8, CPFL_ACT_24B_SET_MD8_MASK_S)
#define CPFL_ACT_24B_SET_MD8_OFFSET_S	16
#define CPFL_ACT_24B_SET_MD8_OFFSET_M	\
	CPFL_MAKE_MASK32(4, CPFL_ACT_24B_SET_MD8_OFFSET_S)
#define CPFL_ACT_24B_SET_MD8_TYPE_ID_S	20
#define CPFL_ACT_24B_SET_MD8_TYPE_ID_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_SET_MD8_TYPE_ID_S)
/* 16-Bit SET_MD */
#define CPFL_ACT_24B_SET_MD16_VAL_S	0
#define CPFL_ACT_24B_SET_MD16_VAL_M	\
	CPFL_MAKE_MASK32(16, CPFL_ACT_24B_SET_MD16_VAL_S)
#define CPFL_ACT_24B_SET_MD16_MASK_L_S	16 /* For chained action */
#define CPFL_ACT_24B_SET_MD16_MASK_L_M	\
	CPFL_MAKE_MASK32(8, CPFL_ACT_24B_SET_MD16_MASK_L_S)
#define CPFL_ACT_24B_SET_MD16_MASK_H_SR	8
#define CPFL_ACT_24B_SET_MD16_MASK_H_M	0xff
#define CPFL_ACT_24B_SET_MD16_OFFSET_S	16
#define CPFL_ACT_24B_SET_MD16_OFFSET_M	\
	CPFL_MAKE_MASK32(4, CPFL_ACT_24B_SET_MD16_OFFSET_S)
#define CPFL_ACT_24B_SET_MD16_TYPE_ID_S	20
#define CPFL_ACT_24B_SET_MD16_TYPE_ID_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_SET_MD16_TYPE_ID_S)
#define CPFL_ACT_24B_SET_MD16		((uint32_t)1 << 23)

#define CPFL_ACT_24B_SET_MD32_VAL_L_M	CPFL_MAKE_MASK32(24, 0)

#define CPFL_ACT_24B_SET_MD8_OFFSET_MAX		15
#define CPFL_ACT_24B_SET_MD8_TYPE_ID_MAX	7
#define CPFL_ACT_24B_SET_MD16_OFFSET_MAX	15
#define CPFL_ACT_24B_SET_MD16_TYPE_ID_MAX	7

/* RANGE_CHECK Action */
enum cpfl_rule_act_rc_mode {
	CPFL_RULE_ACT_RC_1_RANGE = 0,
	CPFL_RULE_ACT_RC_2_RANGES = 1,
	CPFL_RULE_ACT_RC_4_RANGES = 2,
	CPFL_RULE_ACT_RC_8_RANGES = 3
};

#define CPFL_ACT_24B_RC_TBL_IDX_S	0
#define CPFL_ACT_24B_RC_TBL_IDX_M	\
	CPFL_MAKE_MASK32(13, CPFL_ACT_24B_RC_TBL_IDX_S)
#define CPFL_ACT_24B_RC_START_BANK_S	13
#define CPFL_ACT_24B_RC_START_BANK_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_24B_RC_START_BANK_S)
#define CPFL_ACT_24B_RC_MODE_S		16
#define CPFL_ACT_24B_RC_MODE_M		\
	CPFL_MAKE_MASK32(2, CPFL_ACT_24B_RC_MODE_S)
#define CPFL_ACT_24B_RC_XTRACT_PROF_S	18
#define CPFL_ACT_24B_RC_XTRACT_PROF_M	\
	CPFL_MAKE_MASK32(6, CPFL_ACT_24B_RC_XTRACT_PROF_S)

#define CPFL_ACT_24B_RC_TBL_INDEX_CNT	(8 * 1024)
#define CPFL_ACT_24B_RC_BANK_CNT	8
#define CPFL_ACT_24B_RC_XTRACT_PROF_CNT	64

/*******************************************************************************
 * 24-Bit Chained Auxiliary Actions
 ******************************************************************************/

/* TODO: HAS is being updated.  Revise the order of chained and base action
 * when the HAS has it finalized.
 */
/**
 * 24-Bit Chained SET_MD Actions
 *
 * Chained SET_MD actions consume two consecutive action sets.  The first one is
 * the chained AUX action set.  The second one is the base/parent action set.
 * Chained SET_MD actions can add and/or update metadata structure with IDs from
 * 0 to 31 while the non-chained SET_MD variants can only update existing meta-
 * data IDs below 16.
 */

#define CPFL_ACT_24B_SET_MD_AUX_OFFSET_S	8
#define CPFL_ACT_24B_SET_MD_AUX_OFFSET_M	\
	CPFL_MAKE_MASK32(7, CPFL_ACT_24B_SET_MD_AUX_OFFSET_S)
#define CPFL_ACT_24B_SET_MD_AUX_ADD		((uint32_t)1 << 15)
#define CPFL_ACT_24B_SET_MD_AUX_TYPE_ID_S	16
#define CPFL_ACT_24B_SET_MD_AUX_TYPE_ID_M	\
	CPFL_MAKE_MASK32(5, CPFL_ACT_24B_SET_MD_AUX_TYPE_ID_S)
#define CPFL_ACT_24B_SET_MD_AUX_DATA_S		0
#define CPFL_ACT_24B_SET_MD_AUX_DATA_M		\
	CPFL_MAKE_MASK32(8, CPFL_ACT_24B_SET_MD_AUX_DATA_S)

#define CPFL_ACT_24B_SET_MD_AUX_16B_MASK_H_S	0
#define CPFL_ACT_24B_SET_MD_AUX_16B_MASK_H_M	\
	CPFL_MAKE_MASK32(8, CPFL_ACT_24B_SET_MD_AUX_16B_MASK_H_S)
#define CPFL_ACT_24B_SET_MD_AUX_32B_VAL_H_SR	24 /* Upper 8 bits of MD32 */
#define CPFL_ACT_24B_SET_MD_AUX_32B_VAL_H_M	0xff

#define CPFL_ACT_TYPE_CHAIN_DATA_S	29
#define CPFL_ACT_TYPE_CHAIN_DATA_M	\
	CPFL_MAKE_MASK32(3, CPFL_ACT_TYPE_CHAIN_DATA_S)
#define CPFL_ACT_TYPE_CHAIN_DATA	((uint32_t)1 << CPFL_ACT_TYPE_CHAIN_DATA_S)

#define CPFL_ACT_24B_SET_MD_OP_S	21
#define CPFL_ACT_24B_SET_MD_OP_8B	((uint32_t)0 << CPFL_ACT_24B_SET_MD_OP_S)
#define CPFL_ACT_24B_SET_MD_OP_16B	((uint32_t)1 << CPFL_ACT_24B_SET_MD_OP_S)
#define CPFL_ACT_24B_SET_MD_OP_32B	((uint32_t)2 << CPFL_ACT_24B_SET_MD_OP_S)

#define CPFL_ACT_24B_SET_MD_AUX_MAKE(op, mid, off, data) \
	(CPFL_ACT_TYPE_CHAIN_DATA | (op) | \
	 (((uint32_t)(mid) << CPFL_ACT_24B_SET_MD_AUX_TYPE_ID_S) & \
		CPFL_ACT_24B_SET_MD_AUX_TYPE_ID_M) | \
	 (((uint32_t)(off) << CPFL_ACT_24B_SET_MD_AUX_OFFSET_S) & \
		CPFL_ACT_24B_SET_MD_AUX_OFFSET_M) | \
	 (((uint32_t)(data) << CPFL_ACT_24B_SET_MD_AUX_DATA_S) & \
		CPFL_ACT_24B_SET_MD_AUX_DATA_M))

/*******************************************************************************
 * 1-Bit Action Factory
 ******************************************************************************/

/**
 * cpfl_act_drop - Encode a 1-bit DROP action
 *
 * The DROP action has precedence over the DEFER_DOP action.
 * Affect of ACT_COMMIT action on the DROP action:
 *  - CPFL_ACT_COMMIT_ALL: Packet is dropped.
 *  - CPFL_ACT_COMMIT_PRE_MOD or CPFL_ACT_COMMIT_NONE: Packet is not dropped.
 *  - CPFL_ACT_COMMIT_RECIR_ALL: Packet is dropped.  Recirculation is canceled.
 *  - CPFL_ACT_COMMIT_RECIR_PRE_MOD or CPFL_ACT_COMMIT_RECIR_NONE: Packet is not
 *    dropped. Recirculation continues.
 *
 * Once a DROP action is set, it cannot be reverted during the classification
 * process of a network packet.
 */
static inline union cpfl_action_set
cpfl_act_drop(uint8_t prec)
{
	union cpfl_action_set a;

	if (!CPFL_ACT_PREC_CHECK(prec))
		return cpfl_act_nop();
	a.data = CPFL_ACT_MAKE_1B(prec, CPFL_ACT_1B_OP_DROP, 1);
	return a;
}

/**
 * cpfl_act_set_commit_mode - Encode a 1-bit ACT_COMMIT action
 * An ACT_COMMIT action specifies if and when all actions are committed.
 */
static inline union cpfl_action_set
cpfl_act_set_commit_mode(uint8_t prec, enum cpfl_act_commit_mode mode)
{
	union cpfl_action_set a;

	if (!CPFL_ACT_PREC_CHECK(prec))
		return cpfl_act_nop();
	a.data = CPFL_ACT_MAKE_1B(prec, CPFL_ACT_1B_COMMIT_MODE_M,
				  (uint32_t)mode << CPFL_ACT_1B_COMMIT_MODE_S);
	return a;
}

/*******************************************************************************
 * 8-Bit Action Factory
 ******************************************************************************/

/**
 * cpfl_act_mod_meta - Encode an 8-bit MOD_META action
 */
static inline union cpfl_action_set
cpfl_act_mod_meta(uint8_t prec, uint8_t prof)
{
	union cpfl_action_set a;

	if (!CPFL_ACT_PREC_CHECK(prec) || prof >= CPFL_ACT_8B_MOD_META_PROF_CNT)
		return cpfl_act_nop();

	a.data = CPFL_ACT_MAKE_8B(prec, CPFL_ACT_8B_INDEX_MOD_META,
				  CPFL_ACT_8B_MOD_META_VALID | prof);

	return a;
}

/*******************************************************************************
 * 16-Bit Action Factory
 ******************************************************************************/

/**
 * cpfl_act_fwd_vsi - Encode a 16-bit SET_VSI action (forward to a VSI)
 *
 * This encodes the "Forward to Single VSI" variant of SET_VSI action.
 * SEM can use both SET_VSI action slots.  The other classification blocks can
 * only use slot 0.
 */
static inline union cpfl_action_set
cpfl_act_fwd_vsi(uint8_t slot, uint8_t prec, enum cpfl_prot_eng pe, uint16_t vsi)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) || slot >= CPFL_ACT_16B_SET_VSI_SLOTS ||
	    vsi >= CPFL_ACT_16B_FWD_VSI_CNT)
		return cpfl_act_nop();

	val = CPFL_ACT_16B_MAKE_SET_VSI(CPFL_ACT_FWD_VSI, pe, vsi);
	a.data = CPFL_ACT_MAKE_16B(prec, CPFL_ACT_16B_INDEX_SET_VSI + slot,
				   val);

	return a;
}

/**
 * cpfl_act_fwd_port - Encode a 16-bit SET_VSI action (forward to a port)
 *
 * This encodes the "Forward to a port" variant of SET_VSI action.
 * SEM can use both SET_VSI action slots.  The other classification blocks can
 * only use slot 0.
 */
static inline union cpfl_action_set
cpfl_act_fwd_port(uint8_t slot, uint8_t prec, enum cpfl_prot_eng pe, uint8_t port)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) || slot >= CPFL_ACT_16B_SET_VSI_SLOTS ||
	    port >= CPFL_ACT_16B_FWD_PORT_CNT)
		return cpfl_act_nop();

	val = CPFL_ACT_16B_MAKE_SET_VSI(CPFL_ACT_FWD_PORT, pe, port);
	a.data = CPFL_ACT_MAKE_16B(prec, CPFL_ACT_16B_INDEX_SET_VSI + slot,
				   val);

	return a;
}

/*******************************************************************************
 * 24-Bit Action Factory
 ******************************************************************************/

/**
 * cpfl_act_mod_addr - Encode a 24-bit MOD_ADDR action
 *
 * This MOD_ADDR specifies the index of the MOD content entry an accompanying
 * MOD_PROFILE action uses.  Some MOD_PROFILE actions may need to use extra
 * information from a Modify content entry, and requires an accompanying
 * MOD_ADDR action.
 */
static inline union cpfl_action_set
cpfl_act_mod_addr(uint8_t prec, uint32_t mod_addr)
{
	union cpfl_action_set a;

	if (!CPFL_ACT_PREC_CHECK(prec) || mod_addr >= CPFL_ACT_24B_MOD_ADDR_CNT)
		return cpfl_act_nop();

	a.data = CPFL_ACT_MAKE_24B_A(prec, CPFL_ACT_24B_INDEX_MOD_ADDR,
				     mod_addr);

	return a;
}

/**
 * cpfl_act_set_hash_queue - Encode a 24-bit SET_Q action (one queue variant)
 *
 * This action is a "Forward to a single queue" variant of the SET_Q action.
 *
 * SEM performs Implicit VSI for SET_Q action when "no_impliciti_vsi" is false.
 * WCM and LEM never perform Implicit VSI for SET_Q actions.
 */
static inline union cpfl_action_set
cpfl_act_set_hash_queue(uint8_t prec, enum cpfl_prot_eng pe, uint16_t q,
			bool no_implicit_vsi)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) || q >= CPFL_ACT_24B_SET_Q_CNT)
		return cpfl_act_nop();

	val = CPFL_ACT_24B_SET_Q_VALID | (uint32_t)q |
		(((uint32_t)pe << CPFL_ACT_24B_SET_Q_DST_PE_S) &
			CPFL_ACT_24B_SET_Q_DST_PE_M);
	if (no_implicit_vsi)
		val |= CPFL_ACT_24B_SET_Q_IMPLICIT_VSI_DIS;
	a.data = CPFL_ACT_MAKE_24B_A(prec, CPFL_ACT_24B_INDEX_SET_Q, val);

	return a;
}

/**
 * cpfl_act_set_hash_queue_region - Encode a 24-bit SET_Q action (queue region)
 *
 * This action is a "Forward to a queue region" variant of the SET_Q action.
 *
 * SEM performs Implicit VSI for SET_Q action when "no_impliciti_vsi" is false.
 * WCM and LEM never perform Implicit VSI for SET_Q actions.
 */
static inline union cpfl_action_set
cpfl_act_set_hash_queue_region(uint8_t prec, enum cpfl_prot_eng pe, uint16_t q_base,
			       uint8_t q_rgn_bits, bool no_implicit_vsi)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) || q_base >= CPFL_ACT_24B_SET_Q_CNT ||
	    q_rgn_bits > CPFL_ACT_24B_SET_Q_Q_RGN_BITS)
		return cpfl_act_nop();

	val = CPFL_ACT_24B_SET_Q_VALID | (uint32_t)q_base |
		((uint32_t)q_rgn_bits << CPFL_ACT_24B_SET_Q_Q_RGN_S) |
		(((uint32_t)pe << CPFL_ACT_24B_SET_Q_DST_PE_S) &
			CPFL_ACT_24B_SET_Q_DST_PE_M);
	if (no_implicit_vsi)
		val |= CPFL_ACT_24B_SET_Q_IMPLICIT_VSI_DIS;
	a.data = CPFL_ACT_MAKE_24B_A(prec, CPFL_ACT_24B_INDEX_SET_Q, val);

	return a;
}

/**
 * cpfl_act_mod_profile - Encode a 24-bit MOD_PROFILE action
 *
 * This action specifies a Modify profile to use for modifying the network
 * packet being classified.  In addition, it also provides a hint to whether
 * or not an accompanied MOD_ADDR action is expected and should be prefetched.
 *
 * There is only one MOD_PROFILE action slot.  If multiple classification blocks
 * emit this action, the precedence value and auxiliary precedence value will be
 * used to select one with higher precedence.
 */
static inline union cpfl_action_set
cpfl_act_mod_profile(uint8_t prec, uint16_t prof, uint8_t ptype_xltn_idx, bool append_act_bus,
		     bool miss_prepend, enum cpfl_act_mod_profile_hint hint)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) ||
	    prof >= CPFL_ACT_24B_MOD_PROFILE_PROF_CNT ||
	    ptype_xltn_idx >= CPFL_ACT_24B_MOD_PROFILE_PTYPE_XLTN_INDEXES)
		return cpfl_act_nop();

	val = CPFL_ACT_24B_MOD_PROFILE_VALID |
		(((uint32_t)hint << CPFL_ACT_24B_MOD_PROFILE_HINT_S) &
			CPFL_ACT_24B_MOD_PROFILE_HINT_M) |
		(((uint32_t)ptype_xltn_idx << CPFL_ACT_24B_MOD_PROFILE_XTLN_IDX_S) &
			CPFL_ACT_24B_MOD_PROFILE_XTLN_IDX_M) |
		((uint32_t)prof << CPFL_ACT_24B_MOD_PROFILE_PROF_S);
	if (append_act_bus)
		val |= CPFL_ACT_24B_MOD_PROFILE_APPEND_ACT_BUS;
	if (miss_prepend)
		val |= CPFL_ACT_24B_MOD_PROFILE_SET_MISS_PREPEND;

	a.data = CPFL_ACT_MAKE_24B_A(prec, CPFL_ACT_24B_INDEX_MOD_PROFILE, val);

	return a;
}

/**
 * cpfl_act_meter - Encode a 24-bit METER action
 *
 * Return NOP if any given input parameter is invalid.
 *
 * A bank can only be used by one of the METER action slots.  If multiple METER
 * actions select the same bank, the action with the highest action slot wins.
 * In Policer mode, METER actions at the higher indexes have precedence over
 * ones at lower indexes.
 */
static inline union cpfl_action_set
cpfl_act_meter(uint8_t slot, uint8_t prec, uint32_t idx, uint8_t bank)
{
	union cpfl_action_set a;
	uint32_t val;

	if (!CPFL_ACT_PREC_CHECK(prec) || slot >= CPFL_ACT_24B_METER_SLOTS  ||
	    idx >= CPFL_ACT_24B_METER_INDEX_CNT ||
	    bank >= CPFL_ACT_24B_METER_BANK_CNT)
		return cpfl_act_nop();

	val = CPFL_ACT_24B_METER_VALID |
		(uint32_t)idx << CPFL_ACT_24B_METER_INDEX_S |
		(uint32_t)bank << CPFL_ACT_24B_METER_BANK_S;
	a.data = CPFL_ACT_MAKE_24B_A(prec, CPFL_ACT_24B_INDEX_METER + slot,
				     val);

	return a;
}

/**
 * cpfl_act_set_md8 - Encode a 24-bit SET_MD/8 action for an action slot
 *
 * This SET_MD action sets/updates a byte of a given metadata ID structure
 * using one of the SET_MD action slots.  This action variant can only set
 * one the first 16 bytes of any of the first 7 metadata types.
 */
static inline union cpfl_action_set
cpfl_act_set_md8(uint8_t slot, uint8_t prec, uint8_t mid, uint8_t off, uint8_t val, uint8_t mask)
{
	union cpfl_action_set a;
	uint32_t tmp;

	if (!CPFL_ACT_PREC_CHECK(prec) || slot >= CPFL_ACT_24B_SET_MD_SLOTS ||
	    mid > CPFL_ACT_24B_SET_MD8_TYPE_ID_MAX ||
	    off > CPFL_ACT_24B_SET_MD8_OFFSET_MAX)
		return cpfl_act_nop();

	tmp = ((uint32_t)mid << CPFL_ACT_24B_SET_MD8_TYPE_ID_S) |
		((uint32_t)off << CPFL_ACT_24B_SET_MD8_OFFSET_S) |
		((uint32_t)mask << CPFL_ACT_24B_SET_MD8_MASK_S) |
		((uint32_t)val << CPFL_ACT_24B_SET_MD8_VAL_S);
	a.data = CPFL_ACT_MAKE_24B_B(prec, CPFL_ACT_24B_INDEX_SET_MD + slot,
				     tmp);

	return a;
}

/**
 * cpfl_act_set_md16 - Encode a 24-bit SET_MD/16 action for an action slot
 *
 * This SET_MD action sets/updates a word of a given metadata ID structure
 * using one of the SET_MD action slots.  This action variant can only set
 * one the first 16 words of any of the first 7 metadata types.
 */
static inline union cpfl_action_set
cpfl_act_set_md16(uint8_t slot, uint8_t prec, uint8_t mid, uint8_t word_off, uint16_t val)
{
	union cpfl_action_set a;
	uint32_t tmp;

	if (!CPFL_ACT_PREC_CHECK(prec) || slot >= CPFL_ACT_24B_SET_MD_SLOTS ||
	    mid > CPFL_ACT_24B_SET_MD16_TYPE_ID_MAX ||
	    word_off > CPFL_ACT_24B_SET_MD16_OFFSET_MAX)
		return cpfl_act_nop();

	tmp = ((uint32_t)CPFL_ACT_24B_SET_MD16) |
		((uint32_t)mid << CPFL_ACT_24B_SET_MD16_TYPE_ID_S) |
		((uint32_t)word_off << CPFL_ACT_24B_SET_MD16_OFFSET_S) |
		((uint32_t)val << CPFL_ACT_24B_SET_MD16_VAL_S);
	a.data = CPFL_ACT_MAKE_24B_B(prec, CPFL_ACT_24B_INDEX_SET_MD + slot,
				     tmp);

	return a;
}

/**
 * cpfl_act_set_md32_ext - Encode a 24-bit SET_MD/32 action for an action slot
 *
 * This SET_MD action sets/updates a dword of a given metadata ID structure
 * using one of the SET_MD action slots.  This action is made up of 2 chained
 * action sets.  The chained action set is the first.  The base/parent action
 * sets is the second.
 */
static inline void
cpfl_act_set_md32_ext(struct cpfl_action_set_ext *ext, uint8_t slot, uint8_t prec, uint8_t mid,
		      uint8_t off, uint32_t val)
{
	if (slot >= CPFL_ACT_24B_SET_MD_SLOTS || !CPFL_ACT_PREC_CHECK(prec) ||
	    mid >= CPFL_METADATA_ID_CNT ||
	    (off + sizeof(uint32_t)) > CPFL_METADATA_STRUCT_MAX_SZ) {
		ext->acts[0] = cpfl_act_nop();
		ext->acts[1] = cpfl_act_nop();
	} else {
		uint32_t tmp;

		/* Chained action set comes first */
		tmp = val >> CPFL_ACT_24B_SET_MD_AUX_32B_VAL_H_SR;
		ext->acts[0].data =
			CPFL_ACT_24B_SET_MD_AUX_MAKE(CPFL_ACT_24B_SET_MD_OP_32B,
						     mid, off, tmp);

		/* Lower 24 bits of value */
		tmp = val & CPFL_ACT_24B_SET_MD32_VAL_L_M;
		ext->acts[1].data =
			CPFL_ACT_MAKE_24B_B(prec,
					    CPFL_ACT_24B_INDEX_SET_MD + slot,
					    tmp);
	}
}

#endif /* _CPFL_ACTIONS_H_ */
