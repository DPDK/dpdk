/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_ETHDEV_H_
#define _HNS3_ETHDEV_H_

#include <pthread.h>
#include <ethdev_driver.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_spinlock.h>

#include "hns3_cmd.h"
#include "hns3_mbx.h"
#include "hns3_rss.h"
#include "hns3_fdir.h"
#include "hns3_stats.h"
#include "hns3_tm.h"
#include "hns3_flow.h"

/* Vendor ID */
#define PCI_VENDOR_ID_HUAWEI			0x19e5

/* Device IDs */
#define HNS3_DEV_ID_GE				0xA220
#define HNS3_DEV_ID_25GE			0xA221
#define HNS3_DEV_ID_25GE_RDMA			0xA222
#define HNS3_DEV_ID_50GE_RDMA			0xA224
#define HNS3_DEV_ID_100G_RDMA_MACSEC		0xA226
#define HNS3_DEV_ID_200G_RDMA			0xA228
#define HNS3_DEV_ID_100G_VF			0xA22E
#define HNS3_DEV_ID_100G_RDMA_PFC_VF		0xA22F

/* PCI Config offsets */
#define HNS3_PCI_REVISION_ID			0x08
#define HNS3_PCI_REVISION_ID_LEN		1

#define PCI_REVISION_ID_HIP08_B			0x21
#define PCI_REVISION_ID_HIP09_A			0x30

#define HNS3_PF_FUNC_ID			0
#define HNS3_1ST_VF_FUNC_ID		1

#define HNS3_DEFAULT_PORT_CONF_BURST_SIZE	32
#define HNS3_DEFAULT_PORT_CONF_QUEUES_NUM	1

#define HNS3_SW_SHIFT_AND_DISCARD_MODE		0
#define HNS3_HW_SHIFT_AND_DISCARD_MODE		1

#define HNS3_UNLIMIT_PROMISC_MODE       0
#define HNS3_LIMIT_PROMISC_MODE         1

#define HNS3_SPECIAL_PORT_SW_CKSUM_MODE         0
#define HNS3_SPECIAL_PORT_HW_CKSUM_MODE         1

#define HNS3_UC_MACADDR_NUM		128
#define HNS3_VF_UC_MACADDR_NUM		48
#define HNS3_MC_MACADDR_NUM		128

#define HNS3_MAX_BD_SIZE		65535
#define HNS3_MAX_NON_TSO_BD_PER_PKT	8
#define HNS3_MAX_TSO_BD_PER_PKT		63
#define HNS3_MAX_FRAME_LEN		9728
#define HNS3_DEFAULT_RX_BUF_LEN		2048
#define HNS3_MAX_BD_PAYLEN		(1024 * 1024 - 1)
#define HNS3_MAX_TSO_HDR_SIZE		512
#define HNS3_MAX_TSO_HDR_BD_NUM		3
#define HNS3_MAX_LRO_SIZE		64512

#define HNS3_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_VLAN_HLEN * 2)
#define HNS3_PKTLEN_TO_MTU(pktlen)	((pktlen) - HNS3_ETH_OVERHEAD)
#define HNS3_MAX_MTU	(HNS3_MAX_FRAME_LEN - HNS3_ETH_OVERHEAD)
#define HNS3_DEFAULT_MTU		1500UL
#define HNS3_DEFAULT_FRAME_LEN		(HNS3_DEFAULT_MTU + HNS3_ETH_OVERHEAD)
#define HNS3_HIP08_MIN_TX_PKT_LEN	33
#define HNS3_HIP09_MIN_TX_PKT_LEN	9

#define HNS3_BITS_PER_BYTE	8

#define HNS3_4_TCS			4
#define HNS3_8_TCS			8

#define HNS3_MAX_PF_NUM			8
#define HNS3_UMV_TBL_SIZE		3072
#define HNS3_DEFAULT_UMV_SPACE_PER_PF \
	(HNS3_UMV_TBL_SIZE / HNS3_MAX_PF_NUM)

#define HNS3_PF_CFG_BLOCK_SIZE		32
#define HNS3_PF_CFG_DESC_NUM \
	(HNS3_PF_CFG_BLOCK_SIZE / HNS3_CFG_RD_LEN_BYTES)

#define HNS3_DEFAULT_ENABLE_PFC_NUM	0

#define HNS3_INTR_UNREG_FAIL_RETRY_CNT	5
#define HNS3_INTR_UNREG_FAIL_DELAY_MS	500

#define HNS3_QUIT_RESET_CNT		10
#define HNS3_QUIT_RESET_DELAY_MS	100

#define HNS3_POLL_RESPONE_MS		1

#define HNS3_MAX_USER_PRIO		8
#define HNS3_PG_NUM			4
enum hns3_fc_mode {
	HNS3_FC_NONE,
	HNS3_FC_RX_PAUSE,
	HNS3_FC_TX_PAUSE,
	HNS3_FC_FULL,
	HNS3_FC_DEFAULT
};

#define HNS3_SCH_MODE_SP	0
#define HNS3_SCH_MODE_DWRR	1
struct hns3_pg_info {
	uint8_t pg_id;
	uint8_t pg_sch_mode;  /* 0: sp; 1: dwrr */
	uint8_t tc_bit_map;
	uint32_t bw_limit;
	uint8_t tc_dwrr[HNS3_MAX_TC_NUM];
};

