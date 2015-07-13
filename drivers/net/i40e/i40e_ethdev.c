/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_string_fns.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_alarm.h>
#include <rte_dev.h>
#include <rte_eth_ctrl.h>

#include "i40e_logs.h"
#include "base/i40e_prototype.h"
#include "base/i40e_adminq_cmd.h"
#include "base/i40e_type.h"
#include "base/i40e_register.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_pf.h"

/* Maximun number of MAC addresses */
#define I40E_NUM_MACADDR_MAX       64
#define I40E_CLEAR_PXE_WAIT_MS     200

/* Maximun number of capability elements */
#define I40E_MAX_CAP_ELE_NUM       128

/* Wait count and inteval */
#define I40E_CHK_Q_ENA_COUNT       1000
#define I40E_CHK_Q_ENA_INTERVAL_US 1000

/* Maximun number of VSI */
#define I40E_MAX_NUM_VSIS          (384UL)

/* Default queue interrupt throttling time in microseconds */
#define I40E_ITR_INDEX_DEFAULT          0
#define I40E_QUEUE_ITR_INTERVAL_DEFAULT 32 /* 32 us */
#define I40E_QUEUE_ITR_INTERVAL_MAX     8160 /* 8160 us */

#define I40E_PRE_TX_Q_CFG_WAIT_US       10 /* 10 us */

/* Mask of PF interrupt causes */
#define I40E_PFINT_ICR0_ENA_MASK ( \
		I40E_PFINT_ICR0_ENA_ECC_ERR_MASK | \
		I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK | \
		I40E_PFINT_ICR0_ENA_GRST_MASK | \
		I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK | \
		I40E_PFINT_ICR0_ENA_STORM_DETECT_MASK | \
		I40E_PFINT_ICR0_ENA_LINK_STAT_CHANGE_MASK | \
		I40E_PFINT_ICR0_ENA_HMC_ERR_MASK | \
		I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK | \
		I40E_PFINT_ICR0_ENA_VFLR_MASK | \
		I40E_PFINT_ICR0_ENA_ADMINQ_MASK)

#define I40E_FLOW_TYPES ( \
	(1UL << RTE_ETH_FLOW_FRAG_IPV4) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1UL << RTE_ETH_FLOW_FRAG_IPV6) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER) | \
	(1UL << RTE_ETH_FLOW_L2_PAYLOAD))

#define I40E_PTP_40GB_INCVAL  0x0199999999ULL
#define I40E_PTP_10GB_INCVAL  0x0333333333ULL
#define I40E_PTP_1GB_INCVAL   0x2000000000ULL
#define I40E_PRTTSYN_TSYNENA  0x80000000
#define I40E_PRTTSYN_TSYNTYPE 0x0e000000

static int eth_i40e_dev_init(struct rte_eth_dev *eth_dev);
static int eth_i40e_dev_uninit(struct rte_eth_dev *eth_dev);
static int i40e_dev_configure(struct rte_eth_dev *dev);
static int i40e_dev_start(struct rte_eth_dev *dev);
static void i40e_dev_stop(struct rte_eth_dev *dev);
static void i40e_dev_close(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int i40e_dev_set_link_up(struct rte_eth_dev *dev);
static int i40e_dev_set_link_down(struct rte_eth_dev *dev);
static void i40e_dev_stats_get(struct rte_eth_dev *dev,
			       struct rte_eth_stats *stats);
static void i40e_dev_stats_reset(struct rte_eth_dev *dev);
static int i40e_dev_queue_stats_mapping_set(struct rte_eth_dev *dev,
					    uint16_t queue_id,
					    uint8_t stat_idx,
					    uint8_t is_rx);
static void i40e_dev_info_get(struct rte_eth_dev *dev,
			      struct rte_eth_dev_info *dev_info);
static int i40e_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id,
				int on);
static void i40e_vlan_tpid_set(struct rte_eth_dev *dev, uint16_t tpid);
static void i40e_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static void i40e_vlan_strip_queue_set(struct rte_eth_dev *dev,
				      uint16_t queue,
				      int on);
static int i40e_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on);
static int i40e_dev_led_on(struct rte_eth_dev *dev);
static int i40e_dev_led_off(struct rte_eth_dev *dev);
static int i40e_flow_ctrl_set(struct rte_eth_dev *dev,
			      struct rte_eth_fc_conf *fc_conf);
static int i40e_priority_flow_ctrl_set(struct rte_eth_dev *dev,
				       struct rte_eth_pfc_conf *pfc_conf);
static void i40e_macaddr_add(struct rte_eth_dev *dev,
			  struct ether_addr *mac_addr,
			  uint32_t index,
			  uint32_t pool);
static void i40e_macaddr_remove(struct rte_eth_dev *dev, uint32_t index);
static int i40e_dev_rss_reta_update(struct rte_eth_dev *dev,
				    struct rte_eth_rss_reta_entry64 *reta_conf,
				    uint16_t reta_size);
static int i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta_entry64 *reta_conf,
				   uint16_t reta_size);

static int i40e_get_cap(struct i40e_hw *hw);
static int i40e_pf_parameter_init(struct rte_eth_dev *dev);
static int i40e_pf_setup(struct i40e_pf *pf);
static int i40e_dev_rxtx_init(struct i40e_pf *pf);
static int i40e_vmdq_setup(struct rte_eth_dev *dev);
static void i40e_stat_update_32(struct i40e_hw *hw, uint32_t reg,
		bool offset_loaded, uint64_t *offset, uint64_t *stat);
static void i40e_stat_update_48(struct i40e_hw *hw,
			       uint32_t hireg,
			       uint32_t loreg,
			       bool offset_loaded,
			       uint64_t *offset,
			       uint64_t *stat);
static void i40e_pf_config_irq0(struct i40e_hw *hw);
static void i40e_dev_interrupt_handler(
		__rte_unused struct rte_intr_handle *handle, void *param);
static int i40e_res_pool_init(struct i40e_res_pool_info *pool,
				uint32_t base, uint32_t num);
static void i40e_res_pool_destroy(struct i40e_res_pool_info *pool);
static int i40e_res_pool_free(struct i40e_res_pool_info *pool,
			uint32_t base);
static int i40e_res_pool_alloc(struct i40e_res_pool_info *pool,
			uint16_t num);
static int i40e_dev_init_vlan(struct rte_eth_dev *dev);
static int i40e_veb_release(struct i40e_veb *veb);
static struct i40e_veb *i40e_veb_setup(struct i40e_pf *pf,
						struct i40e_vsi *vsi);
static int i40e_pf_config_mq_rx(struct i40e_pf *pf);
static int i40e_vsi_config_double_vlan(struct i40e_vsi *vsi, int on);
static inline int i40e_find_all_vlan_for_mac(struct i40e_vsi *vsi,
					     struct i40e_macvlan_filter *mv_f,
					     int num,
					     struct ether_addr *addr);
static inline int i40e_find_all_mac_for_vlan(struct i40e_vsi *vsi,
					     struct i40e_macvlan_filter *mv_f,
					     int num,
					     uint16_t vlan);
static int i40e_vsi_remove_all_macvlan_filter(struct i40e_vsi *vsi);
static int i40e_dev_rss_hash_update(struct rte_eth_dev *dev,
				    struct rte_eth_rss_conf *rss_conf);
static int i40e_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				      struct rte_eth_rss_conf *rss_conf);
static int i40e_dev_udp_tunnel_add(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel);
static int i40e_dev_udp_tunnel_del(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel);
static int i40e_ethertype_filter_set(struct i40e_pf *pf,
			struct rte_eth_ethertype_filter *filter,
			bool add);
static int i40e_ethertype_filter_handle(struct rte_eth_dev *dev,
				enum rte_filter_op filter_op,
				void *arg);
static int i40e_dev_filter_ctrl(struct rte_eth_dev *dev,
				enum rte_filter_type filter_type,
				enum rte_filter_op filter_op,
				void *arg);
static void i40e_configure_registers(struct i40e_hw *hw);
static void i40e_hw_init(struct i40e_hw *hw);
static int i40e_config_qinq(struct i40e_hw *hw, struct i40e_vsi *vsi);
static int i40e_mirror_rule_set(struct rte_eth_dev *dev,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t sw_id, uint8_t on);
static int i40e_mirror_rule_reset(struct rte_eth_dev *dev, uint8_t sw_id);

static int i40e_timesync_enable(struct rte_eth_dev *dev);
static int i40e_timesync_disable(struct rte_eth_dev *dev);
static int i40e_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
					   struct timespec *timestamp,
					   uint32_t flags);
static int i40e_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
					   struct timespec *timestamp);

static const struct rte_pci_id pci_id_i40e_map[] = {
#define RTE_PCI_DEV_ID_DECL_I40E(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops i40e_eth_dev_ops = {
	.dev_configure                = i40e_dev_configure,
	.dev_start                    = i40e_dev_start,
	.dev_stop                     = i40e_dev_stop,
	.dev_close                    = i40e_dev_close,
	.promiscuous_enable           = i40e_dev_promiscuous_enable,
	.promiscuous_disable          = i40e_dev_promiscuous_disable,
	.allmulticast_enable          = i40e_dev_allmulticast_enable,
	.allmulticast_disable         = i40e_dev_allmulticast_disable,
	.dev_set_link_up              = i40e_dev_set_link_up,
	.dev_set_link_down            = i40e_dev_set_link_down,
	.link_update                  = i40e_dev_link_update,
	.stats_get                    = i40e_dev_stats_get,
	.stats_reset                  = i40e_dev_stats_reset,
	.queue_stats_mapping_set      = i40e_dev_queue_stats_mapping_set,
	.dev_infos_get                = i40e_dev_info_get,
	.vlan_filter_set              = i40e_vlan_filter_set,
	.vlan_tpid_set                = i40e_vlan_tpid_set,
	.vlan_offload_set             = i40e_vlan_offload_set,
	.vlan_strip_queue_set         = i40e_vlan_strip_queue_set,
	.vlan_pvid_set                = i40e_vlan_pvid_set,
	.rx_queue_start               = i40e_dev_rx_queue_start,
	.rx_queue_stop                = i40e_dev_rx_queue_stop,
	.tx_queue_start               = i40e_dev_tx_queue_start,
	.tx_queue_stop                = i40e_dev_tx_queue_stop,
	.rx_queue_setup               = i40e_dev_rx_queue_setup,
	.rx_queue_release             = i40e_dev_rx_queue_release,
	.rx_queue_count               = i40e_dev_rx_queue_count,
	.rx_descriptor_done           = i40e_dev_rx_descriptor_done,
	.tx_queue_setup               = i40e_dev_tx_queue_setup,
	.tx_queue_release             = i40e_dev_tx_queue_release,
	.dev_led_on                   = i40e_dev_led_on,
	.dev_led_off                  = i40e_dev_led_off,
	.flow_ctrl_set                = i40e_flow_ctrl_set,
	.priority_flow_ctrl_set       = i40e_priority_flow_ctrl_set,
	.mac_addr_add                 = i40e_macaddr_add,
	.mac_addr_remove              = i40e_macaddr_remove,
	.reta_update                  = i40e_dev_rss_reta_update,
	.reta_query                   = i40e_dev_rss_reta_query,
	.rss_hash_update              = i40e_dev_rss_hash_update,
	.rss_hash_conf_get            = i40e_dev_rss_hash_conf_get,
	.udp_tunnel_add               = i40e_dev_udp_tunnel_add,
	.udp_tunnel_del               = i40e_dev_udp_tunnel_del,
	.filter_ctrl                  = i40e_dev_filter_ctrl,
	.mirror_rule_set              = i40e_mirror_rule_set,
	.mirror_rule_reset            = i40e_mirror_rule_reset,
	.timesync_enable              = i40e_timesync_enable,
	.timesync_disable             = i40e_timesync_disable,
	.timesync_read_rx_timestamp   = i40e_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp   = i40e_timesync_read_tx_timestamp,
};

static struct eth_driver rte_i40e_pmd = {
	.pci_drv = {
		.name = "rte_i40e_pmd",
		.id_table = pci_id_i40e_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
			RTE_PCI_DRV_DETACHABLE,
	},
	.eth_dev_init = eth_i40e_dev_init,
	.eth_dev_uninit = eth_i40e_dev_uninit,
	.dev_private_size = sizeof(struct i40e_adapter),
};

static inline int
rte_i40e_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				     struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline int
rte_i40e_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				      struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI IXGBE devices.
 */
static int
rte_i40e_pmd_init(const char *name __rte_unused,
		  const char *params __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	rte_eth_driver_register(&rte_i40e_pmd);

	return 0;
}

static struct rte_driver rte_i40e_driver = {
	.type = PMD_PDEV,
	.init = rte_i40e_pmd_init,
};

PMD_REGISTER_DRIVER(rte_i40e_driver);

/*
 * Initialize registers for flexible payload, which should be set by NVM.
 * This should be removed from code once it is fixed in NVM.
 */
#ifndef I40E_GLQF_ORT
#define I40E_GLQF_ORT(_i)    (0x00268900 + ((_i) * 4))
#endif
#ifndef I40E_GLQF_PIT
#define I40E_GLQF_PIT(_i)    (0x00268C80 + ((_i) * 4))
#endif

static inline void i40e_flex_payload_reg_init(struct i40e_hw *hw)
{
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(18), 0x00000030);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(19), 0x00000030);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(26), 0x0000002B);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(30), 0x0000002B);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(33), 0x000000E0);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(34), 0x000000E3);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(35), 0x000000E6);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(20), 0x00000031);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(23), 0x00000031);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(63), 0x0000002D);

	/* GLQF_PIT Registers */
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(16), 0x00007480);
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(17), 0x00007440);
}

static int
eth_i40e_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi;
	int ret;
	uint32_t len;
	uint8_t aq_fail = 0;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &i40e_eth_dev_ops;
	dev->rx_pkt_burst = i40e_recv_pkts;
	dev->tx_pkt_burst = i40e_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		if (dev->data->scattered_rx)
			dev->rx_pkt_burst = i40e_recv_scattered_pkts;
		return 0;
	}
	pci_dev = dev->pci_dev;
	pf->adapter = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	pf->adapter->eth_dev = dev;
	pf->dev_data = dev->data;

	hw->back = I40E_PF_TO_ADAPTER(pf);
	hw->hw_addr = (uint8_t *)(pci_dev->mem_resource[0].addr);
	if (!hw->hw_addr) {
		PMD_INIT_LOG(ERR, "Hardware is not available, "
			     "as address is NULL");
		return -ENODEV;
	}

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;
	hw->adapter_stopped = 0;

	/* Make sure all is clean before doing PF reset */
	i40e_clear_hw(hw);

	/* Initialize the hardware */
	i40e_hw_init(hw);

	/* Reset here to make sure all is clean for each PF */
	ret = i40e_pf_reset(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to reset pf: %d", ret);
		return ret;
	}

	/* Initialize the shared code (base driver) */
	ret = i40e_init_shared_code(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init shared code (base driver): %d", ret);
		return ret;
	}

	/*
	 * To work around the NVM issue,initialize registers
	 * for flexible payload by software.
	 * It should be removed once issues are fixed in NVM.
	 */
	i40e_flex_payload_reg_init(hw);

	/* Initialize the parameters for adminq */
	i40e_init_adminq_parameter(hw);
	ret = i40e_init_adminq(hw);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to init adminq: %d", ret);
		return -EIO;
	}
	PMD_INIT_LOG(INFO, "FW %d.%d API %d.%d NVM %02d.%02d.%02d eetrack %04x",
		     hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
		     hw->aq.api_maj_ver, hw->aq.api_min_ver,
		     ((hw->nvm.version >> 12) & 0xf),
		     ((hw->nvm.version >> 4) & 0xff),
		     (hw->nvm.version & 0xf), hw->nvm.eetrack);

	/* Disable LLDP */
	ret = i40e_aq_stop_lldp(hw, true, NULL);
	if (ret != I40E_SUCCESS) /* Its failure can be ignored */
		PMD_INIT_LOG(INFO, "Failed to stop lldp");

	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);

	/*
	 * On X710, performance number is far from the expectation on recent
	 * firmware versions. The fix for this issue may not be integrated in
	 * the following firmware version. So the workaround in software driver
	 * is needed. It needs to modify the initial values of 3 internal only
	 * registers. Note that the workaround can be removed when it is fixed
	 * in firmware in the future.
	 */
	i40e_configure_registers(hw);

	/* Get hw capabilities */
	ret = i40e_get_cap(hw);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to get capabilities: %d", ret);
		goto err_get_capabilities;
	}

	/* Initialize parameters for PF */
	ret = i40e_pf_parameter_init(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to do parameter init: %d", ret);
		goto err_parameter_init;
	}

	/* Initialize the queue management */
	ret = i40e_res_pool_init(&pf->qp_pool, 0, hw->func_caps.num_tx_qp);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to init queue pool");
		goto err_qp_pool_init;
	}
	ret = i40e_res_pool_init(&pf->msix_pool, 1,
				hw->func_caps.num_msix_vectors - 1);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to init MSIX pool");
		goto err_msix_pool_init;
	}

	/* Initialize lan hmc */
	ret = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
				hw->func_caps.num_rx_qp, 0, 0);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to init lan hmc: %d", ret);
		goto err_init_lan_hmc;
	}

	/* Configure lan hmc */
	ret = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to configure lan hmc: %d", ret);
		goto err_configure_lan_hmc;
	}

	/* Get and check the mac address */
	i40e_get_mac_addr(hw, hw->mac.addr);
	if (i40e_validate_mac_addr(hw->mac.addr) != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "mac address is not valid");
		ret = -EIO;
		goto err_get_mac_addr;
	}
	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->mac.addr,
			(struct ether_addr *) hw->mac.perm_addr);

	/* Disable flow control */
	hw->fc.requested_mode = I40E_FC_NONE;
	i40e_set_fc(hw, &aq_fail, TRUE);

	/* PF setup, which includes VSI setup */
	ret = i40e_pf_setup(pf);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup pf switch: %d", ret);
		goto err_setup_pf_switch;
	}

	vsi = pf->main_vsi;

	/* Disable double vlan by default */
	i40e_vsi_config_double_vlan(vsi, FALSE);

	if (!vsi->max_macaddrs)
		len = ETHER_ADDR_LEN;
	else
		len = ETHER_ADDR_LEN * vsi->max_macaddrs;

	/* Should be after VSI initialized */
	dev->data->mac_addrs = rte_zmalloc("i40e", len, 0);
	if (!dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR, "Failed to allocated memory "
					"for storing mac address");
		goto err_mac_alloc;
	}
	ether_addr_copy((struct ether_addr *)hw->mac.perm_addr,
					&dev->data->mac_addrs[0]);

	/* initialize pf host driver to setup SRIOV resource if applicable */
	i40e_pf_host_init(dev);

	/* register callback func to eal lib */
	rte_intr_callback_register(&(pci_dev->intr_handle),
		i40e_dev_interrupt_handler, (void *)dev);

	/* configure and enable device interrupt */
	i40e_pf_config_irq0(hw);
	i40e_pf_enable_irq0(hw);

	/* enable uio intr after callback register */
	rte_intr_enable(&(pci_dev->intr_handle));

	/* initialize mirror rule list */
	TAILQ_INIT(&pf->mirror_list);

	return 0;

err_mac_alloc:
	i40e_vsi_release(pf->main_vsi);
err_setup_pf_switch:
err_get_mac_addr:
err_configure_lan_hmc:
	(void)i40e_shutdown_lan_hmc(hw);
err_init_lan_hmc:
	i40e_res_pool_destroy(&pf->msix_pool);
err_msix_pool_init:
	i40e_res_pool_destroy(&pf->qp_pool);
err_qp_pool_init:
err_parameter_init:
err_get_capabilities:
	(void)i40e_shutdown_adminq(hw);

	return ret;
}

static int
eth_i40e_dev_uninit(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev;
	struct i40e_hw *hw;
	struct i40e_filter_control_settings settings;
	int ret;
	uint8_t aq_fail = 0;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = dev->pci_dev;

	if (hw->adapter_stopped == 0)
		i40e_dev_close(dev);

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	/* Disable LLDP */
	ret = i40e_aq_stop_lldp(hw, true, NULL);
	if (ret != I40E_SUCCESS) /* Its failure can be ignored */
		PMD_INIT_LOG(INFO, "Failed to stop lldp");

	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);

	/* Unconfigure filter control */
	memset(&settings, 0, sizeof(settings));
	ret = i40e_set_filter_control(hw, &settings);
	if (ret)
		PMD_INIT_LOG(WARNING, "setup_pf_filter_control failed: %d",
					ret);

	/* Disable flow control */
	hw->fc.requested_mode = I40E_FC_NONE;
	i40e_set_fc(hw, &aq_fail, TRUE);

	/* uninitialize pf host driver */
	i40e_pf_host_uninit(dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	/* disable uio intr before callback unregister */
	rte_intr_disable(&(pci_dev->intr_handle));

	/* register callback func to eal lib */
	rte_intr_callback_unregister(&(pci_dev->intr_handle),
		i40e_dev_interrupt_handler, (void *)dev);

	return 0;
}

static int
i40e_dev_configure(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_eth_rx_mq_mode mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	int ret;

	if (dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_PERFECT) {
		ret = i40e_fdir_setup(pf);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to setup flow director.");
			return -ENOTSUP;
		}
		ret = i40e_fdir_configure(dev);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "failed to configure fdir.");
			goto err;
		}
	} else
		i40e_fdir_teardown(pf);

	ret = i40e_dev_init_vlan(dev);
	if (ret < 0)
		goto err;

	/* VMDQ setup.
	 *  Needs to move VMDQ setting out of i40e_pf_config_mq_rx() as VMDQ and
	 *  RSS setting have different requirements.
	 *  General PMD driver call sequence are NIC init, configure,
	 *  rx/tx_queue_setup and dev_start. In rx/tx_queue_setup() function, it
	 *  will try to lookup the VSI that specific queue belongs to if VMDQ
	 *  applicable. So, VMDQ setting has to be done before
	 *  rx/tx_queue_setup(). This function is good  to place vmdq_setup.
	 *  For RSS setting, it will try to calculate actual configured RX queue
	 *  number, which will be available after rx_queue_setup(). dev_start()
	 *  function is good to place RSS setup.
	 */
	if (mq_mode & ETH_MQ_RX_VMDQ_FLAG) {
		ret = i40e_vmdq_setup(dev);
		if (ret)
			goto err;
	}
	return 0;
err:
	i40e_fdir_teardown(pf);
	return ret;
}

void
i40e_vsi_queues_unbind_intr(struct i40e_vsi *vsi)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	uint16_t i;

	for (i = 0; i < vsi->nb_qps; i++) {
		I40E_WRITE_REG(hw, I40E_QINT_TQCTL(vsi->base_queue + i), 0);
		I40E_WRITE_REG(hw, I40E_QINT_RQCTL(vsi->base_queue + i), 0);
		rte_wmb();
	}

	if (vsi->type != I40E_VSI_SRIOV) {
		I40E_WRITE_REG(hw, I40E_PFINT_LNKLSTN(msix_vect - 1), 0);
		I40E_WRITE_REG(hw, I40E_PFINT_ITRN(I40E_ITR_INDEX_DEFAULT,
				msix_vect - 1), 0);
	} else {
		uint32_t reg;
		reg = (hw->func_caps.num_msix_vectors_vf - 1) *
			vsi->user_param + (msix_vect - 1);

		I40E_WRITE_REG(hw, I40E_VPINT_LNKLSTN(reg), 0);
	}
	I40E_WRITE_FLUSH(hw);
}

