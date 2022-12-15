/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _RTE_OCTEONTX_ZIP_REGS_H_
#define _RTE_OCTEONTX_ZIP_REGS_H_


/**
 * Enumeration zip_cc
 *
 * ZIP compression coding Enumeration
 * Enumerates ZIP_INST_S[CC].
 */
enum zip_cc {
	ZIP_CC_DEFAULT = 0,
	ZIP_CC_DYN_HUFF,
	ZIP_CC_FIXED_HUFF,
	ZIP_CC_LZS
};

/**
 * Register (NCB) zip_vq#_ena
 *
 * ZIP VF Queue Enable Register
 * If a queue is disabled, ZIP CTL stops fetching instructions from the queue.
 */
typedef union {
	uint64_t u;
	struct zip_vqx_ena_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t reserved_1_63         : 63;
		uint64_t ena                   : 1;
#else /* Word 0 - Little Endian */
		uint64_t ena                   : 1;
		uint64_t reserved_1_63         : 63;
#endif /* Word 0 - End */
	} s;
} zip_vqx_ena_t;

/**
 * Register (NCB) zip_vq#_sbuf_addr
 *
 * ZIP VF Queue Starting Buffer Address Registers
 * These registers set the buffer parameters for the instruction queues.
 * When quiescent (i.e.
 * outstanding doorbell count is 0), it is safe to rewrite this register
 * to effectively reset the
 * command buffer state machine.
 * These registers must be programmed after software programs the
 * corresponding ZIP_QUE()_SBUF_CTL.
 */
typedef union {
	uint64_t u;
	struct zip_vqx_sbuf_addr_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t reserved_49_63        : 15;
		uint64_t ptr                   : 42;
		uint64_t off                   : 7;
#else /* Word 0 - Little Endian */
		uint64_t off                   : 7;
		uint64_t ptr                   : 42;
		uint64_t reserved_49_63        : 15;
#endif /* Word 0 - End */
	} s;

	struct zip_vqx_sbuf_addr_s9x {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t reserved_53_63        : 11;
		uint64_t ptr                   : 46;
		uint64_t off                   : 7;
#else /* Word 0 - Little Endian */
		uint64_t off                   : 7;
		uint64_t ptr                   : 46;
		uint64_t reserved_53_63        : 11;
#endif /* Word 0 - End */
	} s9x;
} zip_vqx_sbuf_addr_t;

/**
 * Register (NCB) zip_que#_doorbell
 *
 * ZIP Queue Doorbell Registers
 * Doorbells for the ZIP instruction queues.
 */
typedef union {
	uint64_t u;
	struct zip_quex_doorbell_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t reserved_20_63        : 44;
		uint64_t dbell_cnt             : 20;
#else /* Word 0 - Little Endian */
		uint64_t dbell_cnt             : 20;
		uint64_t reserved_20_63        : 44;
#endif /* Word 0 - End */
	} s;
} zip_quex_doorbell_t;

/**
 * Structure zip_nptr_s
 *
 * ZIP Instruction Next-Chunk-Buffer Pointer (NPTR) Structure
 * This structure is used to chain all the ZIP instruction buffers
 * together. ZIP instruction buffers are managed
 * (allocated and released) by software.
 */
union zip_nptr_s {
	uint64_t u;
	struct zip_nptr_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t addr                  : 64;
#else /* Word 0 - Little Endian */
		uint64_t addr                  : 64;
#endif /* Word 0 - End */
	} s;
};

/**
 * generic ptr address
 */
union zip_zptr_addr_s {
	/** This field can be used to set/clear all bits, or do bitwise
	 * operations over the entire structure.
	 */
	uint64_t u;
	/** generic ptr address */
	struct {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		uint64_t addr : 64;
#else /* Word 0 - Little Endian */
		uint64_t addr : 64;
#endif /* Word 0 - End */
	} s;
};

/**
 * generic ptr ctl
 */