struct hns3_tc_info {
	uint8_t tc_id;
	uint8_t tc_sch_mode;  /* 0: sp; 1: dwrr */
	uint8_t pgid;
	uint32_t bw_limit;
	uint8_t up_to_tc_map; /* user priority mapping on the TC */
};

struct hns3_dcb_info {
	uint8_t num_tc;
	uint8_t num_pg;     /* It must be 1 if vNET-Base schd */
	uint8_t pg_dwrr[HNS3_PG_NUM];
	uint8_t prio_tc[HNS3_MAX_USER_PRIO];
	struct hns3_pg_info pg_info[HNS3_PG_NUM];
	struct hns3_tc_info tc_info[HNS3_MAX_TC_NUM];
	uint8_t hw_pfc_map; /* Allow for packet drop or not on this TC */
	uint8_t pfc_en; /* Pfc enabled or not for user priority */
};

enum hns3_fc_status {
	HNS3_FC_STATUS_NONE,
	HNS3_FC_STATUS_MAC_PAUSE,
	HNS3_FC_STATUS_PFC,
};

struct hns3_tc_queue_info {
	uint16_t tqp_offset;    /* TQP offset from base TQP */
	uint16_t tqp_count;     /* Total TQPs */
	uint8_t tc;             /* TC index */
	bool enable;            /* If this TC is enable or not */
};

struct hns3_cfg {
	uint8_t tc_num;
	uint16_t tqp_desc_num;
	uint16_t rx_buf_len;
	uint16_t rss_size_max;
	uint8_t phy_addr;
	uint8_t media_type;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t default_speed;
	uint32_t numa_node_map;
	uint8_t speed_ability;
	uint16_t umv_space;
};

struct hns3_set_link_speed_cfg {
	uint32_t speed;
	uint8_t duplex  : 1;
	uint8_t autoneg : 1;
};

/* mac media type */
enum hns3_media_type {
	HNS3_MEDIA_TYPE_UNKNOWN,
	HNS3_MEDIA_TYPE_FIBER,
	HNS3_MEDIA_TYPE_COPPER,
	HNS3_MEDIA_TYPE_BACKPLANE,
	HNS3_MEDIA_TYPE_NONE,
};

#define HNS3_DEFAULT_QUERY		0
#define HNS3_ACTIVE_QUERY		1

struct hns3_mac {
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t media_type;
	uint8_t phy_addr;
	uint8_t link_duplex  : 1; /* RTE_ETH_LINK_[HALF/FULL]_DUPLEX */
	uint8_t link_autoneg : 1; /* RTE_ETH_LINK_[AUTONEG/FIXED] */
	uint8_t link_status  : 1; /* RTE_ETH_LINK_[DOWN/UP] */
	uint32_t link_speed;      /* RTE_ETH_SPEED_NUM_ */
	/*
	 * Some firmware versions support only the SFP speed query. In addition
	 * to the SFP speed query, some firmware supports the query of the speed
	 * capability, auto-negotiation capability, and FEC mode, which can be
	 * selected by the 'query_type' filed in the HNS3_OPC_GET_SFP_INFO CMD.
	 * This field is used to record the SFP information query mode.
	 * Value range:
	 *       HNS3_DEFAULT_QUERY/HNS3_ACTIVE_QUERY
	 *
	 * - HNS3_DEFAULT_QUERY
	 * Speed obtained is from SFP. When the queried speed changes, the MAC
	 * speed needs to be reconfigured.
	 *
	 * - HNS3_ACTIVE_QUERY
	 * Speed obtained is from MAC. At this time, it is unnecessary for
	 * driver to reconfigured the MAC speed. In addition, more information,
	 * such as, the speed capability, auto-negotiation capability and FEC
	 * mode, can be obtained by the HNS3_OPC_GET_SFP_INFO CMD.
	 */
	uint8_t query_type;
	uint32_t supported_speed;  /* supported speed for current media type */
	uint32_t advertising;     /* advertised capability in the local part */
	uint32_t lp_advertising; /* advertised capability in the link partner */
	uint8_t support_autoneg;
};

struct hns3_fake_queue_data {
	void **rx_queues; /* Array of pointers to fake RX queues. */
	void **tx_queues; /* Array of pointers to fake TX queues. */
	uint16_t nb_fake_rx_queues; /* Number of fake RX queues. */
	uint16_t nb_fake_tx_queues; /* Number of fake TX queues. */
};

#define HNS3_PORT_BASE_VLAN_DISABLE	0
#define HNS3_PORT_BASE_VLAN_ENABLE	1
struct hns3_port_base_vlan_config {
	uint16_t state;
	uint16_t pvid;
};

/* Primary process maintains driver state in main thread.
 *
 * +---------------+
 * | UNINITIALIZED |<-----------+
 * +---------------+		|
 *	|.eth_dev_init		|.eth_dev_uninit
 *	V			|
 * +---------------+------------+
 * |  INITIALIZED  |
 * +---------------+<-----------<---------------+
 *	|.dev_configure		|		|
 *	V			|failed		|
 * +---------------+------------+		|
 * |  CONFIGURING  |				|
 * +---------------+----+			|
 *	|success	|			|
 *	|		|		+---------------+
 *	|		|		|    CLOSING    |
 *	|		|		+---------------+
 *	|		|			^
 *	V		|.dev_configure		|
 * +---------------+----+			|.dev_close
 * |  CONFIGURED   |----------------------------+
 * +---------------+<-----------+
 *	|.dev_start		|
 *	V			|
 * +---------------+		|
 * |   STARTING    |------------^
 * +---------------+ failed	|
 *	|success		|
 *	|		+---------------+
 *	|		|   STOPPING    |
 *	|		+---------------+
 *	|			^
 *	V			|.dev_stop
 * +---------------+------------+
 * |    STARTED    |
 * +---------------+
 */
