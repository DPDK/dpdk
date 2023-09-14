/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

/* This file should not be included directly.  Include common.h instead. */

#ifndef __T4_ADAPTER_H__
#define __T4_ADAPTER_H__

#include <bus_pci_driver.h>
#include <rte_mbuf.h>
#include <rte_io.h>
#include <rte_rwlock.h>
#include <ethdev_driver.h>

#include "../cxgbe_compat.h"
#include "../cxgbe_ofld.h"
#include "t4_regs_values.h"

enum {
	MAX_CTRL_QUEUES = NCHAN,      /* # of control Tx queues */
};

struct adapter;
struct sge_rspq;

enum {
	PORT_RSS_DONE = (1 << 0),
};

struct port_info {
	struct adapter *adapter;        /* adapter that this port belongs to */
	struct rte_eth_dev *eth_dev;    /* associated rte eth device */
	struct port_stats stats_base;   /* port statistics base */
	struct link_config link_cfg;    /* link configuration info */

	unsigned long flags;            /* port related flags */
	short int xact_addr_filt;       /* index of exact MAC address filter */

	u16    viid;                    /* associated virtual interface id */
	u8     port_id;                 /* physical port ID */
	u8     pidx;			/* port index for this PF */
	u8     tx_chan;                 /* associated channel */

	u16    n_rx_qsets;              /* # of rx qsets */
	u16    n_tx_qsets;              /* # of tx qsets */
	u16    first_rxqset;            /* index of first rxqset */
	u16    first_txqset;            /* index of first txqset */

	u16    *rss;                    /* rss table */
	u8     rss_mode;                /* rss mode */
	u16    rss_size;                /* size of VI's RSS table slice */
	u64    rss_hf;			/* RSS Hash Function */

	/* viid fields either returned by fw
	 * or decoded by parsing viid by driver.
	 */
	u8 vin;
	u8 vivld;

	u8 vi_en_rx; /* Enable/disable VI Rx */
	u8 vi_en_tx; /* Enable/disable VI Tx */
};

enum {                                 /* adapter flags */
	FULL_INIT_DONE     = (1 << 0),
	USING_MSI          = (1 << 1),
	USING_MSIX         = (1 << 2),
	FW_QUEUE_BOUND     = (1 << 3),
	FW_OK              = (1 << 4),
	CFG_QUEUES	   = (1 << 5),
	MASTER_PF          = (1 << 6),
};

struct rx_sw_desc {                /* SW state per Rx descriptor */
	void *buf;                 /* struct page or mbuf */
	dma_addr_t dma_addr;
};

struct sge_fl {                     /* SGE free-buffer queue state */
	/* RO fields */
	struct rx_sw_desc *sdesc;   /* address of SW Rx descriptor ring */

	dma_addr_t addr;            /* bus address of HW ring start */
	__be64 *desc;               /* address of HW Rx descriptor ring */

	void __iomem *bar2_addr;    /* address of BAR2 Queue registers */
	unsigned int bar2_qid;      /* Queue ID for BAR2 Queue registers */

	unsigned int cntxt_id;      /* SGE relative QID for the free list */
	unsigned int size;          /* capacity of free list */

	unsigned int avail;         /* # of available Rx buffers */
	unsigned int pend_cred;     /* new buffers since last FL DB ring */
	unsigned int cidx;          /* consumer index */
	unsigned int pidx;          /* producer index */

	unsigned long alloc_failed; /* # of times buffer allocation failed */
	unsigned long low;          /* # of times momentarily starving */
	u8 fl_buf_size_idx;         /* Selected SGE_FL_BUFFER_SIZE index */
};

#define MAX_MBUF_FRAGS (16384 / 512 + 2)

/* A packet gather list */
struct pkt_gl {
	union {
		struct rte_mbuf *mbufs[MAX_MBUF_FRAGS];
	} /* UNNAMED */;
	void *va;                         /* virtual address of first byte */
	unsigned int nfrags;              /* # of fragments */
	unsigned int tot_len;             /* total length of fragments */
};

typedef int (*rspq_handler_t)(struct sge_rspq *q, const __be64 *rsp,
			      const struct pkt_gl *gl);