static inline uint16_t
i40e_calc_itr_interval(int16_t interval)
{
	if (interval < 0 || interval > I40E_QUEUE_ITR_INTERVAL_MAX)
		interval = I40E_QUEUE_ITR_INTERVAL_DEFAULT;

	/* Convert to hardware count, as writing each 1 represents 2 us */
	return (interval/2);
}

void
i40e_vsi_queues_bind_intr(struct i40e_vsi *vsi)
{
	uint32_t val;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	int i;

	for (i = 0; i < vsi->nb_qps; i++)
		I40E_WRITE_REG(hw, I40E_QINT_TQCTL(vsi->base_queue + i), 0);

	/* Bind all RX queues to allocated MSIX interrupt */
	for (i = 0; i < vsi->nb_qps; i++) {
		val = (msix_vect << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
			I40E_QINT_RQCTL_ITR_INDX_MASK |
			((vsi->base_queue + i + 1) <<
			I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
			(0 << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
			I40E_QINT_RQCTL_CAUSE_ENA_MASK;

		if (i == vsi->nb_qps - 1)
			val |= I40E_QINT_RQCTL_NEXTQ_INDX_MASK;
		I40E_WRITE_REG(hw, I40E_QINT_RQCTL(vsi->base_queue + i), val);
	}

	/* Write first RX queue to Link list register as the head element */
	if (vsi->type != I40E_VSI_SRIOV) {
		uint16_t interval =
			i40e_calc_itr_interval(RTE_LIBRTE_I40E_ITR_INTERVAL);

		I40E_WRITE_REG(hw, I40E_PFINT_LNKLSTN(msix_vect - 1),
						(vsi->base_queue <<
				I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
			(0x0 << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));

		I40E_WRITE_REG(hw, I40E_PFINT_ITRN(I40E_ITR_INDEX_DEFAULT,
						msix_vect - 1), interval);

#ifndef I40E_GLINT_CTL
#define I40E_GLINT_CTL                     0x0003F800
#define I40E_GLINT_CTL_DIS_AUTOMASK_N_MASK 0x4
#endif
		/* Disable auto-mask on enabling of all none-zero  interrupt */
		I40E_WRITE_REG(hw, I40E_GLINT_CTL,
			I40E_GLINT_CTL_DIS_AUTOMASK_N_MASK);
	} else {
		uint32_t reg;

		/* num_msix_vectors_vf needs to minus irq0 */
		reg = (hw->func_caps.num_msix_vectors_vf - 1) *
			vsi->user_param + (msix_vect - 1);

		I40E_WRITE_REG(hw, I40E_VPINT_LNKLSTN(reg), (vsi->base_queue <<
					I40E_VPINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
				(0x0 << I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));
	}

	I40E_WRITE_FLUSH(hw);
}

static void
i40e_vsi_enable_queues_intr(struct i40e_vsi *vsi)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t interval = i40e_calc_itr_interval(\
			RTE_LIBRTE_I40E_ITR_INTERVAL);

	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTLN(vsi->msix_intr - 1),
					I40E_PFINT_DYN_CTLN_INTENA_MASK |
					I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
				(0 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT) |
			(interval << I40E_PFINT_DYN_CTLN_INTERVAL_SHIFT));
}

static void
i40e_vsi_disable_queues_intr(struct i40e_vsi *vsi)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTLN(vsi->msix_intr - 1), 0);
}

static inline uint8_t
i40e_parse_link_speed(uint16_t eth_link_speed)
{
	uint8_t link_speed = I40E_LINK_SPEED_UNKNOWN;

	switch (eth_link_speed) {
	case ETH_LINK_SPEED_40G:
		link_speed = I40E_LINK_SPEED_40GB;
		break;
	case ETH_LINK_SPEED_20G:
		link_speed = I40E_LINK_SPEED_20GB;
		break;
	case ETH_LINK_SPEED_10G:
		link_speed = I40E_LINK_SPEED_10GB;
		break;
	case ETH_LINK_SPEED_1000:
		link_speed = I40E_LINK_SPEED_1GB;
		break;
	case ETH_LINK_SPEED_100:
		link_speed = I40E_LINK_SPEED_100MB;
		break;
	}

	return link_speed;
}

static int
i40e_phy_conf_link(struct i40e_hw *hw, uint8_t abilities, uint8_t force_speed)
{
	enum i40e_status_code status;
	struct i40e_aq_get_phy_abilities_resp phy_ab;
	struct i40e_aq_set_phy_config phy_conf;
	const uint8_t mask = I40E_AQ_PHY_FLAG_PAUSE_TX |
			I40E_AQ_PHY_FLAG_PAUSE_RX |
			I40E_AQ_PHY_FLAG_LOW_POWER;
	const uint8_t advt = I40E_LINK_SPEED_40GB |
			I40E_LINK_SPEED_10GB |
			I40E_LINK_SPEED_1GB |
			I40E_LINK_SPEED_100MB;
	int ret = -ENOTSUP;

	/* Skip it on 40G interfaces, as a workaround for the link issue */
	if (i40e_is_40G_device(hw->device_id))
		return I40E_SUCCESS;

	status = i40e_aq_get_phy_capabilities(hw, false, false, &phy_ab,
					      NULL);
	if (status)
		return ret;

	memset(&phy_conf, 0, sizeof(phy_conf));

	/* bits 0-2 use the values from get_phy_abilities_resp */
	abilities &= ~mask;
	abilities |= phy_ab.abilities & mask;

	/* update ablities and speed */
	if (abilities & I40E_AQ_PHY_AN_ENABLED)
		phy_conf.link_speed = advt;
	else
		phy_conf.link_speed = force_speed;

	phy_conf.abilities = abilities;

	/* use get_phy_abilities_resp value for the rest */
	phy_conf.phy_type = phy_ab.phy_type;
	phy_conf.eee_capability = phy_ab.eee_capability;
	phy_conf.eeer = phy_ab.eeer_val;
	phy_conf.low_power_ctrl = phy_ab.d3_lpan;

	PMD_DRV_LOG(DEBUG, "\tCurrent: abilities %x, link_speed %x",
		    phy_ab.abilities, phy_ab.link_speed);
	PMD_DRV_LOG(DEBUG, "\tConfig:  abilities %x, link_speed %x",
		    phy_conf.abilities, phy_conf.link_speed);

	status = i40e_aq_set_phy_config(hw, &phy_conf, NULL);
	if (status)
		return ret;

	return I40E_SUCCESS;
}

static int
i40e_apply_link_speed(struct rte_eth_dev *dev)
{
	uint8_t speed;
	uint8_t abilities = 0;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *conf = &dev->data->dev_conf;

	speed = i40e_parse_link_speed(conf->link_speed);
	abilities |= I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	if (conf->link_speed == ETH_LINK_SPEED_AUTONEG)
		abilities |= I40E_AQ_PHY_AN_ENABLED;
	else
		abilities |= I40E_AQ_PHY_LINK_ENABLED;

	return i40e_phy_conf_link(hw, abilities, speed);
}

static int
i40e_dev_start(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	int ret, i;

	hw->adapter_stopped = 0;

	if ((dev->data->dev_conf.link_duplex != ETH_LINK_AUTONEG_DUPLEX) &&
		(dev->data->dev_conf.link_duplex != ETH_LINK_FULL_DUPLEX)) {
		PMD_INIT_LOG(ERR, "Invalid link_duplex (%hu) for port %hhu",
			     dev->data->dev_conf.link_duplex,
			     dev->data->port_id);
		return -EINVAL;
	}

	/* Initialize VSI */
	ret = i40e_dev_rxtx_init(pf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to init rx/tx queues");
		goto err_up;
	}

	/* Map queues with MSIX interrupt */
	i40e_vsi_queues_bind_intr(main_vsi);
	i40e_vsi_enable_queues_intr(main_vsi);

	/* Map VMDQ VSI queues with MSIX interrupt */
	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_queues_bind_intr(pf->vmdq[i].vsi);
		i40e_vsi_enable_queues_intr(pf->vmdq[i].vsi);
	}

	/* enable FDIR MSIX interrupt */
	if (pf->fdir.fdir_vsi) {
		i40e_vsi_queues_bind_intr(pf->fdir.fdir_vsi);
		i40e_vsi_enable_queues_intr(pf->fdir.fdir_vsi);
	}

	/* Enable all queues which have been configured */
	ret = i40e_dev_switch_queues(pf, TRUE);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to enable VSI");
		goto err_up;
	}

	/* Enable receiving broadcast packets */
	ret = i40e_aq_set_vsi_broadcast(hw, main_vsi->seid, true, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(INFO, "fail to set vsi broadcast");

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		ret = i40e_aq_set_vsi_broadcast(hw, pf->vmdq[i].vsi->seid,
						true, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(INFO, "fail to set vsi broadcast");
	}

	/* Apply link configure */
	ret = i40e_apply_link_speed(dev);
	if (I40E_SUCCESS != ret) {
		PMD_DRV_LOG(ERR, "Fail to apply link setting");
		goto err_up;
	}

	return I40E_SUCCESS;

err_up:
	i40e_dev_switch_queues(pf, FALSE);
	i40e_dev_clear_queues(dev);

	return ret;
}

static void
i40e_dev_stop(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	struct i40e_mirror_rule *p_mirror;
	int i;

	/* Disable all queues */
	i40e_dev_switch_queues(pf, FALSE);

	/* un-map queues with interrupt registers */
	i40e_vsi_disable_queues_intr(main_vsi);
	i40e_vsi_queues_unbind_intr(main_vsi);

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_disable_queues_intr(pf->vmdq[i].vsi);
		i40e_vsi_queues_unbind_intr(pf->vmdq[i].vsi);
	}

	if (pf->fdir.fdir_vsi) {
		i40e_vsi_queues_bind_intr(pf->fdir.fdir_vsi);
		i40e_vsi_enable_queues_intr(pf->fdir.fdir_vsi);
	}
	/* Clear all queues and release memory */
	i40e_dev_clear_queues(dev);

	/* Set link down */
	i40e_dev_set_link_down(dev);

	/* Remove all mirror rules */
	while ((p_mirror = TAILQ_FIRST(&pf->mirror_list))) {
		TAILQ_REMOVE(&pf->mirror_list, p_mirror, rules);
		rte_free(p_mirror);
	}
	pf->nb_mirror_rule = 0;

}

static void
i40e_dev_close(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;
	int i;

	PMD_INIT_FUNC_TRACE();

	i40e_dev_stop(dev);
	hw->adapter_stopped = 1;
	i40e_dev_free_queues(dev);

	/* Disable interrupt */
	i40e_pf_disable_irq0(hw);
	rte_intr_disable(&(dev->pci_dev->intr_handle));

	/* shutdown and destroy the HMC */
	i40e_shutdown_lan_hmc(hw);

	/* release all the existing VSIs and VEBs */
	i40e_fdir_teardown(pf);
	i40e_vsi_release(pf->main_vsi);

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_release(pf->vmdq[i].vsi);
		pf->vmdq[i].vsi = NULL;
	}

	rte_free(pf->vmdq);
	pf->vmdq = NULL;

	/* shutdown the adminq */
	i40e_aq_queue_shutdown(hw, true);
	i40e_shutdown_adminq(hw);

	i40e_res_pool_destroy(&pf->qp_pool);
	i40e_res_pool_destroy(&pf->msix_pool);

	/* force a PF reset to clean anything leftover */
	reg = I40E_READ_REG(hw, I40E_PFGEN_CTRL);
	I40E_WRITE_REG(hw, I40E_PFGEN_CTRL,
			(reg | I40E_PFGEN_CTRL_PFSWR_MASK));
	I40E_WRITE_FLUSH(hw);
}

static void
i40e_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int status;

	status = i40e_aq_set_vsi_unicast_promiscuous(hw, vsi->seid,
							true, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable unicast promiscuous");

	status = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid,
							TRUE, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable multicast promiscuous");

}

static void
i40e_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int status;

	status = i40e_aq_set_vsi_unicast_promiscuous(hw, vsi->seid,
							false, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable unicast promiscuous");

	status = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid,
							false, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable multicast promiscuous");
}

static void
i40e_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid, TRUE, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable multicast promiscuous");
}

static void
i40e_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	if (dev->data->promiscuous == 1)
		return; /* must remain in all_multicast mode */

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw,
				vsi->seid, FALSE, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable multicast promiscuous");
}

/*
 * Set device link up.
 */
static int
i40e_dev_set_link_up(struct rte_eth_dev *dev)
{
	/* re-apply link speed setting */
	return i40e_apply_link_speed(dev);
}

/*
 * Set device link down.
 */
static int
i40e_dev_set_link_down(__rte_unused struct rte_eth_dev *dev)
{
	uint8_t speed = I40E_LINK_SPEED_UNKNOWN;
	uint8_t abilities = I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return i40e_phy_conf_link(hw, abilities, speed);
}

int
i40e_dev_link_update(struct rte_eth_dev *dev,
		     int wait_to_complete)
{
#define CHECK_INTERVAL 100  /* 100ms */
#define MAX_REPEAT_TIME 10  /* 1s (10 * 100ms) in total */
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_link_status link_status;
	struct rte_eth_link link, old;
	int status;
	unsigned rep_cnt = MAX_REPEAT_TIME;

	memset(&link, 0, sizeof(link));
	memset(&old, 0, sizeof(old));
	memset(&link_status, 0, sizeof(link_status));
	rte_i40e_dev_atomic_read_link_status(dev, &old);

	do {
		/* Get link status information from hardware */
		status = i40e_aq_get_link_info(hw, false, &link_status, NULL);
		if (status != I40E_SUCCESS) {
			link.link_speed = ETH_LINK_SPEED_100;
			link.link_duplex = ETH_LINK_FULL_DUPLEX;
			PMD_DRV_LOG(ERR, "Failed to get link info");
			goto out;
		}

		link.link_status = link_status.link_info & I40E_AQ_LINK_UP;
		if (!wait_to_complete)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (!link.link_status && rep_cnt--);

	if (!link.link_status)
		goto out;

	/* i40e uses full duplex only */
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	/* Parse the link status */
	switch (link_status.link_speed) {
	case I40E_LINK_SPEED_100MB:
		link.link_speed = ETH_LINK_SPEED_100;
		break;
	case I40E_LINK_SPEED_1GB:
		link.link_speed = ETH_LINK_SPEED_1000;
		break;
	case I40E_LINK_SPEED_10GB:
		link.link_speed = ETH_LINK_SPEED_10G;
		break;
	case I40E_LINK_SPEED_20GB:
		link.link_speed = ETH_LINK_SPEED_20G;
		break;
	case I40E_LINK_SPEED_40GB:
		link.link_speed = ETH_LINK_SPEED_40G;
		break;
	default:
		link.link_speed = ETH_LINK_SPEED_100;
		break;
	}

out:
	rte_i40e_dev_atomic_write_link_status(dev, &link);
	if (link.link_status == old.link_status)
		return -1;

	return 0;
}

/* Get all the statistics of a VSI */
void
i40e_update_vsi_stats(struct i40e_vsi *vsi)
{
	struct i40e_eth_stats *oes = &vsi->eth_stats_offset;
	struct i40e_eth_stats *nes = &vsi->eth_stats;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int idx = rte_le_to_cpu_16(vsi->info.stat_counter_idx);

	i40e_stat_update_48(hw, I40E_GLV_GORCH(idx), I40E_GLV_GORCL(idx),
			    vsi->offset_loaded, &oes->rx_bytes,
			    &nes->rx_bytes);
	i40e_stat_update_48(hw, I40E_GLV_UPRCH(idx), I40E_GLV_UPRCL(idx),
			    vsi->offset_loaded, &oes->rx_unicast,
			    &nes->rx_unicast);
	i40e_stat_update_48(hw, I40E_GLV_MPRCH(idx), I40E_GLV_MPRCL(idx),
			    vsi->offset_loaded, &oes->rx_multicast,
			    &nes->rx_multicast);
	i40e_stat_update_48(hw, I40E_GLV_BPRCH(idx), I40E_GLV_BPRCL(idx),
			    vsi->offset_loaded, &oes->rx_broadcast,
			    &nes->rx_broadcast);
	i40e_stat_update_32(hw, I40E_GLV_RDPC(idx), vsi->offset_loaded,
			    &oes->rx_discards, &nes->rx_discards);
	/* GLV_REPC not supported */
	/* GLV_RMPC not supported */
	i40e_stat_update_32(hw, I40E_GLV_RUPP(idx), vsi->offset_loaded,
			    &oes->rx_unknown_protocol,
			    &nes->rx_unknown_protocol);
	i40e_stat_update_48(hw, I40E_GLV_GOTCH(idx), I40E_GLV_GOTCL(idx),
			    vsi->offset_loaded, &oes->tx_bytes,
			    &nes->tx_bytes);
	i40e_stat_update_48(hw, I40E_GLV_UPTCH(idx), I40E_GLV_UPTCL(idx),
			    vsi->offset_loaded, &oes->tx_unicast,
			    &nes->tx_unicast);
	i40e_stat_update_48(hw, I40E_GLV_MPTCH(idx), I40E_GLV_MPTCL(idx),
			    vsi->offset_loaded, &oes->tx_multicast,
			    &nes->tx_multicast);
	i40e_stat_update_48(hw, I40E_GLV_BPTCH(idx), I40E_GLV_BPTCL(idx),
			    vsi->offset_loaded,  &oes->tx_broadcast,
			    &nes->tx_broadcast);
	/* GLV_TDPC not supported */
	i40e_stat_update_32(hw, I40E_GLV_TEPC(idx), vsi->offset_loaded,
			    &oes->tx_errors, &nes->tx_errors);
	vsi->offset_loaded = true;

	PMD_DRV_LOG(DEBUG, "***************** VSI[%u] stats start *******************",
		    vsi->vsi_id);
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", nes->rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", nes->rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", nes->rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", nes->rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", nes->rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    nes->rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", nes->tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", nes->tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", nes->tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", nes->tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", nes->tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", nes->tx_errors);
	PMD_DRV_LOG(DEBUG, "***************** VSI[%u] stats end *******************",
		    vsi->vsi_id);
}

/* Get all statistics of a port */
static void
i40e_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint32_t i;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_hw_port_stats *ns = &pf->stats; /* new stats */
	struct i40e_hw_port_stats *os = &pf->stats_offset; /* old stats */

	/* Get statistics of struct i40e_eth_stats */
	i40e_stat_update_48(hw, I40E_GLPRT_GORCH(hw->port),
			    I40E_GLPRT_GORCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_bytes,
			    &ns->eth.rx_bytes);
	i40e_stat_update_48(hw, I40E_GLPRT_UPRCH(hw->port),
			    I40E_GLPRT_UPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_unicast,
			    &ns->eth.rx_unicast);
	i40e_stat_update_48(hw, I40E_GLPRT_MPRCH(hw->port),
			    I40E_GLPRT_MPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_multicast,
			    &ns->eth.rx_multicast);
	i40e_stat_update_48(hw, I40E_GLPRT_BPRCH(hw->port),
			    I40E_GLPRT_BPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_broadcast,
			    &ns->eth.rx_broadcast);
	i40e_stat_update_32(hw, I40E_GLPRT_RDPC(hw->port),
			    pf->offset_loaded, &os->eth.rx_discards,
			    &ns->eth.rx_discards);
	/* GLPRT_REPC not supported */
	/* GLPRT_RMPC not supported */
	i40e_stat_update_32(hw, I40E_GLPRT_RUPP(hw->port),
			    pf->offset_loaded,
			    &os->eth.rx_unknown_protocol,
			    &ns->eth.rx_unknown_protocol);
	i40e_stat_update_48(hw, I40E_GLPRT_GOTCH(hw->port),
			    I40E_GLPRT_GOTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_bytes,
			    &ns->eth.tx_bytes);
	i40e_stat_update_48(hw, I40E_GLPRT_UPTCH(hw->port),
			    I40E_GLPRT_UPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_unicast,
			    &ns->eth.tx_unicast);
	i40e_stat_update_48(hw, I40E_GLPRT_MPTCH(hw->port),
			    I40E_GLPRT_MPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_multicast,
			    &ns->eth.tx_multicast);
	i40e_stat_update_48(hw, I40E_GLPRT_BPTCH(hw->port),
			    I40E_GLPRT_BPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_broadcast,
			    &ns->eth.tx_broadcast);
	/* GLPRT_TEPC not supported */

	/* additional port specific stats */
	i40e_stat_update_32(hw, I40E_GLPRT_TDOLD(hw->port),
			    pf->offset_loaded, &os->tx_dropped_link_down,
			    &ns->tx_dropped_link_down);
	i40e_stat_update_32(hw, I40E_GLPRT_CRCERRS(hw->port),
			    pf->offset_loaded, &os->crc_errors,
			    &ns->crc_errors);
	i40e_stat_update_32(hw, I40E_GLPRT_ILLERRC(hw->port),
			    pf->offset_loaded, &os->illegal_bytes,
			    &ns->illegal_bytes);
	/* GLPRT_ERRBC not supported */
	i40e_stat_update_32(hw, I40E_GLPRT_MLFC(hw->port),
			    pf->offset_loaded, &os->mac_local_faults,
			    &ns->mac_local_faults);
	i40e_stat_update_32(hw, I40E_GLPRT_MRFC(hw->port),
			    pf->offset_loaded, &os->mac_remote_faults,
			    &ns->mac_remote_faults);
	i40e_stat_update_32(hw, I40E_GLPRT_RLEC(hw->port),
			    pf->offset_loaded, &os->rx_length_errors,
			    &ns->rx_length_errors);
	i40e_stat_update_32(hw, I40E_GLPRT_LXONRXC(hw->port),
			    pf->offset_loaded, &os->link_xon_rx,
			    &ns->link_xon_rx);
	i40e_stat_update_32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			    pf->offset_loaded, &os->link_xoff_rx,
			    &ns->link_xoff_rx);
	for (i = 0; i < 8; i++) {
		i40e_stat_update_32(hw, I40E_GLPRT_PXONRXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_rx[i],
				    &ns->priority_xon_rx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_PXOFFRXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xoff_rx[i],
				    &ns->priority_xoff_rx[i]);
	}
	i40e_stat_update_32(hw, I40E_GLPRT_LXONTXC(hw->port),
			    pf->offset_loaded, &os->link_xon_tx,
			    &ns->link_xon_tx);
	i40e_stat_update_32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			    pf->offset_loaded, &os->link_xoff_tx,
			    &ns->link_xoff_tx);
	for (i = 0; i < 8; i++) {
		i40e_stat_update_32(hw, I40E_GLPRT_PXONTXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_tx[i],
				    &ns->priority_xon_tx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_PXOFFTXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xoff_tx[i],
				    &ns->priority_xoff_tx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_RXON2OFFCNT(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_2_xoff[i],
				    &ns->priority_xon_2_xoff[i]);
	}
	i40e_stat_update_48(hw, I40E_GLPRT_PRC64H(hw->port),
			    I40E_GLPRT_PRC64L(hw->port),
			    pf->offset_loaded, &os->rx_size_64,
			    &ns->rx_size_64);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC127H(hw->port),
			    I40E_GLPRT_PRC127L(hw->port),
			    pf->offset_loaded, &os->rx_size_127,
			    &ns->rx_size_127);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC255H(hw->port),
			    I40E_GLPRT_PRC255L(hw->port),
			    pf->offset_loaded, &os->rx_size_255,
			    &ns->rx_size_255);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC511H(hw->port),
			    I40E_GLPRT_PRC511L(hw->port),
			    pf->offset_loaded, &os->rx_size_511,
			    &ns->rx_size_511);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC1023H(hw->port),
			    I40E_GLPRT_PRC1023L(hw->port),
			    pf->offset_loaded, &os->rx_size_1023,
			    &ns->rx_size_1023);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC1522H(hw->port),
			    I40E_GLPRT_PRC1522L(hw->port),
			    pf->offset_loaded, &os->rx_size_1522,
			    &ns->rx_size_1522);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC9522H(hw->port),
			    I40E_GLPRT_PRC9522L(hw->port),
			    pf->offset_loaded, &os->rx_size_big,
			    &ns->rx_size_big);
	i40e_stat_update_32(hw, I40E_GLPRT_RUC(hw->port),
			    pf->offset_loaded, &os->rx_undersize,
			    &ns->rx_undersize);
	i40e_stat_update_32(hw, I40E_GLPRT_RFC(hw->port),
			    pf->offset_loaded, &os->rx_fragments,
			    &ns->rx_fragments);
	i40e_stat_update_32(hw, I40E_GLPRT_ROC(hw->port),
			    pf->offset_loaded, &os->rx_oversize,
			    &ns->rx_oversize);
	i40e_stat_update_32(hw, I40E_GLPRT_RJC(hw->port),
			    pf->offset_loaded, &os->rx_jabber,
			    &ns->rx_jabber);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC64H(hw->port),
			    I40E_GLPRT_PTC64L(hw->port),
			    pf->offset_loaded, &os->tx_size_64,
			    &ns->tx_size_64);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC127H(hw->port),
			    I40E_GLPRT_PTC127L(hw->port),
			    pf->offset_loaded, &os->tx_size_127,
			    &ns->tx_size_127);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC255H(hw->port),
			    I40E_GLPRT_PTC255L(hw->port),
			    pf->offset_loaded, &os->tx_size_255,
			    &ns->tx_size_255);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC511H(hw->port),
			    I40E_GLPRT_PTC511L(hw->port),
			    pf->offset_loaded, &os->tx_size_511,
			    &ns->tx_size_511);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC1023H(hw->port),
			    I40E_GLPRT_PTC1023L(hw->port),
			    pf->offset_loaded, &os->tx_size_1023,
			    &ns->tx_size_1023);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC1522H(hw->port),
			    I40E_GLPRT_PTC1522L(hw->port),
			    pf->offset_loaded, &os->tx_size_1522,
			    &ns->tx_size_1522);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC9522H(hw->port),
			    I40E_GLPRT_PTC9522L(hw->port),
			    pf->offset_loaded, &os->tx_size_big,
			    &ns->tx_size_big);
	i40e_stat_update_32(hw, I40E_GLQF_PCNT(pf->fdir.match_counter_index),
			   pf->offset_loaded,
			   &os->fd_sb_match, &ns->fd_sb_match);
	/* GLPRT_MSPDC not supported */
	/* GLPRT_XEC not supported */

	pf->offset_loaded = true;

	if (pf->main_vsi)
		i40e_update_vsi_stats(pf->main_vsi);

	stats->ipackets = ns->eth.rx_unicast + ns->eth.rx_multicast +
						ns->eth.rx_broadcast;
	stats->opackets = ns->eth.tx_unicast + ns->eth.tx_multicast +
						ns->eth.tx_broadcast;
	stats->ibytes   = ns->eth.rx_bytes;
	stats->obytes   = ns->eth.tx_bytes;
	stats->oerrors  = ns->eth.tx_errors;
	stats->imcasts  = ns->eth.rx_multicast;
	stats->fdirmatch = ns->fd_sb_match;

	/* Rx Errors */
	stats->ibadcrc  = ns->crc_errors;
	stats->ibadlen  = ns->rx_length_errors + ns->rx_undersize +
			ns->rx_oversize + ns->rx_fragments + ns->rx_jabber;
	stats->imissed  = ns->eth.rx_discards;
	stats->ierrors  = stats->ibadcrc + stats->ibadlen + stats->imissed;

	PMD_DRV_LOG(DEBUG, "***************** PF stats start *******************");
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", ns->eth.rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", ns->eth.rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", ns->eth.rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", ns->eth.rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", ns->eth.rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    ns->eth.rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", ns->eth.tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", ns->eth.tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", ns->eth.tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", ns->eth.tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", ns->eth.tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", ns->eth.tx_errors);

	PMD_DRV_LOG(DEBUG, "tx_dropped_link_down:     %"PRIu64"",
		    ns->tx_dropped_link_down);
	PMD_DRV_LOG(DEBUG, "crc_errors:               %"PRIu64"", ns->crc_errors);
	PMD_DRV_LOG(DEBUG, "illegal_bytes:            %"PRIu64"",
		    ns->illegal_bytes);
	PMD_DRV_LOG(DEBUG, "error_bytes:              %"PRIu64"", ns->error_bytes);
	PMD_DRV_LOG(DEBUG, "mac_local_faults:         %"PRIu64"",
		    ns->mac_local_faults);
	PMD_DRV_LOG(DEBUG, "mac_remote_faults:        %"PRIu64"",
		    ns->mac_remote_faults);
	PMD_DRV_LOG(DEBUG, "rx_length_errors:         %"PRIu64"",
		    ns->rx_length_errors);
	PMD_DRV_LOG(DEBUG, "link_xon_rx:              %"PRIu64"", ns->link_xon_rx);
	PMD_DRV_LOG(DEBUG, "link_xoff_rx:             %"PRIu64"", ns->link_xoff_rx);
	for (i = 0; i < 8; i++) {
		PMD_DRV_LOG(DEBUG, "priority_xon_rx[%d]:      %"PRIu64"",
				i, ns->priority_xon_rx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xoff_rx[%d]:     %"PRIu64"",
				i, ns->priority_xoff_rx[i]);
	}
	PMD_DRV_LOG(DEBUG, "link_xon_tx:              %"PRIu64"", ns->link_xon_tx);
	PMD_DRV_LOG(DEBUG, "link_xoff_tx:             %"PRIu64"", ns->link_xoff_tx);
	for (i = 0; i < 8; i++) {
		PMD_DRV_LOG(DEBUG, "priority_xon_tx[%d]:      %"PRIu64"",
				i, ns->priority_xon_tx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xoff_tx[%d]:     %"PRIu64"",
				i, ns->priority_xoff_tx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xon_2_xoff[%d]:  %"PRIu64"",
				i, ns->priority_xon_2_xoff[i]);
	}
	PMD_DRV_LOG(DEBUG, "rx_size_64:               %"PRIu64"", ns->rx_size_64);
	PMD_DRV_LOG(DEBUG, "rx_size_127:              %"PRIu64"", ns->rx_size_127);
	PMD_DRV_LOG(DEBUG, "rx_size_255:              %"PRIu64"", ns->rx_size_255);
	PMD_DRV_LOG(DEBUG, "rx_size_511:              %"PRIu64"", ns->rx_size_511);
	PMD_DRV_LOG(DEBUG, "rx_size_1023:             %"PRIu64"", ns->rx_size_1023);
	PMD_DRV_LOG(DEBUG, "rx_size_1522:             %"PRIu64"", ns->rx_size_1522);
	PMD_DRV_LOG(DEBUG, "rx_size_big:              %"PRIu64"", ns->rx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_undersize:             %"PRIu64"", ns->rx_undersize);
	PMD_DRV_LOG(DEBUG, "rx_fragments:             %"PRIu64"", ns->rx_fragments);
	PMD_DRV_LOG(DEBUG, "rx_oversize:              %"PRIu64"", ns->rx_oversize);
	PMD_DRV_LOG(DEBUG, "rx_jabber:                %"PRIu64"", ns->rx_jabber);
	PMD_DRV_LOG(DEBUG, "tx_size_64:               %"PRIu64"", ns->tx_size_64);
	PMD_DRV_LOG(DEBUG, "tx_size_127:              %"PRIu64"", ns->tx_size_127);
	PMD_DRV_LOG(DEBUG, "tx_size_255:              %"PRIu64"", ns->tx_size_255);
	PMD_DRV_LOG(DEBUG, "tx_size_511:              %"PRIu64"", ns->tx_size_511);
	PMD_DRV_LOG(DEBUG, "tx_size_1023:             %"PRIu64"", ns->tx_size_1023);
	PMD_DRV_LOG(DEBUG, "tx_size_1522:             %"PRIu64"", ns->tx_size_1522);
	PMD_DRV_LOG(DEBUG, "tx_size_big:              %"PRIu64"", ns->tx_size_big);
	PMD_DRV_LOG(DEBUG, "mac_short_packet_dropped: %"PRIu64"",
			ns->mac_short_packet_dropped);
	PMD_DRV_LOG(DEBUG, "checksum_error:           %"PRIu64"",
		    ns->checksum_error);
	PMD_DRV_LOG(DEBUG, "fdir_match:               %"PRIu64"", ns->fd_sb_match);
	PMD_DRV_LOG(DEBUG, "***************** PF stats end ********************");
}

