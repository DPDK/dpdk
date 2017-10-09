/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Semihalf. All rights reserved.
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
 *     * Neither the name of Semihalf nor the names of its
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

#include <rte_ethdev.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_vdev.h>

/* Unluckily, container_of is defined by both DPDK and MUSDK,
 * we'll declare only one version.
 *
 * Note that it is not used in this PMD anyway.
 */
#ifdef container_of
#undef container_of
#endif

#include <drivers/mv_pp2.h>
#include <drivers/mv_pp2_bpool.h>
#include <drivers/mv_pp2_hif.h>

#include <fcntl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mrvl_ethdev.h"
#include "mrvl_qos.h"

/* bitmask with reserved hifs */
#define MRVL_MUSDK_HIFS_RESERVED 0x0F
/* bitmask with reserved bpools */
#define MRVL_MUSDK_BPOOLS_RESERVED 0x07
/* maximum number of available hifs */
#define MRVL_MUSDK_HIFS_MAX 9

#define MRVL_MAC_ADDRS_MAX 1
#define MRVL_MATCH_LEN 16
#define MRVL_PKT_EFFEC_OFFS (MRVL_PKT_OFFS + MV_MH_SIZE)
/* Maximum allowable packet size */
#define MRVL_PKT_SIZE_MAX (10240 - MV_MH_SIZE)

#define MRVL_IFACE_NAME_ARG "iface"
#define MRVL_CFG_ARG "cfg"

#define MRVL_BURST_SIZE 64

#define MRVL_ARP_LENGTH 28

#define MRVL_COOKIE_ADDR_INVALID ~0ULL

#define MRVL_COOKIE_HIGH_ADDR_SHIFT	(sizeof(pp2_cookie_t) * 8)
#define MRVL_COOKIE_HIGH_ADDR_MASK	(~0ULL << MRVL_COOKIE_HIGH_ADDR_SHIFT)

static const char * const valid_args[] = {
	MRVL_IFACE_NAME_ARG,
	MRVL_CFG_ARG,
	NULL
};

static int used_hifs = MRVL_MUSDK_HIFS_RESERVED;
static struct pp2_hif *hifs[RTE_MAX_LCORE];
static int used_bpools[PP2_NUM_PKT_PROC] = {
	MRVL_MUSDK_BPOOLS_RESERVED,
	MRVL_MUSDK_BPOOLS_RESERVED
};

uint64_t cookie_addr_high = MRVL_COOKIE_ADDR_INVALID;

struct mrvl_rxq {
	struct mrvl_priv *priv;
	struct rte_mempool *mp;
	int queue_id;
	int port_id;
};

struct mrvl_txq {
	struct mrvl_priv *priv;
	int queue_id;
	int port_id;
};

/** Number of ports configured. */
int mrvl_ports_nb;