struct sge_rspq {                   /* state for an SGE response queue */
	struct adapter *adapter;      /* adapter that this queue belongs to */
	struct rte_eth_dev *eth_dev;  /* associated rte eth device */
	struct rte_mempool  *mb_pool; /* associated mempool */

	dma_addr_t phys_addr;       /* physical address of the ring */
	__be64 *desc;               /* address of HW response ring */
	const __be64 *cur_desc;     /* current descriptor in queue */

	void __iomem *bar2_addr;    /* address of BAR2 Queue registers */
	unsigned int bar2_qid;      /* Queue ID for BAR2 Queue registers */
	struct sge_qstat *stat;

	unsigned int cidx;          /* consumer index */
	unsigned int gts_idx;	    /* last gts write sent */
	unsigned int iqe_len;       /* entry size */
	unsigned int size;          /* capacity of response queue */
	int offset;                 /* offset into current Rx buffer */

	u8 gen;                     /* current generation bit */
	u8 intr_params;             /* interrupt holdoff parameters */
	u8 next_intr_params;        /* holdoff params for next interrupt */
	u8 pktcnt_idx;              /* interrupt packet threshold */
	u8 port_id;		    /* associated port-id */
	u8 idx;                     /* queue index within its group */
	u16 cntxt_id;               /* SGE relative QID for the response Q */
	u16 abs_id;                 /* absolute SGE id for the response q */

	rspq_handler_t handler;     /* associated handler for this response q */
};

struct sge_eth_rx_stats {	/* Ethernet rx queue statistics */
	u64 pkts;		/* # of ethernet packets */
	u64 rx_bytes;		/* # of ethernet bytes */
	u64 rx_cso;		/* # of Rx checksum offloads */
	u64 vlan_ex;		/* # of Rx VLAN extractions */
	u64 rx_drops;		/* # of packets dropped due to no mem */
};

struct sge_eth_rxq {                /* a SW Ethernet Rx queue */
	unsigned int flags;         /* flags for state of the queue */
	struct sge_rspq rspq;
	struct sge_fl fl;
	struct sge_eth_rx_stats stats;
} __rte_cache_aligned;

/*
 * Currently there are two types of coalesce WR. Type 0 needs 48 bytes per
 * packet (if one sgl is present) and type 1 needs 32 bytes. This means
 * that type 0 can fit a maximum of 10 packets per WR and type 1 can fit
 * 15 packets. We need to keep track of the mbuf pointers in a coalesce WR
 * to be able to free those mbufs when we get completions back from the FW.
 * Allocating the maximum number of pointers in every tx desc is a waste
 * of memory resources so we only store 2 pointers per tx desc which should
 * be enough since a tx desc can only fit 2 packets in the best case
 * scenario where a packet needs 32 bytes.
 */
#define ETH_COALESCE_PKT_NUM 15
#define ETH_COALESCE_VF_PKT_NUM 7
#define ETH_COALESCE_PKT_PER_DESC 2

struct tx_eth_coal_desc {
	struct rte_mbuf *mbuf[ETH_COALESCE_PKT_PER_DESC];
	struct ulptx_sgl *sgl[ETH_COALESCE_PKT_PER_DESC];
	int idx;
};

struct tx_desc {
	__be64 flit[8];
};

struct tx_sw_desc {                /* SW state per Tx descriptor */
	struct rte_mbuf *mbuf;
	struct ulptx_sgl *sgl;
	struct tx_eth_coal_desc coalesce;
};

enum cxgbe_txq_state {
	EQ_STOPPED = (1 << 0),
};

enum cxgbe_rxq_state {
	IQ_STOPPED = (1 << 0),
};

struct eth_coalesce {
	unsigned char *ptr;
	unsigned char type;
	unsigned int idx;
	unsigned int len;
	unsigned int flits;
	unsigned int max;
	__u8 ethmacdst[ETHER_ADDR_LEN];
	__u8 ethmacsrc[ETHER_ADDR_LEN];
	__be16 ethtype;
	__be16 vlantci;
};

struct sge_txq {
	struct tx_desc *desc;       /* address of HW Tx descriptor ring */
	struct tx_sw_desc *sdesc;   /* address of SW Tx descriptor ring */
	struct sge_qstat *stat;     /* queue status entry */
	struct eth_coalesce coalesce; /* coalesce info */

