/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_RXTX_H_
#define _IAVF_RXTX_H_

/* In QLEN must be whole number of 32 descriptors. */
#define IAVF_ALIGN_RING_DESC      32
#define IAVF_MIN_RING_DESC        64
#define IAVF_MAX_RING_DESC        4096
#define IAVF_DMA_MEM_ALIGN        4096
/* Base address of the HW descriptor ring should be 128B aligned. */
#define IAVF_RING_BASE_ALIGN      128

/* used for Rx Bulk Allocate */
#define IAVF_RX_MAX_BURST         32

/* Max data buffer size must be 16K - 128 bytes */
#define IAVF_RX_MAX_DATA_BUF_SIZE (16 * 1024 - 128)

/* used for Vector PMD */
#define IAVF_VPMD_RX_MAX_BURST    32
#define IAVF_VPMD_TX_MAX_BURST    32
#define IAVF_RXQ_REARM_THRESH     32
#define IAVF_VPMD_DESCS_PER_LOOP  4
#define IAVF_VPMD_TX_MAX_FREE_BUF 64

#define IAVF_TX_NO_VECTOR_FLAGS (				 \
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |		 \
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |		 \
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |		 \
		RTE_ETH_TX_OFFLOAD_TCP_TSO |		 \
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |	 \
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |	 \
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |	 \
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |	 \
		RTE_ETH_TX_OFFLOAD_SECURITY)

#define IAVF_TX_VECTOR_OFFLOAD (				 \
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |		 \
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |		 \
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |		 \
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM)

#define IAVF_TX_VECTOR_OFFLOAD_CTX (			\
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |	\
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)

#define IAVF_RX_VECTOR_OFFLOAD (				 \
		RTE_ETH_RX_OFFLOAD_CHECKSUM |		 \
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |		 \
		RTE_ETH_RX_OFFLOAD_VLAN |		 \
		RTE_ETH_RX_OFFLOAD_RSS_HASH |    \
		RTE_ETH_RX_OFFLOAD_TIMESTAMP)

/**
 * According to the vlan capabilities returned by the driver and FW, the vlan tci
 * needs to be inserted to the L2TAG1 or L2TAG2 fields.
 * If L2TAG1, it should be inserted to the L2TAG1 field in data desc.
 * If L2TAG2, it should be inserted to the L2TAG2 field in ctx desc.
 * Besides, tunneling parameters and other fields need be configured in ctx desc
 * if the outer checksum offload is enabled.
 */

#define IAVF_VECTOR_PATH 0
#define IAVF_VECTOR_OFFLOAD_PATH 1
#define IAVF_VECTOR_CTX_OFFLOAD_PATH 2

#define DEFAULT_TX_RS_THRESH     32
#define DEFAULT_TX_FREE_THRESH   32

#define IAVF_MIN_TSO_MSS          256
#define IAVF_MAX_TSO_MSS          9668
#define IAVF_TSO_MAX_SEG          UINT8_MAX
#define IAVF_TX_MAX_MTU_SEG       8

#define IAVF_TX_MIN_PKT_LEN 17

#define IAVF_TX_CKSUM_OFFLOAD_MASK (		 \
		RTE_MBUF_F_TX_IP_CKSUM |		 \
		RTE_MBUF_F_TX_L4_MASK |		 \
		RTE_MBUF_F_TX_TCP_SEG |          \
		RTE_MBUF_F_TX_UDP_SEG |          \
		RTE_MBUF_F_TX_OUTER_IP_CKSUM |   \
		RTE_MBUF_F_TX_OUTER_UDP_CKSUM)

#define IAVF_TX_OFFLOAD_MASK (  \
		RTE_MBUF_F_TX_OUTER_IPV6 |		 \
		RTE_MBUF_F_TX_OUTER_IPV4 |		 \
		RTE_MBUF_F_TX_IPV6 |			 \
		RTE_MBUF_F_TX_IPV4 |			 \
		RTE_MBUF_F_TX_VLAN |		 \
		RTE_MBUF_F_TX_IP_CKSUM |		 \
		RTE_MBUF_F_TX_L4_MASK |		 \
		RTE_MBUF_F_TX_TCP_SEG |		 \
		RTE_MBUF_F_TX_UDP_SEG |      \
		RTE_MBUF_F_TX_TUNNEL_MASK |	\
		RTE_MBUF_F_TX_OUTER_IP_CKSUM |  \
		RTE_MBUF_F_TX_OUTER_UDP_CKSUM | \
		RTE_MBUF_F_TX_SEC_OFFLOAD)

