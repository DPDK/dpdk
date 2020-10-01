/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/queue.h>
#include <arpa/inet.h>

#include <rte_common.h>
#include <rte_prefetch.h>
#include <rte_byteorder.h>

#include "rte_swx_pipeline.h"
#include "rte_swx_ctl.h"

#define CHECK(condition, err_code)                                             \
do {                                                                           \
	if (!(condition))                                                      \
		return -(err_code);                                            \
} while (0)

#define CHECK_NAME(name, err_code)                                             \
	CHECK((name) && (name)[0], err_code)

#ifndef TRACE_LEVEL
#define TRACE_LEVEL 0
#endif

#if TRACE_LEVEL
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

#define ntoh64(x) rte_be_to_cpu_64(x)
#define hton64(x) rte_cpu_to_be_64(x)

/*
 * Struct.
 */
struct field {
	char name[RTE_SWX_NAME_SIZE];
	uint32_t n_bits;
	uint32_t offset;
};

struct struct_type {
	TAILQ_ENTRY(struct_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct field *fields;
	uint32_t n_fields;
	uint32_t n_bits;
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

	/* tx m.port_out */
	INSTR_TX,

	/* extract h.header */
	INSTR_HDR_EXTRACT,
	INSTR_HDR_EXTRACT2,
	INSTR_HDR_EXTRACT3,
	INSTR_HDR_EXTRACT4,
	INSTR_HDR_EXTRACT5,
	INSTR_HDR_EXTRACT6,
	INSTR_HDR_EXTRACT7,
	INSTR_HDR_EXTRACT8,

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
	INSTR_MOV,   /* dst = MEF, src = MEFT */
	INSTR_MOV_S, /* (dst, src) = (MEF, H) or (dst, src) = (H, MEFT) */
	INSTR_MOV_I, /* dst = HMEF, src = I */

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
	INSTR_ALU_CKADD_STRUCT,   /* src = h.hdeader, with any sizeof(header) */

	/* cksub dst src
	 * dst = dst '- src
	 * dst = H, src = H
	 */
	INSTR_ALU_CKSUB_FIELD,

	/* and dst src
	 * dst &= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_AND,   /* dst = MEF, src = MEFT */
	INSTR_ALU_AND_S, /* (dst, src) = (MEF, H) or (dst, src) = (H, MEFT) */
	INSTR_ALU_AND_I, /* dst = HMEF, src = I */

	/* or dst src
	 * dst |= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_OR,   /* dst = MEF, src = MEFT */
	INSTR_ALU_OR_S, /* (dst, src) = (MEF, H) or (dst, src) = (H, MEFT) */
	INSTR_ALU_OR_I, /* dst = HMEF, src = I */

	/* xor dst src
	 * dst ^= src
	 * dst = HMEF, src = HMEFTI
	 */
	INSTR_ALU_XOR,   /* dst = MEF, src = MEFT */
	INSTR_ALU_XOR_S, /* (dst, src) = (MEF, H) or (dst, src) = (H, MEFT) */
	INSTR_ALU_XOR_I, /* dst = HMEF, src = I */

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
};

struct instr_operand {
	uint8_t struct_id;
	uint8_t n_bits;
	uint8_t offset;
	uint8_t pad;
};

struct instr_io {
	struct {
		uint8_t offset;
		uint8_t n_bits;
		uint8_t pad[2];
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

struct instr_dst_src {
	struct instr_operand dst;
	union {
		struct instr_operand src;
		uint32_t src_val;
	};
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

struct instruction {
	enum instruction_type type;
	union {
		struct instr_io io;
		struct instr_hdr_validity valid;
		struct instr_dst_src mov;
		struct instr_dma dma;
		struct instr_dst_src alu;
	};
};

struct instruction_data {
	char label[RTE_SWX_NAME_SIZE];
	char jmp_label[RTE_SWX_NAME_SIZE];
	uint32_t n_users; /* user = jmp instruction to this instruction. */
	int invalid;
};

/*
 * Action.
 */
struct action {
	TAILQ_ENTRY(action) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *st;
	struct instruction *instructions;
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
	int is_header; /* Only valid when n_fields > 0. */
	struct header *header; /* Only valid when n_fields > 0. */

	/* Action. */
	struct action **actions;
	struct action *default_action;
	uint8_t *default_action_data;
	uint32_t n_actions;
	int default_action_is_const;
	uint32_t action_data_size_max;

	uint32_t size;
	uint32_t id;
};

TAILQ_HEAD(table_tailq, table);

struct table_runtime {
	rte_swx_table_lookup_t func;
	void *mailbox;
	uint8_t **key;
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
	struct rte_swx_table_state *table_state;
	uint64_t action_id;
	int hit; /* 0 = Miss, 1 = Hit. */

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

#define ALU_S(thread, ip, operator)  \
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

#define ALU_MH ALU_S

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

#else

#define ALU_S ALU
#define ALU_MH ALU
#define ALU_HM ALU
#define ALU_HH ALU

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

#define MOV_S(thread, ip)  \
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

#else

#define MOV_S MOV

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

	struct port_in_runtime *in;
	struct port_out_runtime *out;
	struct instruction **action_instructions;
	struct rte_swx_table_state *table_state;
	struct instruction *instructions;
	struct thread threads[RTE_SWX_PIPELINE_THREADS_MAX];

	uint32_t n_structs;
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_extern_objs;
	uint32_t n_extern_funcs;
	uint32_t n_actions;
	uint32_t n_tables;
	uint32_t n_headers;
	uint32_t thread_id;
	uint32_t port_id;
	uint32_t n_instructions;
	int build_done;
	int numa_node;
};

/*
 * Struct.
 */
static struct struct_type *
struct_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct struct_type *elem;

	TAILQ_FOREACH(elem, &p->struct_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct field *
struct_type_field_find(struct struct_type *st, const char *name)
{
	uint32_t i;

	for (i = 0; i < st->n_fields; i++) {
		struct field *f = &st->fields[i];

		if (strcmp(f->name, name) == 0)
			return f;
	}

	return NULL;
}

int
rte_swx_pipeline_struct_type_register(struct rte_swx_pipeline *p,
				      const char *name,
				      struct rte_swx_field_params *fields,
				      uint32_t n_fields)
{
	struct struct_type *st;
	uint32_t i;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(fields, EINVAL);
	CHECK(n_fields, EINVAL);

	for (i = 0; i < n_fields; i++) {
		struct rte_swx_field_params *f = &fields[i];
		uint32_t j;

		CHECK_NAME(f->name, EINVAL);
		CHECK(f->n_bits, EINVAL);
		CHECK(f->n_bits <= 64, EINVAL);
		CHECK((f->n_bits & 7) == 0, EINVAL);

		for (j = 0; j < i; j++) {
			struct rte_swx_field_params *f_prev = &fields[j];

			CHECK(strcmp(f->name, f_prev->name), EINVAL);
		}
	}

	CHECK(!struct_type_find(p, name), EEXIST);

	/* Node allocation. */
	st = calloc(1, sizeof(struct struct_type));
	CHECK(st, ENOMEM);

	st->fields = calloc(n_fields, sizeof(struct field));
	if (!st->fields) {
		free(st);
		CHECK(0, ENOMEM);
	}

	/* Node initialization. */
	strcpy(st->name, name);
	for (i = 0; i < n_fields; i++) {
		struct field *dst = &st->fields[i];
		struct rte_swx_field_params *src = &fields[i];

		strcpy(dst->name, src->name);
		dst->n_bits = src->n_bits;
		dst->offset = st->n_bits;

		st->n_bits += src->n_bits;
	}
	st->n_fields = n_fields;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->struct_types, st, node);

	return 0;
}

static int
struct_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		t->structs = calloc(p->n_structs, sizeof(uint8_t *));
		CHECK(t->structs, ENOMEM);
	}

	return 0;
}

static void
struct_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->structs);
		t->structs = NULL;
	}
}

static void
struct_free(struct rte_swx_pipeline *p)
{
	struct_build_free(p);

	/* Struct types. */
	for ( ; ; ) {
		struct struct_type *elem;

		elem = TAILQ_FIRST(&p->struct_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->struct_types, elem, node);
		free(elem->fields);
		free(elem);
	}
}

/*
 * Input port.
 */
static struct port_in_type *
port_in_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct port_in_type *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->port_in_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_port_in_type_register(struct rte_swx_pipeline *p,
				       const char *name,
				       struct rte_swx_port_in_ops *ops)
{
	struct port_in_type *elem;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->free, EINVAL);
	CHECK(ops->pkt_rx, EINVAL);
	CHECK(ops->stats_read, EINVAL);

	CHECK(!port_in_type_find(p, name), EEXIST);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct port_in_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->port_in_types, elem, node);

	return 0;
}

static struct port_in *
port_in_find(struct rte_swx_pipeline *p, uint32_t port_id)
{
	struct port_in *port;

	TAILQ_FOREACH(port, &p->ports_in, node)
		if (port->id == port_id)
			return port;

	return NULL;
}