union zip_zptr_ctl_s {
	/** This field can be used to set/clear all bits, or do bitwise
	 * operations over the entire structure.
	 */
	uint64_t u;
	/** generic ptr ctl */
	struct {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 1 - Big Endian */
		uint64_t reserved_112_127      : 16;
		uint64_t length                : 16;
		uint64_t reserved_67_95        : 29;
		uint64_t fw                    : 1;
		uint64_t nc                    : 1;
		uint64_t data_be               : 1;
#else /* Word 1 - Little Endian */
		uint64_t data_be               : 1;
		uint64_t nc                    : 1;
		uint64_t fw                    : 1;
		uint64_t reserved_67_95        : 29;
		uint64_t length                : 16;
		uint64_t reserved_112_127      : 16;
#endif /* Word 1 - End */
	} s;

};

/**
 * Structure zip_inst_s
 *
 * ZIP Instruction Structure
 * Each ZIP instruction has 16 words (they are called IWORD0 to IWORD15
 * within the structure).
 */
union zip_inst_s {
	/** This field can be used to set/clear all bits, or do bitwise
	 * operations over the entire structure.
	 */
	uint64_t u[16];
	/** ZIP Instruction Structure */
	struct zip_inst_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		/** Done interrupt */
		uint64_t doneint               : 1;
		/** reserved */
		uint64_t reserved_56_62        : 7;
		/**  Total output length */
		uint64_t totaloutputlength     : 24;
		/** reserved */
		uint64_t reserved_27_31        : 5;
		/** EXNUM */
		uint64_t exn                   : 3;
		/**  HASH IV */
		uint64_t iv                    : 1;
		/** EXBITS */
		uint64_t exbits                : 7;
		/** Hash more-in-file */
		uint64_t hmif                  : 1;
		/** Hash Algorithm and enable */
		uint64_t halg                  : 3;
		/** Sync flush*/
		uint64_t sf                    : 1;
		/** Compression speed/storage */
		uint64_t ss                    : 2;
		/** Compression coding */
		uint64_t cc                    : 2;
		/** End of input data */
		uint64_t ef                    : 1;
		/** Beginning of file */
		uint64_t bf                    : 1;
		/** Comp/decomp operation */
		uint64_t op                    : 2;
		/** Data scatter */
		uint64_t ds                    : 1;
		/** Data gather */
		uint64_t dg                    : 1;
		/** History gather */
		uint64_t hg                    : 1;
#else /* Word 0 - Little Endian */
		uint64_t hg                    : 1;
		uint64_t dg                    : 1;
		uint64_t ds                    : 1;
		uint64_t op                    : 2;
		uint64_t bf                    : 1;
		uint64_t ef                    : 1;
		uint64_t cc                    : 2;
		uint64_t ss                    : 2;
		uint64_t sf                    : 1;
		uint64_t halg                  : 3;
		uint64_t hmif                  : 1;
		uint64_t exbits                : 7;
		uint64_t iv                    : 1;
		uint64_t exn                   : 3;
		uint64_t reserved_27_31        : 5;
		uint64_t totaloutputlength     : 24;
		uint64_t reserved_56_62        : 7;
		uint64_t doneint               : 1;

#endif /* Word 0 - End */

#if defined(__BIG_ENDIAN_BITFIELD) /* Word 1 - Big Endian */
		/** History length */
		uint64_t historylength         : 16;
		/** reserved */
		uint64_t reserved_96_111       : 16;
		/** adler/crc32 checksum*/
		uint64_t adlercrc32            : 32;
#else /* Word 1 - Little Endian */
		uint64_t adlercrc32            : 32;
		uint64_t reserved_96_111       : 16;
		uint64_t historylength         : 16;
#endif /* Word 1 - End */

#if defined(__BIG_ENDIAN_BITFIELD) /* Word 2 - Big Endian */
		/** Decompression Context Pointer Address */
		union zip_zptr_addr_s  ctx_ptr_addr;
#else /* Word 2 - Little Endian */
		union zip_zptr_addr_s  ctx_ptr_addr;
#endif /* Word 2 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Decompression Context Pointer Control */
		union zip_zptr_ctl_s   ctx_ptr_ctl;
#else /* Word 3 - Little Endian */
		union zip_zptr_ctl_s   ctx_ptr_ctl;
#endif /* Word 3 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Decompression history pointer address */
		union zip_zptr_addr_s  his_ptr_addr;
#else /* Word 4 - Little Endian */
		union zip_zptr_addr_s  his_ptr_addr;
#endif /* Word 4 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Decompression history pointer control */
		union zip_zptr_ctl_s   his_ptr_ctl;
#else /* Word 5 - Little Endian */
		union zip_zptr_ctl_s   his_ptr_ctl;
#endif /* Word 5 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Input and compression history pointer address */
		union zip_zptr_addr_s  inp_ptr_addr;
#else /* Word 6 - Little Endian */
		union zip_zptr_addr_s  inp_ptr_addr;
#endif /* Word 6 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Input and compression history pointer control */
		union zip_zptr_ctl_s   inp_ptr_ctl;
#else /* Word 7 - Little Endian */
		union zip_zptr_ctl_s   inp_ptr_ctl;
#endif /* Word 7 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Output pointer address */
		union zip_zptr_addr_s  out_ptr_addr;
#else /* Word 8 - Little Endian */
		union zip_zptr_addr_s  out_ptr_addr;
#endif /* Word 8 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Output pointer control */
		union zip_zptr_ctl_s   out_ptr_ctl;
#else /* Word 9 - Little Endian */
		union zip_zptr_ctl_s   out_ptr_ctl;
#endif /* Word 9 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Result pointer address */
		union zip_zptr_addr_s  res_ptr_addr;
#else /* Word 10 - Little Endian */
		union zip_zptr_addr_s  res_ptr_addr;
#endif /* Word 10 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** Result pointer control */
		union zip_zptr_ctl_s   res_ptr_ctl;
#else /* Word 11 - Little Endian */
		union zip_zptr_ctl_s   res_ptr_ctl;
#endif /* Word 11 - End */

#if defined(__BIG_ENDIAN_BITFIELD) /* Word 12 - Big Endian */
		/** reserved */
		uint64_t reserved_812_831      : 20;
		/** SSO guest group */
		uint64_t ggrp                  : 10;
		/** SSO tag type */
		uint64_t tt                    : 2;
		/** SSO tag */
		uint64_t tag                   : 32;
#else /* Word 12 - Little Endian */
		uint64_t tag                   : 32;
		uint64_t tt                    : 2;
		uint64_t ggrp                  : 10;
		uint64_t reserved_812_831      : 20;
#endif /* Word 12 - End */

#if defined(__BIG_ENDIAN_BITFIELD) /* Word 13 - Big Endian */
		/** Work queue entry pointer */
		uint64_t wq_ptr                : 64;
#else /* Word 13 - Little Endian */
		uint64_t wq_ptr                : 64;
#endif /* Word 13 - End */

#if defined(__BIG_ENDIAN_BITFIELD)
		/** reserved */
		uint64_t reserved_896_959      : 64;
#else /* Word 14 - Little Endian */
		uint64_t reserved_896_959      : 64;
#endif /* Word 14 - End */
#if defined(__BIG_ENDIAN_BITFIELD)
		/** Hash structure pointer */
		uint64_t hash_ptr              : 64;
#else /* Word 15 - Little Endian */
		uint64_t hash_ptr              : 64;
#endif /* Word 15 - End */
	} /** ZIP 88xx Instruction Structure */zip88xx;

	/** ZIP Instruction Structure */
	struct zip_inst_s_cn83xx {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		/** Done interrupt */
		uint64_t doneint               : 1;
		/** reserved */
		uint64_t reserved_56_62        : 7;
		/**  Total output length */
		uint64_t totaloutputlength     : 24;
		/** reserved */
		uint64_t reserved_27_31        : 5;
		/** EXNUM */
		uint64_t exn                   : 3;
		/**  HASH IV */
		uint64_t iv                    : 1;
		/** EXBITS */
		uint64_t exbits                : 7;
		/** Hash more-in-file */
		uint64_t hmif                  : 1;
		/** Hash Algorithm and enable */
		uint64_t halg                  : 3;
		/** Sync flush*/
		uint64_t sf                    : 1;
		/** Compression speed/storage */
		uint64_t ss                    : 2;
		/** Compression coding */
		uint64_t cc                    : 2;
		/** End of input data */
		uint64_t ef                    : 1;
		/** Beginning of file */
		uint64_t bf                    : 1;
		/** Comp/decomp operation */
		uint64_t op                    : 2;
		/** Data scatter */
		uint64_t ds                    : 1;
		/** Data gather */
		uint64_t dg                    : 1;
		/** History gather */
		uint64_t hg                    : 1;
#else /* Word 0 - Little Endian */
		uint64_t hg                    : 1;
		uint64_t dg                    : 1;
		uint64_t ds                    : 1;
		uint64_t op                    : 2;
		uint64_t bf                    : 1;
		uint64_t ef                    : 1;
		uint64_t cc                    : 2;
		uint64_t ss                    : 2;
		uint64_t sf                    : 1;
		uint64_t halg                  : 3;
		uint64_t hmif                  : 1;
		uint64_t exbits                : 7;
		uint64_t iv                    : 1;
		uint64_t exn                   : 3;
		uint64_t reserved_27_31        : 5;
		uint64_t totaloutputlength     : 24;
		uint64_t reserved_56_62        : 7;
		uint64_t doneint               : 1;
#endif /* Word 0 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 1 - Big Endian */
		/** History length */
		uint64_t historylength         : 16;
		/** reserved */
		uint64_t reserved_96_111       : 16;
		/** adler/crc32 checksum*/
		uint64_t adlercrc32            : 32;
#else /* Word 1 - Little Endian */
		uint64_t adlercrc32            : 32;
		uint64_t reserved_96_111       : 16;
		uint64_t historylength         : 16;
#endif /* Word 1 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 2 - Big Endian */
		/** Decompression Context Pointer Address */
		union zip_zptr_addr_s  ctx_ptr_addr;
#else /* Word 2 - Little Endian */
		union zip_zptr_addr_s  ctx_ptr_addr;
#endif /* Word 2 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 3 - Big Endian */
		/** Decompression Context Pointer Control */
		union zip_zptr_ctl_s   ctx_ptr_ctl;
#else /* Word 3 - Little Endian */
		union zip_zptr_ctl_s   ctx_ptr_ctl;
#endif /* Word 3 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 4 - Big Endian */
		/** Decompression history pointer address */
		union zip_zptr_addr_s  his_ptr_addr;
#else /* Word 4 - Little Endian */
		union zip_zptr_addr_s  his_ptr_addr;
#endif /* Word 4 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 5 - Big Endian */
		/** Decompression history pointer control */
		union zip_zptr_ctl_s   his_ptr_ctl;
#else /* Word 5 - Little Endian */
		union zip_zptr_ctl_s   his_ptr_ctl;
#endif /* Word 5 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 6 - Big Endian */
		/** Input and compression history pointer address */
		union zip_zptr_addr_s  inp_ptr_addr;
#else /* Word 6 - Little Endian */
		union zip_zptr_addr_s  inp_ptr_addr;
#endif /* Word 6 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 7 - Big Endian */
		/** Input and compression history pointer control */
		union zip_zptr_ctl_s   inp_ptr_ctl;
#else /* Word 7 - Little Endian */
		union zip_zptr_ctl_s   inp_ptr_ctl;
#endif /* Word 7 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 8 - Big Endian */
		/** Output pointer address */
		union zip_zptr_addr_s  out_ptr_addr;
#else /* Word 8 - Little Endian */
		union zip_zptr_addr_s  out_ptr_addr;
#endif /* Word 8 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 9 - Big Endian */
		/** Output pointer control */
		union zip_zptr_ctl_s   out_ptr_ctl;
#else /* Word 9 - Little Endian */
		union zip_zptr_ctl_s   out_ptr_ctl;
#endif /* Word 9 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 10 - Big Endian */
		/** Result pointer address */
		union zip_zptr_addr_s  res_ptr_addr;
#else /* Word 10 - Little Endian */
		union zip_zptr_addr_s  res_ptr_addr;
#endif /* Word 10 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 11 - Big Endian */
		/** Result pointer control */
		union zip_zptr_ctl_s   res_ptr_ctl;
#else /* Word 11 - Little Endian */
		union zip_zptr_ctl_s   res_ptr_ctl;
#endif /* Word 11 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 12 - Big Endian */
		/** reserved */
		uint64_t reserved_812_831      : 20;
		/** SSO guest group */
		uint64_t ggrp                  : 10;
		/** SSO tag type */
		uint64_t tt                    : 2;
		/** SSO tag */
		uint64_t tag                   : 32;
#else /* Word 12 - Little Endian */
		uint64_t tag                   : 32;
		uint64_t tt                    : 2;
		uint64_t ggrp                  : 10;
		uint64_t reserved_812_831      : 20;
#endif /* Word 12 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 13 - Big Endian */
		/** Work queue entry pointer */
		uint64_t wq_ptr                : 64;
#else /* Word 13 - Little Endian */
		uint64_t wq_ptr                : 64;
#endif /* Word 13 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 14 - Big Endian */
		/** reserved */
		uint64_t reserved_896_959      : 64;
#else /* Word 14 - Little Endian */
		uint64_t reserved_896_959      : 64;
#endif /* Word 14 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 15 - Big Endian */
		/** Hash structure pointer */
		uint64_t hash_ptr              : 64;
#else /* Word 15 - Little Endian */
		uint64_t hash_ptr              : 64;
#endif /* Word 15 - End */
	} /** ZIP 83xx Instruction Structure */s;
};

