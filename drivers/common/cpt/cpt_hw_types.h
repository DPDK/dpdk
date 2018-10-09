/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_HW_TYPES_H_
#define _CPT_HW_TYPES_H_

#include <rte_byteorder.h>

/*
 * This file defines HRM specific structs.
 *
 */

#define CPT_VF_INTR_MBOX_MASK   (1<<0)
#define CPT_VF_INTR_DOVF_MASK   (1<<1)
#define CPT_VF_INTR_IRDE_MASK   (1<<2)
#define CPT_VF_INTR_NWRP_MASK   (1<<3)
#define CPT_VF_INTR_SWERR_MASK  (1<<4)
#define CPT_VF_INTR_HWERR_MASK  (1<<5)
#define CPT_VF_INTR_FAULT_MASK  (1<<6)

#define CPT_INST_SIZE           (64)
#define CPT_NEXT_CHUNK_PTR_SIZE (8)

/*
 * CPT_INST_S software command definitions
 * Words EI (0-3)
 */
typedef union {
	uint64_t u64;
	struct {
		uint16_t opcode;
		uint16_t param1;
		uint16_t param2;
		uint16_t dlen;
	} s;
} vq_cmd_word0_t;

typedef union {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t grp	: 3;
		uint64_t cptr	: 61;
#else
		uint64_t cptr	: 61;
		uint64_t grp	: 3;
#endif
	} s;
} vq_cmd_word3_t;

typedef struct cpt_vq_command {
	vq_cmd_word0_t cmd;
	uint64_t dptr;
	uint64_t rptr;
	vq_cmd_word3_t cptr;
} cpt_vq_cmd_t;

/**
 * Structure cpt_inst_s
 *
 * CPT Instruction Structure
 * This structure specifies the instruction layout.
 * Instructions are stored in memory as little-endian unless
 * CPT()_PF_Q()_CTL[INST_BE] is set.
 */
typedef union cpt_inst_s {
	uint64_t u[8];
	struct cpt_inst_s_8s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_17_63        : 47;
		/* [ 16: 16] Done interrupt.
		 * 0 = No interrupts related to this instruction.
		 * 1 = When the instruction completes,CPT()_VQ()_DONE[DONE]
		 * will be incremented, and based on the rules described
		 * there an interrupt may occur.
		 */
		uint64_t doneint               : 1;
		uint64_t reserved_0_15         : 16;
#else /* Word 0 - Little Endian */
		uint64_t reserved_0_15         : 16;
		uint64_t doneint               : 1;
		uint64_t reserved_17_63        : 47;
#endif /* Word 0 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 1 - Big Endian */
		/* [127: 64] Result IOVA.
		 * If nonzero, specifies where to write CPT_RES_S.
		 * If zero, no result structure will be written.
		 * Address must be 16-byte aligned.
		 *
		 * Bits <63:49> are ignored by hardware; software should
		 * use a sign-extended bit <48> for forward compatibility.
		 */
		uint64_t res_addr              : 64;
#else /* Word 1 - Little Endian */
		uint64_t res_addr              : 64;
#endif /* Word 1 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 2 - Big Endian */
		uint64_t reserved_172_191      : 20;
		/* [171:162] If [WQ_PTR] is nonzero, the SSO guest-group to
		 * use when CPT submits work to SSO.
		 * For the SSO to not discard the add-work request, FPA_PF_MAP()
		 * must map [GRP] and CPT()_PF_Q()_GMCTL[GMID] as valid.
		 */
		uint64_t grp                   : 10;
		/* [161:160] If [WQ_PTR] is nonzero, the SSO tag type to use
		 * when CPT submits work to SSO.
		 */
		uint64_t tt                    : 2;
		/* [159:128] If [WQ_PTR] is nonzero, the SSO tag to use when
		 * CPT submits work to SSO.
		 */
		uint64_t tag                   : 32;
#else /* Word 2 - Little Endian */
		uint64_t tag                   : 32;
		uint64_t tt                    : 2;
		uint64_t grp                   : 10;
		uint64_t reserved_172_191      : 20;
#endif /* Word 2 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 3 - Big Endian */
		/** [255:192] If [WQ_PTR] is nonzero, it is a pointer to a
		 * work-queue entry that CPT submits work to SSO after all
		 * context, output data, and result write operations are
		 * visible to other CNXXXX units and the cores.
		 * Bits <2:0> must be zero.
		 * Bits <63:49> are ignored by hardware; software should use a
		 * sign-extended bit <48> for forward compatibility.
		 * Internal:Bits <63:49>, <2:0> are ignored by hardware,
		 * treated as always 0x0.
		 **/
		uint64_t wq_ptr                : 64;
#else /* Word 3 - Little Endian */
		uint64_t wq_ptr                : 64;
#endif /* Word 3 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 4 - Big Endian */
		union {
			/** [319:256] Engine instruction word 0. Passed to the
			 * AE/SE.
			 **/
			uint64_t ei0                   : 64;
			vq_cmd_word0_t vq_cmd_w0;
		};
#else /* Word 4 - Little Endian */
		union {
			uint64_t ei0                   : 64;
			vq_cmd_word0_t vq_cmd_w0;
		};
#endif /* Word 4 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 5 - Big Endian */
		union {
			/** [383:320] Engine instruction word 1. Passed to the
			 * AE/SE.
			 **/
			uint64_t ei1                   : 64;
			uint64_t dptr;
		};
#else /* Word 5 - Little Endian */
		union {
			uint64_t ei1                   : 64;
			uint64_t dptr;
		};
#endif /* Word 5 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 6 - Big Endian */
		union {
			/** [447:384] Engine instruction word 2. Passed to the
			 * AE/SE.
			 **/
			uint64_t ei2                   : 64;
			uint64_t rptr;
		};
#else /* Word 6 - Little Endian */
		union {
			uint64_t ei2                   : 64;
			uint64_t rptr;
		};
#endif /* Word 6 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 7 - Big Endian */
		union {
			/** [511:448] Engine instruction word 3. Passed to the
			 * AE/SE.
			 **/
			uint64_t ei3                   : 64;
			vq_cmd_word3_t vq_cmd_w3;
		};
#else /* Word 7 - Little Endian */
		union {
			uint64_t ei3                   : 64;
			vq_cmd_word3_t vq_cmd_w3;
		};
#endif /* Word 7 - End */
	} s8x;
} cpt_inst_s_t;