int
rte_swx_pipeline_port_in_config(struct rte_swx_pipeline *p,
				uint32_t port_id,
				const char *port_type_name,
				void *args)
{
	struct port_in_type *type = NULL;
	struct port_in *port = NULL;
	void *obj = NULL;

	CHECK(p, EINVAL);

	CHECK(!port_in_find(p, port_id), EINVAL);

	CHECK_NAME(port_type_name, EINVAL);
	type = port_in_type_find(p, port_type_name);
	CHECK(type, EINVAL);

	obj = type->ops.create(args);
	CHECK(obj, ENODEV);

	/* Node allocation. */
	port = calloc(1, sizeof(struct port_in));
	CHECK(port, ENOMEM);

	/* Node initialization. */
	port->type = type;
	port->obj = obj;
	port->id = port_id;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->ports_in, port, node);
	if (p->n_ports_in < port_id + 1)
		p->n_ports_in = port_id + 1;

	return 0;
}

static int
port_in_build(struct rte_swx_pipeline *p)
{
	struct port_in *port;
	uint32_t i;

	CHECK(p->n_ports_in, EINVAL);
	CHECK(rte_is_power_of_2(p->n_ports_in), EINVAL);

	for (i = 0; i < p->n_ports_in; i++)
		CHECK(port_in_find(p, i), EINVAL);

	p->in = calloc(p->n_ports_in, sizeof(struct port_in_runtime));
	CHECK(p->in, ENOMEM);

	TAILQ_FOREACH(port, &p->ports_in, node) {
		struct port_in_runtime *in = &p->in[port->id];

		in->pkt_rx = port->type->ops.pkt_rx;
		in->obj = port->obj;
	}

	return 0;
}

static void
port_in_build_free(struct rte_swx_pipeline *p)
{
	free(p->in);
	p->in = NULL;
}

static void
port_in_free(struct rte_swx_pipeline *p)
{
	port_in_build_free(p);

	/* Input ports. */
	for ( ; ; ) {
		struct port_in *port;

		port = TAILQ_FIRST(&p->ports_in);
		if (!port)
			break;

		TAILQ_REMOVE(&p->ports_in, port, node);
		port->type->ops.free(port->obj);
		free(port);
	}

	/* Input port types. */
	for ( ; ; ) {
		struct port_in_type *elem;

		elem = TAILQ_FIRST(&p->port_in_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->port_in_types, elem, node);
		free(elem);
	}
}

/*
 * Output port.
 */
static struct port_out_type *
port_out_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct port_out_type *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->port_out_types, node)
		if (!strcmp(elem->name, name))
			return elem;

	return NULL;
}

int
rte_swx_pipeline_port_out_type_register(struct rte_swx_pipeline *p,
					const char *name,
					struct rte_swx_port_out_ops *ops)
{
	struct port_out_type *elem;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->free, EINVAL);
	CHECK(ops->pkt_tx, EINVAL);
	CHECK(ops->stats_read, EINVAL);

	CHECK(!port_out_type_find(p, name), EEXIST);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct port_out_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->port_out_types, elem, node);

	return 0;
}

static struct port_out *
port_out_find(struct rte_swx_pipeline *p, uint32_t port_id)
{
	struct port_out *port;

	TAILQ_FOREACH(port, &p->ports_out, node)
		if (port->id == port_id)
			return port;

	return NULL;
}

int
rte_swx_pipeline_port_out_config(struct rte_swx_pipeline *p,
				 uint32_t port_id,
				 const char *port_type_name,
				 void *args)
{
	struct port_out_type *type = NULL;
	struct port_out *port = NULL;
	void *obj = NULL;

	CHECK(p, EINVAL);

	CHECK(!port_out_find(p, port_id), EINVAL);

	CHECK_NAME(port_type_name, EINVAL);
	type = port_out_type_find(p, port_type_name);
	CHECK(type, EINVAL);

	obj = type->ops.create(args);
	CHECK(obj, ENODEV);

	/* Node allocation. */
	port = calloc(1, sizeof(struct port_out));
	CHECK(port, ENOMEM);

	/* Node initialization. */
	port->type = type;
	port->obj = obj;
	port->id = port_id;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->ports_out, port, node);
	if (p->n_ports_out < port_id + 1)
		p->n_ports_out = port_id + 1;

	return 0;
}

static int
port_out_build(struct rte_swx_pipeline *p)
{
	struct port_out *port;
	uint32_t i;

	CHECK(p->n_ports_out, EINVAL);

	for (i = 0; i < p->n_ports_out; i++)
		CHECK(port_out_find(p, i), EINVAL);

	p->out = calloc(p->n_ports_out, sizeof(struct port_out_runtime));
	CHECK(p->out, ENOMEM);

	TAILQ_FOREACH(port, &p->ports_out, node) {
		struct port_out_runtime *out = &p->out[port->id];

		out->pkt_tx = port->type->ops.pkt_tx;
		out->flush = port->type->ops.flush;
		out->obj = port->obj;
	}

	return 0;
}

static void
port_out_build_free(struct rte_swx_pipeline *p)
{
	free(p->out);
	p->out = NULL;
}

static void
port_out_free(struct rte_swx_pipeline *p)
{
	port_out_build_free(p);

	/* Output ports. */
	for ( ; ; ) {
		struct port_out *port;

		port = TAILQ_FIRST(&p->ports_out);
		if (!port)
			break;

		TAILQ_REMOVE(&p->ports_out, port, node);
		port->type->ops.free(port->obj);
		free(port);
	}

	/* Output port types. */
	for ( ; ; ) {
		struct port_out_type *elem;

		elem = TAILQ_FIRST(&p->port_out_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->port_out_types, elem, node);
		free(elem);
	}
}

/*
 * Extern object.
 */
static struct extern_type *
extern_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_type *elem;

	TAILQ_FOREACH(elem, &p->extern_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_type_member_func *
extern_type_member_func_find(struct extern_type *type, const char *name)
{
	struct extern_type_member_func *elem;

	TAILQ_FOREACH(elem, &type->funcs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_obj *
extern_obj_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_obj *elem;

	TAILQ_FOREACH(elem, &p->extern_objs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct field *
extern_obj_mailbox_field_parse(struct rte_swx_pipeline *p,
			       const char *name,
			       struct extern_obj **object)
{
	struct extern_obj *obj;
	struct field *f;
	char *obj_name, *field_name;

	if ((name[0] != 'e') || (name[1] != '.'))
		return NULL;

	obj_name = strdup(&name[2]);
	if (!obj_name)
		return NULL;

	field_name = strchr(obj_name, '.');
	if (!field_name) {
		free(obj_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	obj = extern_obj_find(p, obj_name);
	if (!obj) {
		free(obj_name);
		return NULL;
	}

	f = struct_type_field_find(obj->type->mailbox_struct_type, field_name);
	if (!f) {
		free(obj_name);
		return NULL;
	}

	if (object)
		*object = obj;

	free(obj_name);
	return f;
}

int
rte_swx_pipeline_extern_type_register(struct rte_swx_pipeline *p,
	const char *name,
	const char *mailbox_struct_type_name,
	rte_swx_extern_type_constructor_t constructor,
	rte_swx_extern_type_destructor_t destructor)
{
	struct extern_type *elem;
	struct struct_type *mailbox_struct_type;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_type_find(p, name), EEXIST);

	CHECK_NAME(mailbox_struct_type_name, EINVAL);
	mailbox_struct_type = struct_type_find(p, mailbox_struct_type_name);
	CHECK(mailbox_struct_type, EINVAL);

	CHECK(constructor, EINVAL);
	CHECK(destructor, EINVAL);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct extern_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	elem->mailbox_struct_type = mailbox_struct_type;
	elem->constructor = constructor;
	elem->destructor = destructor;
	TAILQ_INIT(&elem->funcs);

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_types, elem, node);

	return 0;
}

int
rte_swx_pipeline_extern_type_member_func_register(struct rte_swx_pipeline *p,
	const char *extern_type_name,
	const char *name,
	rte_swx_extern_type_member_func_t member_func)
{
	struct extern_type *type;
	struct extern_type_member_func *type_member;

	CHECK(p, EINVAL);

	CHECK(extern_type_name, EINVAL);
	type = extern_type_find(p, extern_type_name);
	CHECK(type, EINVAL);
	CHECK(type->n_funcs < RTE_SWX_EXTERN_TYPE_MEMBER_FUNCS_MAX, ENOSPC);

	CHECK(name, EINVAL);
	CHECK(!extern_type_member_func_find(type, name), EEXIST);

	CHECK(member_func, EINVAL);

	/* Node allocation. */
	type_member = calloc(1, sizeof(struct extern_type_member_func));
	CHECK(type_member, ENOMEM);

	/* Node initialization. */
	strcpy(type_member->name, name);
	type_member->func = member_func;
	type_member->id = type->n_funcs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&type->funcs, type_member, node);
	type->n_funcs++;

	return 0;
}

int
rte_swx_pipeline_extern_object_config(struct rte_swx_pipeline *p,
				      const char *extern_type_name,
				      const char *name,
				      const char *args)
{
	struct extern_type *type;
	struct extern_obj *obj;
	void *obj_handle;

	CHECK(p, EINVAL);

	CHECK_NAME(extern_type_name, EINVAL);
	type = extern_type_find(p, extern_type_name);
	CHECK(type, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_obj_find(p, name), EEXIST);

	/* Node allocation. */
	obj = calloc(1, sizeof(struct extern_obj));
	CHECK(obj, ENOMEM);

	/* Object construction. */
	obj_handle = type->constructor(args);
	if (!obj_handle) {
		free(obj);
		CHECK(0, ENODEV);
	}

	/* Node initialization. */
	strcpy(obj->name, name);
	obj->type = type;
	obj->obj = obj_handle;
	obj->struct_id = p->n_structs;
	obj->id = p->n_extern_objs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_objs, obj, node);
	p->n_extern_objs++;
	p->n_structs++;

	return 0;
}

static int
extern_obj_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct extern_obj *obj;

		t->extern_objs = calloc(p->n_extern_objs,
					sizeof(struct extern_obj_runtime));
		CHECK(t->extern_objs, ENOMEM);

		TAILQ_FOREACH(obj, &p->extern_objs, node) {
			struct extern_obj_runtime *r =
				&t->extern_objs[obj->id];
			struct extern_type_member_func *func;
			uint32_t mailbox_size =
				obj->type->mailbox_struct_type->n_bits / 8;

			r->obj = obj->obj;

			r->mailbox = calloc(1, mailbox_size);
			CHECK(r->mailbox, ENOMEM);

			TAILQ_FOREACH(func, &obj->type->funcs, node)
				r->funcs[func->id] = func->func;

			t->structs[obj->struct_id] = r->mailbox;
		}
	}

	return 0;
}

