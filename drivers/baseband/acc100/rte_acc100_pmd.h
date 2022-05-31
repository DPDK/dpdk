/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_ACC100_PMD_H_
#define _RTE_ACC100_PMD_H_

#include "acc100_pf_enum.h"
#include "acc100_vf_enum.h"
#include "rte_acc100_cfg.h"

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, acc100_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "acc100_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* ACC100 PF and VF driver names */
#define ACC100PF_DRIVER_NAME           intel_acc100_pf
#define ACC100VF_DRIVER_NAME           intel_acc100_vf

/* ACC100 PCI vendor & device IDs */
#define RTE_ACC100_VENDOR_ID           (0x8086)
#define RTE_ACC100_PF_DEVICE_ID        (0x0d5c)
#define RTE_ACC100_VF_DEVICE_ID        (0x0d5d)

/* Values used in filling in descriptors */
#define ACC100_DMA_DESC_TYPE           2
#define ACC100_DMA_CODE_BLK_MODE       0
#define ACC100_DMA_BLKID_FCW           1
#define ACC100_DMA_BLKID_IN            2
#define ACC100_DMA_BLKID_OUT_ENC       1
#define ACC100_DMA_BLKID_OUT_HARD      1
#define ACC100_DMA_BLKID_OUT_SOFT      2
#define ACC100_DMA_BLKID_OUT_HARQ      3
#define ACC100_DMA_BLKID_IN_HARQ       3

/* Values used in filling in decode FCWs */
#define ACC100_FCW_TD_VER              1
#define ACC100_FCW_TD_EXT_COLD_REG_EN  1
#define ACC100_FCW_TD_AUTOMAP          0x0f
#define ACC100_FCW_TD_RVIDX_0          2
#define ACC100_FCW_TD_RVIDX_1          26
#define ACC100_FCW_TD_RVIDX_2          50
#define ACC100_FCW_TD_RVIDX_3          74

/* Values used in writing to the registers */
#define ACC100_REG_IRQ_EN_ALL          0x1FF83FF  /* Enable all interrupts */

/* ACC100 Specific Dimensioning */
#define ACC100_SIZE_64MBYTE            (64*1024*1024)
/* Number of elements in an Info Ring */
#define ACC100_INFO_RING_NUM_ENTRIES   1024
/* Number of elements in HARQ layout memory */
#define ACC100_HARQ_LAYOUT             (64*1024*1024)
/* Assume offset for HARQ in memory */
#define ACC100_HARQ_OFFSET             (32*1024)
/* Mask used to calculate an index in an Info Ring array (not a byte offset) */
#define ACC100_INFO_RING_MASK          (ACC100_INFO_RING_NUM_ENTRIES-1)
/* Number of Virtual Functions ACC100 supports */
#define ACC100_NUM_VFS                  16
#define ACC100_NUM_QGRPS                8
#define ACC100_NUM_QGRPS_PER_WORD       8
#define ACC100_NUM_AQS                  16
#define MAX_ENQ_BATCH_SIZE              255
/* All ACC100 Registers alignment are 32bits = 4B */
#define ACC100_BYTES_IN_WORD                 4
#define ACC100_MAX_E_MBUF                64000

#define ACC100_GRP_ID_SHIFT    10 /* Queue Index Hierarchy */
#define ACC100_VF_ID_SHIFT     4  /* Queue Index Hierarchy */
#define ACC100_VF_OFFSET_QOS   16 /* offset in Memory specific to QoS Mon */
#define ACC100_TMPL_PRI_0      0x03020100
#define ACC100_TMPL_PRI_1      0x07060504
#define ACC100_TMPL_PRI_2      0x0b0a0908
#define ACC100_TMPL_PRI_3      0x0f0e0d0c
#define ACC100_QUEUE_ENABLE    0x80000000  /* Bit to mark Queue as Enabled */
#define ACC100_WORDS_IN_ARAM_SIZE (128 * 1024 / 4)
#define ACC100_FDONE    0x80000000
#define ACC100_SDONE    0x40000000

