/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _ACC_COMMON_H_
#define _ACC_COMMON_H_

#include <bus_pci_driver.h>
#include "rte_acc_common_cfg.h"

/* Values used in filling in descriptors */
#define ACC_DMA_DESC_TYPE           2
#define ACC_DMA_BLKID_FCW           1
#define ACC_DMA_BLKID_IN            2
#define ACC_DMA_BLKID_OUT_ENC       1
#define ACC_DMA_BLKID_OUT_HARD      1
#define ACC_DMA_BLKID_OUT_SOFT      2
#define ACC_DMA_BLKID_OUT_HARQ      3
#define ACC_DMA_BLKID_IN_HARQ       3
#define ACC_DMA_BLKID_IN_MLD_R      3
#define ACC_DMA_BLKID_DEWIN_IN      3

/* Values used in filling in decode FCWs */
#define ACC_FCW_TD_VER              1
#define ACC_FCW_TD_EXT_COLD_REG_EN  1
#define ACC_FCW_TD_AUTOMAP          0x0f
#define ACC_FCW_TD_RVIDX_0          2
#define ACC_FCW_TD_RVIDX_1          26
#define ACC_FCW_TD_RVIDX_2          50
#define ACC_FCW_TD_RVIDX_3          74

#define ACC_SIZE_64MBYTE            (64*1024*1024)
/* Number of elements in an Info Ring */
#define ACC_INFO_RING_NUM_ENTRIES   1024
/* Number of elements in HARQ layout memory
 * 128M x 32kB = 4GB addressable memory
 */
#define ACC_HARQ_LAYOUT             (128 * 1024 * 1024)
/* Assume offset for HARQ in memory */
#define ACC_HARQ_OFFSET             (32 * 1024)
#define ACC_HARQ_OFFSET_SHIFT       15
#define ACC_HARQ_OFFSET_MASK        0x7ffffff
#define ACC_HARQ_OFFSET_THRESHOLD   1024
/* Mask used to calculate an index in an Info Ring array (not a byte offset) */
#define ACC_INFO_RING_MASK          (ACC_INFO_RING_NUM_ENTRIES-1)

#define MAX_ENQ_BATCH_SIZE              255

/* All ACC100 Registers alignment are 32bits = 4B */
#define ACC_BYTES_IN_WORD                 4
#define ACC_MAX_E_MBUF                64000

#define ACC_VF_OFFSET_QOS   16 /* offset in Memory specific to QoS Mon */
#define ACC_TMPL_PRI_0      0x03020100
#define ACC_TMPL_PRI_1      0x07060504
#define ACC_TMPL_PRI_2      0x0b0a0908
#define ACC_TMPL_PRI_3      0x0f0e0d0c
#define ACC_TMPL_PRI_4      0x13121110
#define ACC_TMPL_PRI_5      0x17161514
#define ACC_TMPL_PRI_6      0x1b1a1918
#define ACC_TMPL_PRI_7      0x1f1e1d1c
#define ACC_QUEUE_ENABLE    0x80000000  /* Bit to mark Queue as Enabled */
#define ACC_FDONE           0x80000000
#define ACC_SDONE           0x40000000

#define ACC_NUM_TMPL       32

#define ACC_ACCMAP_0       0
#define ACC_ACCMAP_1       2
#define ACC_ACCMAP_2       1
#define ACC_ACCMAP_3       3
#define ACC_ACCMAP_4       4
#define ACC_ACCMAP_5       5
#define ACC_PF_VAL         2

/* max number of iterations to allocate memory block for all rings */
#define ACC_SW_RING_MEM_ALLOC_ATTEMPTS 5
#define ACC_MAX_QUEUE_DEPTH            1024
#define ACC_DMA_MAX_NUM_POINTERS       14
#define ACC_DMA_MAX_NUM_POINTERS_IN    7
#define ACC_DMA_DESC_PADDINGS          8
#define ACC_FCW_PADDING                12
#define ACC_DESC_FCW_OFFSET            192
#define ACC_DESC_SIZE                  256
#define ACC_DESC_OFFSET                (ACC_DESC_SIZE / 64)
#define ACC_FCW_TE_BLEN                32
#define ACC_FCW_TD_BLEN                24
#define ACC_FCW_LE_BLEN                32
#define ACC_FCW_LD_BLEN                36
#define ACC_FCW_FFT_BLEN               28
#define ACC_FCW_MLDTS_BLEN             32
#define ACC_5GUL_SIZE_0                16
#define ACC_5GUL_SIZE_1                40
#define ACC_5GUL_OFFSET_0              36
#define ACC_COMPANION_PTRS             8
#define ACC_FCW_VER                    2
#define ACC_MUX_5GDL_DESC              6
#define ACC_CMP_ENC_SIZE               20
#define ACC_CMP_DEC_SIZE               24
#define ACC_ENC_OFFSET                (32)
#define ACC_DEC_OFFSET                (80)
#define ACC_LIMIT_DL_MUX_BITS          534
#define ACC_NUM_QGRPS_PER_WORD         8
#define ACC_MAX_NUM_QGRPS              32
#define ACC_RING_SIZE_GRANULARITY      64
#define ACC_MAX_FCW_SIZE              128
#define ACC_IQ_SIZE                    4

#define ACC_FCW_FFT_BLEN_3             28

/* Constants from K0 computation from 3GPP 38.212 Table 5.4.2.1-2 */
#define ACC_N_ZC_1 66 /* N = 66 Zc for BG 1 */
#define ACC_N_ZC_2 50 /* N = 50 Zc for BG 2 */
#define ACC_K_ZC_1 22 /* K = 22 Zc for BG 1 */
#define ACC_K_ZC_2 10 /* K = 10 Zc for BG 2 */
#define ACC_K0_1_1 17 /* K0 fraction numerator for rv 1 and BG 1 */
#define ACC_K0_1_2 13 /* K0 fraction numerator for rv 1 and BG 2 */
#define ACC_K0_2_1 33 /* K0 fraction numerator for rv 2 and BG 1 */
#define ACC_K0_2_2 25 /* K0 fraction numerator for rv 2 and BG 2 */
#define ACC_K0_3_1 56 /* K0 fraction numerator for rv 3 and BG 1 */
#define ACC_K0_3_2 43 /* K0 fraction numerator for rv 3 and BG 2 */

#define ACC_ENGINE_OFFSET    0x1000
#define ACC_LONG_WAIT        1000
#define ACC_MS_IN_US         (1000)

#define ACC_ALGO_SPA                0
#define ACC_ALGO_MSA                1
#define ACC_HARQ_ALIGN_64B          64
#define ACC_MAX_ZC                  384

/* De-ratematch code rate limitation for recommended operation */
#define ACC_LIM_03 2  /* 0.03 */
#define ACC_LIM_09 6  /* 0.09 */
#define ACC_LIM_14 9  /* 0.14 */
#define ACC_LIM_21 14 /* 0.21 */
#define ACC_LIM_31 20 /* 0.31 */
#define ACC_MAX_E (128 * 1024 - 2)
#define ACC_MAX_CS 12

#define ACC100_VARIANT          0
#define VRB1_VARIANT		2
#define VRB2_VARIANT		3

