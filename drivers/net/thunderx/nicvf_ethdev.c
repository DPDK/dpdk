/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <sys/timerfd.h>

#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_pci.h>
#include <rte_tailq.h>

#include "base/nicvf_plat.h"

#include "nicvf_ethdev.h"

#include "nicvf_logs.h"

static inline int
nicvf_atomic_write_link_status(struct rte_eth_dev *dev,
			       struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
		*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline void
nicvf_set_eth_link_status(struct nicvf *nic, struct rte_eth_link *link)
{
	link->link_status = nic->link_up;
	link->link_duplex = ETH_LINK_AUTONEG;
	if (nic->duplex == NICVF_HALF_DUPLEX)
		link->link_duplex = ETH_LINK_HALF_DUPLEX;
	else if (nic->duplex == NICVF_FULL_DUPLEX)
		link->link_duplex = ETH_LINK_FULL_DUPLEX;
	link->link_speed = nic->speed;
	link->link_autoneg = ETH_LINK_SPEED_AUTONEG;
}

static void
nicvf_interrupt(void *arg)
{
	struct nicvf *nic = arg;

	if (nicvf_reg_poll_interrupts(nic) == NIC_MBOX_MSG_BGX_LINK_CHANGE) {
		if (nic->eth_dev->data->dev_conf.intr_conf.lsc)
			nicvf_set_eth_link_status(nic,
					&nic->eth_dev->data->dev_link);
		_rte_eth_dev_callback_process(nic->eth_dev,
				RTE_ETH_EVENT_INTR_LSC);
	}

	rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
				nicvf_interrupt, nic);
}

static int
nicvf_periodic_alarm_start(struct nicvf *nic)
{
	return rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
					nicvf_interrupt, nic);
}

static int
nicvf_periodic_alarm_stop(struct nicvf *nic)
{
	return rte_eal_alarm_cancel(nicvf_interrupt, nic);
}

/*
 * Return 0 means link status changed, -1 means not changed
 */
static int
nicvf_dev_link_update(struct rte_eth_dev *dev,
		      int wait_to_complete __rte_unused)
{
	struct rte_eth_link link;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	memset(&link, 0, sizeof(link));
	nicvf_set_eth_link_status(nic, &link);
	return nicvf_atomic_write_link_status(dev, &link);
}

static int
nicvf_dev_get_reg_length(struct rte_eth_dev *dev  __rte_unused)
{
	return nicvf_reg_get_count();
}

static int
nicvf_dev_get_regs(struct rte_eth_dev *dev, struct rte_dev_reg_info *regs)
{
	uint64_t *data = regs->data;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (data == NULL)
		return -EINVAL;

	/* Support only full register dump */
	if ((regs->length == 0) ||
		(regs->length == (uint32_t)nicvf_reg_get_count())) {
		regs->version = nic->vendor_id << 16 | nic->device_id;
		nicvf_reg_dump(nic, data);
		return 0;
	}
	return -ENOTSUP;
}

static int
nicvf_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	if (!rte_eal_has_hugepages()) {
		PMD_INIT_LOG(INFO, "Huge page is not configured");
		return -EINVAL;
	}

	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "Tx mq_mode DCB or VMDq not supported");
		return -EINVAL;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
		rxmode->mq_mode != ETH_MQ_RX_RSS) {
		PMD_INIT_LOG(INFO, "Unsupported rx qmode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (!rxmode->hw_strip_crc) {
		PMD_INIT_LOG(NOTICE, "Can't disable hw crc strip");
		rxmode->hw_strip_crc = 1;
	}

	if (rxmode->hw_ip_checksum) {
		PMD_INIT_LOG(NOTICE, "Rxcksum not supported");
		rxmode->hw_ip_checksum = 0;
	}

	if (rxmode->split_hdr_size) {
		PMD_INIT_LOG(INFO, "Rxmode does not support split header");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_filter) {
		PMD_INIT_LOG(INFO, "VLAN filter not supported");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_extend) {
		PMD_INIT_LOG(INFO, "VLAN extended not supported");
		return -EINVAL;
	}

	if (rxmode->enable_lro) {
		PMD_INIT_LOG(INFO, "LRO not supported");
		return -EINVAL;
	}

	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(INFO, "Setting link speed/duplex not supported");
		return -EINVAL;
	}

	if (conf->dcb_capability_en) {
		PMD_INIT_LOG(INFO, "DCB enable not supported");
		return -EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		PMD_INIT_LOG(INFO, "Flow director not supported");
		return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "Configured ethdev port%d hwcap=0x%" PRIx64,
		dev->data->port_id, nicvf_hw_cap(nic));

	return 0;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops nicvf_eth_dev_ops = {
	.dev_configure            = nicvf_dev_configure,
	.link_update              = nicvf_dev_link_update,
	.get_reg_length           = nicvf_dev_get_reg_length,
	.get_reg                  = nicvf_dev_get_regs,
};

