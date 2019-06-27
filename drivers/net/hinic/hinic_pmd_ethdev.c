/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_pci.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_mempool.h>
#include <rte_errno.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_hwif.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_cfg.h"
#include "base/hinic_pmd_mgmt.h"
#include "base/hinic_pmd_cmdq.h"
#include "base/hinic_pmd_niccfg.h"
#include "base/hinic_pmd_nicio.h"
#include "hinic_pmd_ethdev.h"
#include "hinic_pmd_tx.h"
#include "hinic_pmd_rx.h"

/* Vendor ID used by Huawei devices */
#define HINIC_HUAWEI_VENDOR_ID		0x19E5

/* Hinic devices */
#define HINIC_DEV_ID_PRD		0x1822
#define HINIC_DEV_ID_MEZZ_25GE		0x0210
#define HINIC_DEV_ID_MEZZ_40GE		0x020D
#define HINIC_DEV_ID_MEZZ_100GE		0x0205

#define HINIC_SERVICE_MODE_NIC		2

#define HINIC_INTR_CB_UNREG_MAX_RETRIES		10

#define DEFAULT_BASE_COS		4
#define NR_MAX_COS			8

#define HINIC_MIN_RX_BUF_SIZE		1024
#define HINIC_MAX_MAC_ADDRS		1

/** Driver-specific log messages type. */
int hinic_logtype;

static const struct rte_eth_desc_lim hinic_rx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_RXD_ALIGN,
};

static const struct rte_eth_desc_lim hinic_tx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_TXD_ALIGN,
};

/**
 * Interrupt handler triggered by NIC  for handling
 * specific event.
 *
 * @param: The address of parameter (struct rte_eth_dev *) regsitered before.
 **/
static void hinic_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (!hinic_test_bit(HINIC_DEV_INTR_EN, &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING, "Device's interrupt is disabled, ignore interrupt event, dev_name: %s, port_id: %d",
			    nic_dev->proc_dev_name, dev->data->port_id);
		return;
	}

	/* aeq0 msg handler */
	hinic_dev_handle_aeq_event(nic_dev->hwdev, param);
}

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues, mtu size
 * and configure RSS.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_dev_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev;
	struct hinic_nic_io *nic_io;
	int err;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	nic_io = nic_dev->hwdev->nic_io;

	nic_dev->num_sq =  dev->data->nb_tx_queues;
	nic_dev->num_rq = dev->data->nb_rx_queues;

	nic_io->num_sqs =  dev->data->nb_tx_queues;
	nic_io->num_rqs = dev->data->nb_rx_queues;

	/* queue pair is max_num(sq, rq) */
	nic_dev->num_qps = (nic_dev->num_sq > nic_dev->num_rq) ?
			nic_dev->num_sq : nic_dev->num_rq;
	nic_io->num_qps = nic_dev->num_qps;

	if (nic_dev->num_qps > nic_io->max_qps) {
		PMD_DRV_LOG(ERR,
			"Queue number out of range, get queue_num:%d, max_queue_num:%d",
			nic_dev->num_qps, nic_io->max_qps);
		return -EINVAL;
	}

	/* mtu size is 256~9600 */
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len < HINIC_MIN_FRAME_SIZE ||
	    dev->data->dev_conf.rxmode.max_rx_pkt_len >
	    HINIC_MAX_JUMBO_FRAME_SIZE) {
		PMD_DRV_LOG(ERR,
			"Max rx pkt len out of range, get max_rx_pkt_len:%d, "
			"expect between %d and %d",
			dev->data->dev_conf.rxmode.max_rx_pkt_len,
			HINIC_MIN_FRAME_SIZE, HINIC_MAX_JUMBO_FRAME_SIZE);
		return -EINVAL;
	}

	nic_dev->mtu_size =
		HINIC_PKTLEN_TO_MTU(dev->data->dev_conf.rxmode.max_rx_pkt_len);

	/* rss template */
	err = hinic_config_mq_mode(dev, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Config multi-queue failed");
		return err;
	}

	return HINIC_OK;
}

/**
 * Get link speed from NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param speed_capa
 *   Pointer to link speed structure.
 */
static void hinic_get_speed_capa(struct rte_eth_dev *dev, uint32_t *speed_capa)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u32 supported_link, advertised_link;
	int err;

#define HINIC_LINK_MODE_SUPPORT_1G	(1U << HINIC_GE_BASE_KX)