/* Reset the statistics */
static void
i40e_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	/* It results in reloading the start point of each counter */
	pf->offset_loaded = false;
}

static int
i40e_dev_queue_stats_mapping_set(__rte_unused struct rte_eth_dev *dev,
				 __rte_unused uint16_t queue_id,
				 __rte_unused uint8_t stat_idx,
				 __rte_unused uint8_t is_rx)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOSYS;
}

static void
i40e_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;

	dev_info->max_rx_queues = vsi->nb_qps;
	dev_info->max_tx_queues = vsi->nb_qps;
	dev_info->min_rx_bufsize = I40E_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = I40E_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs = vsi->max_macaddrs;
	dev_info->max_vfs = dev->pci_dev->max_vfs;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_QINQ_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO;
	dev_info->hash_key_size = (I40E_PFQF_HKEY_MAX_INDEX + 1) *
						sizeof(uint32_t);
	dev_info->reta_size = pf->hash_lut_size;
	dev_info->flow_type_rss_offloads = I40E_RSS_OFFLOAD_ALL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = I40E_DEFAULT_RX_PTHRESH,
			.hthresh = I40E_DEFAULT_RX_HTHRESH,
			.wthresh = I40E_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = I40E_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = I40E_DEFAULT_TX_PTHRESH,
			.hthresh = I40E_DEFAULT_TX_HTHRESH,
			.wthresh = I40E_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = I40E_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = I40E_DEFAULT_TX_RSBIT_THRESH,
		.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS |
				ETH_TXQ_FLAGS_NOOFFLOADS,
	};

	if (pf->flags & I40E_FLAG_VMDQ) {
		dev_info->max_vmdq_pools = pf->max_nb_vmdq_vsi;
		dev_info->vmdq_queue_base = dev_info->max_rx_queues;
		dev_info->vmdq_queue_num = pf->vmdq_nb_qps *
						pf->max_nb_vmdq_vsi;
		dev_info->vmdq_pool_base = I40E_VMDQ_POOL_BASE;
		dev_info->max_rx_queues += dev_info->vmdq_queue_num;
		dev_info->max_tx_queues += dev_info->vmdq_queue_num;
	}
}

static int
i40e_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	PMD_INIT_FUNC_TRACE();

	if (on)
		return i40e_vsi_add_vlan(vsi, vlan_id);
	else
		return i40e_vsi_delete_vlan(vsi, vlan_id);
}

static void
i40e_vlan_tpid_set(__rte_unused struct rte_eth_dev *dev,
		   __rte_unused uint16_t tpid)
{
	PMD_INIT_FUNC_TRACE();
}

static void
i40e_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;

	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		if (dev->data->dev_conf.rxmode.hw_vlan_strip)
			i40e_vsi_config_vlan_stripping(vsi, TRUE);
		else
			i40e_vsi_config_vlan_stripping(vsi, FALSE);
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (dev->data->dev_conf.rxmode.hw_vlan_extend)
			i40e_vsi_config_double_vlan(vsi, TRUE);
		else
			i40e_vsi_config_double_vlan(vsi, FALSE);
	}
}

static void
i40e_vlan_strip_queue_set(__rte_unused struct rte_eth_dev *dev,
			  __rte_unused uint16_t queue,
			  __rte_unused int on)
{
	PMD_INIT_FUNC_TRACE();
}

static int
i40e_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = I40E_VSI_TO_DEV_DATA(vsi);
	struct i40e_vsi_vlan_pvid_info info;

	memset(&info, 0, sizeof(info));
	info.on = on;
	if (info.on)
		info.config.pvid = pvid;
	else {
		info.config.reject.tagged =
				data->dev_conf.txmode.hw_vlan_reject_tagged;
		info.config.reject.untagged =
				data->dev_conf.txmode.hw_vlan_reject_untagged;
	}

	return i40e_vsi_vlan_pvid_set(vsi, &info);
}

static int
i40e_dev_led_on(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mode = i40e_led_get(hw);

	if (mode == 0)
		i40e_led_set(hw, 0xf, true); /* 0xf means led always true */

	return 0;
}

static int
i40e_dev_led_off(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mode = i40e_led_get(hw);

	if (mode != 0)
		i40e_led_set(hw, 0, false);

	return 0;
}

static int
i40e_flow_ctrl_set(__rte_unused struct rte_eth_dev *dev,
		   __rte_unused struct rte_eth_fc_conf *fc_conf)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOSYS;
}

static int
i40e_priority_flow_ctrl_set(__rte_unused struct rte_eth_dev *dev,
			    __rte_unused struct rte_eth_pfc_conf *pfc_conf)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOSYS;
}

/* Add a MAC address, and update filters */
static void
i40e_macaddr_add(struct rte_eth_dev *dev,
		 struct ether_addr *mac_addr,
		 __rte_unused uint32_t index,
		 uint32_t pool)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_mac_filter_info mac_filter;
	struct i40e_vsi *vsi;
	int ret;

	/* If VMDQ not enabled or configured, return */
	if (pool != 0 && (!(pf->flags | I40E_FLAG_VMDQ) || !pf->nb_cfg_vmdq_vsi)) {
		PMD_DRV_LOG(ERR, "VMDQ not %s, can't set mac to pool %u",
			pf->flags | I40E_FLAG_VMDQ ? "configured" : "enabled",
			pool);
		return;
	}

	if (pool > pf->nb_cfg_vmdq_vsi) {
		PMD_DRV_LOG(ERR, "Pool number %u invalid. Max pool is %u",
				pool, pf->nb_cfg_vmdq_vsi);
		return;
	}

	(void)rte_memcpy(&mac_filter.mac_addr, mac_addr, ETHER_ADDR_LEN);
	mac_filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;

	if (pool == 0)
		vsi = pf->main_vsi;
	else
		vsi = pf->vmdq[pool - 1].vsi;

	ret = i40e_vsi_add_mac(vsi, &mac_filter);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter");
		return;
	}
}

/* Remove a MAC address, and update filters */
static void
i40e_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi;
	struct rte_eth_dev_data *data = dev->data;
	struct ether_addr *macaddr;
	int ret;
	uint32_t i;
	uint64_t pool_sel;

	macaddr = &(data->mac_addrs[index]);

	pool_sel = dev->data->mac_pool_sel[index];

	for (i = 0; i < sizeof(pool_sel) * CHAR_BIT; i++) {
		if (pool_sel & (1ULL << i)) {
			if (i == 0)
				vsi = pf->main_vsi;
			else {
				/* No VMDQ pool enabled or configured */
				if (!(pf->flags | I40E_FLAG_VMDQ) ||
					(i > pf->nb_cfg_vmdq_vsi)) {
					PMD_DRV_LOG(ERR, "No VMDQ pool enabled"
							"/configured");
					return;
				}
				vsi = pf->vmdq[i - 1].vsi;
			}
			ret = i40e_vsi_delete_mac(vsi, macaddr);

			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to remove MACVLAN filter");
				return;
			}
		}
	}
}

/* Set perfect match or hash match of MAC and VLAN for a VF */
static int
i40e_vf_mac_filter_set(struct i40e_pf *pf,
		 struct rte_eth_mac_filter *filter,
		 bool add)
{
	struct i40e_hw *hw;
	struct i40e_mac_filter_info mac_filter;
	struct ether_addr old_mac;
	struct ether_addr *new_mac;
	struct i40e_pf_vf *vf = NULL;
	uint16_t vf_id;
	int ret;

	if (pf == NULL) {
		PMD_DRV_LOG(ERR, "Invalid PF argument.");
		return -EINVAL;
	}
	hw = I40E_PF_TO_HW(pf);

	if (filter == NULL) {
		PMD_DRV_LOG(ERR, "Invalid mac filter argument.");
		return -EINVAL;
	}

	new_mac = &filter->mac_addr;

	if (is_zero_ether_addr(new_mac)) {
		PMD_DRV_LOG(ERR, "Invalid ethernet address.");
		return -EINVAL;
	}

	vf_id = filter->dst_id;

	if (vf_id > pf->vf_num - 1 || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}
	vf = &pf->vfs[vf_id];

	if (add && is_same_ether_addr(new_mac, &(pf->dev_addr))) {
		PMD_DRV_LOG(INFO, "Ignore adding permanent MAC address.");
		return -EINVAL;
	}

	if (add) {
		(void)rte_memcpy(&old_mac, hw->mac.addr, ETHER_ADDR_LEN);
		(void)rte_memcpy(hw->mac.addr, new_mac->addr_bytes,
				ETHER_ADDR_LEN);
		(void)rte_memcpy(&mac_filter.mac_addr, &filter->mac_addr,
				 ETHER_ADDR_LEN);

		mac_filter.filter_type = filter->filter_type;
		ret = i40e_vsi_add_mac(vf->vsi, &mac_filter);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to add MAC filter.");
			return -1;
		}
		ether_addr_copy(new_mac, &pf->dev_addr);
	} else {
		(void)rte_memcpy(hw->mac.addr, hw->mac.perm_addr,
				ETHER_ADDR_LEN);
		ret = i40e_vsi_delete_mac(vf->vsi, &filter->mac_addr);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to delete MAC filter.");
			return -1;
		}

		/* Clear device address as it has been removed */
		if (is_same_ether_addr(&(pf->dev_addr), new_mac))
			memset(&pf->dev_addr, 0, sizeof(struct ether_addr));
	}

	return 0;
}

/* MAC filter handle */
static int
i40e_mac_filter_handle(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
		void *arg)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_mac_filter *filter;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	int ret = I40E_NOT_SUPPORTED;

	filter = (struct rte_eth_mac_filter *)(arg);

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		ret = I40E_SUCCESS;
		break;
	case RTE_ETH_FILTER_ADD:
		i40e_pf_disable_irq0(hw);
		if (filter->is_vf)
			ret = i40e_vf_mac_filter_set(pf, filter, 1);
		i40e_pf_enable_irq0(hw);
		break;
	case RTE_ETH_FILTER_DELETE:
		i40e_pf_disable_irq0(hw);
		if (filter->is_vf)
			ret = i40e_vf_mac_filter_set(pf, filter, 0);
		i40e_pf_enable_irq0(hw);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = I40E_ERR_PARAM;
		break;
	}

	return ret;
}

static int
i40e_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t lut, l;
	uint16_t i, j, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t mask;

	if (reta_size != lut_size ||
		reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
					"(%d)\n", reta_size, lut_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += I40E_4_BIT_WIDTH) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
						I40E_4_BIT_MASK);
		if (!mask)
			continue;
		if (mask == I40E_4_BIT_MASK)
			l = 0;
		else
			l = I40E_READ_REG(hw, I40E_PFQF_HLUT(i >> 2));
		for (j = 0, lut = 0; j < I40E_4_BIT_WIDTH; j++) {
			if (mask & (0x1 << j))
				lut |= reta_conf[idx].reta[shift + j] <<
							(CHAR_BIT * j);
			else
				lut |= l & (I40E_8_BIT_MASK << (CHAR_BIT * j));
		}
		I40E_WRITE_REG(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}

	return 0;
}

static int
i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t lut;
	uint16_t i, j, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t mask;

	if (reta_size != lut_size ||
		reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
					"(%d)\n", reta_size, lut_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += I40E_4_BIT_WIDTH) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
						I40E_4_BIT_MASK);
		if (!mask)
			continue;

		lut = I40E_READ_REG(hw, I40E_PFQF_HLUT(i >> 2));
		for (j = 0; j < I40E_4_BIT_WIDTH; j++) {
			if (mask & (0x1 << j))
				reta_conf[idx].reta[shift + j] = ((lut >>
					(CHAR_BIT * j)) & I40E_8_BIT_MASK);
		}
	}

	return 0;
}

/**
 * i40e_allocate_dma_mem_d - specific memory alloc for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to fill out
 * @size: size of memory requested
 * @alignment: what to align the allocation to
 **/
enum i40e_status_code
i40e_allocate_dma_mem_d(__attribute__((unused)) struct i40e_hw *hw,
			struct i40e_dma_mem *mem,
			u64 size,
			u32 alignment)
{
	static uint64_t id = 0;
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return I40E_ERR_PARAM;

	id++;
	snprintf(z_name, sizeof(z_name), "i40e_dma_%"PRIu64, id);
#ifdef RTE_LIBRTE_XEN_DOM0
	mz = rte_memzone_reserve_bounded(z_name, size, 0, 0, alignment,
							RTE_PGSIZE_2M);
#else
	mz = rte_memzone_reserve_aligned(z_name, size, 0, 0, alignment);
#endif
	if (!mz)
		return I40E_ERR_NO_MEMORY;

	mem->id = id;
	mem->size = size;
	mem->va = mz->addr;
#ifdef RTE_LIBRTE_XEN_DOM0
	mem->pa = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);
#else
	mem->pa = mz->phys_addr;
#endif

	return I40E_SUCCESS;
}

/**
 * i40e_free_dma_mem_d - specific memory free for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  ptr to mem struct to free
 **/
enum i40e_status_code
i40e_free_dma_mem_d(__attribute__((unused)) struct i40e_hw *hw,
		    struct i40e_dma_mem *mem)
{
	if (!mem || !mem->va)
		return I40E_ERR_PARAM;

	mem->va = NULL;
	mem->pa = (u64)0;

	return I40E_SUCCESS;
}

/**
 * i40e_allocate_virt_mem_d - specific memory alloc for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to fill out
 * @size: size of memory requested
 **/
enum i40e_status_code
i40e_allocate_virt_mem_d(__attribute__((unused)) struct i40e_hw *hw,
			 struct i40e_virt_mem *mem,
			 u32 size)
{
	if (!mem)
		return I40E_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("i40e", size, 0);

	if (mem->va)
		return I40E_SUCCESS;
	else
		return I40E_ERR_NO_MEMORY;
}

/**
 * i40e_free_virt_mem_d - specific memory free for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to free
 **/
enum i40e_status_code
i40e_free_virt_mem_d(__attribute__((unused)) struct i40e_hw *hw,
		     struct i40e_virt_mem *mem)
{
	if (!mem)
		return I40E_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return I40E_SUCCESS;
}

void
i40e_init_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_init(&sp->spinlock);
}

void
i40e_acquire_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_lock(&sp->spinlock);
}

void
i40e_release_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_unlock(&sp->spinlock);
}

void
i40e_destroy_spinlock_d(__attribute__((unused)) struct i40e_spinlock *sp)
{
	return;
}

/**
 * Get the hardware capabilities, which will be parsed
 * and saved into struct i40e_hw.
 */
