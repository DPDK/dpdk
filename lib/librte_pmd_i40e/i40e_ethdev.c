/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <rte_dev.h>

#include "i40e_logs.h"
#include "i40e/i40e_register_x710_int.h"
#include "i40e/i40e_prototype.h"
#include "i40e/i40e_adminq_cmd.h"
#include "i40e/i40e_type.h"
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

/* Bit shift and mask */
#define I40E_16_BIT_SHIFT 16
#define I40E_16_BIT_MASK  0xFFFF
#define I40E_32_BIT_SHIFT 32
#define I40E_32_BIT_MASK  0xFFFFFFFF
#define I40E_48_BIT_SHIFT 48
#define I40E_48_BIT_MASK  0xFFFFFFFFFFFFULL

/* Default queue interrupt throttling time in microseconds*/
#define I40E_ITR_INDEX_DEFAULT          0
#define I40E_QUEUE_ITR_INTERVAL_DEFAULT 32 /* 32 us */
#define I40E_QUEUE_ITR_INTERVAL_MAX     8160 /* 8160 us */

#define I40E_PRE_TX_Q_CFG_WAIT_US       10 /* 10 us */

#define I40E_RSS_OFFLOAD_ALL ( \
	ETH_RSS_NONF_IPV4_UDP | \
	ETH_RSS_NONF_IPV4_TCP | \
	ETH_RSS_NONF_IPV4_SCTP | \
	ETH_RSS_NONF_IPV4_OTHER | \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONF_IPV6_UDP | \
	ETH_RSS_NONF_IPV6_TCP | \
	ETH_RSS_NONF_IPV6_SCTP | \
	ETH_RSS_NONF_IPV6_OTHER | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_L2_PAYLOAD)

/* All bits of RSS hash enable */
#define I40E_RSS_HENA_ALL ( \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_RX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD))

static int eth_i40e_dev_init(\
			__attribute__((unused)) struct eth_driver *eth_drv,
			struct rte_eth_dev *eth_dev);
static int i40e_dev_configure(struct rte_eth_dev *dev);
static int i40e_dev_start(struct rte_eth_dev *dev);
static void i40e_dev_stop(struct rte_eth_dev *dev);
static void i40e_dev_close(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_disable(struct rte_eth_dev *dev);
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
				    struct rte_eth_rss_reta *reta_conf);
static int i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta *reta_conf);

static int i40e_get_cap(struct i40e_hw *hw);
static int i40e_pf_parameter_init(struct rte_eth_dev *dev);
static int i40e_pf_setup(struct i40e_pf *pf);
static int i40e_vsi_init(struct i40e_vsi *vsi);
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

/* Default hash key buffer for RSS */
static uint32_t rss_key_default[I40E_PFQF_HKEY_MAX_INDEX + 1];

static struct rte_pci_id pci_id_i40e_map[] = {
#define RTE_PCI_DEV_ID_DECL_I40E(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
{ .vendor_id = 0, /* sentinel */ },
};

static struct eth_dev_ops i40e_eth_dev_ops = {
	.dev_configure                = i40e_dev_configure,
	.dev_start                    = i40e_dev_start,
	.dev_stop                     = i40e_dev_stop,
	.dev_close                    = i40e_dev_close,
	.promiscuous_enable           = i40e_dev_promiscuous_enable,
	.promiscuous_disable          = i40e_dev_promiscuous_disable,
	.allmulticast_enable          = i40e_dev_allmulticast_enable,
	.allmulticast_disable         = i40e_dev_allmulticast_disable,
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
};

static struct eth_driver rte_i40e_pmd = {
	{
		.name = "rte_i40e_pmd",
		.id_table = pci_id_i40e_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_i40e_dev_init,
	.dev_private_size = sizeof(struct i40e_adapter),
};

static inline int
i40e_prev_power_of_2(int n)
{
       int p = n;

       --p;
       p |= p >> 1;
       p |= p >> 2;
       p |= p >> 4;
       p |= p >> 8;
       p |= p >> 16;
       if (p == (n - 1))
               return n;
       p >>= 1;

       return ++p;
}

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

static int
eth_i40e_dev_init(__rte_unused struct eth_driver *eth_drv,
                  struct rte_eth_dev *dev)
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
					"as address is NULL\n");
		return -ENODEV;
	}

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;

	/* Make sure all is clean before doing PF reset */
	i40e_clear_hw(hw);

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

	/* Initialize the parameters for adminq */
	i40e_init_adminq_parameter(hw);
	ret = i40e_init_adminq(hw);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to init adminq: %d", ret);
		return -EIO;
	}
	PMD_INIT_LOG(INFO, "FW %d.%d API %d.%d NVM "
			"%02d.%02d.%02d eetrack %04x\n",
			hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
			hw->aq.api_maj_ver, hw->aq.api_min_ver,
			((hw->nvm.version >> 12) & 0xf),
			((hw->nvm.version >> 4) & 0xff),
			(hw->nvm.version & 0xf), hw->nvm.eetrack);

	/* Disable LLDP */
	ret = i40e_aq_stop_lldp(hw, true, NULL);
	if (ret != I40E_SUCCESS) /* Its failure can be ignored */
		PMD_INIT_LOG(INFO, "Failed to stop lldp\n");

	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);

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
		PMD_INIT_LOG(ERR, "Failed to init queue pool\n");
		goto err_qp_pool_init;
	}
	ret = i40e_res_pool_init(&pf->msix_pool, 1,
				hw->func_caps.num_msix_vectors - 1);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to init MSIX pool\n");
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
		goto err_get_mac_addr;
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

	return 0;

