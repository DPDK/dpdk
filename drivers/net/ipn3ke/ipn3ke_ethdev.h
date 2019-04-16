/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _IPN3KE_ETHDEV_H_
#define _IPN3KE_ETHDEV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_flow_driver.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_spinlock.h>

#include <rte_cycles.h>
#include <rte_bus_ifpga.h>
#include <rte_tm_driver.h>

#define IPN3KE_TM_SCRATCH_RW 0

/* TM Levels */
enum ipn3ke_tm_node_level {
	IPN3KE_TM_NODE_LEVEL_PORT,
	IPN3KE_TM_NODE_LEVEL_VT,
	IPN3KE_TM_NODE_LEVEL_COS,
	IPN3KE_TM_NODE_LEVEL_MAX,
};

/* TM Shaper Profile */
struct ipn3ke_tm_shaper_profile {
	uint32_t valid;
	uint32_t m;
	uint32_t e;
	uint64_t rate;
	struct rte_tm_shaper_params params;
};

TAILQ_HEAD(ipn3ke_tm_shaper_profile_list, ipn3ke_tm_shaper_profile);


#define IPN3KE_TDROP_TH1_MASK  0x1ffffff
#define IPN3KE_TDROP_TH1_SHIFT (25)
#define IPN3KE_TDROP_TH2_MASK  0x1ffffff

/* TM TDROP Profile */
struct ipn3ke_tm_tdrop_profile {
	uint32_t tdrop_profile_id;
	uint32_t th1;
	uint32_t th2;
	uint32_t n_users;
	uint32_t valid;
	struct rte_tm_wred_params params;
};

/* TM node priority */
enum ipn3ke_tm_node_state {
	IPN3KE_TM_NODE_STATE_IDLE = 0,
	IPN3KE_TM_NODE_STATE_CONFIGURED_ADD,
	IPN3KE_TM_NODE_STATE_CONFIGURED_DEL,
	IPN3KE_TM_NODE_STATE_COMMITTED,
	IPN3KE_TM_NODE_STATE_MAX,
};

TAILQ_HEAD(ipn3ke_tm_node_list, ipn3ke_tm_node);

/* IPN3KE TM Node */
struct ipn3ke_tm_node {
	TAILQ_ENTRY(ipn3ke_tm_node) node;
	uint32_t node_index;
	uint32_t level;
	uint32_t tm_id;
	enum ipn3ke_tm_node_state node_state;
	uint32_t parent_node_id;
	uint32_t priority;
	uint32_t weight;
	struct ipn3ke_tm_node *parent_node;
	struct ipn3ke_tm_shaper_profile shaper_profile;
	struct ipn3ke_tm_tdrop_profile *tdrop_profile;
	struct rte_tm_node_params params;
	struct rte_tm_node_stats stats;
	uint32_t n_children;
	struct ipn3ke_tm_node_list children_node_list;
};

/* IPN3KE TM Hierarchy Specification */
struct ipn3ke_tm_hierarchy {
	struct ipn3ke_tm_node *port_node;
	/*struct ipn3ke_tm_node_list vt_node_list;*/
	/*struct ipn3ke_tm_node_list cos_node_list;*/

	uint32_t n_shaper_profiles;
	/*uint32_t n_shared_shapers;*/
	uint32_t n_tdrop_profiles;
	uint32_t n_vt_nodes;
	uint32_t n_cos_nodes;

	struct ipn3ke_tm_node *port_commit_node;
	struct ipn3ke_tm_node_list vt_commit_node_list;
	struct ipn3ke_tm_node_list cos_commit_node_list;

	/*uint32_t n_tm_nodes[IPN3KE_TM_NODE_LEVEL_MAX];*/
};

struct ipn3ke_tm_internals {
	/** Hierarchy specification
	 *
	 *     -Hierarchy is unfrozen at init and when port is stopped.
	 *     -Hierarchy is frozen on successful hierarchy commit.
	 *     -Run-time hierarchy changes are not allowed, therefore it makes
	 *      sense to keep the hierarchy frozen after the port is started.
	 */
	struct ipn3ke_tm_hierarchy h;
	int hierarchy_frozen;
	int tm_started;
	uint32_t tm_id;
};

#define IPN3KE_TM_COS_NODE_NUM      (64 * 1024)
#define IPN3KE_TM_VT_NODE_NUM       (IPN3KE_TM_COS_NODE_NUM / 8)
#define IPN3KE_TM_10G_PORT_NODE_NUM (8)
#define IPN3KE_TM_25G_PORT_NODE_NUM (4)

#define IPN3KE_TM_NODE_LEVEL_MOD    (100000)
#define IPN3KE_TM_NODE_MOUNT_MAX    (8)

#define IPN3KE_TM_TDROP_PROFILE_NUM (2 * 1024)

/* TM node priority */
enum ipn3ke_tm_node_priority {
	IPN3KE_TM_NODE_PRIORITY_NORMAL0 = 0,
	IPN3KE_TM_NODE_PRIORITY_LOW,
	IPN3KE_TM_NODE_PRIORITY_NORMAL1,
	IPN3KE_TM_NODE_PRIORITY_HIGHEST,
};

#define IPN3KE_TM_NODE_WEIGHT_MAX UINT8_MAX

/** Set a bit in the uint32 variable */
#define IPN3KE_BIT_SET(var, pos) \
	((var) |= ((uint32_t)1 << ((pos))))

/** Reset the bit in the variable */
#define IPN3KE_BIT_RESET(var, pos) \
	((var) &= ~((uint32_t)1 << ((pos))))

/** Check the bit is set in the variable */
#define IPN3KE_BIT_ISSET(var, pos) \
	(((var) & ((uint32_t)1 << ((pos)))) ? 1 : 0)

struct ipn3ke_hw;

#define IPN3KE_HW_BASE               0x4000000

#define IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.capability_registers_block_offset)

#define IPN3KE_STATUS_REGISTERS_BLOCK_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.status_registers_block_offset)

#define IPN3KE_CTRL_RESET \
	(IPN3KE_HW_BASE + hw->hw_cap.control_registers_block_offset)

#define IPN3KE_CTRL_MTU \
	(IPN3KE_HW_BASE + hw->hw_cap.control_registers_block_offset + 4)