#define ACC100_NUM_TMPL       32
/* Mapping of signals for the available engines */
#define ACC100_SIG_UL_5G      0
#define ACC100_SIG_UL_5G_LAST 7
#define ACC100_SIG_DL_5G      13
#define ACC100_SIG_DL_5G_LAST 15
#define ACC100_SIG_UL_4G      16
#define ACC100_SIG_UL_4G_LAST 21
#define ACC100_SIG_DL_4G      27
#define ACC100_SIG_DL_4G_LAST 31
#define ACC100_NUM_ACCS       5
#define ACC100_ACCMAP_0       0
#define ACC100_ACCMAP_1       2
#define ACC100_ACCMAP_2       1
#define ACC100_ACCMAP_3       3
#define ACC100_ACCMAP_4       4
#define ACC100_PF_VAL         2

/* max number of iterations to allocate memory block for all rings */
#define ACC100_SW_RING_MEM_ALLOC_ATTEMPTS 5
#define ACC100_MAX_QUEUE_DEPTH            1024
#define ACC100_DMA_MAX_NUM_POINTERS       14
#define ACC100_DMA_MAX_NUM_POINTERS_IN    7
#define ACC100_DMA_DESC_PADDING           8
#define ACC100_FCW_PADDING                12
#define ACC100_DESC_FCW_OFFSET            192
#define ACC100_DESC_SIZE                  256
#define ACC100_DESC_OFFSET                (ACC100_DESC_SIZE / 64)
#define ACC100_FCW_TE_BLEN                32
#define ACC100_FCW_TD_BLEN                24
#define ACC100_FCW_LE_BLEN                32
#define ACC100_FCW_LD_BLEN                36
#define ACC100_5GUL_SIZE_0                16
#define ACC100_5GUL_SIZE_1                40
#define ACC100_5GUL_OFFSET_0              36

#define ACC100_FCW_VER         2
#define ACC100_MUX_5GDL_DESC   6
#define ACC100_CMP_ENC_SIZE    20
#define ACC100_CMP_DEC_SIZE    24
#define ACC100_ENC_OFFSET     (32)
#define ACC100_DEC_OFFSET     (80)
#define ACC100_EXT_MEM /* Default option with memory external to CPU */
#define ACC100_HARQ_OFFSET_THRESHOLD 1024

/* Constants from K0 computation from 3GPP 38.212 Table 5.4.2.1-2 */
#define ACC100_N_ZC_1 66 /* N = 66 Zc for BG 1 */
#define ACC100_N_ZC_2 50 /* N = 50 Zc for BG 2 */
#define ACC100_K0_1_1 17 /* K0 fraction numerator for rv 1 and BG 1 */
#define ACC100_K0_1_2 13 /* K0 fraction numerator for rv 1 and BG 2 */
#define ACC100_K0_2_1 33 /* K0 fraction numerator for rv 2 and BG 1 */
#define ACC100_K0_2_2 25 /* K0 fraction numerator for rv 2 and BG 2 */
#define ACC100_K0_3_1 56 /* K0 fraction numerator for rv 3 and BG 1 */
#define ACC100_K0_3_2 43 /* K0 fraction numerator for rv 3 and BG 2 */

/* ACC100 Configuration */
#define ACC100_DDR_ECC_ENABLE
#define ACC100_CFG_DMA_ERROR    0x3D7
#define ACC100_CFG_AXI_CACHE    0x11
#define ACC100_CFG_QMGR_HI_P    0x0F0F
#define ACC100_CFG_PCI_AXI      0xC003
#define ACC100_CFG_PCI_BRIDGE   0x40006033
#define ACC100_QUAD_NUMS        4
#define ACC100_LANES_PER_QUAD   4
#define ACC100_PCIE_LANE_OFFSET 0x200
#define ACC100_PCIE_QUAD_OFFSET 0x2000
#define ACC100_PCS_EQ           0x6007
#define ACC100_ADAPT            0x8400
#define ACC100_ENGINE_OFFSET    0x1000
#define ACC100_RESET_HI         0x20100
#define ACC100_RESET_LO         0x20000
#define ACC100_RESET_HARD       0x1FF
#define ACC100_ENGINES_MAX      9
#define ACC100_LONG_WAIT        1000
#define ACC100_GPEX_AXIMAP_NUM  17
#define ACC100_CLOCK_GATING_EN  0x30000
#define ACC100_FABRIC_MODE      0xB
/* DDR Size per VF - 512MB by default
 * Can be increased up to 4 GB with single PF/VF
 */
#define ACC100_HARQ_DDR         (512 * 1)
#define ACC100_PRQ_DDR_VER       0x10092020
#define ACC100_MS_IN_US         (1000)
#define ACC100_DDR_TRAINING_MAX (5000)

