/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PIPELINE_INTERNAL_H__
#define __INCLUDE_RTE_SWX_PIPELINE_INTERNAL_H__

#include <inttypes.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_meter.h>

#include <rte_swx_table_selector.h>
#include <rte_swx_table_learner.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>

#ifndef TRACE_LEVEL
#define TRACE_LEVEL 0
#endif

#if TRACE_LEVEL
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

/*
 * Environment.
 */
#define ntoh64(x) rte_be_to_cpu_64(x)
#define hton64(x) rte_cpu_to_be_64(x)

/*
 * Struct.
 */
struct field {
	char name[RTE_SWX_NAME_SIZE];
	uint32_t n_bits;
	uint32_t offset;
	int var_size;
};

struct struct_type {
	TAILQ_ENTRY(struct_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct field *fields;
	uint32_t n_fields;
	uint32_t n_bits;
	uint32_t n_bits_min;
	int var_size;
};

TAILQ_HEAD(struct_type_tailq, struct_type);

/*
 * Input port.
 */
struct port_in_type {
	TAILQ_ENTRY(port_in_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct rte_swx_port_in_ops ops;
};

TAILQ_HEAD(port_in_type_tailq, port_in_type);

struct port_in {
	TAILQ_ENTRY(port_in) node;
	struct port_in_type *type;
	void *obj;
	uint32_t id;
};

TAILQ_HEAD(port_in_tailq, port_in);

struct port_in_runtime {
	rte_swx_port_in_pkt_rx_t pkt_rx;
	void *obj;
};

/*
 * Output port.
 */
struct port_out_type {
	TAILQ_ENTRY(port_out_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct rte_swx_port_out_ops ops;
};

TAILQ_HEAD(port_out_type_tailq, port_out_type);

struct port_out {
	TAILQ_ENTRY(port_out) node;
	struct port_out_type *type;
	void *obj;
	uint32_t id;
};

TAILQ_HEAD(port_out_tailq, port_out);

struct port_out_runtime {
	rte_swx_port_out_pkt_tx_t pkt_tx;
	rte_swx_port_out_flush_t flush;
	void *obj;
};

/*
 * Extern object.
 */
struct extern_type_member_func {
	TAILQ_ENTRY(extern_type_member_func) node;
	char name[RTE_SWX_NAME_SIZE];
	rte_swx_extern_type_member_func_t func;
	uint32_t id;
};

TAILQ_HEAD(extern_type_member_func_tailq, extern_type_member_func);

struct extern_type {
	TAILQ_ENTRY(extern_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *mailbox_struct_type;
	rte_swx_extern_type_constructor_t constructor;
	rte_swx_extern_type_destructor_t destructor;
	struct extern_type_member_func_tailq funcs;
	uint32_t n_funcs;
};

TAILQ_HEAD(extern_type_tailq, extern_type);

struct extern_obj {
	TAILQ_ENTRY(extern_obj) node;
	char name[RTE_SWX_NAME_SIZE];
	struct extern_type *type;
	void *obj;
	uint32_t struct_id;
	uint32_t id;
};

TAILQ_HEAD(extern_obj_tailq, extern_obj);

#ifndef RTE_SWX_EXTERN_TYPE_MEMBER_FUNCS_MAX
#define RTE_SWX_EXTERN_TYPE_MEMBER_FUNCS_MAX 8
#endif

struct extern_obj_runtime {
	void *obj;
	uint8_t *mailbox;
	rte_swx_extern_type_member_func_t funcs[RTE_SWX_EXTERN_TYPE_MEMBER_FUNCS_MAX];
};

/*
 * Extern function.
 */
struct extern_func {
	TAILQ_ENTRY(extern_func) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *mailbox_struct_type;
	rte_swx_extern_func_t func;
	uint32_t struct_id;
	uint32_t id;
};

TAILQ_HEAD(extern_func_tailq, extern_func);

struct extern_func_runtime {
	uint8_t *mailbox;
	rte_swx_extern_func_t func;
};

/*
 * Header.
 */
struct header {
	TAILQ_ENTRY(header) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *st;
	uint32_t struct_id;
	uint32_t id;
};

TAILQ_HEAD(header_tailq, header);

struct header_runtime {
	uint8_t *ptr0;
	uint32_t n_bytes;
};

struct header_out_runtime {
	uint8_t *ptr0;
	uint8_t *ptr;
	uint32_t n_bytes;
};

/*
 * Instruction.
 */

/* Packet headers are always in Network Byte Order (NBO), i.e. big endian.
 * Packet meta-data fields are always assumed to be in Host Byte Order (HBO).
 * Table entry fields can be in either NBO or HBO; they are assumed to be in HBO
 * when transferred to packet meta-data and in NBO when transferred to packet
 * headers.
 */

/* Notation conventions:
 *    -Header field: H = h.header.field (dst/src)
 *    -Meta-data field: M = m.field (dst/src)
 *    -Extern object mailbox field: E = e.field (dst/src)
 *    -Extern function mailbox field: F = f.field (dst/src)
 *    -Table action data field: T = t.field (src only)
 *    -Immediate value: I = 32-bit unsigned value (src only)
 */

enum instruction_type {
	/* rx m.port_in */
	INSTR_RX,

	/* tx port_out
	 * port_out = MI
	 */
	INSTR_TX,   /* port_out = M */
	INSTR_TX_I, /* port_out = I */

	/* extract h.header */
	INSTR_HDR_EXTRACT,
	INSTR_HDR_EXTRACT2,
	INSTR_HDR_EXTRACT3,
	INSTR_HDR_EXTRACT4,
	INSTR_HDR_EXTRACT5,
	INSTR_HDR_EXTRACT6,
	INSTR_HDR_EXTRACT7,
	INSTR_HDR_EXTRACT8,

	/* extract h.header m.last_field_size */
	INSTR_HDR_EXTRACT_M,

	/* lookahead h.header */
	INSTR_HDR_LOOKAHEAD,

	/* emit h.header */
	INSTR_HDR_EMIT,
	INSTR_HDR_EMIT_TX,
	INSTR_HDR_EMIT2_TX,
	INSTR_HDR_EMIT3_TX,
	INSTR_HDR_EMIT4_TX,
	INSTR_HDR_EMIT5_TX,
	INSTR_HDR_EMIT6_TX,
	INSTR_HDR_EMIT7_TX,
	INSTR_HDR_EMIT8_TX,

	/* validate h.header */
	INSTR_HDR_VALIDATE,

	/* invalidate h.header */
	INSTR_HDR_INVALIDATE,

	/* mov dst src
	 * dst = src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_MOV,    /* dst = MEF, src = MEFT */
	INSTR_MOV_MH, /* dst = MEF, src = H */
	INSTR_MOV_HM, /* dst = H, src = MEFT */
	INSTR_MOV_HH, /* dst = H, src = H */
	INSTR_MOV_I,  /* dst = HMEF, src = I */

	/* dma h.header t.field
	 * memcpy(h.header, t.field, sizeof(h.header))
	 */
	INSTR_DMA_HT,
	INSTR_DMA_HT2,
	INSTR_DMA_HT3,
	INSTR_DMA_HT4,
	INSTR_DMA_HT5,
	INSTR_DMA_HT6,
	INSTR_DMA_HT7,
	INSTR_DMA_HT8,

	/* add dst src
	 * dst += src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_ADD,    /* dst = MEF, src = MEF */
	INSTR_ALU_ADD_MH, /* dst = MEF, src = H */
	INSTR_ALU_ADD_HM, /* dst = H, src = MEF */
	INSTR_ALU_ADD_HH, /* dst = H, src = H */
	INSTR_ALU_ADD_MI, /* dst = MEF, src = I */
	INSTR_ALU_ADD_HI, /* dst = H, src = I */

	/* sub dst src
	 * dst -= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_SUB,    /* dst = MEF, src = MEF */
	INSTR_ALU_SUB_MH, /* dst = MEF, src = H */
	INSTR_ALU_SUB_HM, /* dst = H, src = MEF */
	INSTR_ALU_SUB_HH, /* dst = H, src = H */
	INSTR_ALU_SUB_MI, /* dst = MEF, src = I */
	INSTR_ALU_SUB_HI, /* dst = H, src = I */

	/* ckadd dst src
	 * dst = dst '+ src[0:1] '+ src[2:3] + ...
	 * dst = H, src = {H, h.header}
	 */
	INSTR_ALU_CKADD_FIELD,    /* src = H */
	INSTR_ALU_CKADD_STRUCT20, /* src = h.header, with sizeof(header) = 20 */
	INSTR_ALU_CKADD_STRUCT,   /* src = h.header, with any sizeof(header) */

	/* cksub dst src
	 * dst = dst '- src
	 * dst = H, src = H
	 */
	INSTR_ALU_CKSUB_FIELD,

	/* and dst src
	 * dst &= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_AND,    /* dst = MEF, src = MEFT */
	INSTR_ALU_AND_MH, /* dst = MEF, src = H */
	INSTR_ALU_AND_HM, /* dst = H, src = MEFT */
	INSTR_ALU_AND_HH, /* dst = H, src = H */
	INSTR_ALU_AND_I,  /* dst = HMEF, src = I */

	/* or dst src
	 * dst |= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_OR,    /* dst = MEF, src = MEFT */
	INSTR_ALU_OR_MH, /* dst = MEF, src = H */
	INSTR_ALU_OR_HM, /* dst = H, src = MEFT */
	INSTR_ALU_OR_HH, /* dst = H, src = H */
	INSTR_ALU_OR_I,  /* dst = HMEF, src = I */

	/* xor dst src
	 * dst ^= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_XOR,    /* dst = MEF, src = MEFT */
	INSTR_ALU_XOR_MH, /* dst = MEF, src = H */
	INSTR_ALU_XOR_HM, /* dst = H, src = MEFT */
	INSTR_ALU_XOR_HH, /* dst = H, src = H */
	INSTR_ALU_XOR_I,  /* dst = HMEF, src = I */

