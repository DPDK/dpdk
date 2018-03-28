/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2015 Intel Corporation
 */

#ifndef _FM10K_H_
#define _FM10K_H_

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include "fm10k_logs.h"
#include "base/fm10k_type.h"

/* descriptor ring base addresses must be aligned to the following */
#define FM10K_ALIGN_RX_DESC  128
#define FM10K_ALIGN_TX_DESC  128

/* The maximum packet size that FM10K supports */
#define FM10K_MAX_PKT_SIZE  (15 * 1024)

/* Minimum size of RX buffer FM10K supported */
#define FM10K_MIN_RX_BUF_SIZE  256

/* The maximum of SRIOV VFs per port supported */
#define FM10K_MAX_VF_NUM    64

/* number of descriptors must be a multiple of the following */
#define FM10K_MULT_RX_DESC  FM10K_REQ_RX_DESCRIPTOR_MULTIPLE
#define FM10K_MULT_TX_DESC  FM10K_REQ_TX_DESCRIPTOR_MULTIPLE

/* maximum size of descriptor rings */
#define FM10K_MAX_RX_RING_SZ  (512 * 1024)
#define FM10K_MAX_TX_RING_SZ  (512 * 1024)

/* minimum and maximum number of descriptors in a ring */
#define FM10K_MIN_RX_DESC  32
#define FM10K_MIN_TX_DESC  32
#define FM10K_MAX_RX_DESC  (FM10K_MAX_RX_RING_SZ / sizeof(union fm10k_rx_desc))
#define FM10K_MAX_TX_DESC  (FM10K_MAX_TX_RING_SZ / sizeof(struct fm10k_tx_desc))

#define FM10K_TX_MAX_SEG     UINT8_MAX
#define FM10K_TX_MAX_MTU_SEG UINT8_MAX

/*
 * byte aligment for HW RX data buffer
 * Datasheet requires RX buffer addresses shall either be 512-byte aligned or
 * be 8-byte aligned but without crossing host memory pages (4KB alignment
 * boundaries). Satisfy first option.
 */
#define FM10K_RX_DATABUF_ALIGN 512

/*
 * threshold default, min, max, and divisor constraints
 * the configured values must satisfy the following:
 *   MIN <= value <= MAX
 *   DIV % value == 0
 */
#define FM10K_RX_FREE_THRESH_DEFAULT(rxq)  32
#define FM10K_RX_FREE_THRESH_MIN(rxq)      1
#define FM10K_RX_FREE_THRESH_MAX(rxq)      ((rxq)->nb_desc - 1)
#define FM10K_RX_FREE_THRESH_DIV(rxq)      ((rxq)->nb_desc)

#define FM10K_TX_FREE_THRESH_DEFAULT(txq)  32
#define FM10K_TX_FREE_THRESH_MIN(txq)      1
#define FM10K_TX_FREE_THRESH_MAX(txq)      ((txq)->nb_desc - 3)
#define FM10K_TX_FREE_THRESH_DIV(txq)      0

#define FM10K_DEFAULT_RX_PTHRESH      8
#define FM10K_DEFAULT_RX_HTHRESH      8
#define FM10K_DEFAULT_RX_WTHRESH      0

#define FM10K_DEFAULT_TX_PTHRESH      32
#define FM10K_DEFAULT_TX_HTHRESH      0
#define FM10K_DEFAULT_TX_WTHRESH      0

#define FM10K_TX_RS_THRESH_DEFAULT(txq)    32
#define FM10K_TX_RS_THRESH_MIN(txq)        1
#define FM10K_TX_RS_THRESH_MAX(txq)        \
	RTE_MIN(((txq)->nb_desc - 2), (txq)->free_thresh)
#define FM10K_TX_RS_THRESH_DIV(txq)        ((txq)->nb_desc)

#define FM10K_VLAN_TAG_SIZE 4

/* Maximum number of MAC addresses per PF/VF */
#define FM10K_MAX_MACADDR_NUM       64

#define FM10K_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define FM10K_VFTA_SIZE            (4096 / FM10K_UINT32_BIT_SIZE)

/* vlan_id is a 12 bit number.
 * The VFTA array is actually a 4096 bit array, 128 of 32bit elements.
 * 2^5 = 32. The val of lower 5 bits specifies the bit in the 32bit element.
 * The higher 7 bit val specifies VFTA array index.
 */
#define FM10K_VFTA_BIT(vlan_id)    (1 << ((vlan_id) & 0x1F))
#define FM10K_VFTA_IDX(vlan_id)    ((vlan_id) >> 5)

#define RTE_FM10K_RXQ_REARM_THRESH      32
#define RTE_FM10K_VPMD_TX_BURST         32
#define RTE_FM10K_MAX_RX_BURST          RTE_FM10K_RXQ_REARM_THRESH
#define RTE_FM10K_TX_MAX_FREE_BUF_SZ    64
#define RTE_FM10K_DESCS_PER_LOOP    4