#define IPN3KE_CLASSIFY_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.classify_offset)

#define IPN3KE_POLICER_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.policer_offset)

#define IPN3KE_RSS_KEY_ARRAY_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.rss_key_array_offset)

#define IPN3KE_RSS_INDIRECTION_TABLE_ARRAY_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.rss_indirection_table_array_offset)

#define IPN3KE_DMAC_MAP_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.dmac_map_offset)

#define IPN3KE_QM_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.qm_offset)

#define IPN3KE_CCB_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.ccb_offset)

#define IPN3KE_QOS_OFFSET \
	(IPN3KE_HW_BASE + hw->hw_cap.qos_offset)

struct ipn3ke_hw_cap {
	uint32_t version_number;
	uint32_t capability_registers_block_offset;
	uint32_t status_registers_block_offset;
	uint32_t control_registers_block_offset;
	uint32_t classify_offset;
	uint32_t classy_size;
	uint32_t policer_offset;
	uint32_t policer_entry_size;
	uint32_t rss_key_array_offset;
	uint32_t rss_key_entry_size;
	uint32_t rss_indirection_table_array_offset;
	uint32_t rss_indirection_table_entry_size;
	uint32_t dmac_map_offset;
	uint32_t dmac_map_size;
	uint32_t qm_offset;
	uint32_t qm_size;
	uint32_t ccb_offset;
	uint32_t ccb_entry_size;
	uint32_t qos_offset;
	uint32_t qos_size;

	uint32_t num_rx_flow;    /* Default: 64K */
	uint32_t num_rss_blocks; /* Default: 512 */
	uint32_t num_dmac_map;   /* Default: 1K */
	uint32_t num_tx_flow;    /* Default: 64K */
	uint32_t num_smac_map;   /* Default: 1K */

	uint32_t link_speed_mbps;
};

/**
 * Strucute to store private data for each representor instance
 */
struct ipn3ke_rpst {
	TAILQ_ENTRY(ipn3ke_rpst) next;       /**< Next in device list. */
	uint16_t switch_domain_id;
	/**< Switch ID */
	uint16_t port_id;
	struct rte_eth_dev *ethdev;
	/**< Port ID */
	struct ipn3ke_hw *hw;
	struct rte_eth_dev *i40e_pf_eth;
	uint16_t i40e_pf_eth_port_id;
	struct rte_eth_link ori_linfo;
	struct ipn3ke_tm_internals tm;
	/**< Private data store of assocaiated physical function */
	struct ether_addr mac_addr;
};

/* UUID IDs */
#define MAP_UUID_10G_LOW                0xffffffffffffffff
#define MAP_UUID_10G_HIGH               0xffffffffffffffff
#define IPN3KE_UUID_10G_LOW             0xc000c9660d824272
#define IPN3KE_UUID_10G_HIGH            0x9aeffe5f84570612
#define IPN3KE_UUID_VBNG_LOW		0x8991165349d23ff9
#define IPN3KE_UUID_VBNG_HIGH		0xb74cf419d15a481f
#define IPN3KE_UUID_25G_LOW             0xb7d9bac566bfbc80
#define IPN3KE_UUID_25G_HIGH            0xb07bac1aeef54d67

#define IPN3KE_AFU_BUF_SIZE_MIN         1024
#define IPN3KE_AFU_FRAME_SIZE_MAX       9728

#define IPN3KE_RAWDEV_ATTR_LEN_MAX      (64)

typedef int (*ipn3ke_indirect_mac_read_t)(struct ipn3ke_hw *hw,
	uint32_t *rd_data, uint32_t addr, uint32_t mac_num,
	uint32_t eth_wrapper_sel);

typedef int (*ipn3ke_indirect_mac_write_t)(struct ipn3ke_hw *hw,
	uint32_t wr_data, uint32_t addr, uint32_t mac_num,
	uint32_t eth_wrapper_sel);

struct ipn3ke_hw {
	struct rte_eth_dev *eth_dev;

	/* afu info */
	struct rte_afu_id afu_id;
	struct rte_rawdev *rawdev;

	struct ipn3ke_hw_cap hw_cap;

	struct ifpga_rawdevg_retimer_info retimer;

	uint16_t switch_domain_id;
	uint16_t port_num;

	uint32_t tm_hw_enable;
	uint32_t flow_hw_enable;

	uint32_t acc_tm;
	uint32_t acc_flow;

	struct ipn3ke_flow_list flow_list;
	uint32_t flow_max_entries;
	uint32_t flow_num_entries;

	struct ipn3ke_tm_node *nodes;
	struct ipn3ke_tm_node *port_nodes;
	struct ipn3ke_tm_node *vt_nodes;
	struct ipn3ke_tm_node *cos_nodes;

	struct ipn3ke_tm_tdrop_profile *tdrop_profile;
	uint32_t tdrop_profile_num;

	uint32_t ccb_status;
	uint32_t ccb_seg_free;
	uint32_t ccb_seg_num;
	uint32_t ccb_seg_k;

	uint8_t *eth_group_bar[2];
	/**< MAC Register read */
	ipn3ke_indirect_mac_read_t f_mac_read;
	/**< MAC Register write */
	ipn3ke_indirect_mac_write_t f_mac_write;

	uint8_t *hw_addr;
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_afu_device.
 */
#define RTE_DEV_TO_AFU(ptr) \
	container_of(ptr, struct rte_afu_device, device)

#define RTE_DEV_TO_AFU_CONST(ptr) \
	container_of(ptr, const struct rte_afu_device, device)

#define RTE_ETH_DEV_TO_AFU(eth_dev) \
	RTE_DEV_TO_AFU((eth_dev)->device)

/**
 * PCIe MMIO Access
 */

#define IPN3KE_PCI_REG(reg)    rte_read32(reg)
#define IPN3KE_PCI_REG_ADDR(a, reg) \
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))
static inline uint32_t ipn3ke_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(IPN3KE_PCI_REG(addr));
}