	/* shl dst src
	 * dst <<= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_SHL,    /* dst = MEF, src = MEF */
	INSTR_ALU_SHL_MH, /* dst = MEF, src = H */
	INSTR_ALU_SHL_HM, /* dst = H, src = MEF */
	INSTR_ALU_SHL_HH, /* dst = H, src = H */
	INSTR_ALU_SHL_MI, /* dst = MEF, src = I */
	INSTR_ALU_SHL_HI, /* dst = H, src = I */

	/* shr dst src
	 * dst >>= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_SHR,    /* dst = MEF, src = MEF */
	INSTR_ALU_SHR_MH, /* dst = MEF, src = H */
	INSTR_ALU_SHR_HM, /* dst = H, src = MEF */
	INSTR_ALU_SHR_HH, /* dst = H, src = H */
	INSTR_ALU_SHR_MI, /* dst = MEF, src = I */
	INSTR_ALU_SHR_HI, /* dst = H, src = I */

	/* regprefetch REGARRAY index
	 * prefetch REGARRAY[index]
	 * index = HMEFTI
	 */
	INSTR_REGPREFETCH_RH, /* index = H */
	INSTR_REGPREFETCH_RM, /* index = MEFT */
	INSTR_REGPREFETCH_RI, /* index = I */

	/* regrd dst REGARRAY index
	 * dst = REGARRAY[index]
	 * dst = HMEF, index = HMEFTI
	 */
	INSTR_REGRD_HRH, /* dst = H, index = H */
	INSTR_REGRD_HRM, /* dst = H, index = MEFT */
	INSTR_REGRD_HRI, /* dst = H, index = I */
	INSTR_REGRD_MRH, /* dst = MEF, index = H */
	INSTR_REGRD_MRM, /* dst = MEF, index = MEFT */
	INSTR_REGRD_MRI, /* dst = MEF, index = I */

	/* regwr REGARRAY index src
	 * REGARRAY[index] = src
	 * index = HMEFTI, src = HMEFTI
	 */
	INSTR_REGWR_RHH, /* index = H, src = H */
	INSTR_REGWR_RHM, /* index = H, src = MEFT */
	INSTR_REGWR_RHI, /* index = H, src = I */
	INSTR_REGWR_RMH, /* index = MEFT, src = H */
	INSTR_REGWR_RMM, /* index = MEFT, src = MEFT */
	INSTR_REGWR_RMI, /* index = MEFT, src = I */
	INSTR_REGWR_RIH, /* index = I, src = H */
	INSTR_REGWR_RIM, /* index = I, src = MEFT */
	INSTR_REGWR_RII, /* index = I, src = I */

	/* regadd REGARRAY index src
	 * REGARRAY[index] += src
	 * index = HMEFTI, src = HMEFTI
	 */
	INSTR_REGADD_RHH, /* index = H, src = H */
	INSTR_REGADD_RHM, /* index = H, src = MEFT */
	INSTR_REGADD_RHI, /* index = H, src = I */
	INSTR_REGADD_RMH, /* index = MEFT, src = H */
	INSTR_REGADD_RMM, /* index = MEFT, src = MEFT */
	INSTR_REGADD_RMI, /* index = MEFT, src = I */
	INSTR_REGADD_RIH, /* index = I, src = H */
	INSTR_REGADD_RIM, /* index = I, src = MEFT */
	INSTR_REGADD_RII, /* index = I, src = I */

	/* metprefetch METARRAY index
	 * prefetch METARRAY[index]
	 * index = HMEFTI
	 */
	INSTR_METPREFETCH_H, /* index = H */
	INSTR_METPREFETCH_M, /* index = MEFT */
	INSTR_METPREFETCH_I, /* index = I */

	/* meter METARRAY index length color_in color_out
	 * color_out = meter(METARRAY[index], length, color_in)
	 * index = HMEFTI, length = HMEFT, color_in = MEFTI, color_out = MEF
	 */
	INSTR_METER_HHM, /* index = H, length = H, color_in = MEFT */
	INSTR_METER_HHI, /* index = H, length = H, color_in = I */
	INSTR_METER_HMM, /* index = H, length = MEFT, color_in = MEFT */
	INSTR_METER_HMI, /* index = H, length = MEFT, color_in = I */
	INSTR_METER_MHM, /* index = MEFT, length = H, color_in = MEFT */
	INSTR_METER_MHI, /* index = MEFT, length = H, color_in = I */
	INSTR_METER_MMM, /* index = MEFT, length = MEFT, color_in = MEFT */
	INSTR_METER_MMI, /* index = MEFT, length = MEFT, color_in = I */
	INSTR_METER_IHM, /* index = I, length = H, color_in = MEFT */
	INSTR_METER_IHI, /* index = I, length = H, color_in = I */
	INSTR_METER_IMM, /* index = I, length = MEFT, color_in = MEFT */
	INSTR_METER_IMI, /* index = I, length = MEFT, color_in = I */

	/* table TABLE */
	INSTR_TABLE,
	INSTR_TABLE_AF,
	INSTR_SELECTOR,
	INSTR_LEARNER,
	INSTR_LEARNER_AF,

	/* learn LEARNER ACTION_NAME [ m.action_first_arg ] */
	INSTR_LEARNER_LEARN,

	/* forget */
	INSTR_LEARNER_FORGET,

	/* extern e.obj.func */
	INSTR_EXTERN_OBJ,

	/* extern f.func */
	INSTR_EXTERN_FUNC,

	/* jmp LABEL
	 * Unconditional jump
	 */
	INSTR_JMP,

	/* jmpv LABEL h.header
	 * Jump if header is valid
	 */
	INSTR_JMP_VALID,

	/* jmpnv LABEL h.header
	 * Jump if header is invalid
	 */
	INSTR_JMP_INVALID,

	/* jmph LABEL
	 * Jump if table lookup hit
	 */
	INSTR_JMP_HIT,

	/* jmpnh LABEL
	 * Jump if table lookup miss
	 */
	INSTR_JMP_MISS,

	/* jmpa LABEL ACTION
	 * Jump if action run
	 */
	INSTR_JMP_ACTION_HIT,

	/* jmpna LABEL ACTION
	 * Jump if action not run
	 */
	INSTR_JMP_ACTION_MISS,

	/* jmpeq LABEL a b
	 * Jump if a is equal to b
	 * a = HMEFT, b = HMEFTI
	 */
	INSTR_JMP_EQ,    /* a = MEFT, b = MEFT */
	INSTR_JMP_EQ_MH, /* a = MEFT, b = H */
	INSTR_JMP_EQ_HM, /* a = H, b = MEFT */
	INSTR_JMP_EQ_HH, /* a = H, b = H */
	INSTR_JMP_EQ_I,  /* (a, b) = (MEFT, I) or (a, b) = (H, I) */

	/* jmpneq LABEL a b
	 * Jump if a is not equal to b
	 * a = HMEFT, b = HMEFTI
	 */
	INSTR_JMP_NEQ,    /* a = MEFT, b = MEFT */
	INSTR_JMP_NEQ_MH, /* a = MEFT, b = H */
	INSTR_JMP_NEQ_HM, /* a = H, b = MEFT */
	INSTR_JMP_NEQ_HH, /* a = H, b = H */
	INSTR_JMP_NEQ_I,  /* (a, b) = (MEFT, I) or (a, b) = (H, I) */

	/* jmplt LABEL a b
	 * Jump if a is less than b
	 * a = HMEFT, b = HMEFTI
	 */
	INSTR_JMP_LT,    /* a = MEFT, b = MEFT */
	INSTR_JMP_LT_MH, /* a = MEFT, b = H */
	INSTR_JMP_LT_HM, /* a = H, b = MEFT */
	INSTR_JMP_LT_HH, /* a = H, b = H */
	INSTR_JMP_LT_MI, /* a = MEFT, b = I */
	INSTR_JMP_LT_HI, /* a = H, b = I */

	/* jmpgt LABEL a b
	 * Jump if a is greater than b
	 * a = HMEFT, b = HMEFTI
	 */
	INSTR_JMP_GT,    /* a = MEFT, b = MEFT */
	INSTR_JMP_GT_MH, /* a = MEFT, b = H */
	INSTR_JMP_GT_HM, /* a = H, b = MEFT */
	INSTR_JMP_GT_HH, /* a = H, b = H */
	INSTR_JMP_GT_MI, /* a = MEFT, b = I */
	INSTR_JMP_GT_HI, /* a = H, b = I */

	/* return
	 * Return from action
	 */
	INSTR_RETURN,

	/* Start of custom instructions. */
	INSTR_CUSTOM_0,
};

struct instr_operand {
	uint8_t struct_id;
	uint8_t n_bits;
	uint8_t offset;
	uint8_t pad;
};

struct instr_io {
	struct {
		union {
			struct {
				uint8_t offset;
				uint8_t n_bits;
				uint8_t pad[2];
			};

			uint32_t val;
		};
	} io;

	struct {
		uint8_t header_id[8];
		uint8_t struct_id[8];
		uint8_t n_bytes[8];
	} hdr;
};

struct instr_hdr_validity {
	uint8_t header_id;
};

struct instr_table {
	uint8_t table_id;
};

struct instr_learn {
	uint8_t action_id;
	uint8_t mf_offset;
};

struct instr_extern_obj {
	uint8_t ext_obj_id;
	uint8_t func_id;
};

struct instr_extern_func {
	uint8_t ext_func_id;
};

struct instr_dst_src {
	struct instr_operand dst;
	union {
		struct instr_operand src;
		uint64_t src_val;
	};
};

struct instr_regarray {
	uint8_t regarray_id;
	uint8_t pad[3];

	union {
		struct instr_operand idx;
		uint32_t idx_val;
	};

	union {
		struct instr_operand dstsrc;
		uint64_t dstsrc_val;
	};
};

struct instr_meter {
	uint8_t metarray_id;
	uint8_t pad[3];

