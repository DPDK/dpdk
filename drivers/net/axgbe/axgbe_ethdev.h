/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#ifndef RTE_ETH_AXGBE_H_
#define RTE_ETH_AXGBE_H_

#include <rte_mempool.h>
#include <rte_lcore.h>
#include "axgbe_common.h"
#include "rte_time.h"

#define IRQ				0xff

#define AXGBE_TX_MAX_BUF_SIZE		(0x3fff & ~(64 - 1))
#define AXGBE_RX_MAX_BUF_SIZE		(0x3fff & ~(64 - 1))
#define AXGBE_RX_MIN_BUF_SIZE		(RTE_ETHER_MAX_LEN + RTE_VLAN_HLEN)
#define AXGBE_MAX_MAC_ADDRS		32
#define AXGBE_MAX_HASH_MAC_ADDRS	256

#define AXGBE_RX_BUF_ALIGN		64

#define AXGBE_MAX_DMA_CHANNELS		16
#define AXGBE_MAX_QUEUES		16
#define AXGBE_PRIORITY_QUEUES		8
#define AXGBE_DMA_STOP_TIMEOUT		1

/* DMA cache settings - Outer sharable, write-back, write-allocate */
#define AXGBE_DMA_OS_AXDOMAIN		0x2
#define AXGBE_DMA_OS_ARCACHE		0xb
#define AXGBE_DMA_OS_AWCACHE		0xf

/* DMA cache settings - System, no caches used */
#define AXGBE_DMA_SYS_AXDOMAIN		0x3
#define AXGBE_DMA_SYS_ARCACHE		0x0
#define AXGBE_DMA_SYS_AWCACHE		0x0

/* DMA channel interrupt modes */
#define AXGBE_IRQ_MODE_EDGE		0
#define AXGBE_IRQ_MODE_LEVEL		1

#define AXGBE_DMA_INTERRUPT_MASK	0x31c7

#define AXGMAC_MIN_PACKET		60
#define AXGMAC_STD_PACKET_MTU		1500
#define AXGMAC_MAX_STD_PACKET		1518
#define AXGMAC_JUMBO_PACKET_MTU		9000
#define AXGMAC_MAX_JUMBO_PACKET		9018
/* Inter-frame gap + preamble */
#define AXGMAC_ETH_PREAMBLE		(12 + 8)

#define AXGMAC_PFC_DATA_LEN		46
#define AXGMAC_PFC_DELAYS		14000

/* PCI BAR mapping */
#define AXGBE_AXGMAC_BAR		0
#define AXGBE_XPCS_BAR			1
#define AXGBE_MAC_PROP_OFFSET		0x1d000
#define AXGBE_I2C_CTRL_OFFSET		0x1e000

/* PCI clock frequencies */
#define AXGBE_V2_DMA_CLOCK_FREQ		500000000
#define AXGBE_V2_PTP_CLOCK_FREQ		125000000

/* Timestamp support - values based on 50MHz PTP clock
 *   50MHz => 20 nsec
 */
#define AXGBE_TSTAMP_SSINC       20
#define AXGBE_TSTAMP_SNSINC      0
#define AXGBE_CYCLECOUNTER_MASK 0xffffffffffffffffULL

#define AXGMAC_FIFO_MIN_ALLOC		2048
#define AXGMAC_FIFO_UNIT		256
#define AXGMAC_FIFO_ALIGN(_x)                            \
	(((_x) + AXGMAC_FIFO_UNIT - 1) & ~(XGMAC_FIFO_UNIT - 1))
#define AXGMAC_FIFO_FC_OFF		2048
#define AXGMAC_FIFO_FC_MIN		4096

#define AXGBE_TC_MIN_QUANTUM		10

/* Flow control queue count */
#define AXGMAC_MAX_FLOW_CONTROL_QUEUES	8

/* Flow control threshold units */
#define AXGMAC_FLOW_CONTROL_UNIT	512
#define AXGMAC_FLOW_CONTROL_ALIGN(_x)				\
	(((_x) + AXGMAC_FLOW_CONTROL_UNIT - 1) &		\
	~(AXGMAC_FLOW_CONTROL_UNIT - 1))
#define AXGMAC_FLOW_CONTROL_VALUE(_x)				\
	(((_x) < 1024) ? 0 : ((_x) / AXGMAC_FLOW_CONTROL_UNIT) - 2)
#define AXGMAC_FLOW_CONTROL_MAX		33280

