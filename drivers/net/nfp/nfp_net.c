/*
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: Main entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/io.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_version.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>

#include "nfp_net_pmd.h"
#include "nfp_net_logs.h"
#include "nfp_net_ctrl.h"

/* Prototypes */
static void nfp_net_close(struct rte_eth_dev *dev);
static int nfp_net_configure(struct rte_eth_dev *dev);
static int nfp_net_init(struct rte_eth_dev *eth_dev);
static int nfp_net_rx_fill_freelist(struct nfp_net_rxq *rxq);
static uint32_t nfp_net_rx_queue_count(struct rte_eth_dev *dev,
				       uint16_t queue_idx);
static uint16_t nfp_net_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
static void nfp_net_rx_queue_release(void *rxq);
static int nfp_net_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				  uint16_t nb_desc, unsigned int socket_id,
				  const struct rte_eth_rxconf *rx_conf,
				  struct rte_mempool *mp);
static int nfp_net_tx_free_bufs(struct nfp_net_txq *txq);
static void nfp_net_tx_queue_release(void *txq);
static int nfp_net_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				  uint16_t nb_desc, unsigned int socket_id,
				  const struct rte_eth_txconf *tx_conf);
static int nfp_net_start(struct rte_eth_dev *dev);
static void nfp_net_stop(struct rte_eth_dev *dev);
static uint16_t nfp_net_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);

/*
 * The offset of the queue controller queues in the PCIe Target. These
 * happen to be at the same offset on the NFP6000 and the NFP3200 so
 * we use a single macro here.
 */
#define NFP_PCIE_QUEUE(_q)	(0x80000 + (0x800 * ((_q) & 0xff)))

/* Maximum value which can be added to a queue with one transaction */
#define NFP_QCP_MAX_ADD	0x7f