	uint64_t phys_addr;         /* physical address of the ring */

	void __iomem *bar2_addr;    /* address of BAR2 Queue registers */
	unsigned int bar2_qid;      /* Queue ID for BAR2 Queue registers */

	unsigned int cntxt_id;     /* SGE relative QID for the Tx Q */
	unsigned int in_use;       /* # of in-use Tx descriptors */
	unsigned int size;         /* # of descriptors */
	unsigned int cidx;         /* SW consumer index */
	unsigned int pidx;         /* producer index */
	unsigned int dbidx;	   /* last idx when db ring was done */
	unsigned int equeidx;	   /* last sent credit request */
	unsigned int last_pidx;	   /* last pidx recorded by tx monitor */
	unsigned int last_coal_idx;/* last coal-idx recorded by tx monitor */
	unsigned int abs_id;

	int db_disabled;            /* doorbell state */
	unsigned short db_pidx;     /* doorbell producer index */
	unsigned short db_pidx_inc; /* doorbell producer increment */
};

struct sge_eth_tx_stats {	/* Ethernet tx queue statistics */
	u64 pkts;		/* # of ethernet packets */
	u64 tx_bytes;		/* # of ethernet bytes */
	u64 tso;		/* # of TSO requests */
	u64 tx_cso;		/* # of Tx checksum offloads */
	u64 vlan_ins;		/* # of Tx VLAN insertions */
	u64 mapping_err;	/* # of I/O MMU packet mapping errors */
	u64 coal_wr;            /* # of coalesced wr */
	u64 coal_pkts;          /* # of coalesced packets */
};

struct sge_eth_txq {                   /* state for an SGE Ethernet Tx queue */
	struct sge_txq q;
	struct rte_eth_dev *eth_dev;   /* port that this queue belongs to */
	struct rte_eth_dev_data *data;
	struct sge_eth_tx_stats stats; /* queue statistics */
	rte_spinlock_t txq_lock;

	unsigned int flags;            /* flags for state of the queue */
} __rte_cache_aligned;

struct sge_ctrl_txq {                /* State for an SGE control Tx queue */
	struct sge_txq q;            /* txq */
	struct adapter *adapter;     /* adapter associated with this queue */
	rte_spinlock_t ctrlq_lock;   /* control queue lock */
	u8 full;                     /* the Tx ring is full */
	u64 txp;                     /* number of transmits */
	struct rte_mempool *mb_pool; /* mempool to generate ctrl pkts */
} __rte_cache_aligned;

struct sge {
	struct sge_eth_txq *ethtxq;
	struct sge_eth_rxq *ethrxq;
	struct sge_rspq fw_evtq __rte_cache_aligned;
	struct sge_ctrl_txq ctrlq[MAX_CTRL_QUEUES];

	u16 max_ethqsets;           /* # of available Ethernet queue sets */
	u32 stat_len;               /* length of status page at ring end */
	u32 pktshift;               /* padding between CPL & packet data */

	/* response queue interrupt parameters */
	u16 timer_val[SGE_NTIMERS];
	u8  counter_val[SGE_NCOUNTERS];

	u32 fl_starve_thres;        /* Free List starvation threshold */
	u32 fl_buffer_size[SGE_FL_BUFFER_SIZE_NUM]; /* Free List buffer sizes */
};

/*
 * OS Lock/List primitives for those interfaces in the Common Code which
 * need this.
 */

struct mbox_entry {
	TAILQ_ENTRY(mbox_entry) next;
};

TAILQ_HEAD(mbox_list, mbox_entry);

struct adapter_devargs {
	bool keep_ovlan;
	bool force_link_up;
	bool tx_mode_latency;
	u32 filtermode;
	u32 filtermask;
};

struct adapter {
	struct rte_pci_device *pdev;       /* associated rte pci device */
	struct rte_eth_dev *eth_dev;       /* first port's rte eth device */
	struct adapter_params params;      /* adapter parameters */
	struct port_info *port[MAX_NPORTS];/* ports belonging to this adapter */
	struct sge sge;                    /* associated SGE */

	/* support for single-threading access to adapter mailbox registers */
	struct mbox_list mbox_list;
	rte_spinlock_t mbox_lock;