/* Maximum MAC address hash table size (256 bits = 8 dword) */
#define AXGBE_MAC_HASH_TABLE_SIZE	8

/* Receive Side Scaling */
#define AXGBE_RSS_OFFLOAD  ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP)

#define AXGBE_RSS_HASH_KEY_SIZE		40
#define AXGBE_RSS_MAX_TABLE_SIZE	256
#define AXGBE_RSS_LOOKUP_TABLE_TYPE	0
#define AXGBE_RSS_HASH_KEY_TYPE		1

/* Auto-negotiation */
#define AXGBE_AN_MS_TIMEOUT		500
#define AXGBE_LINK_TIMEOUT		5

#define AXGBE_SGMII_AN_LINK_STATUS	BIT(1)
#define AXGBE_SGMII_AN_LINK_SPEED	(BIT(2) | BIT(3))
#define AXGBE_SGMII_AN_LINK_SPEED_100	0x04
#define AXGBE_SGMII_AN_LINK_SPEED_1000	0x08
#define AXGBE_SGMII_AN_LINK_DUPLEX	BIT(4)

/* ECC correctable error notification window (seconds) */
#define AXGBE_ECC_LIMIT			60

/* MDIO port types */
#define AXGMAC_MAX_C22_PORT		3

/* The max frame size with default MTU */
#define AXGBE_ETH_MAX_LEN ( \
	RTE_ETHER_MTU + \
	RTE_ETHER_HDR_LEN + \
	RTE_ETHER_CRC_LEN)

/* Helper macro for descriptor handling
 *  Always use AXGBE_GET_DESC_DATA to access the descriptor data
 *  since the index is free-running and needs to be and-ed
 *  with the descriptor count value of the ring to index to
 *  the proper descriptor data.
 */
#define AXGBE_GET_DESC_DATA(_ring, _idx)			\
	((_ring)->rdata +					\
	 ((_idx) & ((_ring)->rdesc_count - 1)))

struct axgbe_port;

enum axgbe_state {
	AXGBE_DOWN,
	AXGBE_LINK_INIT,
	AXGBE_LINK_ERR,
	AXGBE_STOPPED,
};

enum axgbe_int {
	AXGMAC_INT_DMA_CH_SR_TI,
	AXGMAC_INT_DMA_CH_SR_TPS,
	AXGMAC_INT_DMA_CH_SR_TBU,
	AXGMAC_INT_DMA_CH_SR_RI,
	AXGMAC_INT_DMA_CH_SR_RBU,
	AXGMAC_INT_DMA_CH_SR_RPS,
	AXGMAC_INT_DMA_CH_SR_TI_RI,
	AXGMAC_INT_DMA_CH_SR_FBE,
	AXGMAC_INT_DMA_ALL,
};

enum axgbe_int_state {
	AXGMAC_INT_STATE_SAVE,
	AXGMAC_INT_STATE_RESTORE,
};

enum axgbe_ecc_sec {
	AXGBE_ECC_SEC_TX,
	AXGBE_ECC_SEC_RX,
	AXGBE_ECC_SEC_DESC,
};

enum axgbe_speed {
	AXGBE_SPEED_1000 = 0,
	AXGBE_SPEED_2500,
	AXGBE_SPEED_10000,
	AXGBE_SPEEDS,
};

enum axgbe_xpcs_access {
	AXGBE_XPCS_ACCESS_V1 = 0,
	AXGBE_XPCS_ACCESS_V2,
};

enum axgbe_an_mode {
	AXGBE_AN_MODE_CL73 = 0,
	AXGBE_AN_MODE_CL73_REDRV,
	AXGBE_AN_MODE_CL37,
	AXGBE_AN_MODE_CL37_SGMII,
	AXGBE_AN_MODE_NONE,
};

enum axgbe_an {
	AXGBE_AN_READY = 0,
	AXGBE_AN_PAGE_RECEIVED,
	AXGBE_AN_INCOMPAT_LINK,
	AXGBE_AN_COMPLETE,
	AXGBE_AN_NO_LINK,
	AXGBE_AN_ERROR,
};

enum axgbe_rx {
	AXGBE_RX_BPA = 0,
	AXGBE_RX_XNP,
	AXGBE_RX_COMPLETE,
	AXGBE_RX_ERROR,
};