static int
i40e_get_cap(struct i40e_hw *hw)
{
	struct i40e_aqc_list_capabilities_element_resp *buf;
	uint16_t len, size = 0;
	int ret;

	/* Calculate a huge enough buff for saving response data temporarily */
	len = sizeof(struct i40e_aqc_list_capabilities_element_resp) *
						I40E_MAX_CAP_ELE_NUM;
	buf = rte_zmalloc("i40e", len, 0);
	if (!buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	/* Get, parse the capabilities and save it to hw */
	ret = i40e_aq_discover_capabilities(hw, buf, len, &size,
			i40e_aqc_opc_list_func_capabilities, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to discover capabilities");

	/* Free the temporary buffer after being used */
	rte_free(buf);

	return ret;
}

static int
i40e_pf_parameter_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t sum_queues = 0, sum_vsis, left_queues;

	/* First check if FW support SRIOV */
	if (dev->pci_dev->max_vfs && !hw->func_caps.sr_iov_1_1) {
		PMD_INIT_LOG(ERR, "HW configuration doesn't support SRIOV");
		return -EINVAL;
	}

	pf->flags = I40E_FLAG_HEADER_SPLIT_DISABLED;
	pf->max_num_vsi = RTE_MIN(hw->func_caps.num_vsis, I40E_MAX_NUM_VSIS);
	PMD_INIT_LOG(INFO, "Max supported VSIs:%u", pf->max_num_vsi);
	/* Allocate queues for pf */
	if (hw->func_caps.rss) {
		pf->flags |= I40E_FLAG_RSS;
		pf->lan_nb_qps = RTE_MIN(hw->func_caps.num_tx_qp,
			(uint32_t)(1 << hw->func_caps.rss_table_entry_width));
		pf->lan_nb_qps = i40e_align_floor(pf->lan_nb_qps);
	} else
		pf->lan_nb_qps = 1;
	sum_queues = pf->lan_nb_qps;
	/* Default VSI is not counted in */
	sum_vsis = 0;
	PMD_INIT_LOG(INFO, "PF queue pairs:%u", pf->lan_nb_qps);

	if (hw->func_caps.sr_iov_1_1 && dev->pci_dev->max_vfs) {
		pf->flags |= I40E_FLAG_SRIOV;
		pf->vf_nb_qps = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VF;
		if (dev->pci_dev->max_vfs > hw->func_caps.num_vfs) {
			PMD_INIT_LOG(ERR, "Config VF number %u, "
				     "max supported %u.",
				     dev->pci_dev->max_vfs,
				     hw->func_caps.num_vfs);
			return -EINVAL;
		}
		if (pf->vf_nb_qps > I40E_MAX_QP_NUM_PER_VF) {
			PMD_INIT_LOG(ERR, "FVL VF queue %u, "
				     "max support %u queues.",
				     pf->vf_nb_qps, I40E_MAX_QP_NUM_PER_VF);
			return -EINVAL;
		}
		pf->vf_num = dev->pci_dev->max_vfs;
		sum_queues += pf->vf_nb_qps * pf->vf_num;
		sum_vsis   += pf->vf_num;
		PMD_INIT_LOG(INFO, "Max VF num:%u each has queue pairs:%u",
			     pf->vf_num, pf->vf_nb_qps);
	} else
		pf->vf_num = 0;

	if (hw->func_caps.vmdq) {
		pf->flags |= I40E_FLAG_VMDQ;
		pf->vmdq_nb_qps = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
		pf->max_nb_vmdq_vsi = 1;
		/*
		 * If VMDQ available, assume a single VSI can be created.  Will adjust
		 * later.
		 */
		sum_queues += pf->vmdq_nb_qps * pf->max_nb_vmdq_vsi;
		sum_vsis += pf->max_nb_vmdq_vsi;
	} else {
		pf->vmdq_nb_qps = 0;
		pf->max_nb_vmdq_vsi = 0;
	}
	pf->nb_cfg_vmdq_vsi = 0;

	if (hw->func_caps.fd) {
		pf->flags |= I40E_FLAG_FDIR;
		pf->fdir_nb_qps = I40E_DEFAULT_QP_NUM_FDIR;
		/**
		 * Each flow director consumes one VSI and one queue,
		 * but can't calculate out predictably here.
		 */
	}

	if (sum_vsis > pf->max_num_vsi ||
		sum_queues > hw->func_caps.num_rx_qp) {
		PMD_INIT_LOG(ERR, "VSI/QUEUE setting can't be satisfied");
		PMD_INIT_LOG(ERR, "Max VSIs: %u, asked:%u",
			     pf->max_num_vsi, sum_vsis);
		PMD_INIT_LOG(ERR, "Total queue pairs:%u, asked:%u",
			     hw->func_caps.num_rx_qp, sum_queues);
		return -EINVAL;
	}

	/* Adjust VMDQ setting to support as many VMs as possible */
	if (pf->flags & I40E_FLAG_VMDQ) {
		left_queues = hw->func_caps.num_rx_qp - sum_queues;

		pf->max_nb_vmdq_vsi += RTE_MIN(left_queues / pf->vmdq_nb_qps,
					pf->max_num_vsi - sum_vsis);

		/* Limit the max VMDQ number that rte_ether that can support  */
		pf->max_nb_vmdq_vsi = RTE_MIN(pf->max_nb_vmdq_vsi,
					ETH_64_POOLS - 1);

		PMD_INIT_LOG(INFO, "Max VMDQ VSI num:%u",
				pf->max_nb_vmdq_vsi);
		PMD_INIT_LOG(INFO, "VMDQ queue pairs:%u", pf->vmdq_nb_qps);
	}

	/* Each VSI occupy 1 MSIX interrupt at least, plus IRQ0 for misc intr
	 * cause */
	if (sum_vsis > hw->func_caps.num_msix_vectors - 1) {
		PMD_INIT_LOG(ERR, "Too many VSIs(%u), MSIX intr(%u) not enough",
			     sum_vsis, hw->func_caps.num_msix_vectors);
		return -EINVAL;
	}
	return I40E_SUCCESS;
}

static int
i40e_pf_get_switch_config(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_aqc_get_switch_config_resp *switch_config;
	struct i40e_aqc_switch_config_element_resp *element;
	uint16_t start_seid = 0, num_reported;
	int ret;

	switch_config = (struct i40e_aqc_get_switch_config_resp *)\
			rte_zmalloc("i40e", I40E_AQ_LARGE_BUF, 0);
	if (!switch_config) {
		PMD_DRV_LOG(ERR, "Failed to allocated memory");
		return -ENOMEM;
	}

	/* Get the switch configurations */
	ret = i40e_aq_get_switch_config(hw, switch_config,
		I40E_AQ_LARGE_BUF, &start_seid, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get switch configurations");
		goto fail;
	}
	num_reported = rte_le_to_cpu_16(switch_config->header.num_reported);
	if (num_reported != 1) { /* The number should be 1 */
		PMD_DRV_LOG(ERR, "Wrong number of switch config reported");
		goto fail;
	}

	/* Parse the switch configuration elements */
	element = &(switch_config->element[0]);
	if (element->element_type == I40E_SWITCH_ELEMENT_TYPE_VSI) {
		pf->mac_seid = rte_le_to_cpu_16(element->uplink_seid);
		pf->main_vsi_seid = rte_le_to_cpu_16(element->seid);
	} else
		PMD_DRV_LOG(INFO, "Unknown element type");

fail:
	rte_free(switch_config);

	return ret;
}

static int
i40e_res_pool_init (struct i40e_res_pool_info *pool, uint32_t base,
			uint32_t num)
{
	struct pool_entry *entry;

	if (pool == NULL || num == 0)
		return -EINVAL;

	entry = rte_zmalloc("i40e", sizeof(*entry), 0);
	if (entry == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for resource pool");
		return -ENOMEM;
	}

	/* queue heap initialize */
	pool->num_free = num;
	pool->num_alloc = 0;
	pool->base = base;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);

	/* Initialize element  */
	entry->base = 0;
	entry->len = num;

	LIST_INSERT_HEAD(&pool->free_list, entry, next);
	return 0;
}

static void
i40e_res_pool_destroy(struct i40e_res_pool_info *pool)
{
	struct pool_entry *entry;

	if (pool == NULL)
		return;

	LIST_FOREACH(entry, &pool->alloc_list, next) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	LIST_FOREACH(entry, &pool->free_list, next) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	pool->num_free = 0;
	pool->num_alloc = 0;
	pool->base = 0;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);
}

static int
i40e_res_pool_free(struct i40e_res_pool_info *pool,
		       uint32_t base)
{
	struct pool_entry *entry, *next, *prev, *valid_entry = NULL;
	uint32_t pool_offset;
	int insert;

	if (pool == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	pool_offset = base - pool->base;
	/* Lookup in alloc list */
	LIST_FOREACH(entry, &pool->alloc_list, next) {
		if (entry->base == pool_offset) {
			valid_entry = entry;
			LIST_REMOVE(entry, next);
			break;
		}
	}

	/* Not find, return */
	if (valid_entry == NULL) {
		PMD_DRV_LOG(ERR, "Failed to find entry");
		return -EINVAL;
	}

	/**
	 * Found it, move it to free list  and try to merge.
	 * In order to make merge easier, always sort it by qbase.
	 * Find adjacent prev and last entries.
	 */
	prev = next = NULL;
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->base > valid_entry->base) {
			next = entry;
			break;
		}
		prev = entry;
	}

	insert = 0;
	/* Try to merge with next one*/
	if (next != NULL) {
		/* Merge with next one */
		if (valid_entry->base + valid_entry->len == next->base) {
			next->base = valid_entry->base;
			next->len += valid_entry->len;
			rte_free(valid_entry);
			valid_entry = next;
			insert = 1;
		}
	}

	if (prev != NULL) {
		/* Merge with previous one */
		if (prev->base + prev->len == valid_entry->base) {
			prev->len += valid_entry->len;
			/* If it merge with next one, remove next node */
			if (insert == 1) {
				LIST_REMOVE(valid_entry, next);
				rte_free(valid_entry);
			} else {
				rte_free(valid_entry);
				insert = 1;
			}
		}
	}

	/* Not find any entry to merge, insert */
	if (insert == 0) {
		if (prev != NULL)
			LIST_INSERT_AFTER(prev, valid_entry, next);
		else if (next != NULL)
			LIST_INSERT_BEFORE(next, valid_entry, next);
		else /* It's empty list, insert to head */
			LIST_INSERT_HEAD(&pool->free_list, valid_entry, next);
	}

	pool->num_free += valid_entry->len;
	pool->num_alloc -= valid_entry->len;

	return 0;
}

static int
i40e_res_pool_alloc(struct i40e_res_pool_info *pool,
		       uint16_t num)
{
	struct pool_entry *entry, *valid_entry;

	if (pool == NULL || num == 0) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (pool->num_free < num) {
		PMD_DRV_LOG(ERR, "No resource. ask:%u, available:%u",
			    num, pool->num_free);
		return -ENOMEM;
	}

	valid_entry = NULL;
	/* Lookup  in free list and find most fit one */
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->len >= num) {
			/* Find best one */
			if (entry->len == num) {
				valid_entry = entry;
				break;
			}
			if (valid_entry == NULL || valid_entry->len > entry->len)
				valid_entry = entry;
		}
	}

	/* Not find one to satisfy the request, return */
	if (valid_entry == NULL) {
		PMD_DRV_LOG(ERR, "No valid entry found");
		return -ENOMEM;
	}
	/**
	 * The entry have equal queue number as requested,
	 * remove it from alloc_list.
	 */
	if (valid_entry->len == num) {
		LIST_REMOVE(valid_entry, next);
	} else {
		/**
		 * The entry have more numbers than requested,
		 * create a new entry for alloc_list and minus its
		 * queue base and number in free_list.
		 */
		entry = rte_zmalloc("res_pool", sizeof(*entry), 0);
		if (entry == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate memory for "
				    "resource pool");
			return -ENOMEM;
		}
		entry->base = valid_entry->base;
		entry->len = num;
		valid_entry->base += num;
		valid_entry->len -= num;
		valid_entry = entry;
	}

	/* Insert it into alloc list, not sorted */
	LIST_INSERT_HEAD(&pool->alloc_list, valid_entry, next);

	pool->num_free -= valid_entry->len;
	pool->num_alloc += valid_entry->len;

	return (valid_entry->base + pool->base);
}

/**
 * bitmap_is_subset - Check whether src2 is subset of src1
 **/
static inline int
bitmap_is_subset(uint8_t src1, uint8_t src2)
{
	return !((src1 ^ src2) & src2);
}

static int
validate_tcmap_parameter(struct i40e_vsi *vsi, uint8_t enabled_tcmap)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	/* If DCB is not supported, only default TC is supported */
	if (!hw->func_caps.dcb && enabled_tcmap != I40E_DEFAULT_TCMAP) {
		PMD_DRV_LOG(ERR, "DCB is not enabled, only TC0 is supported");
		return -EINVAL;
	}

	if (!bitmap_is_subset(hw->func_caps.enabled_tcmap, enabled_tcmap)) {
		PMD_DRV_LOG(ERR, "Enabled TC map 0x%x not applicable to "
			    "HW support 0x%x", hw->func_caps.enabled_tcmap,
			    enabled_tcmap);
		return -EINVAL;
	}
	return I40E_SUCCESS;
}

int
i40e_vsi_vlan_pvid_set(struct i40e_vsi *vsi,
				struct i40e_vsi_vlan_pvid_info *info)
{
	struct i40e_hw *hw;
	struct i40e_vsi_context ctxt;
	uint8_t vlan_flags = 0;
	int ret;

	if (vsi == NULL || info == NULL) {
		PMD_DRV_LOG(ERR, "invalid parameters");
		return I40E_ERR_PARAM;
	}

	if (info->on) {
		vsi->info.pvid = info->config.pvid;
		/**
		 * If insert pvid is enabled, only tagged pkts are
		 * allowed to be sent out.
		 */
		vlan_flags |= I40E_AQ_VSI_PVLAN_INSERT_PVID |
				I40E_AQ_VSI_PVLAN_MODE_TAGGED;
	} else {
		vsi->info.pvid = 0;
		if (info->config.reject.tagged == 0)
			vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_TAGGED;

		if (info->config.reject.untagged == 0)
			vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_UNTAGGED;
	}
	vsi->info.port_vlan_flags &= ~(I40E_AQ_VSI_PVLAN_INSERT_PVID |
					I40E_AQ_VSI_PVLAN_MODE_MASK);
	vsi->info.port_vlan_flags |= vlan_flags;
	vsi->info.valid_sections =
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
	memset(&ctxt, 0, sizeof(ctxt));
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to update VSI params");

	return ret;
}

static int
i40e_vsi_update_tc_bandwidth(struct i40e_vsi *vsi, uint8_t enabled_tcmap)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int i, ret;
	struct i40e_aqc_configure_vsi_tc_bw_data tc_bw_data;

	ret = validate_tcmap_parameter(vsi, enabled_tcmap);
	if (ret != I40E_SUCCESS)
		return ret;

	if (!vsi->seid) {
		PMD_DRV_LOG(ERR, "seid not valid");
		return -EINVAL;
	}

	memset(&tc_bw_data, 0, sizeof(tc_bw_data));
	tc_bw_data.tc_valid_bits = enabled_tcmap;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		tc_bw_data.tc_bw_credits[i] =
			(enabled_tcmap & (1 << i)) ? 1 : 0;

	ret = i40e_aq_config_vsi_tc_bw(hw, vsi->seid, &tc_bw_data, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to configure TC BW");
		return ret;
	}

	(void)rte_memcpy(vsi->info.qs_handle, tc_bw_data.qs_handles,
					sizeof(vsi->info.qs_handle));
	return I40E_SUCCESS;
}

static int
i40e_vsi_config_tc_queue_mapping(struct i40e_vsi *vsi,
				 struct i40e_aqc_vsi_properties_data *info,
				 uint8_t enabled_tcmap)
{
	int ret, total_tc = 0, i;
	uint16_t qpnum_per_tc, bsf, qp_idx;

	ret = validate_tcmap_parameter(vsi, enabled_tcmap);
	if (ret != I40E_SUCCESS)
		return ret;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		if (enabled_tcmap & (1 << i))
			total_tc++;
	vsi->enabled_tc = enabled_tcmap;

	/* Number of queues per enabled TC */
	qpnum_per_tc = i40e_align_floor(vsi->nb_qps / total_tc);
	qpnum_per_tc = RTE_MIN(qpnum_per_tc, I40E_MAX_Q_PER_TC);
	bsf = rte_bsf32(qpnum_per_tc);

	/* Adjust the queue number to actual queues that can be applied */
	vsi->nb_qps = qpnum_per_tc * total_tc;

	/**
	 * Configure TC and queue mapping parameters, for enabled TC,
	 * allocate qpnum_per_tc queues to this traffic. For disabled TC,
	 * default queue will serve it.
	 */
	qp_idx = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & (1 << i)) {
			info->tc_mapping[i] = rte_cpu_to_le_16((qp_idx <<
					I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
				(bsf << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT));
			qp_idx += qpnum_per_tc;
		} else
			info->tc_mapping[i] = 0;
	}

	/* Associate queue number with VSI */
	if (vsi->type == I40E_VSI_SRIOV) {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_NONCONTIG);
		for (i = 0; i < vsi->nb_qps; i++)
			info->queue_mapping[i] =
				rte_cpu_to_le_16(vsi->base_queue + i);
	} else {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_CONTIG);
		info->queue_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	}
	info->valid_sections |=
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);

	return I40E_SUCCESS;
}

static int
i40e_veb_release(struct i40e_veb *veb)
{
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;

	if (veb == NULL || veb->associate_vsi == NULL)
		return -EINVAL;

	if (!TAILQ_EMPTY(&veb->head)) {
		PMD_DRV_LOG(ERR, "VEB still has VSI attached, can't remove");
		return -EACCES;
	}

	vsi = veb->associate_vsi;
	hw = I40E_VSI_TO_HW(vsi);

	vsi->uplink_seid = veb->uplink_seid;
	i40e_aq_delete_element(hw, veb->seid, NULL);
	rte_free(veb);
	vsi->veb = NULL;
	return I40E_SUCCESS;
}

/* Setup a veb */
static struct i40e_veb *
i40e_veb_setup(struct i40e_pf *pf, struct i40e_vsi *vsi)
{
	struct i40e_veb *veb;
	int ret;
	struct i40e_hw *hw;

	if (NULL == pf || vsi == NULL) {
		PMD_DRV_LOG(ERR, "veb setup failed, "
			    "associated VSI shouldn't null");
		return NULL;
	}
	hw = I40E_PF_TO_HW(pf);

	veb = rte_zmalloc("i40e_veb", sizeof(struct i40e_veb), 0);
	if (!veb) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for veb");
		goto fail;
	}

	veb->associate_vsi = vsi;
	TAILQ_INIT(&veb->head);
	veb->uplink_seid = vsi->uplink_seid;

	ret = i40e_aq_add_veb(hw, veb->uplink_seid, vsi->seid,
		I40E_DEFAULT_TCMAP, false, false, &veb->seid, NULL);

	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Add veb failed, aq_err: %d",
			    hw->aq.asq_last_status);
		goto fail;
	}

	/* get statistics index */
	ret = i40e_aq_get_veb_parameters(hw, veb->seid, NULL, NULL,
				&veb->stats_idx, NULL, NULL, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Get veb statics index failed, aq_err: %d",
			    hw->aq.asq_last_status);
		goto fail;
	}

	/* Get VEB bandwidth, to be implemented */
	/* Now associated vsi binding to the VEB, set uplink to this VEB */
	vsi->uplink_seid = veb->seid;

	return veb;
fail:
	rte_free(veb);
	return NULL;
}

int
i40e_vsi_release(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi_list *vsi_list;
	int ret;
	struct i40e_mac_filter *f;

	if (!vsi)
		return I40E_SUCCESS;

	pf = I40E_VSI_TO_PF(vsi);
	hw = I40E_VSI_TO_HW(vsi);

	/* VSI has child to attach, release child first */
	if (vsi->veb) {
		TAILQ_FOREACH(vsi_list, &vsi->veb->head, list) {
			if (i40e_vsi_release(vsi_list->vsi) != I40E_SUCCESS)
				return -1;
			TAILQ_REMOVE(&vsi->veb->head, vsi_list, list);
		}
		i40e_veb_release(vsi->veb);
	}

	/* Remove all macvlan filters of the VSI */
	i40e_vsi_remove_all_macvlan_filter(vsi);
	TAILQ_FOREACH(f, &vsi->mac_list, next)
		rte_free(f);

	if (vsi->type != I40E_VSI_MAIN) {
		/* Remove vsi from parent's sibling list */
		if (vsi->parent_vsi == NULL || vsi->parent_vsi->veb == NULL) {
			PMD_DRV_LOG(ERR, "VSI's parent VSI is NULL");
			return I40E_ERR_PARAM;
		}
		TAILQ_REMOVE(&vsi->parent_vsi->veb->head,
				&vsi->sib_vsi_list, list);

		/* Remove all switch element of the VSI */
		ret = i40e_aq_delete_element(hw, vsi->seid, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR, "Failed to delete element");
	}
	i40e_res_pool_free(&pf->qp_pool, vsi->base_queue);

	if (vsi->type != I40E_VSI_SRIOV)
		i40e_res_pool_free(&pf->msix_pool, vsi->msix_intr);
	rte_free(vsi);

	return I40E_SUCCESS;
}

static int
i40e_update_default_filter_setting(struct i40e_vsi *vsi)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_remove_macvlan_element_data def_filter;
	struct i40e_mac_filter_info filter;
	int ret;

	if (vsi->type != I40E_VSI_MAIN)
		return I40E_ERR_CONFIG;
	memset(&def_filter, 0, sizeof(def_filter));
	(void)rte_memcpy(def_filter.mac_addr, hw->mac.perm_addr,
					ETH_ADDR_LEN);
	def_filter.vlan_tag = 0;
	def_filter.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
				I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
	ret = i40e_aq_remove_macvlan(hw, vsi->seid, &def_filter, 1, NULL);
	if (ret != I40E_SUCCESS) {
		struct i40e_mac_filter *f;
		struct ether_addr *mac;

		PMD_DRV_LOG(WARNING, "Cannot remove the default "
			    "macvlan filter");
		/* It needs to add the permanent mac into mac list */
		f = rte_zmalloc("macv_filter", sizeof(*f), 0);
		if (f == NULL) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}
		mac = &f->mac_info.mac_addr;
		(void)rte_memcpy(&mac->addr_bytes, hw->mac.perm_addr,
				ETH_ADDR_LEN);
		f->mac_info.filter_type = RTE_MACVLAN_PERFECT_MATCH;
		TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
		vsi->mac_num++;

		return ret;
	}
	(void)rte_memcpy(&filter.mac_addr,
		(struct ether_addr *)(hw->mac.perm_addr), ETH_ADDR_LEN);
	filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
	return i40e_vsi_add_mac(vsi, &filter);
}

static int
i40e_vsi_dump_bw_config(struct i40e_vsi *vsi)
{
	struct i40e_aqc_query_vsi_bw_config_resp bw_config;
	struct i40e_aqc_query_vsi_ets_sla_config_resp ets_sla_config;
	struct i40e_hw *hw = &vsi->adapter->hw;
	i40e_status ret;
	int i;

	memset(&bw_config, 0, sizeof(bw_config));
	ret = i40e_aq_query_vsi_bw_config(hw, vsi->seid, &bw_config, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "VSI failed to get bandwidth configuration %u",
			    hw->aq.asq_last_status);
		return ret;
	}

	memset(&ets_sla_config, 0, sizeof(ets_sla_config));
	ret = i40e_aq_query_vsi_ets_sla_config(hw, vsi->seid,
					&ets_sla_config, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "VSI failed to get TC bandwdith "
			    "configuration %u", hw->aq.asq_last_status);
		return ret;
	}

	/* Not store the info yet, just print out */
	PMD_DRV_LOG(INFO, "VSI bw limit:%u", bw_config.port_bw_limit);
	PMD_DRV_LOG(INFO, "VSI max_bw:%u", bw_config.max_bw);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		PMD_DRV_LOG(INFO, "\tVSI TC%u:share credits %u", i,
			    ets_sla_config.share_credits[i]);
		PMD_DRV_LOG(INFO, "\tVSI TC%u:credits %u", i,
			    rte_le_to_cpu_16(ets_sla_config.credits[i]));
		PMD_DRV_LOG(INFO, "\tVSI TC%u: max credits: %u", i,
			    rte_le_to_cpu_16(ets_sla_config.credits[i / 4]) >>
			    (i * 4));
	}

	return 0;
}

