/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _AGX100_H_
#define _AGX100_H_

#include <stdint.h>
#include <stdbool.h>

/* AGX100 PCI vendor & device IDs. */
#define AGX100_VENDOR_ID (0x8086)
#define AGX100_PF_DEVICE_ID (0x5799)
#define AGX100_VF_DEVICE_ID (0x579A)

/* Maximum number of possible queues supported on device. */
#define AGX100_MAXIMUM_QUEUES_SUPPORTED (64)

/* AGX100 Ring size is in 256 bits (64 bytes) units. */
#define AGX100_RING_DESC_LEN_UNIT_BYTES (64)

/* Align DMA descriptors to 256 bytes - cache-aligned. */
#define AGX100_RING_DESC_ENTRY_LENGTH (8)

/* AGX100 Register mapping on BAR0. */
enum {
	AGX100_FLR_TIME_OUT = 0x0000000E, /* len: 2B. */
	AGX100_QUEUE_MAP = 0x00000100	/* len: 256B. */
};

/* AGX100 DESCRIPTOR ERROR. */
enum {
	AGX100_DESC_ERR_NO_ERR = 0x00, /**< 4'b0000 2'b00. */
	AGX100_DESC_ERR_E_NOT_LEGAL = 0x11, /**< 4'b0001 2'b01. */
	AGX100_DESC_ERR_K_P_OUT_OF_RANGE = 0x21, /**< 4'b0010 2'b01. */
	AGX100_DESC_ERR_NCB_OUT_OF_RANGE = 0x31, /**< 4'b0011 2'b01. */
	AGX100_DESC_ERR_Z_C_NOT_LEGAL = 0x41, /**< 4'b0100 2'b01. */
	AGX100_DESC_ERR_DESC_INDEX_ERR = 0x03, /**< 4'b0000 2'b11. */
	AGX100_DESC_ERR_HARQ_INPUT_LEN_A = 0x51, /**< 4'b0101 2'b01. */
	AGX100_DESC_ERR_HARQ_INPUT_LEN_B = 0x61, /**< 4'b0110 2'b01. */
	AGX100_DESC_ERR_HBSTORE_OFFSET_ERR = 0x71, /**< 4'b0111 2'b01. */
	AGX100_DESC_ERR_TB_CBG_ERR = 0x81, /**< 4'b1000 2'b01. */
	AGX100_DESC_ERR_CBG_OUT_OF_RANGE = 0x91, /**< 4'b1001 2'b01. */
	AGX100_DESC_ERR_CW_RM_NOT_LEGAL = 0xA1, /**< 4'b1010 2'b01. */
	AGX100_DESC_ERR_UNSUPPORTED_REQ = 0x12, /**< 4'b0000 2'b10. */
	AGX100_DESC_ERR_RESERVED = 0x22, /**< 4'b0010 2'b10. */
	AGX100_DESC_ERR_DESC_ABORT = 0x42, /**< 4'b0100 2'b10. */
	AGX100_DESC_ERR_DESC_READ_TLP_POISONED = 0x82 /**< 4'b1000 2'b10. */
};

/* AGX100 TX Slice Descriptor. */
struct __rte_packed_begin agx100_input_slice_desc {
	uint32_t input_start_addr_lo;
	uint32_t input_start_addr_hi;
	uint32_t input_slice_length:21,
		rsrvd0:9,
		end_of_pkt:1,
		start_of_pkt:1;
	uint32_t input_slice_time_stamp:31,
		input_c:1;
} __rte_packed_end;

/* AGX100 RX Slice Descriptor. */
struct __rte_packed_begin agx100_output_slice_desc {
	uint32_t output_start_addr_lo;
	uint32_t output_start_addr_hi;
	uint32_t output_slice_length:21,
		rsrvd0:9,
		end_of_pkt:1,
		start_of_pkt:1;
	uint32_t output_slice_time_stamp:31,
		output_c:1;
} __rte_packed_end;