enum hns3_adapter_state {
	HNS3_NIC_UNINITIALIZED = 0,
	HNS3_NIC_INITIALIZED,
	HNS3_NIC_CONFIGURING,
	HNS3_NIC_CONFIGURED,
	HNS3_NIC_STARTING,
	HNS3_NIC_STARTED,
	HNS3_NIC_STOPPING,
	HNS3_NIC_CLOSING,
	HNS3_NIC_CLOSED,
	HNS3_NIC_REMOVED,
	HNS3_NIC_NSTATES
};

/* Reset various stages, execute in order */
enum hns3_reset_stage {
	/* Stop query services, stop transceiver, disable MAC */
	RESET_STAGE_DOWN,
	/* Clear reset completion flags, disable send command */
	RESET_STAGE_PREWAIT,
	/* Inform IMP to start resetting */
	RESET_STAGE_REQ_HW_RESET,
	/* Waiting for hardware reset to complete */
	RESET_STAGE_WAIT,
	/* Reinitialize hardware */
	RESET_STAGE_DEV_INIT,
	/* Restore user settings and enable MAC */
	RESET_STAGE_RESTORE,
	/* Restart query services, start transceiver */
	RESET_STAGE_DONE,
	/* Not in reset state */
	RESET_STAGE_NONE,
};

enum hns3_reset_level {
	HNS3_FLR_RESET,     /* A VF perform FLR reset */
	HNS3_VF_FUNC_RESET, /* A VF function reset */

	/*
	 * All VFs under a PF perform function reset.
	 * Kernel PF driver use mailbox to inform DPDK VF to do reset, the value
	 * of the reset level and the one defined in kernel driver should be
	 * same.
	 */
	HNS3_VF_PF_FUNC_RESET = 2,

	/*
	 * All VFs under a PF perform FLR reset.
	 * Kernel PF driver use mailbox to inform DPDK VF to do reset, the value
	 * of the reset level and the one defined in kernel driver should be
	 * same.
	 *
	 * According to the protocol of PCIe, FLR to a PF resets the PF state as
	 * well as the SR-IOV extended capability including VF Enable which
	 * means that VFs no longer exist.
	 *
	 * In PF FLR, the register state of VF is not reliable, VF's driver
	 * should not access the registers of the VF device.
	 */
	HNS3_VF_FULL_RESET,

	/* All VFs under the rootport perform a global or IMP reset */
	HNS3_VF_RESET,

	/*
	 * The enumeration value of HNS3_FUNC_RESET/HNS3_GLOBAL_RESET/
	 * HNS3_IMP_RESET/HNS3_NONE_RESET are also used by firmware, and
	 * can not be changed.
	 */

	HNS3_FUNC_RESET = 5,    /* A PF function reset */

	/* All PFs under the rootport perform a global reset */
	HNS3_GLOBAL_RESET,
	HNS3_IMP_RESET,     /* All PFs under the rootport perform a IMP reset */
	HNS3_NONE_RESET,
	HNS3_MAX_RESET
};

enum hns3_wait_result {
	HNS3_WAIT_UNKNOWN,
	HNS3_WAIT_REQUEST,
	HNS3_WAIT_SUCCESS,
	HNS3_WAIT_TIMEOUT
};

#define HNS3_RESET_SYNC_US 100000

struct hns3_reset_stats {
	uint64_t request_cnt; /* Total request reset times */
	uint64_t global_cnt;  /* Total GLOBAL reset times */
	uint64_t imp_cnt;     /* Total IMP reset times */
	uint64_t exec_cnt;    /* Total reset executive times */
	uint64_t success_cnt; /* Total reset successful times */
	uint64_t fail_cnt;    /* Total reset failed times */
	uint64_t merge_cnt;   /* Total merged in high reset times */
};

typedef bool (*check_completion_func)(struct hns3_hw *hw);

struct hns3_wait_data {
	void *hns;
	uint64_t end_ms;
	uint64_t interval;
	int16_t count;
	enum hns3_wait_result result;
	check_completion_func check_completion;
};

struct hns3_reset_ops {
	void (*reset_service)(void *arg);
	int (*stop_service)(struct hns3_adapter *hns);
	int (*prepare_reset)(struct hns3_adapter *hns);
	int (*wait_hardware_ready)(struct hns3_adapter *hns);
	int (*reinit_dev)(struct hns3_adapter *hns);
	int (*restore_conf)(struct hns3_adapter *hns);
	int (*start_service)(struct hns3_adapter *hns);
};

enum hns3_schedule {
	SCHEDULE_NONE,
	SCHEDULE_PENDING,
	SCHEDULE_REQUESTED,
	SCHEDULE_DEFERRED,
};

