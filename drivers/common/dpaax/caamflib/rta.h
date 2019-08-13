/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */

#ifndef __RTA_RTA_H__
#define __RTA_RTA_H__

#include "rta/sec_run_time_asm.h"
#include "rta/fifo_load_store_cmd.h"
#include "rta/header_cmd.h"
#include "rta/jump_cmd.h"
#include "rta/key_cmd.h"
#include "rta/load_cmd.h"
#include "rta/math_cmd.h"
#include "rta/move_cmd.h"
#include "rta/nfifo_cmd.h"
#include "rta/operation_cmd.h"
#include "rta/protocol_cmd.h"
#include "rta/seq_in_out_ptr_cmd.h"
#include "rta/signature_cmd.h"
#include "rta/store_cmd.h"

/**
 * DOC: About
 *
 * RTA (Runtime Assembler) Library is an easy and flexible runtime method for
 * writing SEC descriptors. It implements a thin abstraction layer above
 * SEC commands set; the resulting code is compact and similar to a
 * descriptor sequence.
 *
 * RTA library improves comprehension of the SEC code, adds flexibility for
 * writing complex descriptors and keeps the code lightweight. Should be used
 * by whom needs to encode descriptors at runtime, with comprehensible flow
 * control in descriptor.
 */

/**
 * DOC: Usage
 *
 * RTA is used in kernel space by the SEC / CAAM (Cryptographic Acceleration and
 * Assurance Module) kernel module (drivers/crypto/caam) and SEC / CAAM QI
 * kernel module (Freescale QorIQ SDK).
 *
 * RTA is used in user space by USDPAA - User Space DataPath Acceleration
 * Architecture (Freescale QorIQ SDK).
 */

/**
 * DOC: Descriptor Buffer Management Routines
 *
 * Contains details of RTA descriptor buffer management and SEC Era
 * management routines.
 */

/**
 * PROGRAM_CNTXT_INIT - must be called before any descriptor run-time assembly
 *                      call type field carry info i.e. whether descriptor is
 *                      shared or job descriptor.
 * @program: pointer to struct program
 * @buffer: input buffer where the descriptor will be placed (uint32_t *)
 * @offset: offset in input buffer from where the data will be written
 *          (unsigned int)
 */
#define PROGRAM_CNTXT_INIT(program, buffer, offset) \
	rta_program_cntxt_init(program, buffer, offset)

/**
 * PROGRAM_FINALIZE - must be called to mark completion of RTA call.
 * @program: pointer to struct program
 *
 * Return: total size of the descriptor in words or negative number on error.
 */
#define PROGRAM_FINALIZE(program) rta_program_finalize(program)

/**
 * PROGRAM_SET_36BIT_ADDR - must be called to set pointer size to 36 bits
 * @program: pointer to struct program
 *
 * Return: current size of the descriptor in words (unsigned int).
 */
#define PROGRAM_SET_36BIT_ADDR(program) rta_program_set_36bit_addr(program)

/**
 * PROGRAM_SET_BSWAP - must be called to enable byte swapping
 * @program: pointer to struct program
 *
 * Byte swapping on a 4-byte boundary will be performed at the end - when
 * calling PROGRAM_FINALIZE().
 *
 * Return: current size of the descriptor in words (unsigned int).
 */
#define PROGRAM_SET_BSWAP(program) rta_program_set_bswap(program)

/**
 * WORD - must be called to insert in descriptor buffer a 32bit value
 * @program: pointer to struct program
 * @val: input value to be written in descriptor buffer (uint32_t)
 *
 * Return: the descriptor buffer offset where this command is inserted
 * (unsigned int).
 */
#define WORD(program, val) rta_word(program, val)

/**
 * DWORD - must be called to insert in descriptor buffer a 64bit value
 * @program: pointer to struct program
 * @val: input value to be written in descriptor buffer (uint64_t)
 *
 * Return: the descriptor buffer offset where this command is inserted
 * (unsigned int).
 */
#define DWORD(program, val) rta_dword(program, val)

/**
 * COPY_DATA - must be called to insert in descriptor buffer data larger than
 *             64bits.
 * @program: pointer to struct program
 * @data: input data to be written in descriptor buffer (uint8_t *)
 * @len: length of input data (unsigned int)
 *
 * Return: the descriptor buffer offset where this command is inserted
 * (unsigned int).
 */
#define COPY_DATA(program, data, len) rta_copy_data(program, (data), (len))