#define IAVF_TX_OFFLOAD_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ IAVF_TX_OFFLOAD_MASK)

/* HW requires that TX buffer size ranges from 1B up to (16K-1)B. */
#define IAVF_MAX_DATA_PER_TXD \
	(IAVF_TXD_QW1_TX_BUF_SZ_MASK >> IAVF_TXD_QW1_TX_BUF_SZ_SHIFT)

extern uint64_t iavf_timestamp_dynflag;
extern int iavf_timestamp_dynfield_offset;

/**
 * Rx Flex Descriptors
 * These descriptors are used instead of the legacy version descriptors
 */
union iavf_16b_rx_flex_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
				 /* bit 0 of hdr_addr is DD bit */
	} read;
	struct {
		/* Qword 0 */
		u8 rxdid; /* descriptor builder profile ID */
		u8 mir_id_umb_cast; /* mirror=[5:0], umb=[7:6] */
		__le16 ptype_flex_flags0; /* ptype=[9:0], ff0=[15:10] */
		__le16 pkt_len; /* [15:14] are reserved */
		__le16 hdr_len_sph_flex_flags1; /* header=[10:0] */
						/* sph=[11:11] */
						/* ff1/ext=[15:12] */

		/* Qword 1 */
		__le16 status_error0;
		__le16 l2tag1;
		__le16 flex_meta0;
		__le16 flex_meta1;
	} wb; /* writeback */
};

union iavf_32b_rx_flex_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
				 /* bit 0 of hdr_addr is DD bit */
		__le64 rsvd1;
		__le64 rsvd2;
	} read;
	struct {
		/* Qword 0 */
		u8 rxdid; /* descriptor builder profile ID */
		u8 mir_id_umb_cast; /* mirror=[5:0], umb=[7:6] */
		__le16 ptype_flex_flags0; /* ptype=[9:0], ff0=[15:10] */
		__le16 pkt_len; /* [15:14] are reserved */
		__le16 hdr_len_sph_flex_flags1; /* header=[10:0] */
						/* sph=[11:11] */
						/* ff1/ext=[15:12] */

		/* Qword 1 */
		__le16 status_error0;
		__le16 l2tag1;
		__le16 flex_meta0;
		__le16 flex_meta1;

		/* Qword 2 */
		__le16 status_error1;
		u8 flex_flags2;
		u8 time_stamp_low;
		__le16 l2tag2_1st;
		__le16 l2tag2_2nd;

		/* Qword 3 */
		__le16 flex_meta2;
		__le16 flex_meta3;
		union {
			struct {
				__le16 flex_meta4;
				__le16 flex_meta5;
			} flex;
			__le32 ts_high;
		} flex_ts;
	} wb; /* writeback */
};

/* HW desc structure, both 16-byte and 32-byte types are supported */
#ifdef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
#define iavf_rx_desc iavf_16byte_rx_desc
#define iavf_rx_flex_desc iavf_16b_rx_flex_desc
#else
#define iavf_rx_desc iavf_32byte_rx_desc
#define iavf_rx_flex_desc iavf_32b_rx_flex_desc
#endif

typedef void (*iavf_rxd_to_pkt_fields_t)(struct iavf_rx_queue *rxq,
				struct rte_mbuf *mb,
				volatile union iavf_rx_flex_desc *rxdp);

struct iavf_rxq_ops {
	void (*release_mbufs)(struct iavf_rx_queue *rxq);
};

struct iavf_txq_ops {
	void (*release_mbufs)(struct iavf_tx_queue *txq);
};


struct iavf_rx_queue_stats {
	uint64_t reserved;
	struct iavf_ipsec_crypto_stats ipsec_crypto;
};

/* Structure associated with each Rx queue. */
struct iavf_rx_queue {
	struct rte_mempool *mp;       /* mbuf pool to populate Rx ring */
	const struct rte_memzone *mz; /* memzone for Rx ring */
	volatile union iavf_rx_desc *rx_ring; /* Rx ring virtual address */
	uint64_t rx_ring_phys_addr;   /* Rx ring DMA address */
	struct rte_mbuf **sw_ring;     /* address of SW ring */
	uint16_t nb_rx_desc;          /* ring length */
	uint16_t rx_tail;             /* current value of tail */
	volatile uint8_t *qrx_tail;   /* register address of tail */
	uint16_t rx_free_thresh;      /* max free RX desc to hold */
	uint16_t nb_rx_hold;          /* number of held free RX desc */
	struct rte_mbuf *pkt_first_seg; /* first segment of current packet */
	struct rte_mbuf *pkt_last_seg;  /* last segment of current packet */
	struct rte_mbuf fake_mbuf;      /* dummy mbuf */
	uint8_t rxdid;
	uint8_t rel_mbufs_type;