	u8 *regs;              /* pointer to registers region */
	u8 *bar2;              /* pointer to bar2 region */
	unsigned long flags;   /* adapter flags */
	unsigned int mbox;     /* associated mailbox */
	unsigned int pf;       /* associated physical function id */

	unsigned int vpd_busy;
	unsigned int vpd_flag;

	int use_unpacked_mode; /* unpacked rx mode state */
	rte_spinlock_t win0_lock;

	rte_spinlock_t flow_lock; /* Serialize access for rte_flow ops */

	unsigned int clipt_start; /* CLIP table start */
	unsigned int clipt_end;   /* CLIP table end */
	unsigned int l2t_start;   /* Layer 2 table start */
	unsigned int l2t_end;     /* Layer 2 table end */
	struct clip_tbl *clipt;   /* CLIP table */
	struct l2t_data *l2t;     /* Layer 2 table */
	struct smt_data *smt;     /* Source mac table */
	struct mpstcam_table *mpstcam;

	struct tid_info tids;     /* Info used to access TID related tables */

	struct adapter_devargs devargs;
};

/**
 * t4_os_rwlock_init - initialize rwlock
 * @lock: the rwlock
 */
#define t4_os_rwlock_init(lock) rte_rwlock_init(lock)

/**
 * t4_os_write_lock - get a write lock
 * @lock: the rwlock
 */
#define t4_os_write_lock(lock) rte_rwlock_write_lock(lock)

/**
 * t4_os_write_unlock - unlock a write lock
 * @lock: the rwlock
 */
#define t4_os_write_unlock(lock) rte_rwlock_write_unlock(lock)

/**
 * ethdev2pinfo - return the port_info structure associated with a rte_eth_dev
 * @dev: the rte_eth_dev
 *
 * Return the struct port_info associated with a rte_eth_dev
 */
static inline struct port_info *ethdev2pinfo(const struct rte_eth_dev *dev)
{
	return dev->data->dev_private;
}

/**
 * adap2pinfo - return the port_info of a port
 * @adap: the adapter
 * @idx: the port index
 *
 * Return the port_info structure for the port of the given index.
 */
static inline struct port_info *adap2pinfo(const struct adapter *adap, int idx)
{
	return adap->port[idx];
}

/**
 * ethdev2adap - return the adapter structure associated with a rte_eth_dev
 * @dev: the rte_eth_dev
 *
 * Return the struct adapter associated with a rte_eth_dev
 */
static inline struct adapter *ethdev2adap(const struct rte_eth_dev *dev)
{
	return ethdev2pinfo(dev)->adapter;
}

#define CXGBE_PCI_REG(reg) rte_read32(reg)

static inline uint64_t cxgbe_read_addr64(volatile void *addr)
{
	uint64_t val = CXGBE_PCI_REG(addr);
	uint64_t val2 = CXGBE_PCI_REG(((volatile uint8_t *)(addr) + 4));

	val2 = (uint64_t)(val2 << 32);
	val += val2;
	return val;
}

static inline uint32_t cxgbe_read_addr(volatile void *addr)
{
	return CXGBE_PCI_REG(addr);
}

#define CXGBE_PCI_REG_ADDR(adap, reg) \
	((volatile uint32_t *)((char *)(adap)->regs + (reg)))

#define CXGBE_READ_REG(adap, reg) \
	cxgbe_read_addr(CXGBE_PCI_REG_ADDR((adap), (reg)))

#define CXGBE_READ_REG64(adap, reg) \
	cxgbe_read_addr64(CXGBE_PCI_REG_ADDR((adap), (reg)))

#define CXGBE_PCI_REG_WRITE(reg, value) rte_write32((value), (reg))

#define CXGBE_PCI_REG_WRITE_RELAXED(reg, value) \
	rte_write32_relaxed((value), (reg))

#define CXGBE_WRITE_REG(adap, reg, value) \
	CXGBE_PCI_REG_WRITE(CXGBE_PCI_REG_ADDR((adap), (reg)), (value))

#define CXGBE_WRITE_REG_RELAXED(adap, reg, value) \
	CXGBE_PCI_REG_WRITE_RELAXED(CXGBE_PCI_REG_ADDR((adap), (reg)), (value))