/* Queue Index Hierarchy */
#define VRB1_GRP_ID_SHIFT    10
#define VRB1_VF_ID_SHIFT     4
#define VRB2_GRP_ID_SHIFT    12
#define VRB2_VF_ID_SHIFT     6

#define ACC_MAX_FFT_WIN      16

/* Helper macro for logging */
#define rte_acc_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, RTE_LOG_NOTICE, fmt "\n", \
		##__VA_ARGS__)

/* ACC100 DMA Descriptor triplet */
struct acc_dma_triplet {
	uint64_t address;
	uint32_t blen:20,
		res0:4,
		last:1,
		dma_ext:1,
		res1:2,
		blkid:4;
} __rte_packed;


/* ACC100 Queue Manager Enqueue PCI Register */
union acc_enqueue_reg_fmt {
	uint32_t val;
	struct {
		uint32_t num_elem:8,
			addr_offset:3,
			rsrvd:1,
			req_elem_addr:20;
	};
};

/* FEC 4G Uplink Frame Control Word */
struct __rte_packed acc_fcw_td {
	uint8_t fcw_ver:4,
		num_maps:4; /* Unused in ACC100 */
	uint8_t filler:6, /* Unused in ACC100 */
		rsrvd0:1,
		bypass_sb_deint:1;
	uint16_t k_pos;
	uint16_t k_neg; /* Unused in ACC100 */
	uint8_t c_neg; /* Unused in ACC100 */
	uint8_t c; /* Unused in ACC100 */
	uint32_t ea; /* Unused in ACC100 */
	uint32_t eb; /* Unused in ACC100 */
	uint8_t cab; /* Unused in ACC100 */
	uint8_t k0_start_col; /* Unused in ACC100 */
	uint8_t rsrvd1;
	uint8_t code_block_mode:1, /* Unused in ACC100 */
		turbo_crc_type:1,
		rsrvd2:3,
		bypass_teq:1, /* Unused in ACC100 */
		soft_output_en:1, /* Unused in ACC100 */
		ext_td_cold_reg_en:1;
	union { /* External Cold register */
		uint32_t ext_td_cold_reg;
		struct {
			uint32_t min_iter:4, /* Unused in ACC100 */
				max_iter:4,
				ext_scale:5, /* Unused in ACC100 */
				rsrvd3:3,
				early_stop_en:1, /* Unused in ACC100 */
				sw_soft_out_dis:1, /* Unused in ACC100 */
				sw_et_cont:1, /* Unused in ACC100 */
				sw_soft_out_saturation:1, /* Unused in ACC100 */
				half_iter_on:1, /* Unused in ACC100 */
				raw_decoder_input_on:1, /* Unused in ACC100 */
				rsrvd4:10;
		};
	};
};

/* FEC 4G Downlink Frame Control Word */
struct __rte_packed acc_fcw_te {
	uint16_t k_neg;
	uint16_t k_pos;
	uint8_t c_neg;
	uint8_t c;
	uint8_t filler;
	uint8_t cab;
	uint32_t ea:17,
		rsrvd0:15;
	uint32_t eb:17,
		rsrvd1:15;
	uint16_t ncb_neg;
	uint16_t ncb_pos;
	uint8_t rv_idx0:2,
		rsrvd2:2,
		rv_idx1:2,
		rsrvd3:2;
	uint8_t bypass_rv_idx0:1,
		bypass_rv_idx1:1,
		bypass_rm:1,
		rsrvd4:5;
	uint8_t rsrvd5:1,
		rsrvd6:3,
		code_block_crc:1,
		rsrvd7:3;
	uint8_t code_block_mode:1,
		rsrvd8:7;
	uint64_t rsrvd9;
};

/* FEC 5GNR Downlink Frame Control Word */
struct __rte_packed acc_fcw_le {
	uint32_t FCWversion:4,
		qm:4,
		nfiller:11,
		BG:1,
		Zc:9,
		res0:3;
	uint32_t ncb:16,
		k0:16;
	uint32_t rm_e:22,
		res1:4,
		crc_select:1,
		res2:1,
		bypass_intlv:1,
		res3:3;
	uint32_t res4_a:12,
		mcb_count:3,
		res4_b:1,
		C:8,
		Cab:8;
	uint32_t rm_e_b:22,
		res5:10;
	uint32_t res6;
	uint32_t res7;
	uint32_t res8;
};

/* FEC 5GNR Uplink Frame Control Word */
struct __rte_packed acc_fcw_ld {
	uint32_t FCWversion:4,
		qm:4,
		nfiller:11,
		BG:1,
		Zc:9,
		cnu_algo:1, /* Not supported in ACC100 */
		synd_precoder:1,
		synd_post:1;
	uint32_t ncb:16,
		k0:16;
	uint32_t rm_e:24,
		hcin_en:1,
		hcout_en:1,
		crc_select:1,
		bypass_dec:1,
		bypass_intlv:1,
		so_en:1,
		so_bypass_rm:1,
		so_bypass_intlv:1;
	uint32_t hcin_offset:16,
		hcin_size0:16;
	uint32_t hcin_size1:16,
		hcin_decomp_mode:3,
		llr_pack_mode:1,
		hcout_comp_mode:3,
		saturate_input:1, /* Not supported in VRB1 */
		dec_convllr:4,
		hcout_convllr:4;
	uint32_t itmax:7,
		itstop:1,
		so_it:7,
		minsum_offset:1,  /* Not supported in VRB1 */
		hcout_offset:16;
	uint32_t hcout_size0:16,
		hcout_size1:16;
	uint32_t gain_i:8,
		gain_h:8,
		negstop_th:16;
	uint32_t negstop_it:7,
		negstop_en:1,
		tb_crc_select:2, /* Not supported in ACC100 */
		dec_llrclip:2,  /* Not supported in VRB1 */
		tb_trailer_size:20; /* Not supported in ACC100 */
};

/* FFT Frame Control Word */
struct __rte_packed acc_fcw_fft {
	uint32_t in_frame_size:16,
		leading_pad_size:16;
	uint32_t out_frame_size:16,
		leading_depad_size:16;
	uint32_t cs_window_sel;
	uint32_t cs_window_sel2:16,
		cs_enable_bmap:16;
	uint32_t num_antennas:8,
		idft_size:8,
		dft_size:8,
		cs_offset:8;
	uint32_t idft_shift:8,
		dft_shift:8,
		cs_multiplier:16;
	uint32_t bypass:2,
		fp16_in:1, /* Not supported in VRB1 */
		fp16_out:1,
		exp_adj:4,
		power_shift:4,
		power_en:1,
		res:19;
};

/* FFT Frame Control Word. */
struct __rte_packed acc_fcw_fft_3 {
	uint32_t in_frame_size:16,
		leading_pad_size:16;
	uint32_t out_frame_size:16,
		leading_depad_size:16;
	uint32_t cs_window_sel;
	uint32_t cs_window_sel2:16,
		cs_enable_bmap:16;
	uint32_t num_antennas:8,
		idft_size:8,
		dft_size:8,
		cs_offset:8;
	uint32_t idft_shift:8,
		dft_shift:8,
		cs_multiplier:16;
	uint32_t bypass:2,
		fp16_in:1,
		fp16_out:1,
		exp_adj:4,
		power_shift:4,
		power_en:1,
		enable_dewin:1,
		freq_resample_mode:2,
		depad_output_size:16;
	uint16_t cs_theta_0[ACC_MAX_CS];
	uint32_t cs_theta_d[ACC_MAX_CS];
	int8_t cs_time_offset[ACC_MAX_CS];
};


