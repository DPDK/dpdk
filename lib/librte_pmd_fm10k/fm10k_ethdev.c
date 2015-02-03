/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2013-2015 Intel Corporation. All rights reserved.
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

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>
#include <rte_dev.h>
#include <rte_spinlock.h>

#include "fm10k.h"
#include "base/fm10k_api.h"

#define FM10K_RX_BUFF_ALIGN 512
/* Default delay to acquire mailbox lock */
#define FM10K_MBXLOCK_DELAY_US 20

/* Number of chars per uint32 type */
#define CHARS_PER_UINT32 (sizeof(uint32_t))
#define BIT_MASK_PER_UINT32 ((1 << CHARS_PER_UINT32) - 1)

static void
fm10k_mbx_initlock(struct fm10k_hw *hw)
{
	rte_spinlock_init(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back));
}

static void
fm10k_mbx_lock(struct fm10k_hw *hw)
{
	while (!rte_spinlock_trylock(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back)))
		rte_delay_us(FM10K_MBXLOCK_DELAY_US);
}

static void
fm10k_mbx_unlock(struct fm10k_hw *hw)
{
	rte_spinlock_unlock(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back));
}

/*
 * clean queue, descriptor rings, free software buffers used when stopping
 * device.
 */
static inline void
rx_queue_clean(struct fm10k_rx_queue *q)
{
	union fm10k_rx_desc zero = {.q = {0, 0, 0, 0} };
	uint32_t i;
	PMD_INIT_FUNC_TRACE();

	/* zero descriptor rings */
	for (i = 0; i < q->nb_desc; ++i)
		q->hw_ring[i] = zero;

	/* free software buffers */
	for (i = 0; i < q->nb_desc; ++i) {
		if (q->sw_ring[i]) {
			rte_pktmbuf_free_seg(q->sw_ring[i]);
			q->sw_ring[i] = NULL;
		}
	}
}

/*
 * free all queue memory used when releasing the queue (i.e. configure)
 */
static inline void
rx_queue_free(struct fm10k_rx_queue *q)
{
	PMD_INIT_FUNC_TRACE();
	if (q) {
		PMD_INIT_LOG(DEBUG, "Freeing rx queue %p", q);
		rx_queue_clean(q);
		if (q->sw_ring)
			rte_free(q->sw_ring);
		rte_free(q);
	}
}

static int
fm10k_dev_configure(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.hw_strip_crc == 0)
		PMD_INIT_LOG(WARNING, "fm10k always strip CRC");

	return 0;
}

static int
fm10k_link_update(struct rte_eth_dev *dev,
	__rte_unused int wait_to_complete)
{
	PMD_INIT_FUNC_TRACE();

	/* The host-interface link is always up.  The speed is ~50Gbps per Gen3
	 * x8 PCIe interface. For now, we leave the speed undefined since there
	 * is no 50Gbps Ethernet. */
	dev->data->dev_link.link_speed  = 0;
	dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	dev->data->dev_link.link_status = 1;

	return 0;
}

static void
fm10k_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint64_t ipackets, opackets, ibytes, obytes;
	struct fm10k_hw *hw =
		FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_hw_stats *hw_stats =
		FM10K_DEV_PRIVATE_TO_STATS(dev->data->dev_private);
	int i;

	PMD_INIT_FUNC_TRACE();

	fm10k_update_hw_stats(hw, hw_stats);

	ipackets = opackets = ibytes = obytes = 0;
	for (i = 0; (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) &&
		(i < FM10K_MAX_QUEUES_PF); ++i) {
		stats->q_ipackets[i] = hw_stats->q[i].rx_packets.count;
		stats->q_opackets[i] = hw_stats->q[i].tx_packets.count;
		stats->q_ibytes[i]   = hw_stats->q[i].rx_bytes.count;
		stats->q_obytes[i]   = hw_stats->q[i].tx_bytes.count;
		ipackets += stats->q_ipackets[i];
		opackets += stats->q_opackets[i];
		ibytes   += stats->q_ibytes[i];
		obytes   += stats->q_obytes[i];
	}
	stats->ipackets = ipackets;
	stats->opackets = opackets;
	stats->ibytes = ibytes;
	stats->obytes = obytes;
}

static void
fm10k_stats_reset(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_hw_stats *hw_stats =
		FM10K_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	memset(hw_stats, 0, sizeof(*hw_stats));
	fm10k_rebind_hw_stats(hw, hw_stats);
}

