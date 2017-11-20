/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _LIO_STRUCT_H_
#define _LIO_STRUCT_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_spinlock.h>
#include <rte_atomic.h>

#include "lio_hw_defs.h"

struct lio_stailq_node {
	STAILQ_ENTRY(lio_stailq_node) entries;
};

STAILQ_HEAD(lio_stailq_head, lio_stailq_node);

struct lio_version {
	uint16_t major;
	uint16_t minor;
	uint16_t micro;
	uint16_t reserved;
};

/** Input Queue statistics. Each input queue has four stats fields. */
struct lio_iq_stats {
	uint64_t instr_posted; /**< Instructions posted to this queue. */
	uint64_t instr_processed; /**< Instructions processed in this queue. */
	uint64_t instr_dropped; /**< Instructions that could not be processed */
	uint64_t bytes_sent; /**< Bytes sent through this queue. */
	uint64_t tx_done; /**< Num of packets sent to network. */
	uint64_t tx_iq_busy; /**< Num of times this iq was found to be full. */
	uint64_t tx_dropped; /**< Num of pkts dropped due to xmitpath errors. */
	uint64_t tx_tot_bytes; /**< Total count of bytes sent to network. */
};

/** Output Queue statistics. Each output queue has four stats fields. */
struct lio_droq_stats {
	/** Number of packets received in this queue. */
	uint64_t pkts_received;

	/** Bytes received by this queue. */
	uint64_t bytes_received;

	/** Packets dropped due to no memory available. */
	uint64_t dropped_nomem;

	/** Packets dropped due to large number of pkts to process. */
	uint64_t dropped_toomany;

	/** Number of packets  sent to stack from this queue. */
	uint64_t rx_pkts_received;

	/** Number of Bytes sent to stack from this queue. */
	uint64_t rx_bytes_received;

	/** Num of Packets dropped due to receive path failures. */
	uint64_t rx_dropped;

	/** Num of vxlan packets received; */
	uint64_t rx_vxlan;

	/** Num of failures of rte_pktmbuf_alloc() */
	uint64_t rx_alloc_failure;

};

/** The Descriptor Ring Output Queue structure.
 *  This structure has all the information required to implement a
 *  DROQ.
 */
struct lio_droq {
	/** A spinlock to protect access to this ring. */
	rte_spinlock_t lock;

	uint32_t q_no;

	uint32_t pkt_count;

	struct lio_device *lio_dev;

	/** The 8B aligned descriptor ring starts at this address. */
	struct lio_droq_desc *desc_ring;

	/** Index in the ring where the driver should read the next packet */
	uint32_t read_idx;

	/** Index in the ring where Octeon will write the next packet */
	uint32_t write_idx;

	/** Index in the ring where the driver will refill the descriptor's
	 * buffer
	 */
	uint32_t refill_idx;

	/** Packets pending to be processed */
	rte_atomic64_t pkts_pending;

	/** Number of  descriptors in this ring. */
	uint32_t nb_desc;

	/** The number of descriptors pending refill. */
	uint32_t refill_count;

	uint32_t refill_threshold;

	/** The 8B aligned info ptrs begin from this address. */
	struct lio_droq_info *info_list;

	/** The receive buffer list. This list has the virtual addresses of the
	 *  buffers.
	 */
	struct lio_recv_buffer *recv_buf_list;

	/** The size of each buffer pointed by the buffer pointer. */
	uint32_t buffer_size;

	/** Pointer to the mapped packet credit register.
	 *  Host writes number of info/buffer ptrs available to this register
	 */
	void *pkts_credit_reg;

	/** Pointer to the mapped packet sent register.
	 *  Octeon writes the number of packets DMA'ed to host memory
	 *  in this register.
	 */
	void *pkts_sent_reg;

	/** Statistics for this DROQ. */
	struct lio_droq_stats stats;

	/** DMA mapped address of the DROQ descriptor ring. */
	size_t desc_ring_dma;

	/** Info ptr list are allocated at this virtual address. */
	size_t info_base_addr;

	/** DMA mapped address of the info list */
	size_t info_list_dma;

	/** Allocated size of info list. */
	uint32_t info_alloc_size;

	/** Memory zone **/
	const struct rte_memzone *desc_ring_mz;
	const struct rte_memzone *info_mz;
	struct rte_mempool *mpool;
};

