/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#ifndef CAAM_JR_DESC_H
#define CAAM_JR_DESC_H

#define CMD_HDR_CTYPE_SD		0x16
#define CMD_HDR_CTYPE_JD		0x17

/* The maximum size of a SEC descriptor, in WORDs (32 bits). */
#define MAX_DESC_SIZE_WORDS                     64

/*
 * Macros manipulating descriptors
 */
/* Macro for setting the SD pointer in a JD. Common for all protocols
 * supported by the SEC driver.
 */
#define SEC_JD_SET_SD(descriptor, ptr, len)	   {	  \
	(descriptor)->sd_ptr = (ptr);			       \
	(descriptor)->deschdr.command.jd.shr_desc_len = (len);      \
}

/* Macro for setting a pointer to the job which this descriptor processes.
 * It eases the lookup procedure for identifying the descriptor that has
 * completed.
 */
#define SEC_JD_SET_JOB_PTR(descriptor, ptr) \
	((descriptor)->job_ptr = (ptr))

/* Macro for setting up a JD. The structure of the JD is common across all
 * supported protocols, thus its structure is identical.
 */
#define SEC_JD_INIT(descriptor)	      ({ \
	/* CTYPE = job descriptor			       \
	 * RSMS, DNR = 0
	 * ONE = 1
	 * Start Index = 0
	 * ZRO,TD, MTD = 0
	 * SHR = 1 (there's a shared descriptor referenced
	 *	  by this job descriptor,pointer in next word)
	 * REO = 1 (execute job descr. first, shared descriptor
	 *	  after)
	 * SHARE = DEFER
	 * Descriptor Length = 0 ( to be completed @ runtime ) */ \
	(descriptor)->deschdr.command.word = 0xB0801C0D;	\
	/*
	 * CTYPE = SEQ OUT command * Scater Gather Flag = 0
	 * (can be updated @ runtime) PRE = 0 * EXT = 1
	 * (data length is in next word, following the * command)
	 * RTO = 0 */						\
	(descriptor)->seq_out.command.word = 0xF8400000; /**/	\
	/*
	 * CTYPE = SEQ IN command
	 * Scater Gather Flag = 0 (can be updated @ runtime)
	 * PRE = 0
	 * EXT = 1 ( data length is in next word, following the
	 *	   command)
	 * RTO = 0 */						\
	(descriptor)->seq_in.command.word  = 0xF0400000; /**/	\
	/*
	 * In order to be compatible with QI scenarios, the DPOVRD value
	 * loaded must be formated like this:
	 * DPOVRD_EN (1b) | Res| DPOVRD Value (right aligned). */ \
	(descriptor)->load_dpovrd.command.word = 0x16870004;	\
	/* By default, DPOVRD mechanism is disabled, thus the value to be
	 * LOAD-ed through the above descriptor command will be
	 * 0x0000_0000. */					\
	(descriptor)->dpovrd = 0x00000000;			\
})

/* Macro for setting the pointer to the input buffer in the JD, according to
 * the parameters set by the user in the ::sec_packet_t structure.
 */
#define SEC_JD_SET_IN_PTR(descriptor, phys_addr, offset, length) {     \
	(descriptor)->seq_in_ptr = (phys_addr) + (offset);	      \
	(descriptor)->in_ext_length = (length);			 \
}

/* Macro for setting the pointer to the output buffer in the JD, according to
 * the parameters set by the user in the ::sec_packet_t structure.
 */
#define SEC_JD_SET_OUT_PTR(descriptor, phys_addr, offset, length) {    \
	(descriptor)->seq_out_ptr = (phys_addr) + (offset);	     \
	(descriptor)->out_ext_length = (length);			\
}

/* Macro for setting the Scatter-Gather flag in the SEQ IN command. Used in
 * case the input buffer is split in multiple buffers, according to the user
 * specification.
 */
#define SEC_JD_SET_SG_IN(descriptor) \
	((descriptor)->seq_in.command.field.sgf =  1)

/* Macro for setting the Scatter-Gather flag in the SEQ OUT command. Used in
 * case the output buffer is split in multiple buffers, according to the user
 * specification.
 */
#define SEC_JD_SET_SG_OUT(descriptor) \
	((descriptor)->seq_out.command.field.sgf = 1)

#define SEC_JD_SET_DPOVRD(descriptor) \

/* Macro for retrieving a descriptor's length. Works for both SD and JD. */
#define SEC_GET_DESC_LEN(descriptor)					\
	(((struct descriptor_header_s *)(descriptor))->command.sd.ctype == \
	CMD_HDR_CTYPE_SD ? ((struct descriptor_header_s *) \
	(descriptor))->command.sd.desclen :	\
	((struct descriptor_header_s *)(descriptor))->command.jd.desclen)

/* Helper macro for dumping the hex representation of a descriptor */
#define SEC_DUMP_DESC(descriptor) {					\
	int __i;							\
	CAAM_JR_INFO("Des@ 0x%08x\n", (uint32_t)((uint32_t *)(descriptor)));\
	for (__i = 0;						\
		__i < SEC_GET_DESC_LEN(descriptor);			\
		__i++) {						\
		printf("0x%08x: 0x%08x\n",			\
			(uint32_t)(((uint32_t *)(descriptor)) + __i),	\
			*(((uint32_t *)(descriptor)) + __i));		\
	}								\
}
/* Union describing a descriptor header.
 */
