/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */
#ifndef __BBDEV_LA12XX_IPC_H__
#define __BBDEV_LA12XX_IPC_H__

#define LA12XX_MAX_QUEUES 20
#define HOST_RX_QUEUEID_OFFSET LA12XX_MAX_QUEUES

/** No. of max channel per instance */
#define IPC_MAX_CHANNEL_COUNT	(64)

/** No. of max channel per instance */
#define IPC_MAX_DEPTH	(16)

/** No. of max IPC instance per modem */
#define IPC_MAX_INSTANCE_COUNT	(1)

/** Error codes */
#define IPC_SUCCESS		(0) /** IPC operation success */
#define IPC_INPUT_INVALID	(-1) /** Invalid input to API */
#define IPC_CH_INVALID		(-2) /** Channel no is invalid */
#define IPC_INSTANCE_INVALID	(-3) /** Instance no is invalid */
#define IPC_MEM_INVALID		(-4) /** Insufficient memory */
#define IPC_CH_FULL		(-5) /** Channel is full */
#define IPC_CH_EMPTY		(-6) /** Channel is empty */
#define IPC_BL_EMPTY		(-7) /** Free buffer list is empty */
#define IPC_BL_FULL		(-8) /** Free buffer list is full */
#define IPC_HOST_BUF_ALLOC_FAIL	(-9) /** DPDK malloc fail */
#define IPC_MD_SZ_MISS_MATCH	(-10) /** META DATA size in mhif miss matched*/
#define IPC_MALLOC_FAIL		(-11) /** system malloc fail */
#define IPC_IOCTL_FAIL		(-12) /** IOCTL call failed */
#define IPC_MMAP_FAIL		(-14) /** MMAP fail */
#define IPC_OPEN_FAIL		(-15) /** OPEN fail */
#define IPC_EVENTFD_FAIL	(-16) /** eventfd initialization failed */
#define IPC_NOT_IMPLEMENTED	(-17) /** IPC feature is not implemented yet*/

#define SET_HIF_HOST_RDY(hif, RDY_MASK) (hif->host_ready |= RDY_MASK)
#define CHK_HIF_MOD_RDY(hif, RDY_MASK) (hif->mod_ready & RDY_MASK)

/* Host Ready bits */
#define HIF_HOST_READY_HOST_REGIONS	(1 << 0)
#define HIF_HOST_READY_IPC_LIB		(1 << 12)
#define HIF_HOST_READY_IPC_APP		(1 << 13)
#define HIF_HOST_READY_FECA		(1 << 14)

/* Modem Ready bits */
#define HIF_MOD_READY_IPC_LIB		(1 << 5)
#define HIF_MOD_READY_IPC_APP		(1 << 6)
#define HIF_MOD_READY_FECA		(1 << 7)

typedef void *ipc_t;

struct ipc_msg {
	int chid;
	void *addr;
	uint32_t len;
	uint8_t flags;
};

typedef struct {
	uint64_t host_phys;
	uint32_t modem_phys;
	void    *host_vaddr;
	uint32_t size;
} mem_range_t;

#define GUL_IPC_MAGIC	'R'

#define IOCTL_GUL_IPC_GET_SYS_MAP _IOW(GUL_IPC_MAGIC, 1, struct ipc_msg *)
#define IOCTL_GUL_IPC_CHANNEL_REGISTER _IOWR(GUL_IPC_MAGIC, 4, struct ipc_msg *)
#define IOCTL_GUL_IPC_CHANNEL_DEREGISTER \
	_IOWR(GUL_IPC_MAGIC, 5, struct ipc_msg *)
#define IOCTL_GUL_IPC_CHANNEL_RAISE_INTERRUPT _IOW(GUL_IPC_MAGIC, 6, int *)

#define GUL_USER_HUGE_PAGE_OFFSET	(0)
#define GUL_PCI1_ADDR_BASE	(0x00000000ULL)

#define GUL_USER_HUGE_PAGE_ADDR	(GUL_PCI1_ADDR_BASE + GUL_USER_HUGE_PAGE_OFFSET)

/* IPC PI/CI index & flag manipulation helpers */
#define IPC_PI_CI_FLAG_MASK	0x80000000 /*  (1<<31) */
#define IPC_PI_CI_INDEX_MASK	0x7FFFFFFF /* ~(1<<31) */

#define IPC_SET_PI_FLAG(x)	(x |= IPC_PI_CI_FLAG_MASK)
#define IPC_RESET_PI_FLAG(x)	(x &= IPC_PI_CI_INDEX_MASK)
#define IPC_GET_PI_FLAG(x)	(x >> 31)
#define IPC_GET_PI_INDEX(x)	(x & IPC_PI_CI_INDEX_MASK)

#define IPC_SET_CI_FLAG(x)	(x |= IPC_PI_CI_FLAG_MASK)
#define IPC_RESET_CI_FLAG(x)	(x &= IPC_PI_CI_INDEX_MASK)
#define IPC_GET_CI_FLAG(x)	(x >> 31)
#define IPC_GET_CI_INDEX(x)	(x & IPC_PI_CI_INDEX_MASK)

/** buffer ring common metadata */
typedef struct ipc_bd_ring_md {
	volatile uint32_t pi;		/**< Producer index and flag (MSB)
					  *  which flip for each Ring wrapping
					  */
	volatile uint32_t ci;		/**< Consumer index and flag (MSB)
					  *  which flip for each Ring wrapping
					  */
	uint32_t ring_size;	/**< depth (Used to roll-over pi/ci) */
	uint32_t msg_size;	/**< Size of the each buffer */
} __rte_packed ipc_br_md_t;