static inline int
mrvl_reserve_bit(int *bitmap, int max)
{
	int n = sizeof(*bitmap) * 8 - __builtin_clz(*bitmap);

	if (n >= max)
		return -1;

	*bitmap |= 1 << n;

	return n;
}

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_dev_configure(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	int ret;

	if (dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_NONE) {
		RTE_LOG(INFO, PMD, "Unsupported rx multi queue mode %d\n",
			dev->data->dev_conf.rxmode.mq_mode);
		return -EINVAL;
	}

	if (!dev->data->dev_conf.rxmode.hw_strip_crc) {
		RTE_LOG(INFO, PMD,
			"L2 CRC stripping is always enabled in hw\n");
		dev->data->dev_conf.rxmode.hw_strip_crc = 1;
	}

	if (dev->data->dev_conf.rxmode.hw_vlan_strip) {
		RTE_LOG(INFO, PMD, "VLAN stripping not supported\n");
		return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.split_hdr_size) {
		RTE_LOG(INFO, PMD, "Split headers not supported\n");
		return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.enable_scatter) {
		RTE_LOG(INFO, PMD, "RX Scatter/Gather not supported\n");
		return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.enable_lro) {
		RTE_LOG(INFO, PMD, "LRO not supported\n");
		return -EINVAL;
	}

	ret = mrvl_configure_rxqs(priv, dev->data->port_id,
				  dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	priv->ppio_params.outqs_params.num_outqs = dev->data->nb_tx_queues;
	priv->nb_rx_queues = dev->data->nb_rx_queues;

	return 0;
}

/**
 * DPDK callback to bring the link up.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	int ret;

	ret = pp2_ppio_enable(priv->ppio);
	if (ret)
		return ret;

	dev->data->dev_link.link_status = ETH_LINK_UP;

	return ret;
}

/**
 * DPDK callback to bring the link down.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	int ret;

	ret = pp2_ppio_disable(priv->ppio);
	if (ret)
		return ret;

	dev->data->dev_link.link_status = ETH_LINK_DOWN;

	return ret;
}

/**
 * DPDK callback to start the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mrvl_dev_start(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	char match[MRVL_MATCH_LEN];
	int ret;

	snprintf(match, sizeof(match), "ppio-%d:%d",
		 priv->pp_id, priv->ppio_id);
	priv->ppio_params.match = match;

	/*
	 * Calculate the maximum bpool size for refill feature to 1.5 of the
	 * configured size. In case the bpool size will exceed this value,
	 * superfluous buffers will be removed
	 */
	priv->bpool_max_size = priv->bpool_init_size +
			      (priv->bpool_init_size >> 1);
	/*
	 * Calculate the minimum bpool size for refill feature as follows:
	 * 2 default burst sizes multiply by number of rx queues.
	 * If the bpool size will be below this value, new buffers will
	 * be added to the pool.
	 */
	priv->bpool_min_size = priv->nb_rx_queues * MRVL_BURST_SIZE * 2;

	ret = pp2_ppio_init(&priv->ppio_params, &priv->ppio);
	if (ret)
		return ret;

	/* For default QoS config, don't start classifier. */
	if (mrvl_qos_cfg) {
		ret = mrvl_start_qos_mapping(priv);
		if (ret) {
			pp2_ppio_deinit(priv->ppio);
			return ret;
		}
	}

	ret = mrvl_dev_set_link_up(dev);
	if (ret)
		goto out;

	return 0;
out:
	pp2_ppio_deinit(priv->ppio);
	return ret;
}

/**
 * Flush receive queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mrvl_flush_rx_queues(struct rte_eth_dev *dev)
{
	int i;

	RTE_LOG(INFO, PMD, "Flushing rx queues\n");
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		int ret, num;

		do {
			struct mrvl_rxq *q = dev->data->rx_queues[i];
			struct pp2_ppio_desc descs[MRVL_PP2_RXD_MAX];

			num = MRVL_PP2_RXD_MAX;
			ret = pp2_ppio_recv(q->priv->ppio,
					    q->priv->rxq_map[q->queue_id].tc,
					    q->priv->rxq_map[q->queue_id].inq,
					    descs, (uint16_t *)&num);
		} while (ret == 0 && num);
	}
}

/**
 * Flush hardware bpool (buffer-pool).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mrvl_flush_bpool(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	uint32_t num;
	int ret;

	ret = pp2_bpool_get_num_buffs(priv->bpool, &num);
	if (ret) {
		RTE_LOG(ERR, PMD, "Failed to get bpool buffers number\n");
		return;
	}

	while (num--) {
		struct pp2_buff_inf inf;
		uint64_t addr;

		ret = pp2_bpool_get_buff(hifs[rte_lcore_id()], priv->bpool,
					 &inf);
		if (ret)
			break;

		addr = cookie_addr_high | inf.cookie;
		rte_pktmbuf_free((struct rte_mbuf *)addr);
	}
}

/**
 * DPDK callback to stop the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mrvl_dev_stop(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	mrvl_dev_set_link_down(dev);
	mrvl_flush_rx_queues(dev);
	if (priv->qos_tbl)
		pp2_cls_qos_tbl_deinit(priv->qos_tbl);
	pp2_ppio_deinit(priv->ppio);
	priv->ppio = NULL;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mrvl_dev_close(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	size_t i;

	for (i = 0; i < priv->ppio_params.inqs_params.num_tcs; ++i) {
		struct pp2_ppio_tc_params *tc_params =
			&priv->ppio_params.inqs_params.tcs_params[i];

		if (tc_params->inqs_params) {
			rte_free(tc_params->inqs_params);
			tc_params->inqs_params = NULL;
		}
	}

	mrvl_flush_bpool(dev);
}

/**
 * DPDK callback to set the primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 */