/**
 * Structure zip_zres_s
 *
 * ZIP Result Structure
 * The ZIP coprocessor writes the result structure after it completes the
 * invocation. The result structure is exactly 24 bytes, and each invocation
 * of the ZIP coprocessor produces exactly one result structure.
 */
union zip_zres_s {
	/** This field can be used to set/clear all bits, or do bitwise
	 * operations over the entire structure.
	 */
	uint64_t u[8];
	/** ZIP Result Structure */
	struct zip_zres_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		/** crc32 checksum of uncompressed stream */
		uint64_t crc32                 : 32;
		/** adler32 checksum of uncompressed stream*/
		uint64_t adler32               : 32;
#else /* Word 0 - Little Endian */
		uint64_t adler32               : 32;
		uint64_t crc32                 : 32;
#endif /* Word 0 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 1 - Big Endian */
		/** Total numer of Bytes produced in output stream */
		uint64_t totalbyteswritten     : 32;
		/** Total number of bytes processed from the input stream */
		uint64_t totalbytesread        : 32;
#else /* Word 1 - Little Endian */
		uint64_t totalbytesread        : 32;
		uint64_t totalbyteswritten     : 32;
#endif /* Word 1 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 2 - Big Endian */
		/** Total number of compressed input bits
		 * consumed to decompress all blocks in the file
		 */
		uint64_t totalbitsprocessed    : 32;
		/** Done interrupt*/
		uint64_t doneint               : 1;
		/** reserved */
		uint64_t reserved_155_158      : 4;
		/** EXNUM */
		uint64_t exn                   : 3;
		/** reserved */
		uint64_t reserved_151          : 1;
		/** EXBITS */
		uint64_t exbits                : 7;
		/** reserved */
		uint64_t reserved_137_143      : 7;
		/** End of file */
		uint64_t ef                    : 1;
		/** Completion/error code */
		uint64_t compcode              : 8;
#else /* Word 2 - Little Endian */
		uint64_t compcode              : 8;
		uint64_t ef                    : 1;
		uint64_t reserved_137_143      : 7;
		uint64_t exbits                : 7;
		uint64_t reserved_151          : 1;
		uint64_t exn                   : 3;
		uint64_t reserved_155_158      : 4;
		uint64_t doneint               : 1;
		uint64_t totalbitsprocessed    : 32;
#endif /* Word 2 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 3 - Big Endian */
		/** reserved */
		uint64_t reserved_253_255      : 3;
		/** Hash length in bytes */
		uint64_t hshlen                : 61;
#else /* Word 3 - Little Endian */
		uint64_t hshlen                : 61;
		uint64_t reserved_253_255      : 3;
#endif /* Word 3 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 4 - Big Endian */
		/** Double-word 0 of computed hash */
		uint64_t hash0                 : 64;
#else /* Word 4 - Little Endian */
		uint64_t hash0                 : 64;
#endif /* Word 4 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 5 - Big Endian */
		/** Double-word 1 of computed hash */
		uint64_t hash1                 : 64;
#else /* Word 5 - Little Endian */
		uint64_t hash1                 : 64;
#endif /* Word 5 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 6 - Big Endian */
		/** Double-word 2 of computed hash */
		uint64_t hash2                 : 64;
#else /* Word 6 - Little Endian */
		uint64_t hash2                 : 64;
#endif /* Word 6 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 7 - Big Endian */
		/** Double-word 3 of computed hash */
		uint64_t hash3                 : 64;
#else /* Word 7 - Little Endian */
		uint64_t hash3                 : 64;
#endif /* Word 7 - End */
	} /** ZIP Result Structure */s;
};