	union {
		struct instr_operand idx;
		uint32_t idx_val;
	};

	struct instr_operand length;

	union {
		struct instr_operand color_in;
		uint32_t color_in_val;
	};

	struct instr_operand color_out;
};

struct instr_dma {
	struct {
		uint8_t header_id[8];
		uint8_t struct_id[8];
	} dst;

	struct {
		uint8_t offset[8];
	} src;

	uint16_t n_bytes[8];
};

struct instr_jmp {
	struct instruction *ip;

	union {
		struct instr_operand a;
		uint8_t header_id;
		uint8_t action_id;
	};

	union {
		struct instr_operand b;
		uint64_t b_val;
	};
};

struct instruction {
	enum instruction_type type;
	union {
		struct instr_io io;
		struct instr_hdr_validity valid;
		struct instr_dst_src mov;
		struct instr_regarray regarray;
		struct instr_meter meter;
		struct instr_dma dma;
		struct instr_dst_src alu;
		struct instr_table table;
		struct instr_learn learn;
		struct instr_extern_obj ext_obj;
		struct instr_extern_func ext_func;
		struct instr_jmp jmp;
	};
};

struct instruction_data {
	char label[RTE_SWX_NAME_SIZE];
	char jmp_label[RTE_SWX_NAME_SIZE];
	uint32_t n_users; /* user = jmp instruction to this instruction. */
	int invalid;
};

typedef void (*instr_exec_t)(struct rte_swx_pipeline *);

/*
 * Action.
 */
typedef void
(*action_func_t)(struct rte_swx_pipeline *p);

struct action {
	TAILQ_ENTRY(action) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *st;
	int *args_endianness; /* 0 = Host Byte Order (HBO); 1 = Network Byte Order (NBO). */
	struct instruction *instructions;
	struct instruction_data *instruction_data;
	uint32_t n_instructions;
	uint32_t id;
};

TAILQ_HEAD(action_tailq, action);

/*
 * Table.
 */
struct table_type {
	TAILQ_ENTRY(table_type) node;
	char name[RTE_SWX_NAME_SIZE];
	enum rte_swx_table_match_type match_type;
	struct rte_swx_table_ops ops;
};

TAILQ_HEAD(table_type_tailq, table_type);

struct match_field {
	enum rte_swx_table_match_type match_type;
	struct field *field;
};

struct table {
	TAILQ_ENTRY(table) node;
	char name[RTE_SWX_NAME_SIZE];
	char args[RTE_SWX_NAME_SIZE];
	struct table_type *type; /* NULL when n_fields == 0. */

	/* Match. */
	struct match_field *fields;
	uint32_t n_fields;
	struct header *header; /* Only valid when n_fields > 0. */

	/* Action. */
	struct action **actions;
	struct action *default_action;
	uint8_t *default_action_data;
	uint32_t n_actions;
	int default_action_is_const;
	uint32_t action_data_size_max;
	int *action_is_for_table_entries;
	int *action_is_for_default_entry;

	uint32_t size;
	uint32_t id;
};

TAILQ_HEAD(table_tailq, table);

struct table_runtime {
	rte_swx_table_lookup_t func;
	void *mailbox;
	uint8_t **key;
};

struct table_statistics {
	uint64_t n_pkts_hit[2]; /* 0 = Miss, 1 = Hit. */
	uint64_t *n_pkts_action;
};

/*
 * Selector.
 */
struct selector {
	TAILQ_ENTRY(selector) node;
	char name[RTE_SWX_NAME_SIZE];

	struct field *group_id_field;
	struct field **selector_fields;
	uint32_t n_selector_fields;
	struct header *selector_header;
	struct field *member_id_field;

	uint32_t n_groups_max;
	uint32_t n_members_per_group_max;

	uint32_t id;
};

TAILQ_HEAD(selector_tailq, selector);

struct selector_runtime {
	void *mailbox;
	uint8_t **group_id_buffer;
	uint8_t **selector_buffer;
	uint8_t **member_id_buffer;
};

struct selector_statistics {
	uint64_t n_pkts;
};

/*
 * Learner table.
 */
struct learner {
	TAILQ_ENTRY(learner) node;
	char name[RTE_SWX_NAME_SIZE];

	/* Match. */
	struct field **fields;
	uint32_t n_fields;
	struct header *header;

	/* Action. */
	struct action **actions;
	struct action *default_action;
	uint8_t *default_action_data;
	uint32_t n_actions;
	int default_action_is_const;
	uint32_t action_data_size_max;
	int *action_is_for_table_entries;
	int *action_is_for_default_entry;

	uint32_t size;
	uint32_t timeout;
	uint32_t id;
};

TAILQ_HEAD(learner_tailq, learner);

struct learner_runtime {
	void *mailbox;
	uint8_t **key;
};

struct learner_statistics {
	uint64_t n_pkts_hit[2]; /* 0 = Miss, 1 = Hit. */
	uint64_t n_pkts_learn[2]; /* 0 = Learn OK, 1 = Learn error. */
	uint64_t n_pkts_forget;
	uint64_t *n_pkts_action;
};

/*
 * Register array.
 */
struct regarray {
	TAILQ_ENTRY(regarray) node;
	char name[RTE_SWX_NAME_SIZE];
	uint64_t init_val;
	uint32_t size;
	uint32_t id;
};

TAILQ_HEAD(regarray_tailq, regarray);

struct regarray_runtime {
	uint64_t *regarray;
	uint32_t size_mask;
};

/*
 * Meter array.
 */
struct meter_profile {
	TAILQ_ENTRY(meter_profile) node;
	char name[RTE_SWX_NAME_SIZE];
	struct rte_meter_trtcm_params params;
	struct rte_meter_trtcm_profile profile;
	uint32_t n_users;
};

TAILQ_HEAD(meter_profile_tailq, meter_profile);

struct metarray {
	TAILQ_ENTRY(metarray) node;
	char name[RTE_SWX_NAME_SIZE];
	uint32_t size;
	uint32_t id;
};

TAILQ_HEAD(metarray_tailq, metarray);

struct meter {
	struct rte_meter_trtcm m;
	struct meter_profile *profile;
	enum rte_color color_mask;
	uint8_t pad[20];

	uint64_t n_pkts[RTE_COLORS];
	uint64_t n_bytes[RTE_COLORS];
};

struct metarray_runtime {
	struct meter *metarray;
	uint32_t size_mask;
};

/*
 * Pipeline.
 */
struct thread {
	/* Packet. */
	struct rte_swx_pkt pkt;
	uint8_t *ptr;

	/* Structures. */
	uint8_t **structs;

	/* Packet headers. */
	struct header_runtime *headers; /* Extracted or generated headers. */
	struct header_out_runtime *headers_out; /* Emitted headers. */
	uint8_t *header_storage;
	uint8_t *header_out_storage;
	uint64_t valid_headers;
	uint32_t n_headers_out;

	/* Packet meta-data. */
	uint8_t *metadata;

	/* Tables. */
	struct table_runtime *tables;
	struct selector_runtime *selectors;
	struct learner_runtime *learners;
	struct rte_swx_table_state *table_state;
	uint64_t action_id;
	int hit; /* 0 = Miss, 1 = Hit. */
	uint32_t learner_id;
	uint64_t time;

	/* Extern objects and functions. */
	struct extern_obj_runtime *extern_objs;
	struct extern_func_runtime *extern_funcs;

	/* Instructions. */
	struct instruction *ip;
	struct instruction *ret;
};

#define MASK64_BIT_GET(mask, pos) ((mask) & (1LLU << (pos)))
#define MASK64_BIT_SET(mask, pos) ((mask) | (1LLU << (pos)))
#define MASK64_BIT_CLR(mask, pos) ((mask) & ~(1LLU << (pos)))

#define HEADER_VALID(thread, header_id) \
	MASK64_BIT_GET((thread)->valid_headers, header_id)

static inline uint64_t
instr_operand_hbo(struct thread *t, const struct instr_operand *x)
{
	uint8_t *x_struct = t->structs[x->struct_id];
	uint64_t *x64_ptr = (uint64_t *)&x_struct[x->offset];
	uint64_t x64 = *x64_ptr;
	uint64_t x64_mask = UINT64_MAX >> (64 - x->n_bits);

	return x64 & x64_mask;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint64_t
instr_operand_nbo(struct thread *t, const struct instr_operand *x)
{
	uint8_t *x_struct = t->structs[x->struct_id];
	uint64_t *x64_ptr = (uint64_t *)&x_struct[x->offset];
	uint64_t x64 = *x64_ptr;

	return ntoh64(x64) >> (64 - x->n_bits);
}

#else

#define instr_operand_nbo instr_operand_hbo

#endif

#define ALU(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = dst64 & dst64_mask;                                     \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->alu.src.n_bits);       \
	uint64_t src = src64 & src64_mask;                                     \
									       \
	uint64_t result = dst operator src;                                    \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (result & dst64_mask);            \
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define ALU_MH(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = dst64 & dst64_mask;                                     \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src = ntoh64(src64) >> (64 - (ip)->alu.src.n_bits);           \
									       \
	uint64_t result = dst operator src;                                    \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (result & dst64_mask);            \
}

#define ALU_HM(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = ntoh64(dst64) >> (64 - (ip)->alu.dst.n_bits);           \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->alu.src.n_bits);       \
	uint64_t src = src64 & src64_mask;                                     \
									       \
	uint64_t result = dst operator src;                                    \
	result = hton64(result << (64 - (ip)->alu.dst.n_bits));                \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | result;                           \
}

#define ALU_HM_FAST(thread, ip, operator)  \
{                                                                                 \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];         \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];      \
	uint64_t dst64 = *dst64_ptr;                                              \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);          \
	uint64_t dst = dst64 & dst64_mask;                                        \
										  \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];         \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];      \
	uint64_t src64 = *src64_ptr;                                              \
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->alu.src.n_bits);          \
	uint64_t src = hton64(src64 & src64_mask) >> (64 - (ip)->alu.dst.n_bits); \
										  \
	uint64_t result = dst operator src;                                       \
										  \
	*dst64_ptr = (dst64 & ~dst64_mask) | result;                              \
}