enum axgbe_mode {
	AXGBE_MODE_KX_1000 = 0,
	AXGBE_MODE_KX_2500,
	AXGBE_MODE_KR,
	AXGBE_MODE_X,
	AXGBE_MODE_SGMII_100,
	AXGBE_MODE_SGMII_1000,
	AXGBE_MODE_SFI,
	AXGBE_MODE_UNKNOWN,
};

enum axgbe_speedset {
	AXGBE_SPEEDSET_1000_10000 = 0,
	AXGBE_SPEEDSET_2500_10000,
};

enum axgbe_mdio_mode {
	AXGBE_MDIO_MODE_NONE = 0,
	AXGBE_MDIO_MODE_CL22,
	AXGBE_MDIO_MODE_CL45,
};

struct axgbe_phy {
	uint32_t supported;
	uint32_t advertising;
	uint32_t lp_advertising;

	int address;

	int autoneg;
	int speed;
	int duplex;

	int link;

	int pause_autoneg;
	int tx_pause;
	int rx_pause;
};

enum axgbe_i2c_cmd {
	AXGBE_I2C_CMD_READ = 0,
	AXGBE_I2C_CMD_WRITE,
};

struct axgbe_i2c_op {
	enum axgbe_i2c_cmd cmd;

	unsigned int target;

	uint8_t *buf;
	unsigned int len;
};

struct axgbe_i2c_op_state {
	struct axgbe_i2c_op *op;

	unsigned int tx_len;
	unsigned char *tx_buf;

	unsigned int rx_len;
	unsigned char *rx_buf;

	unsigned int tx_abort_source;

	int ret;
};

struct axgbe_i2c {
	unsigned int started;
	unsigned int max_speed_mode;
	unsigned int rx_fifo_size;
	unsigned int tx_fifo_size;

	struct axgbe_i2c_op_state op_state;
};

struct axgbe_hw_if {
	void (*config_flow_control)(struct axgbe_port *);
	int (*config_rx_mode)(struct axgbe_port *);

	int (*init)(struct axgbe_port *);

	int (*read_mmd_regs)(struct axgbe_port *, int, int);
	void (*write_mmd_regs)(struct axgbe_port *, int, int, int);
	int (*set_speed)(struct axgbe_port *, int);

	int (*set_ext_mii_mode)(struct axgbe_port *, unsigned int,
				enum axgbe_mdio_mode);
	int (*read_ext_mii_regs)(struct axgbe_port *, int, int);
	int (*write_ext_mii_regs)(struct axgbe_port *, int, int, uint16_t);

	/* For FLOW ctrl */
	int (*config_tx_flow_control)(struct axgbe_port *);
	int (*config_rx_flow_control)(struct axgbe_port *);

	/* vlan */
	int (*enable_rx_vlan_stripping)(struct axgbe_port *);
	int (*disable_rx_vlan_stripping)(struct axgbe_port *);
	int (*enable_rx_vlan_filtering)(struct axgbe_port *);
	int (*disable_rx_vlan_filtering)(struct axgbe_port *);
	int (*update_vlan_hash_table)(struct axgbe_port *);

	int (*exit)(struct axgbe_port *);
};

/* This structure represents implementation specific routines for an
 * implementation of a PHY. All routines are required unless noted below.
 *   Optional routines:
 *     kr_training_pre, kr_training_post
 */
struct axgbe_phy_impl_if {
	/* Perform Setup/teardown actions */
	int (*init)(struct axgbe_port *);
	void (*exit)(struct axgbe_port *);

	/* Perform start/stop specific actions */
	int (*reset)(struct axgbe_port *);
	int (*start)(struct axgbe_port *);
	void (*stop)(struct axgbe_port *);

	/* Return the link status */
	int (*link_status)(struct axgbe_port *, int *);

	/* Indicate if a particular speed is valid */
	int (*valid_speed)(struct axgbe_port *, int);

	/* Check if the specified mode can/should be used */
	bool (*use_mode)(struct axgbe_port *, enum axgbe_mode);
	/* Switch the PHY into various modes */
	void (*set_mode)(struct axgbe_port *, enum axgbe_mode);
	/* Retrieve mode needed for a specific speed */
	enum axgbe_mode (*get_mode)(struct axgbe_port *, int);
	/* Retrieve new/next mode when trying to auto-negotiate */
	enum axgbe_mode (*switch_mode)(struct axgbe_port *);
	/* Retrieve current mode */
	enum axgbe_mode (*cur_mode)(struct axgbe_port *);

	/* Retrieve current auto-negotiation mode */
	enum axgbe_an_mode (*an_mode)(struct axgbe_port *);

