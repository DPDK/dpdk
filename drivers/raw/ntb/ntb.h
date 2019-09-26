/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */

#ifndef _NTB_H_
#define _NTB_H_

#include <stdbool.h>

extern int ntb_logtype;

#define NTB_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ntb_logtype,	"%s(): " fmt "\n", \
		__func__, ##args)

/* Vendor ID */
#define NTB_INTEL_VENDOR_ID         0x8086

/* Device IDs */
#define NTB_INTEL_DEV_ID_B2B_SKX    0x201C

/* Reserved to app to use. */
#define NTB_SPAD_USER               "spad_user_"
#define NTB_SPAD_USER_LEN           (sizeof(NTB_SPAD_USER) - 1)
#define NTB_SPAD_USER_MAX_NUM       4
#define NTB_ATTR_NAME_LEN           30

#define NTB_DFLT_TX_FREE_THRESH     256

enum ntb_xstats_idx {
	NTB_TX_PKTS_ID = 0,
	NTB_TX_BYTES_ID,
	NTB_TX_ERRS_ID,
	NTB_RX_PKTS_ID,
	NTB_RX_BYTES_ID,
	NTB_RX_MISS_ID,
};

enum ntb_topo {
	NTB_TOPO_NONE = 0,
	NTB_TOPO_B2B_USD,
	NTB_TOPO_B2B_DSD,
};

enum ntb_link {
	NTB_LINK_DOWN = 0,
	NTB_LINK_UP,
};

enum ntb_speed {
	NTB_SPEED_NONE = 0,
	NTB_SPEED_GEN1 = 1,
	NTB_SPEED_GEN2 = 2,
	NTB_SPEED_GEN3 = 3,
	NTB_SPEED_GEN4 = 4,
};

enum ntb_width {
	NTB_WIDTH_NONE = 0,
	NTB_WIDTH_1 = 1,
	NTB_WIDTH_2 = 2,
	NTB_WIDTH_4 = 4,
	NTB_WIDTH_8 = 8,
	NTB_WIDTH_12 = 12,
	NTB_WIDTH_16 = 16,
	NTB_WIDTH_32 = 32,
};

/* Define spad registers usage. 0 is reserved. */
enum ntb_spad_idx {
	SPAD_NUM_MWS = 1,
	SPAD_NUM_QPS,
	SPAD_Q_SZ,
	SPAD_USED_MWS,
	SPAD_MW0_SZ_H,
	SPAD_MW0_SZ_L,
	SPAD_MW1_SZ_H,
	SPAD_MW1_SZ_L,
	SPAD_MW0_BA_H,
	SPAD_MW0_BA_L,
	SPAD_MW1_BA_H,
	SPAD_MW1_BA_L,
};

/**
 * NTB device operations
 * @ntb_dev_init: Init ntb dev.
 * @get_peer_mw_addr: To get the addr of peer mw[mw_idx].
 * @mw_set_trans: Set translation of internal memory that remote can access.
 * @ioremap: Translate the remote host address to bar address.
 * @get_link_status: get link status, link speed and link width.
 * @set_link: Set local side up/down.
 * @spad_read: Read local/peer spad register val.
 * @spad_write: Write val to local/peer spad register.
 * @db_read: Read doorbells status.
 * @db_clear: Clear local doorbells.
 * @db_set_mask: Set bits in db mask, preventing db interrpts generated
 * for those db bits.
 * @peer_db_set: Set doorbell bit to generate peer interrupt for that bit.
 * @vector_bind: Bind vector source [intr] to msix vector [msix].
 */