/* AGX100 DL DMA Encoding Request Descriptor. */
struct __rte_packed_begin agx100_dma_enc_desc {
	uint32_t done:1, /**< 0: not completed 1: completed. */
		rsrvd0:17,
		error_msg:2,
		error_code:4,
		rsrvd1:8;
	uint32_t ncb:16, /**< Limited circular buffer size. */
		bg_idx:1, /**< Base Graph 0: BG1 1: BG2.*/
		qm_idx:3, /**< 0: BPSK; 1: QPSK; 2: 16QAM; 3: 64QAM; 4: 256QAM. */
		zc:9, /**< Lifting size. */
		rv:2, /**< Redundancy version number. */
		int_en:1; /**< Interrupt enable. */
	uint32_t max_cbg:4, /**< Only valid when workload is TB or CBGs. */
		rsrvd2:4,
		cbgti:8, /**< CBG bitmap. */
		rsrvd3:4,
		cbgs:1, /**< 0: TB or CB 1: CBGs. */
		desc_idx:11; /**< Sequence number of the descriptor. */
	uint32_t ca:10, /**< Code block number with Ea in TB or CBG. */
		c:10, /**< Total code block number in TB or CBG. */
		rsrvd4:2,
		num_null:10; /**< Number of null bits. */
	uint32_t ea:21, /**< Value of E when workload is CB. */
		rsrvd5:11;
	uint32_t eb:21, /**< Only valid when workload is TB or CBGs. */
		rsrvd6:11;
	uint32_t k_:16, /**< Code block length without null bits. */
		rsrvd7:8,
		en_slice_ts:1, /**< Enable slice descriptor timestamp. */
		en_host_ts:1, /**< Enable host descriptor timestamp. */
		en_cb_wr_status:1, /**< Enable code block write back status. */
		en_output_sg:1, /**< Enable RX scatter-gather. */
		en_input_sg:1, /**< Enable TX scatter-gather. */
		tb_cb:1, /**< 2'b10: the descriptor is for a TrBlk.
			   * 2'b00: the descriptor is for a CBlk.
			   * 2'b11 or 01: the descriptor is for a CBGs.
			   */
		crc_en:1, /**< 1: CB CRC enabled 0: CB CRC disabled.
			    * Only valid when workload is CB or CBGs.
			    */
		rsrvd8:1;
	uint32_t rsrvd9;
	union {
		uint32_t input_slice_table_addr_lo; /**<Used when scatter-gather enabled.*/
		uint32_t input_start_addr_lo; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t input_slice_table_addr_hi; /**<Used when scatter-gather enabled.*/
		uint32_t input_start_addr_hi; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t input_slice_num:21, /**< Used when scatter-gather enabled. */
			rsrvd10:11;
		uint32_t input_length:26, /**< Used when scatter-gather disabled. */
			rsrvd11:6;
	};
	union {
		uint32_t output_slice_table_addr_lo; /**< Used when scatter-gather enabled.*/
		uint32_t output_start_addr_lo; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t output_slice_table_addr_hi; /**< Used when scatter-gather enabled.*/
		uint32_t output_start_addr_hi; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t output_slice_num:21, /**< Used when scatter-gather enabled. */
			rsrvd12:11;
		uint32_t output_length:26, /**< Used when scatter-gather disabled. */
			rsrvd13:6;
	};
	uint32_t enqueue_timestamp:31, /**< Time when AGX100 receives descriptor. */
		rsrvd14:1;
	uint32_t completion_timestamp:31, /**< Time when AGX100 completes descriptor. */
		rsrvd15:1;

	union {
		struct {
			/** Virtual addresses used to retrieve SW context info. */
			void *op_addr;
			/** Stores information about total number of Code Blocks
			 * in currently processed Transport Block
			 */
			uint64_t cbs_in_op;
		};

		uint8_t sw_ctxt[AGX100_RING_DESC_LEN_UNIT_BYTES *
					(AGX100_RING_DESC_ENTRY_LENGTH - 1)];
	};
} __rte_packed_end;