static void
mrvl_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	pp2_ppio_set_mac_addr(priv->ppio, mac_addr->addr_bytes);
	/*
	 * TODO
	 * Port stops sending packets if pp2_ppio_set_mac_addr()
	 * was called after pp2_ppio_enable(). As a quick fix issue
	 * enable port once again.
	 */
	pp2_ppio_enable(priv->ppio);
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure (unused).
 * @param info
 *   Info structure output buffer.
 */
static void
mrvl_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		   struct rte_eth_dev_info *info)
{
	info->max_rx_queues = MRVL_PP2_RXQ_MAX;
	info->max_tx_queues = MRVL_PP2_TXQ_MAX;
	info->max_mac_addrs = MRVL_MAC_ADDRS_MAX;

	info->rx_desc_lim.nb_max = MRVL_PP2_RXD_MAX;
	info->rx_desc_lim.nb_min = MRVL_PP2_RXD_MIN;
	info->rx_desc_lim.nb_align = MRVL_PP2_RXD_ALIGN;

	info->tx_desc_lim.nb_max = MRVL_PP2_TXD_MAX;
	info->tx_desc_lim.nb_min = MRVL_PP2_TXD_MIN;
	info->tx_desc_lim.nb_align = MRVL_PP2_TXD_ALIGN;

	/* By default packets are dropped if no descriptors are available */
	info->default_rxconf.rx_drop_en = 1;

	info->max_rx_pktlen = MRVL_PKT_SIZE_MAX;
}

/**
 * DPDK callback to get information about specific receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Receive queue index.
 * @param qinfo
 *   Receive queue information structure.
 */
static void mrvl_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			      struct rte_eth_rxq_info *qinfo)
{
	struct mrvl_rxq *q = dev->data->rx_queues[rx_queue_id];
	struct mrvl_priv *priv = dev->data->dev_private;
	int inq = priv->rxq_map[rx_queue_id].inq;
	int tc = priv->rxq_map[rx_queue_id].tc;
	struct pp2_ppio_tc_params *tc_params =
		&priv->ppio_params.inqs_params.tcs_params[tc];

	qinfo->mp = q->mp;
	qinfo->nb_desc = tc_params->inqs_params[inq].size;
}

/**
 * DPDK callback to get information about specific transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param tx_queue_id
 *   Transmit queue index.
 * @param qinfo
 *   Transmit queue information structure.
 */
static void mrvl_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			      struct rte_eth_txq_info *qinfo)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	qinfo->nb_desc =
		priv->ppio_params.outqs_params.outqs_params[tx_queue_id].size;
}