	/* used for VPMD */
	uint16_t rxrearm_nb;       /* number of remaining to be re-armed */
	uint16_t rxrearm_start;    /* the idx we start the re-arming from */
	uint64_t mbuf_initializer; /* value to init mbufs */

	/* for rx bulk */
	uint16_t rx_nb_avail;      /* number of staged packets ready */
	uint16_t rx_next_avail;    /* index of next staged packets */
	uint16_t rx_free_trigger;  /* triggers rx buffer allocation */
	struct rte_mbuf *rx_stage[IAVF_RX_MAX_BURST * 2]; /* store mbuf */

	uint16_t port_id;        /* device port ID */
	uint8_t crc_len;        /* 0 if CRC stripped, 4 otherwise */
	uint8_t fdir_enabled;   /* 0 if FDIR disabled, 1 when enabled */
	uint16_t queue_id;      /* Rx queue index */
	uint16_t rx_buf_len;    /* The packet buffer size */
	uint16_t rx_hdr_len;    /* The header buffer size */
	uint16_t max_pkt_len;   /* Maximum packet length */
	struct iavf_vsi *vsi; /**< the VSI this queue belongs to */

	bool q_set;             /* if rx queue has been configured */
	bool rx_deferred_start; /* don't start this queue in dev start */
	const struct iavf_rxq_ops *ops;
	uint8_t rx_flags;
#define IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG1     BIT(0)
#define IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2   BIT(1)
	uint8_t proto_xtr; /* protocol extraction type */
	uint64_t xtr_ol_flag;
		/* flexible descriptor metadata extraction offload flag */
	struct iavf_rx_queue_stats stats;
	uint64_t offloads;
	uint64_t phc_time;
	uint64_t hw_time_update;
};

struct iavf_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

struct iavf_tx_vec_entry {
	struct rte_mbuf *mbuf;
};

/* Structure associated with each TX queue. */
struct iavf_tx_queue {
	const struct rte_memzone *mz;  /* memzone for Tx ring */
	volatile struct iavf_tx_desc *tx_ring; /* Tx ring virtual address */
	uint64_t tx_ring_phys_addr;    /* Tx ring DMA address */
	struct iavf_tx_entry *sw_ring;  /* address array of SW ring */
	uint16_t nb_tx_desc;           /* ring length */
	uint16_t tx_tail;              /* current value of tail */
	volatile uint8_t *qtx_tail;    /* register address of tail */
	/* number of used desc since RS bit set */
	uint16_t nb_used;
	uint16_t nb_free;
	uint16_t last_desc_cleaned;    /* last desc have been cleaned*/
	uint16_t free_thresh;
	uint16_t rs_thresh;
	uint8_t rel_mbufs_type;
	struct iavf_vsi *vsi; /**< the VSI this queue belongs to */

	uint16_t port_id;
	uint16_t queue_id;
	uint64_t offloads;
	uint16_t next_dd;              /* next to set RS, for VPMD */
	uint16_t next_rs;              /* next to check DD,  for VPMD */
	uint16_t ipsec_crypto_pkt_md_offset;

	bool q_set;                    /* if rx queue has been configured */
	bool tx_deferred_start;        /* don't start this queue in dev start */
	const struct iavf_txq_ops *ops;
#define IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG1	BIT(0)
#define IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2	BIT(1)
	uint8_t vlan_flag;
	uint8_t tc;
	uint8_t use_ctx:1;            /* if use the ctx desc, a packet needs two descriptors */
};

/* Offload features */
union iavf_tx_offload {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /* L2 (MAC) Header Length. */
		uint64_t l3_len:9; /* L3 (IP) Header Length. */
		uint64_t l4_len:8; /* L4 Header Length. */
		uint64_t tso_segsz:16; /* TCP TSO segment size */
		/* uint64_t unused : 24; */
	};
};

/* Rx Flex Descriptor
 * RxDID Profile ID 16-21
 * Flex-field 0: RSS hash lower 16-bits
 * Flex-field 1: RSS hash upper 16-bits
 * Flex-field 2: Flow ID lower 16-bits
 * Flex-field 3: Flow ID upper 16-bits
 * Flex-field 4: AUX0
 * Flex-field 5: AUX1
 */