struct hns3_reset_data {
	enum hns3_reset_stage stage;
	uint16_t schedule;
	/* Reset flag, covering the entire reset process */
	uint16_t resetting;
	/* Used to disable sending cmds during reset */
	uint16_t disable_cmd;
	/* The reset level being processed */
	enum hns3_reset_level level;
	/* Reset level set, each bit represents a reset level */
	uint64_t pending;
	/* Request reset level set, from interrupt or mailbox */
	uint64_t request;
	int attempts; /* Reset failure retry */
	int retries;  /* Timeout failure retry in reset_post */
	/*
	 * At the time of global or IMP reset, the command cannot be sent to
	 * stop the tx/rx queues. Tx/Rx queues may be access mbuf during the
	 * reset process, so the mbuf is required to be released after the reset
	 * is completed.The mbuf_deferred_free is used to mark whether mbuf
	 * needs to be released.
	 */
	bool mbuf_deferred_free;
	struct timeval start_time;
	struct hns3_reset_stats stats;
	const struct hns3_reset_ops *ops;
	struct hns3_wait_data *wait_data;
};

struct hns3_hw_ops {
	int (*add_mc_mac_addr)(struct hns3_hw *hw,
				struct rte_ether_addr *mac_addr);
	int (*del_mc_mac_addr)(struct hns3_hw *hw,
				struct rte_ether_addr *mac_addr);
	int (*add_uc_mac_addr)(struct hns3_hw *hw,
				struct rte_ether_addr *mac_addr);
	int (*del_uc_mac_addr)(struct hns3_hw *hw,
				struct rte_ether_addr *mac_addr);
	int (*bind_ring_with_vector)(struct hns3_hw *hw, uint16_t vector_id,
				bool en, enum hns3_ring_type queue_type,
				uint16_t queue_id);
};

#define HNS3_INTR_MAPPING_VEC_RSV_ONE		0
#define HNS3_INTR_MAPPING_VEC_ALL		1

#define HNS3_INTR_COALESCE_GL_UINT_2US		0
#define HNS3_INTR_COALESCE_GL_UINT_1US		1

#define HNS3_INTR_QL_NONE			0

struct hns3_queue_intr {
	/*
	 * interrupt mapping mode.
	 * value range:
	 *      HNS3_INTR_MAPPING_VEC_RSV_ONE/HNS3_INTR_MAPPING_VEC_ALL
	 *
	 *  - HNS3_INTR_MAPPING_VEC_RSV_ONE
	 *     For some versions of hardware network engine, because of the
	 *     hardware constraint, we need implement clearing the mapping
	 *     relationship configurations by binding all queues to the last
	 *     interrupt vector and reserving the last interrupt vector. This
	 *     method results in a decrease of the maximum queues when upper
	 *     applications call the rte_eth_dev_configure API function to
	 *     enable Rx interrupt.
	 *
	 *  - HNS3_INTR_MAPPING_VEC_ALL
	 *     PMD can map/unmmap all interrupt vectors with queues when
	 *     Rx interrupt is enabled.
	 */
	uint8_t mapping_mode;
	/*
	 * The unit of GL(gap limiter) configuration for interrupt coalesce of
	 * queue's interrupt.
	 * value range:
	 *      HNS3_INTR_COALESCE_GL_UINT_2US/HNS3_INTR_COALESCE_GL_UINT_1US
	 */
	uint8_t gl_unit;
	/* The max QL(quantity limiter) value */
	uint16_t int_ql_max;
};

#define HNS3_TSO_SW_CAL_PSEUDO_H_CSUM		0
#define HNS3_TSO_HW_CAL_PSEUDO_H_CSUM		1

#define HNS3_PKTS_DROP_STATS_MODE1		0
#define HNS3_PKTS_DROP_STATS_MODE2		1

struct hns3_hw {
	struct rte_eth_dev_data *data;
	void *io_base;
	uint8_t revision;           /* PCI revision, low byte of class word */
	struct hns3_cmq cmq;
	struct hns3_mbx_resp_status mbx_resp; /* mailbox response */
	struct hns3_mac mac;
	/*
	 * This flag indicates dev_set_link_down() API is called, and is cleared
	 * by dev_set_link_up() or dev_start().
	 */
	bool set_link_down;
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
	struct hns3_tqp_stats tqp_stats;
	/* Include Mac stats | Rx stats | Tx stats */
	struct hns3_mac_stats mac_stats;
	uint32_t mac_stats_reg_num;
	struct hns3_rx_missed_stats imissed_stats;
	uint64_t oerror_stats;
	/*
	 * The lock is used to protect statistics update in stats APIs and
	 * periodic task.
	 */
	rte_spinlock_t stats_lock;

	uint32_t fw_version;
	uint16_t pf_vf_if_version;  /* version of communication interface */

	uint16_t num_msi;
	uint16_t total_tqps_num;    /* total task queue pairs of this PF */
	uint16_t tqps_num;          /* num task queue pairs of this function */
	uint16_t intr_tqps_num;     /* num queue pairs mapping interrupt */
	uint16_t rss_size_max;      /* HW defined max RSS task queue */
	uint16_t rx_buf_len;        /* hold min hardware rx buf len */
	uint16_t num_tx_desc;       /* desc num of per tx queue */
	uint16_t num_rx_desc;       /* desc num of per rx queue */
	uint32_t mng_entry_num;     /* number of manager table entry */
	uint32_t mac_entry_num;     /* number of mac-vlan table entry */

	struct rte_ether_addr mc_addrs[HNS3_MC_MACADDR_NUM];
	int mc_addrs_num; /* Multicast mac addresses number */

	/* The configuration info of RSS */
	struct hns3_rss_conf rss_info;
	uint16_t rss_ind_tbl_size;
	uint16_t rss_key_size;

