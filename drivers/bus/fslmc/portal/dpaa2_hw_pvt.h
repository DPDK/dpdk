/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2024 NXP
 *
 */

#ifndef _DPAA2_HW_PVT_H_
#define _DPAA2_HW_PVT_H_

#include <rte_compat.h>
#include <rte_eventdev.h>
#include <dpaax_iova_table.h>

#include <mc/fsl_mc_sys.h>
#include <fsl_qbman_portal.h>
#include <bus_fslmc_driver.h>

#ifndef false
#define false      0
#endif
#ifndef true
#define true       1
#endif
#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#ifndef VLAN_TAG_SIZE
#define VLAN_TAG_SIZE   4 /** < Vlan Header Length */
#endif

/* Maximum number of slots available in TX ring */
#define MAX_TX_RING_SLOTS			32
#define MAX_EQ_RESP_ENTRIES			(MAX_TX_RING_SLOTS + 1)

/* Maximum number of slots available in RX ring */
#define DPAA2_EQCR_RING_SIZE		8
/* Maximum number of slots available in RX ring on LX2 */
#define DPAA2_LX2_EQCR_RING_SIZE	32

/* Maximum number of slots available in RX ring */
#define DPAA2_DQRR_RING_SIZE		16
/* Maximum number of slots available in RX ring on LX2 */
#define DPAA2_LX2_DQRR_RING_SIZE	32

/* EQCR shift to get EQCR size (2 >> 3) = 8 for LS2/LS2 */
#define DPAA2_EQCR_SHIFT		3
/* EQCR shift to get EQCR size for LX2 (2 >> 5) = 32 for LX2 */
#define DPAA2_LX2_EQCR_SHIFT		5

/* Flag to determine an ordered queue mbuf */
#define DPAA2_ENQUEUE_FLAG_ORP		(1ULL << 30)
/* ORP ID shift and mask */
#define DPAA2_EQCR_OPRID_SHIFT		16
#define DPAA2_EQCR_OPRID_MASK		0x3FFF0000
/* Sequence number shift and mask */
#define DPAA2_EQCR_SEQNUM_SHIFT		0
#define DPAA2_EQCR_SEQNUM_MASK		0x0000FFFF

#define DPAA2_SWP_CENA_REGION		0
#define DPAA2_SWP_CINH_REGION		1
#define DPAA2_SWP_CENA_MEM_REGION	2

#define DPAA2_MAX_TX_RETRY_COUNT	10000

#define MC_PORTAL_INDEX		0
#define NUM_DPIO_REGIONS	2
#define NUM_DQS_PER_QUEUE       2

/* Maximum release/acquire from QBMAN */
#define DPAA2_MBUF_MAX_ACQ_REL	7

#define DPAA2_MEMPOOL_OPS_NAME		"dpaa2"

#define MAX_BPID 256
#define DPAA2_MBUF_HW_ANNOTATION	64
#define DPAA2_FD_PTA_SIZE		0

/* we will re-use the HEADROOM for annotation in RX */
#define DPAA2_HW_BUF_RESERVE	0
#define DPAA2_PACKET_LAYOUT_ALIGN	64 /*changing from 256 */

#define DPAA2_DPCI_MAX_QUEUES 2
#define DPAA2_INVALID_FLOW_ID 0xffff
#define DPAA2_INVALID_CGID 0xff

#define SEC_FLC_DHR_OUTBOUND	(-114)
#define SEC_FLC_DHR_INBOUND	0

/** Consider aligning with 8 bytes to multiply point size with 2.*/
#define DPAA2_SEC_SIMPLE_FD_OB_MIN \
	((-SEC_FLC_DHR_OUTBOUND) + sizeof(void *) * 2)

#define DPAA2_SEC_SIMPLE_FD_IB_MIN \
	((-SEC_FLC_DHR_INBOUND) + sizeof(void *) * 2)

struct dpaa2_queue;

struct eqresp_metadata {
	struct dpaa2_queue *dpaa2_q;
	struct rte_mempool *mp;
};

#define DPAA2_PORTAL_DEQUEUE_DEPTH	32
struct dpaa2_portal_dqrr {
	struct rte_mbuf *mbuf[DPAA2_PORTAL_DEQUEUE_DEPTH];
	uint64_t dqrr_held;
	uint8_t dqrr_size;
};