static void
extern_obj_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->extern_objs)
			continue;

		for (j = 0; j < p->n_extern_objs; j++) {
			struct extern_obj_runtime *r = &t->extern_objs[j];

			free(r->mailbox);
		}

		free(t->extern_objs);
		t->extern_objs = NULL;
	}
}

static void
extern_obj_free(struct rte_swx_pipeline *p)
{
	extern_obj_build_free(p);

	/* Extern objects. */
	for ( ; ; ) {
		struct extern_obj *elem;

		elem = TAILQ_FIRST(&p->extern_objs);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_objs, elem, node);
		if (elem->obj)
			elem->type->destructor(elem->obj);
		free(elem);
	}

	/* Extern types. */
	for ( ; ; ) {
		struct extern_type *elem;

		elem = TAILQ_FIRST(&p->extern_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_types, elem, node);

		for ( ; ; ) {
			struct extern_type_member_func *func;

			func = TAILQ_FIRST(&elem->funcs);
			if (!func)
				break;

			TAILQ_REMOVE(&elem->funcs, func, node);
			free(func);
		}

		free(elem);
	}
}

/*
 * Extern function.
 */
static struct extern_func *
extern_func_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_func *elem;

	TAILQ_FOREACH(elem, &p->extern_funcs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct field *
extern_func_mailbox_field_parse(struct rte_swx_pipeline *p,
				const char *name,
				struct extern_func **function)
{
	struct extern_func *func;
	struct field *f;
	char *func_name, *field_name;

	if ((name[0] != 'f') || (name[1] != '.'))
		return NULL;

	func_name = strdup(&name[2]);
	if (!func_name)
		return NULL;

	field_name = strchr(func_name, '.');
	if (!field_name) {
		free(func_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	func = extern_func_find(p, func_name);
	if (!func) {
		free(func_name);
		return NULL;
	}

	f = struct_type_field_find(func->mailbox_struct_type, field_name);
	if (!f) {
		free(func_name);
		return NULL;
	}

	if (function)
		*function = func;

	free(func_name);
	return f;
}

int
rte_swx_pipeline_extern_func_register(struct rte_swx_pipeline *p,
				      const char *name,
				      const char *mailbox_struct_type_name,
				      rte_swx_extern_func_t func)
{
	struct extern_func *f;
	struct struct_type *mailbox_struct_type;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_func_find(p, name), EEXIST);

	CHECK_NAME(mailbox_struct_type_name, EINVAL);
	mailbox_struct_type = struct_type_find(p, mailbox_struct_type_name);
	CHECK(mailbox_struct_type, EINVAL);

	CHECK(func, EINVAL);

	/* Node allocation. */
	f = calloc(1, sizeof(struct extern_func));
	CHECK(func, ENOMEM);

	/* Node initialization. */
	strcpy(f->name, name);
	f->mailbox_struct_type = mailbox_struct_type;
	f->func = func;
	f->struct_id = p->n_structs;
	f->id = p->n_extern_funcs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_funcs, f, node);
	p->n_extern_funcs++;
	p->n_structs++;

	return 0;
}

static int
extern_func_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct extern_func *func;

		/* Memory allocation. */
		t->extern_funcs = calloc(p->n_extern_funcs,
					 sizeof(struct extern_func_runtime));
		CHECK(t->extern_funcs, ENOMEM);

		/* Extern function. */
		TAILQ_FOREACH(func, &p->extern_funcs, node) {
			struct extern_func_runtime *r =
				&t->extern_funcs[func->id];
			uint32_t mailbox_size =
				func->mailbox_struct_type->n_bits / 8;

			r->func = func->func;

			r->mailbox = calloc(1, mailbox_size);
			CHECK(r->mailbox, ENOMEM);

			t->structs[func->struct_id] = r->mailbox;
		}
	}

	return 0;
}

static void
extern_func_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->extern_funcs)
			continue;

		for (j = 0; j < p->n_extern_funcs; j++) {
			struct extern_func_runtime *r = &t->extern_funcs[j];

			free(r->mailbox);
		}

		free(t->extern_funcs);
		t->extern_funcs = NULL;
	}
}

static void
extern_func_free(struct rte_swx_pipeline *p)
{
	extern_func_build_free(p);

	for ( ; ; ) {
		struct extern_func *elem;

		elem = TAILQ_FIRST(&p->extern_funcs);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_funcs, elem, node);
		free(elem);
	}
}

/*
 * Header.
 */
static struct header *
header_find(struct rte_swx_pipeline *p, const char *name)
{
	struct header *elem;

	TAILQ_FOREACH(elem, &p->headers, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct header *
header_parse(struct rte_swx_pipeline *p,
	     const char *name)
{
	if (name[0] != 'h' || name[1] != '.')
		return NULL;

	return header_find(p, &name[2]);
}

static struct field *
header_field_parse(struct rte_swx_pipeline *p,
		   const char *name,
		   struct header **header)
{
	struct header *h;
	struct field *f;
	char *header_name, *field_name;

	if ((name[0] != 'h') || (name[1] != '.'))
		return NULL;

	header_name = strdup(&name[2]);
	if (!header_name)
		return NULL;

	field_name = strchr(header_name, '.');
	if (!field_name) {
		free(header_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	h = header_find(p, header_name);
	if (!h) {
		free(header_name);
		return NULL;
	}

	f = struct_type_field_find(h->st, field_name);
	if (!f) {
		free(header_name);
		return NULL;
	}

	if (header)
		*header = h;

	free(header_name);
	return f;
}

int
rte_swx_pipeline_packet_header_register(struct rte_swx_pipeline *p,
					const char *name,
					const char *struct_type_name)
{
	struct struct_type *st;
	struct header *h;
	size_t n_headers_max;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK_NAME(struct_type_name, EINVAL);

	CHECK(!header_find(p, name), EEXIST);

	st = struct_type_find(p, struct_type_name);
	CHECK(st, EINVAL);

	n_headers_max = RTE_SIZEOF_FIELD(struct thread, valid_headers) * 8;
	CHECK(p->n_headers < n_headers_max, ENOSPC);

	/* Node allocation. */
	h = calloc(1, sizeof(struct header));
	CHECK(h, ENOMEM);

	/* Node initialization. */
	strcpy(h->name, name);
	h->st = st;
	h->struct_id = p->n_structs;
	h->id = p->n_headers;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->headers, h, node);
	p->n_headers++;
	p->n_structs++;

	return 0;
}

static int
header_build(struct rte_swx_pipeline *p)
{
	struct header *h;
	uint32_t n_bytes = 0, i;

	TAILQ_FOREACH(h, &p->headers, node) {
		n_bytes += h->st->n_bits / 8;
	}

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t offset = 0;

		t->headers = calloc(p->n_headers,
				    sizeof(struct header_runtime));
		CHECK(t->headers, ENOMEM);

		t->headers_out = calloc(p->n_headers,
					sizeof(struct header_out_runtime));
		CHECK(t->headers_out, ENOMEM);

		t->header_storage = calloc(1, n_bytes);
		CHECK(t->header_storage, ENOMEM);

		t->header_out_storage = calloc(1, n_bytes);
		CHECK(t->header_out_storage, ENOMEM);

		TAILQ_FOREACH(h, &p->headers, node) {
			uint8_t *header_storage;

			header_storage = &t->header_storage[offset];
			offset += h->st->n_bits / 8;

			t->headers[h->id].ptr0 = header_storage;
			t->structs[h->struct_id] = header_storage;
		}
	}

	return 0;
}

static void
header_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->headers_out);
		t->headers_out = NULL;

		free(t->headers);
		t->headers = NULL;

		free(t->header_out_storage);
		t->header_out_storage = NULL;

		free(t->header_storage);
		t->header_storage = NULL;
	}
}

static void
header_free(struct rte_swx_pipeline *p)
{
	header_build_free(p);

	for ( ; ; ) {
		struct header *elem;

		elem = TAILQ_FIRST(&p->headers);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->headers, elem, node);
		free(elem);
	}
}