	/* Configure auto-negotiation settings */
	int (*an_config)(struct axgbe_port *);

	/* Set/override auto-negotiation advertisement settings */
	unsigned int (*an_advertising)(struct axgbe_port *port);

	/* Process results of auto-negotiation */
	enum axgbe_mode (*an_outcome)(struct axgbe_port *);

	/* Pre/Post auto-negotiation support */
	void (*an_pre)(struct axgbe_port *port);
	void (*an_post)(struct axgbe_port *port);

	/* Pre/Post KR training enablement support */
	void (*kr_training_pre)(struct axgbe_port *);
	void (*kr_training_post)(struct axgbe_port *);
};

struct axgbe_phy_if {
	/* For PHY setup/teardown */
	int (*phy_init)(struct axgbe_port *);
	void (*phy_exit)(struct axgbe_port *);

	/* For PHY support when setting device up/down */
	int (*phy_reset)(struct axgbe_port *);
	int (*phy_start)(struct axgbe_port *);
	void (*phy_stop)(struct axgbe_port *);

	/* For PHY support while device is up */
	void (*phy_status)(struct axgbe_port *);
	int (*phy_config_aneg)(struct axgbe_port *);

	/* For PHY settings validation */
	int (*phy_valid_speed)(struct axgbe_port *, int);
	/* For single interrupt support */
	void (*an_isr)(struct axgbe_port *);
	/* PHY implementation specific services */
	struct axgbe_phy_impl_if phy_impl;
};

struct axgbe_i2c_if {
	/* For initial I2C setup */
	int (*i2c_init)(struct axgbe_port *);

	/* For I2C support when setting device up/down */
	int (*i2c_start)(struct axgbe_port *);
	void (*i2c_stop)(struct axgbe_port *);

	/* For performing I2C operations */
	int (*i2c_xfer)(struct axgbe_port *, struct axgbe_i2c_op *);
};

/* This structure contains flags that indicate what hardware features
 * or configurations are present in the device.
 */
struct axgbe_hw_features {
	/* HW Version */
	unsigned int version;

	/* HW Feature Register0 */
	unsigned int gmii;		/* 1000 Mbps support */
	unsigned int vlhash;		/* VLAN Hash Filter */
	unsigned int sma;		/* SMA(MDIO) Interface */
	unsigned int rwk;		/* PMT remote wake-up packet */
	unsigned int mgk;		/* PMT magic packet */
	unsigned int mmc;		/* RMON module */
	unsigned int aoe;		/* ARP Offload */
	unsigned int ts;		/* IEEE 1588-2008 Advanced Timestamp */
	unsigned int eee;		/* Energy Efficient Ethernet */
	unsigned int tx_coe;		/* Tx Checksum Offload */
	unsigned int rx_coe;		/* Rx Checksum Offload */
	unsigned int addn_mac;		/* Additional MAC Addresses */
	unsigned int ts_src;		/* Timestamp Source */
	unsigned int sa_vlan_ins;	/* Source Address or VLAN Insertion */

	/* HW Feature Register1 */
	unsigned int rx_fifo_size;	/* MTL Receive FIFO Size */
	unsigned int tx_fifo_size;	/* MTL Transmit FIFO Size */
	unsigned int adv_ts_hi;		/* Advance Timestamping High Word */
	unsigned int dma_width;		/* DMA width */
	unsigned int dcb;		/* DCB Feature */
	unsigned int sph;		/* Split Header Feature */
	unsigned int tso;		/* TCP Segmentation Offload */
	unsigned int dma_debug;		/* DMA Debug Registers */
	unsigned int rss;		/* Receive Side Scaling */
	unsigned int tc_cnt;		/* Number of Traffic Classes */
	unsigned int hash_table_size;	/* Hash Table Size */
	unsigned int l3l4_filter_num;	/* Number of L3-L4 Filters */

	/* HW Feature Register2 */
	unsigned int rx_q_cnt;		/* Number of MTL Receive Queues */
	unsigned int tx_q_cnt;		/* Number of MTL Transmit Queues */
	unsigned int rx_ch_cnt;		/* Number of DMA Receive Channels */
	unsigned int tx_ch_cnt;		/* Number of DMA Transmit Channels */
	unsigned int pps_out_num;	/* Number of PPS outputs */
	unsigned int aux_snap_num;	/* Number of Aux snapshot inputs */