/* ACC100 DMA Descriptor triplet */
struct acc100_dma_triplet {
	uint64_t address;
	uint32_t blen:20,
		res0:4,
		last:1,
		dma_ext:1,
		res1:2,
		blkid:4;
} __rte_packed;

/* ACC100 DMA Response Descriptor */
union acc100_dma_rsp_desc {
	uint32_t val;
	struct {
		uint32_t crc_status:1,
			synd_ok:1,
			dma_err:1,
			neg_stop:1,
			fcw_err:1,
			output_err:1,
			input_err:1,
			timestampEn:1,
			iterCountFrac:8,
			iter_cnt:8,
			rsrvd3:6,
			sdone:1,
			fdone:1;
		uint32_t add_info_0;
		uint32_t add_info_1;
	};
};


/* ACC100 Queue Manager Enqueue PCI Register */
union acc100_enqueue_reg_fmt {
	uint32_t val;
	struct {
		uint32_t num_elem:8,
			addr_offset:3,
			rsrvd:1,
			req_elem_addr:20;
	};
};

/* FEC 4G Uplink Frame Control Word */
struct __rte_packed acc100_fcw_td {
	uint8_t fcw_ver:4,
		num_maps:4; /* Unused */
	uint8_t filler:6, /* Unused */
		rsrvd0:1,
		bypass_sb_deint:1;
	uint16_t k_pos;
	uint16_t k_neg; /* Unused */
	uint8_t c_neg; /* Unused */
	uint8_t c; /* Unused */
	uint32_t ea; /* Unused */
	uint32_t eb; /* Unused */
	uint8_t cab; /* Unused */
	uint8_t k0_start_col; /* Unused */
	uint8_t rsrvd1;
	uint8_t code_block_mode:1, /* Unused */
		turbo_crc_type:1,
		rsrvd2:3,
		bypass_teq:1, /* Unused */
		soft_output_en:1, /* Unused */
		ext_td_cold_reg_en:1;
	union { /* External Cold register */
		uint32_t ext_td_cold_reg;
		struct {
			uint32_t min_iter:4, /* Unused */
				max_iter:4,
				ext_scale:5, /* Unused */
				rsrvd3:3,
				early_stop_en:1, /* Unused */
				sw_soft_out_dis:1, /* Unused */
				sw_et_cont:1, /* Unused */
				sw_soft_out_saturation:1, /* Unused */
				half_iter_on:1, /* Unused */
				raw_decoder_input_on:1, /* Unused */
				rsrvd4:10;
		};
	};
};

/* FEC 5GNR Uplink Frame Control Word */
struct __rte_packed acc100_fcw_ld {
	uint32_t FCWversion:4,
		qm:4,
		nfiller:11,
		BG:1,
		Zc:9,
		res0:1,
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
		res2:1,
		dec_convllr:4,
		hcout_convllr:4;
	uint32_t itmax:7,
		itstop:1,
		so_it:7,
		res3:1,
		hcout_offset:16;
	uint32_t hcout_size0:16,
		hcout_size1:16;
	uint32_t gain_i:8,
		gain_h:8,
		negstop_th:16;
	uint32_t negstop_it:7,
		negstop_en:1,
		res4:24;
};

/* FEC 4G Downlink Frame Control Word */
struct __rte_packed acc100_fcw_te {
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
struct __rte_packed acc100_fcw_le {
	uint32_t FCWversion:4,
		qm:4,
		nfiller:11,
		BG:1,
		Zc:9,
		res0:3;
	uint32_t ncb:16,
		k0:16;
	uint32_t rm_e:24,
		res1:2,
		crc_select:1,
		res2:1,
		bypass_intlv:1,
		res3:3;
	uint32_t res4_a:12,
		mcb_count:3,
		res4_b:17;
	uint32_t res5;
	uint32_t res6;
	uint32_t res7;
	uint32_t res8;
};

/* ACC100 DMA Request Descriptor */
struct __rte_packed acc100_dma_req_desc {
	union {
		struct{
			uint32_t type:4,
				rsrvd0:26,
				sdone:1,
				fdone:1;
			uint32_t rsrvd1;
			uint32_t rsrvd2;
			uint32_t pass_param:8,
				sdone_enable:1,
				irq_enable:1,
				timeStampEn:1,
				res0:5,
				numCBs:4,
				res1:4,
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
	struct acc100_dma_triplet data_ptrs[ACC100_DMA_MAX_NUM_POINTERS];

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
				struct acc100_fcw_ld fcw_ld;
				struct acc100_fcw_td fcw_td;
				struct acc100_fcw_le fcw_le;
				struct acc100_fcw_te fcw_te;
				uint32_t pad2[ACC100_FCW_PADDING];
			};
			uint32_t last_desc_in_batch :8,
				cbs_in_tb:8,
				pad4 : 16;
		};
		uint64_t pad3[ACC100_DMA_DESC_PADDING]; /* pad to 64 bits */
	};
};