#define FM10K_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define FM10K_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

#define FM10K_SIMPLE_TX_FLAG ((uint32_t)ETH_TXQ_FLAGS_NOMULTSEGS | \
				ETH_TXQ_FLAGS_NOOFFLOADS)

struct fm10k_macvlan_filter_info {
	uint16_t vlan_num;       /* Total VLAN number */
	uint16_t mac_num;        /* Total mac number */
	uint16_t nb_queue_pools; /* Active queue pools number */
	/* VMDQ ID for each MAC address */
	uint8_t  mac_vmdq_id[FM10K_MAX_MACADDR_NUM];
	uint32_t vfta[FM10K_VFTA_SIZE];        /* VLAN bitmap */
};

struct fm10k_dev_info {
	volatile uint32_t enable;
	volatile uint32_t glort;
	/* Protect the mailbox to avoid race condition */
	rte_spinlock_t    mbx_lock;
	struct fm10k_macvlan_filter_info    macvlan;
	/* Flag to indicate if RX vector conditions satisfied */
	bool rx_vec_allowed;
	bool sm_down;
};

/*
 * Structure to store private data for each driver instance.
 */
struct fm10k_adapter {
	struct fm10k_hw             hw;
	struct fm10k_hw_stats       stats;
	struct fm10k_dev_info       info;
};

#define FM10K_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct fm10k_adapter *)adapter)->hw)

#define FM10K_DEV_PRIVATE_TO_STATS(adapter) \
	(&((struct fm10k_adapter *)adapter)->stats)

#define FM10K_DEV_PRIVATE_TO_INFO(adapter) \
	(&((struct fm10k_adapter *)adapter)->info)

#define FM10K_DEV_PRIVATE_TO_MBXLOCK(adapter) \
	(&(((struct fm10k_adapter *)adapter)->info.mbx_lock))

#define FM10K_DEV_PRIVATE_TO_MACVLAN(adapter) \
		(&(((struct fm10k_adapter *)adapter)->info.macvlan))

struct fm10k_rx_queue {
	struct rte_mempool *mp;
	struct rte_mbuf **sw_ring;
	volatile union fm10k_rx_desc *hw_ring;
	struct rte_mbuf *pkt_first_seg; /* First segment of current packet. */
	struct rte_mbuf *pkt_last_seg;  /* Last segment of current packet. */
	uint64_t hw_ring_phys_addr;
	uint64_t mbuf_initializer; /* value to init mbufs */
	/* need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	uint16_t next_dd;
	uint16_t next_alloc;
	uint16_t next_trigger;
	uint16_t alloc_thresh;
	volatile uint32_t *tail_ptr;
	uint16_t nb_desc;
	/* Number of faked desc added at the tail for Vector RX function */
	uint16_t nb_fake_desc;
	uint16_t queue_id;
	/* Below 2 fields only valid in case vPMD is applied. */
	uint16_t rxrearm_nb;     /* number of remaining to be re-armed */
	uint16_t rxrearm_start;  /* the idx we start the re-arming from */
	uint16_t rx_using_sse; /* indicates that vector RX is in use */
	uint16_t port_id;
	uint8_t drop_en;
	uint8_t rx_deferred_start; /* don't start this queue in dev start. */
	uint16_t rx_ftag_en; /* indicates FTAG RX supported */
	uint64_t offloads; /* offloads of DEV_RX_OFFLOAD_* */
};

/*
 * a FIFO is used to track which descriptors have their RS bit set for Tx
 * queues which are configured to allow multiple descriptors per packet
 */
struct fifo {
	uint16_t *list;
	uint16_t *head;
	uint16_t *tail;
	uint16_t *endp;
};

struct fm10k_txq_ops;

struct fm10k_tx_queue {
	struct rte_mbuf **sw_ring;
	struct fm10k_tx_desc *hw_ring;
	uint64_t hw_ring_phys_addr;
	struct fifo rs_tracker;
	const struct fm10k_txq_ops *ops; /* txq ops */
	uint16_t last_free;
	uint16_t next_free;
	uint16_t nb_free;
	uint16_t nb_used;
	uint16_t free_thresh;
	uint16_t rs_thresh;
	/* Below 2 fields only valid in case vPMD is applied. */
	uint16_t next_rs; /* Next pos to set RS flag */
	uint16_t next_dd; /* Next pos to check DD flag */
	volatile uint32_t *tail_ptr;
	uint32_t txq_flags; /* Holds flags for this TXq */
	uint16_t nb_desc;
	uint16_t port_id;
	uint8_t tx_deferred_start; /** don't start this queue in dev start. */
	uint16_t queue_id;
	uint16_t tx_ftag_en; /* indicates FTAG TX supported */
};

