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

static uint64_t
lio_hweight64(uint64_t w)
{
	uint64_t res = w - ((w >> 1) & 0x5555555555555555ul);

	res =
	    (res & 0x3333333333333333ul) + ((res >> 2) & 0x3333333333333333ul);
	res = (res + (res >> 4)) & 0x0F0F0F0F0F0F0F0Ful;
	res = res + (res >> 8);
	res = res + (res >> 16);

	return (res + (res >> 32)) & 0x00000000000000FFul;
}

/**
 * Setup our receive queue/ringbuffer. This is the
 * queue the Octeon uses to send us packets and
 * responses. We are given a memory pool for our
 * packet buffers that are used to populate the receive
 * queue.
 *
 * @param eth_dev
 *    Pointer to the structure rte_eth_dev
 * @param q_no
 *    Queue number
 * @param num_rx_descs
 *    Number of entries in the queue
 * @param socket_id
 *    Where to allocate memory
 * @param rx_conf
 *    Pointer to the struction rte_eth_rxconf
 * @param mp
 *    Pointer to the packet pool
 *
 * @return
 *    - On success, return 0
 *    - On failure, return -1
 */
static int
lio_dev_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t q_no,
		       uint16_t num_rx_descs, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf __rte_unused,
		       struct rte_mempool *mp)
{
	struct lio_device *lio_dev = LIO_DEV(eth_dev);
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint32_t fw_mapped_oq;
	uint16_t buf_size;

	if (q_no >= lio_dev->nb_rx_queues) {
		lio_dev_err(lio_dev, "Invalid rx queue number %u\n", q_no);
		return -EINVAL;
	}

	lio_dev_dbg(lio_dev, "setting up rx queue %u\n", q_no);

	fw_mapped_oq = lio_dev->linfo.rxpciq[q_no].s.q_no;

	if ((lio_dev->droq[fw_mapped_oq]) &&
	    (num_rx_descs != lio_dev->droq[fw_mapped_oq]->max_count)) {
		lio_dev_err(lio_dev,
			    "Reconfiguring Rx descs not supported. Configure descs to same value %u or restart application\n",
			    lio_dev->droq[fw_mapped_oq]->max_count);
		return -ENOTSUP;
	}

	mbp_priv = rte_mempool_get_priv(mp);
	buf_size = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;

	if (lio_setup_droq(lio_dev, fw_mapped_oq, num_rx_descs, buf_size, mp,
			   socket_id)) {
		lio_dev_err(lio_dev, "droq allocation failed\n");
		return -1;
	}

	eth_dev->data->rx_queues[q_no] = lio_dev->droq[fw_mapped_oq];

	return 0;
}