#define RTE_MBUF_DMA_ADDR_DEFAULT(mb) \
	(uint64_t)((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM)

/* nfp_qcp_ptr - Read or Write Pointer of a queue */
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

/*
 * nfp_qcp_ptr_add - Add the value to the selected pointer of a queue
 * @q: Base address for queue structure
 * @ptr: Add to the Read or Write pointer
 * @val: Value to add to the queue pointer
 *
 * If @val is greater than @NFP_QCP_MAX_ADD multiple writes are performed.
 */
static inline void
nfp_qcp_ptr_add(uint8_t *q, enum nfp_qcp_ptr ptr, uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

	while (val > NFP_QCP_MAX_ADD) {
		nn_writel(rte_cpu_to_le_32(NFP_QCP_MAX_ADD), q + off);
		val -= NFP_QCP_MAX_ADD;
	}

	nn_writel(rte_cpu_to_le_32(val), q + off);
}

/*
 * nfp_qcp_read - Read the current Read/Write pointer value for a queue
 * @q:  Base address for queue structure
 * @ptr: Read or Write pointer
 */
static inline uint32_t
nfp_qcp_read(uint8_t *q, enum nfp_qcp_ptr ptr)
{
	uint32_t off;
	uint32_t val;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_STS_LO;
	else
		off = NFP_QCP_QUEUE_STS_HI;

	val = rte_cpu_to_le_32(nn_readl(q + off));

	if (ptr == NFP_QCP_READ_PTR)
		return val & NFP_QCP_QUEUE_STS_LO_READPTR_mask;
	else
		return val & NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask;
}

/*
 * Functions to read/write from/to Config BAR
 * Performs any endian conversion necessary.
 */
static inline uint8_t
nn_cfg_readb(struct nfp_net_hw *hw, int off)
{
	return nn_readb(hw->ctrl_bar + off);
}

static inline void
nn_cfg_writeb(struct nfp_net_hw *hw, int off, uint8_t val)
{
	nn_writeb(val, hw->ctrl_bar + off);
}

static inline uint32_t
nn_cfg_readl(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_32(nn_readl(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writel(struct nfp_net_hw *hw, int off, uint32_t val)
{
	nn_writel(rte_cpu_to_le_32(val), hw->ctrl_bar + off);
}

static inline uint64_t
nn_cfg_readq(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_64(nn_readq(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writeq(struct nfp_net_hw *hw, int off, uint64_t val)
{
	nn_writeq(rte_cpu_to_le_64(val), hw->ctrl_bar + off);
}

/* Creating memzone for hardware rings. */
static const struct rte_memzone *
ring_dma_zone_reserve(struct rte_eth_dev *dev, const char *ring_name,
		      uint16_t queue_id, uint32_t ring_size, int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
		 dev->driver->pci_drv.name,
		 ring_name, dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, ring_size, socket_id, 0,
					   NFP_MEMZONE_ALIGN);
}

static void
nfp_net_rx_queue_release_mbufs(struct nfp_net_rxq *rxq)
{
	unsigned i;

	if (rxq->rxbufs == NULL)
		return;

	for (i = 0; i < rxq->rx_count; i++) {
		if (rxq->rxbufs[i].mbuf) {
			rte_pktmbuf_free_seg(rxq->rxbufs[i].mbuf);
			rxq->rxbufs[i].mbuf = NULL;
		}
	}
}

static void
nfp_net_rx_queue_release(void *rx_queue)
{
	struct nfp_net_rxq *rxq = rx_queue;

	if (rxq) {
		nfp_net_rx_queue_release_mbufs(rxq);
		rte_free(rxq->rxbufs);
		rte_free(rxq);
	}
}

static void
nfp_net_reset_rx_queue(struct nfp_net_rxq *rxq)
{
	nfp_net_rx_queue_release_mbufs(rxq);
	rxq->wr_p = 0;
	rxq->rd_p = 0;
	rxq->nb_rx_hold = 0;
}

static void
nfp_net_tx_queue_release_mbufs(struct nfp_net_txq *txq)
{
	unsigned i;

	if (txq->txbufs == NULL)
		return;

	for (i = 0; i < txq->tx_count; i++) {
		if (txq->txbufs[i].mbuf) {
			rte_pktmbuf_free_seg(txq->txbufs[i].mbuf);
			txq->txbufs[i].mbuf = NULL;
		}
	}
}

static void
nfp_net_tx_queue_release(void *tx_queue)
{
	struct nfp_net_txq *txq = tx_queue;

	if (txq) {
		nfp_net_tx_queue_release_mbufs(txq);
		rte_free(txq->txbufs);
		rte_free(txq);
	}
}

static void
nfp_net_reset_tx_queue(struct nfp_net_txq *txq)
{
	nfp_net_tx_queue_release_mbufs(txq);
	txq->wr_p = 0;
	txq->rd_p = 0;
	txq->tail = 0;
}

static int
__nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t update)
{
	int cnt;
	uint32_t new;
	struct timespec wait;

	PMD_DRV_LOG(DEBUG, "Writing to the configuration queue (%p)...\n",
		    hw->qcp_cfg);

	if (hw->qcp_cfg == NULL)
		rte_panic("Bad configuration queue pointer\n");

	nfp_qcp_ptr_add(hw->qcp_cfg, NFP_QCP_WRITE_PTR, 1);

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;

	PMD_DRV_LOG(DEBUG, "Polling for update ack...\n");

	/* Poll update field, waiting for NFP to ack the config */
	for (cnt = 0; ; cnt++) {
		new = nn_cfg_readl(hw, NFP_NET_CFG_UPDATE);
		if (new == 0)
			break;
		if (new & NFP_NET_CFG_UPDATE_ERR) {
			PMD_INIT_LOG(ERR, "Reconfig error: 0x%08x\n", new);
			return -1;
		}
		if (cnt >= NFP_NET_POLL_TIMEOUT) {
			PMD_INIT_LOG(ERR, "Reconfig timeout for 0x%08x after"
					  " %dms\n", update, cnt);
			rte_panic("Exiting\n");
		}
		nanosleep(&wait, 0); /* waiting for a 1ms */
	}
	PMD_DRV_LOG(DEBUG, "Ack DONE\n");
	return 0;
}

/*
 * Reconfigure the NIC
 * @nn:    device to reconfigure
 * @ctrl:    The value for the ctrl field in the BAR config
 * @update:  The value for the update field in the BAR config
 *
 * Write the update word to the BAR and ping the reconfig queue. Then poll
 * until the firmware has acknowledged the update by zeroing the update word.
 */
static int
nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update)
{
	uint32_t err;

	PMD_DRV_LOG(DEBUG, "nfp_net_reconfig: ctrl=%08x update=%08x\n",
		    ctrl, update);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, ctrl);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	err = __nfp_net_reconfig(hw, update);

	if (!err)
		return 0;

	/*
	 * Reconfig errors imply situations where they can be handled.
	 * Otherwise, rte_panic is called inside __nfp_net_reconfig
	 */
	PMD_INIT_LOG(ERR, "Error nfp_net reconfig for ctrl: %x update: %x\n",
		     ctrl, update);
	return -EIO;
}

/*
 * Configure an Ethernet device. This function must be invoked first
 * before any other function in the Ethernet API. This function can
 * also be re-invoked when a device is in the stopped state.
 */
static int
nfp_net_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	uint32_t new_ctrl = 0;
	uint32_t update = 0;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * A DPDK app sends info about how many queues to use and how
	 * those queues need to be configured. This is used by the
	 * DPDK core and it makes sure no more queues than those
	 * advertised by the driver are requested. This function is
	 * called after that internal process
	 */

	PMD_INIT_LOG(DEBUG, "Configure\n");

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	/* Checking TX mode */
	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "TX mq_mode DCB and VMDq not supported\n");
		return -EINVAL;
	}

	/* Checking RX mode */
	if (rxmode->mq_mode & ETH_MQ_RX_RSS) {
		if (hw->cap & NFP_NET_CFG_CTRL_RSS) {
			update = NFP_NET_CFG_UPDATE_RSS;
			new_ctrl = NFP_NET_CFG_CTRL_RSS;
		} else {
			PMD_INIT_LOG(INFO, "RSS not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->split_hdr_size) {
		PMD_INIT_LOG(INFO, "rxmode does not support split header\n");
		return -EINVAL;
	}

	if (rxmode->hw_ip_checksum) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM) {
			new_ctrl |= NFP_NET_CFG_CTRL_RXCSUM;
		} else {
			PMD_INIT_LOG(INFO, "RXCSUM not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->hw_vlan_filter) {
		PMD_INIT_LOG(INFO, "VLAN filter not supported\n");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_strip) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXVLAN) {
			new_ctrl |= NFP_NET_CFG_CTRL_RXVLAN;
		} else {
			PMD_INIT_LOG(INFO, "hw vlan strip not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->hw_vlan_extend) {
		PMD_INIT_LOG(INFO, "VLAN extended not supported\n");
		return -EINVAL;
	}

	/* Supporting VLAN insertion by default */
	if (hw->cap & NFP_NET_CFG_CTRL_TXVLAN)
		new_ctrl |= NFP_NET_CFG_CTRL_TXVLAN;

	if (rxmode->jumbo_frame)
		/* this is handled in rte_eth_dev_configure */

	if (rxmode->hw_strip_crc) {
		PMD_INIT_LOG(INFO, "strip CRC not supported\n");
		return -EINVAL;
	}

	if (rxmode->enable_scatter) {
		PMD_INIT_LOG(INFO, "Scatter not supported\n");
		return -EINVAL;
	}

	if (!new_ctrl)
		return 0;

	update |= NFP_NET_CFG_UPDATE_GEN;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	return 0;
}

static void
nfp_net_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint64_t enabled_queues = 0;
	int i;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Enabling the required TX queues in the device */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, enabled_queues);

	enabled_queues = 0;

	/* Enabling the required RX queues in the device */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, enabled_queues);
}

static void
nfp_net_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint32_t new_ctrl, update = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, 0);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, 0);

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
		 NFP_NET_CFG_UPDATE_MSIX;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl &= ~NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return;

	hw->ctrl = new_ctrl;
}