/**
 * Structure cpt_res_s
 *
 * CPT Result Structure
 * The CPT coprocessor writes the result structure after it completes a
 * CPT_INST_S instruction. The result structure is exactly 16 bytes, and each
 * instruction completion produces exactly one result structure.
 *
 * This structure is stored in memory as little-endian unless
 * CPT()_PF_Q()_CTL[INST_BE] is set.
 */
typedef union cpt_res_s {
	uint64_t u[2];
	struct cpt_res_s_8s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_17_63        : 47;
		/** [ 16: 16] Done interrupt. This bit is copied from the
		 * corresponding instruction's CPT_INST_S[DONEINT].
		 **/
		uint64_t doneint               : 1;
		uint64_t reserved_8_15         : 8;
		/** [  7:  0] Indicates completion/error status of the CPT
		 * coprocessor for the associated instruction, as enumerated by
		 * CPT_COMP_E. Core software may write the memory location
		 * containing [COMPCODE] to 0x0 before ringing the doorbell, and
		 * then poll for completion by checking for a nonzero value.
		 *
		 * Once the core observes a nonzero [COMPCODE] value in this
		 * case, the CPT coprocessor will have also completed L2/DRAM
		 * write operations.
		 **/
		uint64_t compcode              : 8;
#else /* Word 0 - Little Endian */
		uint64_t compcode              : 8;
		uint64_t reserved_8_15         : 8;
		uint64_t doneint               : 1;
		uint64_t reserved_17_63        : 47;
#endif /* Word 0 - End */
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 1 - Big Endian */
		uint64_t reserved_64_127       : 64;
#else /* Word 1 - Little Endian */
		uint64_t reserved_64_127       : 64;
#endif /* Word 1 - End */
	} s8x;
} cpt_res_s_t;

/**
 * Register (NCB) cpt#_vq#_ctl
 *
 * CPT VF Queue Control Registers
 * This register configures queues. This register should be changed (other than
 * clearing [ENA]) only when quiescent (see CPT()_VQ()_INPROG[INFLIGHT]).
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_ctl_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_1_63         : 63;
		/** [  0:  0](R/W/H) Enables the logical instruction queue.
		 * See also CPT()_PF_Q()_CTL[CONT_ERR] and
		 * CPT()_VQ()_INPROG[INFLIGHT].
		 * 1 = Queue is enabled.
		 * 0 = Queue is disabled.
		 **/
		uint64_t ena                   : 1;
#else /* Word 0 - Little Endian */
		uint64_t ena                   : 1;
		uint64_t reserved_1_63         : 63;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_ctl_t;