	uint8_t num_tc;             /* Total number of enabled TCs */
	uint8_t hw_tc_map;
	enum hns3_fc_mode requested_fc_mode; /* FC mode requested by user */
	struct hns3_dcb_info dcb_info;
	enum hns3_fc_status current_fc_status; /* current flow control status */
	struct hns3_tc_queue_info tc_queue[HNS3_MAX_TC_NUM];
	uint16_t used_rx_queues;
	uint16_t used_tx_queues;

	/* Config max queue numbers between rx and tx queues from user */
	uint16_t cfg_max_queues;
	struct hns3_fake_queue_data fkq_data;     /* fake queue data */
	uint16_t alloc_rss_size;    /* RX queue number per TC */
	uint16_t tx_qnum_per_tc;    /* TX queue number per TC */

	uint32_t capability;
	uint32_t max_tm_rate;
	/*
	 * The minimum length of the packet supported by hardware in the Tx
	 * direction.
	 */
	uint32_t min_tx_pkt_len;

	struct hns3_queue_intr intr;
	/*
	 * tso mode.
	 * value range:
	 *      HNS3_TSO_SW_CAL_PSEUDO_H_CSUM/HNS3_TSO_HW_CAL_PSEUDO_H_CSUM
	 *
	 *  - HNS3_TSO_SW_CAL_PSEUDO_H_CSUM
	 *     In this mode, because of the hardware constraint, network driver
	 *     software need erase the L4 len value of the TCP pseudo header
	 *     and recalculate the TCP pseudo header checksum of packets that
	 *     need TSO.
	 *
	 *  - HNS3_TSO_HW_CAL_PSEUDO_H_CSUM
	 *     In this mode, hardware support recalculate the TCP pseudo header
	 *     checksum of packets that need TSO, so network driver software
	 *     not need to recalculate it.
	 */
	uint8_t tso_mode;
	/*
	 * vlan mode.
	 * value range:
	 *      HNS3_SW_SHIFT_AND_DISCARD_MODE/HNS3_HW_SHIFT_AND_DISCARD_MODE
	 *
	 *  - HNS3_SW_SHIFT_AND_DISCARD_MODE
	 *     For some versions of hardware network engine, because of the
	 *     hardware limitation, PMD needs to detect the PVID status
	 *     to work with hardware to implement PVID-related functions.
	 *     For example, driver need discard the stripped PVID tag to ensure
	 *     the PVID will not report to mbuf and shift the inserted VLAN tag
	 *     to avoid port based VLAN covering it.
	 *
	 *  - HNS3_HW_SHIT_AND_DISCARD_MODE
	 *     PMD does not need to process PVID-related functions in
	 *     I/O process, Hardware will adjust the sequence between port based
	 *     VLAN tag and BD VLAN tag automatically and VLAN tag stripped by
	 *     PVID will be invisible to driver. And in this mode, hns3 is able
	 *     to send a multi-layer VLAN packets when hw VLAN insert offload
	 *     is enabled.
	 */
	uint8_t vlan_mode;
	/*
	 * promisc mode.
	 * value range:
	 *      HNS3_UNLIMIT_PROMISC_MODE/HNS3_LIMIT_PROMISC_MODE
	 *
	 *  - HNS3_UNLIMIT_PROMISC_MODE
	 *     In this mode, TX unicast promisc will be configured when promisc
	 *     is set, driver can receive all the ingress and outgoing traffic.
	 *     In the words, all the ingress packets, all the packets sent from
	 *     the PF and other VFs on the same physical port.
	 *
	 *  - HNS3_LIMIT_PROMISC_MODE
	 *     In this mode, TX unicast promisc is shutdown when promisc mode
	 *     is set. So, driver will only receive all the ingress traffic.
	 *     The packets sent from the PF and other VFs on the same physical
	 *     port won't be copied to the function which has set promisc mode.
	 */
	uint8_t promisc_mode;

	/*
	 * drop_stats_mode mode.
	 * value range:
	 *      HNS3_PKTS_DROP_STATS_MODE1/HNS3_PKTS_DROP_STATS_MODE2
	 *
	 *  - HNS3_PKTS_DROP_STATS_MODE1
	 *     This mode for kunpeng920. In this mode, port level imissed stats
	 *     is supported. It only includes RPU drop stats.
	 *
	 *  - HNS3_PKTS_DROP_STATS_MODE2
	 *     This mode for kunpeng930. In this mode, imissed stats and oerrors
	 *     stats is supported. Function level imissed stats is supported. It
	 *     includes RPU drop stats in VF, and includes both RPU drop stats
	 *     and SSU drop stats in PF. Oerror stats is also supported in PF.
	 */
	uint8_t drop_stats_mode;

	uint8_t max_non_tso_bd_num; /* max BD number of one non-TSO packet */
	/*
	 * udp checksum mode.
	 * value range:
	 *      HNS3_SPECIAL_PORT_HW_CKSUM_MODE/HNS3_SPECIAL_PORT_SW_CKSUM_MODE
	 *
	 *  - HNS3_SPECIAL_PORT_SW_CKSUM_MODE
	 *     In this mode, HW can not do checksum for special UDP port like
	 *     4789, 4790, 6081 for non-tunnel UDP packets and UDP tunnel
	 *     packets without the RTE_MBUF_F_TX_TUNEL_MASK in the mbuf. So, PMD need
	 *     do the checksum for these packets to avoid a checksum error.
	 *
	 *  - HNS3_SPECIAL_PORT_HW_CKSUM_MODE
	 *     In this mode, HW does not have the preceding problems and can
	 *     directly calculate the checksum of these UDP packets.
	 */
	uint8_t udp_cksum_mode;