#define HINIC_LINK_MODE_SUPPORT_10G	(1U << HINIC_10GE_BASE_KR)

#define HINIC_LINK_MODE_SUPPORT_25G	((1U << HINIC_25GE_BASE_KR_S) | \
					(1U << HINIC_25GE_BASE_CR_S) | \
					(1U << HINIC_25GE_BASE_KR) | \
					(1U << HINIC_25GE_BASE_CR))

#define HINIC_LINK_MODE_SUPPORT_40G	((1U << HINIC_40GE_BASE_KR4) | \
					(1U << HINIC_40GE_BASE_CR4))

#define HINIC_LINK_MODE_SUPPORT_100G	((1U << HINIC_100GE_BASE_KR4) | \
					(1U << HINIC_100GE_BASE_CR4))

	err = hinic_get_link_mode(nic_dev->hwdev,
				  &supported_link, &advertised_link);
	if (err || supported_link == HINIC_SUPPORTED_UNKNOWN ||
	    advertised_link == HINIC_SUPPORTED_UNKNOWN) {
		PMD_DRV_LOG(WARNING, "Get speed capability info failed, device: %s, port_id: %u",
			  nic_dev->proc_dev_name, dev->data->port_id);
	} else {
		*speed_capa = 0;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_1G))
			*speed_capa |= ETH_LINK_SPEED_1G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_10G))
			*speed_capa |= ETH_LINK_SPEED_10G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_25G))
			*speed_capa |= ETH_LINK_SPEED_25G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_40G))
			*speed_capa |= ETH_LINK_SPEED_40G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_100G))
			*speed_capa |= ETH_LINK_SPEED_100G;
	}
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param info
 *   Pointer to Info structure output buffer.
 */
static void
hinic_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	info->max_rx_queues  = nic_dev->nic_cap.max_rqs;
	info->max_tx_queues  = nic_dev->nic_cap.max_sqs;
	info->min_rx_bufsize = HINIC_MIN_RX_BUF_SIZE;
	info->max_rx_pktlen  = HINIC_MAX_JUMBO_FRAME_SIZE;
	info->max_mac_addrs  = HINIC_MAX_MAC_ADDRS;

	hinic_get_speed_capa(dev, &info->speed_capa);
	info->rx_queue_offload_capa = 0;
	info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP |
				DEV_RX_OFFLOAD_IPV4_CKSUM |
				DEV_RX_OFFLOAD_UDP_CKSUM |
				DEV_RX_OFFLOAD_TCP_CKSUM;

	info->tx_queue_offload_capa = 0;
	info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT |
				DEV_TX_OFFLOAD_IPV4_CKSUM |
				DEV_TX_OFFLOAD_UDP_CKSUM |
				DEV_TX_OFFLOAD_TCP_CKSUM |
				DEV_TX_OFFLOAD_SCTP_CKSUM |
				DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				DEV_TX_OFFLOAD_TCP_TSO |
				DEV_TX_OFFLOAD_MULTI_SEGS;

	info->hash_key_size = HINIC_RSS_KEY_SIZE;
	info->reta_size = HINIC_RSS_INDIR_SIZE;
	info->flow_type_rss_offloads = HINIC_RSS_OFFLOAD_ALL;
	info->rx_desc_lim = hinic_rx_desc_lim;
	info->tx_desc_lim = hinic_tx_desc_lim;
}

static void hinic_free_all_rq(struct hinic_nic_dev *nic_dev)
{
	u16 q_id;

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++)
		hinic_destroy_rq(nic_dev->hwdev, q_id);
}

static void hinic_free_all_sq(struct hinic_nic_dev *nic_dev)
{
	u16 q_id;

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++)
		hinic_destroy_sq(nic_dev->hwdev, q_id);
}

static void hinic_disable_interrupt(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	int ret, retries = 0;

	hinic_clear_bit(HINIC_DEV_INTR_EN, &nic_dev->dev_status);

	/* disable msix interrupt in hardware */
	hinic_set_msix_state(nic_dev->hwdev, 0, HINIC_MSIX_DISABLE);

	/* disable rte interrupt */
	ret = rte_intr_disable(&pci_dev->intr_handle);
	if (ret)
		PMD_DRV_LOG(ERR, "Disable intr failed: %d", ret);

	do {
		ret =
		rte_intr_callback_unregister(&pci_dev->intr_handle,
					     hinic_dev_interrupt_handler, dev);
		if (ret >= 0) {
			break;
		} else if (ret == -EAGAIN) {
			rte_delay_ms(100);
			retries++;
		} else {
			PMD_DRV_LOG(ERR, "intr callback unregister failed: %d",
				    ret);
			break;
		}
	} while (retries < HINIC_INTR_CB_UNREG_MAX_RETRIES);

	if (retries == HINIC_INTR_CB_UNREG_MAX_RETRIES)
		PMD_DRV_LOG(ERR, "Unregister intr callback failed after %d retries",
			    retries);
}