/**
 * DESC_LEN -  determines job / shared descriptor buffer length (in words)
 * @buffer: descriptor buffer (uint32_t *)
 *
 * Return: descriptor buffer length in words (unsigned int).
 */
#define DESC_LEN(buffer) rta_desc_len(buffer)

/**
 * DESC_BYTES - determines job / shared descriptor buffer length (in bytes)
 * @buffer: descriptor buffer (uint32_t *)
 *
 * Return: descriptor buffer length in bytes (unsigned int).
 */
#define DESC_BYTES(buffer) rta_desc_bytes(buffer)

/*
 * SEC HW block revision.
 *
 * This *must not be confused with SEC version*:
 * - SEC HW block revision format is "v"
 * - SEC revision format is "x.y"
 */
extern enum rta_sec_era rta_sec_era;

/**
 * rta_set_sec_era - Set SEC Era HW block revision for which the RTA library
 *                   will generate the descriptors.
 * @era: SEC Era (enum rta_sec_era)
 *
 * Return: 0 if the ERA was set successfully, -1 otherwise (int)
 *
 * Warning 1: Must be called *only once*, *before* using any other RTA API
 * routine.
 *
 * Warning 2: *Not thread safe*.
 */
static inline int
rta_set_sec_era(enum rta_sec_era era)
{
	if (era > MAX_SEC_ERA) {
		rta_sec_era = DEFAULT_SEC_ERA;
		pr_err("Unsupported SEC ERA. Defaulting to ERA %d\n",
		       DEFAULT_SEC_ERA + 1);
		return -1;
	}

	rta_sec_era = era;
	return 0;
}

/**
 * rta_get_sec_era - Get SEC Era HW block revision for which the RTA library
 *                   will generate the descriptors.
 *
 * Return: SEC Era (unsigned int).
 */
static inline unsigned int
rta_get_sec_era(void)
{
	return rta_sec_era;
}

/**
 * DOC: SEC Commands Routines
 *
 * Contains details of RTA wrapper routines over SEC engine commands.
 */