static void
fm10k_dev_infos_get(struct rte_eth_dev *dev,
	struct rte_eth_dev_info *dev_info)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	dev_info->min_rx_bufsize     = FM10K_MIN_RX_BUF_SIZE;
	dev_info->max_rx_pktlen      = FM10K_MAX_PKT_SIZE;
	dev_info->max_rx_queues      = hw->mac.max_queues;
	dev_info->max_tx_queues      = hw->mac.max_queues;
	dev_info->max_mac_addrs      = 1;
	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vfs            = FM10K_MAX_VF_NUM;
	dev_info->max_vmdq_pools     = ETH_64_POOLS;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM  |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa    = 0;
	dev_info->reta_size = FM10K_MAX_RSS_INDICES;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = FM10K_DEFAULT_RX_PTHRESH,
			.hthresh = FM10K_DEFAULT_RX_HTHRESH,
			.wthresh = FM10K_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = FM10K_RX_FREE_THRESH_DEFAULT(0),
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = FM10K_DEFAULT_TX_PTHRESH,
			.hthresh = FM10K_DEFAULT_TX_HTHRESH,
			.wthresh = FM10K_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = FM10K_TX_FREE_THRESH_DEFAULT(0),
		.tx_rs_thresh = FM10K_TX_RS_THRESH_DEFAULT(0),
		.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS |
				ETH_TXQ_FLAGS_NOOFFLOADS,
	};

}

static inline int
check_nb_desc(uint16_t min, uint16_t max, uint16_t mult, uint16_t request)
{
	if ((request < min) || (request > max) || ((request % mult) != 0))
		return -1;
	else
		return 0;
}

/*
 * Create a memzone for hardware descriptor rings. Malloc cannot be used since
 * the physical address is required. If the memzone is already created, then
 * this function returns a pointer to the existing memzone.
 */
static inline const struct rte_memzone *
allocate_hw_ring(const char *driver_name, const char *ring_name,
	uint8_t port_id, uint16_t queue_id, int socket_id,
	uint32_t size, uint32_t align)
{
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(name, sizeof(name), "%s_%s_%d_%d_%d",
		 driver_name, ring_name, port_id, queue_id, socket_id);

	/* return the memzone if it already exists */
	mz = rte_memzone_lookup(name);
	if (mz)
		return mz;

#ifdef RTE_LIBRTE_XEN_DOM0
	return rte_memzone_reserve_bounded(name, size, socket_id, 0, align,
					   RTE_PGSIZE_2M);
#else
	return rte_memzone_reserve_aligned(name, size, socket_id, 0, align);
#endif
}

static inline int
check_thresh(uint16_t min, uint16_t max, uint16_t div, uint16_t request)
{
	if ((request < min) || (request > max) || ((div % request) != 0))
		return -1;
	else
		return 0;
}

static inline int
handle_rxconf(struct fm10k_rx_queue *q, const struct rte_eth_rxconf *conf)
{
	uint16_t rx_free_thresh;

	if (conf->rx_free_thresh == 0)
		rx_free_thresh = FM10K_RX_FREE_THRESH_DEFAULT(q);
	else
		rx_free_thresh = conf->rx_free_thresh;

	/* make sure the requested threshold satisfies the constraints */
	if (check_thresh(FM10K_RX_FREE_THRESH_MIN(q),
			FM10K_RX_FREE_THRESH_MAX(q),
			FM10K_RX_FREE_THRESH_DIV(q),
			rx_free_thresh)) {
		PMD_INIT_LOG(ERR, "rx_free_thresh (%u) must be "
			"less than or equal to %u, "
			"greater than or equal to %u, "
			"and a divisor of %u",
			rx_free_thresh, FM10K_RX_FREE_THRESH_MAX(q),
			FM10K_RX_FREE_THRESH_MIN(q),
			FM10K_RX_FREE_THRESH_DIV(q));
		return (-EINVAL);
	}

	q->alloc_thresh = rx_free_thresh;
	q->drop_en = conf->rx_drop_en;
	q->rx_deferred_start = conf->rx_deferred_start;

	return 0;
}

/*
 * Hardware requires specific alignment for Rx packet buffers. At
 * least one of the following two conditions must be satisfied.
 *  1. Address is 512B aligned
 *  2. Address is 8B aligned and buffer does not cross 4K boundary.
 *
 * As such, the driver may need to adjust the DMA address within the
 * buffer by up to 512B. The mempool element size is checked here
 * to make sure a maximally sized Ethernet frame can still be wholly
 * contained within the buffer after 512B alignment.
 *
 * return 1 if the element size is valid, otherwise return 0.
 */