/*
 * Meta-data.
 */
static struct field *
metadata_field_parse(struct rte_swx_pipeline *p, const char *name)
{
	if (!p->metadata_st)
		return NULL;

	if (name[0] != 'm' || name[1] != '.')
		return NULL;

	return struct_type_field_find(p->metadata_st, &name[2]);
}

int
rte_swx_pipeline_packet_metadata_register(struct rte_swx_pipeline *p,
					  const char *struct_type_name)
{
	struct struct_type *st = NULL;

	CHECK(p, EINVAL);

	CHECK_NAME(struct_type_name, EINVAL);
	st  = struct_type_find(p, struct_type_name);
	CHECK(st, EINVAL);
	CHECK(!p->metadata_st, EINVAL);

	p->metadata_st = st;
	p->metadata_struct_id = p->n_structs;

	p->n_structs++;

	return 0;
}

static int
metadata_build(struct rte_swx_pipeline *p)
{
	uint32_t n_bytes = p->metadata_st->n_bits / 8;
	uint32_t i;

	/* Thread-level initialization. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint8_t *metadata;

		metadata = calloc(1, n_bytes);
		CHECK(metadata, ENOMEM);

		t->metadata = metadata;
		t->structs[p->metadata_struct_id] = metadata;
	}

	return 0;
}

static void
metadata_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->metadata);
		t->metadata = NULL;
	}
}

static void
metadata_free(struct rte_swx_pipeline *p)
{
	metadata_build_free(p);
}

/*
 * Instruction.
 */
static struct field *
action_field_parse(struct action *action, const char *name);

static struct field *
struct_field_parse(struct rte_swx_pipeline *p,
		   struct action *action,
		   const char *name,
		   uint32_t *struct_id)
{
	struct field *f;

	switch (name[0]) {
	case 'h':
	{
		struct header *header;

		f = header_field_parse(p, name, &header);
		if (!f)
			return NULL;

		*struct_id = header->struct_id;
		return f;
	}

	case 'm':
	{
		f = metadata_field_parse(p, name);
		if (!f)
			return NULL;

		*struct_id = p->metadata_struct_id;
		return f;
	}

	case 't':
	{
		if (!action)
			return NULL;

		f = action_field_parse(action, name);
		if (!f)
			return NULL;

		*struct_id = 0;
		return f;
	}

	case 'e':
	{
		struct extern_obj *obj;

		f = extern_obj_mailbox_field_parse(p, name, &obj);
		if (!f)
			return NULL;

		*struct_id = obj->struct_id;
		return f;
	}

	case 'f':
	{
		struct extern_func *func;

		f = extern_func_mailbox_field_parse(p, name, &func);
		if (!f)
			return NULL;

		*struct_id = func->struct_id;
		return f;
	}

	default:
		return NULL;
	}
}

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

/*
 * rx.
 */
static int
instr_rx_translate(struct rte_swx_pipeline *p,
		   struct action *action,
		   char **tokens,
		   int n_tokens,
		   struct instruction *instr,
		   struct instruction_data *data __rte_unused)
{
	struct field *f;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, tokens[1]);
	CHECK(f, EINVAL);

	instr->type = INSTR_RX;
	instr->io.io.offset = f->offset / 8;
	instr->io.io.n_bits = f->n_bits;
	return 0;
}

static inline void
instr_rx_exec(struct rte_swx_pipeline *p);

static inline void
instr_rx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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
	thread_ip_inc_cond(t, pkt_received);
	thread_yield(p);
}

/*
 * tx.
 */
static int
instr_tx_translate(struct rte_swx_pipeline *p,
		   struct action *action __rte_unused,
		   char **tokens,
		   int n_tokens,
		   struct instruction *instr,
		   struct instruction_data *data __rte_unused)
{
	struct field *f;

	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, tokens[1]);
	CHECK(f, EINVAL);

	instr->type = INSTR_TX;
	instr->io.io.offset = f->offset / 8;
	instr->io.io.n_bits = f->n_bits;
	return 0;
}

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

	/* Header encapsulation (optionally, with prior header decasulation). */
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

	/* Header insertion. */
	/* TBD */

	/* Header extraction. */
	/* TBD */

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
instr_tx_exec(struct rte_swx_pipeline *p);

static inline void
instr_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

/*
 * extract.
 */
static int
instr_hdr_extract_translate(struct rte_swx_pipeline *p,
			    struct action *action,
			    char **tokens,
			    int n_tokens,
			    struct instruction *instr,
			    struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_EXTRACT;
	instr->io.hdr.header_id[0] = h->id;
	instr->io.hdr.struct_id[0] = h->struct_id;
	instr->io.hdr.n_bytes[0] = h->st->n_bits / 8;
	return 0;
}

static inline void
__instr_hdr_extract_exec(struct rte_swx_pipeline *p, uint32_t n_extract);

static inline void
__instr_hdr_extract_exec(struct rte_swx_pipeline *p, uint32_t n_extract)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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
instr_hdr_extract_exec(struct rte_swx_pipeline *p)
{
	__instr_hdr_extract_exec(p, 1);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract2_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 2);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract3_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 3);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract4_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 4);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract5_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 5);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract6_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 6);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract7_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 7);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract8_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_extract_exec(p, 8);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * emit.
 */
static int
instr_hdr_emit_translate(struct rte_swx_pipeline *p,
			 struct action *action __rte_unused,
			 char **tokens,
			 int n_tokens,
			 struct instruction *instr,
			 struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_EMIT;
	instr->io.hdr.header_id[0] = h->id;
	instr->io.hdr.struct_id[0] = h->struct_id;
	instr->io.hdr.n_bytes[0] = h->st->n_bits / 8;
	return 0;
}

static inline void
__instr_hdr_emit_exec(struct rte_swx_pipeline *p, uint32_t n_emit);

static inline void
__instr_hdr_emit_exec(struct rte_swx_pipeline *p, uint32_t n_emit)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t n_headers_out = t->n_headers_out;
	struct header_out_runtime *ho = &t->headers_out[n_headers_out - 1];
	uint8_t *ho_ptr = NULL;
	uint32_t ho_nbytes = 0, i;

	for (i = 0; i < n_emit; i++) {
		uint32_t header_id = ip->io.hdr.header_id[i];
		uint32_t struct_id = ip->io.hdr.struct_id[i];
		uint32_t n_bytes = ip->io.hdr.n_bytes[i];

		struct header_runtime *hi = &t->headers[header_id];
		uint8_t *hi_ptr = t->structs[struct_id];

		TRACE("[Thread %2u]: emit header %u\n",
		      p->thread_id,
		      header_id);

		/* Headers. */
		if (!i) {
			if (!t->n_headers_out) {
				ho = &t->headers_out[0];

				ho->ptr0 = hi->ptr0;
				ho->ptr = hi_ptr;

				ho_ptr = hi_ptr;
				ho_nbytes = n_bytes;

				n_headers_out = 1;

				continue;
			} else {
				ho_ptr = ho->ptr;
				ho_nbytes = ho->n_bytes;
			}
		}

		if (ho_ptr + ho_nbytes == hi_ptr) {
			ho_nbytes += n_bytes;
		} else {
			ho->n_bytes = ho_nbytes;

			ho++;
			ho->ptr0 = hi->ptr0;
			ho->ptr = hi_ptr;

			ho_ptr = hi_ptr;
			ho_nbytes = n_bytes;

			n_headers_out++;
		}
	}

	ho->n_bytes = ho_nbytes;
	t->n_headers_out = n_headers_out;
}

static inline void
instr_hdr_emit_exec(struct rte_swx_pipeline *p)
{
	__instr_hdr_emit_exec(p, 1);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_emit_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 1);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit2_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 2);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit3_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 3);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit4_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 4);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit5_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 5);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit6_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 6);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit7_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 7);
	instr_tx_exec(p);
}

static inline void
instr_hdr_emit8_tx_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 9 instructions are fused. ***\n",
	      p->thread_id);

	__instr_hdr_emit_exec(p, 8);
	instr_tx_exec(p);
}

/*
 * validate.
 */
static int
instr_hdr_validate_translate(struct rte_swx_pipeline *p,
			     struct action *action __rte_unused,
			     char **tokens,
			     int n_tokens,
			     struct instruction *instr,
			     struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_VALIDATE;
	instr->valid.header_id = h->id;
	return 0;
}

static inline void
instr_hdr_validate_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t header_id = ip->valid.header_id;

	TRACE("[Thread %2u] validate header %u\n", p->thread_id, header_id);

	/* Headers. */
	t->valid_headers = MASK64_BIT_SET(t->valid_headers, header_id);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * invalidate.
 */
static int
instr_hdr_invalidate_translate(struct rte_swx_pipeline *p,
			       struct action *action __rte_unused,
			       char **tokens,
			       int n_tokens,
			       struct instruction *instr,
			       struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_INVALIDATE;
	instr->valid.header_id = h->id;
	return 0;
}

static inline void
instr_hdr_invalidate_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t header_id = ip->valid.header_id;

	TRACE("[Thread %2u] invalidate header %u\n", p->thread_id, header_id);

	/* Headers. */
	t->valid_headers = MASK64_BIT_CLR(t->valid_headers, header_id);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * mov.
 */
