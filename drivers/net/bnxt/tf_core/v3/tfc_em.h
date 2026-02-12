/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_EM_H_
#define _TFC_EM_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <cfa_bld_mpcops.h>

/* Defines the maximum number of outstanding completions supported. */
#define BNXT_MPC_COMP_MAX_COUNT         64

struct tfc_mpc_comp_info_t {
	struct bnxt_mpc_txq *mpc_queue;
	struct bnxt_mpc_mbuf out_msg;
	int type;
	uint16_t read_words;
	uint16_t opaque;
	bool valid;
};

struct tfc_mpc_batch_info_t {
	bool		enabled;
	int		error;
	int		em_error;
	uint32_t	count;
	uint32_t        result[BNXT_MPC_COMP_MAX_COUNT];
	/* List of resources IDs that are to be processed during batch end */
	uint64_t	res_idx[BNXT_MPC_COMP_MAX_COUNT];
	uint64_t	em_hdl[BNXT_MPC_COMP_MAX_COUNT];
	struct tfc_mpc_comp_info_t comp_info[BNXT_MPC_COMP_MAX_COUNT];
};

enum tfc_mpc_cmd_type {
	TFC_MPC_EM_INSERT,
	TFC_MPC_EM_DELETE,
	TFC_MPC_TABLE_WRITE,
	TFC_MPC_TABLE_READ,
	TFC_MPC_TABLE_READ_CLEAR,
	TFC_MPC_INVALIDATE
};

#define TFC_EM_DYNAMIC_BUCKET_EN 0

/*
 * Derived from CAS document
 */
#define TFC_MPC_MAX_TX_BYTES 188
#define TFC_MPC_MAX_RX_BYTES 188

#define TFC_MPC_HEADER_SIZE_BYTES 16

#define TFC_MPC_BYTES_PER_WORD 32
#define TFC_MPC_MAX_TABLE_READ_WORDS 4
#define TFC_MPC_MAX_TABLE_READ_BYTES \
	(TFC_MPC_BYTES_PER_WORD * TFC_MPC_MAX_TABLE_READ_WORDS)

#define TFC_BUCKET_ENTRIES 6

/* MPC opaque currently unused */
#define TFC_MPC_OPAQUE_VAL 0

#define TFC_MOD_STRING_LENGTH  512
#define TFC_STAT_STRING_LENGTH 128
#define TFC_ENC_STRING_LENGTH  256

struct act_compact_info_t {
	bool drop;
	uint8_t vlan_del_rep;
	uint8_t dest_op;
	uint16_t vnic_vport;
	uint8_t decap_func;
	uint8_t mirror;
	uint16_t meter_ptr;
	uint8_t stat0_ctr_type;
	bool stat0_ing_egr;
	uint8_t stat0_offs;
	uint8_t mod_offs;
	uint8_t enc_offs;
	uint8_t src_offs;
	char mod_str[512];
	char stat0_str[128];
	char enc_str[256];
};

struct act_full_info_t {
	bool drop;
	uint8_t vlan_del_rep;
	uint8_t dest_op;
	uint16_t vnic_vport;
	uint8_t decap_func;
	uint16_t mirror;
	uint16_t meter_ptr;
	uint8_t stat0_ctr_type;
	bool stat0_ing_egr;
	uint32_t stat0_ptr;
	uint8_t stat1_ctr_type;
	bool stat1_ing_egr;
	uint32_t stat1_ptr;
	uint32_t mod_ptr;
	uint32_t enc_ptr;
	uint32_t src_ptr;
	char mod_str[512];
	char stat0_str[128];
	char stat1_str[128];
	char enc_str[256];
};

struct act_mcg_info_t {
	uint8_t src_ko_en;
	uint32_t nxt_ptr;
	uint8_t act_hint0;
	uint32_t act_rec_ptr0;
	uint8_t act_hint1;
	uint32_t act_rec_ptr1;
	uint8_t act_hint2;
	uint32_t act_rec_ptr2;
	uint8_t act_hint3;
	uint32_t act_rec_ptr3;
	uint8_t act_hint4;
	uint32_t act_rec_ptr4;
	uint8_t act_hint5;
	uint32_t act_rec_ptr5;
	uint8_t act_hint6;
	uint32_t act_rec_ptr6;
	uint8_t act_hint7;
	uint32_t act_rec_ptr7;
};