#define ALU_HH(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = ntoh64(dst64) >> (64 - (ip)->alu.dst.n_bits);           \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src = ntoh64(src64) >> (64 - (ip)->alu.src.n_bits);           \
									       \
	uint64_t result = dst operator src;                                    \
	result = hton64(result << (64 - (ip)->alu.dst.n_bits));                \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | result;                           \
}

#define ALU_HH_FAST(thread, ip, operator)  \
{                                                                                             \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];                     \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];                  \
	uint64_t dst64 = *dst64_ptr;                                                          \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);                      \
	uint64_t dst = dst64 & dst64_mask;                                                    \
											      \
	uint8_t *src_struct = (thread)->structs[(ip)->alu.src.struct_id];                     \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->alu.src.offset];                  \
	uint64_t src64 = *src64_ptr;                                                          \
	uint64_t src = (src64 << (64 - (ip)->alu.src.n_bits)) >> (64 - (ip)->alu.dst.n_bits); \
											      \
	uint64_t result = dst operator src;                                                   \
											      \
	*dst64_ptr = (dst64 & ~dst64_mask) | result;                                          \
}

#else

#define ALU_MH ALU
#define ALU_HM ALU
#define ALU_HM_FAST ALU
#define ALU_HH ALU
#define ALU_HH_FAST ALU

#endif

#define ALU_I(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = dst64 & dst64_mask;                                     \
									       \
	uint64_t src = (ip)->alu.src_val;                                      \
									       \
	uint64_t result = dst operator src;                                    \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (result & dst64_mask);            \
}

#define ALU_MI ALU_I

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define ALU_HI(thread, ip, operator)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->alu.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->alu.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->alu.dst.n_bits);       \
	uint64_t dst = ntoh64(dst64) >> (64 - (ip)->alu.dst.n_bits);           \
									       \
	uint64_t src = (ip)->alu.src_val;                                      \
									       \
	uint64_t result = dst operator src;                                    \
	result = hton64(result << (64 - (ip)->alu.dst.n_bits));                \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | result;                           \
}

#else

#define ALU_HI ALU_I

#endif

#define MOV(thread, ip)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->mov.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->mov.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->mov.dst.n_bits);       \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->mov.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->mov.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->mov.src.n_bits);       \
	uint64_t src = src64 & src64_mask;                                     \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);               \
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define MOV_MH(thread, ip)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->mov.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->mov.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->mov.dst.n_bits);       \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->mov.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->mov.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src = ntoh64(src64) >> (64 - (ip)->mov.src.n_bits);           \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);               \
}

#define MOV_HM(thread, ip)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->mov.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->mov.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->mov.dst.n_bits);       \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->mov.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->mov.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->mov.src.n_bits);       \
	uint64_t src = src64 & src64_mask;                                     \
									       \
	src = hton64(src) >> (64 - (ip)->mov.dst.n_bits);                      \
	*dst64_ptr = (dst64 & ~dst64_mask) | src;                              \
}

#define MOV_HH(thread, ip)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->mov.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->mov.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->mov.dst.n_bits);       \
									       \
	uint8_t *src_struct = (thread)->structs[(ip)->mov.src.struct_id];      \
	uint64_t *src64_ptr = (uint64_t *)&src_struct[(ip)->mov.src.offset];   \
	uint64_t src64 = *src64_ptr;                                           \
									       \
	uint64_t src = src64 << (64 - (ip)->mov.src.n_bits);                   \
	src = src >> (64 - (ip)->mov.dst.n_bits);                              \
	*dst64_ptr = (dst64 & ~dst64_mask) | src;                              \
}

#else

#define MOV_MH MOV
#define MOV_HM MOV
#define MOV_HH MOV

#endif

#define MOV_I(thread, ip)  \
{                                                                              \
	uint8_t *dst_struct = (thread)->structs[(ip)->mov.dst.struct_id];      \
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[(ip)->mov.dst.offset];   \
	uint64_t dst64 = *dst64_ptr;                                           \
	uint64_t dst64_mask = UINT64_MAX >> (64 - (ip)->mov.dst.n_bits);       \
									       \
	uint64_t src = (ip)->mov.src_val;                                      \
									       \
	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);               \
}

#define JMP_CMP(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a64_mask = UINT64_MAX >> (64 - (ip)->jmp.a.n_bits);           \
	uint64_t a = a64 & a64_mask;                                           \
									       \
	uint8_t *b_struct = (thread)->structs[(ip)->jmp.b.struct_id];          \
	uint64_t *b64_ptr = (uint64_t *)&b_struct[(ip)->jmp.b.offset];         \
	uint64_t b64 = *b64_ptr;                                               \
	uint64_t b64_mask = UINT64_MAX >> (64 - (ip)->jmp.b.n_bits);           \
	uint64_t b = b64 & b64_mask;                                           \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define JMP_CMP_MH(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a64_mask = UINT64_MAX >> (64 - (ip)->jmp.a.n_bits);           \
	uint64_t a = a64 & a64_mask;                                           \
									       \
	uint8_t *b_struct = (thread)->structs[(ip)->jmp.b.struct_id];          \
	uint64_t *b64_ptr = (uint64_t *)&b_struct[(ip)->jmp.b.offset];         \
	uint64_t b64 = *b64_ptr;                                               \
	uint64_t b = ntoh64(b64) >> (64 - (ip)->jmp.b.n_bits);                 \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#define JMP_CMP_HM(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a = ntoh64(a64) >> (64 - (ip)->jmp.a.n_bits);                 \
									       \
	uint8_t *b_struct = (thread)->structs[(ip)->jmp.b.struct_id];          \
	uint64_t *b64_ptr = (uint64_t *)&b_struct[(ip)->jmp.b.offset];         \
	uint64_t b64 = *b64_ptr;                                               \
	uint64_t b64_mask = UINT64_MAX >> (64 - (ip)->jmp.b.n_bits);           \
	uint64_t b = b64 & b64_mask;                                           \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#define JMP_CMP_HH(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a = ntoh64(a64) >> (64 - (ip)->jmp.a.n_bits);                 \
									       \
	uint8_t *b_struct = (thread)->structs[(ip)->jmp.b.struct_id];          \
	uint64_t *b64_ptr = (uint64_t *)&b_struct[(ip)->jmp.b.offset];         \
	uint64_t b64 = *b64_ptr;                                               \
	uint64_t b = ntoh64(b64) >> (64 - (ip)->jmp.b.n_bits);                 \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#define JMP_CMP_HH_FAST(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a = a64 << (64 - (ip)->jmp.a.n_bits);                         \
									       \
	uint8_t *b_struct = (thread)->structs[(ip)->jmp.b.struct_id];          \
	uint64_t *b64_ptr = (uint64_t *)&b_struct[(ip)->jmp.b.offset];         \
	uint64_t b64 = *b64_ptr;                                               \
	uint64_t b = b64 << (64 - (ip)->jmp.b.n_bits);                         \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#else

#define JMP_CMP_MH JMP_CMP
#define JMP_CMP_HM JMP_CMP
#define JMP_CMP_HH JMP_CMP
#define JMP_CMP_HH_FAST JMP_CMP

#endif

#define JMP_CMP_I(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a64_mask = UINT64_MAX >> (64 - (ip)->jmp.a.n_bits);           \
	uint64_t a = a64 & a64_mask;                                           \
									       \
	uint64_t b = (ip)->jmp.b_val;                                          \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#define JMP_CMP_MI JMP_CMP_I

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define JMP_CMP_HI(thread, ip, operator)  \
{                                                                              \
	uint8_t *a_struct = (thread)->structs[(ip)->jmp.a.struct_id];          \
	uint64_t *a64_ptr = (uint64_t *)&a_struct[(ip)->jmp.a.offset];         \
	uint64_t a64 = *a64_ptr;                                               \
	uint64_t a = ntoh64(a64) >> (64 - (ip)->jmp.a.n_bits);                 \
									       \
	uint64_t b = (ip)->jmp.b_val;                                          \
									       \
	(thread)->ip = (a operator b) ? (ip)->jmp.ip : ((thread)->ip + 1);     \
}

#else

#define JMP_CMP_HI JMP_CMP_I

#endif

#define METADATA_READ(thread, offset, n_bits)                                  \
({                                                                             \
	uint64_t *m64_ptr = (uint64_t *)&(thread)->metadata[offset];           \
	uint64_t m64 = *m64_ptr;                                               \
	uint64_t m64_mask = UINT64_MAX >> (64 - (n_bits));                     \
	(m64 & m64_mask);                                                      \
})

#define METADATA_WRITE(thread, offset, n_bits, value)                          \
{                                                                              \
	uint64_t *m64_ptr = (uint64_t *)&(thread)->metadata[offset];           \
	uint64_t m64 = *m64_ptr;                                               \
	uint64_t m64_mask = UINT64_MAX >> (64 - (n_bits));                     \
									       \
	uint64_t m_new = value;                                                \
									       \
	*m64_ptr = (m64 & ~m64_mask) | (m_new & m64_mask);                     \
}

#ifndef RTE_SWX_PIPELINE_THREADS_MAX
#define RTE_SWX_PIPELINE_THREADS_MAX 16
#endif

#ifndef RTE_SWX_PIPELINE_INSTRUCTION_TABLE_SIZE_MAX
#define RTE_SWX_PIPELINE_INSTRUCTION_TABLE_SIZE_MAX 256
#endif

struct rte_swx_pipeline {
	struct struct_type_tailq struct_types;
	struct port_in_type_tailq port_in_types;
	struct port_in_tailq ports_in;
	struct port_out_type_tailq port_out_types;
	struct port_out_tailq ports_out;
	struct extern_type_tailq extern_types;
	struct extern_obj_tailq extern_objs;
	struct extern_func_tailq extern_funcs;
	struct header_tailq headers;
	struct struct_type *metadata_st;
	uint32_t metadata_struct_id;
	struct action_tailq actions;
	struct table_type_tailq table_types;
	struct table_tailq tables;
	struct selector_tailq selectors;
	struct learner_tailq learners;
	struct regarray_tailq regarrays;
	struct meter_profile_tailq meter_profiles;
	struct metarray_tailq metarrays;

	struct port_in_runtime *in;
	struct port_out_runtime *out;
	struct instruction **action_instructions;
	action_func_t *action_funcs;
	struct rte_swx_table_state *table_state;
	struct table_statistics *table_stats;
	struct selector_statistics *selector_stats;
	struct learner_statistics *learner_stats;
	struct regarray_runtime *regarray_runtime;
	struct metarray_runtime *metarray_runtime;
	struct instruction *instructions;
	struct instruction_data *instruction_data;
	instr_exec_t *instruction_table;
	struct thread threads[RTE_SWX_PIPELINE_THREADS_MAX];
	void *lib;

	uint32_t n_structs;
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_extern_objs;
	uint32_t n_extern_funcs;
	uint32_t n_actions;
	uint32_t n_tables;
	uint32_t n_selectors;
	uint32_t n_learners;
	uint32_t n_regarrays;
	uint32_t n_metarrays;
	uint32_t n_headers;
	uint32_t thread_id;
	uint32_t port_id;
	uint32_t n_instructions;
	int build_done;
	int numa_node;
};

/*
 * Instruction.
 */
static inline void
pipeline_port_inc(struct rte_swx_pipeline *p)
{
	p->port_id = (p->port_id + 1) & (p->n_ports_in - 1);
}

static inline void
thread_ip_reset(struct rte_swx_pipeline *p, struct thread *t)
{
	t->ip = p->instructions;
}

static inline void
thread_ip_set(struct thread *t, struct instruction *ip)
{
	t->ip = ip;
}

static inline void
thread_ip_action_call(struct rte_swx_pipeline *p,
		      struct thread *t,
		      uint32_t action_id)
{
	t->ret = t->ip + 1;
	t->ip = p->action_instructions[action_id];
}

static inline void
thread_ip_inc(struct rte_swx_pipeline *p);

static inline void
thread_ip_inc(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];

	t->ip++;
}