static int
nfp_net_rx_freelist_setup(struct rte_eth_dev *dev)
{
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (nfp_net_rx_fill_freelist(dev->data->rx_queues[i]) < 0)
			return -1;
	}
	return 0;
}

static void
nfp_net_params_setup(struct nfp_net_hw *hw)
{
	uint32_t *mac_address;

	nn_cfg_writel(hw, NFP_NET_CFG_MTU, hw->mtu);
	nn_cfg_writel(hw, NFP_NET_CFG_FLBUFSZ, hw->flbufsz);

	/* A MAC address is 8 bytes long */
	mac_address = (uint32_t *)(hw->mac_addr);

	nn_cfg_writel(hw, NFP_NET_CFG_MACADDR,
		      rte_cpu_to_be_32(*mac_address));
	nn_cfg_writel(hw, NFP_NET_CFG_MACADDR + 4,
		      rte_cpu_to_be_32(*(mac_address + 4)));
}

static void
nfp_net_cfg_queue_setup(struct nfp_net_hw *hw)
{
	hw->qcp_cfg = hw->tx_bar + NFP_QCP_QUEUE_ADDR_SZ;
}

static int
nfp_net_start(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_LOG(DEBUG, "Start\n");

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Enable device */
	new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_ENABLE | NFP_NET_CFG_UPDATE_MSIX;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return -EIO;

	/*
	 * Allocating rte mbuffs for configured rx queues.
	 * This requires queues being enabled before
	 */
	if (nfp_net_rx_freelist_setup(dev) < 0) {
		ret = -ENOMEM;
		goto error;
	}

	hw->ctrl = new_ctrl;

	return 0;

error:
	/*
	 * An error returned by this function should mean the app
	 * exiting and then the system releasing all the memory
	 * allocated even memory coming from hugepages.
	 *
	 * The device could be enabled at this point with some queues
	 * ready for getting packets. This is true if the call to
	 * nfp_net_rx_freelist_setup() succeeds for some queues but
	 * fails for subsequent queues.
	 *
	 * This should make the app exiting but better if we tell the
	 * device first.
	 */
	nfp_net_disable_queues(dev);

	return ret;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
static void
nfp_net_stop(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_LOG(DEBUG, "Stop\n");

	nfp_net_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		nfp_net_reset_tx_queue(
			(struct nfp_net_txq *)dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		nfp_net_reset_rx_queue(
			(struct nfp_net_rxq *)dev->data->rx_queues[i]);
	}
}

/* Reset and stop device. The device can not be restarted. */
static void
nfp_net_close(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_INIT_LOG(DEBUG, "Close\n");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */

	nfp_net_stop(dev);

	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);

	/*
	 * The ixgbe PMD driver disables the pcie master on the
	 * device. The i40e does not...
	 */
}

static uint32_t
nfp_net_rx_queue_count(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	struct nfp_net_rxq *rxq;
	struct nfp_net_rx_desc *rxds;
	uint32_t idx;
	uint32_t count;

	rxq = (struct nfp_net_rxq *)dev->data->rx_queues[queue_idx];

	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Bad queue: %u\n", queue_idx);
		return 0;
	}

	idx = rxq->rd_p % rxq->rx_count;
	rxds = &rxq->rxds[idx];

	count = 0;

	/*
	 * Other PMDs are just checking the DD bit in intervals of 4
	 * descriptors and counting all four if the first has the DD
	 * bit on. Of course, this is not accurate but can be good for
	 * perfomance. But ideally that should be done in descriptors
	 * chunks belonging to the same cache line
	 */

	while (count < rxq->rx_count) {
		rxds = &rxq->rxds[idx];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		count++;
		idx++;

		/* Wrapping? */
		if ((idx) == rxq->rx_count)
			idx = 0;
	}

	return count;
}

static int
nfp_net_rx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx, uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	const struct rte_memzone *tz;
	struct nfp_net_rxq *rxq;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* Validating number of descriptors */
	if (((nb_desc * sizeof(struct nfp_net_rx_desc)) % 128) != 0 ||
	    (nb_desc > NFP_NET_MAX_RX_DESC) ||
	    (nb_desc < NFP_NET_MIN_RX_DESC)) {
		RTE_LOG(ERR, PMD, "Wrong nb_desc value\n");
		return (-EINVAL);
	}

	/*
	 * Free memory prior to re-allocation if needed. This is the case after
	 * calling nfp_net_stop
	 */
	if (dev->data->rx_queues[queue_idx]) {
		nfp_net_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocating rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct nfp_net_rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return (-ENOMEM);

	/* Hw queues mapping based on firmware confifguration */
	rxq->qidx = queue_idx;
	rxq->fl_qcidx = queue_idx * hw->stride_rx;
	rxq->rx_qcidx = rxq->fl_qcidx + (hw->stride_rx - 1);
	rxq->qcp_fl = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->fl_qcidx);
	rxq->qcp_rx = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->rx_qcidx);

	/*
	 * Tracking mbuf size for detecting a potential mbuf overflow due to
	 * RX offset
	 */
	rxq->mem_pool = mp;
	rxq->mbuf_size = rxq->mem_pool->elt_size;
	rxq->mbuf_size -= (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
	hw->flbufsz = rxq->mbuf_size;

	rxq->rx_count = nb_desc;
	rxq->port_id = dev->data->port_id;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->crc_len = (uint8_t) ((dev->data->dev_conf.rxmode.hw_strip_crc) ? 0
				  : ETHER_CRC_LEN);
	rxq->drop_en = rx_conf->rx_drop_en;

	/*
	 * Allocate RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = ring_dma_zone_reserve(dev, "rx_ring", queue_idx,
				   sizeof(struct nfp_net_rx_desc) *
				   NFP_NET_MAX_RX_DESC, socket_id);

	if (tz == NULL) {
		RTE_LOG(ERR, PMD, "Error allocatig rx dma\n");
		nfp_net_rx_queue_release(rxq);
		return (-ENOMEM);
	}

	/* Saving physical and virtual addresses for the RX ring */
	rxq->dma = (uint64_t)tz->phys_addr;
	rxq->rxds = (struct nfp_net_rx_desc *)tz->addr;

	/* mbuf pointers array for referencing mbufs linked to RX descriptors */
	rxq->rxbufs = rte_zmalloc_socket("rxq->rxbufs",
					 sizeof(*rxq->rxbufs) * nb_desc,
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->rxbufs == NULL) {
		nfp_net_rx_queue_release(rxq);
		return (-ENOMEM);
	}

	PMD_RX_LOG(DEBUG, "rxbufs=%p hw_ring=%p dma_addr=0x%" PRIx64 "\n",
		   rxq->rxbufs, rxq->rxds, (unsigned long int)rxq->dma);

	nfp_net_reset_rx_queue(rxq);

	dev->data->rx_queues[queue_idx] = rxq;
	rxq->hw = hw;

	/*
	 * Telling the HW about the physical address of the RX ring and number
	 * of descriptors in log2 format
	 */
	nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(queue_idx), rxq->dma);
	nn_cfg_writeb(hw, NFP_NET_CFG_RXR_SZ(queue_idx), log2(nb_desc));

	return 0;
}