/**
 * Register (NCB) cpt#_vq#_done
 *
 * CPT Queue Done Count Registers
 * These registers contain the per-queue instruction done count.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_done_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_20_63        : 44;
		/** [ 19:  0](R/W/H) Done count. When CPT_INST_S[DONEINT] set
		 * and that instruction completes,CPT()_VQ()_DONE[DONE] is
		 * incremented when the instruction finishes. Write to this
		 * field are for diagnostic use only; instead software writes
		 * CPT()_VQ()_DONE_ACK with the number of decrements for this
		 * field.
		 *
		 * Interrupts are sent as follows:
		 *
		 * When CPT()_VQ()_DONE[DONE] = 0, then no results are pending,
		 * the interrupt coalescing timer is held to zero, and an
		 * interrupt is not sent.
		 *
		 * When CPT()_VQ()_DONE[DONE] != 0, then the interrupt
		 * coalescing timer counts. If the counter is >= CPT()_VQ()_DONE
		 * _WAIT[TIME_WAIT]*1024, or CPT()_VQ()_DONE[DONE] >= CPT()_VQ()
		 * _DONE_WAIT[NUM_WAIT], i.e. enough time has passed or enough
		 * results have arrived, then the interrupt is sent.  Otherwise,
		 * it is not sent due to coalescing.
		 *
		 * When CPT()_VQ()_DONE_ACK is written (or CPT()_VQ()_DONE is
		 * written but this is not typical), the interrupt coalescing
		 * timer restarts.  Note after decrementing this interrupt
		 * equation is recomputed, for example if CPT()_VQ()_DONE[DONE]
		 * >= CPT()_VQ()_DONE_WAIT[NUM_WAIT] and because the timer is
		 * zero, the interrupt will be resent immediately.  (This covers
		 * the race case between software acknowledging an interrupt and
		 * a result returning.)
		 *
		 * When CPT()_VQ()_DONE_ENA_W1S[DONE] = 0, interrupts are not
		 * sent, but the counting described above still occurs.
		 *
		 * Since CPT instructions complete out-of-order, if software is
		 * using completion interrupts the suggested scheme is to
		 * request a DONEINT on each request, and when an interrupt
		 * arrives perform a "greedy" scan for completions; even if a
		 * later command is acknowledged first this will not result in
		 * missing a completion.
		 *
		 * Software is responsible for making sure [DONE] does not
		 * overflow; for example by insuring there are not more than
		 * 2^20-1 instructions in flight that may request interrupts.
		 **/
		uint64_t done                  : 20;
#else /* Word 0 - Little Endian */
		uint64_t done                  : 20;
		uint64_t reserved_20_63        : 44;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_done_t;

/**
 * Register (NCB) cpt#_vq#_done_ack
 *
 * CPT Queue Done Count Ack Registers
 * This register is written by software to acknowledge interrupts.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_done_ack_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_20_63        : 44;
		/** [ 19:  0](R/W/H) Number of decrements to CPT()_VQ()_DONE
		 * [DONE]. Reads CPT()_VQ()_DONE[DONE].
		 *
		 * Written by software to acknowledge interrupts. If CPT()_VQ()_
		 * DONE[DONE] is still nonzero the interrupt will be re-sent if
		 * the conditions described in CPT()_VQ()_DONE[DONE] are
		 * satisfied.
		 **/
		uint64_t done_ack              : 20;
#else /* Word 0 - Little Endian */
		uint64_t done_ack              : 20;
		uint64_t reserved_20_63        : 44;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_done_ack_t;

/**
 * Register (NCB) cpt#_vq#_done_wait
 *
 * CPT Queue Done Interrupt Coalescing Wait Registers
 * Specifies the per queue interrupt coalescing settings.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_done_wait_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_48_63        : 16;
		/** [ 47: 32](R/W) Time hold-off. When CPT()_VQ()_DONE[DONE] =
		 * 0, or CPT()_VQ()_DONE_ACK is written a timer is cleared. When
		 * the timer reaches [TIME_WAIT]*1024 then interrupt coalescing
		 * ends; see CPT()_VQ()_DONE[DONE]. If 0x0, time coalescing is
		 * disabled.
		 **/
		uint64_t time_wait             : 16;
		uint64_t reserved_20_31        : 12;
		/** [ 19:  0](R/W) Number of messages hold-off. When
		 * CPT()_VQ()_DONE[DONE] >= [NUM_WAIT] then interrupt coalescing
		 * ends; see CPT()_VQ()_DONE[DONE]. If 0x0, same behavior as
		 * 0x1.
		 **/
		uint64_t num_wait              : 20;
#else /* Word 0 - Little Endian */
		uint64_t num_wait              : 20;
		uint64_t reserved_20_31        : 12;
		uint64_t time_wait             : 16;
		uint64_t reserved_48_63        : 16;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_done_wait_t;