/**
 * Init mac_vlan table in NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is filled,
 *   negative error value otherwise.
 */
static int hinic_init_mac_addr(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
	u16 func_id = 0;
	int rc = 0;

	rc = hinic_get_default_mac(nic_dev->hwdev, addr_bytes);
	if (rc)
		return rc;

	memmove(eth_dev->data->mac_addrs->addr_bytes,
		addr_bytes, RTE_ETHER_ADDR_LEN);

	func_id = hinic_global_func_id(nic_dev->hwdev);
	rc = hinic_set_mac(nic_dev->hwdev, eth_dev->data->mac_addrs->addr_bytes,
			   0, func_id);
	if (rc && rc != HINIC_PF_SET_VF_ALREADY)
		return rc;

	return 0;
}

/**
 * Deinit mac_vlan table in NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is filled,
 *   negative error value otherwise.
 */
static void hinic_deinit_mac_addr(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	int rc;
	u16 func_id = 0;

	if (rte_is_zero_ether_addr(eth_dev->data->mac_addrs))
		return;

	func_id = hinic_global_func_id(nic_dev->hwdev);
	rc = hinic_del_mac(nic_dev->hwdev,
			   eth_dev->data->mac_addrs->addr_bytes,
			   0, func_id);
	if (rc && rc != HINIC_PF_SET_VF_ALREADY)
		PMD_DRV_LOG(ERR, "Delete mac table failed, dev_name: %s",
			    eth_dev->data->name);
}

static int hinic_set_default_pause_feature(struct hinic_nic_dev *nic_dev)
{
	struct nic_pause_config pause_config = {0};

	pause_config.auto_neg = 0;
	pause_config.rx_pause = HINIC_DEFAUT_PAUSE_CONFIG;
	pause_config.tx_pause = HINIC_DEFAUT_PAUSE_CONFIG;

	return hinic_set_pause_config(nic_dev->hwdev, pause_config);
}

static int hinic_set_default_dcb_feature(struct hinic_nic_dev *nic_dev)
{
	u8 up_tc[HINIC_DCB_UP_MAX] = {0};
	u8 up_pgid[HINIC_DCB_UP_MAX] = {0};
	u8 up_bw[HINIC_DCB_UP_MAX] = {0};
	u8 pg_bw[HINIC_DCB_UP_MAX] = {0};
	u8 up_strict[HINIC_DCB_UP_MAX] = {0};
	int i = 0;

	pg_bw[0] = 100;
	for (i = 0; i < HINIC_DCB_UP_MAX; i++)
		up_bw[i] = 100;

	return hinic_dcb_set_ets(nic_dev->hwdev, up_tc, pg_bw,
					up_pgid, up_bw, up_strict);
}

static void hinic_init_default_cos(struct hinic_nic_dev *nic_dev)
{
	nic_dev->default_cos =
			(hinic_global_func_id(nic_dev->hwdev) +
			 DEFAULT_BASE_COS) % NR_MAX_COS;
}

static int hinic_set_default_hw_feature(struct hinic_nic_dev *nic_dev)
{
	int err;

	hinic_init_default_cos(nic_dev);

	/* Restore DCB configure to default status */
	err = hinic_set_default_dcb_feature(nic_dev);
	if (err)
		return err;

	/* disable LRO */
	err = hinic_set_rx_lro(nic_dev->hwdev, 0, 0, (u8)0);
	if (err)
		return err;

	/* Set pause enable, and up will disable pfc. */
	err = hinic_set_default_pause_feature(nic_dev);
	if (err)
		return err;

	err = hinic_reset_port_link_cfg(nic_dev->hwdev);
	if (err)
		return err;

	err = hinic_set_link_status_follow(nic_dev->hwdev,
					   HINIC_LINK_FOLLOW_PORT);
	if (err == HINIC_MGMT_CMD_UNSUPPORTED)
		PMD_DRV_LOG(WARNING, "Don't support to set link status follow phy port status");
	else if (err)
		return err;

	return hinic_set_anti_attack(nic_dev->hwdev, true);
}