/**
 * Release buffers to hardware bpool (buffer-pool)
 *
 * @param rxq
 *   Receive queue pointer.
 * @param num
 *   Number of buffers to release to bpool.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_fill_bpool(struct mrvl_rxq *rxq, int num)
{
	struct buff_release_entry entries[MRVL_PP2_TXD_MAX];
	struct rte_mbuf *mbufs[MRVL_PP2_TXD_MAX];
	int i, ret;
	unsigned int core_id = rte_lcore_id();
	struct pp2_hif *hif = hifs[core_id];
	struct pp2_bpool *bpool = rxq->priv->bpool;

	ret = rte_pktmbuf_alloc_bulk(rxq->mp, mbufs, num);
	if (ret)
		return ret;

	if (cookie_addr_high == MRVL_COOKIE_ADDR_INVALID)
		cookie_addr_high =
			(uint64_t)mbufs[0] & MRVL_COOKIE_HIGH_ADDR_MASK;

	for (i = 0; i < num; i++) {
		if (((uint64_t)mbufs[i] & MRVL_COOKIE_HIGH_ADDR_MASK)
			!= cookie_addr_high) {
			RTE_LOG(ERR, PMD,
				"mbuf virtual addr high 0x%lx out of range\n",
				(uint64_t)mbufs[i] >> 32);
			goto out;
		}

		entries[i].buff.addr =
			rte_mbuf_data_dma_addr_default(mbufs[i]);
		entries[i].buff.cookie = (pp2_cookie_t)(uint64_t)mbufs[i];
		entries[i].bpool = bpool;
	}

	pp2_bpool_put_buffs(hif, entries, (uint16_t *)&i);

	if (i != num)
		goto out;

	return 0;
out:
	for (; i < num; i++)
		rte_pktmbuf_free(mbufs[i]);

	return -1;
}

/**
 * DPDK callback to configure the receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param conf
 *   Thresholds parameters (unused_).
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket,
		    const struct rte_eth_rxconf *conf __rte_unused,
		    struct rte_mempool *mp)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_rxq *rxq;
	uint32_t min_size,
		 max_rx_pkt_len = dev->data->dev_conf.rxmode.max_rx_pkt_len;
	int ret, tc, inq;

	if (priv->rxq_map[idx].tc == MRVL_UNKNOWN_TC) {
		/*
		 * Unknown TC mapping, mapping will not have a correct queue.
		 */
		RTE_LOG(ERR, PMD, "Unknown TC mapping for queue %hu eth%hhu\n",
			idx, priv->ppio_id);
		return -EFAULT;
	}

	min_size = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM -
		   MRVL_PKT_EFFEC_OFFS;
	if (min_size < max_rx_pkt_len) {
		RTE_LOG(ERR, PMD,
			"Mbuf size must be increased to %u bytes to hold up to %u bytes of data.\n",
			max_rx_pkt_len + RTE_PKTMBUF_HEADROOM +
			MRVL_PKT_EFFEC_OFFS,
			max_rx_pkt_len);
		return -EINVAL;
	}

	if (dev->data->rx_queues[idx]) {
		rte_free(dev->data->rx_queues[idx]);
		dev->data->rx_queues[idx] = NULL;
	}

	rxq = rte_zmalloc_socket("rxq", sizeof(*rxq), 0, socket);
	if (!rxq)
		return -ENOMEM;

	rxq->priv = priv;
	rxq->mp = mp;
	rxq->queue_id = idx;
	rxq->port_id = dev->data->port_id;

	tc = priv->rxq_map[rxq->queue_id].tc,
	inq = priv->rxq_map[rxq->queue_id].inq;
	priv->ppio_params.inqs_params.tcs_params[tc].inqs_params[inq].size =
		desc;

	ret = mrvl_fill_bpool(rxq, desc);
	if (ret) {
		rte_free(rxq);
		return ret;
	}

	priv->bpool_init_size += desc;

	dev->data->rx_queues[idx] = rxq;

	return 0;
}

/**
 * DPDK callback to release the receive queue.
 *
 * @param rxq
 *   Generic receive queue pointer.
 */
static void
mrvl_rx_queue_release(void *rxq)
{
	struct mrvl_rxq *q = rxq;
	struct pp2_ppio_tc_params *tc_params;
	int i, num, tc, inq;

	if (!q)
		return;

	tc = q->priv->rxq_map[q->queue_id].tc;
	inq = q->priv->rxq_map[q->queue_id].inq;
	tc_params = &q->priv->ppio_params.inqs_params.tcs_params[tc];
	num = tc_params->inqs_params[inq].size;
	for (i = 0; i < num; i++) {
		struct pp2_buff_inf inf;
		uint64_t addr;

		pp2_bpool_get_buff(hifs[rte_lcore_id()], q->priv->bpool, &inf);
		addr = cookie_addr_high | inf.cookie;
		rte_pktmbuf_free((struct rte_mbuf *)addr);
	}

	rte_free(q);
}