static int
instr_mov_translate(struct rte_swx_pipeline *p,
		    struct action *action,
		    char **tokens,
		    int n_tokens,
		    struct instruction *instr,
		    struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* MOV or MOV_S. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_MOV;
		if ((dst[0] == 'h' && src[0] != 'h') ||
		    (dst[0] != 'h' && src[0] == 'h'))
			instr->type = INSTR_MOV_S;

		instr->mov.dst.struct_id = (uint8_t)dst_struct_id;
		instr->mov.dst.n_bits = fdst->n_bits;
		instr->mov.dst.offset = fdst->offset / 8;
		instr->mov.src.struct_id = (uint8_t)src_struct_id;
		instr->mov.src.n_bits = fsrc->n_bits;
		instr->mov.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* MOV_I. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = htonl(src_val);

	instr->type = INSTR_MOV_I;
	instr->mov.dst.struct_id = (uint8_t)dst_struct_id;
	instr->mov.dst.n_bits = fdst->n_bits;
	instr->mov.dst.offset = fdst->offset / 8;
	instr->mov.src_val = (uint32_t)src_val;
	return 0;
}

static inline void
instr_mov_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] mov\n",
	      p->thread_id);

	MOV(t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_s_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] mov (s)\n",
	      p->thread_id);

	MOV_S(t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] mov m.f %x\n",
	      p->thread_id,
	      ip->mov.src_val);

	MOV_I(t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * dma.
 */
static int
instr_dma_translate(struct rte_swx_pipeline *p,
		    struct action *action,
		    char **tokens,
		    int n_tokens,
		    struct instruction *instr,
		    struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1];
	char *src = tokens[2];
	struct header *h;
	struct field *tf;

	CHECK(action, EINVAL);
	CHECK(n_tokens == 3, EINVAL);

	h = header_parse(p, dst);
	CHECK(h, EINVAL);

	tf = action_field_parse(action, src);
	CHECK(tf, EINVAL);

	instr->type = INSTR_DMA_HT;
	instr->dma.dst.header_id[0] = h->id;
	instr->dma.dst.struct_id[0] = h->struct_id;
	instr->dma.n_bytes[0] = h->st->n_bits / 8;
	instr->dma.src.offset[0] = tf->offset / 8;

	return 0;
}

static inline void
__instr_dma_ht_exec(struct rte_swx_pipeline *p, uint32_t n_dma);

static inline void
__instr_dma_ht_exec(struct rte_swx_pipeline *p, uint32_t n_dma)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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
instr_dma_ht_exec(struct rte_swx_pipeline *p)
{
	__instr_dma_ht_exec(p, 1);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht2_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 2 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 2);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht3_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 3 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 3);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht4_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 4 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 4);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht5_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 5 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 5);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht6_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 6 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 6);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht7_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 7 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 7);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht8_exec(struct rte_swx_pipeline *p)
{
	TRACE("[Thread %2u] *** The next 8 instructions are fused. ***\n",
	      p->thread_id);

	__instr_dma_ht_exec(p, 8);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * alu.
 */
static int
instr_alu_add_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* ADD, ADD_HM, ADD_MH, ADD_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_ADD;
		if (dst[0] == 'h' && src[0] == 'm')
			instr->type = INSTR_ALU_ADD_HM;
		if (dst[0] == 'm' && src[0] == 'h')
			instr->type = INSTR_ALU_ADD_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_ADD_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* ADD_MI, ADD_HI. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_ADD_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_ADD_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static int
instr_alu_sub_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* SUB, SUB_HM, SUB_MH, SUB_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_SUB;
		if (dst[0] == 'h' && src[0] == 'm')
			instr->type = INSTR_ALU_SUB_HM;
		if (dst[0] == 'm' && src[0] == 'h')
			instr->type = INSTR_ALU_SUB_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SUB_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* SUB_MI, SUB_HI. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_SUB_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_SUB_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static int
instr_alu_ckadd_translate(struct rte_swx_pipeline *p,
			  struct action *action __rte_unused,
			  char **tokens,
			  int n_tokens,
			  struct instruction *instr,
			  struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct header *hdst, *hsrc;
	struct field *fdst, *fsrc;

	CHECK(n_tokens == 3, EINVAL);

	fdst = header_field_parse(p, dst, &hdst);
	CHECK(fdst && (fdst->n_bits == 16), EINVAL);

	/* CKADD_FIELD. */
	fsrc = header_field_parse(p, src, &hsrc);
	if (fsrc) {
		instr->type = INSTR_ALU_CKADD_FIELD;
		instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* CKADD_STRUCT, CKADD_STRUCT20. */
	hsrc = header_parse(p, src);
	CHECK(hsrc, EINVAL);

	instr->type = INSTR_ALU_CKADD_STRUCT;
	if ((hsrc->st->n_bits / 8) == 20)
		instr->type = INSTR_ALU_CKADD_STRUCT20;

	instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
	instr->alu.src.n_bits = hsrc->st->n_bits;
	instr->alu.src.offset = 0; /* Unused. */
	return 0;
}

static int
instr_alu_cksub_translate(struct rte_swx_pipeline *p,
			  struct action *action __rte_unused,
			  char **tokens,
			  int n_tokens,
			  struct instruction *instr,
			  struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct header *hdst, *hsrc;
	struct field *fdst, *fsrc;

	CHECK(n_tokens == 3, EINVAL);

	fdst = header_field_parse(p, dst, &hdst);
	CHECK(fdst && (fdst->n_bits == 16), EINVAL);

	fsrc = header_field_parse(p, src, &hsrc);
	CHECK(fsrc, EINVAL);

	instr->type = INSTR_ALU_CKSUB_FIELD;
	instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
	instr->alu.src.n_bits = fsrc->n_bits;
	instr->alu.src.offset = fsrc->offset / 8;
	return 0;
}

static int
instr_alu_shl_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* SHL, SHL_HM, SHL_MH, SHL_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_SHL;
		if (dst[0] == 'h' && src[0] == 'm')
			instr->type = INSTR_ALU_SHL_HM;
		if (dst[0] == 'm' && src[0] == 'h')
			instr->type = INSTR_ALU_SHL_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SHL_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* SHL_MI, SHL_HI. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_SHL_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_SHL_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static int
instr_alu_and_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* AND or AND_S. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_AND;
		if ((dst[0] == 'h' && src[0] != 'h') ||
		    (dst[0] != 'h' && src[0] == 'h'))
			instr->type = INSTR_ALU_AND_S;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* AND_I. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = htonl(src_val);

	instr->type = INSTR_ALU_AND_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static int
instr_alu_or_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* OR or OR_S. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_OR;
		if ((dst[0] == 'h' && src[0] != 'h') ||
		    (dst[0] != 'h' && src[0] == 'h'))
			instr->type = INSTR_ALU_OR_S;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* OR_I. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = htonl(src_val);

	instr->type = INSTR_ALU_OR_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static int
instr_alu_xor_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id, src_struct_id, src_val;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);

	/* XOR or XOR_S. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		instr->type = INSTR_ALU_XOR;
		if ((dst[0] == 'h' && src[0] != 'h') ||
		    (dst[0] != 'h' && src[0] == 'h'))
			instr->type = INSTR_ALU_XOR_S;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* XOR_I. */
	src_val = strtoul(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = htonl(src_val);

	instr->type = INSTR_ALU_XOR_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = (uint32_t)src_val;
	return 0;
}

static inline void
instr_alu_add_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_s_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (s)\n", p->thread_id);

	/* Structs. */
	ALU_S(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_s_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (s)\n", p->thread_id);

	/* Structs. */
	ALU_S(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_s_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (s)\n", p->thread_id);

	/* Structs. */
	ALU_S(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_cksub_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct20_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
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

	/* Thread. */
	thread_ip_inc(p);
}

#define RTE_SWX_INSTRUCTION_TOKENS_MAX 16

static int
instr_translate(struct rte_swx_pipeline *p,
		struct action *action,
		char *string,
		struct instruction *instr,
		struct instruction_data *data)
{
	char *tokens[RTE_SWX_INSTRUCTION_TOKENS_MAX];
	int n_tokens = 0, tpos = 0;

	/* Parse the instruction string into tokens. */
	for ( ; ; ) {
		char *token;

		token = strtok_r(string, " \t\v", &string);
		if (!token)
			break;

		CHECK(n_tokens < RTE_SWX_INSTRUCTION_TOKENS_MAX, EINVAL);

		tokens[n_tokens] = token;
		n_tokens++;
	}

	CHECK(n_tokens, EINVAL);

	/* Handle the optional instruction label. */
	if ((n_tokens >= 2) && !strcmp(tokens[1], ":")) {
		strcpy(data->label, tokens[0]);

		tpos += 2;
		CHECK(n_tokens - tpos, EINVAL);
	}

	/* Identify the instruction type. */
	if (!strcmp(tokens[tpos], "rx"))
		return instr_rx_translate(p,
					  action,
					  &tokens[tpos],
					  n_tokens - tpos,
					  instr,
					  data);

	if (!strcmp(tokens[tpos], "tx"))
		return instr_tx_translate(p,
					  action,
					  &tokens[tpos],
					  n_tokens - tpos,
					  instr,
					  data);

	if (!strcmp(tokens[tpos], "extract"))
		return instr_hdr_extract_translate(p,
						   action,
						   &tokens[tpos],
						   n_tokens - tpos,
						   instr,
						   data);

	if (!strcmp(tokens[tpos], "emit"))
		return instr_hdr_emit_translate(p,
						action,
						&tokens[tpos],
						n_tokens - tpos,
						instr,
						data);

	if (!strcmp(tokens[tpos], "validate"))
		return instr_hdr_validate_translate(p,
						    action,
						    &tokens[tpos],
						    n_tokens - tpos,
						    instr,
						    data);

	if (!strcmp(tokens[tpos], "invalidate"))
		return instr_hdr_invalidate_translate(p,
						      action,
						      &tokens[tpos],
						      n_tokens - tpos,
						      instr,
						      data);

	if (!strcmp(tokens[tpos], "mov"))
		return instr_mov_translate(p,
					   action,
					   &tokens[tpos],
					   n_tokens - tpos,
					   instr,
					   data);

	if (!strcmp(tokens[tpos], "dma"))
		return instr_dma_translate(p,
					   action,
					   &tokens[tpos],
					   n_tokens - tpos,
					   instr,
					   data);

	if (!strcmp(tokens[tpos], "add"))
		return instr_alu_add_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "sub"))
		return instr_alu_sub_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "ckadd"))
		return instr_alu_ckadd_translate(p,
						 action,
						 &tokens[tpos],
						 n_tokens - tpos,
						 instr,
						 data);

	if (!strcmp(tokens[tpos], "cksub"))
		return instr_alu_cksub_translate(p,
						 action,
						 &tokens[tpos],
						 n_tokens - tpos,
						 instr,
						 data);

	if (!strcmp(tokens[tpos], "and"))
		return instr_alu_and_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "or"))
		return instr_alu_or_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "xor"))
		return instr_alu_xor_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "shl"))
		return instr_alu_shl_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	CHECK(0, EINVAL);
}