static int
nfp_net_rx_fill_freelist(struct nfp_net_rxq *rxq)
{
	struct nfp_net_rx_buff *rxe = rxq->rxbufs;
	uint64_t dma_addr;
	unsigned i;

	PMD_RX_LOG(DEBUG, "nfp_net_rx_fill_freelist for %u descriptors\n",
		   rxq->rx_count);

	for (i = 0; i < rxq->rx_count; i++) {
		struct nfp_net_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_pktmbuf_alloc(rxq->mem_pool);

		if (mbuf == NULL) {
			RTE_LOG(ERR, PMD, "RX mbuf alloc failed queue_id=%u\n",
				(unsigned)rxq->qidx);
			return (-ENOMEM);
		}

		dma_addr = rte_cpu_to_le_64(RTE_MBUF_DMA_ADDR_DEFAULT(mbuf));

		rxd = &rxq->rxds[i];
		rxd->fld.dd = 0;
		rxd->fld.dma_addr_hi = (dma_addr >> 32) & 0xff;
		rxd->fld.dma_addr_lo = dma_addr & 0xffffffff;
		rxe[i].mbuf = mbuf;
		PMD_RX_LOG(DEBUG, "[%d]: %" PRIx64 "\n", i, dma_addr);

		rxq->wr_p++;
	}

	/* Make sure all writes are flushed before telling the hardware */
	rte_wmb();

	/* Not advertising the whole ring as the firmware gets confused if so */
	PMD_RX_LOG(DEBUG, "Increment FL write pointer in %u\n",
		   rxq->rx_count - 1);

	nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, rxq->rx_count - 1);

	return 0;
}

static int
nfp_net_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct nfp_net_txq *txq;
	uint16_t tx_free_thresh;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* Validating number of descriptors */
	if (((nb_desc * sizeof(struct nfp_net_tx_desc)) % 128) != 0 ||
	    (nb_desc > NFP_NET_MAX_TX_DESC) ||
	    (nb_desc < NFP_NET_MIN_TX_DESC)) {
		RTE_LOG(ERR, PMD, "Wrong nb_desc value\n");
		return -EINVAL;
	}

	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
				    tx_conf->tx_free_thresh :
				    DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh > (nb_desc)) {
		RTE_LOG(ERR, PMD,
			"tx_free_thresh must be less than the number of TX "
			"descriptors. (tx_free_thresh=%u port=%d "
			"queue=%d)\n", (unsigned int)tx_free_thresh,
			(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * Free memory prior to re-allocation if needed. This is the case after
	 * calling nfp_net_stop
	 */
	if (dev->data->tx_queues[queue_idx]) {
		PMD_TX_LOG(DEBUG, "Freeing memory prior to re-allocation %d\n",
			   queue_idx);
		nfp_net_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct nfp_net_txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		RTE_LOG(ERR, PMD, "Error allocating tx dma\n");
		return (-ENOMEM);
	}

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = ring_dma_zone_reserve(dev, "tx_ring", queue_idx,
				   sizeof(struct nfp_net_tx_desc) *
				   NFP_NET_MAX_TX_DESC, socket_id);
	if (tz == NULL) {
		RTE_LOG(ERR, PMD, "Error allocating tx dma\n");
		nfp_net_tx_queue_release(txq);
		return (-ENOMEM);
	}

	txq->tx_count = nb_desc;
	txq->tail = 0;
	txq->tx_free_thresh = tx_free_thresh;
	txq->tx_pthresh = tx_conf->tx_thresh.pthresh;
	txq->tx_hthresh = tx_conf->tx_thresh.hthresh;
	txq->tx_wthresh = tx_conf->tx_thresh.wthresh;

	/* queue mapping based on firmware configuration */
	txq->qidx = queue_idx;
	txq->tx_qcidx = queue_idx * hw->stride_tx;
	txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);

	txq->port_id = dev->data->port_id;
	txq->txq_flags = tx_conf->txq_flags;

	/* Saving physical and virtual addresses for the TX ring */
	txq->dma = (uint64_t)tz->phys_addr;
	txq->txds = (struct nfp_net_tx_desc *)tz->addr;

	/* mbuf pointers array for referencing mbufs linked to TX descriptors */
	txq->txbufs = rte_zmalloc_socket("txq->txbufs",
					 sizeof(*txq->txbufs) * nb_desc,
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->txbufs == NULL) {
		nfp_net_tx_queue_release(txq);
		return (-ENOMEM);
	}
	PMD_TX_LOG(DEBUG, "txbufs=%p hw_ring=%p dma_addr=0x%" PRIx64 "\n",
		   txq->txbufs, txq->txds, (unsigned long int)txq->dma);

	nfp_net_reset_tx_queue(txq);

	dev->data->tx_queues[queue_idx] = txq;
	txq->hw = hw;

	/*
	 * Telling the HW about the physical address of the TX ring and number
	 * of descriptors in log2 format
	 */
	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(queue_idx), txq->dma);
	nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(queue_idx), log2(nb_desc));

	return 0;
}