/* MLD-TS Frame Control Word */
struct __rte_packed acc_fcw_mldts {
	uint32_t fcw_version:4,
		res0:12,
		nrb:13, /* 1 to 1925 */
		res1:3;
	uint32_t NLayers:2, /* 1: 2L... 3: 4L */
		res2:14,
		Qmod0:2, /* 0: 2...3: 8 */
		res3_0:2,
		Qmod1:2,
		res3_1:2,
		Qmod2:2,
		res3_2:2,
		Qmod3:2,
		res3_3:2;
	uint32_t Rrep:3, /* 0 to 5 */
		res4:1,
		Crep:3, /* 0 to 6 */
		res5:25;
	uint32_t pad0;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	uint32_t pad4;
};

/* DMA Response Descriptor */
union acc_dma_rsp_desc {
	uint32_t val;
	struct {
		uint32_t crc_status:1,
			synd_ok:1,
			dma_err:1,
			neg_stop:1,
			fcw_err:1,
			output_truncate:1,
			input_err:1,
			tsen_pagefault:1,
			iterCountFrac:8,
			iter_cnt:8,
			engine_hung:1,
			core_reset:5,
			sdone:1,
			fdone:1;
		uint32_t add_info_0;
		uint32_t add_info_1;
	};
};

/* DMA Request Descriptor */
struct __rte_packed acc_dma_req_desc {
	union {
		struct{
			uint32_t type:4,
				rsrvd0:26,
				sdone:1,
				fdone:1;
			uint32_t ib_ant_offset:16, /* Not supported in ACC100 */
				res2:12,
				num_ant:4;
			uint32_t ob_ant_offset:16,
				ob_cyc_offset:12,
				num_cs:4;
			uint32_t pass_param:8,
				sdone_enable:1,
				irq_enable:1,
				timeStampEn:1,
				dltb:1, /* Not supported in VRB1 */
				res0:4,
				numCBs:8,
				m2dlen:4,
				d2mlen:4;
		};
		struct{
			uint32_t word0;
			uint32_t word1;
			uint32_t word2;
			uint32_t word3;
		};
	};
	struct acc_dma_triplet data_ptrs[ACC_DMA_MAX_NUM_POINTERS];

	/* Virtual addresses used to retrieve SW context info */
	union {
		void *op_addr;
		uint64_t pad1;  /* pad to 64 bits */
	};
	/*
	 * Stores additional information needed for driver processing:
	 * - last_desc_in_batch - flag used to mark last descriptor (CB)
	 *                        in batch
	 * - cbs_in_tb - stores information about total number of Code Blocks
	 *               in currently processed Transport Block
	 */
	union {
		struct {
			union {
				struct acc_fcw_ld fcw_ld;
				struct acc_fcw_td fcw_td;
				struct acc_fcw_le fcw_le;
				struct acc_fcw_te fcw_te;
				struct acc_fcw_fft fcw_fft;
				struct acc_fcw_mldts fcw_mldts;
				uint32_t pad2[ACC_FCW_PADDING];
			};
			uint32_t last_desc_in_batch :8,
				cbs_in_tb:8,
				pad4 : 16;
		};
		uint64_t pad3[ACC_DMA_DESC_PADDINGS]; /* pad to 64 bits */
	};
};

/* ACC100 DMA Descriptor */
union acc_dma_desc {
	struct acc_dma_req_desc req;
	union acc_dma_rsp_desc rsp;
	uint64_t atom_hdr;
};

/* Union describing Info Ring entry */
union acc_info_ring_data {
	uint32_t val;
	struct {
		union {
			uint16_t detailed_info;
			struct {
				uint16_t aq_id: 4;
				uint16_t qg_id: 4;
				uint16_t vf_id: 6;
				uint16_t reserved: 2;
			};
		};
		uint16_t int_nb: 7;
		uint16_t msi_0: 1;
		uint16_t vf2pf: 6;
		uint16_t loop: 1;
		uint16_t valid: 1;
	};
	struct {
		uint32_t aq_id_vrb2: 6;
		uint32_t qg_id_vrb2: 5;
		uint32_t vf_id_vrb2: 6;
		uint32_t int_nb_vrb2: 6;
		uint32_t msi_0_vrb2: 1;
		uint32_t vf2pf_vrb2: 6;
		uint32_t loop_vrb2: 1;
		uint32_t valid_vrb2: 1;
	};
} __rte_packed;

struct __rte_packed acc_pad_ptr {
	void *op_addr;
	uint64_t pad1;  /* pad to 64 bits */
};

struct __rte_packed acc_ptrs {
	struct acc_pad_ptr ptr[ACC_COMPANION_PTRS];
};

/* Union describing Info Ring entry */
union acc_harq_layout_data {
	uint32_t val;
	struct {
		uint16_t offset;
		uint16_t size0;
	};
} __rte_packed;

/**
 * Structure with details about RTE_BBDEV_EVENT_DEQUEUE event. It's passed to
 * the callback function.
 */
struct acc_deq_intr_details {
	uint16_t queue_id;
};

/* TIP VF2PF Comms */
enum {
	ACC_VF2PF_STATUS_REQUEST = 1,
	ACC_VF2PF_USING_VF = 2,
	ACC_VF2PF_LUT_VER_REQUEST = 3,
	ACC_VF2PF_FFT_WIN_REQUEST = 4,
};


typedef void (*acc10x_fcw_ld_fill_fun_t)(struct rte_bbdev_dec_op *op,
		struct acc_fcw_ld *fcw,
		union acc_harq_layout_data *harq_layout);
typedef uint32_t (*queue_offset_fun_t)(bool pf_device, uint8_t vf_id,
		uint8_t qgrp_id, uint16_t aq_id);

/* Private data structure for each ACC100 device */
struct acc_device {
	void *mmio_base;  /**< Base address of MMIO registers (BAR0) */
	void *sw_rings_base;  /* Base addr of un-aligned memory for sw rings */
	void *sw_rings;  /* 64MBs of 64MB aligned memory for sw rings */
	rte_iova_t sw_rings_iova;  /* IOVA address of sw_rings */
	/* Virtual address of the info memory routed to the this function under
	 * operation, whether it is PF or VF.
	 * HW may DMA information data at this location asynchronously
	 */
	union acc_info_ring_data *info_ring;