err_setup_pf_switch:
	rte_free(pf->main_vsi);
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
i40e_dev_configure(struct rte_eth_dev *dev)
{
	return i40e_dev_init_vlan(dev);
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
	uint16_t interval = i40e_calc_itr_interval(RTE_LIBRTE_I40E_ITR_INTERVAL);
	int i;

	for (i = 0; i < vsi->nb_qps; i++)
		I40E_WRITE_REG(hw, I40E_QINT_TQCTL(vsi->base_queue + i), 0);

	/* Bind all RX queues to allocated MSIX interrupt */
	for (i = 0; i < vsi->nb_qps; i++) {
		val = (msix_vect << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
			(interval << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
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
		I40E_WRITE_REG(hw, I40E_PFINT_LNKLSTN(msix_vect - 1),
			(vsi->base_queue << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
			(0x0 << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));

		I40E_WRITE_REG(hw, I40E_PFINT_ITRN(I40E_ITR_INDEX_DEFAULT,
				msix_vect - 1), interval);

		/* Disable auto-mask on enabling of all none-zero  interrupt */
		I40E_WRITE_REG(hw, I40E_GLINT_CTL,
				I40E_GLINT_CTL_DIS_AUTOMASK_N_MASK);
	}
	else {
		uint32_t reg;
		/* num_msix_vectors_vf needs to minus irq0 */
		reg = (hw->func_caps.num_msix_vectors_vf - 1) *
			vsi->user_param + (msix_vect - 1);

		I40E_WRITE_REG(hw, I40E_VPINT_LNKLSTN(reg),
			(vsi->base_queue << I40E_VPINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
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

static int
i40e_dev_start(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	/* Initialize VSI */
	ret = i40e_vsi_init(vsi);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to init VSI\n");
		goto err_up;
	}

	/* Map queues with MSIX interrupt */
	i40e_vsi_queues_bind_intr(vsi);
	i40e_vsi_enable_queues_intr(vsi);

	/* Enable all queues which have been configured */
	ret = i40e_vsi_switch_queues(vsi, TRUE);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to enable VSI\n");
		goto err_up;
	}

	/* Enable receiving broadcast packets */
	if ((vsi->type == I40E_VSI_MAIN) || (vsi->type == I40E_VSI_VMDQ2)) {
		ret = i40e_aq_set_vsi_broadcast(hw, vsi->seid, true, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(INFO, "fail to set vsi broadcast\n");
	}

	return I40E_SUCCESS;

err_up:
	i40e_vsi_switch_queues(vsi, FALSE);
	i40e_dev_clear_queues(dev);

	return ret;
}

static void
i40e_dev_stop(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;

	/* Disable all queues */
	i40e_vsi_switch_queues(vsi, FALSE);

	/* Clear all queues and release memory */
	i40e_dev_clear_queues(dev);

	/* un-map queues with interrupt registers */
	i40e_vsi_disable_queues_intr(vsi);
	i40e_vsi_queues_unbind_intr(vsi);
}

static void
i40e_dev_close(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	PMD_INIT_FUNC_TRACE();

	i40e_dev_stop(dev);

	/* Disable interrupt */
	i40e_pf_disable_irq0(hw);
	rte_intr_disable(&(dev->pci_dev->intr_handle));

	/* shutdown and destroy the HMC */
	i40e_shutdown_lan_hmc(hw);

	/* release all the existing VSIs and VEBs */
	i40e_vsi_release(pf->main_vsi);

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
		PMD_DRV_LOG(ERR, "Failed to enable unicast promiscuous\n");
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
		PMD_DRV_LOG(ERR, "Failed to disable unicast promiscuous\n");
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
		PMD_DRV_LOG(ERR, "Failed to enable multicast promiscuous\n");
}

static void
i40e_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw,
				vsi->seid, FALSE, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable multicast promiscuous\n");
}

int
i40e_dev_link_update(struct rte_eth_dev *dev,
		     __rte_unused int wait_to_complete)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_link_status link_status;
	struct rte_eth_link link, old;
	int status;

	memset(&link, 0, sizeof(link));
	memset(&old, 0, sizeof(old));
	memset(&link_status, 0, sizeof(link_status));
	rte_i40e_dev_atomic_read_link_status(dev, &old);

	/* Get link status information from hardware */
	status = i40e_aq_get_link_info(hw, false, &link_status, NULL);
	if (status != I40E_SUCCESS) {
		link.link_speed = ETH_LINK_SPEED_100;
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
		PMD_DRV_LOG(ERR, "Failed to get link info\n");
		goto out;
	}

	link.link_status = link_status.link_info & I40E_AQ_LINK_UP;

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

#ifdef RTE_LIBRTE_I40E_DEBUG_DRIVER
	printf("***************** VSI[%u] stats start *******************\n",
								vsi->vsi_id);
	printf("rx_bytes:            %lu\n", nes->rx_bytes);
	printf("rx_unicast:          %lu\n", nes->rx_unicast);
	printf("rx_multicast:        %lu\n", nes->rx_multicast);
	printf("rx_broadcast:        %lu\n", nes->rx_broadcast);
	printf("rx_discards:         %lu\n", nes->rx_discards);
	printf("rx_unknown_protocol: %lu\n", nes->rx_unknown_protocol);
	printf("tx_bytes:            %lu\n", nes->tx_bytes);
	printf("tx_unicast:          %lu\n", nes->tx_unicast);
	printf("tx_multicast:        %lu\n", nes->tx_multicast);
	printf("tx_broadcast:        %lu\n", nes->tx_broadcast);
	printf("tx_discards:         %lu\n", nes->tx_discards);
	printf("tx_errors:           %lu\n", nes->tx_errors);
	printf("***************** VSI[%u] stats end *******************\n",
								vsi->vsi_id);
#endif /* RTE_LIBRTE_I40E_DEBUG_DRIVER */
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
	i40e_stat_update_32(hw, I40E_GLPRT_TDPC(hw->port),
			    pf->offset_loaded, &os->eth.tx_discards,
			    &ns->eth.tx_discards);
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
	/* GLPRT_MSPDC not supported */
	/* GLPRT_XEC not supported */

	pf->offset_loaded = true;

	stats->ipackets = ns->eth.rx_unicast + ns->eth.rx_multicast +
						ns->eth.rx_broadcast;
	stats->opackets = ns->eth.tx_unicast + ns->eth.tx_multicast +
						ns->eth.tx_broadcast;
	stats->ibytes   = ns->eth.rx_bytes;
	stats->obytes   = ns->eth.tx_bytes;
	stats->oerrors  = ns->eth.tx_errors;
	stats->imcasts  = ns->eth.rx_multicast;

	if (pf->main_vsi)
		i40e_update_vsi_stats(pf->main_vsi);

#ifdef RTE_LIBRTE_I40E_DEBUG_DRIVER
	printf("***************** PF stats start *******************\n");
	printf("rx_bytes:            %lu\n", ns->eth.rx_bytes);
	printf("rx_unicast:          %lu\n", ns->eth.rx_unicast);
	printf("rx_multicast:        %lu\n", ns->eth.rx_multicast);
	printf("rx_broadcast:        %lu\n", ns->eth.rx_broadcast);
	printf("rx_discards:         %lu\n", ns->eth.rx_discards);
	printf("rx_unknown_protocol: %lu\n", ns->eth.rx_unknown_protocol);
	printf("tx_bytes:            %lu\n", ns->eth.tx_bytes);
	printf("tx_unicast:          %lu\n", ns->eth.tx_unicast);
	printf("tx_multicast:        %lu\n", ns->eth.tx_multicast);
	printf("tx_broadcast:        %lu\n", ns->eth.tx_broadcast);
	printf("tx_discards:         %lu\n", ns->eth.tx_discards);
	printf("tx_errors:           %lu\n", ns->eth.tx_errors);

	printf("tx_dropped_link_down:     %lu\n", ns->tx_dropped_link_down);
	printf("crc_errors:               %lu\n", ns->crc_errors);
	printf("illegal_bytes:            %lu\n", ns->illegal_bytes);
	printf("error_bytes:              %lu\n", ns->error_bytes);
	printf("mac_local_faults:         %lu\n", ns->mac_local_faults);
	printf("mac_remote_faults:        %lu\n", ns->mac_remote_faults);
	printf("rx_length_errors:         %lu\n", ns->rx_length_errors);
	printf("link_xon_rx:              %lu\n", ns->link_xon_rx);
	printf("link_xoff_rx:             %lu\n", ns->link_xoff_rx);
	for (i = 0; i < 8; i++) {
		printf("priority_xon_rx[%d]:      %lu\n",
				i, ns->priority_xon_rx[i]);
		printf("priority_xoff_rx[%d]:     %lu\n",
				i, ns->priority_xoff_rx[i]);
	}
	printf("link_xon_tx:              %lu\n", ns->link_xon_tx);
	printf("link_xoff_tx:             %lu\n", ns->link_xoff_tx);
	for (i = 0; i < 8; i++) {
		printf("priority_xon_tx[%d]:      %lu\n",
				i, ns->priority_xon_tx[i]);
		printf("priority_xoff_tx[%d]:     %lu\n",
				i, ns->priority_xoff_tx[i]);
		printf("priority_xon_2_xoff[%d]:  %lu\n",
				i, ns->priority_xon_2_xoff[i]);
	}
	printf("rx_size_64:               %lu\n", ns->rx_size_64);
	printf("rx_size_127:              %lu\n", ns->rx_size_127);
	printf("rx_size_255:              %lu\n", ns->rx_size_255);
	printf("rx_size_511:              %lu\n", ns->rx_size_511);
	printf("rx_size_1023:             %lu\n", ns->rx_size_1023);
	printf("rx_size_1522:             %lu\n", ns->rx_size_1522);
	printf("rx_size_big:              %lu\n", ns->rx_size_big);
	printf("rx_undersize:             %lu\n", ns->rx_undersize);
	printf("rx_fragments:             %lu\n", ns->rx_fragments);
	printf("rx_oversize:              %lu\n", ns->rx_oversize);
	printf("rx_jabber:                %lu\n", ns->rx_jabber);
	printf("tx_size_64:               %lu\n", ns->tx_size_64);
	printf("tx_size_127:              %lu\n", ns->tx_size_127);
	printf("tx_size_255:              %lu\n", ns->tx_size_255);
	printf("tx_size_511:              %lu\n", ns->tx_size_511);
	printf("tx_size_1023:             %lu\n", ns->tx_size_1023);
	printf("tx_size_1522:             %lu\n", ns->tx_size_1522);
	printf("tx_size_big:              %lu\n", ns->tx_size_big);
	printf("mac_short_packet_dropped: %lu\n",
			ns->mac_short_packet_dropped);
	printf("checksum_error:           %lu\n", ns->checksum_error);
	printf("***************** PF stats end ********************\n");
#endif /* RTE_LIBRTE_I40E_DEBUG_DRIVER */
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
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM;
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
		 __attribute__((unused)) uint32_t index,
		 __attribute__((unused)) uint32_t pool)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct ether_addr old_mac;
	int ret;

	if (!is_valid_assigned_ether_addr(mac_addr)) {
		PMD_DRV_LOG(ERR, "Invalid ethernet address\n");
		return;
	}

	if (is_same_ether_addr(mac_addr, &(pf->dev_addr))) {
		PMD_DRV_LOG(INFO, "Ignore adding permanent mac address\n");
		return;
	}

	/* Write mac address */
	ret = i40e_aq_mac_address_write(hw, I40E_AQC_WRITE_TYPE_LAA_ONLY,
					mac_addr->addr_bytes, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to write mac address\n");
		return;
	}

	(void)rte_memcpy(&old_mac, hw->mac.addr, ETHER_ADDR_LEN);
	(void)rte_memcpy(hw->mac.addr, mac_addr->addr_bytes,
			ETHER_ADDR_LEN);

	ret = i40e_vsi_add_mac(vsi, mac_addr);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter\n");
		return;
	}

	ether_addr_copy(mac_addr, &pf->dev_addr);
	i40e_vsi_delete_mac(vsi, &old_mac);
}

/* Remove a MAC address, and update filters */
static void
i40e_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = I40E_VSI_TO_DEV_DATA(vsi);
	struct ether_addr *macaddr;
	int ret;
	struct i40e_hw *hw =
		I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (index >= vsi->max_macaddrs)
		return;

	macaddr = &(data->mac_addrs[index]);
	if (!is_valid_assigned_ether_addr(macaddr))
		return;

	ret = i40e_aq_mac_address_write(hw, I40E_AQC_WRITE_TYPE_LAA_ONLY,
					hw->mac.perm_addr, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to write mac address\n");
		return;
	}

	(void)rte_memcpy(hw->mac.addr, hw->mac.perm_addr, ETHER_ADDR_LEN);

	ret = i40e_vsi_delete_mac(vsi, macaddr);
	if (ret != I40E_SUCCESS)
		return;

	/* Clear device address as it has been removed */
	if (is_same_ether_addr(&(pf->dev_addr), macaddr))
		memset(&pf->dev_addr, 0, sizeof(struct ether_addr));
}

static int
i40e_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta *reta_conf)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t lut, l;
	uint8_t i, j, mask, max = ETH_RSS_RETA_NUM_ENTRIES / 2;

	for (i = 0; i < ETH_RSS_RETA_NUM_ENTRIES; i += 4) {
		if (i < max)
			mask = (uint8_t)((reta_conf->mask_lo >> i) & 0xF);
		else
			mask = (uint8_t)((reta_conf->mask_hi >>
						(i - max)) & 0xF);

		if (!mask)
			continue;

		if (mask == 0xF)
			l = 0;
		else
			l = I40E_READ_REG(hw, I40E_PFQF_HLUT(i >> 2));

		for (j = 0, lut = 0; j < 4; j++) {
			if (mask & (0x1 << j))
				lut |= reta_conf->reta[i + j] << (8 * j);
			else
				lut |= l & (0xFF << (8 * j));
		}
		I40E_WRITE_REG(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}

	return 0;
}