#define WCMD 0x8000000000000000
#define RCMD 0x4000000000000000
#define UPL_BASE 0x10000
static inline uint32_t _ipn3ke_indrct_read(struct ipn3ke_hw *hw,
		uint32_t addr)
{
	uint64_t word_offset;
	uint64_t read_data = 0;
	uint64_t indirect_value;
	volatile void *indirect_addrs;

	word_offset = (addr & 0x1FFFFFF) >> 2;
	indirect_value = RCMD | word_offset << 32;
	indirect_addrs = hw->hw_addr + (uint32_t)(UPL_BASE | 0x10);

	rte_delay_us(10);

	rte_write64((rte_cpu_to_le_64(indirect_value)), indirect_addrs);

	indirect_addrs = hw->hw_addr + (uint32_t)(UPL_BASE | 0x18);
	while ((read_data >> 32) != 1)
		read_data = rte_read64(indirect_addrs);

	return rte_le_to_cpu_32(read_data);
}

static inline void _ipn3ke_indrct_write(struct ipn3ke_hw *hw,
		uint32_t addr, uint32_t value)
{
	uint64_t word_offset;
	uint64_t indirect_value;
	volatile void *indirect_addrs = 0;

	word_offset = (addr & 0x1FFFFFF) >> 2;
	indirect_value = WCMD | word_offset << 32 | value;
	indirect_addrs = hw->hw_addr + (uint32_t)(UPL_BASE | 0x10);

	rte_write64((rte_cpu_to_le_64(indirect_value)), indirect_addrs);
	rte_delay_us(10);
}

#define IPN3KE_PCI_REG_WRITE(reg, value) \
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define IPN3KE_PCI_REG_WRITE_RELAXED(reg, value) \
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)

#define IPN3KE_READ_REG(hw, reg) \
	_ipn3ke_indrct_read((hw), (reg))

#define IPN3KE_WRITE_REG(hw, reg, value) \
	_ipn3ke_indrct_write((hw), (reg), (value))

#define IPN3KE_MASK_READ_REG(hw, reg, x, mask) \
	((mask) & IPN3KE_READ_REG((hw), ((reg) + (0x4 * (x)))))

#define IPN3KE_MASK_WRITE_REG(hw, reg, x, value, mask) \
	IPN3KE_WRITE_REG((hw), ((reg) + (0x4 * (x))), ((mask) & (value)))

#define IPN3KE_DEV_PRIVATE_TO_HW(dev) \
	(((struct ipn3ke_rpst *)(dev)->data->dev_private)->hw)

#define IPN3KE_DEV_PRIVATE_TO_RPST(dev) \
	((struct ipn3ke_rpst *)(dev)->data->dev_private)

#define IPN3KE_DEV_PRIVATE_TO_TM(dev) \
	(&(((struct ipn3ke_rpst *)(dev)->data->dev_private)->tm))

/* Byte address of IPN3KE internal module */
#define IPN3KE_TM_VERSION                     (IPN3KE_QM_OFFSET + 0x0000)
#define IPN3KE_TM_SCRATCH                     (IPN3KE_QM_OFFSET + 0x0004)
#define IPN3KE_TM_STATUS                      (IPN3KE_QM_OFFSET + 0x0008)
#define IPN3KE_TM_MISC_STATUS                 (IPN3KE_QM_OFFSET + 0x0010)
#define IPN3KE_TM_MISC_WARNING_0              (IPN3KE_QM_OFFSET + 0x0040)
#define IPN3KE_TM_MISC_MON_0                  (IPN3KE_QM_OFFSET + 0x0048)
#define IPN3KE_TM_MISC_FATAL_0                (IPN3KE_QM_OFFSET + 0x0050)
#define IPN3KE_TM_BW_MON_CTRL_1               (IPN3KE_QM_OFFSET + 0x0080)
#define IPN3KE_TM_BW_MON_CTRL_2               (IPN3KE_QM_OFFSET + 0x0084)
#define IPN3KE_TM_BW_MON_RATE                 (IPN3KE_QM_OFFSET + 0x0088)
#define IPN3KE_TM_STATS_CTRL                  (IPN3KE_QM_OFFSET + 0x0100)
#define IPN3KE_TM_STATS_DATA_0                (IPN3KE_QM_OFFSET + 0x0110)
#define IPN3KE_TM_STATS_DATA_1                (IPN3KE_QM_OFFSET + 0x0114)
#define IPN3KE_QM_UID_CONFIG_CTRL             (IPN3KE_QM_OFFSET + 0x0200)
#define IPN3KE_QM_UID_CONFIG_DATA             (IPN3KE_QM_OFFSET + 0x0204)

#define IPN3KE_BM_VERSION                     (IPN3KE_QM_OFFSET + 0x4000)
#define IPN3KE_BM_STATUS                      (IPN3KE_QM_OFFSET + 0x4008)
#define IPN3KE_BM_STORE_CTRL                  (IPN3KE_QM_OFFSET + 0x4010)
#define IPN3KE_BM_STORE_STATUS                (IPN3KE_QM_OFFSET + 0x4018)
#define IPN3KE_BM_STORE_MON                   (IPN3KE_QM_OFFSET + 0x4028)
#define IPN3KE_BM_WARNING_0                   (IPN3KE_QM_OFFSET + 0x4040)
#define IPN3KE_BM_MON_0                       (IPN3KE_QM_OFFSET + 0x4048)
#define IPN3KE_BM_FATAL_0                     (IPN3KE_QM_OFFSET + 0x4050)
#define IPN3KE_BM_DRAM_ACCESS_CTRL            (IPN3KE_QM_OFFSET + 0x4100)
#define IPN3KE_BM_DRAM_ACCESS_DATA_0          (IPN3KE_QM_OFFSET + 0x4120)
#define IPN3KE_BM_DRAM_ACCESS_DATA_1          (IPN3KE_QM_OFFSET + 0x4124)
#define IPN3KE_BM_DRAM_ACCESS_DATA_2          (IPN3KE_QM_OFFSET + 0x4128)
#define IPN3KE_BM_DRAM_ACCESS_DATA_3          (IPN3KE_QM_OFFSET + 0x412C)
#define IPN3KE_BM_DRAM_ACCESS_DATA_4          (IPN3KE_QM_OFFSET + 0x4130)
#define IPN3KE_BM_DRAM_ACCESS_DATA_5          (IPN3KE_QM_OFFSET + 0x4134)
#define IPN3KE_BM_DRAM_ACCESS_DATA_6          (IPN3KE_QM_OFFSET + 0x4138)