/* ACC100 DMA Descriptor */
union acc100_dma_desc {
	struct acc100_dma_req_desc req;
	union acc100_dma_rsp_desc rsp;
	uint64_t atom_hdr;
};


/* Union describing Info Ring entry */
union acc100_harq_layout_data {
	uint32_t val;
	struct {
		uint16_t offset;
		uint16_t size0;
	};
} __rte_packed;


/* Union describing Info Ring entry */
union acc100_info_ring_data {
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
} __rte_packed;

struct acc100_registry_addr {
	unsigned int dma_ring_dl5g_hi;
	unsigned int dma_ring_dl5g_lo;
	unsigned int dma_ring_ul5g_hi;
	unsigned int dma_ring_ul5g_lo;
	unsigned int dma_ring_dl4g_hi;
	unsigned int dma_ring_dl4g_lo;
	unsigned int dma_ring_ul4g_hi;
	unsigned int dma_ring_ul4g_lo;
	unsigned int ring_size;
	unsigned int info_ring_hi;
	unsigned int info_ring_lo;
	unsigned int info_ring_en;
	unsigned int info_ring_ptr;
	unsigned int tail_ptrs_dl5g_hi;
	unsigned int tail_ptrs_dl5g_lo;
	unsigned int tail_ptrs_ul5g_hi;
	unsigned int tail_ptrs_ul5g_lo;
	unsigned int tail_ptrs_dl4g_hi;
	unsigned int tail_ptrs_dl4g_lo;
	unsigned int tail_ptrs_ul4g_hi;
	unsigned int tail_ptrs_ul4g_lo;
	unsigned int depth_log0_offset;
	unsigned int depth_log1_offset;
	unsigned int qman_group_func;
	unsigned int ddr_range;
};

/* Structure holding registry addresses for PF */
static const struct acc100_registry_addr pf_reg_addr = {
	.dma_ring_dl5g_hi = HWPfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWPfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWPfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWPfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWPfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWPfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWPfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWPfDmaFec4GulDescBaseLoRegVf,
	.ring_size = HWPfQmgrRingSizeVf,
	.info_ring_hi = HWPfHiInfoRingBaseHiRegPf,
	.info_ring_lo = HWPfHiInfoRingBaseLoRegPf,
	.info_ring_en = HWPfHiInfoRingIntWrEnRegPf,
	.info_ring_ptr = HWPfHiInfoRingPointerRegPf,
	.tail_ptrs_dl5g_hi = HWPfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = HWPfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = HWPfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = HWPfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = HWPfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = HWPfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = HWPfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = HWPfDmaFec4GulRespPtrLoRegVf,
	.depth_log0_offset = HWPfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWPfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWPfQmgrGrpFunction0,
	.ddr_range = HWPfDmaVfDdrBaseRw,
};