static int
i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta *reta_conf)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t lut;
	uint8_t i, j, mask, max = ETH_RSS_RETA_NUM_ENTRIES / 2;

	for (i = 0; i < ETH_RSS_RETA_NUM_ENTRIES; i += 4) {
		if (i < max)
			mask = (uint8_t)((reta_conf->mask_lo >> i) & 0xF);
		else
			mask = (uint8_t)((reta_conf->mask_hi >>
						(i - max)) & 0xF);

		if (!mask)
			continue;

		lut = I40E_READ_REG(hw, I40E_PFQF_HLUT(i >> 2));
		for (j = 0; j < 4; j++) {
			if (mask & (0x1 << j))
				reta_conf->reta[i + j] =
					(uint8_t)((lut >> (8 * j)) & 0xFF);
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
	mz = rte_memzone_reserve_aligned(z_name, size, 0, 0, alignment);
	if (!mz)
		return I40E_ERR_NO_MEMORY;

	mem->id = id;
	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->phys_addr;

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
		PMD_DRV_LOG(ERR, "Failed to allocate memory\n");
		return I40E_ERR_NO_MEMORY;
	}

	/* Get, parse the capabilities and save it to hw */
	ret = i40e_aq_discover_capabilities(hw, buf, len, &size,
			i40e_aqc_opc_list_func_capabilities, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to discover capabilities\n");

	/* Free the temporary buffer after being used */
	rte_free(buf);

	return ret;
}

static int
i40e_pf_parameter_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t sum_queues = 0, sum_vsis;

	/* First check if FW support SRIOV */
	if (dev->pci_dev->max_vfs && !hw->func_caps.sr_iov_1_1) {
		PMD_INIT_LOG(ERR, "HW configuration doesn't support SRIOV\n");
		return -EINVAL;
	}

	pf->flags = I40E_FLAG_HEADER_SPLIT_DISABLED;
	pf->max_num_vsi = RTE_MIN(hw->func_caps.num_vsis, I40E_MAX_NUM_VSIS);
	PMD_INIT_LOG(INFO, "Max supported VSIs:%u\n", pf->max_num_vsi);
	/* Allocate queues for pf */
	if (hw->func_caps.rss) {
		pf->flags |= I40E_FLAG_RSS;
		pf->lan_nb_qps = RTE_MIN(hw->func_caps.num_tx_qp,
			(uint32_t)(1 << hw->func_caps.rss_table_entry_width));
		pf->lan_nb_qps = i40e_prev_power_of_2(pf->lan_nb_qps);
	} else
		pf->lan_nb_qps = 1;
	sum_queues = pf->lan_nb_qps;
	/* Default VSI is not counted in */
	sum_vsis = 0;
	PMD_INIT_LOG(INFO, "PF queue pairs:%u\n", pf->lan_nb_qps);

	if (hw->func_caps.sr_iov_1_1 && dev->pci_dev->max_vfs) {
		pf->flags |= I40E_FLAG_SRIOV;
		pf->vf_nb_qps = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VF;
		if (dev->pci_dev->max_vfs > hw->func_caps.num_vfs) {
			PMD_INIT_LOG(ERR, "Config VF number %u, "
				"max supported %u.\n", dev->pci_dev->max_vfs,
						hw->func_caps.num_vfs);
			return -EINVAL;
		}
		if (pf->vf_nb_qps > I40E_MAX_QP_NUM_PER_VF) {
			PMD_INIT_LOG(ERR, "FVL VF queue %u, "
				"max support %u queues.\n", pf->vf_nb_qps,
						I40E_MAX_QP_NUM_PER_VF);
			return -EINVAL;
		}
		pf->vf_num = dev->pci_dev->max_vfs;
		sum_queues += pf->vf_nb_qps * pf->vf_num;
		sum_vsis   += pf->vf_num;
		PMD_INIT_LOG(INFO, "Max VF num:%u each has queue pairs:%u\n",
						pf->vf_num, pf->vf_nb_qps);
	} else
		pf->vf_num = 0;

	if (hw->func_caps.vmdq) {
		pf->flags |= I40E_FLAG_VMDQ;
		pf->vmdq_nb_qps = I40E_DEFAULT_QP_NUM_VMDQ;
		sum_queues += pf->vmdq_nb_qps;
		sum_vsis += 1;
		PMD_INIT_LOG(INFO, "VMDQ queue pairs:%u\n", pf->vmdq_nb_qps);
	}

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
		PMD_INIT_LOG(ERR, "VSI/QUEUE setting can't be satisfied\n");
		PMD_INIT_LOG(ERR, "Max VSIs: %u, asked:%u\n",
				pf->max_num_vsi, sum_vsis);
		PMD_INIT_LOG(ERR, "Total queue pairs:%u, asked:%u\n",
				hw->func_caps.num_rx_qp, sum_queues);
		return -EINVAL;
	}

	/* Each VSI occupy 1 MSIX interrupt at least, plus IRQ0 for misc intr cause */
	if (sum_vsis > hw->func_caps.num_msix_vectors - 1) {
		PMD_INIT_LOG(ERR, "Too many VSIs(%u), MSIX intr(%u) not enough\n",
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
		PMD_DRV_LOG(ERR, "Failed to allocated memory\n");
		return -ENOMEM;
	}

	/* Get the switch configurations */
	ret = i40e_aq_get_switch_config(hw, switch_config,
		I40E_AQ_LARGE_BUF, &start_seid, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get switch configurations\n");
		goto fail;
	}
	num_reported = rte_le_to_cpu_16(switch_config->header.num_reported);
	if (num_reported != 1) { /* The number should be 1 */
		PMD_DRV_LOG(ERR, "Wrong number of switch config reported\n");
		goto fail;
	}

	/* Parse the switch configuration elements */
	element = &(switch_config->element[0]);
	if (element->element_type == I40E_SWITCH_ELEMENT_TYPE_VSI) {
		pf->mac_seid = rte_le_to_cpu_16(element->uplink_seid);
		pf->main_vsi_seid = rte_le_to_cpu_16(element->seid);
	} else
		PMD_DRV_LOG(INFO, "Unknown element type\n");

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
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
						"resource pool\n");
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
		PMD_DRV_LOG(ERR, "Invalid parameter\n");
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
		PMD_DRV_LOG(ERR, "Failed to find entry\n");
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
		PMD_DRV_LOG(ERR, "Invalid parameter\n");
		return -EINVAL;
	}

	if (pool->num_free < num) {
		PMD_DRV_LOG(ERR, "No resource. ask:%u, available:%u\n",
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
		PMD_DRV_LOG(ERR, "No valid entry found\n");
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
					"resource pool\n");
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
		PMD_DRV_LOG(ERR, "DCB is not enabled, "
				"only TC0 is supported\n");
		return -EINVAL;
	}

	if (!bitmap_is_subset(hw->func_caps.enabled_tcmap, enabled_tcmap)) {
		PMD_DRV_LOG(ERR, "Enabled TC map 0x%x not applicable to "
			"HW support 0x%x\n", hw->func_caps.enabled_tcmap,
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
		PMD_DRV_LOG(ERR, "invalid parameters\n");
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
		PMD_DRV_LOG(ERR, "Failed to update VSI params\n");

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
		PMD_DRV_LOG(ERR, "seid not valid\n");
		return -EINVAL;
	}

	memset(&tc_bw_data, 0, sizeof(tc_bw_data));
	tc_bw_data.tc_valid_bits = enabled_tcmap;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		tc_bw_data.tc_bw_credits[i] =
			(enabled_tcmap & (1 << i)) ? 1 : 0;

	ret = i40e_aq_config_vsi_tc_bw(hw, vsi->seid, &tc_bw_data, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to configure TC BW\n");
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
	qpnum_per_tc = i40e_prev_power_of_2(vsi->nb_qps / total_tc);
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
	info->valid_sections =
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
		PMD_DRV_LOG(ERR, "VEB still has VSI attached, can't remove\n");
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
			"associated VSI shouldn't null\n");
		return NULL;
	}
	hw = I40E_PF_TO_HW(pf);

	veb = rte_zmalloc("i40e_veb", sizeof(struct i40e_veb), 0);
	if (!veb) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for veb\n");
		goto fail;
	}

	veb->associate_vsi = vsi;
	TAILQ_INIT(&veb->head);
	veb->uplink_seid = vsi->uplink_seid;

	ret = i40e_aq_add_veb(hw, veb->uplink_seid, vsi->seid,
		I40E_DEFAULT_TCMAP, false, false, &veb->seid, NULL);

	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Add veb failed, aq_err: %d\n",
					hw->aq.asq_last_status);
		goto fail;
	}

	/* get statistics index */
	ret = i40e_aq_get_veb_parameters(hw, veb->seid, NULL, NULL,
				&veb->stats_idx, NULL, NULL, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Get veb statics index failed, aq_err: %d\n",
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
			PMD_DRV_LOG(ERR, "VSI's parent VSI is NULL\n");
			return I40E_ERR_PARAM;
		}
		TAILQ_REMOVE(&vsi->parent_vsi->veb->head,
				&vsi->sib_vsi_list, list);

		/* Remove all switch element of the VSI */
		ret = i40e_aq_delete_element(hw, vsi->seid, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR, "Failed to delete element\n");
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

		PMD_DRV_LOG(WARNING, "Cannot remove the default "
						"macvlan filter\n");
		/* It needs to add the permanent mac into mac list */
		f = rte_zmalloc("macv_filter", sizeof(*f), 0);
		if (f == NULL) {
			PMD_DRV_LOG(ERR, "failed to allocate memory\n");
			return I40E_ERR_NO_MEMORY;
		}
		(void)rte_memcpy(&f->macaddr.addr_bytes, hw->mac.perm_addr,
				ETH_ADDR_LEN);
		TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
		vsi->mac_num++;

		return ret;
	}

	return i40e_vsi_add_mac(vsi, (struct ether_addr *)(hw->mac.perm_addr));
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
		PMD_DRV_LOG(ERR, "VSI failed to get bandwidth "
			"configuration %u\n", hw->aq.asq_last_status);
		return ret;
	}

	memset(&ets_sla_config, 0, sizeof(ets_sla_config));
	ret = i40e_aq_query_vsi_ets_sla_config(hw, vsi->seid,
					&ets_sla_config, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "VSI failed to get TC bandwdith "
			"configuration %u\n", hw->aq.asq_last_status);
		return ret;
	}

	/* Not store the info yet, just print out */
	PMD_DRV_LOG(INFO, "VSI bw limit:%u\n", bw_config.port_bw_limit);
	PMD_DRV_LOG(INFO, "VSI max_bw:%u\n", bw_config.max_bw);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		PMD_DRV_LOG(INFO, "\tVSI TC%u:share credits %u\n", i,
					ets_sla_config.share_credits[i]);
		PMD_DRV_LOG(INFO, "\tVSI TC%u:credits %u\n", i,
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
	int ret;
	struct i40e_vsi_context ctxt;
	struct ether_addr broadcast =
		{.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

	if (type != I40E_VSI_MAIN && uplink_vsi == NULL) {
		PMD_DRV_LOG(ERR, "VSI setup failed, "
			"VSI link shouldn't be NULL\n");
		return NULL;
	}

	if (type == I40E_VSI_MAIN && uplink_vsi != NULL) {
		PMD_DRV_LOG(ERR, "VSI setup failed, MAIN VSI "
				"uplink VSI should be NULL\n");
		return NULL;
	}

	/* If uplink vsi didn't setup VEB, create one first */
	if (type != I40E_VSI_MAIN && uplink_vsi->veb == NULL) {
		uplink_vsi->veb = i40e_veb_setup(pf, uplink_vsi);

		if (NULL == uplink_vsi->veb) {
			PMD_DRV_LOG(ERR, "VEB setup failed\n");
			return NULL;
		}
	}

	vsi = rte_zmalloc("i40e_vsi", sizeof(struct i40e_vsi), 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for vsi\n");
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
	default:
		goto fail_mem;
	}
	ret = i40e_res_pool_alloc(&pf->qp_pool, vsi->nb_qps);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "VSI %d allocate queue failed %d",
				vsi->seid, ret);
		goto fail_mem;
	}
	vsi->base_queue = ret;

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
			PMD_DRV_LOG(ERR, "Failed to get VSI params\n");
			goto fail_msix_alloc;
		}
		(void)rte_memcpy(&vsi->info, &ctxt.info,
			sizeof(struct i40e_aqc_vsi_properties_data));
		vsi->vsi_id = ctxt.vsi_number;
		vsi->info.valid_sections = 0;

		/* Configure tc, enabled TC0 only */
		if (i40e_vsi_update_tc_bandwidth(vsi, I40E_DEFAULT_TCMAP) !=
			I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update TC bandwidth\n");
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
					"TC queue mapping\n");
			goto fail_msix_alloc;
		}
		ctxt.seid = vsi->seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.vf_num = 0;

		/* Update VSI parameters */
		ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update VSI params\n");
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

		/* Configure switch ID */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID);
		ctxt.info.switch_id =
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		/* Configure port/vlan */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to configure "
					"TC queue mapping\n");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
		/**
		 * Since VSI is not created yet, only configure parameter,
		 * will add vsi below.
		 */
	}
	else {
		PMD_DRV_LOG(ERR, "VSI: Not support other type VSI yet\n");
		goto fail_msix_alloc;
	}

	if (vsi->type != I40E_VSI_MAIN) {
		ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
		if (ret) {
			PMD_DRV_LOG(ERR, "add vsi failed, aq_err=%d\n",
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
	ret = i40e_vsi_add_mac(vsi, &broadcast);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter\n");
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
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan stripping\n",
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
		PMD_DRV_LOG(INFO, "Failed to update VSI params\n");

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
		PMD_DRV_LOG(ERR, "Failed to get link status information\n");
		goto write_reg; /* Disable flow control */
	}

	an_info = hw->phy.link_info.an_info;
	if (!(an_info & I40E_AQ_AN_COMPLETED)) {
		PMD_DRV_LOG(INFO, "Link auto negotiation not completed\n");
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
	struct rte_eth_dev_data *dev_data = pf->dev_data;
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

	/* VSI setup */
	vsi = i40e_vsi_setup(pf, I40E_VSI_MAIN, NULL, 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Setup of main vsi failed");
		return I40E_ERR_NOT_READY;
	}
	pf->main_vsi = vsi;
	dev_data->nb_rx_queues = vsi->nb_qps;
	dev_data->nb_tx_queues = vsi->nb_qps;

	/* Configure filter control */
	memset(&settings, 0, sizeof(settings));
	settings.hash_lut_size = I40E_HASH_LUT_SIZE_128;
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
		PMD_DRV_LOG(ERR, "Failed to %s tx queue[%u]\n",
			(on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}

/* Swith on or off the tx queues */
static int
i40e_vsi_switch_tx_queues(struct i40e_vsi *vsi, bool on)
{
	struct rte_eth_dev_data *dev_data = I40E_VSI_TO_DEV_DATA(vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_tx_queue *txq;
	uint16_t i, pf_q;
	int ret;

	pf_q = vsi->base_queue;
	for (i = 0; i < dev_data->nb_tx_queues; i++, pf_q++) {
		txq = dev_data->tx_queues[i];
		if (!txq->q_set)
			continue; /* Queue not configured */
		ret = i40e_switch_tx_queue(hw, pf_q, on);
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
		PMD_DRV_LOG(ERR, "Failed to %s rx queue[%u]\n",
			(on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}
/* Switch on or off the rx queues */
static int
i40e_vsi_switch_rx_queues(struct i40e_vsi *vsi, bool on)
{
	struct rte_eth_dev_data *dev_data = I40E_VSI_TO_DEV_DATA(vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_rx_queue *rxq;
	uint16_t i, pf_q;
	int ret;

	pf_q = vsi->base_queue;
	for (i = 0; i < dev_data->nb_rx_queues; i++, pf_q++) {
		rxq = dev_data->rx_queues[i];
		if (!rxq->q_set)
			continue; /* Queue not configured */
		ret = i40e_switch_rx_queue(hw, pf_q, on);
		if ( ret != I40E_SUCCESS)
			return ret;
	}

	return I40E_SUCCESS;
}

/* Switch on or off all the rx/tx queues */
int
i40e_vsi_switch_queues(struct i40e_vsi *vsi, bool on)
{
	int ret;

	if (on) {
		/* enable rx queues before enabling tx queues */
		ret = i40e_vsi_switch_rx_queues(vsi, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch rx queues\n");
			return ret;
		}
		ret = i40e_vsi_switch_tx_queues(vsi, on);
	} else {
		/* Stop tx queues before stopping rx queues */
		ret = i40e_vsi_switch_tx_queues(vsi, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch tx queues\n");
			return ret;
		}
		ret = i40e_vsi_switch_rx_queues(vsi, on);
	}

	return ret;
}

/* Initialize VSI for TX */
static int
i40e_vsi_tx_init(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	struct rte_eth_dev_data *data = pf->dev_data;
	uint16_t i;
	uint32_t ret = I40E_SUCCESS;

	for (i = 0; i < data->nb_tx_queues; i++) {
		ret = i40e_tx_queue_init(data->tx_queues[i]);
		if (ret != I40E_SUCCESS)
			break;
	}

	return ret;
}

/* Initialize VSI for RX */
static int
i40e_vsi_rx_init(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	struct rte_eth_dev_data *data = pf->dev_data;
	int ret = I40E_SUCCESS;
	uint16_t i;

	i40e_pf_config_mq_rx(pf);
	for (i = 0; i < data->nb_rx_queues; i++) {
		ret = i40e_rx_queue_init(data->rx_queues[i]);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to do RX queue "
					"initialization\n");
			break;
		}
	}

	return ret;
}

/* Initialize VSI */
static int
i40e_vsi_init(struct i40e_vsi *vsi)
{
	int err;

	err = i40e_vsi_tx_init(vsi);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do vsi TX initialization\n");
		return err;
	}
	err = i40e_vsi_rx_init(vsi);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do vsi RX initialization\n");
		return err;
	}

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
			((uint64_t)1 << I40E_32_BIT_SHIFT)) - *offset);
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
			I40E_16_BIT_MASK)) << I40E_32_BIT_SHIFT;

	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (uint64_t)((new_data +
			((uint64_t)1 << I40E_48_BIT_SHIFT)) - *offset);

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
	uint32_t enable;

	/* read pending request and disable first */
	i40e_pf_disable_irq0(hw);
	/**
	 * Enable all interrupt error options to detect possible errors,
	 * other informative int are ignored
	 */
	enable = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK |
	         I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK |
	         I40E_PFINT_ICR0_ENA_GRST_MASK |
	         I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK |
	         I40E_PFINT_ICR0_ENA_LINK_STAT_CHANGE_MASK |
	         I40E_PFINT_ICR0_ENA_HMC_ERR_MASK |
	         I40E_PFINT_ICR0_ENA_VFLR_MASK |
	         I40E_PFINT_ICR0_ENA_ADMINQ_MASK;

	I40E_WRITE_REG(hw, I40E_PFINT_ICR0_ENA, enable);
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
			PMD_DRV_LOG(INFO, "VF %u reset occured\n", abs_vf_id);
			/**
			 * Only notify a VF reset event occured,
			 * don't trigger another SW reset
			 */
			ret = i40e_pf_host_vf_reset(&pf->vfs[i], 0);
			if (ret != I40E_SUCCESS)
				PMD_DRV_LOG(ERR, "Failed to do VF reset\n");
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

	info.msg_size = I40E_AQ_BUF_SZ;
	info.msg_buf = rte_zmalloc("msg_buffer", I40E_AQ_BUF_SZ, 0);
	if (!info.msg_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate mem\n");
		return;
	}

	pending = 1;
	while (pending) {
		ret = i40e_clean_arq_element(hw, &info, &pending);

		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to read msg from AdminQ, "
				"aq_err: %u\n", hw->aq.asq_last_status);
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
					info.msg_size);
			break;
		default:
			PMD_DRV_LOG(ERR, "Request %u is not supported yet\n",
				opcode);
			break;
		}
		/* Reset the buffer after processing one */
		info.msg_size = I40E_AQ_BUF_SZ;
	}
	rte_free(info.msg_buf);
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
	uint32_t cause, enable;

	i40e_pf_disable_irq0(hw);

	cause = I40E_READ_REG(hw, I40E_PFINT_ICR0);
	enable = I40E_READ_REG(hw, I40E_PFINT_ICR0_ENA);

	/* Shared IRQ case, return */
	if (!(cause & I40E_PFINT_ICR0_INTEVENT_MASK)) {
		PMD_DRV_LOG(INFO, "Port%d INT0:share IRQ case, "
			"no INT event to process\n", hw->pf_id);
		goto done;
	}

	if (cause & I40E_PFINT_ICR0_LINK_STAT_CHANGE_MASK) {
		PMD_DRV_LOG(INFO, "INT:Link status changed\n");
		i40e_dev_link_update(dev, 0);
	}

	if (cause & I40E_PFINT_ICR0_ECC_ERR_MASK)
		PMD_DRV_LOG(INFO, "INT:Unrecoverable ECC Error\n");

	if (cause & I40E_PFINT_ICR0_MAL_DETECT_MASK)
		PMD_DRV_LOG(INFO, "INT:Malicious programming detected\n");

	if (cause & I40E_PFINT_ICR0_GRST_MASK)
		PMD_DRV_LOG(INFO, "INT:Global Resets Requested\n");

	if (cause & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK)
		PMD_DRV_LOG(INFO, "INT:PCI EXCEPTION occured\n");

	if (cause & I40E_PFINT_ICR0_HMC_ERR_MASK)
		PMD_DRV_LOG(INFO, "INT:HMC error occured\n");

	/* Add processing func to deal with VF reset vent */
	if (cause & I40E_PFINT_ICR0_VFLR_MASK) {
		PMD_DRV_LOG(INFO, "INT:VF reset detected\n");
		i40e_dev_handle_vfr_event(dev);
	}
	/* Find admin queue event */
	if (cause & I40E_PFINT_ICR0_ADMINQ_MASK) {
		PMD_DRV_LOG(INFO, "INT:ADMINQ event\n");
		i40e_dev_handle_aq_msg(dev);
	}