static inline void
thread_ip_inc_cond(struct thread *t, int cond)
{
	t->ip += cond;
}

static inline void
thread_yield(struct rte_swx_pipeline *p)
{
	p->thread_id = (p->thread_id + 1) & (RTE_SWX_PIPELINE_THREADS_MAX - 1);
}

static inline void
thread_yield_cond(struct rte_swx_pipeline *p, int cond)
{
	p->thread_id = (p->thread_id + cond) & (RTE_SWX_PIPELINE_THREADS_MAX - 1);
}

/*
 * rx.
 */
static inline int
__instr_rx_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct port_in_runtime *port = &p->in[p->port_id];
	struct rte_swx_pkt *pkt = &t->pkt;
	int pkt_received;

	/* Packet. */
	pkt_received = port->pkt_rx(port->obj, pkt);
	t->ptr = &pkt->pkt[pkt->offset];
	rte_prefetch0(t->ptr);

	TRACE("[Thread %2u] rx %s from port %u\n",
	      p->thread_id,
	      pkt_received ? "1 pkt" : "0 pkts",
	      p->port_id);

	/* Headers. */
	t->valid_headers = 0;
	t->n_headers_out = 0;

	/* Meta-data. */
	METADATA_WRITE(t, ip->io.io.offset, ip->io.io.n_bits, p->port_id);

	/* Tables. */
	t->table_state = p->table_state;

	/* Thread. */
	pipeline_port_inc(p);

	return pkt_received;
}

static inline void
instr_rx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	int pkt_received;

	/* Packet. */
	pkt_received = __instr_rx_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc_cond(t, pkt_received);
	thread_yield(p);
}

/*
 * tx.
 */
static inline void
emit_handler(struct thread *t)
{
	struct header_out_runtime *h0 = &t->headers_out[0];
	struct header_out_runtime *h1 = &t->headers_out[1];
	uint32_t offset = 0, i;

	/* No header change or header decapsulation. */
	if ((t->n_headers_out == 1) &&
	    (h0->ptr + h0->n_bytes == t->ptr)) {
		TRACE("Emit handler: no header change or header decap.\n");

		t->pkt.offset -= h0->n_bytes;
		t->pkt.length += h0->n_bytes;

		return;
	}

	/* Header encapsulation (optionally, with prior header decapsulation). */
	if ((t->n_headers_out == 2) &&
	    (h1->ptr + h1->n_bytes == t->ptr) &&
	    (h0->ptr == h0->ptr0)) {
		uint32_t offset;

		TRACE("Emit handler: header encapsulation.\n");

		offset = h0->n_bytes + h1->n_bytes;
		memcpy(t->ptr - offset, h0->ptr, h0->n_bytes);
		t->pkt.offset -= offset;
		t->pkt.length += offset;

		return;
	}

	/* For any other case. */
	TRACE("Emit handler: complex case.\n");

	for (i = 0; i < t->n_headers_out; i++) {
		struct header_out_runtime *h = &t->headers_out[i];

		memcpy(&t->header_out_storage[offset], h->ptr, h->n_bytes);
		offset += h->n_bytes;
	}

	if (offset) {
		memcpy(t->ptr - offset, t->header_out_storage, offset);
		t->pkt.offset -= offset;
		t->pkt.length += offset;
	}
}

static inline void
__instr_tx_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t port_id = METADATA_READ(t, ip->io.io.offset, ip->io.io.n_bits);
	struct port_out_runtime *port = &p->out[port_id];
	struct rte_swx_pkt *pkt = &t->pkt;

	TRACE("[Thread %2u]: tx 1 pkt to port %u\n",
	      p->thread_id,
	      (uint32_t)port_id);

	/* Headers. */
	emit_handler(t);

	/* Packet. */
	port->pkt_tx(port->obj, pkt);
}

static inline void
__instr_tx_i_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t port_id = ip->io.io.val;
	struct port_out_runtime *port = &p->out[port_id];
	struct rte_swx_pkt *pkt = &t->pkt;

	TRACE("[Thread %2u]: tx (i) 1 pkt to port %u\n",
	      p->thread_id,
	      (uint32_t)port_id);

	/* Headers. */
	emit_handler(t);

	/* Packet. */
	port->pkt_tx(port->obj, pkt);
}

/*
 * extract.
 */
static inline void
__instr_hdr_extract_many_exec(struct rte_swx_pipeline *p __rte_unused,
			      struct thread *t,
			      const struct instruction *ip,
			      uint32_t n_extract)
{
	uint64_t valid_headers = t->valid_headers;
	uint8_t *ptr = t->ptr;
	uint32_t offset = t->pkt.offset;
	uint32_t length = t->pkt.length;
	uint32_t i;

	for (i = 0; i < n_extract; i++) {
		uint32_t header_id = ip->io.hdr.header_id[i];
		uint32_t struct_id = ip->io.hdr.struct_id[i];
		uint32_t n_bytes = ip->io.hdr.n_bytes[i];

		TRACE("[Thread %2u]: extract header %u (%u bytes)\n",
		      p->thread_id,
		      header_id,
		      n_bytes);

		/* Headers. */
		t->structs[struct_id] = ptr;
		valid_headers = MASK64_BIT_SET(valid_headers, header_id);

		/* Packet. */
		offset += n_bytes;
		length -= n_bytes;
		ptr += n_bytes;
	}

	/* Headers. */
	t->valid_headers = valid_headers;

	/* Packet. */
	t->pkt.offset = offset;
	t->pkt.length = length;
	t->ptr = ptr;
}

static inline void
__instr_hdr_extract_exec(struct rte_swx_pipeline *p,
			 struct thread *t,
			 const struct instruction *ip)
{
	__instr_hdr_extract_many_exec(p, t, ip, 1);
}

static inline void
__instr_hdr_extract2_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 2);
}

static inline void
__instr_hdr_extract3_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 3);
}

static inline void
__instr_hdr_extract4_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 4);
}

static inline void
__instr_hdr_extract5_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 5);
}

static inline void
__instr_hdr_extract6_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 6);
}

static inline void
__instr_hdr_extract7_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 7);
}

static inline void
__instr_hdr_extract8_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_extract_many_exec(p, t, ip, 8);
}

static inline void
__instr_hdr_extract_m_exec(struct rte_swx_pipeline *p __rte_unused,
			   struct thread *t,
			   const struct instruction *ip)
{
	uint64_t valid_headers = t->valid_headers;
	uint8_t *ptr = t->ptr;
	uint32_t offset = t->pkt.offset;
	uint32_t length = t->pkt.length;

	uint32_t n_bytes_last = METADATA_READ(t, ip->io.io.offset, ip->io.io.n_bits);
	uint32_t header_id = ip->io.hdr.header_id[0];
	uint32_t struct_id = ip->io.hdr.struct_id[0];
	uint32_t n_bytes = ip->io.hdr.n_bytes[0];

	struct header_runtime *h = &t->headers[header_id];

	TRACE("[Thread %2u]: extract header %u (%u + %u bytes)\n",
	      p->thread_id,
	      header_id,
	      n_bytes,
	      n_bytes_last);

	n_bytes += n_bytes_last;

	/* Headers. */
	t->structs[struct_id] = ptr;
	t->valid_headers = MASK64_BIT_SET(valid_headers, header_id);
	h->n_bytes = n_bytes;

	/* Packet. */
	t->pkt.offset = offset + n_bytes;
	t->pkt.length = length - n_bytes;
	t->ptr = ptr + n_bytes;
}