/* nfp_net_tx_cksum - Set TX CSUM offload flags in TX descriptor */
static inline void
nfp_net_tx_cksum(struct nfp_net_txq *txq, struct nfp_net_tx_desc *txd,
		 struct rte_mbuf *mb)
{
	uint16_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if (!(hw->cap & NFP_NET_CFG_CTRL_TXCSUM))
		return;

	ol_flags = mb->ol_flags;

	/* IPv6 does not need checksum */
	if (ol_flags & PKT_TX_IP_CKSUM)
		txd->flags |= PCIE_DESC_TX_IP4_CSUM;

	switch (ol_flags & PKT_TX_L4_MASK) {
	case PKT_TX_UDP_CKSUM:
		txd->flags |= PCIE_DESC_TX_UDP_CSUM;
		break;
	case PKT_TX_TCP_CKSUM:
		txd->flags |= PCIE_DESC_TX_TCP_CSUM;
		break;
	}

	txd->flags |= PCIE_DESC_TX_CSUM;
}

/* nfp_net_rx_cksum - set mbuf checksum flags based on RX descriptor flags */
static inline void
nfp_net_rx_cksum(struct nfp_net_rxq *rxq, struct nfp_net_rx_desc *rxd,
		 struct rte_mbuf *mb)
{
	struct nfp_net_hw *hw = rxq->hw;

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RXCSUM))
		return;

	/* If IPv4 and IP checksum error, fail */
	if ((rxd->rxd.flags & PCIE_DESC_RX_IP4_CSUM) &&
	    !(rxd->rxd.flags & PCIE_DESC_RX_IP4_CSUM_OK))
		mb->ol_flags |= PKT_RX_IP_CKSUM_BAD;

	/* If neither UDP nor TCP return */
	if (!(rxd->rxd.flags & PCIE_DESC_RX_TCP_CSUM) &&
	    !(rxd->rxd.flags & PCIE_DESC_RX_UDP_CSUM))
		return;

	if ((rxd->rxd.flags & PCIE_DESC_RX_TCP_CSUM) &&
	    !(rxd->rxd.flags & PCIE_DESC_RX_TCP_CSUM_OK))
		mb->ol_flags |= PKT_RX_L4_CKSUM_BAD;

	if ((rxd->rxd.flags & PCIE_DESC_RX_UDP_CSUM) &&
	    !(rxd->rxd.flags & PCIE_DESC_RX_UDP_CSUM_OK))
		mb->ol_flags |= PKT_RX_L4_CKSUM_BAD;
}

#define NFP_HASH_OFFSET      ((uint8_t *)mbuf->buf_addr + mbuf->data_off - 4)
#define NFP_HASH_TYPE_OFFSET ((uint8_t *)mbuf->buf_addr + mbuf->data_off - 8)

/*
 * nfp_net_set_hash - Set mbuf hash data
 *
 * The RSS hash and hash-type are pre-pended to the packet data.
 * Extract and decode it and set the mbuf fields.
 */
static inline void
nfp_net_set_hash(struct nfp_net_rxq *rxq, struct nfp_net_rx_desc *rxd,
		 struct rte_mbuf *mbuf)
{
	uint32_t hash;
	uint32_t hash_type;
	struct nfp_net_hw *hw = rxq->hw;

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RSS))
		return;

	if (!(rxd->rxd.flags & PCIE_DESC_RX_RSS))
		return;

	hash = rte_be_to_cpu_32(*(uint32_t *)NFP_HASH_OFFSET);
	hash_type = rte_be_to_cpu_32(*(uint32_t *)NFP_HASH_TYPE_OFFSET);

	/*
	 * hash type is sharing the same word with input port info
	 * 31-8: input port
	 * 7:0: hash type
	 */
	hash_type &= 0xff;
	mbuf->hash.rss = hash;
	mbuf->ol_flags |= PKT_RX_RSS_HASH;

	switch (hash_type) {
	case NFP_NET_RSS_IPV4:
		mbuf->packet_type |= RTE_PTYPE_INNER_L3_IPV4;
		break;
	case NFP_NET_RSS_IPV6:
		mbuf->packet_type |= RTE_PTYPE_INNER_L3_IPV6;
		break;
	case NFP_NET_RSS_IPV6_EX:
		mbuf->packet_type |= RTE_PTYPE_INNER_L3_IPV6_EXT;
		break;
	default:
		mbuf->packet_type |= RTE_PTYPE_INNER_L4_MASK;
	}
}

/* nfp_net_check_port - Set mbuf in_port field */
static void
nfp_net_check_port(struct nfp_net_rx_desc *rxd, struct rte_mbuf *mbuf)
{
	uint32_t port;

	if (!(rxd->rxd.flags & PCIE_DESC_RX_INGRESS_PORT)) {
		mbuf->port = 0;
		return;
	}

	port = rte_be_to_cpu_32(*(uint32_t *)((uint8_t *)mbuf->buf_addr +
					      mbuf->data_off - 8));

	/*
	 * hash type is sharing the same word with input port info
	 * 31-8: input port
	 * 7:0: hash type
	 */
	port = (uint8_t)(port >> 8);
	mbuf->port = port;
}

static inline void
nfp_net_mbuf_alloc_failed(struct nfp_net_rxq *rxq)
{
	rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
}

#define NFP_DESC_META_LEN(d) (d->rxd.meta_len_dd & PCIE_DESC_RX_META_LEN_MASK)