/** Receive Header */
union octeon_rh {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint64_t rh64;
	struct	{
		uint64_t opcode : 4;
		uint64_t subcode : 8;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t reserved : 17;
		uint64_t ossp : 32; /** opcode/subcode specific parameters */
	} r;
	struct	{
		uint64_t opcode : 4;
		uint64_t subcode : 8;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t extra : 28;
		uint64_t vlan : 12;
		uint64_t priority : 3;
		uint64_t csum_verified : 3; /** checksum verified. */
		uint64_t has_hwtstamp : 1; /** Has hardware timestamp.1 = yes.*/
		uint64_t encap_on : 1;
		uint64_t has_hash : 1; /** Has hash (rth or rss). 1 = yes. */
	} r_dh;
	struct {
		uint64_t opcode : 4;
		uint64_t subcode : 8;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t reserved : 8;
		uint64_t extra : 25;
		uint64_t gmxport : 16;
	} r_nic_info;
#else
	uint64_t rh64;
	struct {
		uint64_t ossp : 32; /** opcode/subcode specific parameters */
		uint64_t reserved : 17;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t subcode : 8;
		uint64_t opcode : 4;
	} r;
	struct {
		uint64_t has_hash : 1; /** Has hash (rth or rss). 1 = yes. */
		uint64_t encap_on : 1;
		uint64_t has_hwtstamp : 1;  /** 1 = has hwtstamp */
		uint64_t csum_verified : 3; /** checksum verified. */
		uint64_t priority : 3;
		uint64_t vlan : 12;
		uint64_t extra : 28;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t subcode : 8;
		uint64_t opcode : 4;
	} r_dh;
	struct {
		uint64_t gmxport : 16;
		uint64_t extra : 25;
		uint64_t reserved : 8;
		uint64_t len : 3; /** additional 64-bit words */
		uint64_t subcode : 8;
		uint64_t opcode : 4;
	} r_nic_info;
#endif
};

#define OCTEON_RH_SIZE (sizeof(union octeon_rh))

/** The txpciq info passed to host from the firmware */
union octeon_txpciq {
	uint64_t txpciq64;

	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t q_no : 8;
		uint64_t port : 8;
		uint64_t pkind : 6;
		uint64_t use_qpg : 1;
		uint64_t qpg : 11;
		uint64_t aura_num : 10;
		uint64_t reserved : 20;
#else
		uint64_t reserved : 20;
		uint64_t aura_num : 10;
		uint64_t qpg : 11;
		uint64_t use_qpg : 1;
		uint64_t pkind : 6;
		uint64_t port : 8;
		uint64_t q_no : 8;
#endif
	} s;
};

/** The instruction (input) queue.
 *  The input queue is used to post raw (instruction) mode data or packet
 *  data to Octeon device from the host. Each input queue for
 *  a LIO device has one such structure to represent it.
 */
struct lio_instr_queue {
	/** A spinlock to protect access to the input ring.  */
	rte_spinlock_t lock;

	rte_spinlock_t post_lock;

	struct lio_device *lio_dev;

	uint32_t pkt_in_done;

	rte_atomic64_t iq_flush_running;

	/** Flag that indicates if the queue uses 64 byte commands. */
	uint32_t iqcmd_64B:1;

	/** Queue info. */
	union octeon_txpciq txpciq;

	uint32_t rsvd:17;

	uint32_t status:8;

	/** Number of  descriptors in this ring. */
	uint32_t nb_desc;

	/** Index in input ring where the driver should write the next packet */
	uint32_t host_write_index;

	/** Index in input ring where Octeon is expected to read the next
	 *  packet.
	 */
	uint32_t lio_read_index;

	/** This index aids in finding the window in the queue where Octeon
	 *  has read the commands.
	 */
	uint32_t flush_index;

	/** This field keeps track of the instructions pending in this queue. */
	rte_atomic64_t instr_pending;

	/** Pointer to the Virtual Base addr of the input ring. */
	uint8_t *base_addr;

	struct lio_request_list *request_list;

	/** Octeon doorbell register for the ring. */
	void *doorbell_reg;

	/** Octeon instruction count register for this ring. */
	void *inst_cnt_reg;

	/** Number of instructions pending to be posted to Octeon. */
	uint32_t fill_cnt;

	/** Statistics for this input queue. */
	struct lio_iq_stats stats;

	/** DMA mapped base address of the input descriptor ring. */
	uint64_t base_addr_dma;

	/** Application context */
	void *app_ctx;

	/* network stack queue index */
	int q_index;

	/* Memory zone */
	const struct rte_memzone *iq_mz;
};

/** This structure is used by driver to store information required
 *  to free the mbuff when the packet has been fetched by Octeon.
 *  Bytes offset below assume worst-case of a 64-bit system.
 */
struct lio_buf_free_info {
	/** Bytes 1-8. Pointer to network device private structure. */
	struct lio_device *lio_dev;

	/** Bytes 9-16. Pointer to mbuff. */
	struct rte_mbuf *mbuf;