	union acc_harq_layout_data *harq_layout;
	/* Virtual Info Ring head */
	uint16_t info_ring_head;
	/* Number of bytes available for each queue in device, depending on
	 * how many queues are enabled with configure()
	 */
	uint32_t sw_ring_size;
	uint32_t ddr_size; /* Size in kB */
	uint32_t *tail_ptrs; /* Base address of response tail pointer buffer */
	rte_iova_t tail_ptr_iova; /* IOVA address of tail pointers */
	/* Max number of entries available for each queue in device, depending
	 * on how many queues are enabled with configure()
	 */
	uint32_t sw_ring_max_depth;
	struct rte_acc_conf acc_conf; /* ACC100 Initial configuration */
	/* Bitmap capturing which Queues have already been assigned */
	uint64_t q_assigned_bit_map[ACC_MAX_NUM_QGRPS];
	bool pf_device; /**< True if this is a PF ACC100 device */
	bool configured; /**< True if this ACC100 device is configured */
	uint16_t device_variant;  /**< Device variant */
	const struct acc_registry_addr *reg_addr;
	acc10x_fcw_ld_fill_fun_t fcw_ld_fill;  /**< 5GUL FCW generation function */
	queue_offset_fun_t queue_offset;  /* Device specific queue offset */
	uint16_t num_qgroups;
	uint16_t num_aqs;
	uint16_t fft_window_width[ACC_MAX_FFT_WIN]; /* FFT windowing size. */
};

/* Structure associated with each queue. */
struct __rte_cache_aligned acc_queue {
	union acc_dma_desc *ring_addr;  /* Virtual address of sw ring */
	rte_iova_t ring_addr_iova;  /* IOVA address of software ring */
	uint32_t sw_ring_head;  /* software ring head */
	uint32_t sw_ring_tail;  /* software ring tail */
	/* software ring size (descriptors, not bytes) */
	uint32_t sw_ring_depth;
	/* mask used to wrap enqueued descriptors on the sw ring */
	uint32_t sw_ring_wrap_mask;
	/* Virtual address of companion ring */
	struct acc_ptrs *companion_ring_addr;
	/* MMIO register used to enqueue descriptors */
	void *mmio_reg_enqueue;
	uint8_t vf_id;  /* VF ID (max = 63) */
	uint8_t qgrp_id;  /* Queue Group ID */
	uint16_t aq_id;  /* Atomic Queue ID */
	uint16_t aq_depth;  /* Depth of atomic queue */
	uint32_t aq_enqueued;  /* Count how many "batches" have been enqueued */
	uint32_t aq_dequeued;  /* Count how many "batches" have been dequeued */
	uint32_t irq_enable;  /* Enable ops dequeue interrupts if set to 1 */
	enum rte_bbdev_op_type op_type;  /* Type of this Queue: TE or TD */
	/* Internal Buffers for loopback input */
	uint8_t *lb_in;
	uint8_t *lb_out;
	uint8_t *fcw_ring;
	rte_iova_t lb_in_addr_iova;
	rte_iova_t lb_out_addr_iova;
	rte_iova_t fcw_ring_addr_iova;
	int8_t *derm_buffer; /* interim buffer for de-rm in SDK */
	struct acc_device *d;
};

/* Write to MMIO register address */
static inline void
mmio_write(void *addr, uint32_t value)
{
	*((volatile uint32_t *)(addr)) = rte_cpu_to_le_32(value);
}

/* Write a register of a ACC100 device */
static inline void
acc_reg_write(struct acc_device *d, uint32_t offset, uint32_t value)
{
	void *reg_addr = RTE_PTR_ADD(d->mmio_base, offset);
	mmio_write(reg_addr, value);
	usleep(ACC_LONG_WAIT);
}

/* Read a register of a ACC100 device */
static inline uint32_t
acc_reg_read(struct acc_device *d, uint32_t offset)
{

	void *reg_addr = RTE_PTR_ADD(d->mmio_base, offset);
	uint32_t ret = *((volatile uint32_t *)(reg_addr));
	return rte_le_to_cpu_32(ret);
}

/* Basic Implementation of Log2 for exact 2^N */
static inline uint32_t
log2_basic(uint32_t value)
{
	return (value == 0) ? 0 : rte_bsf32(value);
}

/* Calculate memory alignment offset assuming alignment is 2^N */
static inline uint32_t
calc_mem_alignment_offset(void *unaligned_virt_mem, uint32_t alignment)
{
	rte_iova_t unaligned_phy_mem = rte_malloc_virt2iova(unaligned_virt_mem);
	return (uint32_t)(alignment -
			(unaligned_phy_mem & (alignment-1)));
}

static void
free_base_addresses(void **base_addrs, int size)
{
	int i;
	for (i = 0; i < size; i++)
		rte_free(base_addrs[i]);
}