	/* HW Feature Register3 */
	unsigned int tx_q_vlan_tag_ins; /* Queue/Channel based VLAN tag */
					/* insertion on Tx Enable */
	unsigned int no_of_vlan_extn;   /* Number of Extended VLAN Tag */
					/* Filters Enabled */
};

struct axgbe_version_data {
	void (*init_function_ptrs_phy_impl)(struct axgbe_phy_if *);
	enum axgbe_xpcs_access xpcs_access;
	unsigned int mmc_64bit;
	unsigned int tx_max_fifo_size;
	unsigned int rx_max_fifo_size;
	unsigned int tx_tstamp_workaround;
	unsigned int ecc_support;
	unsigned int i2c_support;
	unsigned int an_cdr_workaround;
};

struct axgbe_mmc_stats {
	/* Tx Stats */
	uint64_t txoctetcount_gb;
	uint64_t txframecount_gb;
	uint64_t txbroadcastframes_g;
	uint64_t txmulticastframes_g;
	uint64_t tx64octets_gb;
	uint64_t tx65to127octets_gb;
	uint64_t tx128to255octets_gb;
	uint64_t tx256to511octets_gb;
	uint64_t tx512to1023octets_gb;
	uint64_t tx1024tomaxoctets_gb;
	uint64_t txunicastframes_gb;
	uint64_t txmulticastframes_gb;
	uint64_t txbroadcastframes_gb;
	uint64_t txunderflowerror;
	uint64_t txoctetcount_g;
	uint64_t txframecount_g;
	uint64_t txpauseframes;
	uint64_t txvlanframes_g;

	/* Rx Stats */
	uint64_t rxframecount_gb;
	uint64_t rxoctetcount_gb;
	uint64_t rxoctetcount_g;
	uint64_t rxbroadcastframes_g;
	uint64_t rxmulticastframes_g;
	uint64_t rxcrcerror;
	uint64_t rxrunterror;
	uint64_t rxjabbererror;
	uint64_t rxundersize_g;
	uint64_t rxoversize_g;
	uint64_t rx64octets_gb;
	uint64_t rx65to127octets_gb;
	uint64_t rx128to255octets_gb;
	uint64_t rx256to511octets_gb;
	uint64_t rx512to1023octets_gb;
	uint64_t rx1024tomaxoctets_gb;
	uint64_t rxunicastframes_g;
	uint64_t rxlengtherror;
	uint64_t rxoutofrangetype;
	uint64_t rxpauseframes;
	uint64_t rxfifooverflow;
	uint64_t rxvlanframes_gb;
	uint64_t rxwatchdogerror;
};

/* Flow control parameters */
struct xgbe_fc_info {
	uint32_t high_water[AXGBE_PRIORITY_QUEUES];
	uint32_t low_water[AXGBE_PRIORITY_QUEUES];
	uint16_t pause_time[AXGBE_PRIORITY_QUEUES];
	uint16_t send_xon;
	enum rte_eth_fc_mode mode;
	uint8_t autoneg;
};

/*
 * Structure to store private data for each port.
 */
struct axgbe_port {
	/*  Ethdev where port belongs*/
	struct rte_eth_dev *eth_dev;
	/* Pci dev info */
	const struct rte_pci_device *pci_dev;
	/* Version related data */
	struct axgbe_version_data *vdata;

	/* AXGMAC/XPCS related mmio registers */
	void *xgmac_regs;	/* AXGMAC CSRs */
	void *xpcs_regs;	/* XPCS MMD registers */
	void *xprop_regs;	/* AXGBE property registers */
	void *xi2c_regs;	/* AXGBE I2C CSRs */

	bool cdr_track_early;
	/* XPCS indirect addressing lock */
	unsigned int xpcs_window_def_reg;
	unsigned int xpcs_window_sel_reg;
	unsigned int xpcs_window;
	unsigned int xpcs_window_size;
	unsigned int xpcs_window_mask;

	/* Flags representing axgbe_state */
	uint32_t dev_state;

	struct axgbe_hw_if hw_if;
	struct axgbe_phy_if phy_if;
	struct axgbe_i2c_if i2c_if;

	/* AXI DMA settings */
	unsigned int coherent;
	unsigned int axdomain;
	unsigned int arcache;
	unsigned int awcache;

	unsigned int tx_max_channel_count;
	unsigned int rx_max_channel_count;
	unsigned int channel_count;
	unsigned int tx_ring_count;
	unsigned int tx_desc_count;
	unsigned int rx_ring_count;
	unsigned int rx_desc_count;