/* Setup a VSI */
struct i40e_vsi *
i40e_vsi_setup(struct i40e_pf *pf,
	       enum i40e_vsi_type type,
	       struct i40e_vsi *uplink_vsi,
	       uint16_t user_param)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	struct i40e_mac_filter_info filter;
	int ret;
	struct i40e_vsi_context ctxt;
	struct ether_addr broadcast =
		{.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

	if (type != I40E_VSI_MAIN && uplink_vsi == NULL) {
		PMD_DRV_LOG(ERR, "VSI setup failed, "
			    "VSI link shouldn't be NULL");
		return NULL;
	}

	if (type == I40E_VSI_MAIN && uplink_vsi != NULL) {
		PMD_DRV_LOG(ERR, "VSI setup failed, MAIN VSI "
			    "uplink VSI should be NULL");
		return NULL;
	}

	/* If uplink vsi didn't setup VEB, create one first */
	if (type != I40E_VSI_MAIN && uplink_vsi->veb == NULL) {
		uplink_vsi->veb = i40e_veb_setup(pf, uplink_vsi);

		if (NULL == uplink_vsi->veb) {
			PMD_DRV_LOG(ERR, "VEB setup failed");
			return NULL;
		}
	}

	vsi = rte_zmalloc("i40e_vsi", sizeof(struct i40e_vsi), 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for vsi");
		return NULL;
	}
	TAILQ_INIT(&vsi->mac_list);
	vsi->type = type;
	vsi->adapter = I40E_PF_TO_ADAPTER(pf);
	vsi->max_macaddrs = I40E_NUM_MACADDR_MAX;
	vsi->parent_vsi = uplink_vsi;
	vsi->user_param = user_param;
	/* Allocate queues */
	switch (vsi->type) {
	case I40E_VSI_MAIN  :
		vsi->nb_qps = pf->lan_nb_qps;
		break;
	case I40E_VSI_SRIOV :
		vsi->nb_qps = pf->vf_nb_qps;
		break;
	case I40E_VSI_VMDQ2:
		vsi->nb_qps = pf->vmdq_nb_qps;
		break;
	case I40E_VSI_FDIR:
		vsi->nb_qps = pf->fdir_nb_qps;
		break;
	default:
		goto fail_mem;
	}
	/*
	 * The filter status descriptor is reported in rx queue 0,
	 * while the tx queue for fdir filter programming has no
	 * such constraints, can be non-zero queues.
	 * To simplify it, choose FDIR vsi use queue 0 pair.
	 * To make sure it will use queue 0 pair, queue allocation
	 * need be done before this function is called
	 */
	if (type != I40E_VSI_FDIR) {
		ret = i40e_res_pool_alloc(&pf->qp_pool, vsi->nb_qps);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "VSI %d allocate queue failed %d",
						vsi->seid, ret);
				goto fail_mem;
			}
			vsi->base_queue = ret;
	} else
		vsi->base_queue = I40E_FDIR_QUEUE_ID;

	/* VF has MSIX interrupt in VF range, don't allocate here */
	if (type != I40E_VSI_SRIOV) {
		ret = i40e_res_pool_alloc(&pf->msix_pool, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "VSI %d get heap failed %d", vsi->seid, ret);
			goto fail_queue_alloc;
		}
		vsi->msix_intr = ret;
	} else
		vsi->msix_intr = 0;
	/* Add VSI */
	if (type == I40E_VSI_MAIN) {
		/* For main VSI, no need to add since it's default one */
		vsi->uplink_seid = pf->mac_seid;
		vsi->seid = pf->main_vsi_seid;
		/* Bind queues with specific MSIX interrupt */
		/**
		 * Needs 2 interrupt at least, one for misc cause which will
		 * enabled from OS side, Another for queues binding the
		 * interrupt from device side only.
		 */

		/* Get default VSI parameters from hardware */
		memset(&ctxt, 0, sizeof(ctxt));
		ctxt.seid = vsi->seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.vf_num = 0;
		ret = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to get VSI params");
			goto fail_msix_alloc;
		}
		(void)rte_memcpy(&vsi->info, &ctxt.info,
			sizeof(struct i40e_aqc_vsi_properties_data));
		vsi->vsi_id = ctxt.vsi_number;
		vsi->info.valid_sections = 0;

		/* Configure tc, enabled TC0 only */
		if (i40e_vsi_update_tc_bandwidth(vsi, I40E_DEFAULT_TCMAP) !=
			I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update TC bandwidth");
			goto fail_msix_alloc;
		}

		/* TC, queue mapping */
		memset(&ctxt, 0, sizeof(ctxt));
		vsi->info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		vsi->info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
					I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
		(void)rte_memcpy(&ctxt.info, &vsi->info,
			sizeof(struct i40e_aqc_vsi_properties_data));
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to configure "
				    "TC queue mapping");
			goto fail_msix_alloc;
		}
		ctxt.seid = vsi->seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.vf_num = 0;

		/* Update VSI parameters */
		ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update VSI params");
			goto fail_msix_alloc;
		}

		(void)rte_memcpy(&vsi->info.tc_mapping, &ctxt.info.tc_mapping,
						sizeof(vsi->info.tc_mapping));
		(void)rte_memcpy(&vsi->info.queue_mapping,
				&ctxt.info.queue_mapping,
			sizeof(vsi->info.queue_mapping));
		vsi->info.mapping_flags = ctxt.info.mapping_flags;
		vsi->info.valid_sections = 0;

		(void)rte_memcpy(pf->dev_addr.addr_bytes, hw->mac.perm_addr,
				ETH_ADDR_LEN);

		/**
		 * Updating default filter settings are necessary to prevent
		 * reception of tagged packets.
		 * Some old firmware configurations load a default macvlan
		 * filter which accepts both tagged and untagged packets.
		 * The updating is to use a normal filter instead if needed.
		 * For NVM 4.2.2 or after, the updating is not needed anymore.
		 * The firmware with correct configurations load the default
		 * macvlan filter which is expected and cannot be removed.
		 */
		i40e_update_default_filter_setting(vsi);
		i40e_config_qinq(hw, vsi);
	} else if (type == I40E_VSI_SRIOV) {
		memset(&ctxt, 0, sizeof(ctxt));
		/**
		 * For other VSI, the uplink_seid equals to uplink VSI's
		 * uplink_seid since they share same VEB
		 */
		vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = hw->func_caps.vf_base_id + user_param;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;
		ctxt.flags = I40E_AQ_VSI_TYPE_VF;

		/**
		 * Do not configure switch ID to enable VEB switch by
		 * I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB. Because in Fortville,
		 * if the source mac address of packet sent from VF is not
		 * listed in the VEB's mac table, the VEB will switch the
		 * packet back to the VF. Need to enable it when HW issue
		 * is fixed.
		 */

		/* Configure port/vlan */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to configure "
				    "TC queue mapping");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
		/**
		 * Since VSI is not created yet, only configure parameter,
		 * will add vsi below.
		 */

		i40e_config_qinq(hw, vsi);
	} else if (type == I40E_VSI_VMDQ2) {
		memset(&ctxt, 0, sizeof(ctxt));
		/*
		 * For other VSI, the uplink_seid equals to uplink VSI's
		 * uplink_seid since they share same VEB
		 */
		vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;
		ctxt.flags = I40E_AQ_VSI_TYPE_VMDQ2;

		ctxt.info.valid_sections |=
				rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID);
		/* user_param carries flag to enable loop back */
		if (user_param) {
			ctxt.info.switch_id =
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_LOCAL_LB);
			ctxt.info.switch_id |=
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}

		/* Configure port/vlan */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to configure "
					"TC queue mapping");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
	} else if (type == I40E_VSI_FDIR) {
		memset(&ctxt, 0, sizeof(ctxt));
		vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;     /* regular data port */
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to configure "
					"TC queue mapping.");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
	} else {
		PMD_DRV_LOG(ERR, "VSI: Not support other type VSI yet");
		goto fail_msix_alloc;
	}

	if (vsi->type != I40E_VSI_MAIN) {
		ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "add vsi failed, aq_err=%d",
				    hw->aq.asq_last_status);
			goto fail_msix_alloc;
		}
		memcpy(&vsi->info, &ctxt.info, sizeof(ctxt.info));
		vsi->info.valid_sections = 0;
		vsi->seid = ctxt.seid;
		vsi->vsi_id = ctxt.vsi_number;
		vsi->sib_vsi_list.vsi = vsi;
		TAILQ_INSERT_TAIL(&uplink_vsi->veb->head,
				&vsi->sib_vsi_list, list);
	}

	/* MAC/VLAN configuration */
	(void)rte_memcpy(&filter.mac_addr, &broadcast, ETHER_ADDR_LEN);
	filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;

	ret = i40e_vsi_add_mac(vsi, &filter);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter");
		goto fail_msix_alloc;
	}

	/* Get VSI BW information */
	i40e_vsi_dump_bw_config(vsi);
	return vsi;
fail_msix_alloc:
	i40e_res_pool_free(&pf->msix_pool,vsi->msix_intr);
fail_queue_alloc:
	i40e_res_pool_free(&pf->qp_pool,vsi->base_queue);
fail_mem:
	rte_free(vsi);
	return NULL;
}

/* Configure vlan stripping on or off */
int
i40e_vsi_config_vlan_stripping(struct i40e_vsi *vsi, bool on)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_vsi_context ctxt;
	uint8_t vlan_flags;
	int ret = I40E_SUCCESS;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID)) {
		if (on) {
			if ((vsi->info.port_vlan_flags &
				I40E_AQ_VSI_PVLAN_EMOD_MASK) == 0)
				return 0; /* already on */
		} else {
			if ((vsi->info.port_vlan_flags &
				I40E_AQ_VSI_PVLAN_EMOD_MASK) ==
				I40E_AQ_VSI_PVLAN_EMOD_MASK)
				return 0; /* already off */
		}
	}

	if (on)
		vlan_flags = I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	else
		vlan_flags = I40E_AQ_VSI_PVLAN_EMOD_NOTHING;
	vsi->info.valid_sections =
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.port_vlan_flags &= ~(I40E_AQ_VSI_PVLAN_EMOD_MASK);
	vsi->info.port_vlan_flags |= vlan_flags;
	ctxt.seid = vsi->seid;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret)
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan stripping",
			    on ? "enable" : "disable");

	return ret;
}

static int
i40e_dev_init_vlan(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	int ret;

	/* Apply vlan offload setting */
	i40e_vlan_offload_set(dev, ETH_VLAN_STRIP_MASK);

	/* Apply double-vlan setting, not implemented yet */

	/* Apply pvid setting */
	ret = i40e_vlan_pvid_set(dev, data->dev_conf.txmode.pvid,
				data->dev_conf.txmode.hw_vlan_insert_pvid);
	if (ret)
		PMD_DRV_LOG(INFO, "Failed to update VSI params");

	return ret;
}

static int
i40e_vsi_config_double_vlan(struct i40e_vsi *vsi, int on)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	return i40e_aq_set_port_parameters(hw, vsi->seid, 0, 1, on, NULL);
}

static int
i40e_update_flow_control(struct i40e_hw *hw)
{
#define I40E_LINK_PAUSE_RXTX (I40E_AQ_LINK_PAUSE_RX | I40E_AQ_LINK_PAUSE_TX)
	struct i40e_link_status link_status;
	uint32_t rxfc = 0, txfc = 0, reg;
	uint8_t an_info;
	int ret;

	memset(&link_status, 0, sizeof(link_status));
	ret = i40e_aq_get_link_info(hw, FALSE, &link_status, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get link status information");
		goto write_reg; /* Disable flow control */
	}

	an_info = hw->phy.link_info.an_info;
	if (!(an_info & I40E_AQ_AN_COMPLETED)) {
		PMD_DRV_LOG(INFO, "Link auto negotiation not completed");
		ret = I40E_ERR_NOT_READY;
		goto write_reg; /* Disable flow control */
	}
	/**
	 * If link auto negotiation is enabled, flow control needs to
	 * be configured according to it
	 */
	switch (an_info & I40E_LINK_PAUSE_RXTX) {
	case I40E_LINK_PAUSE_RXTX:
		rxfc = 1;
		txfc = 1;
		hw->fc.current_mode = I40E_FC_FULL;
		break;
	case I40E_AQ_LINK_PAUSE_RX:
		rxfc = 1;
		hw->fc.current_mode = I40E_FC_RX_PAUSE;
		break;
	case I40E_AQ_LINK_PAUSE_TX:
		txfc = 1;
		hw->fc.current_mode = I40E_FC_TX_PAUSE;
		break;
	default:
		hw->fc.current_mode = I40E_FC_NONE;
		break;
	}

write_reg:
	I40E_WRITE_REG(hw, I40E_PRTDCB_FCCFG,
		txfc << I40E_PRTDCB_FCCFG_TFCE_SHIFT);
	reg = I40E_READ_REG(hw, I40E_PRTDCB_MFLCN);
	reg &= ~I40E_PRTDCB_MFLCN_RFCE_MASK;
	reg |= rxfc << I40E_PRTDCB_MFLCN_RFCE_SHIFT;
	I40E_WRITE_REG(hw, I40E_PRTDCB_MFLCN, reg);

	return ret;
}

/* PF setup */
static int
i40e_pf_setup(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_filter_control_settings settings;
	struct i40e_vsi *vsi;
	int ret;

	/* Clear all stats counters */
	pf->offset_loaded = FALSE;
	memset(&pf->stats, 0, sizeof(struct i40e_hw_port_stats));
	memset(&pf->stats_offset, 0, sizeof(struct i40e_hw_port_stats));

	ret = i40e_pf_get_switch_config(pf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Could not get switch config, err %d", ret);
		return ret;
	}
	if (pf->flags & I40E_FLAG_FDIR) {
		/* make queue allocated first, let FDIR use queue pair 0*/
		ret = i40e_res_pool_alloc(&pf->qp_pool, I40E_DEFAULT_QP_NUM_FDIR);
		if (ret != I40E_FDIR_QUEUE_ID) {
			PMD_DRV_LOG(ERR, "queue allocation fails for FDIR :"
				    " ret =%d", ret);
			pf->flags &= ~I40E_FLAG_FDIR;
		}
	}
	/*  main VSI setup */
	vsi = i40e_vsi_setup(pf, I40E_VSI_MAIN, NULL, 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Setup of main vsi failed");
		return I40E_ERR_NOT_READY;
	}
	pf->main_vsi = vsi;

	/* Configure filter control */
	memset(&settings, 0, sizeof(settings));
	if (hw->func_caps.rss_table_size == ETH_RSS_RETA_SIZE_128)
		settings.hash_lut_size = I40E_HASH_LUT_SIZE_128;
	else if (hw->func_caps.rss_table_size == ETH_RSS_RETA_SIZE_512)
		settings.hash_lut_size = I40E_HASH_LUT_SIZE_512;
	else {
		PMD_DRV_LOG(ERR, "Hash lookup table size (%u) not supported\n",
						hw->func_caps.rss_table_size);
		return I40E_ERR_PARAM;
	}
	PMD_DRV_LOG(INFO, "Hardware capability of hash lookup table "
			"size: %u\n", hw->func_caps.rss_table_size);
	pf->hash_lut_size = hw->func_caps.rss_table_size;

	/* Enable ethtype and macvlan filters */
	settings.enable_ethtype = TRUE;
	settings.enable_macvlan = TRUE;
	ret = i40e_set_filter_control(hw, &settings);
	if (ret)
		PMD_INIT_LOG(WARNING, "setup_pf_filter_control failed: %d",
								ret);

	/* Update flow control according to the auto negotiation */
	i40e_update_flow_control(hw);

	return I40E_SUCCESS;
}

int
i40e_switch_tx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/**
	 * Set or clear TX Queue Disable flags,
	 * which is required by hardware.
	 */
	i40e_pre_tx_queue_cfg(hw, q_idx, on);
	rte_delay_us(I40E_PRE_TX_Q_CFG_WAIT_US);

	/* Wait until the request is finished */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QTX_ENA(q_idx));
		if (!(((reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 0x1) ^
			((reg >> I40E_QTX_ENA_QENA_STAT_SHIFT)
							& 0x1))) {
			break;
		}
	}
	if (on) {
		if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
			return I40E_SUCCESS; /* already on, skip next steps */

		I40E_WRITE_REG(hw, I40E_QTX_HEAD(q_idx), 0);
		reg |= I40E_QTX_ENA_QENA_REQ_MASK;
	} else {
		if (!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
			return I40E_SUCCESS; /* already off, skip next steps */
		reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
	}
	/* Write the register */
	I40E_WRITE_REG(hw, I40E_QTX_ENA(q_idx), reg);
	/* Check the result */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QTX_ENA(q_idx));
		if (on) {
			if ((reg & I40E_QTX_ENA_QENA_REQ_MASK) &&
				(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
		} else {
			if (!(reg & I40E_QTX_ENA_QENA_REQ_MASK) &&
				!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
		}
	}
	/* Check if it is timeout */
	if (j >= I40E_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s tx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}

/* Swith on or off the tx queues */
static int
i40e_dev_switch_tx_queues(struct i40e_pf *pf, bool on)
{
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	struct i40e_tx_queue *txq;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	uint16_t i;
	int ret;

	for (i = 0; i < dev_data->nb_tx_queues; i++) {
		txq = dev_data->tx_queues[i];
		/* Don't operate the queue if not configured or
		 * if starting only per queue */
		if (!txq || !txq->q_set || (on && txq->tx_deferred_start))
			continue;
		if (on)
			ret = i40e_dev_tx_queue_start(dev, i);
		else
			ret = i40e_dev_tx_queue_stop(dev, i);
		if ( ret != I40E_SUCCESS)
			return ret;
	}

	return I40E_SUCCESS;
}

int
i40e_switch_rx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/* Wait until the request is finished */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QRX_ENA(q_idx));
		if (!((reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 0x1) ^
			((reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 0x1))
			break;
	}

	if (on) {
		if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
			return I40E_SUCCESS; /* Already on, skip next steps */
		reg |= I40E_QRX_ENA_QENA_REQ_MASK;
	} else {
		if (!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
			return I40E_SUCCESS; /* Already off, skip next steps */
		reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
	}

	/* Write the register */
	I40E_WRITE_REG(hw, I40E_QRX_ENA(q_idx), reg);
	/* Check the result */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QRX_ENA(q_idx));
		if (on) {
			if ((reg & I40E_QRX_ENA_QENA_REQ_MASK) &&
				(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
		} else {
			if (!(reg & I40E_QRX_ENA_QENA_REQ_MASK) &&
				!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
		}
	}

	/* Check if it is timeout */
	if (j >= I40E_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s rx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}
/* Switch on or off the rx queues */
static int
i40e_dev_switch_rx_queues(struct i40e_pf *pf, bool on)
{
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	struct i40e_rx_queue *rxq;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	uint16_t i;
	int ret;

	for (i = 0; i < dev_data->nb_rx_queues; i++) {
		rxq = dev_data->rx_queues[i];
		/* Don't operate the queue if not configured or
		 * if starting only per queue */
		if (!rxq || !rxq->q_set || (on && rxq->rx_deferred_start))
			continue;
		if (on)
			ret = i40e_dev_rx_queue_start(dev, i);
		else
			ret = i40e_dev_rx_queue_stop(dev, i);
		if (ret != I40E_SUCCESS)
			return ret;
	}

	return I40E_SUCCESS;
}

/* Switch on or off all the rx/tx queues */
int
i40e_dev_switch_queues(struct i40e_pf *pf, bool on)
{
	int ret;

	if (on) {
		/* enable rx queues before enabling tx queues */
		ret = i40e_dev_switch_rx_queues(pf, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch rx queues");
			return ret;
		}
		ret = i40e_dev_switch_tx_queues(pf, on);
	} else {
		/* Stop tx queues before stopping rx queues */
		ret = i40e_dev_switch_tx_queues(pf, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch tx queues");
			return ret;
		}
		ret = i40e_dev_switch_rx_queues(pf, on);
	}

	return ret;
}

/* Initialize VSI for TX */
static int
i40e_dev_tx_init(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	uint16_t i;
	uint32_t ret = I40E_SUCCESS;
	struct i40e_tx_queue *txq;

	for (i = 0; i < data->nb_tx_queues; i++) {
		txq = data->tx_queues[i];
		if (!txq || !txq->q_set)
			continue;
		ret = i40e_tx_queue_init(txq);
		if (ret != I40E_SUCCESS)
			break;
	}

	return ret;
}

/* Initialize VSI for RX */
static int
i40e_dev_rx_init(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	int ret = I40E_SUCCESS;
	uint16_t i;
	struct i40e_rx_queue *rxq;

	i40e_pf_config_mq_rx(pf);
	for (i = 0; i < data->nb_rx_queues; i++) {
		rxq = data->rx_queues[i];
		if (!rxq || !rxq->q_set)
			continue;

		ret = i40e_rx_queue_init(rxq);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to do RX queue "
				    "initialization");
			break;
		}
	}

	return ret;
}

static int
i40e_dev_rxtx_init(struct i40e_pf *pf)
{
	int err;

	err = i40e_dev_tx_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do TX initialization");
		return err;
	}
	err = i40e_dev_rx_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do RX initialization");
		return err;
	}

	return err;
}

static int
i40e_vmdq_setup(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int i, err, conf_vsis, j, loop;
	struct i40e_vsi *vsi;
	struct i40e_vmdq_info *vmdq_info;
	struct rte_eth_vmdq_rx_conf *vmdq_conf;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	/*
	 * Disable interrupt to avoid message from VF. Furthermore, it will
	 * avoid race condition in VSI creation/destroy.
	 */
	i40e_pf_disable_irq0(hw);

	if ((pf->flags & I40E_FLAG_VMDQ) == 0) {
		PMD_INIT_LOG(ERR, "FW doesn't support VMDQ");
		return -ENOTSUP;
	}

	conf_vsis = conf->rx_adv_conf.vmdq_rx_conf.nb_queue_pools;
	if (conf_vsis > pf->max_nb_vmdq_vsi) {
		PMD_INIT_LOG(ERR, "VMDQ config: %u, max support:%u",
			conf->rx_adv_conf.vmdq_rx_conf.nb_queue_pools,
			pf->max_nb_vmdq_vsi);
		return -ENOTSUP;
	}

	if (pf->vmdq != NULL) {
		PMD_INIT_LOG(INFO, "VMDQ already configured");
		return 0;
	}

	pf->vmdq = rte_zmalloc("vmdq_info_struct",
				sizeof(*vmdq_info) * conf_vsis, 0);

	if (pf->vmdq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory");
		return -ENOMEM;
	}

	vmdq_conf = &conf->rx_adv_conf.vmdq_rx_conf;

	/* Create VMDQ VSI */
	for (i = 0; i < conf_vsis; i++) {
		vsi = i40e_vsi_setup(pf, I40E_VSI_VMDQ2, pf->main_vsi,
				vmdq_conf->enable_loop_back);
		if (vsi == NULL) {
			PMD_INIT_LOG(ERR, "Failed to create VMDQ VSI");
			err = -1;
			goto err_vsi_setup;
		}
		vmdq_info = &pf->vmdq[i];
		vmdq_info->pf = pf;
		vmdq_info->vsi = vsi;
	}
	pf->nb_cfg_vmdq_vsi = conf_vsis;

	/* Configure Vlan */
	loop = sizeof(vmdq_conf->pool_map[0].pools) * CHAR_BIT;
	for (i = 0; i < vmdq_conf->nb_pool_maps; i++) {
		for (j = 0; j < loop && j < pf->nb_cfg_vmdq_vsi; j++) {
			if (vmdq_conf->pool_map[i].pools & (1UL << j)) {
				PMD_INIT_LOG(INFO, "Add vlan %u to vmdq pool %u",
					vmdq_conf->pool_map[i].vlan_id, j);

				err = i40e_vsi_add_vlan(pf->vmdq[j].vsi,
						vmdq_conf->pool_map[i].vlan_id);
				if (err) {
					PMD_INIT_LOG(ERR, "Failed to add vlan");
					err = -1;
					goto err_vsi_setup;
				}
			}
		}
	}

	i40e_pf_enable_irq0(hw);

	return 0;

err_vsi_setup:
	for (i = 0; i < conf_vsis; i++)
		if (pf->vmdq[i].vsi == NULL)
			break;
		else
			i40e_vsi_release(pf->vmdq[i].vsi);

	rte_free(pf->vmdq);
	pf->vmdq = NULL;
	i40e_pf_enable_irq0(hw);
	return err;
}

