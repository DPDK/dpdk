/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _VC_5GNR_PMD_H_
#define _VC_5GNR_PMD_H_

#include <stdint.h>
#include <stdbool.h>

/* VC 5GNR FPGA FEC PCI vendor & device IDs. */
#define VC_5GNR_VENDOR_ID	(0x8086)
#define VC_5GNR_PF_DEVICE_ID	(0x0D8F)
#define VC_5GNR_VF_DEVICE_ID	(0x0D90)

#define VC_5GNR_NUM_UL_QUEUES (32)
#define VC_5GNR_NUM_DL_QUEUES (32)
#define VC_5GNR_TOTAL_NUM_QUEUES (VC_5GNR_NUM_UL_QUEUES + VC_5GNR_NUM_DL_QUEUES)

/* VC 5GNR Ring size is in 256 bits (32 bytes) units. */
#define VC_5GNR_RING_DESC_LEN_UNIT_BYTES (32)

/* Align DMA descriptors to 256 bytes - cache-aligned. */
#define VC_5GNR_RING_DESC_ENTRY_LENGTH (8)

/* VC 5GNR FPGA Register mapping on BAR0. */
enum {
	VC_5GNR_CONFIGURATION = 0x00000004, /* len: 2B. */
	VC_5GNR_QUEUE_MAP = 0x00000040	/* len: 256B. */
};

/* VC 5GNR FPGA FEC DESCRIPTOR ERROR. */
enum {
	VC_5GNR_DESC_ERR_NO_ERR = 0x0,
	VC_5GNR_DESC_ERR_K_P_OUT_OF_RANGE = 0x1,
	VC_5GNR_DESC_ERR_Z_C_NOT_LEGAL = 0x2,
	VC_5GNR_DESC_ERR_DESC_OFFSET_ERR = 0x3,
	VC_5GNR_DESC_ERR_DESC_READ_FAIL = 0x8,
	VC_5GNR_DESC_ERR_DESC_READ_TIMEOUT = 0x9,
	VC_5GNR_DESC_ERR_DESC_READ_TLP_POISONED = 0xA,
	VC_5GNR_DESC_ERR_HARQ_INPUT_LEN = 0xB,
	VC_5GNR_DESC_ERR_CB_READ_FAIL = 0xC,
	VC_5GNR_DESC_ERR_CB_READ_TIMEOUT = 0xD,
	VC_5GNR_DESC_ERR_CB_READ_TLP_POISONED = 0xE,
	VC_5GNR_DESC_ERR_HBSTORE_ERR = 0xF
};

/* VC 5GNR FPGA FEC DMA Encoding Request Descriptor. */
struct __rte_packed_begin vc_5gnr_dma_enc_desc {
	uint32_t done:1,
		rsrvd0:7,
		error:4,
		rsrvd1:4,
		num_null:10,
		rsrvd2:6;
	uint32_t ncb:15,
		rsrvd3:1,
		k0:16;
	uint32_t irq_en:1,
		crc_en:1,
		rsrvd4:1,
		qm_idx:3,
		bg_idx:1,
		zc:9,
		desc_idx:10,
		rsrvd5:6;
	uint16_t rm_e;
	uint16_t k_;
	uint32_t out_addr_lw;
	uint32_t out_addr_hi;
	uint32_t in_addr_lw;
	uint32_t in_addr_hi;

	union {
		struct {
			/** Virtual addresses used to retrieve SW context info. */
			void *op_addr;
			/** Stores information about total number of Code Blocks
			 * in currently processed Transport Block.
			 */
			uint64_t cbs_in_op;
		};

		uint8_t sw_ctxt[VC_5GNR_RING_DESC_LEN_UNIT_BYTES *
					(VC_5GNR_RING_DESC_ENTRY_LENGTH - 1)];
	};
} __rte_packed_end;

/* VC 5GNR FPGA DPC FEC DMA Decoding Request Descriptor. */
struct __rte_packed_begin vc_5gnr_dma_dec_desc {
	uint32_t done:1,
		iter:5,
		et_pass:1,
		crcb_pass:1,
		error:4,
		qm_idx:3,
		max_iter:5,
		bg_idx:1,
		rsrvd0:1,
		harqin_en:1,
		zc:9;
	uint32_t hbstroe_offset:22,
		num_null:10;
	uint32_t irq_en:1,
		ncb:15,
		desc_idx:10,
		drop_crc24b:1,
		crc24b_ind:1,
		rv:2,
		et_dis:1,
		rsrvd2:1;
	uint32_t harq_input_length:16,
		rm_e:16; /**< the inbound data byte length. */
	uint32_t out_addr_lw;
	uint32_t out_addr_hi;
	uint32_t in_addr_lw;
	uint32_t in_addr_hi;

	union {
		struct {
			/** Virtual addresses used to retrieve SW context info. */
			void *op_addr;
			/** Stores information about total number of Code Blocks
			 * in currently processed Transport Block.
			 */
			uint8_t cbs_in_op;
		};

		uint32_t sw_ctxt[8 * (VC_5GNR_RING_DESC_ENTRY_LENGTH - 1)];
	};
} __rte_packed_end;

/* Vista Creek 5GNR DMA Descriptor. */
union vc_5gnr_dma_desc {
	struct vc_5gnr_dma_enc_desc enc_req;
	struct vc_5gnr_dma_dec_desc dec_req;
};

#endif /* _VC_5GNR_PMD_H_ */