/**
 * Register (NCB) cpt#_vq#_doorbell
 *
 * CPT Queue Doorbell Registers
 * Doorbells for the CPT instruction queues.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_doorbell_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_20_63        : 44;
		uint64_t dbell_cnt             : 20;
		/** [ 19:  0](R/W/H) Number of instruction queue 64-bit words
		 * to add to the CPT instruction doorbell count. Readback value
		 * is the the current number of pending doorbell requests.
		 *
		 * If counter overflows CPT()_VQ()_MISC_INT[DBELL_DOVF] is set.
		 *
		 * To reset the count back to zero, write one to clear
		 * CPT()_VQ()_MISC_INT_ENA_W1C[DBELL_DOVF], then write a value
		 * of 2^20 minus the read [DBELL_CNT], then write one to
		 * CPT()_VQ()_MISC_INT_W1C[DBELL_DOVF] and
		 * CPT()_VQ()_MISC_INT_ENA_W1S[DBELL_DOVF].
		 *
		 * Must be a multiple of 8.  All CPT instructions are 8 words
		 * and require a doorbell count of multiple of 8.
		 **/
#else /* Word 0 - Little Endian */
		uint64_t dbell_cnt             : 20;
		uint64_t reserved_20_63        : 44;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_doorbell_t;

/**
 * Register (NCB) cpt#_vq#_inprog
 *
 * CPT Queue In Progress Count Registers
 * These registers contain the per-queue instruction in flight registers.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_inprog_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_8_63         : 56;
		/** [  7:  0](RO/H) Inflight count. Counts the number of
		 * instructions for the VF for which CPT is fetching, executing
		 * or responding to instructions. However this does not include
		 * any interrupts that are awaiting software handling
		 * (CPT()_VQ()_DONE[DONE] != 0x0).
		 *
		 * A queue may not be reconfigured until:
		 *  1. CPT()_VQ()_CTL[ENA] is cleared by software.
		 *  2. [INFLIGHT] is polled until equals to zero.
		 **/
		uint64_t inflight              : 8;
#else /* Word 0 - Little Endian */
		uint64_t inflight              : 8;
		uint64_t reserved_8_63         : 56;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_inprog_t;

/**
 * Register (NCB) cpt#_vq#_misc_int
 *
 * CPT Queue Misc Interrupt Register
 * These registers contain the per-queue miscellaneous interrupts.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_misc_int_s {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_7_63         : 57;
		/** [  6:  6](R/W1C/H) Translation fault detected. */
		uint64_t fault		       : 1;
		/** [  5:  5](R/W1C/H) Hardware error from engines. */
		uint64_t hwerr		       : 1;
		/** [  4:  4](R/W1C/H) Software error from engines. */
		uint64_t swerr                 : 1;
		/** [  3:  3](R/W1C/H) NCB result write response error. */
		uint64_t nwrp                  : 1;
		/** [  2:  2](R/W1C/H) Instruction NCB read response error. */
		uint64_t irde                  : 1;
		/** [  1:  1](R/W1C/H) Doorbell overflow. */
		uint64_t dovf                  : 1;
		/** [  0:  0](R/W1C/H) PF to VF mailbox interrupt. Set when
		 * CPT()_VF()_PF_MBOX(0) is written.
		 **/
		uint64_t mbox                  : 1;
#else /* Word 0 - Little Endian */
		uint64_t mbox                  : 1;
		uint64_t dovf                  : 1;
		uint64_t irde                  : 1;
		uint64_t nwrp                  : 1;
		uint64_t swerr                 : 1;
		uint64_t hwerr		       : 1;
		uint64_t fault		       : 1;
		uint64_t reserved_5_63         : 59;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_misc_int_t;

/**
 * Register (NCB) cpt#_vq#_saddr
 *
 * CPT Queue Starting Buffer Address Registers
 * These registers set the instruction buffer starting address.
 */
typedef union {
	uint64_t u;
	struct cptx_vqx_saddr_s	{
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN) /* Word 0 - Big Endian */
		uint64_t reserved_49_63        : 15;
		/** [ 48:  6](R/W/H) Instruction buffer IOVA <48:6>
		 * (64-byte aligned). When written, it is the initial buffer
		 * starting address; when read, it is the next read pointer to
		 * be requested from L2C. The PTR field is overwritten with the
		 * next pointer each time that the command buffer segment is
		 * exhausted. New commands will then be read from the newly
		 * specified command buffer pointer.
		 **/
		uint64_t ptr                   : 43;
		uint64_t reserved_0_5          : 6;
#else /* Word 0 - Little Endian */
		uint64_t reserved_0_5          : 6;
		uint64_t ptr                   : 43;
		uint64_t reserved_49_63        : 15;
#endif /* Word 0 - End */
	} s;
} cptx_vqx_saddr_t;

#endif /*_CPT_HW_TYPES_H_ */