struct dpaa2_dpio_dev {
	TAILQ_ENTRY(dpaa2_dpio_dev) next;
		/**< Pointer to Next device instance */
	uint16_t index; /**< Index of a instance in the list */
	rte_atomic16_t ref_count;
		/**< How many thread contexts are sharing this.*/
	uint16_t eqresp_ci;
	uint16_t eqresp_pi;
	struct qbman_result *eqresp;
	struct eqresp_metadata *eqresp_meta;
	struct fsl_mc_io *dpio; /** handle to DPIO portal object */
	uint16_t token;
	struct qbman_swp *sw_portal; /** SW portal object */
	const struct qbman_result *dqrr[4];
		/**< DQRR Entry for this SW portal */
	void *mc_portal; /**< MC Portal for configuring this device */
	uintptr_t qbman_portal_ce_paddr;
		/**< Physical address of Cache Enabled Area */
	uintptr_t ce_size; /**< Size of the CE region */
	uintptr_t qbman_portal_ci_paddr;
		/**< Physical address of Cache Inhibit Area */
	uintptr_t ci_size; /**< Size of the CI region */
	struct rte_intr_handle *intr_handle; /* Interrupt related info */
	int32_t	epoll_fd; /**< File descriptor created for interrupt polling */
	int32_t hw_id; /**< An unique ID of this DPIO device instance */
	struct dpaa2_portal_dqrr dpaa2_held_bufs;
};

struct dpaa2_dpbp_dev {
	TAILQ_ENTRY(dpaa2_dpbp_dev) next;
		/**< Pointer to Next device instance */
	struct fsl_mc_io dpbp;  /** handle to DPBP portal object */
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpbp_id; /*HW ID for DPBP object */
};

struct queue_storage_info_t {
	struct qbman_result *dq_storage[NUM_DQS_PER_QUEUE];
	struct qbman_result *active_dqs;
	uint8_t active_dpio_id;
	uint8_t toggle;
	uint8_t last_num_pkts;
};

struct dpaa2_queue;

typedef void (dpaa2_queue_cb_dqrr_t)(struct qbman_swp *swp,
		const struct qbman_fd *fd,
		const struct qbman_result *dq,
		struct dpaa2_queue *rxq,
		struct rte_event *ev);

typedef void (dpaa2_queue_cb_eqresp_free_t)(uint16_t eqresp_ci,
					struct dpaa2_queue *dpaa2_q);

struct __rte_cache_aligned dpaa2_queue {
	struct rte_mempool *mb_pool; /**< mbuf pool to populate RX ring. */
	union {
		struct rte_eth_dev_data *eth_data;
		struct rte_cryptodev_data *crypto_data;
	};
	uint32_t fqid;		/*!< Unique ID of this queue */
	uint16_t flow_id;	/*!< To be used by DPAA2 framework */
	uint8_t tc_index;	/*!< traffic class identifier */
	uint8_t cgid;		/*! < Congestion Group id for this queue */
	uint64_t rx_pkts;
	uint64_t tx_pkts;
	uint64_t err_pkts;
	union {
		/**Ingress*/
		struct queue_storage_info_t *q_storage[RTE_MAX_LCORE];
		/**Egress*/
		struct qbman_result *cscn;
	};
	struct rte_event ev;
	dpaa2_queue_cb_dqrr_t *cb;
	dpaa2_queue_cb_eqresp_free_t *cb_eqresp_free;
	struct dpaa2_bp_info *bp_array;
	/*to store tx_conf_queue corresponding to tx_queue*/
	struct dpaa2_queue *tx_conf_queue;
	int32_t eventfd;	/*!< Event Fd of this queue */
	uint16_t nb_desc;
	uint16_t tm_sw_td;	/*!< TM software taildrop */
	uint64_t offloads;
	uint64_t lpbk_cntx;
	uint8_t data_stashing_off;
};

struct swp_active_dqs {
	struct qbman_result *global_active_dqs;
	uint64_t reserved[7];
};

#define dpaa2_queue_storage_alloc(q, num) \
({ \
	int ret = 0, i; \
	\
	for (i = 0; i < (num); i++) { \
		(q)->q_storage[i] = rte_zmalloc(NULL, \
			sizeof(struct queue_storage_info_t), \
			RTE_CACHE_LINE_SIZE); \
		if (!(q)->q_storage[i]) { \
			ret = -ENOBUFS; \
			break; \
		} \
		ret = dpaa2_alloc_dq_storage((q)->q_storage[i]); \
		if (ret) \
			break; \
	} \
	ret; \
})