#define IPN3KE_QM_VERSION                     (IPN3KE_QM_OFFSET + 0x8000)
#define IPN3KE_QM_STATUS                      (IPN3KE_QM_OFFSET + 0x8008)
#define IPN3KE_QM_LL_TABLE_MON                (IPN3KE_QM_OFFSET + 0x8018)
#define IPN3KE_QM_WARNING_0                   (IPN3KE_QM_OFFSET + 0x8040)
#define IPN3KE_QM_MON_0                       (IPN3KE_QM_OFFSET + 0x8048)
#define IPN3KE_QM_FATAL_0                     (IPN3KE_QM_OFFSET + 0x8050)
#define IPN3KE_QM_FATAL_1                     (IPN3KE_QM_OFFSET + 0x8054)
#define IPN3KE_LL_TABLE_ACCESS_CTRL           (IPN3KE_QM_OFFSET + 0x8100)
#define IPN3KE_LL_TABLE_ACCESS_DATA_0         (IPN3KE_QM_OFFSET + 0x8110)
#define IPN3KE_LL_TABLE_ACCESS_DATA_1         (IPN3KE_QM_OFFSET + 0x8114)

#define IPN3KE_CCB_ERROR                      (IPN3KE_CCB_OFFSET + 0x0008)
#define IPN3KE_CCB_NSEGFREE                   (IPN3KE_CCB_OFFSET + 0x200000)
#define IPN3KE_CCB_NSEGFREE_MASK               0x3FFFFF
#define IPN3KE_CCB_PSEGMAX_COEF               (IPN3KE_CCB_OFFSET + 0x200008)
#define IPN3KE_CCB_PSEGMAX_COEF_MASK           0xFFFFF
#define IPN3KE_CCB_NSEG_P                     (IPN3KE_CCB_OFFSET + 0x200080)
#define IPN3KE_CCB_NSEG_MASK                   0x3FFFFF
#define IPN3KE_CCB_QPROFILE_Q                 (IPN3KE_CCB_OFFSET + 0x240000)
#define IPN3KE_CCB_QPROFILE_MASK               0x7FF
#define IPN3KE_CCB_PROFILE_P                  (IPN3KE_CCB_OFFSET + 0x280000)
#define IPN3KE_CCB_PROFILE_MASK                0x1FFFFFF
#define IPN3KE_CCB_PROFILE_MS                 (IPN3KE_CCB_OFFSET + 0xC)
#define IPN3KE_CCB_PROFILE_MS_MASK             0x1FFFFFF
#define IPN3KE_CCB_LR_LB_DBG_CTRL             (IPN3KE_CCB_OFFSET + 0x2C0000)
#define IPN3KE_CCB_LR_LB_DBG_DONE             (IPN3KE_CCB_OFFSET + 0x2C0004)
#define IPN3KE_CCB_LR_LB_DBG_RDATA            (IPN3KE_CCB_OFFSET + 0x2C000C)

#define IPN3KE_QOS_MAP_L1_X                   (IPN3KE_QOS_OFFSET + 0x000000)
#define IPN3KE_QOS_MAP_L1_MASK                 0x1FFF
#define IPN3KE_QOS_MAP_L2_X                   (IPN3KE_QOS_OFFSET + 0x040000)
#define IPN3KE_QOS_MAP_L2_MASK                 0x7
#define IPN3KE_QOS_TYPE_MASK                   0x3
#define IPN3KE_QOS_TYPE_L1_X                  (IPN3KE_QOS_OFFSET + 0x200000)
#define IPN3KE_QOS_TYPE_L2_X                  (IPN3KE_QOS_OFFSET + 0x240000)
#define IPN3KE_QOS_TYPE_L3_X                  (IPN3KE_QOS_OFFSET + 0x280000)
#define IPN3KE_QOS_SCH_WT_MASK                 0xFF
#define IPN3KE_QOS_SCH_WT_L1_X                (IPN3KE_QOS_OFFSET + 0x400000)
#define IPN3KE_QOS_SCH_WT_L2_X                (IPN3KE_QOS_OFFSET + 0x440000)
#define IPN3KE_QOS_SCH_WT_L3_X                (IPN3KE_QOS_OFFSET + 0x480000)
#define IPN3KE_QOS_SHAP_WT_MASK                0x3FFF
#define IPN3KE_QOS_SHAP_WT_L1_X               (IPN3KE_QOS_OFFSET + 0x600000)
#define IPN3KE_QOS_SHAP_WT_L2_X               (IPN3KE_QOS_OFFSET + 0x640000)
#define IPN3KE_QOS_SHAP_WT_L3_X               (IPN3KE_QOS_OFFSET + 0x680000)

#define IPN3KE_CLF_BASE_DST_MAC_ADDR_HI       (IPN3KE_CLASSIFY_OFFSET + 0x0000)
#define IPN3KE_CLF_BASE_DST_MAC_ADDR_LOW      (IPN3KE_CLASSIFY_OFFSET + 0x0004)
#define IPN3KE_CLF_QINQ_STAG                  (IPN3KE_CLASSIFY_OFFSET + 0x0008)
#define IPN3KE_CLF_LKUP_ENABLE                (IPN3KE_CLASSIFY_OFFSET + 0x000C)
#define IPN3KE_CLF_DFT_FLOW_ID                (IPN3KE_CLASSIFY_OFFSET + 0x0040)
#define IPN3KE_CLF_RX_PARSE_CFG               (IPN3KE_CLASSIFY_OFFSET + 0x0080)
#define IPN3KE_CLF_RX_STATS_CFG               (IPN3KE_CLASSIFY_OFFSET + 0x00C0)
#define IPN3KE_CLF_RX_STATS_RPT               (IPN3KE_CLASSIFY_OFFSET + 0x00C4)
#define IPN3KE_CLF_RX_TEST                    (IPN3KE_CLASSIFY_OFFSET + 0x0400)