static int32_t hinic_card_workmode_check(struct hinic_nic_dev *nic_dev)
{
	struct hinic_board_info info = { 0 };
	int rc;

	rc = hinic_get_board_info(nic_dev->hwdev, &info);
	if (rc)
		return rc;

	return (info.service_mode == HINIC_SERVICE_MODE_NIC ? HINIC_OK :
						HINIC_ERROR);
}

static int hinic_copy_mempool_init(struct hinic_nic_dev *nic_dev)
{
	nic_dev->cpy_mpool = rte_mempool_lookup(nic_dev->proc_dev_name);
	if (nic_dev->cpy_mpool == NULL) {
		nic_dev->cpy_mpool =
		rte_pktmbuf_pool_create(nic_dev->proc_dev_name,
					HINIC_COPY_MEMPOOL_DEPTH,
					RTE_CACHE_LINE_SIZE, 0,
					HINIC_COPY_MBUF_SIZE,
					rte_socket_id());
		if (!nic_dev->cpy_mpool) {
			PMD_DRV_LOG(ERR, "Create copy mempool failed, errno: %d, dev_name: %s",
				    rte_errno, nic_dev->proc_dev_name);
			return -ENOMEM;
		}
	}

	return 0;
}

static void hinic_copy_mempool_uninit(struct hinic_nic_dev *nic_dev)
{
	if (nic_dev->cpy_mpool != NULL)
		rte_mempool_free(nic_dev->cpy_mpool);
}

static int hinic_init_sw_rxtxqs(struct hinic_nic_dev *nic_dev)
{
	u32 txq_size;
	u32 rxq_size;

	/* allocate software txq array */
	txq_size = nic_dev->nic_cap.max_sqs * sizeof(*nic_dev->txqs);
	nic_dev->txqs = kzalloc_aligned(txq_size, GFP_KERNEL);
	if (!nic_dev->txqs) {
		PMD_DRV_LOG(ERR, "Allocate txqs failed");
		return -ENOMEM;
	}

	/* allocate software rxq array */
	rxq_size = nic_dev->nic_cap.max_rqs * sizeof(*nic_dev->rxqs);
	nic_dev->rxqs = kzalloc_aligned(rxq_size, GFP_KERNEL);
	if (!nic_dev->rxqs) {
		/* free txqs */
		kfree(nic_dev->txqs);
		nic_dev->txqs = NULL;

		PMD_DRV_LOG(ERR, "Allocate rxqs failed");
		return -ENOMEM;
	}

	return HINIC_OK;
}

static void hinic_deinit_sw_rxtxqs(struct hinic_nic_dev *nic_dev)
{
	kfree(nic_dev->txqs);
	nic_dev->txqs = NULL;

	kfree(nic_dev->rxqs);
	nic_dev->rxqs = NULL;
}

static int hinic_nic_dev_create(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	int rc;

	nic_dev->hwdev = rte_zmalloc("hinic_hwdev", sizeof(*nic_dev->hwdev),
				     RTE_CACHE_LINE_SIZE);
	if (!nic_dev->hwdev) {
		PMD_DRV_LOG(ERR, "Allocate hinic hwdev memory failed, dev_name: %s",
			    eth_dev->data->name);
		return -ENOMEM;
	}
	nic_dev->hwdev->pcidev_hdl = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* init osdep*/
	rc = hinic_osdep_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize os_dep failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_osdep_fail;
	}

	/* init_hwif */
	rc = hinic_hwif_res_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize hwif failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_hwif_fail;
	}

	/* init_cfg_mgmt */
	rc = init_cfg_mgmt(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize cfg_mgmt failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cfgmgnt_fail;
	}

	/* init_aeqs */
	rc = hinic_comm_aeqs_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize aeqs failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_aeqs_fail;
	}

	/* init_pf_to_mgnt */
	rc = hinic_comm_pf_to_mgmt_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize pf_to_mgmt failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_pf_to_mgmt_fail;
	}

	rc = hinic_card_workmode_check(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Check card workmode failed, dev_name: %s",
			    eth_dev->data->name);
		goto workmode_check_fail;
	}

	/* do l2nic reset to make chip clear */
	rc = hinic_l2nic_reset(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Do l2nic reset failed, dev_name: %s",
			    eth_dev->data->name);
		goto l2nic_reset_fail;
	}

	/* init dma and aeq msix attribute table */
	(void)hinic_init_attr_table(nic_dev->hwdev);

	/* init_cmdqs */
	rc = hinic_comm_cmdqs_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize cmdq failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cmdq_fail;
	}

	/* set hardware state active */
	rc = hinic_activate_hwdev_state(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize resources state failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_resources_state_fail;
	}

	/* init_capability */
	rc = hinic_init_capability(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize capability failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cap_fail;
	}

	/* get nic capability */
	if (!hinic_support_nic(nic_dev->hwdev, &nic_dev->nic_cap))
		goto nic_check_fail;

	/* init root cla and function table */
	rc = hinic_init_nicio(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize nic_io failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_nicio_fail;
	}

	/* init_software_txrxq */
	rc = hinic_init_sw_rxtxqs(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize sw_rxtxqs failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_sw_rxtxqs_fail;
	}

	rc = hinic_copy_mempool_init(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create copy mempool failed, dev_name: %s",
			 eth_dev->data->name);
		goto init_mpool_fail;
	}

	/* set hardware feature to default status */
	rc = hinic_set_default_hw_feature(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize hardware default features failed, dev_name: %s",
			    eth_dev->data->name);
		goto set_default_hw_feature_fail;
	}

	return 0;