/* Read flag value 0/1 from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

static inline char *
mbuf_append(struct rte_mbuf *m_head, struct rte_mbuf *m, uint16_t len)
{
	if (unlikely(len > rte_pktmbuf_tailroom(m)))
		return NULL;

	char *tail = (char *)m->buf_addr + m->data_off + m->data_len;
	m->data_len = (uint16_t)(m->data_len + len);
	m_head->pkt_len  = (m_head->pkt_len + len);
	return tail;
}


static inline uint32_t
get_desc_len(void)
{
	return sizeof(union acc_dma_desc);
}

/* Allocate the 2 * 64MB block for the sw rings */
static inline int
alloc_2x64mb_sw_rings_mem(struct rte_bbdev *dev, struct acc_device *d,
		int socket)
{
	uint32_t sw_ring_size = ACC_SIZE_64MBYTE;
	d->sw_rings_base = rte_zmalloc_socket(dev->device->driver->name,
			2 * sw_ring_size, RTE_CACHE_LINE_SIZE, socket);
	if (d->sw_rings_base == NULL) {
		rte_acc_log(ERR, "Failed to allocate memory for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		return -ENOMEM;
	}
	uint32_t next_64mb_align_offset = calc_mem_alignment_offset(
			d->sw_rings_base, ACC_SIZE_64MBYTE);
	d->sw_rings = RTE_PTR_ADD(d->sw_rings_base, next_64mb_align_offset);
	d->sw_rings_iova = rte_malloc_virt2iova(d->sw_rings_base) +
			next_64mb_align_offset;
	d->sw_ring_size = ACC_MAX_QUEUE_DEPTH * get_desc_len();
	d->sw_ring_max_depth = ACC_MAX_QUEUE_DEPTH;

	return 0;
}

/* Attempt to allocate minimised memory space for sw rings */
static inline void
alloc_sw_rings_min_mem(struct rte_bbdev *dev, struct acc_device *d,
		uint16_t num_queues, int socket)
{
	rte_iova_t sw_rings_base_iova, next_64mb_align_addr_iova;
	uint32_t next_64mb_align_offset;
	rte_iova_t sw_ring_iova_end_addr;
	void *base_addrs[ACC_SW_RING_MEM_ALLOC_ATTEMPTS];
	void *sw_rings_base;
	int i = 0;
	uint32_t q_sw_ring_size = ACC_MAX_QUEUE_DEPTH * get_desc_len();
	uint32_t dev_sw_ring_size = q_sw_ring_size * num_queues;
	/* Free first in case this is a reconfiguration */
	rte_free(d->sw_rings_base);

	/* Find an aligned block of memory to store sw rings */
	while (i < ACC_SW_RING_MEM_ALLOC_ATTEMPTS) {
		/*
		 * sw_ring allocated memory is guaranteed to be aligned to
		 * q_sw_ring_size at the condition that the requested size is
		 * less than the page size
		 */
		sw_rings_base = rte_zmalloc_socket(
				dev->device->driver->name,
				dev_sw_ring_size, q_sw_ring_size, socket);

		if (sw_rings_base == NULL) {
			rte_acc_log(ERR,
					"Failed to allocate memory for %s:%u",
					dev->device->driver->name,
					dev->data->dev_id);
			break;
		}

		sw_rings_base_iova = rte_malloc_virt2iova(sw_rings_base);
		next_64mb_align_offset = calc_mem_alignment_offset(
				sw_rings_base, ACC_SIZE_64MBYTE);
		next_64mb_align_addr_iova = sw_rings_base_iova +
				next_64mb_align_offset;
		sw_ring_iova_end_addr = sw_rings_base_iova + dev_sw_ring_size;

		/* Check if the end of the sw ring memory block is before the
		 * start of next 64MB aligned mem address
		 */
		if (sw_ring_iova_end_addr < next_64mb_align_addr_iova) {
			d->sw_rings_iova = sw_rings_base_iova;
			d->sw_rings = sw_rings_base;
			d->sw_rings_base = sw_rings_base;
			d->sw_ring_size = q_sw_ring_size;
			d->sw_ring_max_depth = ACC_MAX_QUEUE_DEPTH;
			break;
		}
		/* Store the address of the unaligned mem block */
		base_addrs[i] = sw_rings_base;
		i++;
	}

	/* Free all unaligned blocks of mem allocated in the loop */
	free_base_addresses(base_addrs, i);
}

/* Wrapper to provide VF index from ring data. */
static inline uint16_t
vf_from_ring(const union acc_info_ring_data ring_data, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return ring_data.vf_id_vrb2;
	else
		return ring_data.vf_id;
}

/* Wrapper to provide QG index from ring data. */
static inline uint16_t
qg_from_ring(const union acc_info_ring_data ring_data, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return ring_data.qg_id_vrb2;
	else
		return ring_data.qg_id;
}

/* Wrapper to provide AQ index from ring data. */
static inline uint16_t
aq_from_ring(const union acc_info_ring_data ring_data, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return ring_data.aq_id_vrb2;
	else
		return ring_data.aq_id;
}

/* Wrapper to provide int index from ring data. */
static inline uint16_t
int_from_ring(const union acc_info_ring_data ring_data, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return ring_data.int_nb_vrb2;
	else
		return ring_data.int_nb;
}

/* Wrapper to provide queue index from group and aq index. */
static inline int
queue_index(uint16_t group_idx, uint16_t aq_idx, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return (group_idx << VRB2_GRP_ID_SHIFT) + aq_idx;
	else
		return (group_idx << VRB1_GRP_ID_SHIFT) + aq_idx;
}

/* Wrapper to provide queue group from queue index. */
static inline int
qg_from_q(uint32_t q_idx, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return (q_idx >> VRB2_GRP_ID_SHIFT) & 0x1F;
	else
		return (q_idx >> VRB1_GRP_ID_SHIFT) & 0xF;
}

/* Wrapper to provide vf from queue index. */
static inline int32_t
vf_from_q(uint32_t q_idx, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return (q_idx >> VRB2_VF_ID_SHIFT)  & 0x3F;
	else
		return (q_idx >> VRB1_VF_ID_SHIFT)  & 0x3F;
}

/* Wrapper to provide aq index from queue index. */
static inline int32_t
aq_from_q(uint32_t q_idx, uint16_t device_variant)
{
	if (device_variant == VRB2_VARIANT)
		return q_idx & 0x3F;
	else
		return q_idx & 0xF;
}

/* Wrapper to set VF index in ring data. */
static inline int32_t
set_vf_in_ring(volatile union acc_info_ring_data *ring_data,
		uint16_t device_variant, uint16_t value)
{
	if (device_variant == VRB2_VARIANT)
		return ring_data->vf_id_vrb2 = value;
	else
		return ring_data->vf_id = value;
}

/*
 * Find queue_id of a device queue based on details from the Info Ring.
 * If a queue isn't found UINT16_MAX is returned.
 */
static inline uint16_t
get_queue_id_from_ring_info(struct rte_bbdev_data *data, const union acc_info_ring_data ring_data)
{
	uint16_t queue_id;
	struct acc_queue *acc_q;
	struct acc_device *d = data->dev_private;

	for (queue_id = 0; queue_id < data->num_queues; ++queue_id) {
		acc_q = data->queues[queue_id].queue_private;

		if (acc_q != NULL && acc_q->aq_id == aq_from_ring(ring_data, d->device_variant) &&
				acc_q->qgrp_id == qg_from_ring(ring_data, d->device_variant) &&
				acc_q->vf_id == vf_from_ring(ring_data, d->device_variant))
			return queue_id;
	}

	return UINT16_MAX;
}

/* Fill in a frame control word for turbo encoding. */
static inline void
acc_fcw_te_fill(const struct rte_bbdev_enc_op *op, struct acc_fcw_te *fcw)
{
	fcw->code_block_mode = op->turbo_enc.code_block_mode;
	if (fcw->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		fcw->k_neg = op->turbo_enc.tb_params.k_neg;
		fcw->k_pos = op->turbo_enc.tb_params.k_pos;
		fcw->c_neg = op->turbo_enc.tb_params.c_neg;
		fcw->c = op->turbo_enc.tb_params.c;
		fcw->ncb_neg = op->turbo_enc.tb_params.ncb_neg;
		fcw->ncb_pos = op->turbo_enc.tb_params.ncb_pos;

		if (check_bit(op->turbo_enc.op_flags,
				RTE_BBDEV_TURBO_RATE_MATCH)) {
			fcw->bypass_rm = 0;
			fcw->cab = op->turbo_enc.tb_params.cab;
			fcw->ea = op->turbo_enc.tb_params.ea;
			fcw->eb = op->turbo_enc.tb_params.eb;
		} else {
			/* E is set to the encoding output size when RM is
			 * bypassed.
			 */
			fcw->bypass_rm = 1;
			fcw->cab = fcw->c_neg;
			fcw->ea = 3 * fcw->k_neg + 12;
			fcw->eb = 3 * fcw->k_pos + 12;
		}
	} else { /* For CB mode */
		fcw->k_pos = op->turbo_enc.cb_params.k;
		fcw->ncb_pos = op->turbo_enc.cb_params.ncb;

		if (check_bit(op->turbo_enc.op_flags,
				RTE_BBDEV_TURBO_RATE_MATCH)) {
			fcw->bypass_rm = 0;
			fcw->eb = op->turbo_enc.cb_params.e;
		} else {
			/* E is set to the encoding output size when RM is
			 * bypassed.
			 */
			fcw->bypass_rm = 1;
			fcw->eb = 3 * fcw->k_pos + 12;
		}
	}

	fcw->bypass_rv_idx1 = check_bit(op->turbo_enc.op_flags,
			RTE_BBDEV_TURBO_RV_INDEX_BYPASS);
	fcw->code_block_crc = check_bit(op->turbo_enc.op_flags,
			RTE_BBDEV_TURBO_CRC_24B_ATTACH);
	fcw->rv_idx1 = op->turbo_enc.rv_index;
}

/* Compute value of k0.
 * Based on 3GPP 38.212 Table 5.4.2.1-2
 * Starting position of different redundancy versions, k0
 */
static inline uint16_t
get_k0(uint16_t n_cb, uint16_t z_c, uint8_t bg, uint8_t rv_index)
{
	if (rv_index == 0)
		return 0;
	uint16_t n = (bg == 1 ? ACC_N_ZC_1 : ACC_N_ZC_2) * z_c;
	if (n_cb == n) {
		if (rv_index == 1)
			return (bg == 1 ? ACC_K0_1_1 : ACC_K0_1_2) * z_c;
		else if (rv_index == 2)
			return (bg == 1 ? ACC_K0_2_1 : ACC_K0_2_2) * z_c;
		else
			return (bg == 1 ? ACC_K0_3_1 : ACC_K0_3_2) * z_c;
	}
	/* LBRM case - includes a division by N */
	if (unlikely(z_c == 0))
		return 0;
	if (rv_index == 1)
		return (((bg == 1 ? ACC_K0_1_1 : ACC_K0_1_2) * n_cb)
				/ n) * z_c;
	else if (rv_index == 2)
		return (((bg == 1 ? ACC_K0_2_1 : ACC_K0_2_2) * n_cb)
				/ n) * z_c;
	else
		return (((bg == 1 ? ACC_K0_3_1 : ACC_K0_3_2) * n_cb)
				/ n) * z_c;
}

/* Fill in a frame control word for LDPC encoding. */
static inline void
acc_fcw_le_fill(const struct rte_bbdev_enc_op *op,
		struct acc_fcw_le *fcw, int num_cb, uint32_t default_e)
{
	fcw->qm = op->ldpc_enc.q_m;
	fcw->nfiller = op->ldpc_enc.n_filler;
	fcw->BG = (op->ldpc_enc.basegraph - 1);
	fcw->Zc = op->ldpc_enc.z_c;
	fcw->ncb = op->ldpc_enc.n_cb;
	fcw->k0 = get_k0(fcw->ncb, fcw->Zc, op->ldpc_enc.basegraph,
			op->ldpc_enc.rv_index);
	fcw->rm_e = (default_e == 0) ? op->ldpc_enc.cb_params.e : default_e;
	fcw->crc_select = check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24B_ATTACH);
	fcw->bypass_intlv = check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_INTERLEAVER_BYPASS);
	fcw->mcb_count = num_cb;
}