/*
 * RX path design:
 *
 * There are some decissions to take:
 * 1) How to check DD RX descriptors bit
 * 2) How and when to allocate new mbufs
 *
 * Current implementation checks just one single DD bit each loop. As each
 * descriptor is 8 bytes, it is likely a good idea to check descriptors in
 * a single cache line instead. Tests with this change have not shown any
 * performance improvement but it requires further investigation. For example,
 * depending on which descriptor is next, the number of descriptors could be
 * less than 8 for just checking those in the same cache line. This implies
 * extra work which could be counterproductive by itself. Indeed, last firmware
 * changes are just doing this: writing several descriptors with the DD bit
 * for saving PCIe bandwidth and DMA operations from the NFP.
 *
 * Mbuf allocation is done when a new packet is received. Then the descriptor
 * is automatically linked with the new mbuf and the old one is given to the
 * user. The main drawback with this design is mbuf allocation is heavier than
 * using bulk allocations allowed by DPDK with rte_mempool_get_bulk. From the
 * cache point of view it does not seem allocating the mbuf early on as we are
 * doing now have any benefit at all. Again, tests with this change have not
 * shown any improvement. Also, rte_mempool_get_bulk returns all or nothing
 * so looking at the implications of this type of allocation should be studied
 * deeply
 */

static uint16_t
nfp_net_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct nfp_net_rxq *rxq;
	struct nfp_net_rx_desc *rxds;
	struct nfp_net_rx_buff *rxb;
	struct nfp_net_hw *hw;
	struct rte_mbuf *mb;
	struct rte_mbuf *new_mb;
	int idx;
	uint16_t nb_hold;
	uint64_t dma_addr;
	int avail;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured
		 */
		RTE_LOG(ERR, PMD, "RX Bad queue\n");
		return -EINVAL;
	}

	hw = rxq->hw;
	avail = 0;
	nb_hold = 0;

	while (avail < nb_pkts) {
		idx = rxq->rd_p % rxq->rx_count;

		rxb = &rxq->rxbufs[idx];
		if (unlikely(rxb == NULL)) {
			RTE_LOG(ERR, PMD, "rxb does not exist!\n");
			break;
		}

		/*
		 * Memory barrier to ensure that we won't do other
		 * reads before the DD bit.
		 */
		rte_rmb();

		rxds = &rxq->rxds[idx];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		/*
		 * We got a packet. Let's alloc a new mbuff for refilling the
		 * free descriptor ring as soon as possible
		 */
		new_mb = rte_pktmbuf_alloc(rxq->mem_pool);
		if (unlikely(new_mb == NULL)) {
			RTE_LOG(DEBUG, PMD, "RX mbuf alloc failed port_id=%u "
				"queue_id=%u\n", (unsigned)rxq->port_id,
				(unsigned)rxq->qidx);
			nfp_net_mbuf_alloc_failed(rxq);
			break;
		}

		nb_hold++;

		/*
		 * Grab the mbuff and refill the descriptor with the
		 * previously allocated mbuff
		 */
		mb = rxb->mbuf;
		rxb->mbuf = new_mb;

		PMD_RX_LOG(DEBUG, "Packet len: %u, mbuf_size: %u\n",
			   rxds->rxd.data_len, rxq->mbuf_size);

		/* Size of this segment */
		mb->data_len = rxds->rxd.data_len - NFP_DESC_META_LEN(rxds);
		/* Size of the whole packet. We just support 1 segment */
		mb->pkt_len = rxds->rxd.data_len - NFP_DESC_META_LEN(rxds);

		if (unlikely((mb->data_len + hw->rx_offset) >
			     rxq->mbuf_size)) {
			/*
			 * This should not happen and the user has the
			 * responsibility of avoiding it. But we have
			 * to give some info about the error
			 */
			RTE_LOG(ERR, PMD,
				"mbuf overflow likely due to the RX offset.\n"
				"\t\tYour mbuf size should have extra space for"
				" RX offset=%u bytes.\n"
				"\t\tCurrently you just have %u bytes available"
				" but the received packet is %u bytes long",
				hw->rx_offset,
				rxq->mbuf_size - hw->rx_offset,
				mb->data_len);
			return -EINVAL;
		}

		/* Filling the received mbuff with packet info */
		if (hw->rx_offset)
			mb->data_off = RTE_PKTMBUF_HEADROOM + hw->rx_offset;
		else
			mb->data_off = RTE_PKTMBUF_HEADROOM +
				       NFP_DESC_META_LEN(rxds);

		/* No scatter mode supported */
		mb->nb_segs = 1;
		mb->next = NULL;

		/* Checking the RSS flag */
		nfp_net_set_hash(rxq, rxds, mb);

		/* Checking the checksum flag */
		nfp_net_rx_cksum(rxq, rxds, mb);

		/* Checking the port flag */
		nfp_net_check_port(rxds, mb);

		if ((rxds->rxd.flags & PCIE_DESC_RX_VLAN) &&
		    (hw->ctrl & NFP_NET_CFG_CTRL_RXVLAN)) {
			mb->vlan_tci = rte_cpu_to_le_32(rxds->rxd.vlan);
			mb->ol_flags |= PKT_RX_VLAN_PKT;
		}

		/* Adding the mbuff to the mbuff array passed by the app */
		rx_pkts[avail++] = mb;

		/* Now resetting and updating the descriptor */
		rxds->vals[0] = 0;
		rxds->vals[1] = 0;
		dma_addr = rte_cpu_to_le_64(RTE_MBUF_DMA_ADDR_DEFAULT(new_mb));
		rxds->fld.dd = 0;
		rxds->fld.dma_addr_hi = (dma_addr >> 32) & 0xff;
		rxds->fld.dma_addr_lo = dma_addr & 0xffffffff;

		rxq->rd_p++;
	}

	if (nb_hold == 0)
		return nb_hold;

	PMD_RX_LOG(DEBUG, "RX  port_id=%u queue_id=%u, %d packets received\n",
		   (unsigned)rxq->port_id, (unsigned)rxq->qidx, nb_hold);

	nb_hold += rxq->nb_rx_hold;

	/*
	 * FL descriptors needs to be written before incrementing the
	 * FL queue WR pointer
	 */
	rte_wmb();
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port=%u queue=%u nb_hold=%u avail=%u\n",
			   (unsigned)rxq->port_id, (unsigned)rxq->qidx,
			   (unsigned)nb_hold, (unsigned)avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return avail;
}

/*
 * nfp_net_tx_free_bufs - Check for descriptors with a complete
 * status
 * @txq: TX queue to work with
 * Returns number of descriptors freed
 */