struct ntb_dev_ops {
	int (*ntb_dev_init)(const struct rte_rawdev *dev);
	void *(*get_peer_mw_addr)(const struct rte_rawdev *dev, int mw_idx);
	int (*mw_set_trans)(const struct rte_rawdev *dev, int mw_idx,
			    uint64_t addr, uint64_t size);
	void *(*ioremap)(const struct rte_rawdev *dev, uint64_t addr);
	int (*get_link_status)(const struct rte_rawdev *dev);
	int (*set_link)(const struct rte_rawdev *dev, bool up);
	uint32_t (*spad_read)(const struct rte_rawdev *dev, int spad,
			      bool peer);
	int (*spad_write)(const struct rte_rawdev *dev, int spad,
			  bool peer, uint32_t spad_v);
	uint64_t (*db_read)(const struct rte_rawdev *dev);
	int (*db_clear)(const struct rte_rawdev *dev, uint64_t db_bits);
	int (*db_set_mask)(const struct rte_rawdev *dev, uint64_t db_mask);
	int (*peer_db_set)(const struct rte_rawdev *dev, uint8_t db_bit);
	int (*vector_bind)(const struct rte_rawdev *dev, uint8_t intr,
			   uint8_t msix);
};

struct ntb_desc {
	uint64_t addr; /* buffer addr */
	uint16_t len;  /* buffer length */
	uint16_t rsv1;
	uint32_t rsv2;
};

#define NTB_FLAG_EOP    1 /* end of packet */
struct ntb_used {
	uint16_t len;     /* buffer length */
	uint16_t flags;   /* flags */
};

struct ntb_rx_entry {
	struct rte_mbuf *mbuf;
};

struct ntb_rx_queue {
	struct ntb_desc *rx_desc_ring;
	volatile struct ntb_used *rx_used_ring;
	uint16_t *avail_cnt;
	volatile uint16_t *used_cnt;
	uint16_t last_avail;
	uint16_t last_used;
	uint16_t nb_rx_desc;

	uint16_t rx_free_thresh;

	struct rte_mempool *mpool; /* mempool for mbuf allocation */
	struct ntb_rx_entry *sw_ring;

	uint16_t queue_id;         /* DPDK queue index. */
	uint16_t port_id;          /* Device port identifier. */

	struct ntb_hw *hw;
};

struct ntb_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

struct ntb_tx_queue {
	volatile struct ntb_desc *tx_desc_ring;
	struct ntb_used *tx_used_ring;
	volatile uint16_t *avail_cnt;
	uint16_t *used_cnt;
	uint16_t last_avail;          /* Next need to be free. */
	uint16_t last_used;           /* Next need to be sent. */
	uint16_t nb_tx_desc;

	/* Total number of TX descriptors ready to be allocated. */
	uint16_t nb_tx_free;
	uint16_t tx_free_thresh;

	struct ntb_tx_entry *sw_ring;

	uint16_t queue_id;            /* DPDK queue index. */
	uint16_t port_id;             /* Device port identifier. */

	struct ntb_hw *hw;
};

struct ntb_header {
	uint16_t avail_cnt __rte_cache_aligned;
	uint16_t used_cnt __rte_cache_aligned;
	struct ntb_desc desc_ring[] __rte_cache_aligned;
};

/* ntb private data. */
struct ntb_hw {
	uint8_t mw_cnt;
	uint8_t db_cnt;
	uint8_t spad_cnt;

	uint64_t db_valid_mask;
	uint64_t db_mask;

	enum ntb_topo topo;

	enum ntb_link link_status;
	enum ntb_speed link_speed;
	enum ntb_width link_width;

	const struct ntb_dev_ops *ntb_ops;

	struct rte_pci_device *pci_dev;
	char *hw_addr;

	uint8_t peer_dev_up;
	uint64_t *mw_size;
	/* remote mem base addr */
	uint64_t *peer_mw_base;

	uint16_t queue_pairs;
	uint16_t queue_size;
	uint32_t hdr_size_per_queue;

	struct ntb_rx_queue **rx_queues;
	struct ntb_tx_queue **tx_queues;

	/* memzone to populate RX ring. */
	const struct rte_memzone **mz;
	uint8_t used_mw_num;

	uint8_t peer_used_mws;

	uint64_t *ntb_xstats;
	uint64_t *ntb_xstats_off;

	/* Reserve several spad for app to use. */
	int spad_user_list[NTB_SPAD_USER_MAX_NUM];
};

#endif /* _NTB_H_ */
