/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_alarm.h>

#include "lio_logs.h"
#include "lio_23xx_vf.h"
#include "lio_ethdev.h"
#include "lio_rxtx.h"

static void
lio_check_pf_hs_response(void *lio_dev)
{
	struct lio_device *dev = lio_dev;

	/* check till response arrives */
	if (dev->pfvf_hsword.coproc_tics_per_us)
		return;

	cn23xx_vf_handle_mbox(dev);

	rte_eal_alarm_set(1, lio_check_pf_hs_response, lio_dev);
}

/**
 * \brief Identify the LIO device and to map the BAR address space
 * @param lio_dev lio device
 */
static int
lio_chip_specific_setup(struct lio_device *lio_dev)
{
	struct rte_pci_device *pdev = lio_dev->pci_dev;
	uint32_t dev_id = pdev->id.device_id;
	const char *s;
	int ret = 1;

	switch (dev_id) {
	case LIO_CN23XX_VF_VID:
		lio_dev->chip_id = LIO_CN23XX_VF_VID;
		ret = cn23xx_vf_setup_device(lio_dev);
		s = "CN23XX VF";
		break;
	default:
		s = "?";
		lio_dev_err(lio_dev, "Unsupported Chip\n");
	}

	if (!ret)
		lio_dev_info(lio_dev, "DEVICE : %s\n", s);

	return ret;
}

static int
lio_first_time_init(struct lio_device *lio_dev,
		    struct rte_pci_device *pdev)
{
	int dpdk_queues;

	PMD_INIT_FUNC_TRACE();

	/* set dpdk specific pci device pointer */
	lio_dev->pci_dev = pdev;

	/* Identify the LIO type and set device ops */
	if (lio_chip_specific_setup(lio_dev)) {
		lio_dev_err(lio_dev, "Chip specific setup failed\n");
		return -1;
	}

	if (lio_dev->fn_list.setup_mbox(lio_dev)) {
		lio_dev_err(lio_dev, "Mailbox setup failed\n");
		goto error;
	}

	/* Check PF response */
	lio_check_pf_hs_response((void *)lio_dev);

	/* Do handshake and exit if incompatible PF driver */
	if (cn23xx_pfvf_handshake(lio_dev))
		goto error;

	/* Initial reset */
	cn23xx_vf_ask_pf_to_do_flr(lio_dev);
	/* Wait for FLR for 100ms per SRIOV specification */
	rte_delay_ms(100);

	if (cn23xx_vf_set_io_queues_off(lio_dev)) {
		lio_dev_err(lio_dev, "Setting io queues off failed\n");
		goto error;
	}

	if (lio_dev->fn_list.setup_device_regs(lio_dev)) {
		lio_dev_err(lio_dev, "Failed to configure device registers\n");
		goto error;
	}

	if (lio_setup_instr_queue0(lio_dev)) {
		lio_dev_err(lio_dev, "Failed to setup instruction queue 0\n");
		goto error;
	}

	dpdk_queues = (int)lio_dev->sriov_info.rings_per_vf;

	lio_dev->max_tx_queues = dpdk_queues;
	lio_dev->max_rx_queues = dpdk_queues;

	return 0;

error:
	if (lio_dev->mbox[0])
		lio_dev->fn_list.free_mbox(lio_dev);
	if (lio_dev->instr_queue[0])
		lio_free_instr_queue0(lio_dev);

	return -1;
}

static int
lio_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	return 0;
}

static int
lio_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_DEV_TO_PCI(eth_dev->device);
	struct lio_device *lio_dev = LIO_DEV(eth_dev);

	PMD_INIT_FUNC_TRACE();

	/* Primary does the initialization. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(eth_dev, pdev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	if (pdev->mem_resource[0].addr) {
		lio_dev->hw_addr = pdev->mem_resource[0].addr;
	} else {
		PMD_INIT_LOG(ERR, "ERROR: Failed to map BAR0\n");
		return -ENODEV;
	}

	lio_dev->eth_dev = eth_dev;
	/* set lio device print string */
	snprintf(lio_dev->dev_string, sizeof(lio_dev->dev_string),
		 "%s[%02x:%02x.%x]", pdev->driver->driver.name,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function);

	lio_dev->port_id = eth_dev->data->port_id;

	if (lio_first_time_init(lio_dev, pdev)) {
		lio_dev_err(lio_dev, "Device init failed\n");
		return -EINVAL;
	}

	eth_dev->data->mac_addrs = rte_zmalloc("lio", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		lio_dev_err(lio_dev,
			    "MAC addresses memory allocation failed\n");
		return -ENOMEM;
	}

	return 0;
}

/* Set of PCI devices this driver supports */
static const struct rte_pci_id pci_id_liovf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, LIO_CN23XX_VF_VID) },
	{ .vendor_id = 0, /* sentinel */ }
};

static struct eth_driver rte_liovf_pmd = {
	.pci_drv = {
		.id_table	= pci_id_liovf_map,
		.drv_flags      = RTE_PCI_DRV_NEED_MAPPING,
		.probe		= rte_eth_dev_pci_probe,
		.remove		= rte_eth_dev_pci_remove,
	},
	.eth_dev_init		= lio_eth_dev_init,
	.eth_dev_uninit		= lio_eth_dev_uninit,
	.dev_private_size	= sizeof(struct lio_device),
};

RTE_PMD_REGISTER_PCI(net_liovf, rte_liovf_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_liovf, pci_id_liovf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_liovf, "* igb_uio | vfio");