/**
 * SHR_HDR - Configures Shared Descriptor HEADER command
 * @program: pointer to struct program
 * @share: descriptor share state (enum rta_share_type)
 * @start_idx: index in descriptor buffer where the execution of the shared
 *             descriptor should start (@c unsigned int).
 * @flags: operational flags: RIF, DNR, CIF, SC, PD
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SHR_HDR(program, share, start_idx, flags) \
	rta_shr_header(program, share, start_idx, flags)

/**
 * JOB_HDR - Configures JOB Descriptor HEADER command
 * @program: pointer to struct program
 * @share: descriptor share state (enum rta_share_type)
 * @start_idx: index in descriptor buffer where the execution of the job
 *            descriptor should start (unsigned int). In case SHR bit is present
 *            in flags, this will be the shared descriptor length.
 * @share_desc: pointer to shared descriptor, in case SHR bit is set (uint64_t)
 * @flags: operational flags: RSMS, DNR, TD, MTD, REO, SHR
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define JOB_HDR(program, share, start_idx, share_desc, flags) \
	rta_job_header(program, share, start_idx, share_desc, flags, 0)

/**
 * JOB_HDR_EXT - Configures JOB Descriptor HEADER command
 * @program: pointer to struct program
 * @share: descriptor share state (enum rta_share_type)
 * @start_idx: index in descriptor buffer where the execution of the job
 *            descriptor should start (unsigned int). In case SHR bit is present
 *            in flags, this will be the shared descriptor length.
 * @share_desc: pointer to shared descriptor, in case SHR bit is set (uint64_t)
 * @flags: operational flags: RSMS, DNR, TD, MTD, REO, SHR
 * @ext_flags: extended header flags: DSV (DECO Select Valid), DECO Id (limited
 *             by DSEL_MASK).
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define JOB_HDR_EXT(program, share, start_idx, share_desc, flags, ext_flags) \
	rta_job_header(program, share, start_idx, share_desc, flags | EXT, \
		       ext_flags)

/**
 * MOVE - Configures MOVE and MOVE_LEN commands
 * @program: pointer to struct program
 * @src: internal source of data that will be moved: CONTEXT1, CONTEXT2, OFIFO,
 *       DESCBUF, MATH0-MATH3, IFIFOABD, IFIFOAB1, IFIFOAB2, AB1, AB2, ABD.
 * @src_offset: offset in source data (uint16_t)
 * @dst: internal destination of data that will be moved: CONTEXT1, CONTEXT2,
 *       OFIFO, DESCBUF, MATH0-MATH3, IFIFOAB1, IFIFOAB2, IFIFO, PKA, KEY1,
 *       KEY2, ALTSOURCE.
 * @dst_offset: offset in destination data (uint16_t)
 * @length: size of data to be moved: for MOVE must be specified as immediate
 *          value and IMMED flag must be set; for MOVE_LEN must be specified
 *          using MATH0-MATH3.
 * @opt: operational flags: WAITCOMP, FLUSH1, FLUSH2, LAST1, LAST2, SIZE_WORD,
 *       SIZE_BYTE, SIZE_DWORD, IMMED (not valid for MOVE_LEN).
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MOVE(program, src, src_offset, dst, dst_offset, length, opt) \
	rta_move(program, __MOVE, src, src_offset, dst, dst_offset, length, opt)

/**
 * MOVEB - Configures MOVEB command
 * @program: pointer to struct program
 * @src: internal source of data that will be moved: CONTEXT1, CONTEXT2, OFIFO,
 *       DESCBUF, MATH0-MATH3, IFIFOABD, IFIFOAB1, IFIFOAB2, AB1, AB2, ABD.
 * @src_offset: offset in source data (uint16_t)
 * @dst: internal destination of data that will be moved: CONTEXT1, CONTEXT2,
 *       OFIFO, DESCBUF, MATH0-MATH3, IFIFOAB1, IFIFOAB2, IFIFO, PKA, KEY1,
 *       KEY2, ALTSOURCE.
 * @dst_offset: offset in destination data (uint16_t)
 * @length: size of data to be moved: for MOVE must be specified as immediate
 *          value and IMMED flag must be set; for MOVE_LEN must be specified
 *          using MATH0-MATH3.
 * @opt: operational flags: WAITCOMP, FLUSH1, FLUSH2, LAST1, LAST2, SIZE_WORD,
 *       SIZE_BYTE, SIZE_DWORD, IMMED (not valid for MOVE_LEN).
 *
 * Identical with MOVE command if byte swapping not enabled; else - when src/dst
 * is descriptor buffer or MATH registers, data type is byte array when MOVE
 * data type is 4-byte array and vice versa.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MOVEB(program, src, src_offset, dst, dst_offset, length, opt) \
	rta_move(program, __MOVEB, src, src_offset, dst, dst_offset, length, \
		 opt)

/**
 * MOVEDW - Configures MOVEDW command
 * @program: pointer to struct program
 * @src: internal source of data that will be moved: CONTEXT1, CONTEXT2, OFIFO,
 *       DESCBUF, MATH0-MATH3, IFIFOABD, IFIFOAB1, IFIFOAB2, AB1, AB2, ABD.
 * @src_offset: offset in source data (uint16_t)
 * @dst: internal destination of data that will be moved: CONTEXT1, CONTEXT2,
 *       OFIFO, DESCBUF, MATH0-MATH3, IFIFOAB1, IFIFOAB2, IFIFO, PKA, KEY1,
 *       KEY2, ALTSOURCE.
 * @dst_offset: offset in destination data (uint16_t)
 * @length: size of data to be moved: for MOVE must be specified as immediate
 *          value and IMMED flag must be set; for MOVE_LEN must be specified
 *          using MATH0-MATH3.
 * @opt: operational flags: WAITCOMP, FLUSH1, FLUSH2, LAST1, LAST2, SIZE_WORD,
 *       SIZE_BYTE, SIZE_DWORD, IMMED (not valid for MOVE_LEN).
 *
 * Identical with MOVE command, with the following differences: data type is
 * 8-byte array; word swapping is performed when SEC is programmed in little
 * endian mode.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MOVEDW(program, src, src_offset, dst, dst_offset, length, opt) \
	rta_move(program, __MOVEDW, src, src_offset, dst, dst_offset, length, \
		 opt)

/**
 * FIFOLOAD - Configures FIFOLOAD command to load message data, PKHA data, IV,
 *            ICV, AAD and bit length message data into Input Data FIFO.
 * @program: pointer to struct program
 * @data: input data type to store: PKHA registers, IFIFO, MSG1, MSG2,
 *        MSGOUTSNOOP, MSGINSNOOP, IV1, IV2, AAD1, ICV1, ICV2, BIT_DATA, SKIP.
 * @src: pointer or actual data in case of immediate load; IMMED, COPY and DCOPY
 *       flags indicate action taken (inline imm data, inline ptr, inline from
 *       ptr).
 * @length: number of bytes to load (uint32_t)
 * @flags: operational flags: SGF, IMMED, EXT, CLASS1, CLASS2, BOTH, FLUSH1,
 *         LAST1, LAST2, COPY, DCOPY.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define FIFOLOAD(program, data, src, length, flags) \
	rta_fifo_load(program, data, src, length, flags)

/**
 * SEQFIFOLOAD - Configures SEQ FIFOLOAD command to load message data, PKHA
 *               data, IV, ICV, AAD and bit length message data into Input Data
 *               FIFO.
 * @program: pointer to struct program
 * @data: input data type to store: PKHA registers, IFIFO, MSG1, MSG2,
 *        MSGOUTSNOOP, MSGINSNOOP, IV1, IV2, AAD1, ICV1, ICV2, BIT_DATA, SKIP.
 * @length: number of bytes to load; can be set to 0 for SEQ command w/ VLF set
 *          (uint32_t).
 * @flags: operational flags: VLF, CLASS1, CLASS2, BOTH, FLUSH1, LAST1, LAST2,
 *         AIDF.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQFIFOLOAD(program, data, length, flags) \
	rta_fifo_load(program, data, NONE, length, flags|SEQ)

/**
 * FIFOSTORE - Configures FIFOSTORE command, to move data from Output Data FIFO
 *             to external memory via DMA.
 * @program: pointer to struct program
 * @data: output data type to store: PKHA registers, IFIFO, OFIFO, RNG,
 *        RNGOFIFO, AFHA_SBOX, MDHA_SPLIT_KEY, MSG, KEY1, KEY2, SKIP.
 * @encrypt_flags: store data encryption mode: EKT, TK
 * @dst: pointer to store location (uint64_t)
 * @length: number of bytes to load (uint32_t)
 * @flags: operational flags: SGF, CONT, EXT, CLASS1, CLASS2, BOTH
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define FIFOSTORE(program, data, encrypt_flags, dst, length, flags) \
	rta_fifo_store(program, data, encrypt_flags, dst, length, flags)

/**
 * SEQFIFOSTORE - Configures SEQ FIFOSTORE command, to move data from Output
 *                Data FIFO to external memory via DMA.
 * @program: pointer to struct program
 * @data: output data type to store: PKHA registers, IFIFO, OFIFO, RNG,
 *        RNGOFIFO, AFHA_SBOX, MDHA_SPLIT_KEY, MSG, KEY1, KEY2, METADATA, SKIP.
 * @encrypt_flags: store data encryption mode: EKT, TK
 * @length: number of bytes to load; can be set to 0 for SEQ command w/ VLF set
 *          (uint32_t).
 * @flags: operational flags: VLF, CONT, EXT, CLASS1, CLASS2, BOTH
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQFIFOSTORE(program, data, encrypt_flags, length, flags) \
	rta_fifo_store(program, data, encrypt_flags, 0, length, flags|SEQ)

/**
 * KEY - Configures KEY and SEQ KEY commands
 * @program: pointer to struct program
 * @key_dst: key store location: KEY1, KEY2, PKE, AFHA_SBOX, MDHA_SPLIT_KEY
 * @encrypt_flags: key encryption mode: ENC, EKT, TK, NWB, PTS
 * @src: pointer or actual data in case of immediate load (uint64_t); IMMED,
 *       COPY and DCOPY flags indicate action taken (inline imm data,
 *       inline ptr, inline from ptr).
 * @length: number of bytes to load; can be set to 0 for SEQ command w/ VLF set
 *          (uint32_t).
 * @flags: operational flags: for KEY: SGF, IMMED, COPY, DCOPY; for SEQKEY: SEQ,
 *         VLF, AIDF.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define KEY(program, key_dst, encrypt_flags, src, length, flags) \
	rta_key(program, key_dst, encrypt_flags, src, length, flags)

/**
 * SEQINPTR - Configures SEQ IN PTR command
 * @program: pointer to struct program
 * @src: starting address for Input Sequence (uint64_t)
 * @length: number of bytes in (or to be added to) Input Sequence (uint32_t)
 * @flags: operational flags: RBS, INL, SGF, PRE, EXT, RTO, RJD, SOP (when PRE,
 *         RTO or SOP are set, @src parameter must be 0).
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQINPTR(program, src, length, flags) \
	rta_seq_in_ptr(program, src, length, flags)

/**
 * SEQOUTPTR - Configures SEQ OUT PTR command
 * @program: pointer to struct program
 * @dst: starting address for Output Sequence (uint64_t)
 * @length: number of bytes in (or to be added to) Output Sequence (uint32_t)
 * @flags: operational flags: SGF, PRE, EXT, RTO, RST, EWS (when PRE or RTO are
 *         set, @dst parameter must be 0).
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQOUTPTR(program, dst, length, flags) \
	rta_seq_out_ptr(program, dst, length, flags)

/**
 * ALG_OPERATION - Configures ALGORITHM OPERATION command
 * @program: pointer to struct program
 * @cipher_alg: algorithm to be used
 * @aai: Additional Algorithm Information; contains mode information that is
 *       associated with the algorithm (check desc.h for specific values).
 * @algo_state: algorithm state; defines the state of the algorithm that is
 *              being executed (check desc.h file for specific values).
 * @icv_check: ICV checking; selects whether the algorithm should check
 *             calculated ICV with known ICV: ICV_CHECK_ENABLE,
 *             ICV_CHECK_DISABLE.
 * @enc: selects between encryption and decryption: DIR_ENC, DIR_DEC
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define ALG_OPERATION(program, cipher_alg, aai, algo_state, icv_check, enc) \
	rta_operation(program, cipher_alg, aai, algo_state, icv_check, enc)

/**
 * PROTOCOL - Configures PROTOCOL OPERATION command
 * @program: pointer to struct program
 * @optype: operation type: OP_TYPE_UNI_PROTOCOL / OP_TYPE_DECAP_PROTOCOL /
 *          OP_TYPE_ENCAP_PROTOCOL.
 * @protid: protocol identifier value (check desc.h file for specific values)
 * @protoinfo: protocol dependent value (check desc.h file for specific values)
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define PROTOCOL(program, optype, protid, protoinfo) \
	rta_proto_operation(program, optype, protid, protoinfo)

/**
 * DKP_PROTOCOL - Configures DKP (Derived Key Protocol) PROTOCOL command
 * @program: pointer to struct program
 * @protid: protocol identifier value - one of the following:
 *          OP_PCLID_DKP_{MD5 | SHA1 | SHA224 | SHA256 | SHA384 | SHA512}
 * @key_src: How the initial ("negotiated") key is provided to the DKP protocol.
 *           Valid values - one of OP_PCL_DKP_SRC_{IMM, SEQ, PTR, SGF}. Not all
 *           (key_src,key_dst) combinations are allowed.
 * @key_dst: How the derived ("split") key is returned by the DKP protocol.
 *           Valid values - one of OP_PCL_DKP_DST_{IMM, SEQ, PTR, SGF}. Not all
 *           (key_src,key_dst) combinations are allowed.
 * @keylen: length of the initial key, in bytes (uint16_t)
 * @key: address where algorithm key resides; virtual address if key_type is
 *       RTA_DATA_IMM, physical (bus) address if key_type is RTA_DATA_PTR or
 *       RTA_DATA_IMM_DMA.
 * @key_type: enum rta_data_type
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define DKP_PROTOCOL(program, protid, key_src, key_dst, keylen, key, key_type) \
	rta_dkp_proto(program, protid, key_src, key_dst, keylen, key, key_type)

/**
 * PKHA_OPERATION - Configures PKHA OPERATION command
 * @program: pointer to struct program
 * @op_pkha: PKHA operation; indicates the modular arithmetic function to
 *           execute (check desc.h file for specific values).
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define PKHA_OPERATION(program, op_pkha)   rta_pkha_operation(program, op_pkha)

/**
 * JUMP - Configures JUMP command
 * @program: pointer to struct program
 * @addr: local offset for local jumps or address pointer for non-local jumps;
 *        IMM or PTR macros must be used to indicate type.
 * @jump_type: type of action taken by jump (enum rta_jump_type)
 * @test_type: defines how jump conditions are evaluated (enum rta_jump_cond)
 * @cond: jump conditions: operational flags - DONE1, DONE2, BOTH; various
 *        sharing and wait conditions (JSL = 1) - NIFP, NIP, NOP, NCP, CALM,
 *        SELF, SHARED, JQP; Math and PKHA status conditions (JSL = 0) - Z, N,
 *        NV, C, PK0, PK1, PKP.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define JUMP(program, addr, jump_type, test_type, cond) \
	rta_jump(program, addr, jump_type, test_type, cond, NONE)

/**
 * JUMP_INC - Configures JUMP_INC command
 * @program: pointer to struct program
 * @addr: local offset; IMM or PTR macros must be used to indicate type
 * @test_type: defines how jump conditions are evaluated (enum rta_jump_cond)
 * @cond: jump conditions: Math status conditions (JSL = 0): Z, N, NV, C
 * @src_dst: register to increment / decrement: MATH0-MATH3, DPOVRD, SEQINSZ,
 *           SEQOUTSZ, VSEQINSZ, VSEQOUTSZ.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define JUMP_INC(program, addr, test_type, cond, src_dst) \
	rta_jump(program, addr, LOCAL_JUMP_INC, test_type, cond, src_dst)

/**
 * JUMP_DEC - Configures JUMP_DEC command
 * @program: pointer to struct program
 * @addr: local offset; IMM or PTR macros must be used to indicate type
 * @test_type: defines how jump conditions are evaluated (enum rta_jump_cond)
 * @cond: jump conditions: Math status conditions (JSL = 0): Z, N, NV, C
 * @src_dst: register to increment / decrement: MATH0-MATH3, DPOVRD, SEQINSZ,
 *           SEQOUTSZ, VSEQINSZ, VSEQOUTSZ.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define JUMP_DEC(program, addr, test_type, cond, src_dst) \
	rta_jump(program, addr, LOCAL_JUMP_DEC, test_type, cond, src_dst)

/**
 * LOAD - Configures LOAD command to load data registers from descriptor or from
 *        a memory location.
 * @program: pointer to struct program
 * @addr: immediate value or pointer to the data to be loaded; IMMED, COPY and
 *        DCOPY flags indicate action taken (inline imm data, inline ptr, inline
 *        from ptr).
 * @dst: destination register (uint64_t)
 * @offset: start point to write data in destination register (uint32_t)
 * @length: number of bytes to load (uint32_t)
 * @flags: operational flags: VLF, IMMED, COPY, DCOPY
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define LOAD(program, addr, dst, offset, length, flags) \
	rta_load(program, addr, dst, offset, length, flags)

/**
 * SEQLOAD - Configures SEQ LOAD command to load data registers from descriptor
 *           or from a memory location.
 * @program: pointer to struct program
 * @dst: destination register (uint64_t)
 * @offset: start point to write data in destination register (uint32_t)
 * @length: number of bytes to load (uint32_t)
 * @flags: operational flags: SGF
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQLOAD(program, dst, offset, length, flags) \
	rta_load(program, NONE, dst, offset, length, flags|SEQ)

/**
 * STORE - Configures STORE command to read data from registers and write them
 *         to a memory location.
 * @program: pointer to struct program
 * @src: immediate value or source register for data to be stored: KEY1SZ,
 *       KEY2SZ, DJQDA, MODE1, MODE2, DJQCTRL, DATA1SZ, DATA2SZ, DSTAT, ICV1SZ,
 *       ICV2SZ, DPID, CCTRL, ICTRL, CLRW, CSTAT, MATH0-MATH3, PKHA registers,
 *       CONTEXT1, CONTEXT2, DESCBUF, JOBDESCBUF, SHAREDESCBUF. In case of
 *       immediate value, IMMED, COPY and DCOPY flags indicate action taken
 *       (inline imm data, inline ptr, inline from ptr).
 * @offset: start point for reading from source register (uint16_t)
 * @dst: pointer to store location (uint64_t)
 * @length: number of bytes to store (uint32_t)
 * @flags: operational flags: VLF, IMMED, COPY, DCOPY
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define STORE(program, src, offset, dst, length, flags) \
	rta_store(program, src, offset, dst, length, flags)

/**
 * SEQSTORE - Configures SEQ STORE command to read data from registers and write
 *            them to a memory location.
 * @program: pointer to struct program
 * @src: immediate value or source register for data to be stored: KEY1SZ,
 *       KEY2SZ, DJQDA, MODE1, MODE2, DJQCTRL, DATA1SZ, DATA2SZ, DSTAT, ICV1SZ,
 *       ICV2SZ, DPID, CCTRL, ICTRL, CLRW, CSTAT, MATH0-MATH3, PKHA registers,
 *       CONTEXT1, CONTEXT2, DESCBUF, JOBDESCBUF, SHAREDESCBUF. In case of
 *       immediate value, IMMED, COPY and DCOPY flags indicate action taken
 *       (inline imm data, inline ptr, inline from ptr).
 * @offset: start point for reading from source register (uint16_t)
 * @length: number of bytes to store (uint32_t)
 * @flags: operational flags: SGF, IMMED, COPY, DCOPY
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SEQSTORE(program, src, offset, length, flags) \
	rta_store(program, src, offset, NONE, length, flags|SEQ)

/**
 * MATHB - Configures MATHB command to perform binary operations
 * @program: pointer to struct program
 * @operand1: first operand: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ, VSEQINSZ,
 *            VSEQOUTSZ, ZERO, ONE, NONE, Immediate value. IMMED must be used to
 *            indicate immediate value.
 * @operator: function to be performed: ADD, ADDC, SUB, SUBB, OR, AND, XOR,
 *            LSHIFT, RSHIFT, SHLD.
 * @operand2: second operand: MATH0-MATH3, DPOVRD, VSEQINSZ, VSEQOUTSZ, ABD,
 *            OFIFO, JOBSRC, ZERO, ONE, Immediate value. IMMED2 must be used to
 *            indicate immediate value.
 * @result: destination for the result: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ,
 *          NONE, VSEQINSZ, VSEQOUTSZ.
 * @length: length in bytes of the operation and the immediate value, if there
 *          is one (int).
 * @opt: operational flags: IFB, NFU, STL, SWP, IMMED, IMMED2
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MATHB(program, operand1, operator, operand2, result, length, opt) \
	rta_math(program, operand1, MATH_FUN_##operator, operand2, result, \
		 length, opt)

/**
 * MATHI - Configures MATHI command to perform binary operations
 * @program: pointer to struct program
 * @operand: if !SSEL: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ, VSEQINSZ,
 *           VSEQOUTSZ, ZERO, ONE.
 *           if SSEL: MATH0-MATH3, DPOVRD, VSEQINSZ, VSEQOUTSZ, ABD, OFIFO,
 *           JOBSRC, ZERO, ONE.
 * @operator: function to be performed: ADD, ADDC, SUB, SUBB, OR, AND, XOR,
 *            LSHIFT, RSHIFT, FBYT (for !SSEL only).
 * @imm: Immediate value (uint8_t). IMMED must be used to indicate immediate
 *       value.
 * @result: destination for the result: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ,
 *          NONE, VSEQINSZ, VSEQOUTSZ.
 * @length: length in bytes of the operation and the immediate value, if there
 *          is one (int). @imm is left-extended with zeros if needed.
 * @opt: operational flags: NFU, SSEL, SWP, IMMED
 *
 * If !SSEL, @operand <@operator> @imm -> @result
 * If SSEL, @imm <@operator> @operand -> @result
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MATHI(program, operand, operator, imm, result, length, opt) \
	rta_mathi(program, operand, MATH_FUN_##operator, imm, result, length, \
		  opt)

/**
 * MATHU - Configures MATHU command to perform unary operations
 * @program: pointer to struct program
 * @operand1: operand: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ, VSEQINSZ,
 *            VSEQOUTSZ, ZERO, ONE, NONE, Immediate value. IMMED must be used to
 *            indicate immediate value.
 * @operator: function to be performed: ZBYT, BSWAP
 * @result: destination for the result: MATH0-MATH3, DPOVRD, SEQINSZ, SEQOUTSZ,
 *          NONE, VSEQINSZ, VSEQOUTSZ.
 * @length: length in bytes of the operation and the immediate value, if there
 *          is one (int).
 * @opt: operational flags: NFU, STL, SWP, IMMED
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define MATHU(program, operand1, operator, result, length, opt) \
	rta_math(program, operand1, MATH_FUN_##operator, NONE, result, length, \
		 opt)

/**
 * SIGNATURE - Configures SIGNATURE command
 * @program: pointer to struct program
 * @sign_type: signature type: SIGN_TYPE_FINAL, SIGN_TYPE_FINAL_RESTORE,
 *             SIGN_TYPE_FINAL_NONZERO, SIGN_TYPE_IMM_2, SIGN_TYPE_IMM_3,
 *             SIGN_TYPE_IMM_4.
 *
 * After SIGNATURE command, DWORD or WORD must be used to insert signature in
 * descriptor buffer.
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define SIGNATURE(program, sign_type)   rta_signature(program, sign_type)

/**
 * NFIFOADD - Configures NFIFO command, a shortcut of RTA Load command to write
 *            to iNfo FIFO.
 * @program: pointer to struct program
 * @src: source for the input data in Alignment Block:IFIFO, OFIFO, PAD,
 *       MSGOUTSNOOP, ALTSOURCE, OFIFO_SYNC, MSGOUTSNOOP_ALT.
 * @data: type of data that is going through the Input Data FIFO: MSG, MSG1,
 *        MSG2, IV1, IV2, ICV1, ICV2, SAD1, AAD1, AAD2, AFHA_SBOX, SKIP,
 *        PKHA registers, AB1, AB2, ABD.
 * @length: length of the data copied in FIFO registers (uint32_t)
 * @flags: select options between:
 *         -operational flags: LAST1, LAST2, FLUSH1, FLUSH2, OC, BP
 *         -when PAD is selected as source: BM, PR, PS
 *         -padding type: <em>PAD_ZERO, PAD_NONZERO, PAD_INCREMENT, PAD_RANDOM,
 *          PAD_ZERO_N1, PAD_NONZERO_0, PAD_N1, PAD_NONZERO_N
 *
 * Return: On success, descriptor buffer offset where this command is inserted.
 *         On error, a negative error code; first error program counter will
 *         point to offset in descriptor buffer where the instruction should
 *         have been written.
 */