static inline void
__instr_hdr_lookahead_exec(struct rte_swx_pipeline *p __rte_unused,
			   struct thread *t,
			   const struct instruction *ip)
{
	uint64_t valid_headers = t->valid_headers;
	uint8_t *ptr = t->ptr;

	uint32_t header_id = ip->io.hdr.header_id[0];
	uint32_t struct_id = ip->io.hdr.struct_id[0];

	TRACE("[Thread %2u]: lookahead header %u\n",
	      p->thread_id,
	      header_id);

	/* Headers. */
	t->structs[struct_id] = ptr;
	t->valid_headers = MASK64_BIT_SET(valid_headers, header_id);
}

/*
 * emit.
 */
static inline void
__instr_hdr_emit_many_exec(struct rte_swx_pipeline *p __rte_unused,
			   struct thread *t,
			   const struct instruction *ip,
			   uint32_t n_emit)
{
	uint64_t valid_headers = t->valid_headers;
	uint32_t n_headers_out = t->n_headers_out;
	struct header_out_runtime *ho = NULL;
	uint8_t *ho_ptr = NULL;
	uint32_t ho_nbytes = 0, i;

	for (i = 0; i < n_emit; i++) {
		uint32_t header_id = ip->io.hdr.header_id[i];
		uint32_t struct_id = ip->io.hdr.struct_id[i];

		struct header_runtime *hi = &t->headers[header_id];
		uint8_t *hi_ptr0 = hi->ptr0;
		uint32_t n_bytes = hi->n_bytes;

		uint8_t *hi_ptr = t->structs[struct_id];

		if (!MASK64_BIT_GET(valid_headers, header_id)) {
			TRACE("[Thread %2u]: emit header %u (invalid)\n",
			      p->thread_id,
			      header_id);

			continue;
		}

		TRACE("[Thread %2u]: emit header %u (valid)\n",
		      p->thread_id,
		      header_id);

		/* Headers. */
		if (!ho) {
			if (!n_headers_out) {
				ho = &t->headers_out[0];

				ho->ptr0 = hi_ptr0;
				ho->ptr = hi_ptr;

				ho_ptr = hi_ptr;
				ho_nbytes = n_bytes;

				n_headers_out = 1;

				continue;
			} else {
				ho = &t->headers_out[n_headers_out - 1];

				ho_ptr = ho->ptr;
				ho_nbytes = ho->n_bytes;
			}
		}

		if (ho_ptr + ho_nbytes == hi_ptr) {
			ho_nbytes += n_bytes;
		} else {
			ho->n_bytes = ho_nbytes;

			ho++;
			ho->ptr0 = hi_ptr0;
			ho->ptr = hi_ptr;

			ho_ptr = hi_ptr;
			ho_nbytes = n_bytes;

			n_headers_out++;
		}
	}

	if (ho)
		ho->n_bytes = ho_nbytes;
	t->n_headers_out = n_headers_out;
}

static inline void
__instr_hdr_emit_exec(struct rte_swx_pipeline *p,
		      struct thread *t,
		      const struct instruction *ip)
{
	__instr_hdr_emit_many_exec(p, t, ip, 1);
}

static inline void
__instr_hdr_emit_tx_exec(struct rte_swx_pipeline *p,
			 struct thread *t,
			 const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 1);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit2_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 2);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit3_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 3);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit4_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 4);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit5_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 5);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit6_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 6);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit7_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 7);
	__instr_tx_exec(p, t, ip);
}

static inline void
__instr_hdr_emit8_tx_exec(struct rte_swx_pipeline *p,
			  struct thread *t,
			  const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 9 instructions are fused. ***\n", p->thread_id);

	__instr_hdr_emit_many_exec(p, t, ip, 8);
	__instr_tx_exec(p, t, ip);
}

/*
 * validate.
 */
static inline void
__instr_hdr_validate_exec(struct rte_swx_pipeline *p __rte_unused,
			  struct thread *t,
			  const struct instruction *ip)
{
	uint32_t header_id = ip->valid.header_id;

	TRACE("[Thread %2u] validate header %u\n", p->thread_id, header_id);

	/* Headers. */
	t->valid_headers = MASK64_BIT_SET(t->valid_headers, header_id);
}

/*
 * invalidate.
 */
static inline void
__instr_hdr_invalidate_exec(struct rte_swx_pipeline *p __rte_unused,
			    struct thread *t,
			    const struct instruction *ip)
{
	uint32_t header_id = ip->valid.header_id;

	TRACE("[Thread %2u] invalidate header %u\n", p->thread_id, header_id);

	/* Headers. */
	t->valid_headers = MASK64_BIT_CLR(t->valid_headers, header_id);
}

/*
 * learn.
 */
static inline void
__instr_learn_exec(struct rte_swx_pipeline *p,
		   struct thread *t,
		   const struct instruction *ip)
{
	uint64_t action_id = ip->learn.action_id;
	uint32_t mf_offset = ip->learn.mf_offset;
	uint32_t learner_id = t->learner_id;
	struct rte_swx_table_state *ts = &t->table_state[p->n_tables +
		p->n_selectors + learner_id];
	struct learner_runtime *l = &t->learners[learner_id];
	struct learner_statistics *stats = &p->learner_stats[learner_id];
	uint32_t status;

	/* Table. */
	status = rte_swx_table_learner_add(ts->obj,
					   l->mailbox,
					   t->time,
					   action_id,
					   &t->metadata[mf_offset]);

	TRACE("[Thread %2u] learner %u learn %s\n",
	      p->thread_id,
	      learner_id,
	      status ? "ok" : "error");

	stats->n_pkts_learn[status] += 1;
}

/*
 * forget.
 */
static inline void
__instr_forget_exec(struct rte_swx_pipeline *p,
		    struct thread *t,
		    const struct instruction *ip __rte_unused)
{
	uint32_t learner_id = t->learner_id;
	struct rte_swx_table_state *ts = &t->table_state[p->n_tables +
		p->n_selectors + learner_id];
	struct learner_runtime *l = &t->learners[learner_id];
	struct learner_statistics *stats = &p->learner_stats[learner_id];

	/* Table. */
	rte_swx_table_learner_delete(ts->obj, l->mailbox);

	TRACE("[Thread %2u] learner %u forget\n",
	      p->thread_id,
	      learner_id);

	stats->n_pkts_forget += 1;
}

/*
 * extern.
 */
static inline uint32_t
__instr_extern_obj_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	uint32_t obj_id = ip->ext_obj.ext_obj_id;
	uint32_t func_id = ip->ext_obj.func_id;
	struct extern_obj_runtime *obj = &t->extern_objs[obj_id];
	rte_swx_extern_type_member_func_t func = obj->funcs[func_id];
	uint32_t done;

	TRACE("[Thread %2u] extern obj %u member func %u\n",
	      p->thread_id,
	      obj_id,
	      func_id);

	done = func(obj->obj, obj->mailbox);

	return done;
}

static inline uint32_t
__instr_extern_func_exec(struct rte_swx_pipeline *p __rte_unused,
			 struct thread *t,
			 const struct instruction *ip)
{
	uint32_t ext_func_id = ip->ext_func.ext_func_id;
	struct extern_func_runtime *ext_func = &t->extern_funcs[ext_func_id];
	rte_swx_extern_func_t func = ext_func->func;
	uint32_t done;

	TRACE("[Thread %2u] extern func %u\n",
	      p->thread_id,
	      ext_func_id);

	done = func(ext_func->mailbox);

	return done;
}

/*
 * mov.
 */
static inline void
__instr_mov_exec(struct rte_swx_pipeline *p __rte_unused,
		 struct thread *t,
		 const struct instruction *ip)
{
	TRACE("[Thread %2u] mov\n", p->thread_id);

	MOV(t, ip);
}

static inline void
__instr_mov_mh_exec(struct rte_swx_pipeline *p __rte_unused,
		    struct thread *t,
		    const struct instruction *ip)
{
	TRACE("[Thread %2u] mov (mh)\n", p->thread_id);

	MOV_MH(t, ip);
}

static inline void
__instr_mov_hm_exec(struct rte_swx_pipeline *p __rte_unused,
		    struct thread *t,
		    const struct instruction *ip)
{
	TRACE("[Thread %2u] mov (hm)\n", p->thread_id);

	MOV_HM(t, ip);
}

static inline void
__instr_mov_hh_exec(struct rte_swx_pipeline *p __rte_unused,
		    struct thread *t,
		    const struct instruction *ip)
{
	TRACE("[Thread %2u] mov (hh)\n", p->thread_id);

	MOV_HH(t, ip);
}

static inline void
__instr_mov_i_exec(struct rte_swx_pipeline *p __rte_unused,
		   struct thread *t,
		   const struct instruction *ip)
{
	TRACE("[Thread %2u] mov m.f %" PRIx64 "\n", p->thread_id, ip->mov.src_val);

	MOV_I(t, ip);
}

/*
 * dma.
 */
static inline void
__instr_dma_ht_many_exec(struct rte_swx_pipeline *p __rte_unused,
			 struct thread *t,
			 const struct instruction *ip,
			 uint32_t n_dma)
{
	uint8_t *action_data = t->structs[0];
	uint64_t valid_headers = t->valid_headers;
	uint32_t i;

	for (i = 0; i < n_dma; i++) {
		uint32_t header_id = ip->dma.dst.header_id[i];
		uint32_t struct_id = ip->dma.dst.struct_id[i];
		uint32_t offset = ip->dma.src.offset[i];
		uint32_t n_bytes = ip->dma.n_bytes[i];

		struct header_runtime *h = &t->headers[header_id];
		uint8_t *h_ptr0 = h->ptr0;
		uint8_t *h_ptr = t->structs[struct_id];

		void *dst = MASK64_BIT_GET(valid_headers, header_id) ?
			h_ptr : h_ptr0;
		void *src = &action_data[offset];

		TRACE("[Thread %2u] dma h.s t.f\n", p->thread_id);

		/* Headers. */
		memcpy(dst, src, n_bytes);
		t->structs[struct_id] = dst;
		valid_headers = MASK64_BIT_SET(valid_headers, header_id);
	}

	t->valid_headers = valid_headers;
}