static int lio_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct lio_device *lio_dev = LIO_DEV(eth_dev);
	uint16_t timeout = LIO_MAX_CMD_TIMEOUT;
	int retval, num_iqueues, num_oqueues;
	uint8_t mac[ETHER_ADDR_LEN], i;
	struct lio_if_cfg_resp *resp;
	struct lio_soft_command *sc;
	union lio_if_cfg if_cfg;
	uint32_t resp_size;

	PMD_INIT_FUNC_TRACE();

	/* Re-configuring firmware not supported.
	 * Can't change tx/rx queues per port from initial value.
	 */
	if (lio_dev->port_configured) {
		if ((lio_dev->nb_rx_queues != eth_dev->data->nb_rx_queues) ||
		    (lio_dev->nb_tx_queues != eth_dev->data->nb_tx_queues)) {
			lio_dev_err(lio_dev,
				    "rxq/txq re-conf not supported. Restart application with new value.\n");
			return -ENOTSUP;
		}
		return 0;
	}

	lio_dev->nb_rx_queues = eth_dev->data->nb_rx_queues;
	lio_dev->nb_tx_queues = eth_dev->data->nb_tx_queues;

	resp_size = sizeof(struct lio_if_cfg_resp);
	sc = lio_alloc_soft_command(lio_dev, 0, resp_size, 0);
	if (sc == NULL)
		return -ENOMEM;

	resp = (struct lio_if_cfg_resp *)sc->virtrptr;

	/* Firmware doesn't have capability to reconfigure the queues,
	 * Claim all queues, and use as many required
	 */
	if_cfg.if_cfg64 = 0;
	if_cfg.s.num_iqueues = lio_dev->nb_tx_queues;
	if_cfg.s.num_oqueues = lio_dev->nb_rx_queues;
	if_cfg.s.base_queue = 0;

	if_cfg.s.gmx_port_id = lio_dev->pf_num;

	lio_prepare_soft_command(lio_dev, sc, LIO_OPCODE,
				 LIO_OPCODE_IF_CFG, 0,
				 if_cfg.if_cfg64, 0);

	/* Setting wait time in seconds */
	sc->wait_time = LIO_MAX_CMD_TIMEOUT / 1000;

	retval = lio_send_soft_command(lio_dev, sc);
	if (retval == LIO_IQ_SEND_FAILED) {
		lio_dev_err(lio_dev, "iq/oq config failed status: %x\n",
			    retval);
		/* Soft instr is freed by driver in case of failure. */
		goto nic_config_fail;
	}

	/* Sleep on a wait queue till the cond flag indicates that the
	 * response arrived or timed-out.
	 */
	while ((*sc->status_word == LIO_COMPLETION_WORD_INIT) && --timeout) {
		lio_process_ordered_list(lio_dev);
		rte_delay_ms(1);
	}

	retval = resp->status;
	if (retval) {
		lio_dev_err(lio_dev, "iq/oq config failed\n");
		goto nic_config_fail;
	}

	lio_swap_8B_data((uint64_t *)(&resp->cfg_info),
			 sizeof(struct octeon_if_cfg_info) >> 3);

	num_iqueues = lio_hweight64(resp->cfg_info.iqmask);
	num_oqueues = lio_hweight64(resp->cfg_info.oqmask);

	if (!(num_iqueues) || !(num_oqueues)) {
		lio_dev_err(lio_dev,
			    "Got bad iqueues (%016lx) or oqueues (%016lx) from firmware.\n",
			    (unsigned long)resp->cfg_info.iqmask,
			    (unsigned long)resp->cfg_info.oqmask);
		goto nic_config_fail;
	}

	lio_dev_dbg(lio_dev,
		    "interface %d, iqmask %016lx, oqmask %016lx, numiqueues %d, numoqueues %d\n",
		    eth_dev->data->port_id,
		    (unsigned long)resp->cfg_info.iqmask,
		    (unsigned long)resp->cfg_info.oqmask,
		    num_iqueues, num_oqueues);

	lio_dev->linfo.num_rxpciq = num_oqueues;
	lio_dev->linfo.num_txpciq = num_iqueues;

	for (i = 0; i < num_oqueues; i++) {
		lio_dev->linfo.rxpciq[i].rxpciq64 =
		    resp->cfg_info.linfo.rxpciq[i].rxpciq64;
		lio_dev_dbg(lio_dev, "index %d OQ %d\n",
			    i, lio_dev->linfo.rxpciq[i].s.q_no);
	}

	for (i = 0; i < num_iqueues; i++) {
		lio_dev->linfo.txpciq[i].txpciq64 =
		    resp->cfg_info.linfo.txpciq[i].txpciq64;
		lio_dev_dbg(lio_dev, "index %d IQ %d\n",
			    i, lio_dev->linfo.txpciq[i].s.q_no);
	}

	lio_dev->linfo.hw_addr = resp->cfg_info.linfo.hw_addr;
	lio_dev->linfo.gmxport = resp->cfg_info.linfo.gmxport;
	lio_dev->linfo.link.link_status64 =
			resp->cfg_info.linfo.link.link_status64;

	/* 64-bit swap required on LE machines */
	lio_swap_8B_data(&lio_dev->linfo.hw_addr, 1);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		mac[i] = *((uint8_t *)(((uint8_t *)&lio_dev->linfo.hw_addr) +
				       2 + i));

	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *)mac, &eth_dev->data->mac_addrs[0]);

	lio_dev->port_configured = 1;

	lio_free_soft_command(sc);

	return 0;

nic_config_fail:
	lio_dev_err(lio_dev, "Failed retval %d\n", retval);
	lio_free_soft_command(sc);
	lio_free_instr_queue0(lio_dev);

	return -ENODEV;
}

/* Define our ethernet definitions */
static const struct eth_dev_ops liovf_eth_dev_ops = {
	.dev_configure		= lio_dev_configure,
	.rx_queue_setup		= lio_dev_rx_queue_setup,
};

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

	/* Initialize soft command buffer pool */
	if (lio_setup_sc_buffer_pool(lio_dev)) {
		lio_dev_err(lio_dev, "sc buffer pool allocation failed\n");
		return -1;
	}

	/* Initialize lists to manage the requests of different types that
	 * arrive from applications for this lio device.
	 */
	lio_setup_response_list(lio_dev);

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
	lio_free_sc_buffer_pool(lio_dev);
	if (lio_dev->mbox[0])
		lio_dev->fn_list.free_mbox(lio_dev);
	if (lio_dev->instr_queue[0])
		lio_free_instr_queue0(lio_dev);

	return -1;
}

static int
lio_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct lio_device *lio_dev = LIO_DEV(eth_dev);

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	/* lio_free_sc_buffer_pool */
	lio_free_sc_buffer_pool(lio_dev);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	eth_dev->rx_pkt_burst = NULL;

	return 0;
}

static int
lio_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_DEV_TO_PCI(eth_dev->device);
	struct lio_device *lio_dev = LIO_DEV(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->rx_pkt_burst = &lio_dev_recv_pkts;

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

	eth_dev->dev_ops = &liovf_eth_dev_ops;
	eth_dev->data->mac_addrs = rte_zmalloc("lio", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		lio_dev_err(lio_dev,
			    "MAC addresses memory allocation failed\n");
		eth_dev->dev_ops = NULL;
		eth_dev->rx_pkt_burst = NULL;
		return -ENOMEM;
	}

	rte_atomic64_set(&lio_dev->status, LIO_DEV_RUNNING);
	rte_wmb();

	lio_dev->port_configured = 0;
	/* Always allow unicast packets */
	lio_dev->ifflags |= LIO_IFFLAG_UNICAST;

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