	struct hns3_port_base_vlan_config port_base_vlan_cfg;

	pthread_mutex_t flows_lock; /* rte_flow ops lock */
	struct hns3_fdir_rule_list flow_fdir_list; /* flow fdir rule list */
	struct hns3_rss_filter_list flow_rss_list; /* flow RSS rule list */
	struct hns3_flow_mem_list flow_list;

	struct hns3_hw_ops ops;

	/*
	 * PMD setup and configuration is not thread safe. Since it is not
	 * performance sensitive, it is better to guarantee thread-safety
	 * and add device level lock. Adapter control operations which
	 * change its state should acquire the lock.
	 */
	rte_spinlock_t lock;
	enum hns3_adapter_state adapter_state;
	struct hns3_reset_data reset;
};

#define HNS3_FLAG_TC_BASE_SCH_MODE		1
#define HNS3_FLAG_VNET_BASE_SCH_MODE		2

/* vlan entry information. */
struct hns3_user_vlan_table {
	LIST_ENTRY(hns3_user_vlan_table) next;
	bool hd_tbl_status;
	uint16_t vlan_id;
};

/* Vlan tag configuration for RX direction */
struct hns3_rx_vtag_cfg {
	bool rx_vlan_offload_en;    /* Whether enable rx vlan offload */
	bool strip_tag1_en;         /* Whether strip inner vlan tag */
	bool strip_tag2_en;         /* Whether strip outer vlan tag */
	/*
	 * If strip_tag_en is enabled, this bit decide whether to map the vlan
	 * tag to descriptor.
	 */
	bool strip_tag1_discard_en;
	bool strip_tag2_discard_en;
	/*
	 * If this bit is enabled, only map inner/outer priority to descriptor
	 * and the vlan tag is always 0.
	 */
	bool vlan1_vlan_prionly;
	bool vlan2_vlan_prionly;
};

/* Vlan tag configuration for TX direction */
struct hns3_tx_vtag_cfg {
	bool accept_tag1;           /* Whether accept tag1 packet from host */
	bool accept_untag1;         /* Whether accept untag1 packet from host */
	bool accept_tag2;
	bool accept_untag2;
	bool insert_tag1_en;        /* Whether insert outer vlan tag */
	bool insert_tag2_en;        /* Whether insert inner vlan tag */
	/*
	 * In shift mode, hw will shift the sequence of port based VLAN and
	 * BD VLAN.
	 */
	bool tag_shift_mode_en;     /* hw shift vlan tag automatically */
	uint16_t default_tag1;      /* The default outer vlan tag to insert */
	uint16_t default_tag2;      /* The default inner vlan tag to insert */
};

struct hns3_vtag_cfg {
	struct hns3_rx_vtag_cfg rx_vcfg;
	struct hns3_tx_vtag_cfg tx_vcfg;
};

/* Request types for IPC. */
enum hns3_mp_req_type {
	HNS3_MP_REQ_START_RXTX = 1,
	HNS3_MP_REQ_STOP_RXTX,
	HNS3_MP_REQ_START_TX,
	HNS3_MP_REQ_STOP_TX,
	HNS3_MP_REQ_MAX
};

/* Parameters for IPC. */
struct hns3_mp_param {
	enum hns3_mp_req_type type;
	int port_id;
	int result;
};

/* Request timeout for IPC. */
#define HNS3_MP_REQ_TIMEOUT_SEC 5

/* Key string for IPC. */
#define HNS3_MP_NAME "net_hns3_mp"

#define HNS3_L2TBL_NUM	4
#define HNS3_L3TBL_NUM	16
#define HNS3_L4TBL_NUM	16
#define HNS3_OL2TBL_NUM	4
#define HNS3_OL3TBL_NUM	16
#define HNS3_OL4TBL_NUM	16
#define HNS3_PTYPE_NUM	256

struct hns3_ptype_table {
	/*
	 * The next fields used to calc packet-type by the
	 * L3_ID/L4_ID/OL3_ID/OL4_ID from the Rx descriptor.
	 */
	uint32_t l3table[HNS3_L3TBL_NUM];
	uint32_t l4table[HNS3_L4TBL_NUM];
	uint32_t inner_l3table[HNS3_L3TBL_NUM];
	uint32_t inner_l4table[HNS3_L4TBL_NUM];
	uint32_t ol3table[HNS3_OL3TBL_NUM];
	uint32_t ol4table[HNS3_OL4TBL_NUM];

	/*
	 * The next field used to calc packet-type by the PTYPE from the Rx
	 * descriptor, it functions only when firmware report the capability of
	 * HNS3_CAPS_RXD_ADV_LAYOUT_B and driver enabled it.
	 */
	uint32_t ptype[HNS3_PTYPE_NUM] __rte_cache_aligned;
};

#define HNS3_FIXED_MAX_TQP_NUM_MODE		0
#define HNS3_FLEX_MAX_TQP_NUM_MODE		1

struct hns3_pf {
	struct hns3_adapter *adapter;
	bool is_main_pf;
	uint16_t func_num; /* num functions of this pf, include pf and vfs */