#define IPN3KE_CLF_EM_VERSION       (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x0000)
#define IPN3KE_CLF_EM_NUM           (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x0008)
#define IPN3KE_CLF_EM_KEY_WDTH      (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x000C)
#define IPN3KE_CLF_EM_RES_WDTH      (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x0010)
#define IPN3KE_CLF_EM_ALARMS        (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x0014)
#define IPN3KE_CLF_EM_DRC_RLAT      (IPN3KE_CLASSIFY_OFFSET + 0x40000 + 0x0018)

#define IPN3KE_CLF_MHL_VERSION      (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x0000)
#define IPN3KE_CLF_MHL_GEN_CTRL     (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x0018)
#define IPN3KE_CLF_MHL_MGMT_CTRL    (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x0020)
#define IPN3KE_CLF_MHL_MGMT_CTRL_BIT_BUSY      31
#define IPN3KE_CLF_MHL_MGMT_CTRL_FLUSH         0x0
#define IPN3KE_CLF_MHL_MGMT_CTRL_INSERT        0x1
#define IPN3KE_CLF_MHL_MGMT_CTRL_DELETE        0x2
#define IPN3KE_CLF_MHL_MGMT_CTRL_SEARCH        0x3
#define IPN3KE_CLF_MHL_FATAL_0     (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x0050)
#define IPN3KE_CLF_MHL_MON_0       (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x0060)
#define IPN3KE_CLF_MHL_TOTAL_ENTRIES   (IPN3KE_CLASSIFY_OFFSET + \
					0x50000 + 0x0080)
#define IPN3KE_CLF_MHL_ONEHIT_BUCKETS  (IPN3KE_CLASSIFY_OFFSET + \
					0x50000 + 0x0084)
#define IPN3KE_CLF_MHL_KEY_MASK         0xFFFFFFFF
#define IPN3KE_CLF_MHL_KEY_0       (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x1000)
#define IPN3KE_CLF_MHL_KEY_1       (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x1004)
#define IPN3KE_CLF_MHL_KEY_2       (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x1008)
#define IPN3KE_CLF_MHL_KEY_3       (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x100C)
#define IPN3KE_CLF_MHL_RES_MASK    0xFFFFFFFF
#define IPN3KE_CLF_MHL_RES         (IPN3KE_CLASSIFY_OFFSET + 0x50000 + 0x2000)

int
ipn3ke_rpst_dev_set_link_up(struct rte_eth_dev *dev);
int
ipn3ke_rpst_dev_set_link_down(struct rte_eth_dev *dev);
int
ipn3ke_rpst_link_update(struct rte_eth_dev *ethdev,
	__rte_unused int wait_to_complete);
void
ipn3ke_rpst_promiscuous_enable(struct rte_eth_dev *ethdev);
void
ipn3ke_rpst_promiscuous_disable(struct rte_eth_dev *ethdev);
void
ipn3ke_rpst_allmulticast_enable(struct rte_eth_dev *ethdev);
void
ipn3ke_rpst_allmulticast_disable(struct rte_eth_dev *ethdev);
int
ipn3ke_rpst_mac_addr_set(struct rte_eth_dev *ethdev,
		struct ether_addr *mac_addr);
int
ipn3ke_rpst_mtu_set(struct rte_eth_dev *ethdev, uint16_t mtu);

int
ipn3ke_rpst_init(struct rte_eth_dev *ethdev, void *init_params);
int
ipn3ke_rpst_uninit(struct rte_eth_dev *ethdev);
int
ipn3ke_hw_tm_init(struct ipn3ke_hw *hw);
void
ipn3ke_tm_init(struct ipn3ke_rpst *rpst);
int
ipn3ke_tm_ops_get(struct rte_eth_dev *ethdev,
		void *arg);


/* IPN3KE_MASK is a macro used on 32 bit registers */
#define IPN3KE_MASK(mask, shift) ((mask) << (shift))

#define IPN3KE_MAC_CTRL_BASE_0    0x00000000
#define IPN3KE_MAC_CTRL_BASE_1    0x00008000

#define IPN3KE_MAC_STATS_MASK    0xFFFFFFFFF

/* All the address are in 4Bytes*/
#define IPN3KE_MAC_PRIMARY_MAC_ADDR0    0x0010
#define IPN3KE_MAC_PRIMARY_MAC_ADDR1    0x0011

#define IPN3KE_MAC_MAC_RESET_CONTROL    0x001F
#define IPN3KE_MAC_MAC_RESET_CONTROL_TX_SHIFT    0
#define IPN3KE_MAC_MAC_RESET_CONTROL_TX_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_MAC_RESET_CONTROL_TX_SHIFT)

#define IPN3KE_MAC_MAC_RESET_CONTROL_RX_SHIFT    8
#define IPN3KE_MAC_MAC_RESET_CONTROL_RX_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_MAC_RESET_CONTROL_RX_SHIFT)

#define IPN3KE_MAC_TX_PACKET_CONTROL    0x0020
#define IPN3KE_MAC_TX_PACKET_CONTROL_SHIFT    0
#define IPN3KE_MAC_TX_PACKET_CONTROL_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_TX_PACKET_CONTROL_SHIFT)

#define IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE    0x002A
#define IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE_SHIFT    0
#define IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE_SHIFT)

#define IPN3KE_MAC_TX_FRAME_MAXLENGTH    0x002C
#define IPN3KE_MAC_TX_FRAME_MAXLENGTH_SHIFT    0
#define IPN3KE_MAC_TX_FRAME_MAXLENGTH_MASK \
	IPN3KE_MASK(0xFFFF, IPN3KE_MAC_TX_FRAME_MAXLENGTH_SHIFT)

#define IPN3KE_MAC_TX_PAUSEFRAME_CONTROL    0x0040
#define IPN3KE_MAC_TX_PAUSEFRAME_CONTROL_SHIFT    0
#define IPN3KE_MAC_TX_PAUSEFRAME_CONTROL_MASK \
	IPN3KE_MASK(0x3, IPN3KE_MAC_TX_PAUSEFRAME_CONTROL_SHIFT)