static void
i40e_stat_update_32(struct i40e_hw *hw,
		   uint32_t reg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)I40E_READ_REG(hw, reg);
	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = (uint64_t)(new_data - *offset);
	else
		*stat = (uint64_t)((new_data +
			((uint64_t)1 << I40E_32_BIT_WIDTH)) - *offset);
}

static void
i40e_stat_update_48(struct i40e_hw *hw,
		   uint32_t hireg,
		   uint32_t loreg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)I40E_READ_REG(hw, loreg);
	new_data |= ((uint64_t)(I40E_READ_REG(hw, hireg) &
			I40E_16_BIT_MASK)) << I40E_32_BIT_WIDTH;

	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (uint64_t)((new_data +
			((uint64_t)1 << I40E_48_BIT_WIDTH)) - *offset);

	*stat &= I40E_48_BIT_MASK;
}

/* Disable IRQ0 */
void
i40e_pf_disable_irq0(struct i40e_hw *hw)
{
	/* Disable all interrupt types */
	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0, 0);
	I40E_WRITE_FLUSH(hw);
}

/* Enable IRQ0 */
void
i40e_pf_enable_irq0(struct i40e_hw *hw)
{
	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0,
		I40E_PFINT_DYN_CTL0_INTENA_MASK |
		I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
		I40E_PFINT_DYN_CTL0_ITR_INDX_MASK);
	I40E_WRITE_FLUSH(hw);
}

static void
i40e_pf_config_irq0(struct i40e_hw *hw)
{
	/* read pending request and disable first */
	i40e_pf_disable_irq0(hw);
	I40E_WRITE_REG(hw, I40E_PFINT_ICR0_ENA, I40E_PFINT_ICR0_ENA_MASK);
	I40E_WRITE_REG(hw, I40E_PFINT_STAT_CTL0,
		I40E_PFINT_STAT_CTL0_OTHER_ITR_INDX_MASK);

	/* Link no queues with irq0 */
	I40E_WRITE_REG(hw, I40E_PFINT_LNKLST0,
		I40E_PFINT_LNKLST0_FIRSTQ_INDX_MASK);
}

static void
i40e_dev_handle_vfr_event(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int i;
	uint16_t abs_vf_id;
	uint32_t index, offset, val;

	if (!pf->vfs)
		return;
	/**
	 * Try to find which VF trigger a reset, use absolute VF id to access
	 * since the reg is global register.
	 */
	for (i = 0; i < pf->vf_num; i++) {
		abs_vf_id = hw->func_caps.vf_base_id + i;
		index = abs_vf_id / I40E_UINT32_BIT_SIZE;
		offset = abs_vf_id % I40E_UINT32_BIT_SIZE;
		val = I40E_READ_REG(hw, I40E_GLGEN_VFLRSTAT(index));
		/* VFR event occured */
		if (val & (0x1 << offset)) {
			int ret;

			/* Clear the event first */
			I40E_WRITE_REG(hw, I40E_GLGEN_VFLRSTAT(index),
							(0x1 << offset));
			PMD_DRV_LOG(INFO, "VF %u reset occured", abs_vf_id);
			/**
			 * Only notify a VF reset event occured,
			 * don't trigger another SW reset
			 */
			ret = i40e_pf_host_vf_reset(&pf->vfs[i], 0);
			if (ret != I40E_SUCCESS)
				PMD_DRV_LOG(ERR, "Failed to do VF reset");
		}
	}
}

static void
i40e_dev_handle_aq_msg(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_arq_event_info info;
	uint16_t pending, opcode;
	int ret;

	info.buf_len = I40E_AQ_BUF_SZ;
	info.msg_buf = rte_zmalloc("msg_buffer", info.buf_len, 0);
	if (!info.msg_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate mem");
		return;
	}

	pending = 1;
	while (pending) {
		ret = i40e_clean_arq_element(hw, &info, &pending);

		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to read msg from AdminQ, "
				    "aq_err: %u", hw->aq.asq_last_status);
			break;
		}
		opcode = rte_le_to_cpu_16(info.desc.opcode);

		switch (opcode) {
		case i40e_aqc_opc_send_msg_to_pf:
			/* Refer to i40e_aq_send_msg_to_pf() for argument layout*/
			i40e_pf_host_handle_vf_msg(dev,
					rte_le_to_cpu_16(info.desc.retval),
					rte_le_to_cpu_32(info.desc.cookie_high),
					rte_le_to_cpu_32(info.desc.cookie_low),
					info.msg_buf,
					info.msg_len);
			break;
		default:
			PMD_DRV_LOG(ERR, "Request %u is not supported yet",
				    opcode);
			break;
		}
	}
	rte_free(info.msg_buf);
}

/*
 * Interrupt handler is registered as the alarm callback for handling LSC
 * interrupt in a definite of time, in order to wait the NIC into a stable
 * state. Currently it waits 1 sec in i40e for the link up interrupt, and
 * no need for link down interrupt.
 */
static void
i40e_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t icr0;

	/* read interrupt causes again */
	icr0 = I40E_READ_REG(hw, I40E_PFINT_ICR0);

#ifdef RTE_LIBRTE_I40E_DEBUG_DRIVER
	if (icr0 & I40E_PFINT_ICR0_ECC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: unrecoverable ECC error\n");
	if (icr0 & I40E_PFINT_ICR0_MAL_DETECT_MASK)
		PMD_DRV_LOG(ERR, "ICR0: malicious programming detected\n");
	if (icr0 & I40E_PFINT_ICR0_GRST_MASK)
		PMD_DRV_LOG(INFO, "ICR0: global reset requested\n");
	if (icr0 & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK)
		PMD_DRV_LOG(INFO, "ICR0: PCI exception\n activated\n");
	if (icr0 & I40E_PFINT_ICR0_STORM_DETECT_MASK)
		PMD_DRV_LOG(INFO, "ICR0: a change in the storm control "
								"state\n");
	if (icr0 & I40E_PFINT_ICR0_HMC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: HMC error\n");
	if (icr0 & I40E_PFINT_ICR0_PE_CRITERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: protocol engine critical error\n");
#endif /* RTE_LIBRTE_I40E_DEBUG_DRIVER */

	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK) {
		PMD_DRV_LOG(INFO, "INT:VF reset detected\n");
		i40e_dev_handle_vfr_event(dev);
	}
	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		PMD_DRV_LOG(INFO, "INT:ADMINQ event\n");
		i40e_dev_handle_aq_msg(dev);
	}

	/* handle the link up interrupt in an alarm callback */
	i40e_dev_link_update(dev, 0);
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC);

	i40e_pf_enable_irq0(hw);
	rte_intr_enable(&(dev->pci_dev->intr_handle));
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
i40e_dev_interrupt_handler(__rte_unused struct rte_intr_handle *handle,
			   void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t icr0;

	/* Disable interrupt */
	i40e_pf_disable_irq0(hw);

	/* read out interrupt causes */
	icr0 = I40E_READ_REG(hw, I40E_PFINT_ICR0);

	/* No interrupt event indicated */
	if (!(icr0 & I40E_PFINT_ICR0_INTEVENT_MASK)) {
		PMD_DRV_LOG(INFO, "No interrupt event");
		goto done;
	}
#ifdef RTE_LIBRTE_I40E_DEBUG_DRIVER
	if (icr0 & I40E_PFINT_ICR0_ECC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: unrecoverable ECC error");
	if (icr0 & I40E_PFINT_ICR0_MAL_DETECT_MASK)
		PMD_DRV_LOG(ERR, "ICR0: malicious programming detected");
	if (icr0 & I40E_PFINT_ICR0_GRST_MASK)
		PMD_DRV_LOG(INFO, "ICR0: global reset requested");
	if (icr0 & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK)
		PMD_DRV_LOG(INFO, "ICR0: PCI exception activated");
	if (icr0 & I40E_PFINT_ICR0_STORM_DETECT_MASK)
		PMD_DRV_LOG(INFO, "ICR0: a change in the storm control state");
	if (icr0 & I40E_PFINT_ICR0_HMC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: HMC error");
	if (icr0 & I40E_PFINT_ICR0_PE_CRITERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: protocol engine critical error");
#endif /* RTE_LIBRTE_I40E_DEBUG_DRIVER */

	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK) {
		PMD_DRV_LOG(INFO, "ICR0: VF reset detected");
		i40e_dev_handle_vfr_event(dev);
	}
	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		PMD_DRV_LOG(INFO, "ICR0: adminq event");
		i40e_dev_handle_aq_msg(dev);
	}

	/* Link Status Change interrupt */
	if (icr0 & I40E_PFINT_ICR0_LINK_STAT_CHANGE_MASK) {
#define I40E_US_PER_SECOND 1000000
		struct rte_eth_link link;

		PMD_DRV_LOG(INFO, "ICR0: link status changed\n");
		memset(&link, 0, sizeof(link));
		rte_i40e_dev_atomic_read_link_status(dev, &link);
		i40e_dev_link_update(dev, 0);

		/*
		 * For link up interrupt, it needs to wait 1 second to let the
		 * hardware be a stable state. Otherwise several consecutive
		 * interrupts can be observed.
		 * For link down interrupt, no need to wait.
		 */
		if (!link.link_status && rte_eal_alarm_set(I40E_US_PER_SECOND,
			i40e_dev_interrupt_delayed_handler, (void *)dev) >= 0)
			return;
		else
			_rte_eth_dev_callback_process(dev,
				RTE_ETH_EVENT_INTR_LSC);
	}

done:
	/* Enable interrupt */
	i40e_pf_enable_irq0(hw);
	rte_intr_enable(&(dev->pci_dev->intr_handle));
}

static int
i40e_add_macvlan_filters(struct i40e_vsi *vsi,
			 struct i40e_macvlan_filter *filter,
			 int total)
{
	int ele_num, ele_buff_size;
	int num, actual_num, i;
	uint16_t flags;
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_add_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;
	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_add", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	num = 0;
	do {
		actual_num = (num + ele_num > total) ? (total - num) : ele_num;
		memset(req_list, 0, ele_buff_size);

		for (i = 0; i < actual_num; i++) {
			(void)rte_memcpy(req_list[i].mac_addr,
				&filter[num + i].macaddr, ETH_ADDR_LEN);
			req_list[i].vlan_tag =
				rte_cpu_to_le_16(filter[num + i].vlan_id);

			switch (filter[num + i].filter_type) {
			case RTE_MAC_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_PERFECT_MATCH |
					I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
				break;
			case RTE_MAC_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_HASH_MATCH |
					I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_HASH_MATCH;
				break;
			default:
				PMD_DRV_LOG(ERR, "Invalid MAC match type\n");
				ret = I40E_ERR_PARAM;
				goto DONE;
			}

			req_list[i].queue_number = 0;

			req_list[i].flags = rte_cpu_to_le_16(flags);
		}

		ret = i40e_aq_add_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to add macvlan filter");
			goto DONE;
		}
		num += actual_num;
	} while (num < total);

DONE:
	rte_free(req_list);
	return ret;
}

static int
i40e_remove_macvlan_filters(struct i40e_vsi *vsi,
			    struct i40e_macvlan_filter *filter,
			    int total)
{
	int ele_num, ele_buff_size;
	int num, actual_num, i;
	uint16_t flags;
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_remove_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;

	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_remove", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	num = 0;
	do {
		actual_num = (num + ele_num > total) ? (total - num) : ele_num;
		memset(req_list, 0, ele_buff_size);

		for (i = 0; i < actual_num; i++) {
			(void)rte_memcpy(req_list[i].mac_addr,
				&filter[num + i].macaddr, ETH_ADDR_LEN);
			req_list[i].vlan_tag =
				rte_cpu_to_le_16(filter[num + i].vlan_id);

			switch (filter[num + i].filter_type) {
			case RTE_MAC_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
					I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
				break;
			case RTE_MAC_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_HASH_MATCH |
					I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_HASH_MATCH;
				break;
			default:
				PMD_DRV_LOG(ERR, "Invalid MAC filter type\n");
				ret = I40E_ERR_PARAM;
				goto DONE;
			}
			req_list[i].flags = rte_cpu_to_le_16(flags);
		}

		ret = i40e_aq_remove_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to remove macvlan filter");
			goto DONE;
		}
		num += actual_num;
	} while (num < total);

DONE:
	rte_free(req_list);
	return ret;
}

/* Find out specific MAC filter */
static struct i40e_mac_filter *
i40e_find_mac_filter(struct i40e_vsi *vsi,
			 struct ether_addr *macaddr)
{
	struct i40e_mac_filter *f;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (is_same_ether_addr(macaddr, &f->mac_info.mac_addr))
			return f;
	}

	return NULL;
}

static bool
i40e_find_vlan_filter(struct i40e_vsi *vsi,
			 uint16_t vlan_id)
{
	uint32_t vid_idx, vid_bit;

	if (vlan_id > ETH_VLAN_ID_MAX)
		return 0;

	vid_idx = I40E_VFTA_IDX(vlan_id);
	vid_bit = I40E_VFTA_BIT(vlan_id);

	if (vsi->vfta[vid_idx] & vid_bit)
		return 1;
	else
		return 0;
}

static void
i40e_set_vlan_filter(struct i40e_vsi *vsi,
			 uint16_t vlan_id, bool on)
{
	uint32_t vid_idx, vid_bit;

	if (vlan_id > ETH_VLAN_ID_MAX)
		return;

	vid_idx = I40E_VFTA_IDX(vlan_id);
	vid_bit = I40E_VFTA_BIT(vlan_id);

	if (on)
		vsi->vfta[vid_idx] |= vid_bit;
	else
		vsi->vfta[vid_idx] &= ~vid_bit;
}

/**
 * Find all vlan options for specific mac addr,
 * return with actual vlan found.
 */
static inline int
i40e_find_all_vlan_for_mac(struct i40e_vsi *vsi,
			   struct i40e_macvlan_filter *mv_f,
			   int num, struct ether_addr *addr)
{
	int i;
	uint32_t j, k;

	/**
	 * Not to use i40e_find_vlan_filter to decrease the loop time,
	 * although the code looks complex.
	  */
	if (num < vsi->vlan_num)
		return I40E_ERR_PARAM;

	i = 0;
	for (j = 0; j < I40E_VFTA_SIZE; j++) {
		if (vsi->vfta[j]) {
			for (k = 0; k < I40E_UINT32_BIT_SIZE; k++) {
				if (vsi->vfta[j] & (1 << k)) {
					if (i > num - 1) {
						PMD_DRV_LOG(ERR, "vlan number "
							    "not match");
						return I40E_ERR_PARAM;
					}
					(void)rte_memcpy(&mv_f[i].macaddr,
							addr, ETH_ADDR_LEN);
					mv_f[i].vlan_id =
						j * I40E_UINT32_BIT_SIZE + k;
					i++;
				}
			}
		}
	}
	return I40E_SUCCESS;
}

static inline int
i40e_find_all_mac_for_vlan(struct i40e_vsi *vsi,
			   struct i40e_macvlan_filter *mv_f,
			   int num,
			   uint16_t vlan)
{
	int i = 0;
	struct i40e_mac_filter *f;

	if (num < vsi->mac_num)
		return I40E_ERR_PARAM;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (i > num - 1) {
			PMD_DRV_LOG(ERR, "buffer number not match");
			return I40E_ERR_PARAM;
		}
		(void)rte_memcpy(&mv_f[i].macaddr, &f->mac_info.mac_addr,
				ETH_ADDR_LEN);
		mv_f[i].vlan_id = vlan;
		mv_f[i].filter_type = f->mac_info.filter_type;
		i++;
	}

	return I40E_SUCCESS;
}

static int
i40e_vsi_remove_all_macvlan_filter(struct i40e_vsi *vsi)
{
	int i, num;
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int ret = I40E_SUCCESS;

	if (vsi == NULL || vsi->mac_num == 0)
		return I40E_ERR_PARAM;

	/* Case that no vlan is set */
	if (vsi->vlan_num == 0)
		num = vsi->mac_num;
	else
		num = vsi->mac_num * vsi->vlan_num;

	mv_f = rte_zmalloc("macvlan_data", num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	i = 0;
	if (vsi->vlan_num == 0) {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			(void)rte_memcpy(&mv_f[i].macaddr,
				&f->mac_info.mac_addr, ETH_ADDR_LEN);
			mv_f[i].vlan_id = 0;
			i++;
		}
	} else {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			ret = i40e_find_all_vlan_for_mac(vsi,&mv_f[i],
					vsi->vlan_num, &f->mac_info.mac_addr);
			if (ret != I40E_SUCCESS)
				goto DONE;
			i += vsi->vlan_num;
		}
	}

	ret = i40e_remove_macvlan_filters(vsi, mv_f, num);
DONE:
	rte_free(mv_f);

	return ret;
}

int
i40e_vsi_add_vlan(struct i40e_vsi *vsi, uint16_t vlan)
{
	struct i40e_macvlan_filter *mv_f;
	int mac_num;
	int ret = I40E_SUCCESS;

	if (!vsi || vlan > ETHER_MAX_VLAN_ID)
		return I40E_ERR_PARAM;

	/* If it's already set, just return */
	if (i40e_find_vlan_filter(vsi,vlan))
		return I40E_SUCCESS;

	mac_num = vsi->mac_num;

	if (mac_num == 0) {
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, vlan);

	if (ret != I40E_SUCCESS)
		goto DONE;

	ret = i40e_add_macvlan_filters(vsi, mv_f, mac_num);

	if (ret != I40E_SUCCESS)
		goto DONE;

	i40e_set_vlan_filter(vsi, vlan, 1);

	vsi->vlan_num++;
	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

int
i40e_vsi_delete_vlan(struct i40e_vsi *vsi, uint16_t vlan)
{
	struct i40e_macvlan_filter *mv_f;
	int mac_num;
	int ret = I40E_SUCCESS;

	/**
	 * Vlan 0 is the generic filter for untagged packets
	 * and can't be removed.
	 */
	if (!vsi || vlan == 0 || vlan > ETHER_MAX_VLAN_ID)
		return I40E_ERR_PARAM;

	/* If can't find it, just return */
	if (!i40e_find_vlan_filter(vsi, vlan))
		return I40E_ERR_PARAM;

	mac_num = vsi->mac_num;

	if (mac_num == 0) {
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, vlan);

	if (ret != I40E_SUCCESS)
		goto DONE;

	ret = i40e_remove_macvlan_filters(vsi, mv_f, mac_num);

	if (ret != I40E_SUCCESS)
		goto DONE;

	/* This is last vlan to remove, replace all mac filter with vlan 0 */
	if (vsi->vlan_num == 1) {
		ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, 0);
		if (ret != I40E_SUCCESS)
			goto DONE;

		ret = i40e_add_macvlan_filters(vsi, mv_f, mac_num);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	i40e_set_vlan_filter(vsi, vlan, 0);

	vsi->vlan_num--;
	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

int
i40e_vsi_add_mac(struct i40e_vsi *vsi, struct i40e_mac_filter_info *mac_filter)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num = 0;
	int ret = I40E_SUCCESS;

	/* If it's add and we've config it, return */
	f = i40e_find_mac_filter(vsi, &mac_filter->mac_addr);
	if (f != NULL)
		return I40E_SUCCESS;
	if ((mac_filter->filter_type == RTE_MACVLAN_PERFECT_MATCH) ||
		(mac_filter->filter_type == RTE_MACVLAN_HASH_MATCH)) {

		/**
		 * If vlan_num is 0, that's the first time to add mac,
		 * set mask for vlan_id 0.
		 */
		if (vsi->vlan_num == 0) {
			i40e_set_vlan_filter(vsi, 0, 1);
			vsi->vlan_num = 1;
		}
		vlan_num = vsi->vlan_num;
	} else if ((mac_filter->filter_type == RTE_MAC_PERFECT_MATCH) ||
			(mac_filter->filter_type == RTE_MAC_HASH_MATCH))
		vlan_num = 1;

	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	for (i = 0; i < vlan_num; i++) {
		mv_f[i].filter_type = mac_filter->filter_type;
		(void)rte_memcpy(&mv_f[i].macaddr, &mac_filter->mac_addr,
				ETH_ADDR_LEN);
	}

	if (mac_filter->filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		mac_filter->filter_type == RTE_MACVLAN_HASH_MATCH) {
		ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num,
					&mac_filter->mac_addr);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	ret = i40e_add_macvlan_filters(vsi, mv_f, vlan_num);
	if (ret != I40E_SUCCESS)
		goto DONE;

	/* Add the mac addr into mac list */
	f = rte_zmalloc("macv_filter", sizeof(*f), 0);
	if (f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = I40E_ERR_NO_MEMORY;
		goto DONE;
	}
	(void)rte_memcpy(&f->mac_info.mac_addr, &mac_filter->mac_addr,
			ETH_ADDR_LEN);
	f->mac_info.filter_type = mac_filter->filter_type;
	TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
	vsi->mac_num++;

	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);

	return ret;
}

int
i40e_vsi_delete_mac(struct i40e_vsi *vsi, struct ether_addr *addr)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num;
	enum rte_mac_filter_type filter_type;
	int ret = I40E_SUCCESS;

	/* Can't find it, return an error */
	f = i40e_find_mac_filter(vsi, addr);
	if (f == NULL)
		return I40E_ERR_PARAM;

	vlan_num = vsi->vlan_num;
	filter_type = f->mac_info.filter_type;
	if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		filter_type == RTE_MACVLAN_HASH_MATCH) {
		if (vlan_num == 0) {
			PMD_DRV_LOG(ERR, "VLAN number shouldn't be 0\n");
			return I40E_ERR_PARAM;
		}
	} else if (filter_type == RTE_MAC_PERFECT_MATCH ||
			filter_type == RTE_MAC_HASH_MATCH)
		vlan_num = 1;

	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	for (i = 0; i < vlan_num; i++) {
		mv_f[i].filter_type = filter_type;
		(void)rte_memcpy(&mv_f[i].macaddr, &f->mac_info.mac_addr,
				ETH_ADDR_LEN);
	}
	if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
			filter_type == RTE_MACVLAN_HASH_MATCH) {
		ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num, addr);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	ret = i40e_remove_macvlan_filters(vsi, mv_f, vlan_num);
	if (ret != I40E_SUCCESS)
		goto DONE;

	/* Remove the mac addr into mac list */
	TAILQ_REMOVE(&vsi->mac_list, f, next);
	rte_free(f);
	vsi->mac_num--;

	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