int
nfp_net_tx_free_bufs(struct nfp_net_txq *txq)
{
	uint32_t qcp_rd_p;
	int todo;

	PMD_TX_LOG(DEBUG, "queue %u. Check for descriptor with a complete"
		   " status\n", txq->qidx);

	/* Work out how many packets have been sent */
	qcp_rd_p = nfp_qcp_read(txq->qcp_q, NFP_QCP_READ_PTR);

	if (qcp_rd_p == txq->qcp_rd_p) {
		PMD_TX_LOG(DEBUG, "queue %u: It seems harrier is not sending "
			   "packets (%u, %u)\n", txq->qidx,
			   qcp_rd_p, txq->qcp_rd_p);
		return 0;
	}

	if (qcp_rd_p > txq->qcp_rd_p)
		todo = qcp_rd_p - txq->qcp_rd_p;
	else
		todo = qcp_rd_p + txq->tx_count - txq->qcp_rd_p;

	PMD_TX_LOG(DEBUG, "qcp_rd_p %u, txq->qcp_rd_p: %u, qcp->rd_p: %u\n",
		   qcp_rd_p, txq->qcp_rd_p, txq->rd_p);

	if (todo == 0)
		return todo;

	txq->qcp_rd_p += todo;
	txq->qcp_rd_p %= txq->tx_count;
	txq->rd_p += todo;

	return todo;
}

/* Leaving always free descriptors for avoiding wrapping confusion */
#define NFP_FREE_TX_DESC(t) (t->tx_count - (t->wr_p - t->rd_p) - 8)

/*
 * nfp_net_txq_full - Check if the TX queue free descriptors
 * is below tx_free_threshold
 *
 * @txq: TX queue to check
 *
 * This function uses the host copy* of read/write pointers
 */
static inline
int nfp_net_txq_full(struct nfp_net_txq *txq)
{
	return NFP_FREE_TX_DESC(txq) < txq->tx_free_thresh;
}

static uint16_t
nfp_net_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct nfp_net_txq *txq;
	struct nfp_net_hw *hw;
	struct nfp_net_tx_desc *txds;
	struct rte_mbuf *pkt;
	uint64_t dma_addr;
	int pkt_size, dma_size;
	uint16_t free_descs, issued_descs;
	struct rte_mbuf **lmbuf;
	int i;

	txq = tx_queue;
	hw = txq->hw;
	txds = &txq->txds[txq->tail];

	PMD_TX_LOG(DEBUG, "working for queue %u at pos %d and %u packets\n",
		   txq->qidx, txq->tail, nb_pkts);

	if ((NFP_FREE_TX_DESC(txq) < nb_pkts) || (nfp_net_txq_full(txq)))
		nfp_net_tx_free_bufs(txq);

	free_descs = (uint16_t)NFP_FREE_TX_DESC(txq);
	if (unlikely(free_descs == 0))
		return 0;

	pkt = *tx_pkts;

	i = 0;
	issued_descs = 0;
	PMD_TX_LOG(DEBUG, "queue: %u. Sending %u packets\n",
		   txq->qidx, nb_pkts);
	/* Sending packets */
	while ((i < nb_pkts) && free_descs) {
		/* Grabbing the mbuf linked to the current descriptor */
		lmbuf = &txq->txbufs[txq->tail].mbuf;
		/* Warming the cache for releasing the mbuf later on */
		RTE_MBUF_PREFETCH_TO_FREE(*lmbuf);

		pkt = *(tx_pkts + i);

		if (unlikely((pkt->nb_segs > 1) &&
			     !(hw->cap & NFP_NET_CFG_CTRL_GATHER))) {
			PMD_INIT_LOG(INFO, "NFP_NET_CFG_CTRL_GATHER not set\n");
			rte_panic("Multisegment packet unsupported\n");
		}

		/* Checking if we have enough descriptors */
		if (unlikely(pkt->nb_segs > free_descs))
			goto xmit_end;

		/*
		 * Checksum and VLAN flags just in the first descriptor for a
		 * multisegment packet
		 */
		nfp_net_tx_cksum(txq, txds, pkt);

		if ((pkt->ol_flags & PKT_TX_VLAN_PKT) &&
		    (hw->cap & NFP_NET_CFG_CTRL_TXVLAN)) {
			txds->flags |= PCIE_DESC_TX_VLAN;
			txds->vlan = pkt->vlan_tci;
		}

		if (pkt->ol_flags & PKT_TX_TCP_SEG)
			rte_panic("TSO is not supported\n");

		/*
		 * mbuf data_len is the data in one segment and pkt_len data
		 * in the whole packet. When the packet is just one segment,
		 * then data_len = pkt_len
		 */
		pkt_size = pkt->pkt_len;

		while (pkt_size) {
			/* Releasing mbuf which was prefetched above */
			if (*lmbuf)
				rte_pktmbuf_free_seg(*lmbuf);

			dma_size = pkt->data_len;
			dma_addr = RTE_MBUF_DATA_DMA_ADDR(pkt);
			PMD_TX_LOG(DEBUG, "Working with mbuf at dma address:"
				   "%" PRIx64 "\n", dma_addr);

			/* Filling descriptors fields */
			txds->dma_len = dma_size;
			txds->data_len = pkt->pkt_len;
			txds->dma_addr_hi = (dma_addr >> 32) & 0xff;
			txds->dma_addr_lo = (dma_addr & 0xffffffff);
			ASSERT(free_descs > 0);
			free_descs--;

			/*
			 * Linking mbuf with descriptor for being released
			 * next time descriptor is used
			 */
			*lmbuf = pkt;

			txq->wr_p++;
			txq->tail++;
			if (unlikely(txq->tail == txq->tx_count)) /* wrapping?*/
				txq->tail = 0;

			pkt_size -= dma_size;
			if (!pkt_size) {
				/* End of packet */
				txds->offset_eop |= PCIE_DESC_TX_EOP;
			} else {
				txds->offset_eop &= PCIE_DESC_TX_OFFSET_MASK;
				pkt = pkt->next;
			}
			/* Referencing next free TX descriptor */
			txds = &txq->txds[txq->tail];
			issued_descs++;
		}
		i++;
	}