#define IPN3KE_MAC_TX_PAUSEFRAME_QUANTA    0x0042
#define IPN3KE_MAC_TX_PAUSEFRAME_QUANTA_SHIFT    0
#define IPN3KE_MAC_TX_PAUSEFRAME_QUANTA_MASK \
	IPN3KE_MASK(0xFFFF, IPN3KE_MAC_TX_PAUSEFRAME_QUANTA_SHIFT)

#define IPN3KE_MAC_TX_PAUSEFRAME_HOLDOFF_QUANTA    0x0043
#define IPN3KE_MAC_TX_PAUSEFRAME_HOLDOFF_QUANTA_SHIFT    0
#define IPN3KE_MAC_TX_PAUSEFRAME_HOLDOFF_QUANTA_MASK \
	IPN3KE_MASK(0xFFFF, IPN3KE_MAC_TX_PAUSEFRAME_HOLDOFF_QUANTA_SHIFT)

#define IPN3KE_MAC_TX_PAUSEFRAME_ENABLE    0x0044
#define IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_CFG_SHIFT    0
#define IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_CFG_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_CFG_SHIFT)

#define IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_TYPE_SHIFT    1
#define IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_TYPE_MASK \
	IPN3KE_MASK(0x3, IPN3KE_MAC_TX_PAUSEFRAME_ENABLE_TYPE_SHIFT)

#define IPN3KE_MAC_RX_TRANSFER_CONTROL    0x00A0
#define IPN3KE_MAC_RX_TRANSFER_CONTROL_SHIFT    0x0
#define IPN3KE_MAC_RX_TRANSFER_CONTROL_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_RX_TRANSFER_CONTROL_SHIFT)

#define IPN3KE_MAC_RX_FRAME_CONTROL    0x00AC
#define IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_SHIFT    0x0
#define IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_SHIFT)

#define IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT    0x1
#define IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT)

#define IPN3KE_VLAN_TAG_SIZE    4
/**
 * The overhead from MTU to max frame size.
 * Considering QinQ packet, the VLAN tag needs to be counted twice.
 */
#define IPN3KE_ETH_OVERHEAD \
		(ETHER_HDR_LEN + ETHER_CRC_LEN + IPN3KE_VLAN_TAG_SIZE * 2)

#define IPN3KE_MAC_FRAME_SIZE_MAX    9728
#define IPN3KE_MAC_RX_FRAME_MAXLENGTH    0x00AE
#define IPN3KE_MAC_RX_FRAME_MAXLENGTH_SHIFT    0
#define IPN3KE_MAC_RX_FRAME_MAXLENGTH_MASK \
	IPN3KE_MASK(0xFFFF, IPN3KE_MAC_RX_FRAME_MAXLENGTH_SHIFT)

#define IPN3KE_MAC_TX_STATS_CLR    0x0140
#define IPN3KE_MAC_TX_STATS_CLR_CLEAR_SHIFT    0
#define IPN3KE_MAC_TX_STATS_CLR_CLEAR_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_TX_STATS_CLR_CLEAR_SHIFT)

#define IPN3KE_MAC_RX_STATS_CLR    0x01C0
#define IPN3KE_MAC_RX_STATS_CLR_CLEAR_SHIFT    0
#define IPN3KE_MAC_RX_STATS_CLR_CLEAR_MASK \
	IPN3KE_MASK(0x1, IPN3KE_MAC_RX_STATS_CLR_CLEAR_SHIFT)

/*tx_stats_framesOK*/
#define IPN3KE_MAC_TX_STATS_FRAMESOK_HI  0x0142
#define IPN3KE_MAC_TX_STATS_FRAMESOK_LOW 0x0143

/*rx_stats_framesOK*/
#define IPN3KE_MAC_RX_STATS_FRAMESOK_HI  0x01C2
#define IPN3KE_MAC_RX_STATS_FRAMESOK_LOW 0x01C3

/*tx_stats_framesErr*/
#define IPN3KE_MAC_TX_STATS_FRAMESERR_HI  0x0144
#define IPN3KE_MAC_TX_STATS_FRAMESERR_LOW 0x0145

/*rx_stats_framesErr*/
#define IPN3KE_MAC_RX_STATS_FRAMESERR_HI  0x01C4
#define IPN3KE_MAC_RX_STATS_FRAMESERR_LOW 0x01C5

/*rx_stats_framesCRCErr*/
#define IPN3KE_MAC_RX_STATS_FRAMESCRCERR_HI  0x01C6
#define IPN3KE_MAC_RX_STATS_FRAMESCRCERR_LOW 0x01C7

/*tx_stats_octetsOK 64b*/
#define IPN3KE_MAC_TX_STATS_OCTETSOK_HI  0x0148
#define IPN3KE_MAC_TX_STATS_OCTETSOK_LOW 0x0149

/*rx_stats_octetsOK 64b*/
#define IPN3KE_MAC_RX_STATS_OCTETSOK_HI  0x01C8
#define IPN3KE_MAC_RX_STATS_OCTETSOK_LOW 0x01C9

/*tx_stats_pauseMACCtrl_Frames*/
#define IPN3KE_MAC_TX_STATS_PAUSEMACCTRL_FRAMES_HI  0x014A
#define IPN3KE_MAC_TX_STATS_PAUSEMACCTRL_FRAMES_LOW 0x014B

/*rx_stats_pauseMACCtrl_Frames*/
#define IPN3KE_MAC_RX_STATS_PAUSEMACCTRL_FRAMES_HI  0x01CA
#define IPN3KE_MAC_RX_STATS_PAUSEMACCTRL_FRAMES_LOW 0x01CB

/*tx_stats_ifErrors*/
#define IPN3KE_MAC_TX_STATS_IFERRORS_HI  0x014C
#define IPN3KE_MAC_TX_STATS_IFERRORS_LOW 0x014D

/*rx_stats_ifErrors*/
#define IPN3KE_MAC_RX_STATS_IFERRORS_HI  0x01CC
#define IPN3KE_MAC_RX_STATS_IFERRORS_LOW 0x01CD

/*tx_stats_unicast_FramesOK*/
#define IPN3KE_MAC_TX_STATS_UNICAST_FRAMESOK_HI  0x014E
#define IPN3KE_MAC_TX_STATS_UNICAST_FRAMESOK_LOW 0x014F