/**
 * Structure zip_zptr_s
 *
 * ZIP Generic Pointer Structure
 * This structure is the generic format of pointers in ZIP_INST_S.
 */
union zip_zptr_s {
	/** This field can be used to set/clear all bits, or do bitwise
	 * operations over the entire structure.
	 */
	uint64_t u[2];
	/** ZIP Generic Pointer Structure */
	struct zip_zptr_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 0 - Big Endian */
		/** Pointer to Data or scatter-gather list */
		uint64_t addr                  : 64;
#else /* Word 0 - Little Endian */
		uint64_t addr                  : 64;
#endif /* Word 0 - End */
#if defined(__BIG_ENDIAN_BITFIELD) /* Word 1 - Big Endian */
		/** reserved */
		uint64_t reserved_112_127      : 16;
		/** Length of Data or scatter-gather list*/
		uint64_t length                : 16;
		/** reserved */
		uint64_t reserved_67_95        : 29;
		/** Full-block write */
		uint64_t fw                    : 1;
		/** No cache allocation */
		uint64_t nc                    : 1;
		/** reserved */
		uint64_t data_be               : 1;
#else /* Word 1 - Little Endian */
		uint64_t data_be               : 1;
		uint64_t nc                    : 1;
		uint64_t fw                    : 1;
		uint64_t reserved_67_95        : 29;
		uint64_t length                : 16;
		uint64_t reserved_112_127      : 16;
#endif /* Word 1 - End */
	} /** ZIP Generic Pointer Structure */s;
};