/* Provide the descriptor index on a given queue */
static inline uint16_t
acc_desc_idx(struct acc_queue *q, uint16_t offset)
{
	return (q->sw_ring_head + offset) & q->sw_ring_wrap_mask;
}

/* Provide the descriptor pointer on a given queue */
static inline union acc_dma_desc*
acc_desc(struct acc_queue *q, uint16_t offset)
{
	return q->ring_addr + acc_desc_idx(q, offset);
}

/* Provide the descriptor index for the tail of a given queue */
static inline uint16_t
acc_desc_idx_tail(struct acc_queue *q, uint16_t offset)
{
	return (q->sw_ring_tail + offset) & q->sw_ring_wrap_mask;
}

/* Provide the descriptor tail pointer on a given queue */
static inline union acc_dma_desc*
acc_desc_tail(struct acc_queue *q, uint16_t offset)
{
	return q->ring_addr + acc_desc_idx_tail(q, offset);
}

/* Provide the operation pointer from the tail of a given queue */
static inline void*
acc_op_tail(struct acc_queue *q, uint16_t offset)
{
	return (q->ring_addr + ((q->sw_ring_tail + offset) & q->sw_ring_wrap_mask))->req.op_addr;
}

/* Enqueue a number of operations to HW and update software rings */
static inline void
acc_dma_enqueue(struct acc_queue *q, uint16_t n,
		struct rte_bbdev_stats *queue_stats)
{
	union acc_enqueue_reg_fmt enq_req;
	union acc_dma_desc *desc;
	uint64_t start_time = 0;
	queue_stats->acc_offload_cycles = 0;

	/* Set Sdone and IRQ enable bit on last descriptor. */
	desc = acc_desc(q, n - 1);
	desc->req.sdone_enable = 1;
	desc->req.irq_enable = q->irq_enable;

	enq_req.val = 0;
	/* Setting offset, 100b for 256 DMA Desc */
	enq_req.addr_offset = ACC_DESC_OFFSET;

	/* Split ops into batches */
	do {
		uint16_t enq_batch_size;
		uint64_t offset;
		rte_iova_t req_elem_addr;

		enq_batch_size = RTE_MIN(n, MAX_ENQ_BATCH_SIZE);

		/* Set flag on last descriptor in a batch */
		desc = acc_desc(q, enq_batch_size - 1);
		desc->req.last_desc_in_batch = 1;

		/* Calculate the 1st descriptor's address */
		offset = ((q->sw_ring_head & q->sw_ring_wrap_mask) * sizeof(union acc_dma_desc));
		req_elem_addr = q->ring_addr_iova + offset;

		/* Fill enqueue struct */
		enq_req.num_elem = enq_batch_size;
		/* low 6 bits are not needed */
		enq_req.req_elem_addr = (uint32_t)(req_elem_addr >> 6);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_memdump(stderr, "Req sdone", desc, sizeof(*desc));
#endif
		rte_acc_log(DEBUG, "Enqueue %u reqs (phys %#"PRIx64") to reg %p",
				enq_batch_size,
				req_elem_addr,
				(void *)q->mmio_reg_enqueue);

		rte_wmb();

		/* Start time measurement for enqueue function offload. */
		start_time = rte_rdtsc_precise();

		rte_acc_log(DEBUG, "Debug : MMIO Enqueue");
		mmio_write(q->mmio_reg_enqueue, enq_req.val);

		queue_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;

		q->aq_enqueued++;
		q->sw_ring_head += enq_batch_size;
		n -= enq_batch_size;

	} while (n);


}

/* Convert offset to harq index for harq_layout structure */
static inline uint32_t hq_index(uint32_t offset)
{
	return (offset >> ACC_HARQ_OFFSET_SHIFT) & ACC_HARQ_OFFSET_MASK;
}

/* Calculates number of CBs in processed encoder TB based on 'r' and input
 * length.
 */
static inline uint8_t
get_num_cbs_in_tb_enc(struct rte_bbdev_op_turbo_enc *turbo_enc)
{
	uint8_t c, c_neg, r, crc24_bits = 0;
	uint16_t k, k_neg, k_pos;
	uint8_t cbs_in_tb = 0;
	int32_t length;

	length = turbo_enc->input.length;
	r = turbo_enc->tb_params.r;
	c = turbo_enc->tb_params.c;
	c_neg = turbo_enc->tb_params.c_neg;
	k_neg = turbo_enc->tb_params.k_neg;
	k_pos = turbo_enc->tb_params.k_pos;
	crc24_bits = 0;
	if (check_bit(turbo_enc->op_flags, RTE_BBDEV_TURBO_CRC_24B_ATTACH))
		crc24_bits = 24;
	while (length > 0 && r < c) {
		k = (r < c_neg) ? k_neg : k_pos;
		length -= (k - crc24_bits) >> 3;
		r++;
		cbs_in_tb++;
	}

	return cbs_in_tb;
}