static int
mempool_element_size_valid(struct rte_mempool *mp)
{
	uint32_t min_size;

	/* elt_size includes mbuf header and headroom */
	min_size = mp->elt_size - sizeof(struct rte_mbuf) -
			RTE_PKTMBUF_HEADROOM;

	/* account for up to 512B of alignment */
	min_size -= FM10K_RX_BUFF_ALIGN;

	/* sanity check for overflow */
	if (min_size > mp->elt_size)
		return 0;

	if (min_size < ETHER_MAX_VLAN_FRAME_LEN)
		return 0;

	/* size is valid */
	return 1;
}

static int
fm10k_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id,
	uint16_t nb_desc, unsigned int socket_id,
	const struct rte_eth_rxconf *conf, struct rte_mempool *mp)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_rx_queue *q;
	const struct rte_memzone *mz;

	PMD_INIT_FUNC_TRACE();

	/* make sure the mempool element size can account for alignment. */
	if (!mempool_element_size_valid(mp)) {
		PMD_INIT_LOG(ERR, "Error : Mempool element size is too small");
		return (-EINVAL);
	}

	/* make sure a valid number of descriptors have been requested */
	if (check_nb_desc(FM10K_MIN_RX_DESC, FM10K_MAX_RX_DESC,
				FM10K_MULT_RX_DESC, nb_desc)) {
		PMD_INIT_LOG(ERR, "Number of Rx descriptors (%u) must be "
			"less than or equal to %"PRIu32", "
			"greater than or equal to %u, "
			"and a multiple of %u",
			nb_desc, (uint32_t)FM10K_MAX_RX_DESC, FM10K_MIN_RX_DESC,
			FM10K_MULT_RX_DESC);
		return (-EINVAL);
	}

	/*
	 * if this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->rx_queues[queue_id] != NULL) {
		rx_queue_free(dev->data->rx_queues[queue_id]);
		dev->data->rx_queues[queue_id] = NULL;
	}

	/* allocate memory for the queue structure */
	q = rte_zmalloc_socket("fm10k", sizeof(*q), RTE_CACHE_LINE_SIZE,
				socket_id);
	if (q == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate queue structure");
		return (-ENOMEM);
	}

	/* setup queue */
	q->mp = mp;
	q->nb_desc = nb_desc;
	q->port_id = dev->data->port_id;
	q->queue_id = queue_id;
	q->tail_ptr = (volatile uint32_t *)
		&((uint32_t *)hw->hw_addr)[FM10K_RDT(queue_id)];
	if (handle_rxconf(q, conf))
		return (-EINVAL);

	/* allocate memory for the software ring */
	q->sw_ring = rte_zmalloc_socket("fm10k sw ring",
					nb_desc * sizeof(struct rte_mbuf *),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (q->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate software ring");
		rte_free(q);
		return (-ENOMEM);
	}

	/*
	 * allocate memory for the hardware descriptor ring. A memzone large
	 * enough to hold the maximum ring size is requested to allow for
	 * resizing in later calls to the queue setup function.
	 */
	mz = allocate_hw_ring(dev->driver->pci_drv.name, "rx_ring",
				dev->data->port_id, queue_id, socket_id,
				FM10K_MAX_RX_RING_SZ, FM10K_ALIGN_RX_DESC);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate hardware ring");
		rte_free(q->sw_ring);
		rte_free(q);
		return (-ENOMEM);
	}
	q->hw_ring = mz->addr;
	q->hw_ring_phys_addr = mz->phys_addr;

	dev->data->rx_queues[queue_id] = q;
	return 0;
}

static void
fm10k_rx_queue_release(void *queue)
{
	PMD_INIT_FUNC_TRACE();

	rx_queue_free(queue);
}

static int
fm10k_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t i, j, idx, shift;
	uint8_t mask;
	uint32_t reta;

	PMD_INIT_FUNC_TRACE();

	if (reta_size > FM10K_MAX_RSS_INDICES) {
		PMD_INIT_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, FM10K_MAX_RSS_INDICES);
		return -EINVAL;
	}

	/*
	 * Update Redirection Table RETA[n], n=0..31. The redirection table has
	 * 128-entries in 32 registers
	 */
	for (i = 0; i < FM10K_MAX_RSS_INDICES; i += CHARS_PER_UINT32) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
				BIT_MASK_PER_UINT32);
		if (mask == 0)
			continue;

		reta = 0;
		if (mask != BIT_MASK_PER_UINT32)
			reta = FM10K_READ_REG(hw, FM10K_RETA(0, i >> 2));

		for (j = 0; j < CHARS_PER_UINT32; j++) {
			if (mask & (0x1 << j)) {
				if (mask != 0xF)
					reta &= ~(UINT8_MAX << CHAR_BIT * j);
				reta |= reta_conf[idx].reta[shift + j] <<
						(CHAR_BIT * j);
			}
		}
		FM10K_WRITE_REG(hw, FM10K_RETA(0, i >> 2), reta);
	}

	return 0;
}

