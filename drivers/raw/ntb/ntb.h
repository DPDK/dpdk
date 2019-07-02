/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */

#ifndef _NTB_RAWDEV_H_
#define _NTB_RAWDEV_H_

#include <stdbool.h>

extern int ntb_logtype;

#define NTB_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ntb_logtype,	"%s(): " fmt "\n", \
		__func__, ##args)

/* Vendor ID */
#define NTB_INTEL_VENDOR_ID         0x8086

/* Device IDs */
#define NTB_INTEL_DEV_ID_B2B_SKX    0x201C

#define NTB_TOPO_NAME               "topo"
#define NTB_LINK_STATUS_NAME        "link_status"
#define NTB_SPEED_NAME              "speed"
#define NTB_WIDTH_NAME              "width"
#define NTB_MW_CNT_NAME             "mw_count"
#define NTB_DB_CNT_NAME             "db_count"
#define NTB_SPAD_CNT_NAME           "spad_count"
/* Reserved to app to use. */
#define NTB_SPAD_USER               "spad_user_"
#define NTB_SPAD_USER_LEN           (sizeof(NTB_SPAD_USER) - 1)
#define NTB_SPAD_USER_MAX_NUM       10
#define NTB_ATTR_NAME_LEN           30
#define NTB_ATTR_VAL_LEN            30
#define NTB_ATTR_MAX                20

/* NTB Attributes */
struct ntb_attr {
	/**< Name of the attribute */
	char name[NTB_ATTR_NAME_LEN];
	/**< Value or reference of value of attribute */
	char value[NTB_ATTR_NAME_LEN];
};

enum ntb_attr_idx {
	NTB_TOPO_ID = 0,
	NTB_LINK_STATUS_ID,
	NTB_SPEED_ID,
	NTB_WIDTH_ID,
	NTB_MW_CNT_ID,
	NTB_DB_CNT_ID,
	NTB_SPAD_CNT_ID,
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
	SPAD_MW0_SZ_H,
	SPAD_MW0_SZ_L,
	SPAD_MW1_SZ_H,
	SPAD_MW1_SZ_L,
};

/**
 * NTB device operations
 * @ntb_dev_init: Init ntb dev.
 * @get_peer_mw_addr: To get the addr of peer mw[mw_idx].
 * @mw_set_trans: Set translation of internal memory that remote can access.
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
	int (*ntb_dev_init)(struct rte_rawdev *dev);
	void *(*get_peer_mw_addr)(struct rte_rawdev *dev, int mw_idx);
	int (*mw_set_trans)(struct rte_rawdev *dev, int mw_idx,
			    uint64_t addr, uint64_t size);
	int (*get_link_status)(struct rte_rawdev *dev);
	int (*set_link)(struct rte_rawdev *dev, bool up);
	uint32_t (*spad_read)(struct rte_rawdev *dev, int spad, bool peer);
	int (*spad_write)(struct rte_rawdev *dev, int spad,
			  bool peer, uint32_t spad_v);
	uint64_t (*db_read)(struct rte_rawdev *dev);
	int (*db_clear)(struct rte_rawdev *dev, uint64_t db_bits);
	int (*db_set_mask)(struct rte_rawdev *dev, uint64_t db_mask);
	int (*peer_db_set)(struct rte_rawdev *dev, uint8_t db_bit);
	int (*vector_bind)(struct rte_rawdev *dev, uint8_t intr, uint8_t msix);
};

/* ntb private data. */
struct ntb_hw {
	uint8_t mw_cnt;
	uint8_t peer_mw_cnt;
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

	uint64_t *mw_size;
	uint64_t *peer_mw_size;
	uint8_t peer_dev_up;

	uint16_t queue_pairs;
	uint16_t queue_size;

	/**< mem zone to populate RX ring. */
	const struct rte_memzone **mz;

	/* Reserve several spad for app to use. */
	int spad_user_list[NTB_SPAD_USER_MAX_NUM];
};

#endif /* _NTB_RAWDEV_H_ */