static inline void
__instr_dma_ht_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	__instr_dma_ht_many_exec(p, t, ip, 1);
}

static inline void
__instr_dma_ht2_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 2);
}

static inline void
__instr_dma_ht3_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 3);
}

static inline void
__instr_dma_ht4_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 4);
}

static inline void
__instr_dma_ht5_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 5);
}

static inline void
__instr_dma_ht6_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 6);
}

static inline void
__instr_dma_ht7_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 7);
}

static inline void
__instr_dma_ht8_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n", p->thread_id);

	__instr_dma_ht_many_exec(p, t, ip, 8);
}

/*
 * alu.
 */
static inline void
__instr_alu_add_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] add\n", p->thread_id);

	ALU(t, ip, +);
}

static inline void
__instr_alu_add_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] add (mh)\n", p->thread_id);

	ALU_MH(t, ip, +);
}

static inline void
__instr_alu_add_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] add (hm)\n", p->thread_id);

	ALU_HM(t, ip, +);
}

static inline void
__instr_alu_add_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] add (hh)\n", p->thread_id);

	ALU_HH(t, ip, +);
}

static inline void
__instr_alu_add_mi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] add (mi)\n", p->thread_id);

	ALU_MI(t, ip, +);
}

static inline void
__instr_alu_add_hi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] add (hi)\n", p->thread_id);

	ALU_HI(t, ip, +);
}

static inline void
__instr_alu_sub_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] sub\n", p->thread_id);

	ALU(t, ip, -);
}

static inline void
__instr_alu_sub_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] sub (mh)\n", p->thread_id);

	ALU_MH(t, ip, -);
}

static inline void
__instr_alu_sub_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] sub (hm)\n", p->thread_id);

	ALU_HM(t, ip, -);
}

static inline void
__instr_alu_sub_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] sub (hh)\n", p->thread_id);

	ALU_HH(t, ip, -);
}

static inline void
__instr_alu_sub_mi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] sub (mi)\n", p->thread_id);

	ALU_MI(t, ip, -);
}

static inline void
__instr_alu_sub_hi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] sub (hi)\n", p->thread_id);

	ALU_HI(t, ip, -);
}

static inline void
__instr_alu_shl_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] shl\n", p->thread_id);

	ALU(t, ip, <<);
}

static inline void
__instr_alu_shl_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shl (mh)\n", p->thread_id);

	ALU_MH(t, ip, <<);
}

static inline void
__instr_alu_shl_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shl (hm)\n", p->thread_id);

	ALU_HM(t, ip, <<);
}

static inline void
__instr_alu_shl_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shl (hh)\n", p->thread_id);

	ALU_HH(t, ip, <<);
}

static inline void
__instr_alu_shl_mi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shl (mi)\n", p->thread_id);

	ALU_MI(t, ip, <<);
}

static inline void
__instr_alu_shl_hi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shl (hi)\n", p->thread_id);

	ALU_HI(t, ip, <<);
}

static inline void
__instr_alu_shr_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] shr\n", p->thread_id);

	ALU(t, ip, >>);
}

static inline void
__instr_alu_shr_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shr (mh)\n", p->thread_id);

	ALU_MH(t, ip, >>);
}

static inline void
__instr_alu_shr_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shr (hm)\n", p->thread_id);

	ALU_HM(t, ip, >>);
}

static inline void
__instr_alu_shr_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shr (hh)\n", p->thread_id);

	ALU_HH(t, ip, >>);
}

static inline void
__instr_alu_shr_mi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shr (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, >>);
}

static inline void
__instr_alu_shr_hi_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] shr (hi)\n", p->thread_id);

	ALU_HI(t, ip, >>);
}

static inline void
__instr_alu_and_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] and\n", p->thread_id);

	ALU(t, ip, &);
}

static inline void
__instr_alu_and_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] and (mh)\n", p->thread_id);

	ALU_MH(t, ip, &);
}

static inline void
__instr_alu_and_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] and (hm)\n", p->thread_id);

	ALU_HM_FAST(t, ip, &);
}

static inline void
__instr_alu_and_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] and (hh)\n", p->thread_id);

	ALU_HH_FAST(t, ip, &);
}

static inline void
__instr_alu_and_i_exec(struct rte_swx_pipeline *p __rte_unused,
		       struct thread *t,
		       const struct instruction *ip)
{
	TRACE("[Thread %2u] and (i)\n", p->thread_id);

	ALU_I(t, ip, &);
}

static inline void
__instr_alu_or_exec(struct rte_swx_pipeline *p __rte_unused,
		    struct thread *t,
		    const struct instruction *ip)
{
	TRACE("[Thread %2u] or\n", p->thread_id);

	ALU(t, ip, |);
}

static inline void
__instr_alu_or_mh_exec(struct rte_swx_pipeline *p __rte_unused,
		       struct thread *t,
		       const struct instruction *ip)
{
	TRACE("[Thread %2u] or (mh)\n", p->thread_id);

	ALU_MH(t, ip, |);
}

static inline void
__instr_alu_or_hm_exec(struct rte_swx_pipeline *p __rte_unused,
		       struct thread *t,
		       const struct instruction *ip)
{
	TRACE("[Thread %2u] or (hm)\n", p->thread_id);

	ALU_HM_FAST(t, ip, |);
}

static inline void
__instr_alu_or_hh_exec(struct rte_swx_pipeline *p __rte_unused,
		       struct thread *t,
		       const struct instruction *ip)
{
	TRACE("[Thread %2u] or (hh)\n", p->thread_id);

	ALU_HH_FAST(t, ip, |);
}

static inline void
__instr_alu_or_i_exec(struct rte_swx_pipeline *p __rte_unused,
		      struct thread *t,
		      const struct instruction *ip)
{
	TRACE("[Thread %2u] or (i)\n", p->thread_id);

	ALU_I(t, ip, |);
}

static inline void
__instr_alu_xor_exec(struct rte_swx_pipeline *p __rte_unused,
		     struct thread *t,
		     const struct instruction *ip)
{
	TRACE("[Thread %2u] xor\n", p->thread_id);

	ALU(t, ip, ^);
}

static inline void
__instr_alu_xor_mh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] xor (mh)\n", p->thread_id);

	ALU_MH(t, ip, ^);
}

static inline void
__instr_alu_xor_hm_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] xor (hm)\n", p->thread_id);

	ALU_HM_FAST(t, ip, ^);
}

static inline void
__instr_alu_xor_hh_exec(struct rte_swx_pipeline *p __rte_unused,
			struct thread *t,
			const struct instruction *ip)
{
	TRACE("[Thread %2u] xor (hh)\n", p->thread_id);

	ALU_HH_FAST(t, ip, ^);
}

static inline void
__instr_alu_xor_i_exec(struct rte_swx_pipeline *p __rte_unused,
		       struct thread *t,
		       const struct instruction *ip)
{
	TRACE("[Thread %2u] xor (i)\n", p->thread_id);

	ALU_I(t, ip, ^);
}