static int
fm10k_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t i, j, idx, shift;
	uint8_t mask;
	uint32_t reta;

	PMD_INIT_FUNC_TRACE();

	if (reta_size < FM10K_MAX_RSS_INDICES) {
		PMD_INIT_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, FM10K_MAX_RSS_INDICES);
		return -EINVAL;
	}

	/*
	 * Read Redirection Table RETA[n], n=0..31. The redirection table has
	 * 128-entries in 32 registers
	 */
	for (i = 0; i < FM10K_MAX_RSS_INDICES; i += CHARS_PER_UINT32) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
				BIT_MASK_PER_UINT32);
		if (mask == 0)
			continue;

		reta = FM10K_READ_REG(hw, FM10K_RETA(0, i >> 2));
		for (j = 0; j < CHARS_PER_UINT32; j++) {
			if (mask & (0x1 << j))
				reta_conf[idx].reta[shift + j] = ((reta >>
					CHAR_BIT * j) & UINT8_MAX);
		}
	}

	return 0;
}

/* Mailbox message handler in VF */
static const struct fm10k_msg_data fm10k_msgdata_vf[] = {
	FM10K_TLV_MSG_TEST_HANDLER(fm10k_tlv_msg_test),
	FM10K_VF_MSG_MAC_VLAN_HANDLER(fm10k_msg_mac_vlan_vf),
	FM10K_VF_MSG_LPORT_STATE_HANDLER(fm10k_msg_lport_state_vf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

/* Mailbox message handler in PF */
static const struct fm10k_msg_data fm10k_msgdata_pf[] = {
	FM10K_PF_MSG_ERR_HANDLER(XCAST_MODES, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(UPDATE_MAC_FWD_RULE, fm10k_msg_err_pf),
	FM10K_PF_MSG_LPORT_MAP_HANDLER(fm10k_msg_lport_map_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_CREATE, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_DELETE, fm10k_msg_err_pf),
	FM10K_PF_MSG_UPDATE_PVID_HANDLER(fm10k_msg_update_pvid_pf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

static int
fm10k_setup_mbx_service(struct fm10k_hw *hw)
{
	int err;

	/* Initialize mailbox lock */
	fm10k_mbx_initlock(hw);

	/* Replace default message handler with new ones */
	if (hw->mac.type == fm10k_mac_pf)
		err = hw->mbx.ops.register_handlers(&hw->mbx, fm10k_msgdata_pf);
	else
		err = hw->mbx.ops.register_handlers(&hw->mbx, fm10k_msgdata_vf);

	if (err) {
		PMD_INIT_LOG(ERR, "Failed to register mailbox handler.err:%d",
				err);
		return err;
	}
	/* Connect to SM for PF device or PF for VF device */
	return hw->mbx.ops.connect(hw, &hw->mbx);
}

static struct eth_dev_ops fm10k_eth_dev_ops = {
	.dev_configure		= fm10k_dev_configure,
	.stats_get		= fm10k_stats_get,
	.stats_reset		= fm10k_stats_reset,
	.link_update		= fm10k_link_update,
	.dev_infos_get		= fm10k_dev_infos_get,
	.rx_queue_setup		= fm10k_rx_queue_setup,
	.rx_queue_release	= fm10k_rx_queue_release,
	.reta_update		= fm10k_reta_update,
	.reta_query		= fm10k_reta_query,
};

static int
eth_fm10k_dev_init(__rte_unused struct eth_driver *eth_drv,
	struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int diag;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &fm10k_eth_dev_ops;

	/* only initialize in the primary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Vendor and Device ID need to be set before init of shared code */
	memset(hw, 0, sizeof(*hw));
	hw->device_id = dev->pci_dev->id.device_id;
	hw->vendor_id = dev->pci_dev->id.vendor_id;
	hw->subsystem_device_id = dev->pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = dev->pci_dev->id.subsystem_vendor_id;
	hw->revision_id = 0;
	hw->hw_addr = (void *)dev->pci_dev->mem_resource[0].addr;
	if (hw->hw_addr == NULL) {
		PMD_INIT_LOG(ERR, "Bad mem resource."
			" Try to blacklist unused devices.");
		return -EIO;
	}

	/* Store fm10k_adapter pointer */
	hw->back = dev->data->dev_private;

	/* Initialize the shared code */
	diag = fm10k_init_shared_code(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", diag);
		return -EIO;
	}

	/*
	 * Inialize bus info. Normally we would call fm10k_get_bus_info(), but
	 * there is no way to get link status without reading BAR4.  Until this
	 * works, assume we have maximum bandwidth.
	 * @todo - fix bus info
	 */
	hw->bus_caps.speed = fm10k_bus_speed_8000;
	hw->bus_caps.width = fm10k_bus_width_pcie_x8;
	hw->bus_caps.payload = fm10k_bus_payload_512;
	hw->bus.speed = fm10k_bus_speed_8000;
	hw->bus.width = fm10k_bus_width_pcie_x8;
	hw->bus.payload = fm10k_bus_payload_256;

	/* Initialize the hw */
	diag = fm10k_init_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware init failed: %d", diag);
		return -EIO;
	}

	/* Initialize MAC address(es) */
	dev->data->mac_addrs = rte_zmalloc("fm10k", ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate memory for MAC addresses");
		return -ENOMEM;
	}

	diag = fm10k_read_mac_addr(hw);
	if (diag != FM10K_SUCCESS) {
		/*
		 * TODO: remove special handling on VF. Need shared code to
		 * fix first.
		 */
		if (hw->mac.type == fm10k_mac_pf) {
			PMD_INIT_LOG(ERR, "Read MAC addr failed: %d", diag);
			return -EIO;
		} else {
			/* Generate a random addr */
			eth_random_addr(hw->mac.addr);
			memcpy(hw->mac.perm_addr, hw->mac.addr, ETH_ALEN);
		}
	}

	ether_addr_copy((const struct ether_addr *)hw->mac.addr,
			&dev->data->mac_addrs[0]);

	/* Reset the hw statistics */
	fm10k_stats_reset(dev);

	/* Reset the hw */
	diag = fm10k_reset_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware reset failed: %d", diag);
		return -EIO;
	}

	/* Setup mailbox service */
	diag = fm10k_setup_mbx_service(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to setup mailbox: %d", diag);
		return -EIO;
	}

	/*
	 * Below function will trigger operations on mailbox, acquire lock to
	 * avoid race condition from interrupt handler. Operations on mailbox
	 * FIFO will trigger interrupt to PF/SM, in which interrupt handler
	 * will handle and generate an interrupt to our side. Then,  FIFO in
	 * mailbox will be touched.
	 */
	fm10k_mbx_lock(hw);
	/* Enable port first */
	hw->mac.ops.update_lport_state(hw, 0, 0, 1);

	/* Update default vlan */
	hw->mac.ops.update_vlan(hw, hw->mac.default_vid, 0, true);

	/*
	 * Add default mac/vlan filter. glort is assigned by SM for PF, while is
	 * unused for VF. PF will assign correct glort for VF.
	 */
	hw->mac.ops.update_uc_addr(hw, hw->mac.dglort_map, hw->mac.addr,
			      hw->mac.default_vid, 1, 0);

	/* Set unicast mode by default. App can change to other mode in other
	 * API func.
	 */
	hw->mac.ops.update_xcast_mode(hw, hw->mac.dglort_map,
					FM10K_XCAST_MODE_MULTI);

	fm10k_mbx_unlock(hw);

	return 0;
}

/*
 * The set of PCI devices this driver supports. This driver will enable both PF
 * and SRIOV-VF devices.
 */
static struct rte_pci_id pci_id_fm10k_map[] = {
#define RTE_PCI_DEV_ID_DECL_FM10K(vend, dev) { RTE_PCI_DEVICE(vend, dev) },
#include "rte_pci_dev_ids.h"
	{ .vendor_id = 0, /* sentinel */ },
};

static struct eth_driver rte_pmd_fm10k = {
	{
		.name = "rte_pmd_fm10k",
		.id_table = pci_id_fm10k_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_fm10k_dev_init,
	.dev_private_size = sizeof(struct fm10k_adapter),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI FM10K devices.
 */
static int
rte_pmd_fm10k_init(__rte_unused const char *name,
	__rte_unused const char *params)
{
	PMD_INIT_FUNC_TRACE();
	rte_eth_driver_register(&rte_pmd_fm10k);
	return 0;
}

static struct rte_driver rte_fm10k_driver = {
	.type = PMD_PDEV,
	.init = rte_pmd_fm10k_init,
};

PMD_REGISTER_DRIVER(rte_fm10k_driver);