struct iavf_32b_rx_flex_desc_comms {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 flow_id;
	union {
		struct {
			__le16 aux0;
			__le16 aux1;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/* Rx Flex Descriptor
 * RxDID Profile ID 22-23 (swap Hash and FlowID)
 * Flex-field 0: Flow ID lower 16-bits
 * Flex-field 1: Flow ID upper 16-bits
 * Flex-field 2: RSS hash lower 16-bits
 * Flex-field 3: RSS hash upper 16-bits
 * Flex-field 4: AUX0
 * Flex-field 5: AUX1
 */
struct iavf_32b_rx_flex_desc_comms_ovs {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 flow_id;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rss_hash;
	union {
		struct {
			__le16 aux0;
			__le16 aux1;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/* Rx Flex Descriptor
 * RxDID Profile ID 24 Inline IPsec
 * Flex-field 0: RSS hash lower 16-bits
 * Flex-field 1: RSS hash upper 16-bits
 * Flex-field 2: Flow ID lower 16-bits
 * Flex-field 3: Flow ID upper 16-bits
 * Flex-field 4: Inline IPsec SAID lower 16-bits
 * Flex-field 5: Inline IPsec SAID upper 16-bits
 */
struct iavf_32b_rx_flex_desc_comms_ipsec {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 flow_id;
	__le32 ipsec_said;
};

enum iavf_rxtx_rel_mbufs_type {
	IAVF_REL_MBUFS_DEFAULT		= 0,
	IAVF_REL_MBUFS_SSE_VEC		= 1,
	IAVF_REL_MBUFS_AVX512_VEC	= 2,
};

/* Receive Flex Descriptor profile IDs: There are a total
 * of 64 profiles where profile IDs 0/1 are for legacy; and
 * profiles 2-63 are flex profiles that can be programmed
 * with a specific metadata (profile 7 reserved for HW)
 */
enum iavf_rxdid {
	IAVF_RXDID_LEGACY_0		= 0,
	IAVF_RXDID_LEGACY_1		= 1,
	IAVF_RXDID_FLEX_NIC		= 2,
	IAVF_RXDID_FLEX_NIC_2		= 6,
	IAVF_RXDID_HW			= 7,
	IAVF_RXDID_COMMS_GENERIC	= 16,
	IAVF_RXDID_COMMS_AUX_VLAN	= 17,
	IAVF_RXDID_COMMS_AUX_IPV4	= 18,
	IAVF_RXDID_COMMS_AUX_IPV6	= 19,
	IAVF_RXDID_COMMS_AUX_IPV6_FLOW	= 20,
	IAVF_RXDID_COMMS_AUX_TCP	= 21,
	IAVF_RXDID_COMMS_OVS_1		= 22,
	IAVF_RXDID_COMMS_OVS_2		= 23,
	IAVF_RXDID_COMMS_IPSEC_CRYPTO	= 24,
	IAVF_RXDID_COMMS_AUX_IP_OFFSET	= 25,
	IAVF_RXDID_LAST			= 63,
};

enum iavf_rx_flex_desc_status_error_0_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_FLEX_DESC_STATUS0_DD_S = 0,
	IAVF_RX_FLEX_DESC_STATUS0_EOF_S,
	IAVF_RX_FLEX_DESC_STATUS0_HBO_S,
	IAVF_RX_FLEX_DESC_STATUS0_L3L4P_S,
	IAVF_RX_FLEX_DESC_STATUS0_XSUM_IPE_S,
	IAVF_RX_FLEX_DESC_STATUS0_XSUM_L4E_S,
	IAVF_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S,
	IAVF_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S,
	IAVF_RX_FLEX_DESC_STATUS0_LPBK_S,
	IAVF_RX_FLEX_DESC_STATUS0_IPV6EXADD_S,
	IAVF_RX_FLEX_DESC_STATUS0_RXE_S,
	IAVF_RX_FLEX_DESC_STATUS0_CRCP_S,
	IAVF_RX_FLEX_DESC_STATUS0_RSS_VALID_S,
	IAVF_RX_FLEX_DESC_STATUS0_L2TAG1P_S,
	IAVF_RX_FLEX_DESC_STATUS0_XTRMD0_VALID_S,
	IAVF_RX_FLEX_DESC_STATUS0_XTRMD1_VALID_S,
	IAVF_RX_FLEX_DESC_STATUS0_LAST /* this entry must be last!!! */
};

enum iavf_rx_flex_desc_status_error_1_bits {
	/* Note: These are predefined bit offsets */
	/* Bits 3:0 are reserved for inline ipsec status */
	IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_0 = 0,
	IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_1,
	IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_2,
	IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_3,
	IAVF_RX_FLEX_DESC_STATUS1_NAT_S,
	IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_PROCESSED,
	/* [10:6] reserved */
	IAVF_RX_FLEX_DESC_STATUS1_L2TAG2P_S = 11,
	IAVF_RX_FLEX_DESC_STATUS1_XTRMD2_VALID_S = 12,
	IAVF_RX_FLEX_DESC_STATUS1_XTRMD3_VALID_S = 13,
	IAVF_RX_FLEX_DESC_STATUS1_XTRMD4_VALID_S = 14,
	IAVF_RX_FLEX_DESC_STATUS1_XTRMD5_VALID_S = 15,
	IAVF_RX_FLEX_DESC_STATUS1_LAST /* this entry must be last!!! */
};

#define IAVF_RX_FLEX_DESC_IPSEC_CRYPTO_STATUS_MASK  (		\
	BIT(IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_0) |	\
	BIT(IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_1) |	\
	BIT(IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_2) |	\
	BIT(IAVF_RX_FLEX_DESC_STATUS1_IPSEC_CRYPTO_STATUS_3))

enum iavf_rx_flex_desc_ipsec_crypto_status {
	IAVF_IPSEC_CRYPTO_STATUS_SUCCESS = 0,
	IAVF_IPSEC_CRYPTO_STATUS_SAD_MISS,
	IAVF_IPSEC_CRYPTO_STATUS_NOT_PROCESSED,
	IAVF_IPSEC_CRYPTO_STATUS_ICV_CHECK_FAIL,
	IAVF_IPSEC_CRYPTO_STATUS_LENGTH_ERR,
	/* Reserved */
	IAVF_IPSEC_CRYPTO_STATUS_MISC_ERR = 0xF
};



#define IAVF_TXD_DATA_QW1_DTYPE_SHIFT	(0)
#define IAVF_TXD_DATA_QW1_DTYPE_MASK	(0xFUL << IAVF_TXD_QW1_DTYPE_SHIFT)

#define IAVF_TXD_DATA_QW1_CMD_SHIFT	(4)
#define IAVF_TXD_DATA_QW1_CMD_MASK	(0x3FFUL << IAVF_TXD_DATA_QW1_CMD_SHIFT)

#define IAVF_TXD_DATA_QW1_OFFSET_SHIFT	(16)
#define IAVF_TXD_DATA_QW1_OFFSET_MASK	(0x3FFFFULL << \
					IAVF_TXD_DATA_QW1_OFFSET_SHIFT)

#define IAVF_TXD_DATA_QW1_OFFSET_MACLEN_SHIFT	(IAVF_TXD_DATA_QW1_OFFSET_SHIFT)
#define IAVF_TXD_DATA_QW1_OFFSET_MACLEN_MASK	\
	(0x7FUL << IAVF_TXD_DATA_QW1_OFFSET_MACLEN_SHIFT)

#define IAVF_TXD_DATA_QW1_OFFSET_IPLEN_SHIFT	\
	(IAVF_TXD_DATA_QW1_OFFSET_SHIFT + IAVF_TX_DESC_LENGTH_IPLEN_SHIFT)
#define IAVF_TXD_DATA_QW1_OFFSET_IPLEN_MASK	\
	(0x7FUL << IAVF_TXD_DATA_QW1_OFFSET_IPLEN_SHIFT)

#define IAVF_TXD_DATA_QW1_OFFSET_L4LEN_SHIFT	\
	(IAVF_TXD_DATA_QW1_OFFSET_SHIFT + IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT)
#define IAVF_TXD_DATA_QW1_OFFSET_L4LEN_MASK	\
	(0xFUL << IAVF_TXD_DATA_QW1_OFFSET_L4LEN_SHIFT)

#define IAVF_TXD_DATA_QW1_MACLEN_MASK	\
	(0x7FUL << IAVF_TX_DESC_LENGTH_MACLEN_SHIFT)
#define IAVF_TXD_DATA_QW1_IPLEN_MASK	\
	(0x7FUL << IAVF_TX_DESC_LENGTH_IPLEN_SHIFT)
#define IAVF_TXD_DATA_QW1_L4LEN_MASK	\
	(0xFUL << IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT)
#define IAVF_TXD_DATA_QW1_FCLEN_MASK	\
	(0xFUL << IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT)

#define IAVF_TXD_DATA_QW1_TX_BUF_SZ_SHIFT	(34)
#define IAVF_TXD_DATA_QW1_TX_BUF_SZ_MASK	\
	(0x3FFFULL << IAVF_TXD_DATA_QW1_TX_BUF_SZ_SHIFT)

#define IAVF_TXD_DATA_QW1_L2TAG1_SHIFT		(48)
#define IAVF_TXD_DATA_QW1_L2TAG1_MASK		\
	(0xFFFFULL << IAVF_TXD_DATA_QW1_L2TAG1_SHIFT)

#define IAVF_TXD_CTX_QW1_IPSEC_PARAMS_CIPHERBLK_SHIFT	(11)
#define IAVF_TXD_CTX_QW1_IPSEC_PARAMS_CIPHERBLK_MASK	\
	(0x7UL << IAVF_TXD_CTX_QW1_IPSEC_PARAMS_CIPHERBLK_SHIFT)

#define IAVF_TXD_CTX_QW1_IPSEC_PARAMS_ICVLEN_SHIFT	(14)
#define IAVF_TXD_CTX_QW1_IPSEC_PARAMS_ICVLEN_MASK	\
	(0xFUL << IAVF_TXD_CTX_QW1_IPSEC_PARAMS_ICVLEN_SHIFT)

#define IAVF_TXD_CTX_QW1_SEG_PARAMS_TLEN_SHIFT		(30)
#define IAVF_TXD_CTX_QW1_SEG_PARAMS_TLEN_MASK		\
	(0x3FFFFUL << IAVF_TXD_CTX_QW1_SEG_PARAMS_TLEN_SHIFT)

#define IAVF_TXD_CTX_QW1_TSYNC_PARAMS_TLEN_SHIFT	(30)
#define IAVF_TXD_CTX_QW1_TSYNC_PARAMS_TLEN_MASK		\
	(0x3FUL << IAVF_TXD_CTX_QW1_SEG_PARAMS_TLEN_SHIFT)

#define IAVF_TXD_CTX_QW1_SEG_PARAMS_MSS_SHIFT		(50)
#define IAVF_TXD_CTX_QW1_SEG_PARAMS_MSS_MASK		\
	(0x3FFFUL << IAVF_TXD_CTX_QW1_SEG_PARAMS_MSS_SHIFT)

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPT_SHIFT		(0)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPT_MASK		(0x3UL)

enum iavf_tx_ctx_desc_tunnel_external_ip_type {
	IAVF_TX_CTX_DESC_EIPT_NONE,
	IAVF_TX_CTX_DESC_EIPT_IPV6,
	IAVF_TX_CTX_DESC_EIPT_IPV4_NO_CHECKSUM_OFFLOAD,
	IAVF_TX_CTX_DESC_EIPT_IPV4_CHECKSUM_OFFLOAD
};

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPLEN_SHIFT	(2)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPLEN_MASK		(0x7FUL)

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4TUNT_SHIFT	(9)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4TUNT_MASK		(0x3UL)

enum iavf_tx_ctx_desc_tunnel_l4_tunnel_type {
	IAVF_TX_CTX_DESC_L4_TUN_TYP_NO_UDP_GRE,
	IAVF_TX_CTX_DESC_L4_TUN_TYP_UDP,
	IAVF_TX_CTX_DESC_L4_TUN_TYP_GRE
};

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIP_NOINC_SHIFT	(11)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_EIP_NOINC_MASK	(0x1UL)

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4TUNLEN_SHIFT	(12)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4TUNLEN_MASK	(0x7FUL)

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_DECTTL_SHIFT	(19)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_DECTTL_MASK		(0xFUL)

#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4T_CS_SHIFT	(23)
#define IAVF_TXD_CTX_QW0_TUN_PARAMS_L4T_CS_MASK		(0x1UL)

#define IAVF_TXD_CTX_QW0_L2TAG2_PARAM			(32)
#define IAVF_TXD_CTX_QW0_L2TAG2_MASK			(0xFFFFUL)


#define IAVF_RX_FLEX_DESC_IPSEC_CRYPTO_SAID_MASK	(0xFFFFF)

/* for iavf_32b_rx_flex_desc.ptype_flex_flags0 member */
#define IAVF_RX_FLEX_DESC_PTYPE_M	(0x3FF) /* 10-bits */


/* for iavf_32b_rx_flex_desc.ptype_flex_flags0 member */
#define IAVF_RX_FLEX_DESC_PTYPE_M	(0x3FF) /* 10-bits */

/* for iavf_32b_rx_flex_desc.pkt_len member */
#define IAVF_RX_FLX_DESC_PKT_LEN_M	(0x3FFF) /* 14-bits */

int iavf_dev_rx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_rxconf *rx_conf,
			   struct rte_mempool *mp);

int iavf_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int iavf_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
void iavf_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

int iavf_dev_tx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_txconf *tx_conf);
int iavf_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int iavf_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int iavf_dev_tx_done_cleanup(void *txq, uint32_t free_cnt);
void iavf_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void iavf_stop_queues(struct rte_eth_dev *dev);
uint16_t iavf_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts);
uint16_t iavf_recv_pkts_flex_rxd(void *rx_queue,
				 struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts(void *rx_queue,
				 struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_flex_rxd(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts);
uint16_t iavf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
uint16_t iavf_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
void iavf_set_rx_function(struct rte_eth_dev *dev);
void iavf_set_tx_function(struct rte_eth_dev *dev);
void iavf_dev_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
			  struct rte_eth_rxq_info *qinfo);
void iavf_dev_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
			  struct rte_eth_txq_info *qinfo);