static inline void
__instr_alu_ckadd_field_exec(struct rte_swx_pipeline *p __rte_unused,
			     struct thread *t,
			     const struct instruction *ip)
{
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr, dst;
	uint64_t *src64_ptr, src64, src64_mask, src;
	uint64_t r;

	TRACE("[Thread %2u] ckadd (field)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];
	dst = *dst16_ptr;

	src_struct = t->structs[ip->alu.src.struct_id];
	src64_ptr = (uint64_t *)&src_struct[ip->alu.src.offset];
	src64 = *src64_ptr;
	src64_mask = UINT64_MAX >> (64 - ip->alu.src.n_bits);
	src = src64 & src64_mask;

	r = dst;
	r = ~r & 0xFFFF;

	/* The first input (r) is a 16-bit number. The second and the third
	 * inputs are 32-bit numbers. In the worst case scenario, the sum of the
	 * three numbers (output r) is a 34-bit number.
	 */
	r += (src >> 32) + (src & 0xFFFFFFFF);

	/* The first input is a 16-bit number. The second input is an 18-bit
	 * number. In the worst case scenario, the sum of the two numbers is a
	 * 19-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 3-bit number (0 .. 7). Their sum is a 17-bit number (0 .. 0x10006).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x10006), the output r is (0 .. 7). So no carry bit can be generated,
	 * therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;
}

static inline void
__instr_alu_cksub_field_exec(struct rte_swx_pipeline *p __rte_unused,
			     struct thread *t,
			     const struct instruction *ip)
{
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr, dst;
	uint64_t *src64_ptr, src64, src64_mask, src;
	uint64_t r;

	TRACE("[Thread %2u] cksub (field)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];
	dst = *dst16_ptr;

	src_struct = t->structs[ip->alu.src.struct_id];
	src64_ptr = (uint64_t *)&src_struct[ip->alu.src.offset];
	src64 = *src64_ptr;
	src64_mask = UINT64_MAX >> (64 - ip->alu.src.n_bits);
	src = src64 & src64_mask;

	r = dst;
	r = ~r & 0xFFFF;

	/* Subtraction in 1's complement arithmetic (i.e. a '- b) is the same as
	 * the following sequence of operations in 2's complement arithmetic:
	 *    a '- b = (a - b) % 0xFFFF.
	 *
	 * In order to prevent an underflow for the below subtraction, in which
	 * a 33-bit number (the subtrahend) is taken out of a 16-bit number (the
	 * minuend), we first add a multiple of the 0xFFFF modulus to the
	 * minuend. The number we add to the minuend needs to be a 34-bit number
	 * or higher, so for readability reasons we picked the 36-bit multiple.
	 * We are effectively turning the 16-bit minuend into a 36-bit number:
	 *    (a - b) % 0xFFFF = (a + 0xFFFF00000 - b) % 0xFFFF.
	 */
	r += 0xFFFF00000ULL; /* The output r is a 36-bit number. */

	/* A 33-bit number is subtracted from a 36-bit number (the input r). The
	 * result (the output r) is a 36-bit number.
	 */
	r -= (src >> 32) + (src & 0xFFFFFFFF);

	/* The first input is a 16-bit number. The second input is a 20-bit
	 * number. Their sum is a 21-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 5-bit number (0 .. 31). The sum is a 17-bit number (0 .. 0x1001E).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1001E), the output r is (0 .. 31). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;
}

static inline void
__instr_alu_ckadd_struct20_exec(struct rte_swx_pipeline *p __rte_unused,
				struct thread *t,
				const struct instruction *ip)
{
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr;
	uint32_t *src32_ptr;
	uint64_t r0, r1;

	TRACE("[Thread %2u] ckadd (struct of 20 bytes)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];

	src_struct = t->structs[ip->alu.src.struct_id];
	src32_ptr = (uint32_t *)&src_struct[0];

	r0 = src32_ptr[0]; /* r0 is a 32-bit number. */
	r1 = src32_ptr[1]; /* r1 is a 32-bit number. */
	r0 += src32_ptr[2]; /* The output r0 is a 33-bit number. */
	r1 += src32_ptr[3]; /* The output r1 is a 33-bit number. */
	r0 += r1 + src32_ptr[4]; /* The output r0 is a 35-bit number. */

	/* The first input is a 16-bit number. The second input is a 19-bit
	 * number. Their sum is a 20-bit number.
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 4-bit number (0 .. 15). The sum is a 17-bit number (0 .. 0x1000E).
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1000E), the output r is (0 .. 15). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	r0 = ~r0 & 0xFFFF;
	r0 = r0 ? r0 : 0xFFFF;

	*dst16_ptr = (uint16_t)r0;
}

static inline void
__instr_alu_ckadd_struct_exec(struct rte_swx_pipeline *p __rte_unused,
			      struct thread *t,
			      const struct instruction *ip)
{
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr;
	uint32_t *src32_ptr;
	uint64_t r = 0;
	uint32_t i;

	TRACE("[Thread %2u] ckadd (struct)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];

	src_struct = t->structs[ip->alu.src.struct_id];
	src32_ptr = (uint32_t *)&src_struct[0];

	/* The max number of 32-bit words in a 256-byte header is 8 = 2^3.
	 * Therefore, in the worst case scenario, a 35-bit number is added to a
	 * 16-bit number (the input r), so the output r is 36-bit number.
	 */
	for (i = 0; i < ip->alu.src.n_bits / 32; i++, src32_ptr++)
		r += *src32_ptr;

	/* The first input is a 16-bit number. The second input is a 20-bit
	 * number. Their sum is a 21-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 5-bit number (0 .. 31). The sum is a 17-bit number (0 .. 0x1000E).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1001E), the output r is (0 .. 31). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;
}

/*
 * Register array.
 */
static inline uint64_t *
instr_regarray_regarray(struct rte_swx_pipeline *p, const struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];
	return r->regarray;
}

static inline uint64_t
instr_regarray_idx_hbo(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint8_t *idx_struct = t->structs[ip->regarray.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->regarray.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx64_mask = UINT64_MAX >> (64 - ip->regarray.idx.n_bits);
	uint64_t idx = idx64 & idx64_mask & r->size_mask;

	return idx;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint64_t
instr_regarray_idx_nbo(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint8_t *idx_struct = t->structs[ip->regarray.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->regarray.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx = (ntoh64(idx64) >> (64 - ip->regarray.idx.n_bits)) & r->size_mask;

	return idx;
}

#else

#define instr_regarray_idx_nbo instr_regarray_idx_hbo

#endif

static inline uint64_t
instr_regarray_idx_imm(struct rte_swx_pipeline *p, const struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint64_t idx = ip->regarray.idx_val & r->size_mask;

	return idx;
}

static inline uint64_t
instr_regarray_src_hbo(struct thread *t, const struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->regarray.dstsrc.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);
	uint64_t src = src64 & src64_mask;

	return src;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint64_t
instr_regarray_src_nbo(struct thread *t, const struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->regarray.dstsrc.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src = ntoh64(src64) >> (64 - ip->regarray.dstsrc.n_bits);

	return src;
}

#else

#define instr_regarray_src_nbo instr_regarray_src_hbo

#endif

static inline void
instr_regarray_dst_hbo_src_hbo_set(struct thread *t, const struct instruction *ip, uint64_t src)
{
	uint8_t *dst_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->regarray.dstsrc.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);

	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);

}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline void
instr_regarray_dst_nbo_src_hbo_set(struct thread *t, const struct instruction *ip, uint64_t src)
{
	uint8_t *dst_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->regarray.dstsrc.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);

	src = hton64(src) >> (64 - ip->regarray.dstsrc.n_bits);
	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);
}

#else

#define instr_regarray_dst_nbo_src_hbo_set instr_regarray_dst_hbo_src_hbo_set

#endif

static inline void
__instr_regprefetch_rh_exec(struct rte_swx_pipeline *p,
			    struct thread *t,
			    const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[h])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	rte_prefetch0(&regarray[idx]);
}

static inline void
__instr_regprefetch_rm_exec(struct rte_swx_pipeline *p,
			    struct thread *t,
			    const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[m])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	rte_prefetch0(&regarray[idx]);
}

static inline void
__instr_regprefetch_ri_exec(struct rte_swx_pipeline *p,
			    struct thread *t __rte_unused,
			    const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[i])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	rte_prefetch0(&regarray[idx]);
}

static inline void
__instr_regrd_hrh_exec(struct rte_swx_pipeline *p,
		       struct thread *t,
		       const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[h])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regrd_hrm_exec(struct rte_swx_pipeline *p,
		       struct thread *t,
		       const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[m])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regrd_mrh_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (m = r[h])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regrd_mrm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (m = r[m])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regrd_hri_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[i])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regrd_mri_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (m = r[i])\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);
}

static inline void
__instr_regwr_rhh_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rhm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rmh_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rmm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rhi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;
}

static inline void
__instr_regwr_rmi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;
}

static inline void
__instr_regwr_rih_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rim_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;
}

static inline void
__instr_regwr_rii_exec(struct rte_swx_pipeline *p,
		       struct thread *t __rte_unused,
		       const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;
}

static inline void
__instr_regadd_rhh_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rhm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rmh_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rmm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rhi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;
}

static inline void
__instr_regadd_rmi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;
}

static inline void
__instr_regadd_rih_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += h)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rim_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += m)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;
}

static inline void
__instr_regadd_rii_exec(struct rte_swx_pipeline *p,
			struct thread *t __rte_unused,
			const struct instruction *ip)
{
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += i)\n", p->thread_id);

	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;
}

/*
 * metarray.
 */
static inline struct meter *
instr_meter_idx_hbo(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint8_t *idx_struct = t->structs[ip->meter.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->meter.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx64_mask = UINT64_MAX >> (64 - (ip)->meter.idx.n_bits);
	uint64_t idx = idx64 & idx64_mask & r->size_mask;

	return &r->metarray[idx];
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline struct meter *
instr_meter_idx_nbo(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint8_t *idx_struct = t->structs[ip->meter.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->meter.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx = (ntoh64(idx64) >> (64 - ip->meter.idx.n_bits)) & r->size_mask;

	return &r->metarray[idx];
}

#else

#define instr_meter_idx_nbo instr_meter_idx_hbo

#endif

static inline struct meter *
instr_meter_idx_imm(struct rte_swx_pipeline *p, const struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint64_t idx =  ip->meter.idx_val & r->size_mask;

	return &r->metarray[idx];
}

static inline uint32_t
instr_meter_length_hbo(struct thread *t, const struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.length.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.length.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->meter.length.n_bits);
	uint64_t src = src64 & src64_mask;

	return (uint32_t)src;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint32_t
instr_meter_length_nbo(struct thread *t, const struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.length.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.length.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src = ntoh64(src64) >> (64 - ip->meter.length.n_bits);

	return (uint32_t)src;
}

#else

#define instr_meter_length_nbo instr_meter_length_hbo

#endif

static inline enum rte_color
instr_meter_color_in_hbo(struct thread *t, const struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.color_in.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.color_in.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - ip->meter.color_in.n_bits);
	uint64_t src = src64 & src64_mask;

	return (enum rte_color)src;
}

static inline void
instr_meter_color_out_hbo_set(struct thread *t,
			      const struct instruction *ip,
			      enum rte_color color_out)
{
	uint8_t *dst_struct = t->structs[ip->meter.color_out.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->meter.color_out.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->meter.color_out.n_bits);

	uint64_t src = (uint64_t)color_out;

	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);
}

static inline void
__instr_metprefetch_h_exec(struct rte_swx_pipeline *p,
			   struct thread *t,
			   const struct instruction *ip)
{
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (h)\n", p->thread_id);

	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m);
}

static inline void
__instr_metprefetch_m_exec(struct rte_swx_pipeline *p,
			   struct thread *t,
			   const struct instruction *ip)
{
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (m)\n", p->thread_id);

	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m);
}

static inline void
__instr_metprefetch_i_exec(struct rte_swx_pipeline *p,
			   struct thread *t __rte_unused,
			   const struct instruction *ip)
{
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (i)\n", p->thread_id);

	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m);
}

static inline void
__instr_meter_hhm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hhm)\n", p->thread_id);

	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_hhi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hhi)\n", p->thread_id);

	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_hmm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hmm)\n", p->thread_id);

	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_hmi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hmi)\n", p->thread_id);

	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_mhm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mhm)\n", p->thread_id);

	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_mhi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mhi)\n", p->thread_id);

	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_mmm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mmm)\n", p->thread_id);

	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_mmi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mmi)\n", p->thread_id);

	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_ihm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (ihm)\n", p->thread_id);

	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_ihi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (ihi)\n", p->thread_id);

	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_imm_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (imm)\n", p->thread_id);

	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

static inline void
__instr_meter_imi_exec(struct rte_swx_pipeline *p, struct thread *t, const struct instruction *ip)
{
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (imi)\n", p->thread_id);

	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;
}

#endif