/**
 * Enumeration zip_comp_e
 *
 * ZIP Completion Enumeration
 * Enumerates the values of ZIP_ZRES_S[COMPCODE].
 */
#define ZIP_COMP_E_NOTDONE       (0)
#define ZIP_COMP_E_SUCCESS       (1)
#define ZIP_COMP_E_DTRUNC        (2)
#define ZIP_COMP_E_DSTOP         (3)
#define ZIP_COMP_E_ITRUNC        (4)
#define ZIP_COMP_E_RBLOCK        (5)
#define ZIP_COMP_E_NLEN          (6)
#define ZIP_COMP_E_BADCODE       (7)
#define ZIP_COMP_E_BADCODE2      (8)
#define ZIP_COMP_E_ZERO_LEN      (9)
#define ZIP_COMP_E_PARITY        (0xa)
#define ZIP_COMP_E_FATAL         (0xb)
#define ZIP_COMP_E_TIMEOUT       (0xc)
#define ZIP_COMP_E_INSTR_ERR     (0xd)
#define ZIP_COMP_E_HCTX_ERR      (0xe)
#define ZIP_COMP_E_PTR_ERR       (0xf)
#define ZIP_COMP_E_STOP          (3)

/**
 * Enumeration zip_op_e
 *
 * ZIP Operation Enumeration
 * Enumerates ZIP_INST_S[OP].
 * Internal:
 */
#define ZIP_OP_E_DECOMP   (0)
#define ZIP_OP_E_NOCOMP   (1)
#define ZIP_OP_E_COMP     (2)

/**
 * Enumeration zip compression levels
 *
 * ZIP Compression Level Enumeration
 * Enumerates ZIP_INST_S[SS].
 * Internal:
 */
#define ZIP_COMP_E_LEVEL_MAX  (0)
#define ZIP_COMP_E_LEVEL_MED  (1)
#define ZIP_COMP_E_LEVEL_LOW  (2)
#define ZIP_COMP_E_LEVEL_MIN  (3)

#endif	/* _RTE_ZIP_REGS_H_ */