/**
 * DPDK callback to configure the transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Transmit queue index.
 * @param desc
 *   Number of descriptors to configure in the queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param conf
 *   Thresholds parameters (unused).
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket,
		    const struct rte_eth_txconf *conf __rte_unused)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct mrvl_txq *txq;

	if (dev->data->tx_queues[idx]) {
		rte_free(dev->data->tx_queues[idx]);
		dev->data->tx_queues[idx] = NULL;
	}

	txq = rte_zmalloc_socket("txq", sizeof(*txq), 0, socket);
	if (!txq)
		return -ENOMEM;

	txq->priv = priv;
	txq->queue_id = idx;
	txq->port_id = dev->data->port_id;
	dev->data->tx_queues[idx] = txq;

	priv->ppio_params.outqs_params.outqs_params[idx].size = desc;
	priv->ppio_params.outqs_params.outqs_params[idx].weight = 1;

	return 0;
}

/**
 * DPDK callback to release the transmit queue.
 *
 * @param txq
 *   Generic transmit queue pointer.
 */
static void
mrvl_tx_queue_release(void *txq)
{
	struct mrvl_txq *q = txq;

	if (!q)
		return;

	rte_free(q);
}

static const struct eth_dev_ops mrvl_ops = {
	.dev_configure = mrvl_dev_configure,
	.dev_start = mrvl_dev_start,
	.dev_stop = mrvl_dev_stop,
	.dev_set_link_up = mrvl_dev_set_link_up,
	.dev_set_link_down = mrvl_dev_set_link_down,
	.dev_close = mrvl_dev_close,
	.mac_addr_set = mrvl_mac_addr_set,
	.dev_infos_get = mrvl_dev_infos_get,
	.rxq_info_get = mrvl_rxq_info_get,
	.txq_info_get = mrvl_txq_info_get,
	.rx_queue_setup = mrvl_rx_queue_setup,
	.rx_queue_release = mrvl_rx_queue_release,
	.tx_queue_setup = mrvl_tx_queue_setup,
	.tx_queue_release = mrvl_tx_queue_release,
};

/**
 * Initialize packet processor.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_init_pp2(void)
{
	struct pp2_init_params init_params;

	memset(&init_params, 0, sizeof(init_params));
	init_params.hif_reserved_map = MRVL_MUSDK_HIFS_RESERVED;
	init_params.bm_pool_reserved_map = MRVL_MUSDK_BPOOLS_RESERVED;

	return pp2_init(&init_params);
}

/**
 * Deinitialize packet processor.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static void
mrvl_deinit_pp2(void)
{
	pp2_deinit();
}

/**
 * Create private device structure.
 *
 * @param dev_name
 *   Pointer to the port name passed in the initialization parameters.
 *
 * @return
 *   Pointer to the newly allocated private device structure.
 */
static struct mrvl_priv *
mrvl_priv_create(const char *dev_name)
{
	struct pp2_bpool_params bpool_params;
	char match[MRVL_MATCH_LEN];
	struct mrvl_priv *priv;
	int ret, bpool_bit;

	priv = rte_zmalloc_socket(dev_name, sizeof(*priv), 0, rte_socket_id());
	if (!priv)
		return NULL;

	ret = pp2_netdev_get_ppio_info((char *)(uintptr_t)dev_name,
				       &priv->pp_id, &priv->ppio_id);
	if (ret)
		goto out_free_priv;

	bpool_bit = mrvl_reserve_bit(&used_bpools[priv->pp_id],
				     PP2_BPOOL_NUM_POOLS);
	if (bpool_bit < 0)
		goto out_free_priv;
	priv->bpool_bit = bpool_bit;

	snprintf(match, sizeof(match), "pool-%d:%d", priv->pp_id,
		 priv->bpool_bit);
	memset(&bpool_params, 0, sizeof(bpool_params));
	bpool_params.match = match;
	bpool_params.buff_len = MRVL_PKT_SIZE_MAX + MRVL_PKT_EFFEC_OFFS;
	ret = pp2_bpool_init(&bpool_params, &priv->bpool);
	if (ret)
		goto out_clear_bpool_bit;

	priv->ppio_params.type = PP2_PPIO_T_NIC;

	return priv;
out_clear_bpool_bit:
	used_bpools[priv->pp_id] &= ~(1 << priv->bpool_bit);
out_free_priv:
	rte_free(priv);
	return NULL;
}

