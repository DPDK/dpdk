/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#ifndef _HNS3_ETHDEV_H_
#define _HNS3_ETHDEV_H_

#include <sys/time.h>
#include <rte_alarm.h>

#include "hns3_cmd.h"
#include "hns3_mbx.h"
#include "hns3_rss.h"
#include "hns3_fdir.h"
#include "hns3_stats.h"

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

#define HNS3_UC_MACADDR_NUM		128
#define HNS3_VF_UC_MACADDR_NUM		48
#define HNS3_MC_MACADDR_NUM		128

#define HNS3_MAX_BD_SIZE		65535
#define HNS3_MAX_NON_TSO_BD_PER_PKT	8
#define HNS3_MAX_TSO_BD_PER_PKT		63
#define HNS3_MAX_FRAME_LEN		9728
#define HNS3_VLAN_TAG_SIZE		4
#define HNS3_DEFAULT_RX_BUF_LEN		2048
#define HNS3_MAX_BD_PAYLEN		(1024 * 1024 - 1)
#define HNS3_MAX_TSO_HDR_SIZE		512
#define HNS3_MAX_TSO_HDR_BD_NUM		3
#define HNS3_MAX_LRO_SIZE		64512

#define HNS3_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + HNS3_VLAN_TAG_SIZE * 2)
#define HNS3_PKTLEN_TO_MTU(pktlen)	((pktlen) - HNS3_ETH_OVERHEAD)
#define HNS3_MAX_MTU	(HNS3_MAX_FRAME_LEN - HNS3_ETH_OVERHEAD)
#define HNS3_DEFAULT_MTU		1500UL
#define HNS3_DEFAULT_FRAME_LEN		(HNS3_DEFAULT_MTU + HNS3_ETH_OVERHEAD)
#define HNS3_MIN_PKT_SIZE		60

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
	uint8_t up_to_tc_map; /* user priority maping on the TC */
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
	uint8_t	tqp_offset;     /* TQP offset from base TQP */
	uint8_t	tqp_count;      /* Total TQPs */
	uint8_t	tc;             /* TC index */
	bool enable;            /* If this TC is enable or not */
};

struct hns3_cfg {
	uint8_t vmdq_vport_num;
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

/* mac media type */
enum hns3_media_type {
	HNS3_MEDIA_TYPE_UNKNOWN,
	HNS3_MEDIA_TYPE_FIBER,
	HNS3_MEDIA_TYPE_COPPER,
	HNS3_MEDIA_TYPE_BACKPLANE,
	HNS3_MEDIA_TYPE_NONE,
};

struct hns3_mac {
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	bool default_addr_setted; /* whether default addr(mac_addr) is setted */
	uint8_t media_type;
	uint8_t phy_addr;
	uint8_t link_duplex  : 1; /* ETH_LINK_[HALF/FULL]_DUPLEX */
	uint8_t link_autoneg : 1; /* ETH_LINK_[AUTONEG/FIXED] */
	uint8_t link_status  : 1; /* ETH_LINK_[DOWN/UP] */
	uint32_t link_speed;      /* ETH_SPEED_NUM_ */
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
	HNS3_NONE_RESET,
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
	 */
	HNS3_VF_FULL_RESET = 3,
	HNS3_FLR_RESET,     /* A VF perform FLR reset */
	/* All VFs under the rootport perform a global or IMP reset */
	HNS3_VF_RESET,
	HNS3_FUNC_RESET,    /* A PF function reset */
	/* All PFs under the rootport perform a global reset */
	HNS3_GLOBAL_RESET,
	HNS3_IMP_RESET,     /* All PFs under the rootport perform a IMP reset */
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
	rte_atomic16_t schedule;
	/* Reset flag, covering the entire reset process */
	rte_atomic16_t resetting;
	/* Used to disable sending cmds during reset */
	rte_atomic16_t disable_cmd;
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

struct hns3_hw {
	struct rte_eth_dev_data *data;
	void *io_base;
	uint8_t revision;           /* PCI revision, low byte of class word */
	struct hns3_cmq cmq;
	struct hns3_mbx_resp_status mbx_resp; /* mailbox response */
	struct hns3_mbx_arq_ring arq;         /* mailbox async rx queue */
	pthread_t irq_thread_id;
	struct hns3_mac mac;
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
	struct hns3_tqp_stats tqp_stats;
	/* Include Mac stats | Rx stats | Tx stats */
	struct hns3_mac_stats mac_stats;
	uint32_t fw_version;