struct fm10k_txq_ops {
	void (*reset)(struct fm10k_tx_queue *txq);
};

#define MBUF_DMA_ADDR(mb) \
	((uint64_t) ((mb)->buf_iova + (mb)->data_off))

/* enforce 512B alignment on default Rx DMA addresses */
#define MBUF_DMA_ADDR_DEFAULT(mb) \
	((uint64_t) RTE_ALIGN(((mb)->buf_iova + RTE_PKTMBUF_HEADROOM),\
			FM10K_RX_DATABUF_ALIGN))

static inline void fifo_reset(struct fifo *fifo, uint32_t len)
{
	fifo->head = fifo->tail = fifo->list;
	fifo->endp = fifo->list + len;
}

static inline void fifo_insert(struct fifo *fifo, uint16_t val)
{
	*fifo->head = val;
	if (++fifo->head == fifo->endp)
		fifo->head = fifo->list;
}

/* do not worry about list being empty since we only check it once we know
 * we have used enough descriptors to set the RS bit at least once */
static inline uint16_t fifo_peek(struct fifo *fifo)
{
	return *fifo->tail;
}

static inline uint16_t fifo_remove(struct fifo *fifo)
{
	uint16_t val;
	val = *fifo->tail;
	if (++fifo->tail == fifo->endp)
		fifo->tail = fifo->list;
	return val;
}

static inline void
fm10k_pktmbuf_reset(struct rte_mbuf *mb, uint16_t in_port)
{
	rte_mbuf_refcnt_set(mb, 1);
	mb->next = NULL;
	mb->nb_segs = 1;

	/* enforce 512B alignment on default Rx virtual addresses */
	mb->data_off = (uint16_t)(RTE_PTR_ALIGN((char *)mb->buf_addr +
			RTE_PKTMBUF_HEADROOM, FM10K_RX_DATABUF_ALIGN)
			- (char *)mb->buf_addr);
	mb->port = in_port;
}

/*
 * Verify Rx packet buffer alignment is valid.
 *
 * Hardware requires specific alignment for Rx packet buffers. At
 * least one of the following two conditions must be satisfied.
 *  1. Address is 512B aligned
 *  2. Address is 8B aligned and buffer does not cross 4K boundary.
 *
 * Return 1 if buffer alignment satisfies at least one condition,
 * otherwise return 0.
 *
 * Note: Alignment is checked by the driver when the Rx queue is reset. It
 *       is assumed that if an entire descriptor ring can be filled with
 *       buffers containing valid alignment, then all buffers in that mempool
 *       have valid address alignment. It is the responsibility of the user
 *       to ensure all buffers have valid alignment, as it is the user who
 *       creates the mempool.
 * Note: It is assumed the buffer needs only to store a maximum size Ethernet
 *       frame.
 */
static inline int
fm10k_addr_alignment_valid(struct rte_mbuf *mb)
{
	uint64_t addr = MBUF_DMA_ADDR_DEFAULT(mb);
	uint64_t boundary1, boundary2;

	/* 512B aligned? */
	if (RTE_ALIGN(addr, FM10K_RX_DATABUF_ALIGN) == addr)
		return 1;

	/* 8B aligned, and max Ethernet frame would not cross a 4KB boundary? */
	if (RTE_ALIGN(addr, 8) == addr) {
		boundary1 = RTE_ALIGN_FLOOR(addr, 4096);
		boundary2 = RTE_ALIGN_FLOOR(addr + ETHER_MAX_VLAN_FRAME_LEN,
						4096);
		if (boundary1 == boundary2)
			return 1;
	}

	PMD_INIT_LOG(ERR, "Error: Invalid buffer alignment!");

	return 0;
}

/* Rx and Tx prototypes */
uint16_t fm10k_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);

uint16_t fm10k_recv_scattered_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

int
fm10k_dev_rx_descriptor_done(void *rx_queue, uint16_t offset);

uint16_t fm10k_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

uint16_t fm10k_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

int fm10k_rxq_vec_setup(struct fm10k_rx_queue *rxq);
int fm10k_rx_vec_condition_check(struct rte_eth_dev *);
void fm10k_rx_queue_release_mbufs_vec(struct fm10k_rx_queue *rxq);
uint16_t fm10k_recv_pkts_vec(void *, struct rte_mbuf **, uint16_t);
uint16_t fm10k_recv_scattered_pkts_vec(void *, struct rte_mbuf **,
					uint16_t);
uint16_t fm10k_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
				    uint16_t nb_pkts);
void fm10k_txq_vec_setup(struct fm10k_tx_queue *txq);
int fm10k_tx_vec_condition_check(struct fm10k_tx_queue *txq);

#endif