done:
	I40E_WRITE_REG(hw, I40E_PFINT_ICR0_ENA, enable);
	/* Re-enable interrupt from device side */
	i40e_pf_enable_irq0(hw);
	/* Re-enable interrupt from host side */
	rte_intr_enable(&(dev->pci_dev->intr_handle));
}

static int
i40e_add_macvlan_filters(struct i40e_vsi *vsi,
			 struct i40e_macvlan_filter *filter,
			 int total)
{
	int ele_num, ele_buff_size;
	int num, actual_num, i;
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_add_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;
	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_add", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory\n");
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
			req_list[i].flags = rte_cpu_to_le_16(\
				I40E_AQC_MACVLAN_ADD_PERFECT_MATCH);
			req_list[i].queue_number = 0;
		}

		ret = i40e_aq_add_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to add macvlan filter\n");
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
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_remove_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;

	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_remove", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory\n");
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
			req_list[i].flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
		}

		ret = i40e_aq_remove_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to remove macvlan filter\n");
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
		if (is_same_ether_addr(macaddr, &(f->macaddr)))
			return f;
	}

	return NULL;
}

static bool
i40e_find_vlan_filter(struct i40e_vsi *vsi,
			 uint16_t vlan_id)
{
	uint32_t vid_idx, vid_bit;

	vid_idx = (uint32_t) ((vlan_id >> 5) & 0x7F);
	vid_bit = (uint32_t) (1 << (vlan_id & 0x1F));

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

#define UINT32_BIT_MASK      0x1F
#define VALID_VLAN_BIT_MASK  0xFFF
	/* VFTA is 32-bits size array, each element contains 32 vlan bits, Find the
	 *  element first, then find the bits it belongs to
	 */
	vid_idx = (uint32_t) ((vlan_id & VALID_VLAN_BIT_MASK) >>
		  sizeof(uint32_t));
	vid_bit = (uint32_t) (1 << (vlan_id & UINT32_BIT_MASK));

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
								"not match\n");
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
			PMD_DRV_LOG(ERR, "buffer number not match\n");
			return I40E_ERR_PARAM;
		}
		(void)rte_memcpy(&mv_f[i].macaddr, &f->macaddr, ETH_ADDR_LEN);
		mv_f[i].vlan_id = vlan;
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
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
		return I40E_ERR_NO_MEMORY;
	}

	i = 0;
	if (vsi->vlan_num == 0) {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			(void)rte_memcpy(&mv_f[i].macaddr,
				&f->macaddr, ETH_ADDR_LEN);
			mv_f[i].vlan_id = 0;
			i++;
		}
	} else {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			ret = i40e_find_all_vlan_for_mac(vsi,&mv_f[i],
					vsi->vlan_num, &f->macaddr);
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
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr\n");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
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
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr\n");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
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
i40e_vsi_add_mac(struct i40e_vsi *vsi, struct ether_addr *addr)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int vlan_num;
	int ret = I40E_SUCCESS;

	/* If it's add and we've config it, return */
	f = i40e_find_mac_filter(vsi, addr);
	if (f != NULL)
		return I40E_SUCCESS;

	/**
	 * If vlan_num is 0, that's the first time to add mac,
	 * set mask for vlan_id 0.
	 */
	if (vsi->vlan_num == 0) {
		i40e_set_vlan_filter(vsi, 0, 1);
		vsi->vlan_num = 1;
	}

	vlan_num = vsi->vlan_num;

	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num, addr);
	if (ret != I40E_SUCCESS)
		goto DONE;

	ret = i40e_add_macvlan_filters(vsi, mv_f, vlan_num);
	if (ret != I40E_SUCCESS)
		goto DONE;

	/* Add the mac addr into mac list */
	f = rte_zmalloc("macv_filter", sizeof(*f), 0);
	if (f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
		ret = I40E_ERR_NO_MEMORY;
		goto DONE;
	}
	(void)rte_memcpy(&f->macaddr, addr, ETH_ADDR_LEN);
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
	int vlan_num;
	int ret = I40E_SUCCESS;

	/* Can't find it, return an error */
	f = i40e_find_mac_filter(vsi, addr);
	if (f == NULL)
		return I40E_ERR_PARAM;

	vlan_num = vsi->vlan_num;
	if (vlan_num == 0) {
		PMD_DRV_LOG(ERR, "VLAN number shouldn't be 0\n");
		return I40E_ERR_PARAM;
	}
	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory\n");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num, addr);
	if (ret != I40E_SUCCESS)
		goto DONE;

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
static uint64_t
i40e_config_hena(uint64_t flags)
{
	uint64_t hena = 0;

	if (!flags)
		return hena;

	if (flags & ETH_RSS_NONF_IPV4_UDP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	if (flags & ETH_RSS_NONF_IPV4_TCP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP;
	if (flags & ETH_RSS_NONF_IPV4_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP;
	if (flags & ETH_RSS_NONF_IPV4_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
	if (flags & ETH_RSS_FRAG_IPV4)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4;
	if (flags & ETH_RSS_NONF_IPV6_UDP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP;
	if (flags & ETH_RSS_NONF_IPV6_TCP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP;
	if (flags & ETH_RSS_NONF_IPV6_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;
	if (flags & ETH_RSS_NONF_IPV6_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
	if (flags & ETH_RSS_FRAG_IPV6)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6;
	if (flags & ETH_RSS_L2_PAYLOAD)
		hena |= 1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD;

	return hena;
}

/* Parse the hash enable flags */
static uint64_t
i40e_parse_hena(uint64_t flags)
{
	uint64_t rss_hf = 0;

	if (!flags)
		return rss_hf;

	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP))
		rss_hf |= ETH_RSS_NONF_IPV4_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP))
		rss_hf |= ETH_RSS_NONF_IPV4_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP))
		rss_hf |= ETH_RSS_NONF_IPV4_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER))
		rss_hf |= ETH_RSS_NONF_IPV4_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4))
		rss_hf |= ETH_RSS_FRAG_IPV4;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP))
		rss_hf |= ETH_RSS_NONF_IPV6_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP))
		rss_hf |= ETH_RSS_NONF_IPV6_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP))
		rss_hf |= ETH_RSS_NONF_IPV6_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER))
		rss_hf |= ETH_RSS_NONF_IPV6_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6))
		rss_hf |= ETH_RSS_FRAG_IPV6;
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

/* Configure RSS */
static int
i40e_pf_config_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct rte_eth_rss_conf rss_conf;
	uint32_t i, lut = 0;
	uint16_t j, num = i40e_prev_power_of_2(pf->dev_data->nb_rx_queues);

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
		/* Calculate the default hash key */
		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			rss_key_default[i] = (uint32_t)rte_rand();
		rss_conf.rss_key = (uint8_t *)rss_key_default;
		rss_conf.rss_key_len = (I40E_PFQF_HKEY_MAX_INDEX + 1) *
							sizeof(uint32_t);
	}

	return i40e_hw_rss_hash_set(hw, &rss_conf);
}

static int
i40e_pf_config_mq_rx(struct i40e_pf *pf)
{
	if (!pf->dev_data->sriov.active) {
		switch (pf->dev_data->dev_conf.rxmode.mq_mode) {
		case ETH_MQ_RX_RSS:
			i40e_pf_config_rss(pf);
			break;
		default:
			i40e_pf_disable_rss(pf);
			break;
		}
	}

	return 0;
}