set_default_hw_feature_fail:
	hinic_copy_mempool_uninit(nic_dev);

init_mpool_fail:
	hinic_deinit_sw_rxtxqs(nic_dev);

init_sw_rxtxqs_fail:
	hinic_deinit_nicio(nic_dev->hwdev);

nic_check_fail:
init_nicio_fail:
init_cap_fail:
	hinic_deactivate_hwdev_state(nic_dev->hwdev);

init_resources_state_fail:
	hinic_comm_cmdqs_free(nic_dev->hwdev);

init_cmdq_fail:
l2nic_reset_fail:
workmode_check_fail:
	hinic_comm_pf_to_mgmt_free(nic_dev->hwdev);

init_pf_to_mgmt_fail:
	hinic_comm_aeqs_free(nic_dev->hwdev);

init_aeqs_fail:
	free_cfg_mgmt(nic_dev->hwdev);

init_cfgmgnt_fail:
	hinic_hwif_res_free(nic_dev->hwdev);

init_hwif_fail:
	hinic_osdep_deinit(nic_dev->hwdev);

init_osdep_fail:
	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;

	return rc;
}

static void hinic_nic_dev_destroy(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
			HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	(void)hinic_set_link_status_follow(nic_dev->hwdev,
					   HINIC_LINK_FOLLOW_DEFAULT);
	hinic_copy_mempool_uninit(nic_dev);
	hinic_deinit_sw_rxtxqs(nic_dev);
	hinic_deinit_nicio(nic_dev->hwdev);
	hinic_deactivate_hwdev_state(nic_dev->hwdev);
	hinic_comm_cmdqs_free(nic_dev->hwdev);
	hinic_comm_pf_to_mgmt_free(nic_dev->hwdev);
	hinic_comm_aeqs_free(nic_dev->hwdev);
	free_cfg_mgmt(nic_dev->hwdev);
	hinic_hwif_res_free(nic_dev->hwdev);
	hinic_osdep_deinit(nic_dev->hwdev);
	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;
}