struct descriptor_header_s {
	union {
		uint32_t word;
		struct {
			/* 4  */ unsigned int ctype:5;
			/* 5  */ unsigned int res1:2;
			/* 7  */ unsigned int dnr:1;
			/* 8  */ unsigned int one:1;
			/* 9  */ unsigned int res2:1;
			/* 10 */ unsigned int start_idx:6;
			/* 16 */ unsigned int res3:2;
			/* 18 */ unsigned int cif:1;
			/* 19 */ unsigned int sc:1;
			/* 20 */ unsigned int pd:1;
			/* 21 */ unsigned int res4:1;
			/* 22 */ unsigned int share:2;
			/* 24 */ unsigned int res5:2;
			/* 26 */ unsigned int desclen:6;
		} sd;
		struct {
			/* TODO only below struct members are corrected,
			 * all others also need to be reversed please verify it
			 */
			/* 0 */ unsigned int desclen:7;
			/* 7 */ unsigned int res4:1;
			/* 8 */ unsigned int share:3;
			/* 11 */ unsigned int reo:1;
			/* 12 */ unsigned int shr:1;
			/* 13 */ unsigned int mtd:1;
			/* 14 */ unsigned int td:1;
			/* 15 */ unsigned int zero:1;
			/* 16 */ unsigned int shr_desc_len:6;
			/* 22  */ unsigned int res2:1;
			/* 23  */ unsigned int one:1;
			/* 24  */ unsigned int dnr:1;
			/* 25  */ unsigned int rsms:1;
			/* 26  */ unsigned int res1:1;
			/* 27  */ unsigned int ctype:5;
		} jd;
	} __rte_packed command;
} __rte_packed;

/* Union describing a KEY command in a descriptor.
 */
struct key_command_s {
	union {
		uint32_t word;
		struct {
			unsigned int ctype:5;
			unsigned int cls:2;
			unsigned int sgf:1;
			unsigned int imm:1;
			unsigned int enc:1;
			unsigned int nwb:1;
			unsigned int ekt:1;
			unsigned int kdest:4;
			unsigned int tk:1;
			unsigned int rsvd1:5;
			unsigned int length:10;
		} __rte_packed field;
	} __rte_packed command;
} __rte_packed;

/* Union describing a PROTOCOL command
 * in a descriptor.
 */
struct protocol_operation_command_s {
	union {
		uint32_t word;
		struct {
			unsigned int ctype:5;
			unsigned int optype:3;
			unsigned char protid;
			unsigned short protinfo;
		} __rte_packed field;
	} __rte_packed command;
} __rte_packed;

/* Union describing a SEQIN command in a
 * descriptor.
 */
struct seq_in_command_s {
	union {
		uint32_t word;
		struct {
			unsigned int ctype:5;
			unsigned int res1:1;
			unsigned int inl:1;
			unsigned int sgf:1;
			unsigned int pre:1;
			unsigned int ext:1;
			unsigned int rto:1;
			unsigned int rjd:1;
			unsigned int res2:4;
			unsigned int length:16;
		} field;
	} __rte_packed command;
} __rte_packed;

/* Union describing a SEQOUT command in a
 * descriptor.
 */
struct seq_out_command_s {
	union {
		uint32_t word;
		struct {
			unsigned int ctype:5;
			unsigned int res1:2;
			unsigned int sgf:1;
			unsigned int pre:1;
			unsigned int ext:1;
			unsigned int rto:1;
			unsigned int res2:5;
			unsigned int length:16;
		} field;
	} __rte_packed command;
} __rte_packed;

struct load_command_s {
	union {
		uint32_t word;
		struct {
			unsigned int ctype:5;
			unsigned int class:2;
			unsigned int sgf:1;
			unsigned int imm:1;
			unsigned int dst:7;
			unsigned char offset;
			unsigned char length;
		} fields;
	} __rte_packed command;
} __rte_packed;

/* Structure encompassing a general shared descriptor of maximum
 * size (64 WORDs). Usually, other specific shared descriptor structures
 * will be type-casted to this one
 * this one.
 */
struct sec_sd_t {
	uint32_t rsvd[MAX_DESC_SIZE_WORDS];
} __attribute__((packed, aligned(64)));

/* Structure encompassing a job descriptor which processes
 * a single packet from a context. The job descriptor references
 * a shared descriptor from a SEC context.
 */
struct sec_job_descriptor_t {
	struct descriptor_header_s deschdr;
	dma_addr_t sd_ptr;
	struct seq_out_command_s seq_out;
	dma_addr_t seq_out_ptr;
	uint32_t out_ext_length;
	struct seq_in_command_s seq_in;
	dma_addr_t seq_in_ptr;
	uint32_t in_ext_length;
	struct load_command_s load_dpovrd;
	uint32_t dpovrd;
} __attribute__((packed, aligned(64)));

#endif