#define dpaa2_queue_storage_free(q, num) \
({ \
	int i; \
	\
	for (i = 0; i < (num); i++) { \
		if ((q)->q_storage[i]) { \
			dpaa2_free_dq_storage((q)->q_storage[i]); \
			rte_free((q)->q_storage[i]); \
			(q)->q_storage[i] = NULL; \
		} \
	} \
})

#define NUM_MAX_SWP 64

extern struct swp_active_dqs rte_global_active_dqs_list[NUM_MAX_SWP];

/**
 * A structure describing a DPAA2 container.
 */
struct dpaa2_dprc_dev {
	TAILQ_ENTRY(dpaa2_dprc_dev) next;
		/**< Pointer to Next device instance */
	const char *name;
	struct fsl_mc_io dprc;  /** handle to DPRC portal object */
	uint16_t token;
	uint32_t dprc_id; /*HW ID for DPRC object */
};

struct dpaa2_dpci_dev {
	TAILQ_ENTRY(dpaa2_dpci_dev) next;
		/**< Pointer to Next device instance */
	struct fsl_mc_io dpci;  /** handle to DPCI portal object */
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpci_id; /*HW ID for DPCI object */
	struct dpaa2_queue rx_queue[DPAA2_DPCI_MAX_QUEUES];
	struct dpaa2_queue tx_queue[DPAA2_DPCI_MAX_QUEUES];
};

struct dpaa2_dpcon_dev {
	TAILQ_ENTRY(dpaa2_dpcon_dev) next;
	struct fsl_mc_io dpcon;
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpcon_id;
	uint16_t qbman_ch_id;
	uint8_t num_priorities;
	uint8_t channel_index;
};

/* Refer to Table 7-3 in SEC BG */
#define QBMAN_FLE_WORD4_FMT_SBF 0x0    /* Single buffer frame */
#define QBMAN_FLE_WORD4_FMT_SGE 0x2 /* Scatter gather frame */

struct qbman_fle_word4 {
	uint32_t bpid:14; /* Frame buffer pool ID */
	uint32_t ivp:1; /* Invalid Pool ID. */
	uint32_t bmt:1; /* Bypass Memory Translation */
	uint32_t offset:12; /* Frame offset */
	uint32_t fmt:2; /* Frame Format */
	uint32_t sl:1; /* Short Length */
	uint32_t f:1; /* Final bit */
};

struct qbman_fle {
	uint32_t addr_lo;
	uint32_t addr_hi;
	uint32_t length;
	/* FMT must be 00, MSB is final bit  */
	union {
		uint32_t fin_bpid_offset;
		struct qbman_fle_word4 word4;
	};
	uint32_t frc;
	uint32_t reserved[3]; /* Not used currently */
};

struct qbman_sge {
	uint32_t addr_lo;
	uint32_t addr_hi;
	uint32_t length;
	uint32_t fin_bpid_offset;
};

/* There are three types of frames: Single, Scatter Gather and Frame Lists */
enum qbman_fd_format {
	qbman_fd_single = 0,
	qbman_fd_list,
	qbman_fd_sg
};
/*Macros to define operations on FD*/
#define DPAA2_SET_FD_ADDR(fd, addr) do {			\
	(fd)->simple.addr_lo = lower_32_bits((size_t)(addr));	\
	(fd)->simple.addr_hi = upper_32_bits((uint64_t)(addr));	\
} while (0)
#define DPAA2_SET_FD_LEN(fd, length)	((fd)->simple.len = length)
#define DPAA2_SET_FD_BPID(fd, bpid)	((fd)->simple.bpid_offset |= bpid)
#define DPAA2_SET_ONLY_FD_BPID(fd, bpid) \
	((fd)->simple.bpid_offset = bpid)
#define DPAA2_SET_FD_IVP(fd)   (((fd)->simple.bpid_offset |= 0x00004000))
#define DPAA2_SET_FD_OFFSET(fd, offset)	\
	(((fd)->simple.bpid_offset |= (uint32_t)(offset) << 16))
#define DPAA2_SET_FD_INTERNAL_JD(fd, len) \
	((fd)->simple.frc = (0x80000000 | (len)))