/**
 * Create device representing Ethernet port.
 *
 * @param name
 *   Pointer to the port's name.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_eth_dev_create(struct rte_vdev_device *vdev, const char *name)
{
	int ret, fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct rte_eth_dev *eth_dev;
	struct mrvl_priv *priv;
	struct ifreq req;

	eth_dev = rte_eth_dev_allocate(name);
	if (!eth_dev)
		return -ENOMEM;

	priv = mrvl_priv_create(name);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_dev;
	}

	eth_dev->data->mac_addrs =
		rte_zmalloc("mac_addrs",
			    ETHER_ADDR_LEN * MRVL_MAC_ADDRS_MAX, 0);
	if (!eth_dev->data->mac_addrs) {
		RTE_LOG(ERR, PMD, "Failed to allocate space for eth addrs\n");
		ret = -ENOMEM;
		goto out_free_priv;
	}

	memset(&req, 0, sizeof(req));
	strcpy(req.ifr_name, name);
	ret = ioctl(fd, SIOCGIFHWADDR, &req);
	if (ret)
		goto out_free_mac;

	memcpy(eth_dev->data->mac_addrs[0].addr_bytes,
	       req.ifr_addr.sa_data, ETHER_ADDR_LEN);

	eth_dev->data->dev_private = priv;
	eth_dev->device = &vdev->device;
	eth_dev->dev_ops = &mrvl_ops;

	return 0;
out_free_mac:
	rte_free(eth_dev->data->mac_addrs);
out_free_dev:
	rte_eth_dev_release_port(eth_dev);
out_free_priv:
	rte_free(priv);

	return ret;
}

/**
 * Cleanup previously created device representing Ethernet port.
 *
 * @param name
 *   Pointer to the port name.
 */
static void
mrvl_eth_dev_destroy(const char *name)
{
	struct rte_eth_dev *eth_dev;
	struct mrvl_priv *priv;

	eth_dev = rte_eth_dev_allocated(name);
	if (!eth_dev)
		return;

	priv = eth_dev->data->dev_private;
	pp2_bpool_deinit(priv->bpool);
	rte_free(priv);
	rte_free(eth_dev->data->mac_addrs);
	rte_eth_dev_release_port(eth_dev);
}

/**
 * Callback used by rte_kvargs_process() during argument parsing.
 *
 * @param key
 *   Pointer to the parsed key (unused).
 * @param value
 *   Pointer to the parsed value.
 * @param extra_args
 *   Pointer to the extra arguments which contains address of the
 *   table of pointers to parsed interface names.
 *
 * @return
 *   Always 0.
 */
static int
mrvl_get_ifnames(const char *key __rte_unused, const char *value,
		 void *extra_args)
{
	const char **ifnames = extra_args;

	ifnames[mrvl_ports_nb++] = value;

	return 0;
}

/**
 * Initialize per-lcore MUSDK hardware interfaces (hifs).
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mrvl_init_hifs(void)
{
	struct pp2_hif_params params;
	char match[MRVL_MATCH_LEN];
	int i, ret;

	RTE_LCORE_FOREACH(i) {
		ret = mrvl_reserve_bit(&used_hifs, MRVL_MUSDK_HIFS_MAX);
		if (ret < 0)
			return ret;

		snprintf(match, sizeof(match), "hif-%d", ret);
		memset(&params, 0, sizeof(params));
		params.match = match;
		params.out_size = MRVL_PP2_AGGR_TXQD_MAX;
		ret = pp2_hif_init(&params, &hifs[i]);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to initialize hif %d\n", i);
			return ret;
		}
	}

	return 0;
}

/**
 * Deinitialize per-lcore MUSDK hardware interfaces (hifs).
 */