	unsigned int tx_max_q_count;
	unsigned int rx_max_q_count;
	unsigned int tx_q_count;
	unsigned int rx_q_count;

	/* Tx/Rx common settings */
	unsigned int pblx8;

	/* Tx settings */
	unsigned int tx_sf_mode;
	unsigned int tx_threshold;
	unsigned int tx_pbl;
	unsigned int tx_osp_mode;
	unsigned int tx_max_fifo_size;

	/* Rx settings */
	unsigned int rx_sf_mode;
	unsigned int rx_threshold;
	unsigned int rx_pbl;
	unsigned int rx_max_fifo_size;
	unsigned int rx_buf_size;

	/* Device clocks */
	unsigned long sysclk_rate;
	unsigned long ptpclk_rate;

	/* Keeps track of power mode */
	unsigned int power_down;

	/* Current PHY settings */
	int phy_link;
	int phy_speed;

	pthread_mutex_t xpcs_mutex;
	pthread_mutex_t i2c_mutex;
	pthread_mutex_t an_mutex;
	pthread_mutex_t phy_mutex;

	/* Flow control settings */
	unsigned int pause_autoneg;
	unsigned int tx_pause;
	unsigned int rx_pause;
	unsigned int rx_rfa[AXGBE_MAX_QUEUES];
	unsigned int rx_rfd[AXGBE_MAX_QUEUES];
	unsigned int fifo;
	unsigned int pfc_map[AXGBE_MAX_QUEUES];

	/* Receive Side Scaling settings */
	u8 rss_key[AXGBE_RSS_HASH_KEY_SIZE];
	uint32_t rss_table[AXGBE_RSS_MAX_TABLE_SIZE];
	uint32_t rss_options;
	int rss_enable;
	uint64_t rss_hf;

	/* Hardware features of the device */
	struct axgbe_hw_features hw_feat;

	struct rte_ether_addr mac_addr;

	/* Software Tx/Rx structure pointers*/
	void **rx_queues;
	void **tx_queues;

	/* MDIO/PHY related settings */
	unsigned int phy_started;
	void *phy_data;
	struct axgbe_phy phy;
	int mdio_mmd;
	unsigned long link_check;
	volatile int mdio_completion;

	unsigned int kr_redrv;

	/* Auto-negotiation state machine support */
	unsigned int an_int;
	unsigned int an_status;
	enum axgbe_an an_result;
	enum axgbe_an an_state;
	enum axgbe_rx kr_state;
	enum axgbe_rx kx_state;
	unsigned int an_supported;
	unsigned int parallel_detect;
	unsigned int fec_ability;
	unsigned long an_start;
	enum axgbe_an_mode an_mode;

	/* I2C support */
	struct axgbe_i2c i2c;
	volatile int i2c_complete;

	/* CRC stripping by H/w for Rx packet*/
	int crc_strip_enable;
	/* csum enable to hardware */
	uint32_t rx_csum_enable;

	struct axgbe_mmc_stats mmc_stats;
	struct xgbe_fc_info fc;

	/* Hash filtering */
	unsigned int hash_table_shift;
	unsigned int hash_table_count;
	unsigned int uc_hash_mac_addr;
	unsigned int uc_hash_table[AXGBE_MAC_HASH_TABLE_SIZE];

	/* Filtering support */
	unsigned long active_vlans[VLAN_TABLE_SIZE];

	/* For IEEE1588 PTP */
	struct rte_timecounter systime_tc;
	struct rte_timecounter tx_tstamp;
	unsigned int tstamp_addend;

};

void axgbe_init_function_ptrs_dev(struct axgbe_hw_if *hw_if);
void axgbe_init_function_ptrs_phy(struct axgbe_phy_if *phy_if);
void axgbe_init_function_ptrs_phy_v2(struct axgbe_phy_if *phy_if);
void axgbe_init_function_ptrs_i2c(struct axgbe_i2c_if *i2c_if);
void axgbe_set_mac_addn_addr(struct axgbe_port *pdata, u8 *addr,
			     uint32_t index);
void axgbe_set_mac_hash_table(struct axgbe_port *pdata, u8 *addr, bool add);
int axgbe_write_rss_lookup_table(struct axgbe_port *pdata);
int axgbe_write_rss_hash_key(struct axgbe_port *pdata);

#endif /* RTE_ETH_AXGBE_H_ */