xmit_end:
	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, issued_descs);

	return i;
}

/* Initialise and register driver with DPDK Application */
static struct eth_dev_ops nfp_net_eth_dev_ops = {
	.dev_configure		= nfp_net_configure,
	.dev_start		= nfp_net_start,
	.dev_stop		= nfp_net_stop,
	.dev_close		= nfp_net_close,
	.rx_queue_setup		= nfp_net_rx_queue_setup,
	.rx_queue_release	= nfp_net_rx_queue_release,
	.rx_queue_count		= nfp_net_rx_queue_count,
	.tx_queue_setup		= nfp_net_tx_queue_setup,
	.tx_queue_release	= nfp_net_tx_queue_release,
};

static int
nfp_net_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;

	uint32_t tx_bar_off, rx_bar_off;
	uint32_t start_q;
	int stride = 4;

	PMD_INIT_FUNC_TRACE();

	hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	eth_dev->dev_ops = &nfp_net_eth_dev_ops;
	eth_dev->rx_pkt_burst = &nfp_net_recv_pkts;
	eth_dev->tx_pkt_burst = &nfp_net_xmit_pkts;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = eth_dev->pci_dev;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_INIT_LOG(DEBUG, "nfp_net: device (%u:%u) %u:%u:%u:%u\n",
		     pci_dev->id.vendor_id, pci_dev->id.device_id,
		     pci_dev->addr.domain, pci_dev->addr.bus,
		     pci_dev->addr.devid, pci_dev->addr.function);

	hw->ctrl_bar = (uint8_t *)pci_dev->mem_resource[0].addr;
	if (hw->ctrl_bar == NULL) {
		RTE_LOG(ERR, PMD,
			"hw->ctrl_bar is NULL. BAR0 not configured\n");
		return -ENODEV;
	}
	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
		tx_bar_off = NFP_PCIE_QUEUE(start_q);
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
		rx_bar_off = NFP_PCIE_QUEUE(start_q);
		break;
	default:
		RTE_LOG(ERR, PMD, "nfp_net: no device ID matching\n");
		return -ENODEV;
	}

	PMD_INIT_LOG(DEBUG, "tx_bar_off: 0x%08x\n", tx_bar_off);
	PMD_INIT_LOG(DEBUG, "rx_bar_off: 0x%08x\n", rx_bar_off);

	hw->tx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + tx_bar_off;
	hw->rx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + rx_bar_off;

	PMD_INIT_LOG(DEBUG, "ctrl_bar: %p, tx_bar: %p, rx_bar: %p\n",
		     hw->ctrl_bar, hw->tx_bar, hw->rx_bar);

	nfp_net_cfg_queue_setup(hw);

	/* Get some of the read-only fields from the config BAR */
	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	hw->mtu = hw->max_mtu;

	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	PMD_INIT_LOG(INFO, "VER: %#x, Maximum supported MTU: %d\n",
		     hw->ver, hw->max_mtu);
	PMD_INIT_LOG(INFO, "CAP: %#x, %s%s%s%s%s%s%s%s%s\n", hw->cap,
		     hw->cap & NFP_NET_CFG_CTRL_PROMISC ? "PROMISC " : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXCSUM  ? "RXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXCSUM  ? "TXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXVLAN  ? "RXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXVLAN  ? "TXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_SCATTER ? "SCATTER " : "",
		     hw->cap & NFP_NET_CFG_CTRL_GATHER  ? "GATHER "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_LSO     ? "TSO "     : "",
		     hw->cap & NFP_NET_CFG_CTRL_RSS     ? "RSS "     : "");

	pci_dev = eth_dev->pci_dev;
	hw->ctrl = 0;

	hw->stride_rx = stride;
	hw->stride_tx = stride;

	PMD_INIT_LOG(INFO, "max_rx_queues: %u, max_tx_queues: %u\n",
		     hw->max_rx_queues, hw->max_tx_queues);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address");
		return -ENOMEM;
	}

	/* Using random mac addresses for VFs */
	eth_random_addr(&hw->mac_addr[0]);

	/* Copying mac address to DPDK eth_dev struct */
	ether_addr_copy(&eth_dev->data->mac_addrs[0],
			(struct ether_addr *)hw->mac_addr);

	PMD_INIT_LOG(INFO, "port %d VendorID=0x%x DeviceID=0x%x "
		     "mac=%02x:%02x:%02x:%02x:%02x:%02x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id,
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	return 0;
}

static struct rte_pci_id pci_id_nfp_net_map[] = {
	{
		.vendor_id = PCI_VENDOR_ID_NETRONOME,
		.device_id = PCI_DEVICE_ID_NFP6000_PF_NIC,
		.subsystem_vendor_id = PCI_ANY_ID,
		.subsystem_device_id = PCI_ANY_ID,
	},
	{
		.vendor_id = PCI_VENDOR_ID_NETRONOME,
		.device_id = PCI_DEVICE_ID_NFP6000_VF_NIC,
		.subsystem_vendor_id = PCI_ANY_ID,
		.subsystem_device_id = PCI_ANY_ID,
	},
	{
		.vendor_id = 0,
	},
};

static struct eth_driver rte_nfp_net_pmd = {
	{
		.name = "rte_nfp_net_pmd",
		.id_table = pci_id_nfp_net_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = nfp_net_init,
	.dev_private_size = sizeof(struct nfp_net_adapter),
};

static int
nfp_net_pmd_init(const char *name __rte_unused,
		 const char *params __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	PMD_INIT_LOG(INFO, "librte_pmd_nfp_net version %s\n",
		     NFP_NET_PMD_VERSION);

	rte_eth_driver_register(&rte_nfp_net_pmd);
	return 0;
}

static struct rte_driver rte_nfp_net_driver = {
	.type = PMD_PDEV,
	.init = nfp_net_pmd_init,
};

PMD_REGISTER_DRIVER(rte_nfp_net_driver);

/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