/** IPC buffer descriptor */
typedef struct ipc_buffer_desc {
	union {
		uint64_t host_virt;	/**< msg's host virtual address */
		struct {
			uint32_t host_virt_l;
			uint32_t host_virt_h;
		};
	};
	uint32_t modem_ptr;	/**< msg's modem physical address */
	uint32_t len;		/**< msg len */
} __rte_packed ipc_bd_t;

typedef struct ipc_channel {
	uint32_t ch_id;		/**< Channel id */
	ipc_br_md_t md;			/**< Metadata for BD ring */
	ipc_bd_t bd_h[IPC_MAX_DEPTH];	/**< Buffer Descriptor on Host */
	ipc_bd_t bd_m[IPC_MAX_DEPTH];	/**< Buffer Descriptor on Modem */
	uint32_t op_type;		/**< Type of the BBDEV operation
					  * supported on this channel
					  */
	uint32_t depth;			/**< Channel depth */
	uint32_t feca_blk_id;	/**< FECA Transport Block ID for processing */
	uint32_t la12xx_core_id;/**< LA12xx core ID on which this will be
				  * scheduled
				  */
	uint32_t feca_input_circ_size;	/**< FECA transport block input
					  * circular buffer size
					  */
	uint32_t host_ipc_params;	/**< Address for host IPC parameters */
} __rte_packed ipc_ch_t;

typedef struct ipc_instance {
	uint32_t instance_id;		/**< instance id, use to init this
					  * instance by ipc_init API
					  */
	uint32_t initialized;		/**< Set in ipc_init */
	ipc_ch_t ch_list[IPC_MAX_CHANNEL_COUNT];
		/**< Channel descriptors in this instance */
} __rte_packed ipc_instance_t;

typedef struct ipc_metadata {
	uint32_t ipc_host_signature; /**< IPC host signature, Set by host/L2 */
	uint32_t ipc_geul_signature; /**< IPC geul signature, Set by modem */
	ipc_instance_t instance_list[IPC_MAX_INSTANCE_COUNT];
} __rte_packed ipc_metadata_t;

typedef struct ipc_channel_us_priv {
	int32_t		eventfd;
	uint32_t	channel_id;
	/* In flight packets status for buffer list. */
	uint8_t		bufs_inflight[IPC_MAX_DEPTH];
} ipc_channel_us_t;

typedef struct {
	uint64_t host_phys;
	uint32_t modem_phys;
	uint32_t size;
} mem_strt_addr_t;

typedef struct {
	mem_strt_addr_t modem_ccsrbar;
	mem_strt_addr_t peb_start; /* PEB meta data */
	mem_strt_addr_t mhif_start; /* MHIF meta daat */
	mem_strt_addr_t hugepg_start; /* Modem to access hugepage */
} sys_map_t;

typedef struct ipc_priv_t {
	int instance_id;
	int dev_ipc;
	int dev_mem;
	sys_map_t sys_map;
	mem_range_t modem_ccsrbar;
	mem_range_t peb_start;
	mem_range_t mhif_start;
	mem_range_t hugepg_start;
	ipc_channel_us_t *channels[IPC_MAX_CHANNEL_COUNT];
	ipc_instance_t	*instance;
	ipc_instance_t	*instance_bk;
} ipc_userspace_t;

/** Structure specifying enqueue operation (enqueue at LA1224) */
struct bbdev_ipc_enqueue_op {
	/** Status of operation that was performed */
	int32_t status;
	/** CRC Status of SD operation that was performed */
	int32_t crc_stat_addr;
	/** HARQ Output buffer memory length for Shared Decode.
	 * Filled by LA12xx.
	 */
	uint32_t out_len;
	/** Reserved (for 8 byte alignment) */
	uint32_t rsvd;
};

/** Structure specifying dequeue operation (dequeue at LA1224) */
struct bbdev_ipc_dequeue_op {
	/** Input buffer memory address */
	uint32_t in_addr;
	/** Input buffer memory length */
	uint32_t in_len;
	/** Output buffer memory address */
	uint32_t out_addr;
	/** Output buffer memory length */
	uint32_t out_len;
	/* Number of code blocks. Only set when HARQ is used */
	uint32_t num_code_blocks;
	/** Dequeue Operation flags */
	uint32_t op_flags;
	/** Shared metadata between L1 and L2 */
	uint32_t shared_metadata;
};

/* This shared memory would be on the host side which have copy of some
 * of the parameters which are also part of Shared BD ring. Read access
 * of these parameters from the host side would not be over PCI.
 */
typedef struct host_ipc_params {
	volatile uint32_t pi;
	volatile uint32_t ci;
	volatile uint32_t bd_m_modem_ptr[IPC_MAX_DEPTH];
} __rte_packed host_ipc_params_t;

struct hif_ipc_regs {
	uint32_t ipc_mdata_offset;
	uint32_t ipc_mdata_size;
} __rte_packed;

struct gul_hif {
	uint32_t ver;
	uint32_t hif_ver;
	uint32_t status;
	volatile uint32_t host_ready;
	volatile uint32_t mod_ready;
	struct hif_ipc_regs ipc_regs;
} __rte_packed;

#endif