static uint32_t
label_is_used(struct instruction_data *data, uint32_t n, const char *label)
{
	uint32_t count = 0, i;

	if (!label[0])
		return 0;

	for (i = 0; i < n; i++)
		if (!strcmp(label, data[i].jmp_label))
			count++;

	return count;
}

static int
instr_label_check(struct instruction_data *instruction_data,
		  uint32_t n_instructions)
{
	uint32_t i;

	/* Check that all instruction labels are unique. */
	for (i = 0; i < n_instructions; i++) {
		struct instruction_data *data = &instruction_data[i];
		char *label = data->label;
		uint32_t j;

		if (!label[0])
			continue;

		for (j = i + 1; j < n_instructions; j++)
			CHECK(strcmp(label, data[j].label), EINVAL);
	}

	/* Get users for each instruction label. */
	for (i = 0; i < n_instructions; i++) {
		struct instruction_data *data = &instruction_data[i];
		char *label = data->label;

		data->n_users = label_is_used(instruction_data,
					      n_instructions,
					      label);
	}

	return 0;
}

static int
instruction_config(struct rte_swx_pipeline *p,
		   struct action *a,
		   const char **instructions,
		   uint32_t n_instructions)
{
	struct instruction *instr = NULL;
	struct instruction_data *data = NULL;
	char *string = NULL;
	int err = 0;
	uint32_t i;

	CHECK(n_instructions, EINVAL);
	CHECK(instructions, EINVAL);
	for (i = 0; i < n_instructions; i++)
		CHECK(instructions[i], EINVAL);

	/* Memory allocation. */
	instr = calloc(n_instructions, sizeof(struct instruction));
	if (!instr) {
		err = ENOMEM;
		goto error;
	}

	data = calloc(n_instructions, sizeof(struct instruction_data));
	if (!data) {
		err = ENOMEM;
		goto error;
	}

	for (i = 0; i < n_instructions; i++) {
		string = strdup(instructions[i]);
		if (!string) {
			err = ENOMEM;
			goto error;
		}

		err = instr_translate(p, a, string, &instr[i], &data[i]);
		if (err)
			goto error;

		free(string);
	}

	err = instr_label_check(data, n_instructions);
	if (err)
		goto error;

	free(data);

	if (a) {
		a->instructions = instr;
		a->n_instructions = n_instructions;
	} else {
		p->instructions = instr;
		p->n_instructions = n_instructions;
	}

	return 0;

error:
	free(string);
	free(data);
	free(instr);
	return err;
}

typedef void (*instr_exec_t)(struct rte_swx_pipeline *);

static instr_exec_t instruction_table[] = {
	[INSTR_RX] = instr_rx_exec,
	[INSTR_TX] = instr_tx_exec,

	[INSTR_HDR_EXTRACT] = instr_hdr_extract_exec,
	[INSTR_HDR_EXTRACT2] = instr_hdr_extract2_exec,
	[INSTR_HDR_EXTRACT3] = instr_hdr_extract3_exec,
	[INSTR_HDR_EXTRACT4] = instr_hdr_extract4_exec,
	[INSTR_HDR_EXTRACT5] = instr_hdr_extract5_exec,
	[INSTR_HDR_EXTRACT6] = instr_hdr_extract6_exec,
	[INSTR_HDR_EXTRACT7] = instr_hdr_extract7_exec,
	[INSTR_HDR_EXTRACT8] = instr_hdr_extract8_exec,

	[INSTR_HDR_EMIT] = instr_hdr_emit_exec,
	[INSTR_HDR_EMIT_TX] = instr_hdr_emit_tx_exec,
	[INSTR_HDR_EMIT2_TX] = instr_hdr_emit2_tx_exec,
	[INSTR_HDR_EMIT3_TX] = instr_hdr_emit3_tx_exec,
	[INSTR_HDR_EMIT4_TX] = instr_hdr_emit4_tx_exec,
	[INSTR_HDR_EMIT5_TX] = instr_hdr_emit5_tx_exec,
	[INSTR_HDR_EMIT6_TX] = instr_hdr_emit6_tx_exec,
	[INSTR_HDR_EMIT7_TX] = instr_hdr_emit7_tx_exec,
	[INSTR_HDR_EMIT8_TX] = instr_hdr_emit8_tx_exec,

	[INSTR_HDR_VALIDATE] = instr_hdr_validate_exec,
	[INSTR_HDR_INVALIDATE] = instr_hdr_invalidate_exec,

	[INSTR_MOV] = instr_mov_exec,
	[INSTR_MOV_S] = instr_mov_s_exec,
	[INSTR_MOV_I] = instr_mov_i_exec,

	[INSTR_DMA_HT] = instr_dma_ht_exec,
	[INSTR_DMA_HT2] = instr_dma_ht2_exec,
	[INSTR_DMA_HT3] = instr_dma_ht3_exec,
	[INSTR_DMA_HT4] = instr_dma_ht4_exec,
	[INSTR_DMA_HT5] = instr_dma_ht5_exec,
	[INSTR_DMA_HT6] = instr_dma_ht6_exec,
	[INSTR_DMA_HT7] = instr_dma_ht7_exec,
	[INSTR_DMA_HT8] = instr_dma_ht8_exec,

	[INSTR_ALU_ADD] = instr_alu_add_exec,
	[INSTR_ALU_ADD_MH] = instr_alu_add_mh_exec,
	[INSTR_ALU_ADD_HM] = instr_alu_add_hm_exec,
	[INSTR_ALU_ADD_HH] = instr_alu_add_hh_exec,
	[INSTR_ALU_ADD_MI] = instr_alu_add_mi_exec,
	[INSTR_ALU_ADD_HI] = instr_alu_add_hi_exec,

	[INSTR_ALU_SUB] = instr_alu_sub_exec,
	[INSTR_ALU_SUB_MH] = instr_alu_sub_mh_exec,
	[INSTR_ALU_SUB_HM] = instr_alu_sub_hm_exec,
	[INSTR_ALU_SUB_HH] = instr_alu_sub_hh_exec,
	[INSTR_ALU_SUB_MI] = instr_alu_sub_mi_exec,
	[INSTR_ALU_SUB_HI] = instr_alu_sub_hi_exec,

	[INSTR_ALU_CKADD_FIELD] = instr_alu_ckadd_field_exec,
	[INSTR_ALU_CKADD_STRUCT] = instr_alu_ckadd_struct_exec,
	[INSTR_ALU_CKADD_STRUCT20] = instr_alu_ckadd_struct20_exec,
	[INSTR_ALU_CKSUB_FIELD] = instr_alu_cksub_field_exec,

	[INSTR_ALU_AND] = instr_alu_and_exec,
	[INSTR_ALU_AND_S] = instr_alu_and_s_exec,
	[INSTR_ALU_AND_I] = instr_alu_and_i_exec,

	[INSTR_ALU_OR] = instr_alu_or_exec,
	[INSTR_ALU_OR_S] = instr_alu_or_s_exec,
	[INSTR_ALU_OR_I] = instr_alu_or_i_exec,

	[INSTR_ALU_XOR] = instr_alu_xor_exec,
	[INSTR_ALU_XOR_S] = instr_alu_xor_s_exec,
	[INSTR_ALU_XOR_I] = instr_alu_xor_i_exec,

	[INSTR_ALU_SHL] = instr_alu_shl_exec,
	[INSTR_ALU_SHL_MH] = instr_alu_shl_mh_exec,
	[INSTR_ALU_SHL_HM] = instr_alu_shl_hm_exec,
	[INSTR_ALU_SHL_HH] = instr_alu_shl_hh_exec,
	[INSTR_ALU_SHL_MI] = instr_alu_shl_mi_exec,
	[INSTR_ALU_SHL_HI] = instr_alu_shl_hi_exec,
};