/* Calculates number of CBs in processed decoder TB based on 'r' and input
 * length.
 */
static inline uint16_t
get_num_cbs_in_tb_dec(struct rte_bbdev_op_turbo_dec *turbo_dec)
{
	uint8_t c, c_neg, r = 0;
	uint16_t kw, k, k_neg, k_pos, cbs_in_tb = 0;
	int32_t length;

	length = turbo_dec->input.length;
	r = turbo_dec->tb_params.r;
	c = turbo_dec->tb_params.c;
	c_neg = turbo_dec->tb_params.c_neg;
	k_neg = turbo_dec->tb_params.k_neg;
	k_pos = turbo_dec->tb_params.k_pos;
	while (length > 0 && r < c) {
		k = (r < c_neg) ? k_neg : k_pos;
		kw = RTE_ALIGN_CEIL(k + 4, 32) * 3;
		length -= kw;
		r++;
		cbs_in_tb++;
	}

	return cbs_in_tb;
}

/* Calculates number of CBs in processed decoder TB based on 'r' and input
 * length.
 */
static inline uint16_t
get_num_cbs_in_tb_ldpc_dec(struct rte_bbdev_op_ldpc_dec *ldpc_dec)
{
	uint16_t r, cbs_in_tb = 0;
	int32_t length = ldpc_dec->input.length;
	r = ldpc_dec->tb_params.r;
	while (length > 0 && r < ldpc_dec->tb_params.c) {
		length -=  (r < ldpc_dec->tb_params.cab) ?
				ldpc_dec->tb_params.ea :
				ldpc_dec->tb_params.eb;
		r++;
		cbs_in_tb++;
	}
	return cbs_in_tb;
}

/* Check we can mux encode operations with common FCW */
static inline int16_t
check_mux(struct rte_bbdev_enc_op **ops, uint16_t num) {
	uint16_t i;
	if (num <= 1)
		return 1;
	for (i = 1; i < num; ++i) {
		/* Only mux compatible code blocks */
		if (memcmp((uint8_t *)(&ops[i]->ldpc_enc) + ACC_ENC_OFFSET,
				(uint8_t *)(&ops[0]->ldpc_enc) +
				ACC_ENC_OFFSET,
				ACC_CMP_ENC_SIZE) != 0)
			return i;
	}
	/* Avoid multiplexing small inbound size frames */
	int Kp = (ops[0]->ldpc_enc.basegraph == 1 ? 22 : 10) *
			ops[0]->ldpc_enc.z_c - ops[0]->ldpc_enc.n_filler;
	if (Kp  <= ACC_LIMIT_DL_MUX_BITS)
		return 1;
	return num;
}

/* Check we can mux encode operations with common FCW */
static inline bool
cmp_ldpc_dec_op(struct rte_bbdev_dec_op **ops) {
	/* Only mux compatible code blocks */
	if (memcmp((uint8_t *)(&ops[0]->ldpc_dec) + ACC_DEC_OFFSET,
			(uint8_t *)(&ops[1]->ldpc_dec) +
			ACC_DEC_OFFSET, ACC_CMP_DEC_SIZE) != 0) {
		return false;
	} else
		return true;
}

/**
 * Fills descriptor with data pointers of one block type.
 *
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be encoded. It can be changed
 *   and points to next segment in scatter-gather case.
 * @param offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param cb_len
 *   Length of currently processed Code Block
 * @param seg_total_left
 *   It indicates how many bytes still left in segment (mbuf) for further
 *   processing.
 * @param op_flags
 *   Store information about device capabilities
 * @param next_triplet
 *   Index for VRB1 DMA Descriptor triplet
 * @param scattergather
 *   Flag to support scatter-gather for the mbuf
 *
 * @return
 *   Returns index of next triplet on success, other value if lengths of
 *   pkt and processed cb do not match.
 *
 */
static inline int
acc_dma_fill_blk_type_in(struct acc_dma_req_desc *desc,
		struct rte_mbuf **input, uint32_t *offset, uint32_t cb_len,
		uint32_t *seg_total_left, int next_triplet,
		bool scattergather)
{
	uint32_t part_len;
	struct rte_mbuf *m = *input;
	if (scattergather)
		part_len = (*seg_total_left < cb_len) ?
				*seg_total_left : cb_len;
	else
		part_len = cb_len;
	cb_len -= part_len;
	*seg_total_left -= part_len;

	desc->data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(m, *offset);
	desc->data_ptrs[next_triplet].blen = part_len;
	desc->data_ptrs[next_triplet].blkid = ACC_DMA_BLKID_IN;
	desc->data_ptrs[next_triplet].last = 0;
	desc->data_ptrs[next_triplet].dma_ext = 0;
	*offset += part_len;
	next_triplet++;

	while (cb_len > 0) {
		if (next_triplet < ACC_DMA_MAX_NUM_POINTERS_IN && m->next != NULL) {

			m = m->next;
			*seg_total_left = rte_pktmbuf_data_len(m);
			part_len = (*seg_total_left < cb_len) ?
					*seg_total_left :
					cb_len;
			desc->data_ptrs[next_triplet].address =
					rte_pktmbuf_iova_offset(m, 0);
			desc->data_ptrs[next_triplet].blen = part_len;
			desc->data_ptrs[next_triplet].blkid =
					ACC_DMA_BLKID_IN;
			desc->data_ptrs[next_triplet].last = 0;
			desc->data_ptrs[next_triplet].dma_ext = 0;
			cb_len -= part_len;
			*seg_total_left -= part_len;
			/* Initializing offset for next segment (mbuf) */
			*offset = part_len;
			next_triplet++;
		} else {
			rte_acc_log(ERR,
				"Some data still left for processing: "
				"data_left: %u, next_triplet: %u, next_mbuf: %p",
				cb_len, next_triplet, m->next);
			return -EINVAL;
		}
	}
	/* Storing new mbuf as it could be changed in scatter-gather case*/
	*input = m;

	return next_triplet;
}

/* Fills descriptor with data pointers of one block type.
 * Returns index of next triplet
 */
static inline int
acc_dma_fill_blk_type(struct acc_dma_req_desc *desc,
		struct rte_mbuf *mbuf, uint32_t offset,
		uint32_t len, int next_triplet, int blk_id)
{
	desc->data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(mbuf, offset);
	desc->data_ptrs[next_triplet].blen = len;
	desc->data_ptrs[next_triplet].blkid = blk_id;
	desc->data_ptrs[next_triplet].last = 0;
	desc->data_ptrs[next_triplet].dma_ext = 0;
	next_triplet++;

	return next_triplet;
}