/*rx_stats_unicast_FramesOK*/
#define IPN3KE_MAC_RX_STATS_UNICAST_FRAMESOK_HI  0x01CE
#define IPN3KE_MAC_RX_STATS_UNICAST_FRAMESOK_LOW 0x01CF

/*tx_stats_unicast_FramesErr*/
#define IPN3KE_MAC_TX_STATS_UNICAST_FRAMESERR_HI  0x0150
#define IPN3KE_MAC_TX_STATS_UNICAST_FRAMESERR_LOW 0x0151

/*rx_stats_unicast_FramesErr*/
#define IPN3KE_MAC_RX_STATS_UNICAST_FRAMESERR_HI  0x01D0
#define IPN3KE_MAC_RX_STATS_UNICAST_FRAMESERR_LOW 0x01D1

/*tx_stats_multicast_FramesOK*/
#define IPN3KE_MAC_TX_STATS_MULTICAST_FRAMESOK_HI  0x0152
#define IPN3KE_MAC_TX_STATS_MULTICAST_FRAMESOK_LOW 0x0153

/*rx_stats_multicast_FramesOK*/
#define IPN3KE_MAC_RX_STATS_MULTICAST_FRAMESOK_HI  0x01D2
#define IPN3KE_MAC_RX_STATS_MULTICAST_FRAMESOK_LOW 0x01D3

/*tx_stats_multicast_FramesErr*/
#define IPN3KE_MAC_TX_STATS_MULTICAST_FRAMESERR_HI  0x0154
#define IPN3KE_MAC_TX_STATS_MULTICAST_FRAMESERR_LOW 0x0155

/*rx_stats_multicast_FramesErr*/
#define IPN3KE_MAC_RX_STATS_MULTICAST_FRAMESERR_HI  0x01D4
#define IPN3KE_MAC_RX_STATS_MULTICAST_FRAMESERR_LOW 0x01D5

/*tx_stats_broadcast_FramesOK*/
#define IPN3KE_MAC_TX_STATS_BROADCAST_FRAMESOK_HI  0x0156
#define IPN3KE_MAC_TX_STATS_BROADCAST_FRAMESOK_LOW 0x0157

/*rx_stats_broadcast_FramesOK*/
#define IPN3KE_MAC_RX_STATS_BROADCAST_FRAMESOK_HI  0x01D6
#define IPN3KE_MAC_RX_STATS_BROADCAST_FRAMESOK_LOW 0x01D7

/*tx_stats_broadcast_FramesErr*/
#define IPN3KE_MAC_TX_STATS_BROADCAST_FRAMESERR_HI  0x0158
#define IPN3KE_MAC_TX_STATS_BROADCAST_FRAMESERR_LOW 0x0159

/*rx_stats_broadcast_FramesErr*/
#define IPN3KE_MAC_RX_STATS_BROADCAST_FRAMESERR_HI  0x01D8
#define IPN3KE_MAC_RX_STATS_BROADCAST_FRAMESERR_LOW 0x01D9

/*tx_stats_etherStatsOctets 64b*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSOCTETS_HI  0x015A
#define IPN3KE_MAC_TX_STATS_ETHERSTATSOCTETS_LOW 0x015B

/*rx_stats_etherStatsOctets 64b*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSOCTETS_HI  0x01DA
#define IPN3KE_MAC_RX_STATS_ETHERSTATSOCTETS_LOW 0x01DB

/*tx_stats_etherStatsPkts*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS_HI  0x015C
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS_LOW 0x015D

/*rx_stats_etherStatsPkts*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS_HI  0x01DC
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS_LOW 0x01DD

/*tx_stats_etherStatsUndersizePkts*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSUNDERSIZEPKTS_HI  0x015E
#define IPN3KE_MAC_TX_STATS_ETHERSTATSUNDERSIZEPKTS_LOW 0x015F

/*rx_stats_etherStatsUndersizePkts*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSUNDERSIZEPKTS_HI  0x01DE
#define IPN3KE_MAC_RX_STATS_ETHERSTATSUNDERSIZEPKTS_LOW 0x01DF

/*tx_stats_etherStatsOversizePkts*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSOVERSIZEPKTS_HI  0x0160
#define IPN3KE_MAC_TX_STATS_ETHERSTATSOVERSIZEPKTS_LOW 0x0161

/*rx_stats_etherStatsOversizePkts*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSOVERSIZEPKTS_HI  0x01E0
#define IPN3KE_MAC_RX_STATS_ETHERSTATSOVERSIZEPKTS_LOW 0x01E1

/*tx_stats_etherStatsPkts64Octets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS64OCTETS_HI  0x0162
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS64OCTETS_LOW 0x0163

/*rx_stats_etherStatsPkts64Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS64OCTETS_HI  0x01E2
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS64OCTETS_LOW 0x01E3

/*tx_stats_etherStatsPkts65to127Octets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS65TO127OCTETS_HI  0x0164
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS65TO127OCTETS_LOW 0x0165

/*rx_stats_etherStatsPkts65to127Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS65TO127OCTETS_HI  0x01E4
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS65TO127OCTETS_LOW 0x01E5

/*tx_stats_etherStatsPkts128to255Octets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS128TO255OCTETS_HI  0x0166
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS128TO255OCTETS_LOW 0x0167

/*rx_stats_etherStatsPkts128to255Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS128TO255OCTETS_HI  0x01E6
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS128TO255OCTETS_LOW 0x01E7

/*tx_stats_etherStatsPkts256to511Octet*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS256TO511OCTET_HI  0x0168
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS256TO511OCTET_LOW 0x0169

/*rx_stats_etherStatsPkts256to511Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS256TO511OCTETS_HI  0x01E8
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS256TO511OCTETS_LOW 0x01E9

/*tx_stats_etherStatsPkts512to1023Octets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS512TO1023OCTETS_HI  0x016A
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS512TO1023OCTETS_LOW 0x016B

/*rx_stats_etherStatsPkts512to1023Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS512TO1023OCTETS_HI  0x01EA
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS512TO1023OCTETS_LOW 0x01EB

/*tx_stats_etherStatPkts1024to1518Octets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATPKTS1024TO1518OCTETS_HI  0x016C
#define IPN3KE_MAC_TX_STATS_ETHERSTATPKTS1024TO1518OCTETS_LOW 0x016D

/*rx_stats_etherStatPkts1024to1518Octets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATPKTS1024TO1518OCTETS_HI  0x01EC
#define IPN3KE_MAC_RX_STATS_ETHERSTATPKTS1024TO1518OCTETS_LOW 0x01ED

/*tx_stats_etherStatsPkts1519toXOctets*/
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS1519TOXOCTETS_HI  0x016E
#define IPN3KE_MAC_TX_STATS_ETHERSTATSPKTS1519TOXOCTETS_LOW 0x016F