static inline void
instr_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	instr_exec_t instr = instruction_table[ip->type];

	instr(p);
}

/*
 * Action.
 */
static struct action *
action_find(struct rte_swx_pipeline *p, const char *name)
{
	struct action *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->actions, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct field *
action_field_find(struct action *a, const char *name)
{
	return a->st ? struct_type_field_find(a->st, name) : NULL;
}

static struct field *
action_field_parse(struct action *action, const char *name)
{
	if (name[0] != 't' || name[1] != '.')
		return NULL;

	return action_field_find(action, &name[2]);
}

int
rte_swx_pipeline_action_config(struct rte_swx_pipeline *p,
			       const char *name,
			       const char *args_struct_type_name,
			       const char **instructions,
			       uint32_t n_instructions)
{
	struct struct_type *args_struct_type;
	struct action *a;
	int err;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!action_find(p, name), EEXIST);

	if (args_struct_type_name) {
		CHECK_NAME(args_struct_type_name, EINVAL);
		args_struct_type = struct_type_find(p, args_struct_type_name);
		CHECK(args_struct_type, EINVAL);
	} else {
		args_struct_type = NULL;
	}

	/* Node allocation. */
	a = calloc(1, sizeof(struct action));
	CHECK(a, ENOMEM);

	/* Node initialization. */
	strcpy(a->name, name);
	a->st = args_struct_type;
	a->id = p->n_actions;

	/* Instruction translation. */
	err = instruction_config(p, a, instructions, n_instructions);
	if (err) {
		free(a);
		return err;
	}

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->actions, a, node);
	p->n_actions++;

	return 0;
}

static int
action_build(struct rte_swx_pipeline *p)
{
	struct action *action;

	p->action_instructions = calloc(p->n_actions,
					sizeof(struct instruction *));
	CHECK(p->action_instructions, ENOMEM);

	TAILQ_FOREACH(action, &p->actions, node)
		p->action_instructions[action->id] = action->instructions;

	return 0;
}

static void
action_build_free(struct rte_swx_pipeline *p)
{
	free(p->action_instructions);
	p->action_instructions = NULL;
}

static void
action_free(struct rte_swx_pipeline *p)
{
	action_build_free(p);

	for ( ; ; ) {
		struct action *action;

		action = TAILQ_FIRST(&p->actions);
		if (!action)
			break;

		TAILQ_REMOVE(&p->actions, action, node);
		free(action->instructions);
		free(action);
	}
}

/*
 * Table.
 */
static struct table_type *
table_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct table_type *elem;

	TAILQ_FOREACH(elem, &p->table_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct table_type *
table_type_resolve(struct rte_swx_pipeline *p,
		   const char *recommended_type_name,
		   enum rte_swx_table_match_type match_type)
{
	struct table_type *elem;

	/* Only consider the recommended type if the match type is correct. */
	if (recommended_type_name)
		TAILQ_FOREACH(elem, &p->table_types, node)
			if (!strcmp(elem->name, recommended_type_name) &&
			    (elem->match_type == match_type))
				return elem;

	/* Ignore the recommended type and get the first element with this match
	 * type.
	 */
	TAILQ_FOREACH(elem, &p->table_types, node)
		if (elem->match_type == match_type)
			return elem;

	return NULL;
}

static struct table *
table_find(struct rte_swx_pipeline *p, const char *name)
{
	struct table *elem;

	TAILQ_FOREACH(elem, &p->tables, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct table *
table_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct table *table = NULL;

	TAILQ_FOREACH(table, &p->tables, node)
		if (table->id == id)
			return table;

	return NULL;
}

int
rte_swx_pipeline_table_type_register(struct rte_swx_pipeline *p,
				     const char *name,
				     enum rte_swx_table_match_type match_type,
				     struct rte_swx_table_ops *ops)
{
	struct table_type *elem;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_type_find(p, name), EEXIST);

	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->lkp, EINVAL);
	CHECK(ops->free, EINVAL);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct table_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	elem->match_type = match_type;
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->table_types, elem, node);

	return 0;
}

static enum rte_swx_table_match_type
table_match_type_resolve(struct rte_swx_match_field_params *fields,
			 uint32_t n_fields)
{
	uint32_t i;

	for (i = 0; i < n_fields; i++)
		if (fields[i].match_type != RTE_SWX_TABLE_MATCH_EXACT)
			break;

	if (i == n_fields)
		return RTE_SWX_TABLE_MATCH_EXACT;

	if ((i == n_fields - 1) &&
	    (fields[i].match_type == RTE_SWX_TABLE_MATCH_LPM))
		return RTE_SWX_TABLE_MATCH_LPM;

	return RTE_SWX_TABLE_MATCH_WILDCARD;
}

int
rte_swx_pipeline_table_config(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_swx_pipeline_table_params *params,
			      const char *recommended_table_type_name,
			      const char *args,
			      uint32_t size)
{
	struct table_type *type;
	struct table *t;
	struct action *default_action;
	struct header *header = NULL;
	int is_header = 0;
	uint32_t offset_prev = 0, action_data_size_max = 0, i;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_find(p, name), EEXIST);

	CHECK(params, EINVAL);

	/* Match checks. */
	CHECK(!params->n_fields || params->fields, EINVAL);
	for (i = 0; i < params->n_fields; i++) {
		struct rte_swx_match_field_params *field = &params->fields[i];
		struct header *h;
		struct field *hf, *mf;
		uint32_t offset;

		CHECK_NAME(field->name, EINVAL);

		hf = header_field_parse(p, field->name, &h);
		mf = metadata_field_parse(p, field->name);
		CHECK(hf || mf, EINVAL);

		offset = hf ? hf->offset : mf->offset;

		if (i == 0) {
			is_header = hf ? 1 : 0;
			header = hf ? h : NULL;
			offset_prev = offset;

			continue;
		}

		CHECK((is_header && hf && (h->id == header->id)) ||
		      (!is_header && mf), EINVAL);

		CHECK(offset > offset_prev, EINVAL);
		offset_prev = offset;
	}

	/* Action checks. */
	CHECK(params->n_actions, EINVAL);
	CHECK(params->action_names, EINVAL);
	for (i = 0; i < params->n_actions; i++) {
		const char *action_name = params->action_names[i];
		struct action *a;
		uint32_t action_data_size;

		CHECK(action_name, EINVAL);

		a = action_find(p, action_name);
		CHECK(a, EINVAL);

		action_data_size = a->st ? a->st->n_bits / 8 : 0;
		if (action_data_size > action_data_size_max)
			action_data_size_max = action_data_size;
	}

	CHECK(params->default_action_name, EINVAL);
	for (i = 0; i < p->n_actions; i++)
		if (!strcmp(params->action_names[i],
			    params->default_action_name))
			break;
	CHECK(i < params->n_actions, EINVAL);
	default_action = action_find(p, params->default_action_name);
	CHECK((default_action->st && params->default_action_data) ||
	      !params->default_action_data, EINVAL);

	/* Table type checks. */
	if (params->n_fields) {
		enum rte_swx_table_match_type match_type;

		match_type = table_match_type_resolve(params->fields,
						      params->n_fields);
		type = table_type_resolve(p,
					  recommended_table_type_name,
					  match_type);
		CHECK(type, EINVAL);
	} else {
		type = NULL;
	}

	/* Memory allocation. */
	t = calloc(1, sizeof(struct table));
	CHECK(t, ENOMEM);

	t->fields = calloc(params->n_fields, sizeof(struct match_field));
	if (!t->fields) {
		free(t);
		CHECK(0, ENOMEM);
	}

	t->actions = calloc(params->n_actions, sizeof(struct action *));
	if (!t->actions) {
		free(t->fields);
		free(t);
		CHECK(0, ENOMEM);
	}

	if (action_data_size_max) {
		t->default_action_data = calloc(1, action_data_size_max);
		if (!t->default_action_data) {
			free(t->actions);
			free(t->fields);
			free(t);
			CHECK(0, ENOMEM);
		}
	}

	/* Node initialization. */
	strcpy(t->name, name);
	if (args && args[0])
		strcpy(t->args, args);
	t->type = type;

	for (i = 0; i < params->n_fields; i++) {
		struct rte_swx_match_field_params *field = &params->fields[i];
		struct match_field *f = &t->fields[i];

		f->match_type = field->match_type;
		f->field = is_header ?
			header_field_parse(p, field->name, NULL) :
			metadata_field_parse(p, field->name);
	}
	t->n_fields = params->n_fields;
	t->is_header = is_header;
	t->header = header;

	for (i = 0; i < params->n_actions; i++)
		t->actions[i] = action_find(p, params->action_names[i]);
	t->default_action = default_action;
	if (default_action->st)
		memcpy(t->default_action_data,
		       params->default_action_data,
		       default_action->st->n_bits / 8);
	t->n_actions = params->n_actions;
	t->default_action_is_const = params->default_action_is_const;
	t->action_data_size_max = action_data_size_max;

	t->size = size;
	t->id = p->n_tables;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->tables, t, node);
	p->n_tables++;

	return 0;
}