static void
mrvl_deinit_hifs(void)
{
	int i;

	RTE_LCORE_FOREACH(i) {
		if (hifs[i])
			pp2_hif_deinit(hifs[i]);
	}
}

/**
 * DPDK callback to register the virtual device.
 *
 * @param vdev
 *   Pointer to the virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
rte_pmd_mrvl_probe(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist;
	const char *ifnames[PP2_NUM_ETH_PPIO * PP2_NUM_PKT_PROC];
	int ret = -EINVAL;
	uint32_t i, ifnum, cfgnum;
	const char *params;

	params = rte_vdev_device_args(vdev);
	if (!params)
		return -EINVAL;

	kvlist = rte_kvargs_parse(params, valid_args);
	if (!kvlist)
		return -EINVAL;

	ifnum = rte_kvargs_count(kvlist, MRVL_IFACE_NAME_ARG);
	if (ifnum > RTE_DIM(ifnames))
		goto out_free_kvlist;

	rte_kvargs_process(kvlist, MRVL_IFACE_NAME_ARG,
			   mrvl_get_ifnames, &ifnames);

	cfgnum = rte_kvargs_count(kvlist, MRVL_CFG_ARG);
	if (cfgnum > 1) {
		RTE_LOG(ERR, PMD, "Cannot handle more than one config file!\n");
		goto out_free_kvlist;
	} else if (cfgnum == 1) {
		rte_kvargs_process(kvlist, MRVL_CFG_ARG,
				   mrvl_get_qoscfg, &mrvl_qos_cfg);
	}

	/*
	 * ret == -EEXIST is correct, it means DMA
	 * has been already initialized (by another PMD).
	 */
	ret = mv_sys_dma_mem_init(RTE_MRVL_MUSDK_DMA_MEMSIZE);
	if (ret < 0 && ret != -EEXIST)
		goto out_free_kvlist;

	ret = mrvl_init_pp2();
	if (ret) {
		RTE_LOG(ERR, PMD, "Failed to init PP!\n");
		goto out_deinit_dma;
	}

	ret = mrvl_init_hifs();
	if (ret)
		goto out_deinit_hifs;

	for (i = 0; i < ifnum; i++) {
		RTE_LOG(INFO, PMD, "Creating %s\n", ifnames[i]);
		ret = mrvl_eth_dev_create(vdev, ifnames[i]);
		if (ret)
			goto out_cleanup;
	}

	rte_kvargs_free(kvlist);

	return 0;
out_cleanup:
	for (; i > 0; i--)
		mrvl_eth_dev_destroy(ifnames[i]);
out_deinit_hifs:
	mrvl_deinit_hifs();
	mrvl_deinit_pp2();
out_deinit_dma:
	mv_sys_dma_mem_destroy();
out_free_kvlist:
	rte_kvargs_free(kvlist);

	return ret;
}

/**
 * DPDK callback to remove virtual device.
 *
 * @param vdev
 *   Pointer to the removed virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
rte_pmd_mrvl_remove(struct rte_vdev_device *vdev)
{
	int i;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (!name)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Removing %s\n", name);

	for (i = 0; i < rte_eth_dev_count(); i++) {
		char ifname[RTE_ETH_NAME_MAX_LEN];

		rte_eth_dev_get_name_by_port(i, ifname);
		mrvl_eth_dev_destroy(ifname);
	}

	mrvl_deinit_hifs();
	mrvl_deinit_pp2();
	mv_sys_dma_mem_destroy();

	return 0;
}

static struct rte_vdev_driver pmd_mrvl_drv = {
	.probe = rte_pmd_mrvl_probe,
	.remove = rte_pmd_mrvl_remove,
};

RTE_PMD_REGISTER_VDEV(net_mrvl, pmd_mrvl_drv);
RTE_PMD_REGISTER_ALIAS(net_mrvl, eth_mrvl);