	/*
	 * tqp_config mode
	 * tqp_config_mode value range:
	 *	HNS3_FIXED_MAX_TQP_NUM_MODE,
	 *	HNS3_FLEX_MAX_TQP_NUM_MODE
	 *
	 * - HNS3_FIXED_MAX_TQP_NUM_MODE
	 *   There is a limitation on the number of pf interrupts available for
	 *   on some versions of network engines. In this case, the maximum
	 *   queue number of pf can not be greater than the interrupt number,
	 *   such as pf of network engine with revision_id 0x21. So the maximum
	 *   number of queues must be fixed.
	 *
	 * - HNS3_FLEX_MAX_TQP_NUM_MODE
	 *   In this mode, the maximum queue number of pf has not any constraint
	 *   and comes from the macro RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF
	 *   in the config file. Users can modify the macro according to their
	 *   own application scenarios, which is more flexible to use.
	 */
	uint8_t tqp_config_mode;

	uint32_t pkt_buf_size; /* Total pf buf size for tx/rx */
	uint32_t tx_buf_size; /* Tx buffer size for each TC */
	uint32_t dv_buf_size; /* Dv buffer size for each TC */

	uint16_t mps; /* Max packet size */

	uint8_t tx_sch_mode;
	uint8_t tc_max; /* max number of tc driver supported */
	uint8_t local_max_tc; /* max number of local tc */
	uint8_t pfc_max;
	uint8_t prio_tc[HNS3_MAX_USER_PRIO]; /* TC indexed by prio */
	uint16_t pause_time;
	bool support_fc_autoneg;       /* support FC autonegotiate */
	bool support_multi_tc_pause;

	uint16_t wanted_umv_size;
	uint16_t max_umv_size;
	uint16_t used_umv_size;

	bool support_sfp_query;
	uint32_t fec_mode; /* current FEC mode for ethdev */

	bool ptp_enable;

	/* Stores timestamp of last received packet on dev */
	uint64_t rx_timestamp;

	struct hns3_vtag_cfg vtag_config;
	LIST_HEAD(vlan_tbl, hns3_user_vlan_table) vlan_list;

	struct hns3_fdir_info fdir; /* flow director info */
	LIST_HEAD(counters, hns3_flow_counter) flow_counters;

	struct hns3_tm_conf tm_conf;
};

enum {
	HNS3_PF_PUSH_LSC_CAP_NOT_SUPPORTED,
	HNS3_PF_PUSH_LSC_CAP_SUPPORTED,
	HNS3_PF_PUSH_LSC_CAP_UNKNOWN
};

struct hns3_vf {
	struct hns3_adapter *adapter;

	/* Whether PF support push link status change to VF */
	uint16_t pf_push_lsc_cap;

	/*
	 * If PF support push link status change, VF still need send request to
	 * get link status in some cases (such as reset recover stage), so use
	 * the req_link_info_cnt to control max request count.
	 */
	uint16_t req_link_info_cnt;

	uint16_t poll_job_started; /* whether poll job is started */
};

struct hns3_adapter {
	struct hns3_hw hw;

	/* Specific for PF or VF */
	bool is_vf; /* false - PF, true - VF */
	union {
		struct hns3_pf pf;
		struct hns3_vf vf;
	};

	uint32_t rx_func_hint;
	uint32_t tx_func_hint;

	uint64_t dev_caps_mask;
	uint16_t mbx_time_limit_ms; /* wait time for mbx message */

	struct hns3_ptype_table ptype_tbl __rte_cache_aligned;
};

#define HNS3_DEVARG_RX_FUNC_HINT	"rx_func_hint"
#define HNS3_DEVARG_TX_FUNC_HINT	"tx_func_hint"

#define HNS3_DEVARG_DEV_CAPS_MASK	"dev_caps_mask"

#define HNS3_DEVARG_MBX_TIME_LIMIT_MS	"mbx_time_limit_ms"

enum {
	HNS3_DEV_SUPPORT_DCB_B,
	HNS3_DEV_SUPPORT_COPPER_B,
	HNS3_DEV_SUPPORT_FD_QUEUE_REGION_B,
	HNS3_DEV_SUPPORT_PTP_B,
	HNS3_DEV_SUPPORT_TX_PUSH_B,
	HNS3_DEV_SUPPORT_INDEP_TXRX_B,
	HNS3_DEV_SUPPORT_STASH_B,
	HNS3_DEV_SUPPORT_RXD_ADV_LAYOUT_B,
	HNS3_DEV_SUPPORT_OUTER_UDP_CKSUM_B,
	HNS3_DEV_SUPPORT_RAS_IMP_B,
	HNS3_DEV_SUPPORT_TM_B,
	HNS3_DEV_SUPPORT_VF_VLAN_FLT_MOD_B,
};