	uint16_t num_msi;
	uint16_t total_tqps_num;    /* total task queue pairs of this PF */
	uint16_t tqps_num;          /* num task queue pairs of this function */
	uint16_t intr_tqps_num;     /* num queue pairs mapping interrupt */
	uint16_t rss_size_max;      /* HW defined max RSS task queue */
	uint16_t num_tx_desc;       /* desc num of per tx queue */
	uint16_t num_rx_desc;       /* desc num of per rx queue */

	struct rte_ether_addr mc_addrs[HNS3_MC_MACADDR_NUM];
	int mc_addrs_num; /* Multicast mac addresses number */

	/* The configuration info of RSS */
	struct hns3_rss_conf rss_info;
	bool rss_dis_flag; /* disable rss flag. true: disable, false: enable */

	uint8_t num_tc;             /* Total number of enabled TCs */
	uint8_t hw_tc_map;
	enum hns3_fc_mode current_mode;
	enum hns3_fc_mode requested_mode;
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

	struct hns3_port_base_vlan_config port_base_vlan_cfg;
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

struct hns3_err_msix_intr_stats {
	uint64_t mac_afifo_tnl_intr_cnt;
	uint64_t ppu_mpf_abnormal_intr_st2_cnt;
	uint64_t ssu_port_based_pf_intr_cnt;
	uint64_t ppp_pf_abnormal_intr_cnt;
	uint64_t ppu_pf_abnormal_intr_cnt;
};

/* vlan entry information. */
struct hns3_user_vlan_table {
	LIST_ENTRY(hns3_user_vlan_table) next;
	bool hd_tbl_status;
	uint16_t vlan_id;
};

/* Vlan tag configuration for RX direction */
struct hns3_rx_vtag_cfg {
	uint8_t rx_vlan_offload_en; /* Whether enable rx vlan offload */
	uint8_t strip_tag1_en;      /* Whether strip inner vlan tag */
	uint8_t strip_tag2_en;      /* Whether strip outer vlan tag */
	uint8_t vlan1_vlan_prionly; /* Inner VLAN Tag up to descriptor Enable */
	uint8_t vlan2_vlan_prionly; /* Outer VLAN Tag up to descriptor Enable */
};

/* Vlan tag configuration for TX direction */
struct hns3_tx_vtag_cfg {
	bool accept_tag1;           /* Whether accept tag1 packet from host */
	bool accept_untag1;         /* Whether accept untag1 packet from host */
	bool accept_tag2;
	bool accept_untag2;
	bool insert_tag1_en;        /* Whether insert inner vlan tag */
	bool insert_tag2_en;        /* Whether insert outer vlan tag */
	uint16_t default_tag1;      /* The default inner vlan tag to insert */
	uint16_t default_tag2;      /* The default outer vlan tag to insert */
};

struct hns3_vtag_cfg {
	struct hns3_rx_vtag_cfg rx_vcfg;
	struct hns3_tx_vtag_cfg tx_vcfg;
};

/* Request types for IPC. */
enum hns3_mp_req_type {
	HNS3_MP_REQ_START_RXTX = 1,
	HNS3_MP_REQ_STOP_RXTX,
	HNS3_MP_REQ_MAX
};

/* Pameters for IPC. */
struct hns3_mp_param {
	enum hns3_mp_req_type type;
	int port_id;
	int result;
};

/* Request timeout for IPC. */
#define HNS3_MP_REQ_TIMEOUT_SEC 5

/* Key string for IPC. */
#define HNS3_MP_NAME "net_hns3_mp"

struct hns3_pf {
	struct hns3_adapter *adapter;
	bool is_main_pf;
	uint16_t func_num; /* num functions of this pf, include pf and vfs */

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