/* Configure hash enable flags for RSS */
uint64_t
i40e_config_hena(uint64_t flags)
{
	uint64_t hena = 0;

	if (!flags)
		return hena;

	if (flags & ETH_RSS_FRAG_IPV4)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4;
	if (flags & ETH_RSS_NONFRAG_IPV4_TCP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP;
	if (flags & ETH_RSS_NONFRAG_IPV4_UDP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	if (flags & ETH_RSS_NONFRAG_IPV4_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP;
	if (flags & ETH_RSS_NONFRAG_IPV4_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
	if (flags & ETH_RSS_FRAG_IPV6)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6;
	if (flags & ETH_RSS_NONFRAG_IPV6_TCP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP;
	if (flags & ETH_RSS_NONFRAG_IPV6_UDP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP;
	if (flags & ETH_RSS_NONFRAG_IPV6_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;
	if (flags & ETH_RSS_NONFRAG_IPV6_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
	if (flags & ETH_RSS_L2_PAYLOAD)
		hena |= 1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD;

	return hena;
}

/* Parse the hash enable flags */
uint64_t
i40e_parse_hena(uint64_t flags)
{
	uint64_t rss_hf = 0;

	if (!flags)
		return rss_hf;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4))
		rss_hf |= ETH_RSS_FRAG_IPV4;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6))
		rss_hf |= ETH_RSS_FRAG_IPV6;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD))
		rss_hf |= ETH_RSS_L2_PAYLOAD;

	return rss_hf;
}

/* Disable RSS */
static void
i40e_pf_disable_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint64_t hena;

	hena = (uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(1))) << 32;
	hena &= ~I40E_RSS_HENA_ALL;
	I40E_WRITE_REG(hw, I40E_PFQF_HENA(0), (uint32_t)hena);
	I40E_WRITE_REG(hw, I40E_PFQF_HENA(1), (uint32_t)(hena >> 32));
	I40E_WRITE_FLUSH(hw);
}

static int
i40e_hw_rss_hash_set(struct i40e_hw *hw, struct rte_eth_rss_conf *rss_conf)
{
	uint32_t *hash_key;
	uint8_t hash_key_len;
	uint64_t rss_hf;
	uint16_t i;
	uint64_t hena;

	hash_key = (uint32_t *)(rss_conf->rss_key);
	hash_key_len = rss_conf->rss_key_len;
	if (hash_key != NULL && hash_key_len >=
		(I40E_PFQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t)) {
		/* Fill in RSS hash key */
		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			I40E_WRITE_REG(hw, I40E_PFQF_HKEY(i), hash_key[i]);
	}

	rss_hf = rss_conf->rss_hf;
	hena = (uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(1))) << 32;
	hena &= ~I40E_RSS_HENA_ALL;
	hena |= i40e_config_hena(rss_hf);
	I40E_WRITE_REG(hw, I40E_PFQF_HENA(0), (uint32_t)hena);
	I40E_WRITE_REG(hw, I40E_PFQF_HENA(1), (uint32_t)(hena >> 32));
	I40E_WRITE_FLUSH(hw);

	return 0;
}

static int
i40e_dev_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t rss_hf = rss_conf->rss_hf & I40E_RSS_OFFLOAD_ALL;
	uint64_t hena;

	hena = (uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(1))) << 32;
	if (!(hena & I40E_RSS_HENA_ALL)) { /* RSS disabled */
		if (rss_hf != 0) /* Enable RSS */
			return -EINVAL;
		return 0; /* Nothing to do */
	}
	/* RSS enabled */
	if (rss_hf == 0) /* Disable RSS */
		return -EINVAL;

	return i40e_hw_rss_hash_set(hw, rss_conf);
}

static int
i40e_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *hash_key = (uint32_t *)(rss_conf->rss_key);
	uint64_t hena;
	uint16_t i;

	if (hash_key != NULL) {
		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			hash_key[i] = I40E_READ_REG(hw, I40E_PFQF_HKEY(i));
		rss_conf->rss_key_len = i * sizeof(uint32_t);
	}
	hena = (uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)I40E_READ_REG(hw, I40E_PFQF_HENA(1))) << 32;
	rss_conf->rss_hf = i40e_parse_hena(hena);

	return 0;
}

static int
i40e_dev_get_filter_type(uint16_t filter_type, uint16_t *flag)
{
	switch (filter_type) {
	case RTE_TUNNEL_FILTER_IMAC_IVLAN:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN;
		break;
	case RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN_TEN_ID;
		break;
	case RTE_TUNNEL_FILTER_IMAC_TENID:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_TEN_ID;
		break;
	case RTE_TUNNEL_FILTER_OMAC_TENID_IMAC:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_OMAC_TEN_ID_IMAC;
		break;
	case ETH_TUNNEL_FILTER_IMAC:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid tunnel filter type");
		return -EINVAL;
	}

	return 0;
}

static int
i40e_dev_tunnel_filter_set(struct i40e_pf *pf,
			struct rte_eth_tunnel_filter_conf *tunnel_filter,
			uint8_t add)
{
	uint16_t ip_type;
	uint8_t tun_type = 0;
	int val, ret = 0;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct i40e_aqc_add_remove_cloud_filters_element_data  *cld_filter;
	struct i40e_aqc_add_remove_cloud_filters_element_data  *pfilter;

	cld_filter = rte_zmalloc("tunnel_filter",
		sizeof(struct i40e_aqc_add_remove_cloud_filters_element_data),
		0);

	if (NULL == cld_filter) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -EINVAL;
	}
	pfilter = cld_filter;

	(void)rte_memcpy(&pfilter->outer_mac, tunnel_filter->outer_mac,
			sizeof(struct ether_addr));
	(void)rte_memcpy(&pfilter->inner_mac, tunnel_filter->inner_mac,
			sizeof(struct ether_addr));

	pfilter->inner_vlan = tunnel_filter->inner_vlan;
	if (tunnel_filter->ip_type == RTE_TUNNEL_IPTYPE_IPV4) {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV4;
		(void)rte_memcpy(&pfilter->ipaddr.v4.data,
				&tunnel_filter->ip_addr,
				sizeof(pfilter->ipaddr.v4.data));
	} else {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV6;
		(void)rte_memcpy(&pfilter->ipaddr.v6.data,
				&tunnel_filter->ip_addr,
				sizeof(pfilter->ipaddr.v6.data));
	}

	/* check tunneled type */
	switch (tunnel_filter->tunnel_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_XVLAN;
		break;
	case RTE_TUNNEL_TYPE_NVGRE:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_NVGRE_OMAC;
		break;
	default:
		/* Other tunnel types is not supported. */
		PMD_DRV_LOG(ERR, "tunnel type is not supported.");
		rte_free(cld_filter);
		return -EINVAL;
	}

	val = i40e_dev_get_filter_type(tunnel_filter->filter_type,
						&pfilter->flags);
	if (val < 0) {
		rte_free(cld_filter);
		return -EINVAL;
	}

	pfilter->flags |= I40E_AQC_ADD_CLOUD_FLAGS_TO_QUEUE | ip_type |
		(tun_type << I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT);
	pfilter->tenant_id = tunnel_filter->tenant_id;
	pfilter->queue_number = tunnel_filter->queue_id;

	if (add)
		ret = i40e_aq_add_cloud_filters(hw, vsi->seid, cld_filter, 1);
	else
		ret = i40e_aq_remove_cloud_filters(hw, vsi->seid,
						cld_filter, 1);

	rte_free(cld_filter);
	return ret;
}

static int
i40e_get_vxlan_port_idx(struct i40e_pf *pf, uint16_t port)
{
	uint8_t i;

	for (i = 0; i < I40E_MAX_PF_UDP_OFFLOAD_PORTS; i++) {
		if (pf->vxlan_ports[i] == port)
			return i;
	}

	return -1;
}

static int
i40e_add_vxlan_port(struct i40e_pf *pf, uint16_t port)
{
	int  idx, ret;
	uint8_t filter_idx;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	idx = i40e_get_vxlan_port_idx(pf, port);

	/* Check if port already exists */
	if (idx >= 0) {
		PMD_DRV_LOG(ERR, "Port %d already offloaded", port);
		return -EINVAL;
	}

	/* Now check if there is space to add the new port */
	idx = i40e_get_vxlan_port_idx(pf, 0);
	if (idx < 0) {
		PMD_DRV_LOG(ERR, "Maximum number of UDP ports reached,"
			"not adding port %d", port);
		return -ENOSPC;
	}

	ret =  i40e_aq_add_udp_tunnel(hw, port, I40E_AQC_TUNNEL_TYPE_VXLAN,
					&filter_idx, NULL);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to add VXLAN UDP port %d", port);
		return -1;
	}

	PMD_DRV_LOG(INFO, "Added port %d with AQ command with index %d",
			 port,  filter_idx);

	/* New port: add it and mark its index in the bitmap */
	pf->vxlan_ports[idx] = port;
	pf->vxlan_bitmap |= (1 << idx);

	if (!(pf->flags & I40E_FLAG_VXLAN))
		pf->flags |= I40E_FLAG_VXLAN;

	return 0;
}

static int
i40e_del_vxlan_port(struct i40e_pf *pf, uint16_t port)
{
	int idx;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	if (!(pf->flags & I40E_FLAG_VXLAN)) {
		PMD_DRV_LOG(ERR, "VXLAN UDP port was not configured.");
		return -EINVAL;
	}

	idx = i40e_get_vxlan_port_idx(pf, port);

	if (idx < 0) {
		PMD_DRV_LOG(ERR, "Port %d doesn't exist", port);
		return -EINVAL;
	}

	if (i40e_aq_del_udp_tunnel(hw, idx, NULL) < 0) {
		PMD_DRV_LOG(ERR, "Failed to delete VXLAN UDP port %d", port);
		return -1;
	}

	PMD_DRV_LOG(INFO, "Deleted port %d with AQ command with index %d",
			port, idx);

	pf->vxlan_ports[idx] = 0;
	pf->vxlan_bitmap &= ~(1 << idx);

	if (!pf->vxlan_bitmap)
		pf->flags &= ~I40E_FLAG_VXLAN;

	return 0;
}

/* Add UDP tunneling port */
static int
i40e_dev_udp_tunnel_add(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = i40e_add_vxlan_port(pf, udp_tunnel->udp_port);
		break;

	case RTE_TUNNEL_TYPE_GENEVE:
	case RTE_TUNNEL_TYPE_TEREDO:
		PMD_DRV_LOG(ERR, "Tunnel type is not supported now.");
		ret = -1;
		break;

	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -1;
		break;
	}

	return ret;
}

/* Remove UDP tunneling port */
static int
i40e_dev_udp_tunnel_del(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = i40e_del_vxlan_port(pf, udp_tunnel->udp_port);
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
	case RTE_TUNNEL_TYPE_TEREDO:
		PMD_DRV_LOG(ERR, "Tunnel type is not supported now.");
		ret = -1;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -1;
		break;
	}

	return ret;
}

/* Calculate the maximum number of contiguous PF queues that are configured */
static int
i40e_pf_calc_configured_queues_num(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	int i, num;
	struct i40e_rx_queue *rxq;

	num = 0;
	for (i = 0; i < pf->lan_nb_qps; i++) {
		rxq = data->rx_queues[i];
		if (rxq && rxq->q_set)
			num++;
		else
			break;
	}

	return num;
}

/* Configure RSS */
static int
i40e_pf_config_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct rte_eth_rss_conf rss_conf;
	uint32_t i, lut = 0;
	uint16_t j, num;

	/*
	 * If both VMDQ and RSS enabled, not all of PF queues are configured.
	 * It's necessary to calulate the actual PF queues that are configured.
	 */
	if (pf->dev_data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_VMDQ_FLAG) {
		num = i40e_pf_calc_configured_queues_num(pf);
		num = i40e_align_floor(num);
	} else
		num = i40e_align_floor(pf->dev_data->nb_rx_queues);

	PMD_INIT_LOG(INFO, "Max of contiguous %u PF queues are configured",
			num);

	if (num == 0) {
		PMD_INIT_LOG(ERR, "No PF queues are configured to enable RSS");
		return -ENOTSUP;
	}

	for (i = 0, j = 0; i < hw->func_caps.rss_table_size; i++, j++) {
		if (j == num)
			j = 0;
		lut = (lut << 8) | (j & ((0x1 <<
			hw->func_caps.rss_table_entry_width) - 1));
		if ((i & 3) == 3)
			I40E_WRITE_REG(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}

	rss_conf = pf->dev_data->dev_conf.rx_adv_conf.rss_conf;
	if ((rss_conf.rss_hf & I40E_RSS_OFFLOAD_ALL) == 0) {
		i40e_pf_disable_rss(pf);
		return 0;
	}
	if (rss_conf.rss_key == NULL || rss_conf.rss_key_len <
		(I40E_PFQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t)) {
		/* Random default keys */
		static uint32_t rss_key_default[] = {0x6b793944,
			0x23504cb5, 0x5bea75b6, 0x309f4f12, 0x3dc0a2b8,
			0x024ddcdf, 0x339b8ca0, 0x4c4af64a, 0x34fac605,
			0x55d85839, 0x3a58997d, 0x2ec938e1, 0x66031581};

		rss_conf.rss_key = (uint8_t *)rss_key_default;
		rss_conf.rss_key_len = (I40E_PFQF_HKEY_MAX_INDEX + 1) *
							sizeof(uint32_t);
	}

	return i40e_hw_rss_hash_set(hw, &rss_conf);
}

static int
i40e_tunnel_filter_param_check(struct i40e_pf *pf,
			struct rte_eth_tunnel_filter_conf *filter)
{
	if (pf == NULL || filter == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (filter->queue_id >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}

	if (filter->inner_vlan > ETHER_MAX_VLAN_ID) {
		PMD_DRV_LOG(ERR, "Invalid inner VLAN ID");
		return -EINVAL;
	}

	if ((filter->filter_type & ETH_TUNNEL_FILTER_OMAC) &&
		(is_zero_ether_addr(filter->outer_mac))) {
		PMD_DRV_LOG(ERR, "Cannot add NULL outer MAC address");
		return -EINVAL;
	}

	if ((filter->filter_type & ETH_TUNNEL_FILTER_IMAC) &&
		(is_zero_ether_addr(filter->inner_mac))) {
		PMD_DRV_LOG(ERR, "Cannot add NULL inner MAC address");
		return -EINVAL;
	}

	return 0;
}

static int
i40e_tunnel_filter_handle(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
			void *arg)
{
	struct rte_eth_tunnel_filter_conf *filter;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = I40E_SUCCESS;

	filter = (struct rte_eth_tunnel_filter_conf *)(arg);

	if (i40e_tunnel_filter_param_check(pf, filter) < 0)
		return I40E_ERR_PARAM;

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		if (!(pf->flags & I40E_FLAG_VXLAN))
			ret = I40E_NOT_SUPPORTED;
	case RTE_ETH_FILTER_ADD:
		ret = i40e_dev_tunnel_filter_set(pf, filter, 1);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = i40e_dev_tunnel_filter_set(pf, filter, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = I40E_ERR_PARAM;
		break;
	}

	return ret;
}

static int
i40e_pf_config_mq_rx(struct i40e_pf *pf)
{
	int ret = 0;
	enum rte_eth_rx_mq_mode mq_mode = pf->dev_data->dev_conf.rxmode.mq_mode;

	if (mq_mode & ETH_MQ_RX_DCB_FLAG) {
		PMD_INIT_LOG(ERR, "i40e doesn't support DCB yet");
		return -ENOTSUP;
	}

	/* RSS setup */
	if (mq_mode & ETH_MQ_RX_RSS_FLAG)
		ret = i40e_pf_config_rss(pf);
	else
		i40e_pf_disable_rss(pf);

	return ret;
}

/* Get the symmetric hash enable configurations per port */
static void
i40e_get_symmetric_hash_enable_per_port(struct i40e_hw *hw, uint8_t *enable)
{
	uint32_t reg = I40E_READ_REG(hw, I40E_PRTQF_CTL_0);

	*enable = reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK ? 1 : 0;
}

/* Set the symmetric hash enable configurations per port */
static void
i40e_set_symmetric_hash_enable_per_port(struct i40e_hw *hw, uint8_t enable)
{
	uint32_t reg = I40E_READ_REG(hw, I40E_PRTQF_CTL_0);

	if (enable > 0) {
		if (reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK) {
			PMD_DRV_LOG(INFO, "Symmetric hash has already "
							"been enabled");
			return;
		}
		reg |= I40E_PRTQF_CTL_0_HSYM_ENA_MASK;
	} else {
		if (!(reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK)) {
			PMD_DRV_LOG(INFO, "Symmetric hash has already "
							"been disabled");
			return;
		}
		reg &= ~I40E_PRTQF_CTL_0_HSYM_ENA_MASK;
	}
	I40E_WRITE_REG(hw, I40E_PRTQF_CTL_0, reg);
	I40E_WRITE_FLUSH(hw);
}

/*
 * Get global configurations of hash function type and symmetric hash enable
 * per flow type (pctype). Note that global configuration means it affects all
 * the ports on the same NIC.
 */
static int
i40e_get_hash_filter_global_config(struct i40e_hw *hw,
				   struct rte_eth_hash_global_conf *g_cfg)
{
	uint32_t reg, mask = I40E_FLOW_TYPES;
	uint16_t i;
	enum i40e_filter_pctype pctype;

	memset(g_cfg, 0, sizeof(*g_cfg));
	reg = I40E_READ_REG(hw, I40E_GLQF_CTL);
	if (reg & I40E_GLQF_CTL_HTOEP_MASK)
		g_cfg->hash_func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
	else
		g_cfg->hash_func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
	PMD_DRV_LOG(DEBUG, "Hash function is %s",
		(reg & I40E_GLQF_CTL_HTOEP_MASK) ? "Toeplitz" : "Simple XOR");

	for (i = 0; mask && i < RTE_ETH_FLOW_MAX; i++) {
		if (!(mask & (1UL << i)))
			continue;
		mask &= ~(1UL << i);
		/* Bit set indicats the coresponding flow type is supported */
		g_cfg->valid_bit_mask[0] |= (1UL << i);
		pctype = i40e_flowtype_to_pctype(i);
		reg = I40E_READ_REG(hw, I40E_GLQF_HSYM(pctype));
		if (reg & I40E_GLQF_HSYM_SYMH_ENA_MASK)
			g_cfg->sym_hash_enable_mask[0] |= (1UL << i);
	}

	return 0;
}

static int
i40e_hash_global_config_check(struct rte_eth_hash_global_conf *g_cfg)
{
	uint32_t i;
	uint32_t mask0, i40e_mask = I40E_FLOW_TYPES;

	if (g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_TOEPLITZ &&
		g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_SIMPLE_XOR &&
		g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		PMD_DRV_LOG(ERR, "Unsupported hash function type %d",
						g_cfg->hash_func);
		return -EINVAL;
	}

	/*
	 * As i40e supports less than 32 flow types, only first 32 bits need to
	 * be checked.
	 */
	mask0 = g_cfg->valid_bit_mask[0];
	for (i = 0; i < RTE_SYM_HASH_MASK_ARRAY_SIZE; i++) {
		if (i == 0) {
			/* Check if any unsupported flow type configured */
			if ((mask0 | i40e_mask) ^ i40e_mask)
				goto mask_err;
		} else {
			if (g_cfg->valid_bit_mask[i])
				goto mask_err;
		}
	}

	return 0;

mask_err:
	PMD_DRV_LOG(ERR, "i40e unsupported flow type bit(s) configured");

	return -EINVAL;
}

/*
 * Set global configurations of hash function type and symmetric hash enable
 * per flow type (pctype). Note any modifying global configuration will affect
 * all the ports on the same NIC.
 */
static int
i40e_set_hash_filter_global_config(struct i40e_hw *hw,
				   struct rte_eth_hash_global_conf *g_cfg)
{
	int ret;
	uint16_t i;
	uint32_t reg;
	uint32_t mask0 = g_cfg->valid_bit_mask[0];
	enum i40e_filter_pctype pctype;

	/* Check the input parameters */
	ret = i40e_hash_global_config_check(g_cfg);
	if (ret < 0)
		return ret;

	for (i = 0; mask0 && i < UINT32_BIT; i++) {
		if (!(mask0 & (1UL << i)))
			continue;
		mask0 &= ~(1UL << i);
		pctype = i40e_flowtype_to_pctype(i);
		reg = (g_cfg->sym_hash_enable_mask[0] & (1UL << i)) ?
				I40E_GLQF_HSYM_SYMH_ENA_MASK : 0;
		I40E_WRITE_REG(hw, I40E_GLQF_HSYM(pctype), reg);
	}

	reg = I40E_READ_REG(hw, I40E_GLQF_CTL);
	if (g_cfg->hash_func == RTE_ETH_HASH_FUNCTION_TOEPLITZ) {
		/* Toeplitz */
		if (reg & I40E_GLQF_CTL_HTOEP_MASK) {
			PMD_DRV_LOG(DEBUG, "Hash function already set to "
								"Toeplitz");
			goto out;
		}
		reg |= I40E_GLQF_CTL_HTOEP_MASK;
	} else if (g_cfg->hash_func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		/* Simple XOR */
		if (!(reg & I40E_GLQF_CTL_HTOEP_MASK)) {
			PMD_DRV_LOG(DEBUG, "Hash function already set to "
							"Simple XOR");
			goto out;
		}
		reg &= ~I40E_GLQF_CTL_HTOEP_MASK;
	} else
		/* Use the default, and keep it as it is */
		goto out;

	I40E_WRITE_REG(hw, I40E_GLQF_CTL, reg);

out:
	I40E_WRITE_FLUSH(hw);

	return 0;
}

static int
i40e_hash_filter_get(struct i40e_hw *hw, struct rte_eth_hash_filter_info *info)
{
	int ret = 0;

	if (!hw || !info) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}