static inline void
acc_header_init(struct acc_dma_req_desc *desc)
{
	desc->word0 = ACC_DMA_DESC_TYPE;
	desc->word1 = 0; /**< Timestamp could be disabled */
	desc->word2 = 0;
	desc->word3 = 0;
	desc->numCBs = 1;
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
/* Check if any input data is unexpectedly left for processing */
static inline int
check_mbuf_total_left(uint32_t mbuf_total_left)
{
	if (mbuf_total_left == 0)
		return 0;
	rte_acc_log(ERR,
		"Some date still left for processing: mbuf_total_left = %u",
		mbuf_total_left);
	return -EINVAL;
}
#endif

static inline int
acc_dma_desc_te_fill(struct rte_bbdev_enc_op *op,
		struct acc_dma_req_desc *desc, struct rte_mbuf **input,
		struct rte_mbuf *output, uint32_t *in_offset,
		uint32_t *out_offset, uint32_t *out_length,
		uint32_t *mbuf_total_left, uint32_t *seg_total_left, uint8_t r)
{
	int next_triplet = 1; /* FCW already done */
	uint32_t e, ea, eb, length;
	uint16_t k, k_neg, k_pos;
	uint8_t cab, c_neg;

	desc->word0 = ACC_DMA_DESC_TYPE;
	desc->word1 = 0; /**< Timestamp could be disabled */
	desc->word2 = 0;
	desc->word3 = 0;
	desc->numCBs = 1;

	if (op->turbo_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		ea = op->turbo_enc.tb_params.ea;
		eb = op->turbo_enc.tb_params.eb;
		cab = op->turbo_enc.tb_params.cab;
		k_neg = op->turbo_enc.tb_params.k_neg;
		k_pos = op->turbo_enc.tb_params.k_pos;
		c_neg = op->turbo_enc.tb_params.c_neg;
		e = (r < cab) ? ea : eb;
		k = (r < c_neg) ? k_neg : k_pos;
	} else {
		e = op->turbo_enc.cb_params.e;
		k = op->turbo_enc.cb_params.k;
	}

	if (check_bit(op->turbo_enc.op_flags, RTE_BBDEV_TURBO_CRC_24B_ATTACH))
		length = (k - 24) >> 3;
	else
		length = k >> 3;

	if (unlikely((*mbuf_total_left == 0) || (*mbuf_total_left < length))) {
		rte_acc_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				*mbuf_total_left, length);
		return -1;
	}

	next_triplet = acc_dma_fill_blk_type_in(desc, input, in_offset,
			length, seg_total_left, next_triplet,
			check_bit(op->turbo_enc.op_flags,
			RTE_BBDEV_TURBO_ENC_SCATTER_GATHER));
	if (unlikely(next_triplet < 0)) {
		rte_acc_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->m2dlen = next_triplet;
	*mbuf_total_left -= length;

	/* Set output length */
	if (check_bit(op->turbo_enc.op_flags, RTE_BBDEV_TURBO_RATE_MATCH))
		/* Integer round up division by 8 */
		*out_length = (e + 7) >> 3;
	else
		*out_length = (k >> 3) * 3 + 2;

	next_triplet = acc_dma_fill_blk_type(desc, output, *out_offset,
			*out_length, next_triplet, ACC_DMA_BLKID_OUT_ENC);
	if (unlikely(next_triplet < 0)) {
		rte_acc_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}
	op->turbo_enc.output.length += *out_length;
	*out_offset += *out_length;
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->d2mlen = next_triplet - desc->m2dlen;

	desc->op_addr = op;

	return 0;
}

static inline int
acc_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev;
	int ret;
	uint8_t dev_id;

	if (pci_dev == NULL)
		return -EINVAL;

	/* Find device */
	bbdev = rte_bbdev_get_named_dev(pci_dev->device.name);
	if (bbdev == NULL) {
		rte_acc_log(CRIT,
				"Couldn't find HW dev \"%s\" to uninitialise it",
				pci_dev->device.name);
		return -ENODEV;
	}
	dev_id = bbdev->data->dev_id;

	/* free device private memory before close */
	rte_free(bbdev->data->dev_private);

	/* Close device */
	ret = rte_bbdev_close(dev_id);
	if (ret < 0)
		rte_acc_log(ERR,
				"Device %i failed to close during uninit: %i",
				dev_id, ret);

	/* release bbdev from library */
	rte_bbdev_release(bbdev);

	return 0;
}

static inline void
acc_enqueue_status(struct rte_bbdev_queue_data *q_data,
		enum rte_bbdev_enqueue_status status)
{
	q_data->enqueue_status = status;
	q_data->queue_stats.enqueue_status_count[status]++;

	rte_acc_log(WARNING, "Enqueue Status: %s %#"PRIx64"",
			rte_bbdev_enqueue_status_str(status),
			q_data->queue_stats.enqueue_status_count[status]);
}

static inline void
acc_enqueue_invalid(struct rte_bbdev_queue_data *q_data)
{
	acc_enqueue_status(q_data, RTE_BBDEV_ENQ_STATUS_INVALID_OP);
}

static inline void
acc_enqueue_ring_full(struct rte_bbdev_queue_data *q_data)
{
	acc_enqueue_status(q_data, RTE_BBDEV_ENQ_STATUS_RING_FULL);
}

static inline void
acc_enqueue_queue_full(struct rte_bbdev_queue_data *q_data)
{
	acc_enqueue_status(q_data, RTE_BBDEV_ENQ_STATUS_QUEUE_FULL);
}

/* Number of available descriptor in ring to enqueue */
static inline uint32_t
acc_ring_avail_enq(struct acc_queue *q)
{
	return (q->sw_ring_depth - 1 + q->sw_ring_tail - q->sw_ring_head) & q->sw_ring_wrap_mask;
}

/* Number of available descriptor in ring to dequeue */
static inline uint32_t
acc_ring_avail_deq(struct acc_queue *q)
{
	return (q->sw_ring_depth + q->sw_ring_head - q->sw_ring_tail) & q->sw_ring_wrap_mask;
}

/* Check room in AQ for the enqueues batches into Qmgr */
static inline int32_t
acc_aq_avail(struct rte_bbdev_queue_data *q_data, uint16_t num_ops)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t aq_avail = q->aq_depth -
			((q->aq_enqueued - q->aq_dequeued +
			ACC_MAX_QUEUE_DEPTH) % ACC_MAX_QUEUE_DEPTH)
			- (num_ops >> 7);
	if (aq_avail <= 0)
		acc_enqueue_queue_full(q_data);
	return aq_avail;
}

/* Calculates number of CBs in processed encoder TB based on 'r' and input
 * length.
 */
static inline uint8_t
get_num_cbs_in_tb_ldpc_enc(struct rte_bbdev_op_ldpc_enc *ldpc_enc)
{
	uint8_t c, r, crc24_bits = 0;
	uint16_t k = (ldpc_enc->basegraph == 1 ? 22 : 10) * ldpc_enc->z_c
		- ldpc_enc->n_filler;
	uint8_t cbs_in_tb = 0;
	int32_t length;

	length = ldpc_enc->input.length;
	r = ldpc_enc->tb_params.r;
	c = ldpc_enc->tb_params.c;
	crc24_bits = 0;
	if (check_bit(ldpc_enc->op_flags, RTE_BBDEV_LDPC_CRC_24B_ATTACH))
		crc24_bits = 24;
	while (length > 0 && r < c) {
		length -= (k - crc24_bits) >> 3;
		r++;
		cbs_in_tb++;
	}
	return cbs_in_tb;
}

static inline void
acc_reg_fast_write(struct acc_device *d, uint32_t offset, uint32_t value)
{
	void *reg_addr = RTE_PTR_ADD(d->mmio_base, offset);
	mmio_write(reg_addr, value);
}

#endif /* _ACC_COMMON_H_ */