/* Structure holding registry addresses for VF */
static const struct acc100_registry_addr vf_reg_addr = {
	.dma_ring_dl5g_hi = HWVfDmaFec5GdlDescBaseHiRegVf,
	.dma_ring_dl5g_lo = HWVfDmaFec5GdlDescBaseLoRegVf,
	.dma_ring_ul5g_hi = HWVfDmaFec5GulDescBaseHiRegVf,
	.dma_ring_ul5g_lo = HWVfDmaFec5GulDescBaseLoRegVf,
	.dma_ring_dl4g_hi = HWVfDmaFec4GdlDescBaseHiRegVf,
	.dma_ring_dl4g_lo = HWVfDmaFec4GdlDescBaseLoRegVf,
	.dma_ring_ul4g_hi = HWVfDmaFec4GulDescBaseHiRegVf,
	.dma_ring_ul4g_lo = HWVfDmaFec4GulDescBaseLoRegVf,
	.ring_size = HWVfQmgrRingSizeVf,
	.info_ring_hi = HWVfHiInfoRingBaseHiVf,
	.info_ring_lo = HWVfHiInfoRingBaseLoVf,
	.info_ring_en = HWVfHiInfoRingIntWrEnVf,
	.info_ring_ptr = HWVfHiInfoRingPointerVf,
	.tail_ptrs_dl5g_hi = HWVfDmaFec5GdlRespPtrHiRegVf,
	.tail_ptrs_dl5g_lo = HWVfDmaFec5GdlRespPtrLoRegVf,
	.tail_ptrs_ul5g_hi = HWVfDmaFec5GulRespPtrHiRegVf,
	.tail_ptrs_ul5g_lo = HWVfDmaFec5GulRespPtrLoRegVf,
	.tail_ptrs_dl4g_hi = HWVfDmaFec4GdlRespPtrHiRegVf,
	.tail_ptrs_dl4g_lo = HWVfDmaFec4GdlRespPtrLoRegVf,
	.tail_ptrs_ul4g_hi = HWVfDmaFec4GulRespPtrHiRegVf,
	.tail_ptrs_ul4g_lo = HWVfDmaFec4GulRespPtrLoRegVf,
	.depth_log0_offset = HWVfQmgrGrpDepthLog20Vf,
	.depth_log1_offset = HWVfQmgrGrpDepthLog21Vf,
	.qman_group_func = HWVfQmgrGrpFunction0Vf,
	.ddr_range = HWVfDmaDdrBaseRangeRoVf,
};

/* Structure associated with each queue. */
struct __rte_cache_aligned acc100_queue {
	union acc100_dma_desc *ring_addr;  /* Virtual address of sw ring */
	rte_iova_t ring_addr_iova;  /* IOVA address of software ring */
	uint32_t sw_ring_head;  /* software ring head */
	uint32_t sw_ring_tail;  /* software ring tail */
	/* software ring size (descriptors, not bytes) */
	uint32_t sw_ring_depth;
	/* mask used to wrap enqueued descriptors on the sw ring */
	uint32_t sw_ring_wrap_mask;
	/* MMIO register used to enqueue descriptors */
	void *mmio_reg_enqueue;
	uint8_t vf_id;  /* VF ID (max = 63) */
	uint8_t qgrp_id;  /* Queue Group ID */
	uint16_t aq_id;  /* Atomic Queue ID */
	uint16_t aq_depth;  /* Depth of atomic queue */
	uint32_t aq_enqueued;  /* Count how many "batches" have been enqueued */
	uint32_t aq_dequeued;  /* Count how many "batches" have been dequeued */
	uint32_t irq_enable;  /* Enable ops dequeue interrupts if set to 1 */
	struct rte_mempool *fcw_mempool;  /* FCW mempool */
	enum rte_bbdev_op_type op_type;  /* Type of this Queue: TE or TD */
	/* Internal Buffers for loopback input */
	uint8_t *lb_in;
	uint8_t *lb_out;
	rte_iova_t lb_in_addr_iova;
	rte_iova_t lb_out_addr_iova;
	struct acc100_device *d;
};

/* Private data structure for each ACC100 device */
struct acc100_device {
	void *mmio_base;  /**< Base address of MMIO registers (BAR0) */
	void *sw_rings_base;  /* Base addr of un-aligned memory for sw rings */
	void *sw_rings;  /* 64MBs of 64MB aligned memory for sw rings */
	rte_iova_t sw_rings_iova;  /* IOVA address of sw_rings */
	/* Virtual address of the info memory routed to the this function under
	 * operation, whether it is PF or VF.
	 * HW may DMA information data at this location asynchronously
	 */
	union acc100_info_ring_data *info_ring;

	union acc100_harq_layout_data *harq_layout;
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
	struct rte_acc100_conf acc100_conf; /* ACC100 Initial configuration */
	/* Bitmap capturing which Queues have already been assigned */
	uint16_t q_assigned_bit_map[ACC100_NUM_QGRPS];
	bool pf_device; /**< True if this is a PF ACC100 device */
	bool configured; /**< True if this ACC100 device is configured */
};

/**
 * Structure with details about RTE_BBDEV_EVENT_DEQUEUE event. It's passed to
 * the callback function.
 */
struct acc100_deq_intr_details {
	uint16_t queue_id;
};

#endif /* _RTE_ACC100_PMD_H_ */