static struct rte_swx_table_params *
table_params_get(struct table *table)
{
	struct rte_swx_table_params *params;
	struct field *first, *last;
	uint8_t *key_mask;
	uint32_t key_size, key_offset, action_data_size, i;

	/* Memory allocation. */
	params = calloc(1, sizeof(struct rte_swx_table_params));
	if (!params)
		return NULL;

	/* Key offset and size. */
	first = table->fields[0].field;
	last = table->fields[table->n_fields - 1].field;
	key_offset = first->offset / 8;
	key_size = (last->offset + last->n_bits - first->offset) / 8;

	/* Memory allocation. */
	key_mask = calloc(1, key_size);
	if (!key_mask) {
		free(params);
		return NULL;
	}

	/* Key mask. */
	for (i = 0; i < table->n_fields; i++) {
		struct field *f = table->fields[i].field;
		uint32_t start = (f->offset - first->offset) / 8;
		size_t size = f->n_bits / 8;

		memset(&key_mask[start], 0xFF, size);
	}

	/* Action data size. */
	action_data_size = 0;
	for (i = 0; i < table->n_actions; i++) {
		struct action *action = table->actions[i];
		uint32_t ads = action->st ? action->st->n_bits / 8 : 0;

		if (ads > action_data_size)
			action_data_size = ads;
	}

	/* Fill in. */
	params->match_type = table->type->match_type;
	params->key_size = key_size;
	params->key_offset = key_offset;
	params->key_mask0 = key_mask;
	params->action_data_size = action_data_size;
	params->n_keys_max = table->size;

	return params;
}

static void
table_params_free(struct rte_swx_table_params *params)
{
	if (!params)
		return;

	free(params->key_mask0);
	free(params);
}

static int
table_state_build(struct rte_swx_pipeline *p)
{
	struct table *table;

	p->table_state = calloc(p->n_tables,
				sizeof(struct rte_swx_table_state));
	CHECK(p->table_state, ENOMEM);

	TAILQ_FOREACH(table, &p->tables, node) {
		struct rte_swx_table_state *ts = &p->table_state[table->id];

		if (table->type) {
			struct rte_swx_table_params *params;

			/* ts->obj. */
			params = table_params_get(table);
			CHECK(params, ENOMEM);

			ts->obj = table->type->ops.create(params,
				NULL,
				table->args,
				p->numa_node);

			table_params_free(params);
			CHECK(ts->obj, ENODEV);
		}

		/* ts->default_action_data. */
		if (table->action_data_size_max) {
			ts->default_action_data =
				malloc(table->action_data_size_max);
			CHECK(ts->default_action_data, ENOMEM);

			memcpy(ts->default_action_data,
			       table->default_action_data,
			       table->action_data_size_max);
		}

		/* ts->default_action_id. */
		ts->default_action_id = table->default_action->id;
	}

	return 0;
}

static void
table_state_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	if (!p->table_state)
		return;

	for (i = 0; i < p->n_tables; i++) {
		struct rte_swx_table_state *ts = &p->table_state[i];
		struct table *table = table_find_by_id(p, i);

		/* ts->obj. */
		if (table->type && ts->obj)
			table->type->ops.free(ts->obj);

		/* ts->default_action_data. */
		free(ts->default_action_data);
	}

	free(p->table_state);
	p->table_state = NULL;
}

static void
table_state_free(struct rte_swx_pipeline *p)
{
	table_state_build_free(p);
}

static int
table_stub_lkp(void *table __rte_unused,
	       void *mailbox __rte_unused,
	       uint8_t **key __rte_unused,
	       uint64_t *action_id __rte_unused,
	       uint8_t **action_data __rte_unused,
	       int *hit)
{
	*hit = 0;
	return 1; /* DONE. */
}

static int
table_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct table *table;

		t->tables = calloc(p->n_tables, sizeof(struct table_runtime));
		CHECK(t->tables, ENOMEM);

		TAILQ_FOREACH(table, &p->tables, node) {
			struct table_runtime *r = &t->tables[table->id];

			if (table->type) {
				uint64_t size;

				size = table->type->ops.mailbox_size_get();

				/* r->func. */
				r->func = table->type->ops.lkp;

				/* r->mailbox. */
				if (size) {
					r->mailbox = calloc(1, size);
					CHECK(r->mailbox, ENOMEM);
				}

				/* r->key. */
				r->key = table->is_header ?
					&t->structs[table->header->struct_id] :
					&t->structs[p->metadata_struct_id];
			} else {
				r->func = table_stub_lkp;
			}
		}
	}

	return 0;
}

static void
table_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->tables)
			continue;

		for (j = 0; j < p->n_tables; j++) {
			struct table_runtime *r = &t->tables[j];

			free(r->mailbox);
		}

		free(t->tables);
		t->tables = NULL;
	}
}

static void
table_free(struct rte_swx_pipeline *p)
{
	table_build_free(p);

	/* Tables. */
	for ( ; ; ) {
		struct table *elem;

		elem = TAILQ_FIRST(&p->tables);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->tables, elem, node);
		free(elem->fields);
		free(elem->actions);
		free(elem->default_action_data);
		free(elem);
	}

	/* Table types. */
	for ( ; ; ) {
		struct table_type *elem;

		elem = TAILQ_FIRST(&p->table_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->table_types, elem, node);
		free(elem);
	}
}

/*
 * Pipeline.
 */
int
rte_swx_pipeline_config(struct rte_swx_pipeline **p, int numa_node)
{
	struct rte_swx_pipeline *pipeline;

	/* Check input parameters. */
	CHECK(p, EINVAL);

	/* Memory allocation. */
	pipeline = calloc(1, sizeof(struct rte_swx_pipeline));
	CHECK(pipeline, ENOMEM);

	/* Initialization. */
	TAILQ_INIT(&pipeline->struct_types);
	TAILQ_INIT(&pipeline->port_in_types);
	TAILQ_INIT(&pipeline->ports_in);
	TAILQ_INIT(&pipeline->port_out_types);
	TAILQ_INIT(&pipeline->ports_out);
	TAILQ_INIT(&pipeline->extern_types);
	TAILQ_INIT(&pipeline->extern_objs);
	TAILQ_INIT(&pipeline->extern_funcs);
	TAILQ_INIT(&pipeline->headers);
	TAILQ_INIT(&pipeline->actions);
	TAILQ_INIT(&pipeline->table_types);
	TAILQ_INIT(&pipeline->tables);

	pipeline->n_structs = 1; /* Struct 0 is reserved for action_data. */
	pipeline->numa_node = numa_node;

	*p = pipeline;
	return 0;
}

void
rte_swx_pipeline_free(struct rte_swx_pipeline *p)
{
	if (!p)
		return;

	free(p->instructions);

	table_state_free(p);
	table_free(p);
	action_free(p);
	metadata_free(p);
	header_free(p);
	extern_func_free(p);
	extern_obj_free(p);
	port_out_free(p);
	port_in_free(p);
	struct_free(p);

	free(p);
}

int
rte_swx_pipeline_instructions_config(struct rte_swx_pipeline *p,
				     const char **instructions,
				     uint32_t n_instructions)
{
	int err;
	uint32_t i;

	err = instruction_config(p, NULL, instructions, n_instructions);
	if (err)
		return err;

	/* Thread instruction pointer reset. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		thread_ip_reset(p, t);
	}

	return 0;
}

int
rte_swx_pipeline_build(struct rte_swx_pipeline *p)
{
	int status;

	CHECK(p, EINVAL);
	CHECK(p->build_done == 0, EEXIST);

	status = port_in_build(p);
	if (status)
		goto error;

	status = port_out_build(p);
	if (status)
		goto error;

	status = struct_build(p);
	if (status)
		goto error;

	status = extern_obj_build(p);
	if (status)
		goto error;

	status = extern_func_build(p);
	if (status)
		goto error;

	status = header_build(p);
	if (status)
		goto error;

	status = metadata_build(p);
	if (status)
		goto error;

	status = action_build(p);
	if (status)
		goto error;

	status = table_build(p);
	if (status)
		goto error;

	status = table_state_build(p);
	if (status)
		goto error;

	p->build_done = 1;
	return 0;

error:
	table_state_build_free(p);
	table_build_free(p);
	action_build_free(p);
	metadata_build_free(p);
	header_build_free(p);
	extern_func_build_free(p);
	extern_obj_build_free(p);
	port_out_build_free(p);
	port_in_build_free(p);
	struct_build_free(p);

	return status;
}

void
rte_swx_pipeline_run(struct rte_swx_pipeline *p, uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; i++)
		instr_exec(p);
}

/*
 * Control.
 */
int
rte_swx_pipeline_table_state_get(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state **table_state)
{
	if (!p || !table_state || !p->build_done)
		return -EINVAL;

	*table_state = p->table_state;
	return 0;
}

int
rte_swx_pipeline_table_state_set(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state *table_state)
{
	if (!p || !table_state || !p->build_done)
		return -EINVAL;

	p->table_state = table_state;
	return 0;
}