static inline uint64_t cxgbe_write_addr64(volatile void *addr, uint64_t val)
{
	CXGBE_PCI_REG_WRITE(addr, val);
	CXGBE_PCI_REG_WRITE(((volatile uint8_t *)(addr) + 4), (val >> 32));
	return val;
}

#define CXGBE_WRITE_REG64(adap, reg, value) \
	cxgbe_write_addr64(CXGBE_PCI_REG_ADDR((adap), (reg)), (value))

/**
 * t4_read_reg - read a HW register
 * @adapter: the adapter
 * @reg_addr: the register address
 *
 * Returns the 32-bit value of the given HW register.
 */
static inline u32 t4_read_reg(struct adapter *adapter, u32 reg_addr)
{
	return CXGBE_READ_REG(adapter, reg_addr);
}

/**
 * t4_write_reg - write a HW register with barrier
 * @adapter: the adapter
 * @reg_addr: the register address
 * @val: the value to write
 *
 * Write a 32-bit value into the given HW register.
 */
static inline void t4_write_reg(struct adapter *adapter, u32 reg_addr, u32 val)
{
	CXGBE_WRITE_REG(adapter, reg_addr, val);
}

/**
 * t4_write_reg_relaxed - write a HW register with no barrier
 * @adapter: the adapter
 * @reg_addr: the register address
 * @val: the value to write
 *
 * Write a 32-bit value into the given HW register.
 */
static inline void t4_write_reg_relaxed(struct adapter *adapter, u32 reg_addr,
					u32 val)
{
	CXGBE_WRITE_REG_RELAXED(adapter, reg_addr, val);
}

/**
 * t4_read_reg64 - read a 64-bit HW register
 * @adapter: the adapter
 * @reg_addr: the register address
 *
 * Returns the 64-bit value of the given HW register.
 */
static inline u64 t4_read_reg64(struct adapter *adapter, u32 reg_addr)
{
	return CXGBE_READ_REG64(adapter, reg_addr);
}

/**
 * t4_write_reg64 - write a 64-bit HW register
 * @adapter: the adapter
 * @reg_addr: the register address
 * @val: the value to write
 *
 * Write a 64-bit value into the given HW register.
 */
static inline void t4_write_reg64(struct adapter *adapter, u32 reg_addr,
				  u64 val)
{
	CXGBE_WRITE_REG64(adapter, reg_addr, val);
}

#define PCI_CAP_ID_EXP          RTE_PCI_CAP_ID_EXP
#define PCI_EXP_DEVCTL          0x0008  /* Device control */
#define PCI_EXP_DEVCTL2         40      /* Device Control 2 */
#define PCI_EXP_DEVCTL_EXT_TAG  0x0100  /* Extended Tag Field Enable */
#define PCI_EXP_DEVCTL_PAYLOAD  0x00E0  /* Max payload */
#define PCI_CAP_ID_VPD          0x03    /* Vital Product Data */
#define PCI_VPD_ADDR            2       /* Address to access (15 bits!) */
#define PCI_VPD_ADDR_F          0x8000  /* Write 0, 1 indicates completion */
#define PCI_VPD_DATA            4       /* 32-bits of data returned here */

/**
 * t4_os_pci_write_cfg4 - 32-bit write to PCI config space
 * @adapter: the adapter
 * @addr: the register address
 * @val: the value to write
 *
 * Write a 32-bit value into the given register in PCI config space.
 */
static inline void t4_os_pci_write_cfg4(struct adapter *adapter, size_t addr,
					off_t val)
{
	u32 val32 = val;

	if (rte_pci_write_config(adapter->pdev, &val32, sizeof(val32),
				     addr) < 0)
		dev_err(adapter, "Can't write to PCI config space\n");
}

/**
 * t4_os_pci_read_cfg4 - read a 32-bit value from PCI config space
 * @adapter: the adapter
 * @addr: the register address
 * @val: where to store the value read
 *
 * Read a 32-bit value from the given register in PCI config space.
 */
static inline void t4_os_pci_read_cfg4(struct adapter *adapter, size_t addr,
				       u32 *val)
{
	if (rte_pci_read_config(adapter->pdev, val, sizeof(*val),
				    addr) < 0)
		dev_err(adapter, "Can't read from PCI config space\n");
}