	uint16_t wanted_umv_size;
	uint16_t max_umv_size;
	uint16_t used_umv_size;

	/* Statistics information for abnormal interrupt */
	struct hns3_err_msix_intr_stats abn_int_stats;

	bool support_sfp_query;

	struct hns3_vtag_cfg vtag_config;
	LIST_HEAD(vlan_tbl, hns3_user_vlan_table) vlan_list;

	struct hns3_fdir_info fdir; /* flow director info */
	LIST_HEAD(counters, hns3_flow_counter) flow_counters;
};

struct hns3_vf {
	struct hns3_adapter *adapter;
};

struct hns3_adapter {
	struct hns3_hw hw;

	/* Specific for PF or VF */
	bool is_vf; /* false - PF, true - VF */
	union {
		struct hns3_pf pf;
		struct hns3_vf vf;
	};
};

#define HNS3_DEV_SUPPORT_DCB_B			0x0
#define HNS3_DEV_SUPPORT_COPPER_B		0x1

#define hns3_dev_dcb_supported(hw) \
	hns3_get_bit((hw)->capability, HNS3_DEV_SUPPORT_DCB_B)

#define hns3_dev_copper_supported(hw) \
	hns3_get_bit((hw)->capability, HNS3_DEV_SUPPORT_COPPER_B)

#define HNS3_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct hns3_adapter *)adapter)->hw)
#define HNS3_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct hns3_adapter *)adapter)
#define HNS3_DEV_PRIVATE_TO_PF(adapter) \
	(&((struct hns3_adapter *)adapter)->pf)
#define HNS3VF_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct hns3_adapter *)adapter)->vf)
#define HNS3_DEV_HW_TO_ADAPTER(hw) \
	container_of(hw, struct hns3_adapter, hw)

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

#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y) ((x) - ((x) % (y)))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define max_t(type, x, y) ({                    \
	type __max1 = (x);                      \
	type __max2 = (y);                      \
	__max1 > __max2 ? __max1 : __max2; })

static inline void hns3_write_reg(void *base, uint32_t reg, uint32_t value)
{
	rte_write32(value, (volatile void *)((char *)base + reg));
}

static inline uint32_t hns3_read_reg(void *base, uint32_t reg)
{
	return rte_read32((volatile void *)((char *)base + reg));
}

#define hns3_write_dev(a, reg, value) \
	hns3_write_reg((a)->io_base, (reg), (value))

#define hns3_read_dev(a, reg) \
	hns3_read_reg((a)->io_base, (reg))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define NEXT_ITEM_OF_ACTION(act, actions, index)                        \
	do {								\
		act = (actions) + (index);				\
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {	\
			(index)++;					\
			act = actions + index;				\
		}							\
	} while (0)

#define MSEC_PER_SEC              1000L
#define USEC_PER_MSEC             1000L

static inline uint64_t
get_timeofday_ms(void)
{
	struct timeval tv;

	(void)gettimeofday(&tv, NULL);

	return (uint64_t)tv.tv_sec * MSEC_PER_SEC + tv.tv_usec / USEC_PER_MSEC;
}

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

int hns3_buffer_alloc(struct hns3_hw *hw);
int hns3_dev_filter_ctrl(struct rte_eth_dev *dev,
			 enum rte_filter_type filter_type,
			 enum rte_filter_op filter_op, void *arg);
bool hns3_is_reset_pending(struct hns3_adapter *hns);
bool hns3vf_is_reset_pending(struct hns3_adapter *hns);
void hns3_update_link_status(struct hns3_hw *hw);

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

static inline uint64_t
hns3_txvlan_cap_get(struct hns3_hw *hw)
{
	if (hw->port_base_vlan_cfg.state)
		return DEV_TX_OFFLOAD_VLAN_INSERT;
	else
		return DEV_TX_OFFLOAD_VLAN_INSERT | DEV_TX_OFFLOAD_QINQ_INSERT;
}

#endif /* _HNS3_ETHDEV_H_ */