static int
nicvf_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	int ret;
	struct rte_pci_device *pci_dev;
	struct nicvf *nic = nicvf_pmd_priv(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &nicvf_eth_dev_ops;

	pci_dev = eth_dev->pci_dev;
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	nic->device_id = pci_dev->id.device_id;
	nic->vendor_id = pci_dev->id.vendor_id;
	nic->subsystem_device_id = pci_dev->id.subsystem_device_id;
	nic->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	nic->eth_dev = eth_dev;

	PMD_INIT_LOG(DEBUG, "nicvf: device (%x:%x) %u:%u:%u:%u",
			pci_dev->id.vendor_id, pci_dev->id.device_id,
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);

	nic->reg_base = (uintptr_t)pci_dev->mem_resource[0].addr;
	if (!nic->reg_base) {
		PMD_INIT_LOG(ERR, "Failed to map BAR0");
		ret = -ENODEV;
		goto fail;
	}

	nicvf_disable_all_interrupts(nic);

	ret = nicvf_periodic_alarm_start(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to start period alarm");
		goto fail;
	}

	ret = nicvf_mbox_check_pf_ready(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get ready message from PF");
		goto alarm_fail;
	} else {
		PMD_INIT_LOG(INFO,
			"node=%d vf=%d mode=%s sqs=%s loopback_supported=%s",
			nic->node, nic->vf_id,
			nic->tns_mode == NIC_TNS_MODE ? "tns" : "tns-bypass",
			nic->sqs_mode ? "true" : "false",
			nic->loopback_supported ? "true" : "false"
			);
	}

	if (nic->sqs_mode) {
		PMD_INIT_LOG(INFO, "Unsupported SQS VF detected, Detaching...");
		/* Detach port by returning Positive error number */
		ret = ENOTSUP;
		goto alarm_fail;
	}

	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for mac addr");
		ret = -ENOMEM;
		goto alarm_fail;
	}
	if (is_zero_ether_addr((struct ether_addr *)nic->mac_addr))
		eth_random_addr(&nic->mac_addr[0]);

	ether_addr_copy((struct ether_addr *)nic->mac_addr,
			&eth_dev->data->mac_addrs[0]);

	ret = nicvf_mbox_set_mac_addr(nic, nic->mac_addr);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to set mac addr");
		goto malloc_fail;
	}

	ret = nicvf_base_init(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to execute nicvf_base_init");
		goto malloc_fail;
	}

	ret = nicvf_mbox_get_rss_size(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get rss table size");
		goto malloc_fail;
	}

	PMD_INIT_LOG(INFO, "Port %d (%x:%x) mac=%02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id, nic->vendor_id, nic->device_id,
		nic->mac_addr[0], nic->mac_addr[1], nic->mac_addr[2],
		nic->mac_addr[3], nic->mac_addr[4], nic->mac_addr[5]);

	return 0;

malloc_fail:
	rte_free(eth_dev->data->mac_addrs);
alarm_fail:
	nicvf_periodic_alarm_stop(nic);
fail:
	return ret;
}

static const struct rte_pci_id pci_id_nicvf_map[] = {
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_PASS1_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_THUNDERX_PASS1_NICVF,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_PASS2_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_THUNDERX_PASS2_NICVF,
	},
	{
		.vendor_id = 0,
	},
};

static struct eth_driver rte_nicvf_pmd = {
	.pci_drv = {
		.name = "rte_nicvf_pmd",
		.id_table = pci_id_nicvf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = nicvf_eth_dev_init,
	.dev_private_size = sizeof(struct nicvf),
};

static int
rte_nicvf_pmd_init(const char *name __rte_unused, const char *para __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	PMD_INIT_LOG(INFO, "librte_pmd_thunderx nicvf version %s",
			THUNDERX_NICVF_PMD_VERSION);

	rte_eth_driver_register(&rte_nicvf_pmd);
	return 0;
}

static struct rte_driver rte_nicvf_driver = {
	.name = "nicvf_driver",
	.type = PMD_PDEV,
	.init = rte_nicvf_pmd_init,
};

PMD_REGISTER_DRIVER(rte_nicvf_driver);