static int hinic_func_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct rte_ether_addr *eth_addr;
	struct hinic_nic_dev *nic_dev;
	int rc;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* EAL is SECONDARY and eth_dev is already created */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		rc = rte_intr_callback_register(&pci_dev->intr_handle,
						hinic_dev_interrupt_handler,
						(void *)eth_dev);
		if (rc)
			PMD_DRV_LOG(ERR, "Initialize %s failed in secondary process",
				    eth_dev->data->name);

		return rc;
	}

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	memset(nic_dev, 0, sizeof(*nic_dev));

	snprintf(nic_dev->proc_dev_name,
		 sizeof(nic_dev->proc_dev_name),
		 "hinic-%.4x:%.2x:%.2x.%x",
		 pci_dev->addr.domain, pci_dev->addr.bus,
		 pci_dev->addr.devid, pci_dev->addr.function);

	/* alloc mac_addrs */
	eth_addr = rte_zmalloc("hinic_mac", sizeof(*eth_addr), 0);
	if (!eth_addr) {
		PMD_DRV_LOG(ERR, "Allocate ethernet addresses' memory failed, dev_name: %s",
			    eth_dev->data->name);
		rc = -ENOMEM;
		goto eth_addr_fail;
	}
	eth_dev->data->mac_addrs = eth_addr;

	/*
	 * Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	/* create hardware nic_device */
	rc = hinic_nic_dev_create(eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create nic device failed, dev_name: %s",
			    eth_dev->data->name);
		goto create_nic_dev_fail;
	}

	rc = hinic_init_mac_addr(eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize mac table failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_mac_fail;
	}

	/* register callback func to eal lib */
	rc = rte_intr_callback_register(&pci_dev->intr_handle,
					hinic_dev_interrupt_handler,
					(void *)eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Register rte interrupt callback failed, dev_name: %s",
			    eth_dev->data->name);
		goto reg_intr_cb_fail;
	}

	/* enable uio/vfio intr/eventfd mapping */
	rc = rte_intr_enable(&pci_dev->intr_handle);
	if (rc) {
		PMD_DRV_LOG(ERR, "Enable rte interrupt failed, dev_name: %s",
			    eth_dev->data->name);
		goto enable_intr_fail;
	}
	hinic_set_bit(HINIC_DEV_INTR_EN, &nic_dev->dev_status);

	hinic_set_bit(HINIC_DEV_INIT, &nic_dev->dev_status);
	PMD_DRV_LOG(INFO, "Initialize %s in primary successfully",
		    eth_dev->data->name);

	return 0;

enable_intr_fail:
	(void)rte_intr_callback_unregister(&pci_dev->intr_handle,
					   hinic_dev_interrupt_handler,
					   (void *)eth_dev);

reg_intr_cb_fail:
	hinic_deinit_mac_addr(eth_dev);

init_mac_fail:
	hinic_nic_dev_destroy(eth_dev);

create_nic_dev_fail:
	rte_free(eth_addr);
	eth_dev->data->mac_addrs = NULL;

eth_addr_fail:
	PMD_DRV_LOG(ERR, "Initialize %s in primary failed",
		    eth_dev->data->name);
	return rc;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void hinic_dev_close(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (hinic_test_and_set_bit(HINIC_DEV_CLOSE, &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING, "Device %s already closed",
			    dev->data->name);
		return;
	}

	/* rx_cqe, rx_info */
	hinic_free_all_rx_resources(dev);

	/* tx_info */
	hinic_free_all_tx_resources(dev);

	/* free wq, pi_dma_addr */
	hinic_free_all_rq(nic_dev);

	/* free wq, db_addr */
	hinic_free_all_sq(nic_dev);

	/* deinit mac vlan tbl */
	hinic_deinit_mac_addr(dev);

	/* disable hardware and uio interrupt */
	hinic_disable_interrupt(dev);

	/* deinit nic hardware device */
	hinic_nic_dev_destroy(dev);
}

static const struct eth_dev_ops hinic_pmd_ops = {
	.dev_configure                 = hinic_dev_configure,
	.dev_infos_get                 = hinic_dev_infos_get,
	.dev_close                     = hinic_dev_close,
};

static int hinic_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_DRV_LOG(INFO, "Initializing pf hinic-%.4x:%.2x:%.2x.%x in %s process",
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function,
		    (rte_eal_process_type() == RTE_PROC_PRIMARY) ?
		    "primary" : "secondary");

	/* rte_eth_dev ops, rx_burst and tx_burst */
	eth_dev->dev_ops = &hinic_pmd_ops;

	return hinic_func_init(eth_dev);
}

static int hinic_dev_uninit(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hinic_clear_bit(HINIC_DEV_INIT, &nic_dev->dev_status);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hinic_dev_close(dev);

	dev->dev_ops = NULL;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	return HINIC_OK;
}

static struct rte_pci_id pci_id_hinic_map[] = {
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_PRD) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_25GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_40GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_100GE) },
	{.vendor_id = 0},
};

static int hinic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct hinic_nic_dev), hinic_dev_init);
}

static int hinic_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hinic_dev_uninit);
}

static struct rte_pci_driver rte_hinic_pmd = {
	.id_table = pci_id_hinic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = hinic_pci_probe,
	.remove = hinic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hinic, rte_hinic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hinic, pci_id_hinic_map);

RTE_INIT(hinic_init_log)
{
	hinic_logtype = rte_log_register("pmd.net.hinic");
	if (hinic_logtype >= 0)
		rte_log_set_level(hinic_logtype, RTE_LOG_INFO);
}