/**
 * t4_os_pci_write_cfg2 - 16-bit write to PCI config space
 * @adapter: the adapter
 * @addr: the register address
 * @val: the value to write
 *
 * Write a 16-bit value into the given register in PCI config space.
 */
static inline void t4_os_pci_write_cfg2(struct adapter *adapter, size_t addr,
					off_t val)
{
	u16 val16 = val;

	if (rte_pci_write_config(adapter->pdev, &val16, sizeof(val16),
				     addr) < 0)
		dev_err(adapter, "Can't write to PCI config space\n");
}

/**
 * t4_os_pci_read_cfg2 - read a 16-bit value from PCI config space
 * @adapter: the adapter
 * @addr: the register address
 * @val: where to store the value read
 *
 * Read a 16-bit value from the given register in PCI config space.
 */
static inline void t4_os_pci_read_cfg2(struct adapter *adapter, size_t addr,
				       u16 *val)
{
	if (rte_pci_read_config(adapter->pdev, val, sizeof(*val),
				    addr) < 0)
		dev_err(adapter, "Can't read from PCI config space\n");
}

/**
 * t4_os_pci_read_cfg - read a 8-bit value from PCI config space
 * @adapter: the adapter
 * @addr: the register address
 * @val: where to store the value read
 *
 * Read a 8-bit value from the given register in PCI config space.
 */
static inline void t4_os_pci_read_cfg(struct adapter *adapter, size_t addr,
				      u8 *val)
{
	if (rte_pci_read_config(adapter->pdev, val, sizeof(*val),
				    addr) < 0)
		dev_err(adapter, "Can't read from PCI config space\n");
}

/**
 * t4_os_find_pci_capability - lookup a capability in the PCI capability list
 * @adapter: the adapter
 * @cap: the capability
 *
 * Return the address of the given capability within the PCI capability list.
 */
static inline int t4_os_find_pci_capability(struct adapter *adapter, int cap)
{
	if (!rte_pci_has_capability_list(adapter->pdev)) {
		dev_err(adapter, "PCIe capability reading failed\n");
		return -1;
	}

	return rte_pci_find_capability(adapter->pdev, cap);
}

/**
 * t4_os_set_hw_addr - store a port's MAC address in SW
 * @adapter: the adapter
 * @port_idx: the port index
 * @hw_addr: the Ethernet address
 *
 * Store the Ethernet address of the given port in SW.  Called by the
 * common code when it retrieves a port's Ethernet address from EEPROM.
 */
static inline void t4_os_set_hw_addr(struct adapter *adapter, int port_idx,
				     u8 hw_addr[])
{
	struct port_info *pi = adap2pinfo(adapter, port_idx);

	rte_ether_addr_copy((struct rte_ether_addr *)hw_addr,
			&pi->eth_dev->data->mac_addrs[0]);
}

/**
 * t4_os_lock_init - initialize spinlock
 * @lock: the spinlock
 */
#define t4_os_lock_init(lock) rte_spinlock_init(lock)

/**
 * t4_os_lock - spin until lock is acquired
 * @lock: the spinlock
 */
#define t4_os_lock(lock) rte_spinlock_lock(lock)

/**
 * t4_os_unlock - unlock a spinlock
 * @lock: the spinlock
 */
#define t4_os_unlock(lock) rte_spinlock_unlock(lock)

/**
 * t4_os_trylock - try to get a lock
 * @lock: the spinlock
 */
#define t4_os_trylock(lock) rte_spinlock_trylock(lock)

/**
 * t4_os_init_list_head - initialize
 * @head: head of list to initialize [to empty]
 */
static inline void t4_os_init_list_head(struct mbox_list *head)
{
	TAILQ_INIT(head);
}

static inline struct mbox_entry *t4_os_list_first_entry(struct mbox_list *head)
{
	return TAILQ_FIRST(head);
}

/**
 * t4_os_atomic_add_tail - Enqueue list element atomically onto list
 * @new: the entry to be addded to the queue
 * @head: current head of the linked list
 * @lock: lock to use to guarantee atomicity
 */
static inline void t4_os_atomic_add_tail(struct mbox_entry *entry,
					 struct mbox_list *head,
					 rte_spinlock_t *lock)
{
	t4_os_lock(lock);
	TAILQ_INSERT_TAIL(head, entry, next);
	t4_os_unlock(lock);
}