struct act_info_t {
	bool valid;
	uint8_t vector;
	union {
		struct act_compact_info_t compact;
		struct act_full_info_t full;
		struct act_mcg_info_t mcg;
	};
};

struct em_info_t {
	bool valid;
	uint8_t rec_size;
	uint16_t epoch0;
	uint16_t epoch1;
	uint8_t opcode;
	uint8_t strength;
	uint8_t act_hint;
	uint32_t act_rec_ptr;	/* Not FAST */
	uint32_t destination;	/* Just FAST */
	uint8_t tcp_direction;	/* Just CT */
	uint8_t tcp_update_en;
	uint8_t tcp_win;
	uint32_t tcp_msb_loc;
	uint32_t tcp_msb_opp;
	uint8_t tcp_msb_opp_init;
	uint8_t state;
	uint8_t timer_value;
	uint16_t ring_table_idx;	/* Not CT and not RECYCLE */
	uint8_t act_rec_size;
	uint8_t paths_m1;
	uint8_t fc_op;
	uint8_t fc_type;
	uint32_t fc_ptr;
	uint8_t recycle_dest;	/* Just Recycle */
	uint8_t prof_func;
	uint8_t meta_prof;
	uint32_t metadata;
	uint8_t range_profile;
	uint16_t range_index;
	uint8_t *key;
	struct act_info_t act_info;
};

struct sb_entry_t {
	uint16_t hash_msb;
	uint32_t entry_ptr;
};

struct bucket_info_t {
	bool valid;
	bool chain;
	uint32_t chain_ptr;
	struct sb_entry_t entries[TFC_BUCKET_ENTRIES];
	struct em_info_t em_info[TFC_BUCKET_ENTRIES];
};

#define CALC_NUM_RECORDS_IN_POOL(a, b, c)

/* Calculates number of 32Byte records from total size in 32bit words */
#define CALC_NUM_RECORDS(result, key_sz_words) \
	(*(result) = (((key_sz_words) + 7) / 8))

/* Calculates the entry offset */
#define CREATE_OFFSET(result, pool_sz_exp, pool_id, record_offset) \
	(*(result) = (((pool_id) << (pool_sz_exp)) | (record_offset)))

#define REMOVE_POOL_FROM_OFFSET(pool_sz_exp, record_offset) \
	(((1 << (pool_sz_exp)) - 1) & (record_offset))

int tfc_em_delete_raw(struct tfc *tfcp,
		      uint8_t tsid,
		      enum cfa_dir dir,
		      uint32_t offset,
		      uint32_t static_bucket,
		      struct tfc_mpc_batch_info_t *batch_info
#if TFC_EM_DYNAMIC_BUCKET_EN
		      , bool *db_unused,
		      uint32_t *db_offset
#endif
#ifdef BNXT_MPC_COMP_COUNT
		      , uint32_t comp_count
#endif
		      );

int tfc_mpc_table_read(struct tfc *tfcp,
		       uint8_t tsid,
		       enum cfa_dir dir,
		       uint32_t type,
		       uint32_t offset,
		       uint8_t words,
		       uint8_t *data,
		       uint8_t debug);

int tfc_em_delete_entries_by_pool_id(struct tfc *tfcp,
				     uint8_t tsid,
				     enum cfa_dir dir,
				     uint16_t pool_id,
				     uint8_t debug,
				     uint8_t *data);

int tfc_act_set_response(struct cfa_bld_mpcinfo *mpc_info,
			 struct bnxt_mpc_mbuf *mpc_msg_out,
			 uint8_t *rx_msg);

int tfc_act_get_only_response(struct cfa_bld_mpcinfo *mpc_info,
			      struct bnxt_mpc_mbuf *mpc_msg_out,
			      uint8_t *rx_msg,
			      uint16_t *data_sz_words);

int tfc_act_get_clear_response(struct cfa_bld_mpcinfo *mpc_info,
			       struct bnxt_mpc_mbuf *mpc_msg_out,
			       uint8_t *rx_msg,
			       uint16_t *data_sz_words);

int tfc_mpc_send(struct bnxt *bp,
		 struct bnxt_mpc_mbuf *in_msg,
		 struct bnxt_mpc_mbuf *out_msg,
		 uint16_t *opaque,
		 int type,
		 struct tfc_mpc_batch_info_t *batch_info);

#endif /* _TFC_EM_H_ */