uint32_t iavf_dev_rxq_count(void *rx_queue);
int iavf_dev_rx_desc_status(void *rx_queue, uint16_t offset);
int iavf_dev_tx_desc_status(void *tx_queue, uint16_t offset);

uint16_t iavf_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_flex_rxd(void *rx_queue, struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec(void *rx_queue,
				     struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_flex_rxd(void *rx_queue,
					       struct rte_mbuf **rx_pkts,
					       uint16_t nb_pkts);
uint16_t iavf_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx2_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
					 uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx2_flex_rxd(void *rx_queue,
					  struct rte_mbuf **rx_pkts,
					  uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx2_flex_rxd_offload(void *rx_queue,
						  struct rte_mbuf **rx_pkts,
						  uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx2(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx2_offload(void *rx_queue,
						   struct rte_mbuf **rx_pkts,
						   uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx2_flex_rxd(void *rx_queue,
						    struct rte_mbuf **rx_pkts,
						    uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx2_flex_rxd_offload(void *rx_queue,
							    struct rte_mbuf **rx_pkts,
							    uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			    uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
				 uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec_avx2_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
					 uint16_t nb_pkts);
int iavf_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc);
int iavf_rx_vec_dev_check(struct rte_eth_dev *dev);
int iavf_tx_vec_dev_check(struct rte_eth_dev *dev);
int iavf_rxq_vec_setup(struct iavf_rx_queue *rxq);
int iavf_txq_vec_setup(struct iavf_tx_queue *txq);
uint16_t iavf_recv_pkts_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx512_offload(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx512_flex_rxd(void *rx_queue,
					    struct rte_mbuf **rx_pkts,
					    uint16_t nb_pkts);
uint16_t iavf_recv_pkts_vec_avx512_flex_rxd_offload(void *rx_queue,
						    struct rte_mbuf **rx_pkts,
						    uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx512(void *rx_queue,
					     struct rte_mbuf **rx_pkts,
					     uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx512_offload(void *rx_queue,
						     struct rte_mbuf **rx_pkts,
						     uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx512_flex_rxd(void *rx_queue,
						      struct rte_mbuf **rx_pkts,
						      uint16_t nb_pkts);
uint16_t iavf_recv_scattered_pkts_vec_avx512_flex_rxd_offload(void *rx_queue,
							      struct rte_mbuf **rx_pkts,
							      uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec_avx512_offload(void *tx_queue,
					   struct rte_mbuf **tx_pkts,
					   uint16_t nb_pkts);
uint16_t iavf_xmit_pkts_vec_avx512_ctx_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);
int iavf_txq_vec_setup_avx512(struct iavf_tx_queue *txq);

uint8_t iavf_proto_xtr_type_to_rxdid(uint8_t xtr_type);

void iavf_set_default_ptype_table(struct rte_eth_dev *dev);
void iavf_tx_queue_release_mbufs_avx512(struct iavf_tx_queue *txq);
void iavf_rx_queue_release_mbufs_sse(struct iavf_rx_queue *rxq);
void iavf_tx_queue_release_mbufs_sse(struct iavf_tx_queue *txq);

static inline
void iavf_dump_rx_descriptor(struct iavf_rx_queue *rxq,
			    const volatile void *desc,
			    uint16_t rx_id)
{
#ifdef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
	const volatile union iavf_16byte_rx_desc *rx_desc = desc;

	printf("Queue %d Rx_desc %d: QW0: 0x%016"PRIx64" QW1: 0x%016"PRIx64"\n",
	       rxq->queue_id, rx_id, rx_desc->read.pkt_addr,
	       rx_desc->read.hdr_addr);
#else
	const volatile union iavf_32byte_rx_desc *rx_desc = desc;

	printf("Queue %d Rx_desc %d: QW0: 0x%016"PRIx64" QW1: 0x%016"PRIx64
	       " QW2: 0x%016"PRIx64" QW3: 0x%016"PRIx64"\n", rxq->queue_id,
	       rx_id, rx_desc->read.pkt_addr, rx_desc->read.hdr_addr,
	       rx_desc->read.rsvd1, rx_desc->read.rsvd2);
#endif
}

/* All the descriptors are 16 bytes, so just use one of them
 * to print the qwords
 */
static inline
void iavf_dump_tx_descriptor(const struct iavf_tx_queue *txq,
			    const volatile void *desc, uint16_t tx_id)
{
	const char *name;
	const volatile struct iavf_tx_desc *tx_desc = desc;
	enum iavf_tx_desc_dtype_value type;


	type = (enum iavf_tx_desc_dtype_value)
		rte_le_to_cpu_64(tx_desc->cmd_type_offset_bsz &
			rte_cpu_to_le_64(IAVF_TXD_DATA_QW1_DTYPE_MASK));
	switch (type) {
	case IAVF_TX_DESC_DTYPE_DATA:
		name = "Tx_data_desc";
		break;
	case IAVF_TX_DESC_DTYPE_CONTEXT:
		name = "Tx_context_desc";
		break;
	case IAVF_TX_DESC_DTYPE_IPSEC:
		name = "Tx_IPsec_desc";
		break;
	default:
		name = "unknown_desc";
		break;
	}

	printf("Queue %d %s %d: QW0: 0x%016"PRIx64" QW1: 0x%016"PRIx64"\n",
		txq->queue_id, name, tx_id, tx_desc->buffer_addr,
		tx_desc->cmd_type_offset_bsz);
}

#define FDIR_PROC_ENABLE_PER_QUEUE(ad, on) do { \
	int i; \
	for (i = 0; i < (ad)->dev_data->nb_rx_queues; i++) { \
		struct iavf_rx_queue *rxq = (ad)->dev_data->rx_queues[i]; \
		if (!rxq) \
			continue; \
		rxq->fdir_enabled = on; \
	} \
	PMD_DRV_LOG(DEBUG, "FDIR processing on RX set to %d", on); \
} while (0)

/* Enable/disable flow director Rx processing in data path. */
static inline
void iavf_fdir_rx_proc_enable(struct iavf_adapter *ad, bool on)
{
	if (on) {
		/* enable flow director processing */
		FDIR_PROC_ENABLE_PER_QUEUE(ad, on);
		ad->fdir_ref_cnt++;
	} else {
		if (ad->fdir_ref_cnt >= 1) {
			ad->fdir_ref_cnt--;

			if (ad->fdir_ref_cnt == 0)
				FDIR_PROC_ENABLE_PER_QUEUE(ad, on);
		}
	}
}

static inline
uint64_t iavf_tstamp_convert_32b_64b(uint64_t time, uint32_t in_timestamp)
{
	const uint64_t mask = 0xFFFFFFFF;
	uint32_t delta;
	uint64_t ns;

	delta = (in_timestamp - (uint32_t)(time & mask));
	if (delta > (mask / 2)) {
		delta = ((uint32_t)(time & mask) - in_timestamp);
		ns = time - delta;
	} else {
		ns = time + delta;
	}

	return ns;
}

#ifdef RTE_LIBRTE_IAVF_DEBUG_DUMP_DESC
#define IAVF_DUMP_RX_DESC(rxq, desc, rx_id) \
	iavf_dump_rx_descriptor(rxq, desc, rx_id)
#define IAVF_DUMP_TX_DESC(txq, desc, tx_id) \
	iavf_dump_tx_descriptor(txq, desc, tx_id)
#else
#define IAVF_DUMP_RX_DESC(rxq, desc, rx_id) do { } while (0)
#define IAVF_DUMP_TX_DESC(txq, desc, tx_id) do { } while (0)
#endif

#endif /* _IAVF_RXTX_H_ */