/**
 * t4_os_atomic_list_del - Dequeue list element atomically from list
 * @entry: the entry to be remove/dequeued from the list.
 * @lock: the spinlock
 */
static inline void t4_os_atomic_list_del(struct mbox_entry *entry,
					 struct mbox_list *head,
					 rte_spinlock_t *lock)
{
	t4_os_lock(lock);
	TAILQ_REMOVE(head, entry, next);
	t4_os_unlock(lock);
}

/**
 * t4_init_completion - initialize completion
 * @c: the completion context
 */
static inline void t4_init_completion(struct t4_completion *c)
{
	c->done = 0;
	t4_os_lock_init(&c->lock);
}

/**
 * t4_complete - set completion as done
 * @c: the completion context
 */
static inline void t4_complete(struct t4_completion *c)
{
	t4_os_lock(&c->lock);
	c->done = 1;
	t4_os_unlock(&c->lock);
}

/**
 * cxgbe_port_viid - get the VI id of a port
 * @dev: the device for the port
 *
 * Return the VI id of the given port.
 */
static inline unsigned int cxgbe_port_viid(const struct rte_eth_dev *dev)
{
	return ethdev2pinfo(dev)->viid;
}

void *t4_alloc_mem(size_t size);
void t4_free_mem(void *addr);
#define t4_os_alloc(_size)     t4_alloc_mem((_size))
#define t4_os_free(_ptr)       t4_free_mem((_ptr))

void t4_os_portmod_changed(const struct adapter *adap, int port_id);
void t4_os_link_changed(struct adapter *adap, int port_id);

void reclaim_completed_tx(struct sge_txq *q);
void t4_free_sge_resources(struct adapter *adap);
void t4_sge_tx_monitor_start(struct adapter *adap);
void t4_sge_tx_monitor_stop(struct adapter *adap);
int t4_eth_xmit(struct sge_eth_txq *txq, struct rte_mbuf *mbuf,
		uint16_t nb_pkts);
int t4_mgmt_tx(struct sge_ctrl_txq *txq, struct rte_mbuf *mbuf);
int t4_sge_init(struct adapter *adap);
int t4vf_sge_init(struct adapter *adap);
int t4_sge_alloc_eth_txq(struct adapter *adap, struct sge_eth_txq *txq,
			 struct rte_eth_dev *eth_dev, uint16_t queue_id,
			 unsigned int iqid, int socket_id);
int t4_sge_alloc_ctrl_txq(struct adapter *adap, struct sge_ctrl_txq *txq,
			  struct rte_eth_dev *eth_dev, uint16_t queue_id,
			  unsigned int iqid, int socket_id);
int t4_sge_alloc_rxq(struct adapter *adap, struct sge_rspq *rspq, bool fwevtq,
		     struct rte_eth_dev *eth_dev, int intr_idx,
		     struct sge_fl *fl, rspq_handler_t handler,
		     int cong, struct rte_mempool *mp, int queue_id,
		     int socket_id);
int t4_sge_eth_txq_start(struct sge_eth_txq *txq);
int t4_sge_eth_txq_stop(struct sge_eth_txq *txq);
void t4_sge_eth_txq_release(struct adapter *adap, struct sge_eth_txq *txq);
int t4_sge_eth_rxq_start(struct adapter *adap, struct sge_eth_rxq *rxq);
int t4_sge_eth_rxq_stop(struct adapter *adap, struct sge_eth_rxq *rxq);
void t4_sge_eth_rxq_release(struct adapter *adap, struct sge_eth_rxq *rxq);
void t4_sge_eth_clear_queues(struct port_info *pi);
void t4_sge_eth_release_queues(struct port_info *pi);
int cxgb4_set_rspq_intr_params(struct sge_rspq *q, unsigned int us,
			       unsigned int cnt);
int cxgbe_poll(struct sge_rspq *q, struct rte_mbuf **rx_pkts,
	       unsigned int budget, unsigned int *work_done);
int cxgbe_write_rss(const struct port_info *pi, const u16 *queues);
int cxgbe_write_rss_conf(const struct port_info *pi, uint64_t flags);

#endif /* __T4_ADAPTER_H__ */