	/** Bytes 17-24. Pointer to gather list. */
	struct lio_gather *g;

	/** Bytes 25-32. Physical address of mbuf->data or gather list. */
	uint64_t dptr;

	/** Bytes 33-47. Piggybacked soft command, if any */
	struct lio_soft_command *sc;

	/** Bytes 48-63. iq no */
	uint64_t iq_no;
};

/* The Scatter-Gather List Entry. The scatter or gather component used with
 * input instruction has this format.
 */
struct lio_sg_entry {
	/** The first 64 bit gives the size of data in each dptr. */
	union {
		uint16_t size[4];
		uint64_t size64;
	} u;

	/** The 4 dptr pointers for this entry. */
	uint64_t ptr[4];
};

#define LIO_SG_ENTRY_SIZE	(sizeof(struct lio_sg_entry))

/** Structure of a node in list of gather components maintained by
 *  driver for each network device.
 */
struct lio_gather {
	/** List manipulation. Next and prev pointers. */
	struct lio_stailq_node list;

	/** Size of the gather component at sg in bytes. */
	int sg_size;

	/** Number of bytes that sg was adjusted to make it 8B-aligned. */
	int adjust;

	/** Gather component that can accommodate max sized fragment list
	 *  received from the IP layer.
	 */
	struct lio_sg_entry *sg;
};

struct lio_rss_ctx {
	uint16_t hash_key_size;
	uint8_t  hash_key[LIO_RSS_MAX_KEY_SZ];
	/* Ideally a factor of number of queues */
	uint8_t  itable[LIO_RSS_MAX_TABLE_SZ];
	uint8_t  itable_size;
	uint8_t  ip;
	uint8_t  tcp_hash;
	uint8_t  ipv6;
	uint8_t  ipv6_tcp_hash;
	uint8_t  ipv6_ex;
	uint8_t  ipv6_tcp_ex_hash;
	uint8_t  hash_disable;
};

struct lio_io_enable {
	uint64_t iq;
	uint64_t oq;
	uint64_t iq64B;
};

struct lio_fn_list {
	void (*setup_iq_regs)(struct lio_device *, uint32_t);
	void (*setup_oq_regs)(struct lio_device *, uint32_t);

	int (*setup_mbox)(struct lio_device *);
	void (*free_mbox)(struct lio_device *);

	int (*setup_device_regs)(struct lio_device *);
	int (*enable_io_queues)(struct lio_device *);
	void (*disable_io_queues)(struct lio_device *);
};

struct lio_pf_vf_hs_word {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/** PKIND value assigned for the DPI interface */
	uint64_t pkind : 8;

	/** OCTEON core clock multiplier */
	uint64_t core_tics_per_us : 16;

	/** OCTEON coprocessor clock multiplier */
	uint64_t coproc_tics_per_us : 16;

	/** app that currently running on OCTEON */
	uint64_t app_mode : 8;

	/** RESERVED */
	uint64_t reserved : 16;

#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN

	/** RESERVED */
	uint64_t reserved : 16;

	/** app that currently running on OCTEON */
	uint64_t app_mode : 8;

	/** OCTEON coprocessor clock multiplier */
	uint64_t coproc_tics_per_us : 16;

	/** OCTEON core clock multiplier */
	uint64_t core_tics_per_us : 16;

	/** PKIND value assigned for the DPI interface */
	uint64_t pkind : 8;
#endif
};

struct lio_sriov_info {
	/** Number of rings assigned to VF */
	uint32_t rings_per_vf;

	/** Number of VF devices enabled */
	uint32_t num_vfs;
};

/* Head of a response list */
struct lio_response_list {
	/** List structure to add delete pending entries to */
	struct lio_stailq_head head;

	/** A lock for this response list */
	rte_spinlock_t lock;

	rte_atomic64_t pending_req_count;
};

/* Structure to define the configuration attributes for each Input queue. */
struct lio_iq_config {
	/* Max number of IQs available */
	uint8_t max_iqs;

	/** Pending list size (usually set to the sum of the size of all Input
	 *  queues)
	 */
	uint32_t pending_list_size;

	/** Command size - 32 or 64 bytes */
	uint32_t instr_type;
};

/* Structure to define the configuration attributes for each Output queue. */
struct lio_oq_config {
	/* Max number of OQs available */
	uint8_t max_oqs;

	/** If set, the Output queue uses info-pointer mode. (Default: 1 ) */
	uint32_t info_ptr;

	/** The number of buffers that were consumed during packet processing by
	 *  the driver on this Output queue before the driver attempts to
	 *  replenish the descriptor ring with new buffers.
	 */
	uint32_t refill_threshold;
};