#define DPAA2_GET_FD_FRC_PARSE_SUM(fd)	\
			((uint16_t)(((fd)->simple.frc & 0xffff0000) >> 16))
#define DPAA2_RESET_FD_FRC(fd)		((fd)->simple.frc = 0)
#define DPAA2_SET_FD_FRC(fd, _frc)	((fd)->simple.frc = _frc)
#define DPAA2_RESET_FD_CTRL(fd)	 ((fd)->simple.ctrl = 0)

#define	DPAA2_SET_FD_ASAL(fd, asal)	((fd)->simple.ctrl |= (asal << 16))

#define DPAA2_RESET_FD_FLC(fd)	do {	\
	(fd)->simple.flc_lo = 0;	\
	(fd)->simple.flc_hi = 0;	\
} while (0)

#define DPAA2_SET_FD_FLC(fd, addr)	do { \
	(fd)->simple.flc_lo = lower_32_bits((size_t)(addr));	\
	(fd)->simple.flc_hi = upper_32_bits((uint64_t)(addr));	\
} while (0)
#define DPAA2_SET_FLE_INTERNAL_JD(fle, len) ((fle)->frc = (0x80000000 | (len)))
#define DPAA2_GET_FLE_ADDR(fle)					\
	(size_t)((((uint64_t)((fle)->addr_hi)) << 32) + (fle)->addr_lo)
#define DPAA2_SET_FLE_ADDR(fle, addr) do { \
	(fle)->addr_lo = lower_32_bits((size_t)addr);		\
	(fle)->addr_hi = upper_32_bits((uint64_t)addr);		\
} while (0)
#define DPAA2_GET_FLE_CTXT(fle)					\
	((((uint64_t)((fle)->reserved[1])) << 32) + (fle)->reserved[0])
#define DPAA2_FLE_SAVE_CTXT(fle, addr) do { \
	(fle)->reserved[0] = lower_32_bits((size_t)addr);	\
	(fle)->reserved[1] = upper_32_bits((uint64_t)addr);	\
} while (0)
#define DPAA2_SET_FLE_OFFSET(fle, offset) \
	((fle)->fin_bpid_offset |= (uint32_t)(offset) << 16)
#define DPAA2_SET_FLE_LEN(fle, len)    ((fle)->length = len)
#define DPAA2_SET_FLE_BPID(fle, bpid) ((fle)->fin_bpid_offset |= (size_t)bpid)
#define DPAA2_GET_FLE_BPID(fle) ((fle)->fin_bpid_offset & 0x000000ff)
#define DPAA2_SET_FLE_FIN(fle)	((fle)->fin_bpid_offset |= 1 << 31)
#define DPAA2_SET_FLE_IVP(fle)   (((fle)->fin_bpid_offset |= 0x00004000))
#define DPAA2_SET_FLE_BMT(fle)   (((fle)->fin_bpid_offset |= 0x00008000))
#define DPAA2_SET_FD_COMPOUND_FMT(fd)	\
	((fd)->simple.bpid_offset |= (uint32_t)1 << 28)
#define DPAA2_GET_FD_ADDR(fd)	\
(((((uint64_t)((fd)->simple.addr_hi)) << 32) + (fd)->simple.addr_lo))

#define DPAA2_GET_FD_LEN(fd)	((fd)->simple.len)
#define DPAA2_GET_FD_BPID(fd)	(((fd)->simple.bpid_offset & 0x00003FFF))
#define DPAA2_GET_FD_IVP(fd)   (((fd)->simple.bpid_offset & 0x00004000) >> 14)
#define DPAA2_GET_FD_OFFSET(fd)	(((fd)->simple.bpid_offset & 0x0FFF0000) >> 16)
#define DPAA2_GET_FD_DROPP(fd)  (((fd)->simple.ctrl & 0x07000000) >> 24)
#define DPAA2_GET_FD_FRC(fd)   ((fd)->simple.frc)
#define DPAA2_GET_FD_FLC(fd) \
	(((uint64_t)((fd)->simple.flc_hi) << 32) + (fd)->simple.flc_lo)