	switch (info->info_type) {
	case RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT:
		i40e_get_symmetric_hash_enable_per_port(hw,
					&(info->info.enable));
		break;
	case RTE_ETH_HASH_FILTER_GLOBAL_CONFIG:
		ret = i40e_get_hash_filter_global_config(hw,
				&(info->info.global_conf));
		break;
	default:
		PMD_DRV_LOG(ERR, "Hash filter info type (%d) not supported",
							info->info_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
i40e_hash_filter_set(struct i40e_hw *hw, struct rte_eth_hash_filter_info *info)
{
	int ret = 0;

	if (!hw || !info) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}

	switch (info->info_type) {
	case RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT:
		i40e_set_symmetric_hash_enable_per_port(hw, info->info.enable);
		break;
	case RTE_ETH_HASH_FILTER_GLOBAL_CONFIG:
		ret = i40e_set_hash_filter_global_config(hw,
				&(info->info.global_conf));
		break;
	default:
		PMD_DRV_LOG(ERR, "Hash filter info type (%d) not supported",
							info->info_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Operations for hash function */
static int
i40e_hash_filter_ctrl(struct rte_eth_dev *dev,
		      enum rte_filter_op filter_op,
		      void *arg)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret = 0;

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		break;
	case RTE_ETH_FILTER_GET:
		ret = i40e_hash_filter_get(hw,
			(struct rte_eth_hash_filter_info *)arg);
		break;
	case RTE_ETH_FILTER_SET:
		ret = i40e_hash_filter_set(hw,
			(struct rte_eth_hash_filter_info *)arg);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter operation (%d) not supported",
								filter_op);
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

/*
 * Configure ethertype filter, which can director packet by filtering
 * with mac address and ether_type or only ether_type
 */
static int
i40e_ethertype_filter_set(struct i40e_pf *pf,
			struct rte_eth_ethertype_filter *filter,
			bool add)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_control_filter_stats stats;
	uint16_t flags = 0;
	int ret;

	if (filter->queue >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}
	if (filter->ether_type == ETHER_TYPE_IPv4 ||
		filter->ether_type == ETHER_TYPE_IPv6) {
		PMD_DRV_LOG(ERR, "unsupported ether_type(0x%04x) in"
			" control packet filter.", filter->ether_type);
		return -EINVAL;
	}
	if (filter->ether_type == ETHER_TYPE_VLAN)
		PMD_DRV_LOG(WARNING, "filter vlan ether_type in first tag is"
			" not supported.");

	if (!(filter->flags & RTE_ETHTYPE_FLAGS_MAC))
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC;
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP)
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP;
	flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TO_QUEUE;

	memset(&stats, 0, sizeof(stats));
	ret = i40e_aq_add_rem_control_packet_filter(hw,
			filter->mac_addr.addr_bytes,
			filter->ether_type, flags,
			pf->main_vsi->seid,
			filter->queue, add, &stats, NULL);

	PMD_DRV_LOG(INFO, "add/rem control packet filter, return %d,"
			 " mac_etype_used = %u, etype_used = %u,"
			 " mac_etype_free = %u, etype_free = %u\n",
			 ret, stats.mac_etype_used, stats.etype_used,
			 stats.mac_etype_free, stats.etype_free);
	if (ret < 0)
		return -ENOSYS;
	return 0;
}

/*
 * Handle operations for ethertype filter.
 */
static int
i40e_ethertype_filter_handle(struct rte_eth_dev *dev,
				enum rte_filter_op filter_op,
				void *arg)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = 0;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return ret;

	if (arg == NULL) {
		PMD_DRV_LOG(ERR, "arg shouldn't be NULL for operation %u",
			    filter_op);
		return -EINVAL;
	}

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = i40e_ethertype_filter_set(pf,
			(struct rte_eth_ethertype_filter *)arg,
			TRUE);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = i40e_ethertype_filter_set(pf,
			(struct rte_eth_ethertype_filter *)arg,
			FALSE);
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported operation %u\n", filter_op);
		ret = -ENOSYS;
		break;
	}
	return ret;
}

static int
i40e_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	switch (filter_type) {
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_ctrl(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_MACVLAN:
		ret = i40e_mac_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_ethertype_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_tunnel_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_fdir_ctrl_func(dev, filter_op, arg);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
							filter_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * As some registers wouldn't be reset unless a global hardware reset,
 * hardware initialization is needed to put those registers into an
 * expected initial state.
 */
static void
i40e_hw_init(struct i40e_hw *hw)
{
	/* clear the PF Queue Filter control register */
	I40E_WRITE_REG(hw, I40E_PFQF_CTL_0, 0);

	/* Disable symmetric hash per port */
	i40e_set_symmetric_hash_enable_per_port(hw, 0);
}

enum i40e_filter_pctype
i40e_flowtype_to_pctype(uint16_t flow_type)
{
	static const enum i40e_filter_pctype pctype_table[] = {
		[RTE_ETH_FLOW_FRAG_IPV4] = I40E_FILTER_PCTYPE_FRAG_IPV4,
		[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_SCTP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_OTHER] =
			I40E_FILTER_PCTYPE_NONF_IPV4_OTHER,
		[RTE_ETH_FLOW_FRAG_IPV6] = I40E_FILTER_PCTYPE_FRAG_IPV6,
		[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_SCTP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_OTHER] =
			I40E_FILTER_PCTYPE_NONF_IPV6_OTHER,
		[RTE_ETH_FLOW_L2_PAYLOAD] = I40E_FILTER_PCTYPE_L2_PAYLOAD,
	};

	return pctype_table[flow_type];
}

uint16_t
i40e_pctype_to_flowtype(enum i40e_filter_pctype pctype)
{
	static const uint16_t flowtype_table[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] = RTE_ETH_FLOW_FRAG_IPV4,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] =
			RTE_ETH_FLOW_NONFRAG_IPV4_OTHER,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] = RTE_ETH_FLOW_FRAG_IPV6,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] =
			RTE_ETH_FLOW_NONFRAG_IPV6_OTHER,
		[I40E_FILTER_PCTYPE_L2_PAYLOAD] = RTE_ETH_FLOW_L2_PAYLOAD,
	};

	return flowtype_table[pctype];
}

/*
 * On X710, performance number is far from the expectation on recent firmware
 * versions; on XL710, performance number is also far from the expectation on
 * recent firmware versions, if promiscuous mode is disabled, or promiscuous
 * mode is enabled and port MAC address is equal to the packet destination MAC
 * address. The fix for this issue may not be integrated in the following
 * firmware version. So the workaround in software driver is needed. It needs
 * to modify the initial values of 3 internal only registers for both X710 and
 * XL710. Note that the values for X710 or XL710 could be different, and the
 * workaround can be removed when it is fixed in firmware in the future.
 */

/* For both X710 and XL710 */
#define I40E_GL_SWR_PRI_JOIN_MAP_0_VALUE 0x10000200
#define I40E_GL_SWR_PRI_JOIN_MAP_0       0x26CE00

#define I40E_GL_SWR_PRI_JOIN_MAP_2_VALUE 0x011f0200
#define I40E_GL_SWR_PRI_JOIN_MAP_2       0x26CE08

/* For X710 */
#define I40E_GL_SWR_PM_UP_THR_EF_VALUE   0x03030303
/* For XL710 */
#define I40E_GL_SWR_PM_UP_THR_SF_VALUE   0x06060606
#define I40E_GL_SWR_PM_UP_THR            0x269FBC

static void
i40e_configure_registers(struct i40e_hw *hw)
{
	static struct {
		uint32_t addr;
		uint64_t val;
	} reg_table[] = {
		{I40E_GL_SWR_PRI_JOIN_MAP_0, I40E_GL_SWR_PRI_JOIN_MAP_0_VALUE},
		{I40E_GL_SWR_PRI_JOIN_MAP_2, I40E_GL_SWR_PRI_JOIN_MAP_2_VALUE},
		{I40E_GL_SWR_PM_UP_THR, 0}, /* Compute value dynamically */
	};
	uint64_t reg;
	uint32_t i;
	int ret;

	for (i = 0; i < RTE_DIM(reg_table); i++) {
		if (reg_table[i].addr == I40E_GL_SWR_PM_UP_THR) {
			if (i40e_is_40G_device(hw->device_id)) /* For XL710 */
				reg_table[i].val =
					I40E_GL_SWR_PM_UP_THR_SF_VALUE;
			else /* For X710 */
				reg_table[i].val =
					I40E_GL_SWR_PM_UP_THR_EF_VALUE;
		}

		ret = i40e_aq_debug_read_register(hw, reg_table[i].addr,
							&reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to read from 0x%"PRIx32,
							reg_table[i].addr);
			break;
		}
		PMD_DRV_LOG(DEBUG, "Read from 0x%"PRIx32": 0x%"PRIx64,
						reg_table[i].addr, reg);
		if (reg == reg_table[i].val)
			continue;

		ret = i40e_aq_debug_write_register(hw, reg_table[i].addr,
						reg_table[i].val, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to write 0x%"PRIx64" to the "
				"address of 0x%"PRIx32, reg_table[i].val,
							reg_table[i].addr);
			break;
		}
		PMD_DRV_LOG(DEBUG, "Write 0x%"PRIx64" to the address of "
			"0x%"PRIx32, reg_table[i].val, reg_table[i].addr);
	}
}

#define I40E_VSI_TSR(_i)            (0x00050800 + ((_i) * 4))
#define I40E_VSI_TSR_QINQ_CONFIG    0xc030
#define I40E_VSI_L2TAGSTXVALID(_i)  (0x00042800 + ((_i) * 4))
#define I40E_VSI_L2TAGSTXVALID_QINQ 0xab
static int
i40e_config_qinq(struct i40e_hw *hw, struct i40e_vsi *vsi)
{
	uint32_t reg;
	int ret;

	if (vsi->vsi_id >= I40E_MAX_NUM_VSIS) {
		PMD_DRV_LOG(ERR, "VSI ID exceeds the maximum");
		return -EINVAL;
	}

	/* Configure for double VLAN RX stripping */
	reg = I40E_READ_REG(hw, I40E_VSI_TSR(vsi->vsi_id));
	if ((reg & I40E_VSI_TSR_QINQ_CONFIG) != I40E_VSI_TSR_QINQ_CONFIG) {
		reg |= I40E_VSI_TSR_QINQ_CONFIG;
		ret = i40e_aq_debug_write_register(hw,
						   I40E_VSI_TSR(vsi->vsi_id),
						   reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to update VSI_TSR[%d]",
				    vsi->vsi_id);
			return I40E_ERR_CONFIG;
		}
	}

	/* Configure for double VLAN TX insertion */
	reg = I40E_READ_REG(hw, I40E_VSI_L2TAGSTXVALID(vsi->vsi_id));
	if ((reg & 0xff) != I40E_VSI_L2TAGSTXVALID_QINQ) {
		reg = I40E_VSI_L2TAGSTXVALID_QINQ;
		ret = i40e_aq_debug_write_register(hw,
						   I40E_VSI_L2TAGSTXVALID(
						   vsi->vsi_id), reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to update "
				"VSI_L2TAGSTXVALID[%d]", vsi->vsi_id);
			return I40E_ERR_CONFIG;
		}
	}

	return 0;
}

/**
 * i40e_aq_add_mirror_rule
 * @hw: pointer to the hardware structure
 * @seid: VEB seid to add mirror rule to
 * @dst_id: destination vsi seid
 * @entries: Buffer which contains the entities to be mirrored
 * @count: number of entities contained in the buffer
 * @rule_id:the rule_id of the rule to be added
 *
 * Add a mirror rule for a given veb.
 *
 **/
static enum i40e_status_code
i40e_aq_add_mirror_rule(struct i40e_hw *hw,
			uint16_t seid, uint16_t dst_id,
			uint16_t rule_type, uint16_t *entries,
			uint16_t count, uint16_t *rule_id)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_delete_mirror_rule cmd;
	struct i40e_aqc_add_delete_mirror_rule_completion *resp =
		(struct i40e_aqc_add_delete_mirror_rule_completion *)
		&desc.params.raw;
	uint16_t buff_len;
	enum i40e_status_code status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_add_mirror_rule);
	memset(&cmd, 0, sizeof(cmd));

	buff_len = sizeof(uint16_t) * count;
	desc.datalen = rte_cpu_to_le_16(buff_len);
	if (buff_len > 0)
		desc.flags |= rte_cpu_to_le_16(
			(uint16_t)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd.rule_type = rte_cpu_to_le_16(rule_type <<
				I40E_AQC_MIRROR_RULE_TYPE_SHIFT);
	cmd.num_entries = rte_cpu_to_le_16(count);
	cmd.seid = rte_cpu_to_le_16(seid);
	cmd.destination = rte_cpu_to_le_16(dst_id);

	rte_memcpy(&desc.params.raw, &cmd, sizeof(cmd));
	status = i40e_asq_send_command(hw, &desc, entries, buff_len, NULL);
	PMD_DRV_LOG(INFO, "i40e_aq_add_mirror_rule, aq_status %d,"
			 "rule_id = %u"
			 " mirror_rules_used = %u, mirror_rules_free = %u,",
			 hw->aq.asq_last_status, resp->rule_id,
			 resp->mirror_rules_used, resp->mirror_rules_free);
	*rule_id = rte_le_to_cpu_16(resp->rule_id);

	return status;
}

/**
 * i40e_aq_del_mirror_rule
 * @hw: pointer to the hardware structure
 * @seid: VEB seid to add mirror rule to
 * @entries: Buffer which contains the entities to be mirrored
 * @count: number of entities contained in the buffer
 * @rule_id:the rule_id of the rule to be delete
 *
 * Delete a mirror rule for a given veb.
 *
 **/
static enum i40e_status_code
i40e_aq_del_mirror_rule(struct i40e_hw *hw,
		uint16_t seid, uint16_t rule_type, uint16_t *entries,
		uint16_t count, uint16_t rule_id)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_delete_mirror_rule cmd;
	uint16_t buff_len = 0;
	enum i40e_status_code status;
	void *buff = NULL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_delete_mirror_rule);
	memset(&cmd, 0, sizeof(cmd));
	if (rule_type == I40E_AQC_MIRROR_RULE_TYPE_VLAN) {
		desc.flags |= rte_cpu_to_le_16((uint16_t)(I40E_AQ_FLAG_BUF |
							  I40E_AQ_FLAG_RD));
		cmd.num_entries = count;
		buff_len = sizeof(uint16_t) * count;
		desc.datalen = rte_cpu_to_le_16(buff_len);
		buff = (void *)entries;
	} else
		/* rule id is filled in destination field for deleting mirror rule */
		cmd.destination = rte_cpu_to_le_16(rule_id);

	cmd.rule_type = rte_cpu_to_le_16(rule_type <<
				I40E_AQC_MIRROR_RULE_TYPE_SHIFT);
	cmd.seid = rte_cpu_to_le_16(seid);

	rte_memcpy(&desc.params.raw, &cmd, sizeof(cmd));
	status = i40e_asq_send_command(hw, &desc, buff, buff_len, NULL);

	return status;
}

/**
 * i40e_mirror_rule_set
 * @dev: pointer to the hardware structure
 * @mirror_conf: mirror rule info
 * @sw_id: mirror rule's sw_id
 * @on: enable/disable
 *
 * set a mirror rule.
 *
 **/
static int
i40e_mirror_rule_set(struct rte_eth_dev *dev,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t sw_id, uint8_t on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_mirror_rule *it, *mirr_rule = NULL;
	struct i40e_mirror_rule *parent = NULL;
	uint16_t seid, dst_seid, rule_id;
	uint16_t i, j = 0;
	int ret;

	PMD_DRV_LOG(DEBUG, "i40e_mirror_rule_set: sw_id = %d.", sw_id);

	if (pf->main_vsi->veb == NULL || pf->vfs == NULL) {
		PMD_DRV_LOG(ERR, "mirror rule can not be configured"
			" without veb or vfs.");
		return -ENOSYS;
	}
	if (pf->nb_mirror_rule > I40E_MAX_MIRROR_RULES) {
		PMD_DRV_LOG(ERR, "mirror table is full.");
		return -ENOSPC;
	}
	if (mirror_conf->dst_pool > pf->vf_num) {
		PMD_DRV_LOG(ERR, "invalid destination pool %u.",
				 mirror_conf->dst_pool);
		return -EINVAL;
	}

	seid = pf->main_vsi->veb->seid;

	TAILQ_FOREACH(it, &pf->mirror_list, rules) {
		if (sw_id <= it->index) {
			mirr_rule = it;
			break;
		}
		parent = it;
	}
	if (mirr_rule && sw_id == mirr_rule->index) {
		if (on) {
			PMD_DRV_LOG(ERR, "mirror rule exists.");
			return -EEXIST;
		} else {
			ret = i40e_aq_del_mirror_rule(hw, seid,
					mirr_rule->rule_type,
					mirr_rule->entries,
					mirr_rule->num_entries, mirr_rule->id);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "failed to remove mirror rule:"
						   " ret = %d, aq_err = %d.",
						   ret, hw->aq.asq_last_status);
				return -ENOSYS;
			}
			TAILQ_REMOVE(&pf->mirror_list, mirr_rule, rules);
			rte_free(mirr_rule);
			pf->nb_mirror_rule--;
			return 0;
		}
	} else if (!on) {
		PMD_DRV_LOG(ERR, "mirror rule doesn't exist.");
		return -ENOENT;
	}

	mirr_rule = rte_zmalloc("i40e_mirror_rule",
				sizeof(struct i40e_mirror_rule) , 0);
	if (!mirr_rule) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}
	switch (mirror_conf->rule_type) {
	case ETH_MIRROR_VLAN:
		for (i = 0, j = 0; i < ETH_MIRROR_MAX_VLANS; i++) {
			if (mirror_conf->vlan.vlan_mask & (1ULL << i)) {
				mirr_rule->entries[j] =
					mirror_conf->vlan.vlan_id[i];
				j++;
			}
		}
		if (j == 0) {
			PMD_DRV_LOG(ERR, "vlan is not specified.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_VLAN;
		break;
	case ETH_MIRROR_VIRTUAL_POOL_UP:
	case ETH_MIRROR_VIRTUAL_POOL_DOWN:
		/* check if the specified pool bit is out of range */
		if (mirror_conf->pool_mask > (uint64_t)(1ULL << (pf->vf_num + 1))) {
			PMD_DRV_LOG(ERR, "pool mask is out of range.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		for (i = 0, j = 0; i < pf->vf_num; i++) {
			if (mirror_conf->pool_mask & (1ULL << i)) {
				mirr_rule->entries[j] = pf->vfs[i].vsi->seid;
				j++;
			}
		}
		if (mirror_conf->pool_mask & (1ULL << pf->vf_num)) {
			/* add pf vsi to entries */
			mirr_rule->entries[j] = pf->main_vsi_seid;
			j++;
		}
		if (j == 0) {
			PMD_DRV_LOG(ERR, "pool is not specified.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		/* egress and ingress in aq commands means from switch but not port */
		mirr_rule->rule_type =
			(mirror_conf->rule_type == ETH_MIRROR_VIRTUAL_POOL_UP) ?
			I40E_AQC_MIRROR_RULE_TYPE_VPORT_EGRESS :
			I40E_AQC_MIRROR_RULE_TYPE_VPORT_INGRESS;
		break;
	case ETH_MIRROR_UPLINK_PORT:
		/* egress and ingress in aq commands means from switch but not port*/
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_ALL_EGRESS;
		break;
	case ETH_MIRROR_DOWNLINK_PORT:
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_ALL_INGRESS;
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported mirror type %d.",
			mirror_conf->rule_type);
		rte_free(mirr_rule);
		return -EINVAL;
	}

	/* If the dst_pool is equal to vf_num, consider it as PF */
	if (mirror_conf->dst_pool == pf->vf_num)
		dst_seid = pf->main_vsi_seid;
	else
		dst_seid = pf->vfs[mirror_conf->dst_pool].vsi->seid;

	ret = i40e_aq_add_mirror_rule(hw, seid, dst_seid,
				      mirr_rule->rule_type, mirr_rule->entries,
				      j, &rule_id);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "failed to add mirror rule:"
				   " ret = %d, aq_err = %d.",
				   ret, hw->aq.asq_last_status);
		rte_free(mirr_rule);
		return -ENOSYS;
	}

	mirr_rule->index = sw_id;
	mirr_rule->num_entries = j;
	mirr_rule->id = rule_id;
	mirr_rule->dst_vsi_seid = dst_seid;

	if (parent)
		TAILQ_INSERT_AFTER(&pf->mirror_list, parent, mirr_rule, rules);
	else
		TAILQ_INSERT_HEAD(&pf->mirror_list, mirr_rule, rules);

	pf->nb_mirror_rule++;
	return 0;
}

/**
 * i40e_mirror_rule_reset
 * @dev: pointer to the device
 * @sw_id: mirror rule's sw_id
 *
 * reset a mirror rule.
 *
 **/
static int
i40e_mirror_rule_reset(struct rte_eth_dev *dev, uint8_t sw_id)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_mirror_rule *it, *mirr_rule = NULL;
	uint16_t seid;
	int ret;

	PMD_DRV_LOG(DEBUG, "i40e_mirror_rule_reset: sw_id = %d.", sw_id);

	seid = pf->main_vsi->veb->seid;

	TAILQ_FOREACH(it, &pf->mirror_list, rules) {
		if (sw_id == it->index) {
			mirr_rule = it;
			break;
		}
	}
	if (mirr_rule) {
		ret = i40e_aq_del_mirror_rule(hw, seid,
				mirr_rule->rule_type,
				mirr_rule->entries,
				mirr_rule->num_entries, mirr_rule->id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "failed to remove mirror rule:"
					   " status = %d, aq_err = %d.",
					   ret, hw->aq.asq_last_status);
			return -ENOSYS;
		}
		TAILQ_REMOVE(&pf->mirror_list, mirr_rule, rules);
		rte_free(mirr_rule);
		pf->nb_mirror_rule--;
	} else {
		PMD_DRV_LOG(ERR, "mirror rule doesn't exist.");
		return -ENOENT;
	}
	return 0;
}

static int
i40e_timesync_enable(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link *link = &dev->data->dev_link;
	uint32_t tsync_ctl_l;
	uint32_t tsync_ctl_h;
	uint32_t tsync_inc_l;
	uint32_t tsync_inc_h;

	switch (link->link_speed) {
	case ETH_LINK_SPEED_40G:
		tsync_inc_l = I40E_PTP_40GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_40GB_INCVAL >> 32;
		break;
	case ETH_LINK_SPEED_10G:
		tsync_inc_l = I40E_PTP_10GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_10GB_INCVAL >> 32;
		break;
	case ETH_LINK_SPEED_1000:
		tsync_inc_l = I40E_PTP_1GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_1GB_INCVAL >> 32;
		break;
	default:
		tsync_inc_l = 0x0;
		tsync_inc_h = 0x0;
	}

	/* Clear timesync registers. */
	I40E_READ_REG(hw, I40E_PRTTSYN_STAT_0);
	I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_H);
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(0));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(1));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(2));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(3));
	I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_H);

	/* Set the timesync increment value. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_L, tsync_inc_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_H, tsync_inc_h);

	/* Enable timestamping of PTP packets. */
	tsync_ctl_l = I40E_READ_REG(hw, I40E_PRTTSYN_CTL0);
	tsync_ctl_l |= I40E_PRTTSYN_TSYNENA;

	tsync_ctl_h = I40E_READ_REG(hw, I40E_PRTTSYN_CTL1);
	tsync_ctl_h |= I40E_PRTTSYN_TSYNENA;
	tsync_ctl_h |= I40E_PRTTSYN_TSYNTYPE;

	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL0, tsync_ctl_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL1, tsync_ctl_h);

	return 0;
}

static int
i40e_timesync_disable(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t tsync_ctl_l;
	uint32_t tsync_ctl_h;

	/* Disable timestamping of transmitted PTP packets. */
	tsync_ctl_l = I40E_READ_REG(hw, I40E_PRTTSYN_CTL0);
	tsync_ctl_l &= ~I40E_PRTTSYN_TSYNENA;

	tsync_ctl_h = I40E_READ_REG(hw, I40E_PRTTSYN_CTL1);
	tsync_ctl_h &= ~I40E_PRTTSYN_TSYNENA;

	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL0, tsync_ctl_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL1, tsync_ctl_h);

	/* Set the timesync increment value. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_L, 0x0);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_H, 0x0);

	return 0;
}

static int
i40e_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp, uint32_t flags)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t sync_status;
	uint32_t rx_stmpl;
	uint32_t rx_stmph;
	uint32_t index = flags & 0x03;

	sync_status = I40E_READ_REG(hw, I40E_PRTTSYN_STAT_1);
	if ((sync_status & (1 << index)) == 0)
		return -EINVAL;

	rx_stmpl = I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(index));
	rx_stmph = I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(index));

	timestamp->tv_sec = (uint64_t)(((uint64_t)rx_stmph << 32) | rx_stmpl);
	timestamp->tv_nsec = 0;

	return  0;
}

static int
i40e_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t sync_status;
	uint32_t tx_stmpl;
	uint32_t tx_stmph;

	sync_status = I40E_READ_REG(hw, I40E_PRTTSYN_STAT_0);
	if ((sync_status & I40E_PRTTSYN_STAT_0_TXTIME_MASK) == 0)
		return -EINVAL;

	tx_stmpl = I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_L);
	tx_stmph = I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_H);

	timestamp->tv_sec = (uint64_t)(((uint64_t)tx_stmph << 32) | tx_stmpl);
	timestamp->tv_nsec = 0;

	return  0;
}