/* Structure to define the configuration. */
struct lio_config {
	uint16_t card_type;
	const char *card_name;

	/** Input Queue attributes. */
	struct lio_iq_config iq;

	/** Output Queue attributes. */
	struct lio_oq_config oq;

	int num_nic_ports;

	int num_def_tx_descs;

	/* Num of desc for rx rings */
	int num_def_rx_descs;

	int def_rx_buf_size;
};

/** Status of a RGMII Link on Octeon as seen by core driver. */
union octeon_link_status {
	uint64_t link_status64;

	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t duplex : 8;
		uint64_t mtu : 16;
		uint64_t speed : 16;
		uint64_t link_up : 1;
		uint64_t autoneg : 1;
		uint64_t if_mode : 5;
		uint64_t pause : 1;
		uint64_t flashing : 1;
		uint64_t reserved : 15;
#else
		uint64_t reserved : 15;
		uint64_t flashing : 1;
		uint64_t pause : 1;
		uint64_t if_mode : 5;
		uint64_t autoneg : 1;
		uint64_t link_up : 1;
		uint64_t speed : 16;
		uint64_t mtu : 16;
		uint64_t duplex : 8;
#endif
	} s;
};

/** The rxpciq info passed to host from the firmware */
union octeon_rxpciq {
	uint64_t rxpciq64;

	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t q_no : 8;
		uint64_t reserved : 56;
#else
		uint64_t reserved : 56;
		uint64_t q_no : 8;
#endif
	} s;
};

/** Information for a OCTEON ethernet interface shared between core & host. */
struct octeon_link_info {
	union octeon_link_status link;
	uint64_t hw_addr;

#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint64_t gmxport : 16;
	uint64_t macaddr_is_admin_assigned : 1;
	uint64_t vlan_is_admin_assigned : 1;
	uint64_t rsvd : 30;
	uint64_t num_txpciq : 8;
	uint64_t num_rxpciq : 8;
#else
	uint64_t num_rxpciq : 8;
	uint64_t num_txpciq : 8;
	uint64_t rsvd : 30;
	uint64_t vlan_is_admin_assigned : 1;
	uint64_t macaddr_is_admin_assigned : 1;
	uint64_t gmxport : 16;
#endif

	union octeon_txpciq txpciq[LIO_MAX_IOQS_PER_IF];
	union octeon_rxpciq rxpciq[LIO_MAX_IOQS_PER_IF];
};

/* -----------------------  THE LIO DEVICE  --------------------------- */
/** The lio device.
 *  Each lio device has this structure to represent all its
 *  components.
 */
struct lio_device {
	/** PCI device pointer */
	struct rte_pci_device *pci_dev;

	/** Octeon Chip type */
	uint16_t chip_id;
	uint16_t pf_num;
	uint16_t vf_num;

	/** This device's PCIe port used for traffic. */
	uint16_t pcie_port;

	/** The state of this device */
	rte_atomic64_t status;

	uint8_t intf_open;

	struct octeon_link_info linfo;

	uint8_t *hw_addr;

	struct lio_fn_list fn_list;

	uint32_t num_iqs;

	/** Guards each glist */
	rte_spinlock_t *glist_lock;
	/** Array of gather component linked lists */
	struct lio_stailq_head *glist_head;

	/* The pool containing pre allocated buffers used for soft commands */
	struct rte_mempool *sc_buf_pool;

	/** The input instruction queues */
	struct lio_instr_queue *instr_queue[LIO_MAX_POSSIBLE_INSTR_QUEUES];

	/** The singly-linked tail queues of instruction response */
	struct lio_response_list response_list;

	uint32_t num_oqs;

	/** The DROQ output queues  */
	struct lio_droq *droq[LIO_MAX_POSSIBLE_OUTPUT_QUEUES];

	struct lio_io_enable io_qmask;

	struct lio_sriov_info sriov_info;

	struct lio_pf_vf_hs_word pfvf_hsword;

	/** Mail Box details of each lio queue. */
	struct lio_mbox **mbox;

	char dev_string[LIO_DEVICE_NAME_LEN]; /* Device print string */

	const struct lio_config *default_config;

	struct rte_eth_dev      *eth_dev;

	uint64_t ifflags;
	uint8_t max_rx_queues;
	uint8_t max_tx_queues;
	uint8_t nb_rx_queues;
	uint8_t nb_tx_queues;
	uint8_t port_configured;
	struct lio_rss_ctx rss_state;
	uint16_t port_id;
	char firmware_version[LIO_FW_VERSION_LENGTH];
};
#endif /* _LIO_STRUCT_H_ */