#define hns3_dev_get_support(hw, _name) \
	hns3_get_bit((hw)->capability, HNS3_DEV_SUPPORT_##_name##_B)

#define HNS3_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct hns3_adapter *)adapter)->hw)
#define HNS3_DEV_PRIVATE_TO_PF(adapter) \
	(&((struct hns3_adapter *)adapter)->pf)
#define HNS3_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct hns3_adapter *)adapter)->vf)
#define HNS3_DEV_HW_TO_ADAPTER(hw) \
	container_of(hw, struct hns3_adapter, hw)

static inline struct hns3_pf *HNS3_DEV_HW_TO_PF(struct hns3_hw *hw)
{
	struct hns3_adapter *adapter = HNS3_DEV_HW_TO_ADAPTER(hw);
	return &adapter->pf;
}

static inline struct hns3_vf *HNS3_DEV_HW_TO_VF(struct hns3_hw *hw)
{
	struct hns3_adapter *adapter = HNS3_DEV_HW_TO_ADAPTER(hw);
	return &adapter->vf;
}

#define hns3_set_field(origin, mask, shift, val) \
	do { \
		(origin) &= (~(mask)); \
		(origin) |= ((val) << (shift)) & (mask); \
	} while (0)
#define hns3_get_field(origin, mask, shift) \
	(((origin) & (mask)) >> (shift))
#define hns3_set_bit(origin, shift, val) \
	hns3_set_field((origin), (0x1UL << (shift)), (shift), (val))
#define hns3_get_bit(origin, shift) \
	hns3_get_field((origin), (0x1UL << (shift)), (shift))

#define hns3_gen_field_val(mask, shift, val) (((val) << (shift)) & (mask))

/*
 * upper_32_bits - return bits 32-63 of a number
 * A basic shift-right of a 64- or 32-bit quantity. Use this to suppress
 * the "right shift count >= width of type" warning when that quantity is
 * 32-bits.
 */
#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))

/* lower_32_bits - return bits 0-31 of a number */
#define lower_32_bits(n) ((uint32_t)(n))

#define BIT(nr) (1UL << (nr))

#define BIT_ULL(x) (1ULL << (x))

#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y) ((x) - ((x) % (y)))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/*
 * Because hardware always access register in little-endian mode based on hns3
 * network engine, so driver should also call rte_cpu_to_le_32 to convert data
 * in little-endian mode before writing register and call rte_le_to_cpu_32 to
 * convert data after reading from register.
 *
 * Here the driver encapsulates the data conversion operation in the register
 * read/write operation function as below:
 *   hns3_write_reg
 *   hns3_write_reg_opt
 *   hns3_read_reg
 * Therefore, when calling these functions, conversion is not required again.
 */
static inline void hns3_write_reg(void *base, uint32_t reg, uint32_t value)
{
	rte_write32(rte_cpu_to_le_32(value),
		    (volatile void *)((char *)base + reg));
}

/*
 * The optimized function for writing registers reduces one address addition
 * calculation, it was used in the '.rx_pkt_burst' and '.tx_pkt_burst' ops
 * implementation function.
 */
static inline void hns3_write_reg_opt(volatile void *addr, uint32_t value)
{
	rte_write32(rte_cpu_to_le_32(value), addr);
}

static inline uint32_t hns3_read_reg(void *base, uint32_t reg)
{
	uint32_t read_val = rte_read32((volatile void *)((char *)base + reg));
	return rte_le_to_cpu_32(read_val);
}

#define hns3_write_dev(a, reg, value) \
	hns3_write_reg((a)->io_base, (reg), (value))

#define hns3_read_dev(a, reg) \
	hns3_read_reg((a)->io_base, (reg))

#define NEXT_ITEM_OF_ACTION(act, actions, index)                        \
	do {								\
		act = (actions) + (index);				\
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {	\
			(index)++;					\
			act = actions + index;				\
		}							\
	} while (0)

static inline uint64_t
hns3_atomic_test_bit(unsigned int nr, volatile uint64_t *addr)
{
	uint64_t res;

	res = (__atomic_load_n(addr, __ATOMIC_RELAXED) & (1UL << nr)) != 0;
	return res;
}

static inline void
hns3_atomic_set_bit(unsigned int nr, volatile uint64_t *addr)
{
	__atomic_fetch_or(addr, (1UL << nr), __ATOMIC_RELAXED);
}

static inline void
hns3_atomic_clear_bit(unsigned int nr, volatile uint64_t *addr)
{
	__atomic_fetch_and(addr, ~(1UL << nr), __ATOMIC_RELAXED);
}

static inline int64_t
hns3_test_and_clear_bit(unsigned int nr, volatile uint64_t *addr)
{
	uint64_t mask = (1UL << nr);

	return __atomic_fetch_and(addr, ~mask, __ATOMIC_RELAXED) & mask;
}

uint32_t hns3_get_speed_capa(struct hns3_hw *hw);

int hns3_buffer_alloc(struct hns3_hw *hw);
bool hns3_is_reset_pending(struct hns3_adapter *hns);
bool hns3vf_is_reset_pending(struct hns3_adapter *hns);
void hns3_update_linkstatus_and_event(struct hns3_hw *hw, bool query);
void hns3vf_update_link_status(struct hns3_hw *hw, uint8_t link_status,
			  uint32_t link_speed, uint8_t link_duplex);
void hns3vf_update_push_lsc_cap(struct hns3_hw *hw, bool supported);

int hns3_restore_ptp(struct hns3_adapter *hns);
int hns3_mbuf_dyn_rx_timestamp_register(struct rte_eth_dev *dev,
				    struct rte_eth_conf *conf);
int hns3_ptp_init(struct hns3_hw *hw);
int hns3_timesync_enable(struct rte_eth_dev *dev);
int hns3_timesync_disable(struct rte_eth_dev *dev);
int hns3_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp,
				uint32_t flags __rte_unused);
int hns3_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp);
int hns3_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts);
int hns3_timesync_write_time(struct rte_eth_dev *dev,
			const struct timespec *ts);
int hns3_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);

static inline bool
is_reset_pending(struct hns3_adapter *hns)
{
	bool ret;
	if (hns->is_vf)
		ret = hns3vf_is_reset_pending(hns);
	else
		ret = hns3_is_reset_pending(hns);
	return ret;
}

#endif /* _HNS3_ETHDEV_H_ */