/*rx_stats_etherStatsPkts1519toXOctets*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS1519TOXOCTETS_HI  0x01EE
#define IPN3KE_MAC_RX_STATS_ETHERSTATSPKTS1519TOXOCTETS_LOW 0x01EF

/*rx_stats_etherStatsFragments*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSFRAGMENTS_HI  0x01F0
#define IPN3KE_MAC_RX_STATS_ETHERSTATSFRAGMENTS_LOW 0x01F1

/*rx_stats_etherStatsJabbers*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSJABBERS_HI  0x01F2
#define IPN3KE_MAC_RX_STATS_ETHERSTATSJABBERS_LOW 0x01F3

/*rx_stats_etherStatsCRCErr*/
#define IPN3KE_MAC_RX_STATS_ETHERSTATSCRCERR_HI  0x01F4
#define IPN3KE_MAC_RX_STATS_ETHERSTATSCRCERR_LOW 0x01F5

/*tx_stats_unicastMACCtrlFrames*/
#define IPN3KE_MAC_TX_STATS_UNICASTMACCTRLFRAMES_HI  0x0176
#define IPN3KE_MAC_TX_STATS_UNICASTMACCTRLFRAMES_LOW 0x0177

/*rx_stats_unicastMACCtrlFrames*/
#define IPN3KE_MAC_RX_STATS_UNICASTMACCTRLFRAMES_HI  0x01F6
#define IPN3KE_MAC_RX_STATS_UNICASTMACCTRLFRAMES_LOW 0x01F7

/*tx_stats_multicastMACCtrlFrames*/
#define IPN3KE_MAC_TX_STATS_MULTICASTMACCTRLFRAMES_HI  0x0178
#define IPN3KE_MAC_TX_STATS_MULTICASTMACCTRLFRAMES_LOW 0x0179

/*rx_stats_multicastMACCtrlFrames*/
#define IPN3KE_MAC_RX_STATS_MULTICASTMACCTRLFRAMES_HI  0x01F8
#define IPN3KE_MAC_RX_STATS_MULTICASTMACCTRLFRAMES_LOW 0x01F9

/*tx_stats_broadcastMACCtrlFrames*/
#define IPN3KE_MAC_TX_STATS_BROADCASTMACCTRLFRAMES_HI  0x017A
#define IPN3KE_MAC_TX_STATS_BROADCASTMACCTRLFRAMES_LOW 0x017B

/*rx_stats_broadcastMACCtrlFrames*/
#define IPN3KE_MAC_RX_STATS_BROADCASTMACCTRLFRAMES_HI  0x01FA
#define IPN3KE_MAC_RX_STATS_BROADCASTMACCTRLFRAMES_LOW 0x01FB

/*tx_stats_PFCMACCtrlFrames*/
#define IPN3KE_MAC_TX_STATS_PFCMACCTRLFRAMES_HI  0x017C
#define IPN3KE_MAC_TX_STATS_PFCMACCTRLFRAMES_LOW 0x017D

/*rx_stats_PFCMACCtrlFrames*/
#define IPN3KE_MAC_RX_STATS_PFCMACCTRLFRAMES_HI  0x01FC
#define IPN3KE_MAC_RX_STATS_PFCMACCTRLFRAMES_LOW 0x01FD

static inline void ipn3ke_xmac_tx_enable(struct ipn3ke_hw *hw,
		uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_TX_ENABLE (0 & (IPN3KE_MAC_TX_PACKET_CONTROL_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_TX_ENABLE,
					IPN3KE_MAC_TX_PACKET_CONTROL,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_tx_disable(struct ipn3ke_hw *hw,
		uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_TX_DISABLE (1 & (IPN3KE_MAC_TX_PACKET_CONTROL_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_TX_DISABLE,
					IPN3KE_MAC_TX_PACKET_CONTROL,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_rx_enable(struct ipn3ke_hw *hw,
		uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_RX_ENABLE (0 & (IPN3KE_MAC_RX_TRANSFER_CONTROL_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_RX_ENABLE,
					IPN3KE_MAC_RX_TRANSFER_CONTROL,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_rx_disable(struct ipn3ke_hw *hw,
		uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_RX_DISABLE (1 & (IPN3KE_MAC_RX_TRANSFER_CONTROL_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_RX_DISABLE,
					IPN3KE_MAC_RX_TRANSFER_CONTROL,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_smac_ovd_dis(struct ipn3ke_hw *hw,
	uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_SMAC_OVERRIDE_DISABLE (0 & \
	(IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_SMAC_OVERRIDE_DISABLE,
					IPN3KE_MAC_TX_SRC_ADDR_OVERRIDE,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_tx_clr_stcs(struct ipn3ke_hw *hw,
	uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_TX_CLR_STCS (1 & \
	(IPN3KE_MAC_TX_STATS_CLR_CLEAR_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_TX_CLR_STCS,
					IPN3KE_MAC_TX_STATS_CLR,
					mac_num,
					eth_group_sel);
}

static inline void ipn3ke_xmac_rx_clr_stcs(struct ipn3ke_hw *hw,
	uint32_t mac_num, uint32_t eth_group_sel)
{
#define IPN3KE_XMAC_RX_CLR_STCS (1 & \
	(IPN3KE_MAC_RX_STATS_CLR_CLEAR_MASK))

	(*hw->f_mac_write)(hw,
					IPN3KE_XMAC_RX_CLR_STCS,
					IPN3KE_MAC_RX_STATS_CLR,
					mac_num,
					eth_group_sel);
}


#endif /* _IPN3KE_ETHDEV_H_ */