#define DPAA2_GET_FD_ERR(fd)   ((fd)->simple.ctrl & 0x000000FF)
#define DPAA2_GET_FD_FA_ERR(fd)   ((fd)->simple.ctrl & 0x00000040)
#define DPAA2_GET_FLE_OFFSET(fle) (((fle)->fin_bpid_offset & 0x0FFF0000) >> 16)
#define DPAA2_SET_FLE_SG_EXT(fle) ((fle)->fin_bpid_offset |= (uint64_t)1 << 29)
#define DPAA2_IS_SET_FLE_SG_EXT(fle)	\
	(((fle)->fin_bpid_offset & ((uint64_t)1 << 29)) ? 1 : 0)

#define DPAA2_INLINE_MBUF_FROM_BUF(buf, meta_data_size) \
	((struct rte_mbuf *)((size_t)(buf) - (meta_data_size)))

#define DPAA2_ASAL_VAL (DPAA2_MBUF_HW_ANNOTATION / 64)

#define DPAA2_FD_SET_FORMAT(fd, format)	do {				\
		(fd)->simple.bpid_offset &= 0xCFFFFFFF;			\
		(fd)->simple.bpid_offset |= (uint32_t)format << 28;	\
} while (0)
#define DPAA2_FD_GET_FORMAT(fd)	(((fd)->simple.bpid_offset >> 28) & 0x3)

#define DPAA2_SG_SET_FORMAT(sg, format)	do {				\
		(sg)->fin_bpid_offset &= 0xCFFFFFFF;			\
		(sg)->fin_bpid_offset |= (uint32_t)format << 28;	\
} while (0)

#define DPAA2_SG_SET_FINAL(sg, fin)	do {				\
		(sg)->fin_bpid_offset &= 0x7FFFFFFF;			\
		(sg)->fin_bpid_offset |= (uint32_t)fin << 31;		\
} while (0)
#define DPAA2_SG_IS_FINAL(sg) (!!((sg)->fin_bpid_offset >> 31))
/* Only Enqueue Error responses will be
 * pushed on FQID_ERR of Enqueue FQ
 */
#define DPAA2_EQ_RESP_ERR_FQ		0
/* All Enqueue responses will be pushed on address
 * set with qbman_eq_desc_set_response
 */
#define DPAA2_EQ_RESP_ALWAYS		1

static inline uint64_t
dpaa2_mem_va_to_iova(void *va)
{
	if (likely(rte_eal_iova_mode() == RTE_IOVA_VA))
		return (uint64_t)va;

	return rte_fslmc_mem_vaddr_to_iova(va);
}

static inline void *
dpaa2_mem_iova_to_va(uint64_t iova)
{
	if (likely(rte_eal_iova_mode() == RTE_IOVA_VA))
		return (void *)(uintptr_t)iova;

	return rte_fslmc_mem_iova_to_vaddr(iova);
}

#define DPAA2_MBUF_VADDR_TO_IOVA(mbuf) ((mbuf)->buf_iova)
#define DPAA2_VADDR_TO_IOVA(_vaddr) \
	dpaa2_mem_va_to_iova((void *)(uintptr_t)_vaddr)
#define DPAA2_IOVA_TO_VADDR(_iova) \
	dpaa2_mem_iova_to_va((uint64_t)_iova)
#define DPAA2_MODIFY_IOVA_TO_VADDR(_mem, _type) \
	{_mem = (_type)DPAA2_IOVA_TO_VADDR(_mem); }

#define DPAA2_VAMODE_VADDR_TO_IOVA(_vaddr) ((uint64_t)_vaddr)
#define DPAA2_VAMODE_IOVA_TO_VADDR(_iova) ((void *)_iova)
#define DPAA2_VAMODE_MODIFY_IOVA_TO_VADDR(_mem, _type) \
	{_mem = (_type)(_mem); }

#define DPAA2_PAMODE_VADDR_TO_IOVA(_vaddr) \
	rte_fslmc_mem_vaddr_to_iova((void *)_vaddr)
#define DPAA2_PAMODE_IOVA_TO_VADDR(_iova) \
	rte_fslmc_mem_iova_to_vaddr((uint64_t)_iova)
#define DPAA2_PAMODE_MODIFY_IOVA_TO_VADDR(_mem, _type) \
	{_mem = (_type)rte_fslmc_mem_iova_to_vaddr(_mem); }