#define NFIFOADD(program, src, data, length, flags) \
	rta_nfifo_load(program, src, data, length, flags)

/**
 * DOC: Self Referential Code Management Routines
 *
 * Contains details of RTA self referential code routines.
 */

/**
 * REFERENCE - initialize a variable used for storing an index inside a
 *             descriptor buffer.
 * @ref: reference to a descriptor buffer's index where an update is required
 *       with a value that will be known latter in the program flow.
 */
#define REFERENCE(ref)    int ref = -1

/**
 * LABEL - initialize a variable used for storing an index inside a descriptor
 *         buffer.
 * @label: label stores the value with what should be updated the REFERENCE line
 *         in the descriptor buffer.
 */
#define LABEL(label)      unsigned int label = 0

/**
 * SET_LABEL - set a LABEL value
 * @program: pointer to struct program
 * @label: value that will be inserted in a line previously written in the
 *         descriptor buffer.
 */
#define SET_LABEL(program, label)  (label = rta_set_label(program))

/**
 * PATCH_JUMP - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @new_ref: updated value that will be inserted in descriptor buffer at the
 *          specified line; this value is previously obtained using SET_LABEL
 *          macro near the line that will be used as reference (unsigned int).
 *          For JUMP command, the value represents the offset field (in words).
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_JUMP(program, line, new_ref) rta_patch_jmp(program, line, new_ref)

/**
 * PATCH_MOVE - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @new_ref: updated value that will be inserted in descriptor buffer at the
 *          specified line; this value is previously obtained using SET_LABEL
 *          macro near the line that will be used as reference (unsigned int).
 *          For MOVE command, the value represents the offset field (in words).
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_MOVE(program, line, new_ref) \
	rta_patch_move(program, line, new_ref)

/**
 * PATCH_LOAD - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @new_ref: updated value that will be inserted in descriptor buffer at the
 *          specified line; this value is previously obtained using SET_LABEL
 *          macro near the line that will be used as reference (unsigned int).
 *          For LOAD command, the value represents the offset field (in words).
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_LOAD(program, line, new_ref) \
	rta_patch_load(program, line, new_ref)

/**
 * PATCH_STORE - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @new_ref: updated value that will be inserted in descriptor buffer at the
 *          specified line; this value is previously obtained using SET_LABEL
 *          macro near the line that will be used as reference (unsigned int).
 *          For STORE command, the value represents the offset field (in words).
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_STORE(program, line, new_ref) \
	rta_patch_store(program, line, new_ref)

/**
 * PATCH_HDR - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @new_ref: updated value that will be inserted in descriptor buffer at the
 *          specified line; this value is previously obtained using SET_LABEL
 *          macro near the line that will be used as reference (unsigned int).
 *          For HEADER command, the value represents the start index field.
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_HDR(program, line, new_ref) \
	rta_patch_header(program, line, new_ref)

/**
 * PATCH_RAW - Auxiliary command to resolve self referential code
 * @program: buffer to be updated (struct program *)
 * @line: position in descriptor buffer where the update will be done; this
 *        value is previously retained in program flow using a reference near
 *        the sequence to be modified.
 * @mask: mask to be used for applying the new value (unsigned int). The mask
 *        selects which bits from the provided @new_val are taken into
 *        consideration when overwriting the existing value.
 * @new_val: updated value that will be masked using the provided mask value
 *           and inserted in descriptor buffer at the specified line.
 *
 * Return: 0 in case of success, a negative error code if it fails
 */
#define PATCH_RAW(program, line, mask, new_val) \
	rta_patch_raw(program, line, mask, new_val)

#endif /* __RTA_RTA_H__ */