/* AGX100 UL DMA Decoding Request Descriptor. */
struct __rte_packed_begin agx100_dma_dec_desc {
	uint32_t done:1, /**< 0: not completed 1: completed. */
		tb_crc_pass:1, /**< 0: doesn't pass 1: pass. */
		cb_crc_all_pass:1, /**< 0: doesn't pass 1: pass. */
		cb_all_et_pass:1, /**< 0: not all decoded 1: all decoded. */
		max_iter_ret:6, /**< Iteration number returned by LDPC decoder. */
		cgb_crc_bitmap:8, /**< Field valid only when workload is TB or CBGs. */
		error_msg:2,
		error_code:4,
		et_dis:1, /**< Disable the early termination feature of LDPC decoder. */
		harq_in_en:1, /**< 0: combine disabled 1: combine enable.*/
		max_iter:6; /**< Maximum value of iteration for decoding CB. */
	uint32_t ncb:16, /**< Limited circular buffer size. */
		bg_idx:1, /**< Base Graph 0: BG1 1: BG2.*/
		qm_idx:3, /**< 0: BPSK; 1: QPSK; 2: 16QAM; 3: 64QAM; 4: 256QAM. */
		zc:9, /**< Lifting size. */
		rv:2, /**< Redundancy version number. */
		int_en:1; /**< Interrupt enable. */
	uint32_t max_cbg:4, /**< Only valid when workload is TB or CBGs. */
		rsrvd0:4,
		cbgti:8, /**< CBG bitmap. */
		cbgfi:1, /**< 0: overwrite HARQ buffer 1: enable HARQ for CBGs. */
		rsrvd1:3,
		cbgs:1, /**< 0: TB or CB 1: CBGs. */
		desc_idx:11; /**< Sequence number of the descriptor. */
	uint32_t ca:10, /**< Code block number with Ea in TB or CBG. */
		c:10, /**< Total code block number in TB or CBG. */
		llr_pckg:1, /**< 0: 8-bit LLR 1: 6-bit LLR packed together. */
		syndrome_check_mode:1, /**<0: full syndrome check 1: 4-layer syndome check.*/
		num_null:10; /**< Number of null bits. */
	uint32_t ea:21, /**< Value of E when workload is CB. */
		rsrvd2:3,
		eba:8; /**< Only valid when workload is TB or CBGs. */
	uint32_t hbstore_offset_out:24, /**< HARQ buffer write address. */
		rsrvd3:8;
	uint32_t hbstore_offset_in:24, /**< HARQ buffer read address. */
		en_slice_ts:1, /**< Enable slice descriptor timestamp. */
		en_host_ts:1, /**< Enable host descriptor timestamp. */
		en_cb_wr_status:1, /**< Enable code block write back status. */
		en_output_sg:1, /**< Enable RX scatter-gather. */
		en_input_sg:1, /**< Enable TX scatter-gather. */
		tb_cb:1, /**< 2'b10: the descriptor is for a TrBlk.
			   * 2'b00: the descriptor is for a CBlk.
			   * 2'b11 or 01: the descriptor is for a CBGs.
			   */
		crc24b_ind:1,  /**< 1: CB includes CRC, need LDPC-V to check the CB CRC.
				 * 0: There is no CB CRC check.
				 * Only valid when workload is CB or CBGs.
				 */
		drop_crc24b:1; /**< 1: CB CRC will be dropped. */
	uint32_t harq_input_length_a: 16, /**< HARQ_input_length for CB. */
		harq_input_length_b:16; /**< Only valid when workload is TB or CBGs. */
	union {
		uint32_t input_slice_table_addr_lo; /**< Used when scatter-gather enabled.*/
		uint32_t input_start_addr_lo; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t input_slice_table_addr_hi; /**< Used when scatter-gather enabled.*/
		uint32_t input_start_addr_hi; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t input_slice_num:21, /**< Used when scatter-gather enabled. */
			rsrvd4:11;
		uint32_t input_length:26, /**< Used when scatter-gather disabled. */
			rsrvd5:6;
	};
	union {
		uint32_t output_slice_table_addr_lo; /**< Used when scatter-gather enabled.*/
		uint32_t output_start_addr_lo; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t output_slice_table_addr_hi; /**< Used when scatter-gather enabled.*/
		uint32_t output_start_addr_hi; /**< Used when scatter-gather disabled. */
	};
	union {
		uint32_t output_slice_num:21, /**< Used when scatter-gather enabled. */
			rsrvd6:11;
		uint32_t output_length:26, /**< Used when scatter-gather disabled. */
			rsrvd7:6;
	};
	uint32_t enqueue_timestamp:31, /**< Time when AGX100 receives descriptor. */
		rsrvd8:1;
	uint32_t completion_timestamp:31, /**< Time when AGX100 completes descriptor. */
		rsrvd9:1;

	union {
		struct {
			/** Virtual addresses used to retrieve SW context info. */
			void *op_addr;
			/** Stores information about total number of Code Blocks
			 * in currently processed Transport Block
			 */
			uint8_t cbs_in_op;
		};

		uint8_t sw_ctxt[AGX100_RING_DESC_LEN_UNIT_BYTES *
					(AGX100_RING_DESC_ENTRY_LENGTH - 1)];
	};
} __rte_packed_end;

/* AGX100 DMA Descriptor. */
union agx100_dma_desc {
	struct agx100_dma_enc_desc enc_req;
	struct agx100_dma_dec_desc dec_req;
};

#endif /* _AGX100_H_ */