static inline uint64_t
dpaa2_mem_va_to_iova_check(void *va, uint64_t size)
{
	uint64_t iova = rte_fslmc_cold_mem_vaddr_to_iova(va, size);

	if (iova == RTE_BAD_IOVA)
		return RTE_BAD_IOVA;

	/** Double check the iova is valid.*/
	if (iova != rte_mem_virt2iova(va))
		return RTE_BAD_IOVA;

	return iova;
}

#define DPAA2_VADDR_TO_IOVA_AND_CHECK(_vaddr, size) \
	dpaa2_mem_va_to_iova_check(_vaddr, size)
#define DPAA2_IOVA_TO_VADDR_AND_CHECK(_iova, size) \
	rte_fslmc_cold_mem_iova_to_vaddr(_iova, size)

static inline
int check_swp_active_dqs(uint16_t dpio_index)
{
	if (rte_global_active_dqs_list[dpio_index].global_active_dqs != NULL)
		return 1;
	return 0;
}

static inline
void clear_swp_active_dqs(uint16_t dpio_index)
{
	rte_global_active_dqs_list[dpio_index].global_active_dqs = NULL;
}

static inline
struct qbman_result *get_swp_active_dqs(uint16_t dpio_index)
{
	return rte_global_active_dqs_list[dpio_index].global_active_dqs;
}

/* 00 00 00 - last 6 bit represent data, annotation,
 * context stashing setting 01 01 00 (0x14)
 * (in following order ->DS AS CS)
 * to enable 1 line data, 1 line annotation.
 * For LX2, this setting should be 01 00 00 (0x10)
 */
#define DPAA2_FLC_STASHING_MAX_BIT_SIZE 2
#define DPAA2_FLC_STASHING_MAX_CACHE_LINE \
	((1ULL << DPAA2_FLC_STASHING_MAX_BIT_SIZE) - 1)

enum dpaa2_flc_stashing_type {
	DPAA2_FLC_CNTX_STASHING = 0,
	DPAA2_FLC_ANNO_STASHING =
		DPAA2_FLC_CNTX_STASHING + DPAA2_FLC_STASHING_MAX_BIT_SIZE,
	DPAA2_FLC_DATA_STASHING =
		DPAA2_FLC_ANNO_STASHING + DPAA2_FLC_STASHING_MAX_BIT_SIZE,
	DPAA2_FLC_END_STASHING =
		DPAA2_FLC_DATA_STASHING + DPAA2_FLC_STASHING_MAX_BIT_SIZE
};

#define DPAA2_STASHING_ALIGN_SIZE (1 << DPAA2_FLC_END_STASHING)

static inline void
dpaa2_flc_stashing_set(enum dpaa2_flc_stashing_type type,
	uint8_t cache_line, uint64_t *flc)
{
	RTE_ASSERT(cache_line <= DPAA2_FLC_STASHING_MAX_CACHE_LINE);
	RTE_ASSERT(type == DPAA2_FLC_CNTX_STASHING ||
		type == DPAA2_FLC_ANNO_STASHING ||
		type == DPAA2_FLC_DATA_STASHING);

	(*flc) &= ~(DPAA2_FLC_STASHING_MAX_CACHE_LINE << type);
	(*flc) |= (cache_line << type);
}

static inline void
dpaa2_flc_stashing_clear_all(uint64_t *flc)
{
	dpaa2_flc_stashing_set(DPAA2_FLC_CNTX_STASHING, 0, flc);
	dpaa2_flc_stashing_set(DPAA2_FLC_ANNO_STASHING, 0, flc);
	dpaa2_flc_stashing_set(DPAA2_FLC_DATA_STASHING, 0, flc);
}

static inline
void set_swp_active_dqs(uint16_t dpio_index, struct qbman_result *dqs)
{
	rte_global_active_dqs_list[dpio_index].global_active_dqs = dqs;
}

__rte_internal
struct dpaa2_dpbp_dev *dpaa2_alloc_dpbp_dev(void);

__rte_internal
void dpaa2_free_dpbp_dev(struct dpaa2_dpbp_dev *dpbp);

__rte_internal
int dpaa2_dpbp_supported(void);

__rte_internal
struct dpaa2_dpci_dev *rte_dpaa2_alloc_dpci_dev(void);

__rte_internal
void rte_dpaa2_free_dpci_dev(struct dpaa2_dpci_dev *dpci);

/* Global MCP pointer */
__rte_internal
void *dpaa2_get_mcp_ptr(int portal_idx);

#endif
