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
#define UINT64_LOWER_32BITS_MASK 0x00000000ffffffffULL

/* Number of chars per uint32 type */
#define CHARS_PER_UINT32 (sizeof(uint32_t))
#define BIT_MASK_PER_UINT32 ((1 << CHARS_PER_UINT32) - 1)

static void fm10k_close_mbx_service(struct fm10k_hw *hw);

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
 * reset queue to initial state, allocate software buffers used when starting
 * device.
 * return 0 on success
 * return -ENOMEM if buffers cannot be allocated
 * return -EINVAL if buffers do not satisfy alignment condition
 */
static inline int
rx_queue_reset(struct fm10k_rx_queue *q)
{
	uint64_t dma_addr;
	int i, diag;
	PMD_INIT_FUNC_TRACE();

	diag = rte_mempool_get_bulk(q->mp, (void **)q->sw_ring, q->nb_desc);
	if (diag != 0)
		return -ENOMEM;

	for (i = 0; i < q->nb_desc; ++i) {
		fm10k_pktmbuf_reset(q->sw_ring[i], q->port_id);
		if (!fm10k_addr_alignment_valid(q->sw_ring[i])) {
			rte_mempool_put_bulk(q->mp, (void **)q->sw_ring,
						q->nb_desc);
			return -EINVAL;
		}
		dma_addr = MBUF_DMA_ADDR_DEFAULT(q->sw_ring[i]);
		q->hw_ring[i].q.pkt_addr = dma_addr;
		q->hw_ring[i].q.hdr_addr = dma_addr;
	}

	q->next_dd = 0;
	q->next_alloc = 0;
	q->next_trigger = q->alloc_thresh - 1;
	FM10K_PCI_REG_WRITE(q->tail_ptr, q->nb_desc - 1);
	return 0;
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
		if (q->sw_ring) {
			rte_free(q->sw_ring);
			q->sw_ring = NULL;
		}
		rte_free(q);
		q = NULL;
	}
}

/*
 * disable RX queue, wait unitl HW finished necessary flush operation
 */
static inline int
rx_queue_disable(struct fm10k_hw *hw, uint16_t qnum)
{
	uint32_t reg, i;

	reg = FM10K_READ_REG(hw, FM10K_RXQCTL(qnum));
	FM10K_WRITE_REG(hw, FM10K_RXQCTL(qnum),
			reg & ~FM10K_RXQCTL_ENABLE);

	/* Wait 100us at most */
	for (i = 0; i < FM10K_QUEUE_DISABLE_TIMEOUT; i++) {
		rte_delay_us(1);
		reg = FM10K_READ_REG(hw, FM10K_RXQCTL(i));
		if (!(reg & FM10K_RXQCTL_ENABLE))
			break;
	}

	if (i == FM10K_QUEUE_DISABLE_TIMEOUT)
		return -1;

	return 0;
}

/*
 * reset queue to initial state, allocate software buffers used when starting
 * device
 */
static inline void
tx_queue_reset(struct fm10k_tx_queue *q)
{
	PMD_INIT_FUNC_TRACE();
	q->last_free = 0;
	q->next_free = 0;
	q->nb_used = 0;
	q->nb_free = q->nb_desc - 1;
	q->free_trigger = q->nb_free - q->free_thresh;
	fifo_reset(&q->rs_tracker, (q->nb_desc + 1) / q->rs_thresh);
	FM10K_PCI_REG_WRITE(q->tail_ptr, 0);
}

/*
 * clean queue, descriptor rings, free software buffers used when stopping
 * device
 */
static inline void
tx_queue_clean(struct fm10k_tx_queue *q)
{
	struct fm10k_tx_desc zero = {0, 0, 0, 0, 0, 0};
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
tx_queue_free(struct fm10k_tx_queue *q)
{
	PMD_INIT_FUNC_TRACE();
	if (q) {
		PMD_INIT_LOG(DEBUG, "Freeing tx queue %p", q);
		tx_queue_clean(q);
		if (q->rs_tracker.list) {
			rte_free(q->rs_tracker.list);
			q->rs_tracker.list = NULL;
		}
		if (q->sw_ring) {
			rte_free(q->sw_ring);
			q->sw_ring = NULL;
		}
		rte_free(q);
		q = NULL;
	}
}

/*
 * disable TX queue, wait unitl HW finished necessary flush operation
 */
static inline int
tx_queue_disable(struct fm10k_hw *hw, uint16_t qnum)
{
	uint32_t reg, i;

	reg = FM10K_READ_REG(hw, FM10K_TXDCTL(qnum));
	FM10K_WRITE_REG(hw, FM10K_TXDCTL(qnum),
			reg & ~FM10K_TXDCTL_ENABLE);

	/* Wait 100us at most */
	for (i = 0; i < FM10K_QUEUE_DISABLE_TIMEOUT; i++) {
		rte_delay_us(1);
		reg = FM10K_READ_REG(hw, FM10K_TXDCTL(i));
		if (!(reg & FM10K_TXDCTL_ENABLE))
			break;
	}

	if (i == FM10K_QUEUE_DISABLE_TIMEOUT)
		return -1;

	return 0;
}

static int
fm10k_dev_configure(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.hw_strip_crc == 0)
		PMD_INIT_LOG(WARNING, "fm10k always strip CRC");

	return 0;
}

static void
fm10k_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	uint32_t mrqc, *key, i, reta, j;
	uint64_t hf;

#define RSS_KEY_SIZE 40
	static uint8_t rss_intel_key[RSS_KEY_SIZE] = {
		0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
		0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
		0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
		0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
		0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
	};

	if (dev->data->nb_rx_queues == 1 ||
	    dev_conf->rxmode.mq_mode != ETH_MQ_RX_RSS ||
	    dev_conf->rx_adv_conf.rss_conf.rss_hf == 0)
		return;

	/* random key is rss_intel_key (default) or user provided (rss_key) */
	if (dev_conf->rx_adv_conf.rss_conf.rss_key == NULL)
		key = (uint32_t *)rss_intel_key;
	else
		key = (uint32_t *)dev_conf->rx_adv_conf.rss_conf.rss_key;

	/* Now fill our hash function seeds, 4 bytes at a time */
	for (i = 0; i < RSS_KEY_SIZE / sizeof(*key); ++i)
		FM10K_WRITE_REG(hw, FM10K_RSSRK(0, i), key[i]);

	/*
	 * Fill in redirection table
	 * The byte-swap is needed because NIC registers are in
	 * little-endian order.
	 */
	reta = 0;
	for (i = 0, j = 0; i < FM10K_RETA_SIZE; i++, j++) {
		if (j == dev->data->nb_rx_queues)
			j = 0;
		reta = (reta << CHAR_BIT) | j;
		if ((i & 3) == 3)
			FM10K_WRITE_REG(hw, FM10K_RETA(0, i >> 2),
					rte_bswap32(reta));
	}

	/*
	 * Generate RSS hash based on packet types, TCP/UDP
	 * port numbers and/or IPv4/v6 src and dst addresses
	 */
	hf = dev_conf->rx_adv_conf.rss_conf.rss_hf;
	mrqc = 0;
	mrqc |= (hf & ETH_RSS_IPV4)              ? FM10K_MRQC_IPV4     : 0;
	mrqc |= (hf & ETH_RSS_IPV6)              ? FM10K_MRQC_IPV6     : 0;
	mrqc |= (hf & ETH_RSS_IPV6_EX)           ? FM10K_MRQC_IPV6     : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV4_TCP)  ? FM10K_MRQC_TCP_IPV4 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV6_TCP)  ? FM10K_MRQC_TCP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_IPV6_TCP_EX)       ? FM10K_MRQC_TCP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV4_UDP)  ? FM10K_MRQC_UDP_IPV4 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV6_UDP)  ? FM10K_MRQC_UDP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_IPV6_UDP_EX)       ? FM10K_MRQC_UDP_IPV6 : 0;

	if (mrqc == 0) {
		PMD_INIT_LOG(ERR, "Specified RSS mode 0x%"PRIx64"is not"
			"supported", hf);
		return;
	}

	FM10K_WRITE_REG(hw, FM10K_MRQC(0), mrqc);
}

static int
fm10k_dev_tx_init(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int i, ret;
	struct fm10k_tx_queue *txq;
	uint64_t base_addr;
	uint32_t size;

	/* Disable TXINT to avoid possible interrupt */
	for (i = 0; i < hw->mac.max_queues; i++)
		FM10K_WRITE_REG(hw, FM10K_TXINT(i),
				3 << FM10K_TXINT_TIMER_SHIFT);

	/* Setup TX queue */
	for (i = 0; i < dev->data->nb_tx_queues; ++i) {
		txq = dev->data->tx_queues[i];
		base_addr = txq->hw_ring_phys_addr;
		size = txq->nb_desc * sizeof(struct fm10k_tx_desc);

		/* disable queue to avoid issues while updating state */
		ret = tx_queue_disable(hw, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "failed to disable queue %d", i);
			return -1;
		}

		/* set location and size for descriptor ring */
		FM10K_WRITE_REG(hw, FM10K_TDBAL(i),
				base_addr & UINT64_LOWER_32BITS_MASK);
		FM10K_WRITE_REG(hw, FM10K_TDBAH(i),
				base_addr >> (CHAR_BIT * sizeof(uint32_t)));
		FM10K_WRITE_REG(hw, FM10K_TDLEN(i), size);
	}
	return 0;
}

static int
fm10k_dev_rx_init(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int i, ret;
	struct fm10k_rx_queue *rxq;
	uint64_t base_addr;
	uint32_t size;
	uint32_t rxdctl = FM10K_RXDCTL_WRITE_BACK_MIN_DELAY;
	uint16_t buf_size;
	struct rte_pktmbuf_pool_private *mbp_priv;

	/* Disable RXINT to avoid possible interrupt */
	for (i = 0; i < hw->mac.max_queues; i++)
		FM10K_WRITE_REG(hw, FM10K_RXINT(i),
				3 << FM10K_RXINT_TIMER_SHIFT);

	/* Setup RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; ++i) {
		rxq = dev->data->rx_queues[i];
		base_addr = rxq->hw_ring_phys_addr;
		size = rxq->nb_desc * sizeof(union fm10k_rx_desc);

		/* disable queue to avoid issues while updating state */
		ret = rx_queue_disable(hw, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "failed to disable queue %d", i);
			return -1;
		}

		/* Setup the Base and Length of the Rx Descriptor Ring */
		FM10K_WRITE_REG(hw, FM10K_RDBAL(i),
				base_addr & UINT64_LOWER_32BITS_MASK);
		FM10K_WRITE_REG(hw, FM10K_RDBAH(i),
				base_addr >> (CHAR_BIT * sizeof(uint32_t)));
		FM10K_WRITE_REG(hw, FM10K_RDLEN(i), size);

		/* Configure the Rx buffer size for one buff without split */
		mbp_priv = rte_mempool_get_priv(rxq->mp);
		buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
					RTE_PKTMBUF_HEADROOM);
		FM10K_WRITE_REG(hw, FM10K_SRRCTL(i),
				buf_size >> FM10K_SRRCTL_BSIZEPKT_SHIFT);

		/* It adds dual VLAN length for supporting dual VLAN */
		if ((dev->data->dev_conf.rxmode.max_rx_pkt_len +
				2 * FM10K_VLAN_TAG_SIZE) > buf_size){
			dev->data->scattered_rx = 1;
			dev->rx_pkt_burst = fm10k_recv_scattered_pkts;
		}

		/* Enable drop on empty, it's RO for VF */
		if (hw->mac.type == fm10k_mac_pf && rxq->drop_en)
			rxdctl |= FM10K_RXDCTL_DROP_ON_EMPTY;

		FM10K_WRITE_REG(hw, FM10K_RXDCTL(i), rxdctl);
		FM10K_WRITE_FLUSH(hw);
	}

	if (dev->data->dev_conf.rxmode.enable_scatter) {
		dev->rx_pkt_burst = fm10k_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	/* Configure RSS if applicable */
	fm10k_dev_mq_rx_configure(dev);
	return 0;
}

static int
fm10k_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int err = -1;
	uint32_t reg;
	struct fm10k_rx_queue *rxq;

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];
		err = rx_queue_reset(rxq);
		if (err == -ENOMEM) {
			PMD_INIT_LOG(ERR, "Failed to alloc memory : %d", err);
			return err;
		} else if (err == -EINVAL) {
			PMD_INIT_LOG(ERR, "Invalid buffer address alignment :"
				" %d", err);
			return err;
		}

		/* Setup the HW Rx Head and Tail Descriptor Pointers
		 * Note: this must be done AFTER the queue is enabled on real
		 * hardware, but BEFORE the queue is enabled when using the
		 * emulation platform. Do it in both places for now and remove
		 * this comment and the following two register writes when the
		 * emulation platform is no longer being used.
		 */
		FM10K_WRITE_REG(hw, FM10K_RDH(rx_queue_id), 0);
		FM10K_WRITE_REG(hw, FM10K_RDT(rx_queue_id), rxq->nb_desc - 1);

		/* Set PF ownership flag for PF devices */
		reg = FM10K_READ_REG(hw, FM10K_RXQCTL(rx_queue_id));
		if (hw->mac.type == fm10k_mac_pf)
			reg |= FM10K_RXQCTL_PF;
		reg |= FM10K_RXQCTL_ENABLE;
		/* enable RX queue */
		FM10K_WRITE_REG(hw, FM10K_RXQCTL(rx_queue_id), reg);
		FM10K_WRITE_FLUSH(hw);

		/* Setup the HW Rx Head and Tail Descriptor Pointers
		 * Note: this must be done AFTER the queue is enabled
		 */
		FM10K_WRITE_REG(hw, FM10K_RDH(rx_queue_id), 0);
		FM10K_WRITE_REG(hw, FM10K_RDT(rx_queue_id), rxq->nb_desc - 1);
	}

	return err;
}

static int
fm10k_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id < dev->data->nb_rx_queues) {
		/* Disable RX queue */
		rx_queue_disable(hw, rx_queue_id);

		/* Free mbuf and clean HW ring */
		rx_queue_clean(dev->data->rx_queues[rx_queue_id]);
	}

	return 0;
}

static int
fm10k_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	/** @todo - this should be defined in the shared code */
#define FM10K_TXDCTL_WRITE_BACK_MIN_DELAY	0x00010000
	uint32_t txdctl = FM10K_TXDCTL_WRITE_BACK_MIN_DELAY;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	if (tx_queue_id < dev->data->nb_tx_queues) {
		tx_queue_reset(dev->data->tx_queues[tx_queue_id]);

		/* reset head and tail pointers */
		FM10K_WRITE_REG(hw, FM10K_TDH(tx_queue_id), 0);
		FM10K_WRITE_REG(hw, FM10K_TDT(tx_queue_id), 0);

		/* enable TX queue */
		FM10K_WRITE_REG(hw, FM10K_TXDCTL(tx_queue_id),
					FM10K_TXDCTL_ENABLE | txdctl);
		FM10K_WRITE_FLUSH(hw);
	} else
		err = -1;

	return err;
}

static int
fm10k_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (tx_queue_id < dev->data->nb_tx_queues) {
		tx_queue_disable(hw, tx_queue_id);
		tx_queue_clean(dev->data->tx_queues[tx_queue_id]);
	}

	return 0;
}

/* fls = find last set bit = 32 minus the number of leading zeros */
#ifndef fls
#define fls(x) (((x) == 0) ? 0 : (32 - __builtin_clz((x))))
#endif
#define BSIZEPKT_ROUNDUP ((1 << FM10K_SRRCTL_BSIZEPKT_SHIFT) - 1)
static int
fm10k_dev_start(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int i, diag;

	PMD_INIT_FUNC_TRACE();

	/* stop, init, then start the hw */
	diag = fm10k_stop_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware stop failed: %d", diag);
		return -EIO;
	}

	diag = fm10k_init_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware init failed: %d", diag);
		return -EIO;
	}

	diag = fm10k_start_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware start failed: %d", diag);
		return -EIO;
	}

	diag = fm10k_dev_tx_init(dev);
	if (diag) {
		PMD_INIT_LOG(ERR, "TX init failed: %d", diag);
		return diag;
	}

	diag = fm10k_dev_rx_init(dev);
	if (diag) {
		PMD_INIT_LOG(ERR, "RX init failed: %d", diag);
		return diag;
	}

	if (hw->mac.type == fm10k_mac_pf) {
		/* Establish only VSI 0 as valid */
		FM10K_WRITE_REG(hw, FM10K_DGLORTMAP(0), FM10K_DGLORTMAP_ANY);

		/* Configure RSS bits used in RETA table */
		FM10K_WRITE_REG(hw, FM10K_DGLORTDEC(0),
				fls(dev->data->nb_rx_queues - 1) <<
				FM10K_DGLORTDEC_RSSLENGTH_SHIFT);

		/* Invalidate all other GLORT entries */
		for (i = 1; i < FM10K_DGLORT_COUNT; i++)
			FM10K_WRITE_REG(hw, FM10K_DGLORTMAP(i),
					FM10K_DGLORTMAP_NONE);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct fm10k_rx_queue *rxq;
		rxq = dev->data->rx_queues[i];

		if (rxq->rx_deferred_start)
			continue;
		diag = fm10k_dev_rx_queue_start(dev, i);
		if (diag != 0) {
			int j;
			for (j = 0; j < i; ++j)
				rx_queue_clean(dev->data->rx_queues[j]);
			return diag;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct fm10k_tx_queue *txq;
		txq = dev->data->tx_queues[i];

		if (txq->tx_deferred_start)
			continue;
		diag = fm10k_dev_tx_queue_start(dev, i);
		if (diag != 0) {
			int j;
			for (j = 0; j < dev->data->nb_rx_queues; ++j)
				rx_queue_clean(dev->data->rx_queues[j]);
			return diag;
		}
	}

	return 0;
}

static void
fm10k_dev_stop(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		fm10k_dev_tx_queue_stop(dev, i);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		fm10k_dev_rx_queue_stop(dev, i);
}

static void
fm10k_dev_close(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* Stop mailbox service first */
	fm10k_close_mbx_service(hw);
	fm10k_dev_stop(dev);
	fm10k_stop_hw(hw);
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

static int
fm10k_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* @todo - add support for the VF */
	if (hw->mac.type != fm10k_mac_pf)
		return -ENOTSUP;

	return fm10k_update_vlan(hw, vlan_id, 0, on);
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

static inline int
handle_txconf(struct fm10k_tx_queue *q, const struct rte_eth_txconf *conf)
{
	uint16_t tx_free_thresh;
	uint16_t tx_rs_thresh;

	/* constraint MACROs require that tx_free_thresh is configured
	 * before tx_rs_thresh */
	if (conf->tx_free_thresh == 0)
		tx_free_thresh = FM10K_TX_FREE_THRESH_DEFAULT(q);
	else
		tx_free_thresh = conf->tx_free_thresh;

	/* make sure the requested threshold satisfies the constraints */
	if (check_thresh(FM10K_TX_FREE_THRESH_MIN(q),
			FM10K_TX_FREE_THRESH_MAX(q),
			FM10K_TX_FREE_THRESH_DIV(q),
			tx_free_thresh)) {
		PMD_INIT_LOG(ERR, "tx_free_thresh (%u) must be "
			"less than or equal to %u, "
			"greater than or equal to %u, "
			"and a divisor of %u",
			tx_free_thresh, FM10K_TX_FREE_THRESH_MAX(q),
			FM10K_TX_FREE_THRESH_MIN(q),
			FM10K_TX_FREE_THRESH_DIV(q));
		return (-EINVAL);
	}

	q->free_thresh = tx_free_thresh;

	if (conf->tx_rs_thresh == 0)
		tx_rs_thresh = FM10K_TX_RS_THRESH_DEFAULT(q);
	else
		tx_rs_thresh = conf->tx_rs_thresh;

	q->tx_deferred_start = conf->tx_deferred_start;

	/* make sure the requested threshold satisfies the constraints */
	if (check_thresh(FM10K_TX_RS_THRESH_MIN(q),
			FM10K_TX_RS_THRESH_MAX(q),
			FM10K_TX_RS_THRESH_DIV(q),
			tx_rs_thresh)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh (%u) must be "
			"less than or equal to %u, "
			"greater than or equal to %u, "
			"and a divisor of %u",
			tx_rs_thresh, FM10K_TX_RS_THRESH_MAX(q),
			FM10K_TX_RS_THRESH_MIN(q),
			FM10K_TX_RS_THRESH_DIV(q));
		return (-EINVAL);
	}

	q->rs_thresh = tx_rs_thresh;

	return 0;
}

static int
fm10k_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id,
	uint16_t nb_desc, unsigned int socket_id,
	const struct rte_eth_txconf *conf)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_tx_queue *q;
	const struct rte_memzone *mz;

	PMD_INIT_FUNC_TRACE();

	/* make sure a valid number of descriptors have been requested */
	if (check_nb_desc(FM10K_MIN_TX_DESC, FM10K_MAX_TX_DESC,
				FM10K_MULT_TX_DESC, nb_desc)) {
		PMD_INIT_LOG(ERR, "Number of Tx descriptors (%u) must be "
			"less than or equal to %"PRIu32", "
			"greater than or equal to %u, "
			"and a multiple of %u",
			nb_desc, (uint32_t)FM10K_MAX_TX_DESC, FM10K_MIN_TX_DESC,
			FM10K_MULT_TX_DESC);
		return (-EINVAL);
	}

	/*
	 * if this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->tx_queues[queue_id] != NULL) {
		tx_queue_free(dev->data->tx_queues[queue_id]);
		dev->data->tx_queues[queue_id] = NULL;
	}

	/* allocate memory for the queue structure */
	q = rte_zmalloc_socket("fm10k", sizeof(*q), RTE_CACHE_LINE_SIZE,
				socket_id);
	if (q == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate queue structure");
		return (-ENOMEM);
	}

	/* setup queue */
	q->nb_desc = nb_desc;
	q->port_id = dev->data->port_id;
	q->queue_id = queue_id;
	q->tail_ptr = (volatile uint32_t *)
		&((uint32_t *)hw->hw_addr)[FM10K_TDT(queue_id)];
	if (handle_txconf(q, conf))
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
	mz = allocate_hw_ring(dev->driver->pci_drv.name, "tx_ring",
				dev->data->port_id, queue_id, socket_id,
				FM10K_MAX_TX_RING_SZ, FM10K_ALIGN_TX_DESC);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate hardware ring");
		rte_free(q->sw_ring);
		rte_free(q);
		return (-ENOMEM);
	}
	q->hw_ring = mz->addr;
	q->hw_ring_phys_addr = mz->phys_addr;

	/*
	 * allocate memory for the RS bit tracker. Enough slots to hold the
	 * descriptor index for each RS bit needing to be set are required.
	 */
	q->rs_tracker.list = rte_zmalloc_socket("fm10k rs tracker",
				((nb_desc + 1) / q->rs_thresh) *
				sizeof(uint16_t),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (q->rs_tracker.list == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate RS bit tracker");
		rte_free(q->sw_ring);
		rte_free(q);
		return (-ENOMEM);
	}

	dev->data->tx_queues[queue_id] = q;
	return 0;
}

static void
fm10k_tx_queue_release(void *queue)
{
	PMD_INIT_FUNC_TRACE();

	tx_queue_free(queue);
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

static int
fm10k_rss_hash_update(struct rte_eth_dev *dev,
	struct rte_eth_rss_conf *rss_conf)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *key = (uint32_t *)rss_conf->rss_key;
	uint32_t mrqc;
	uint64_t hf = rss_conf->rss_hf;
	int i;

	PMD_INIT_FUNC_TRACE();

	if (rss_conf->rss_key_len < FM10K_RSSRK_SIZE *
		FM10K_RSSRK_ENTRIES_PER_REG)
		return -EINVAL;

	if (hf == 0)
		return -EINVAL;

	mrqc = 0;
	mrqc |= (hf & ETH_RSS_IPV4)              ? FM10K_MRQC_IPV4     : 0;
	mrqc |= (hf & ETH_RSS_IPV6)              ? FM10K_MRQC_IPV6     : 0;
	mrqc |= (hf & ETH_RSS_IPV6_EX)           ? FM10K_MRQC_IPV6     : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV4_TCP)  ? FM10K_MRQC_TCP_IPV4 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV6_TCP)  ? FM10K_MRQC_TCP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_IPV6_TCP_EX)       ? FM10K_MRQC_TCP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV4_UDP)  ? FM10K_MRQC_UDP_IPV4 : 0;
	mrqc |= (hf & ETH_RSS_NONFRAG_IPV6_UDP)  ? FM10K_MRQC_UDP_IPV6 : 0;
	mrqc |= (hf & ETH_RSS_IPV6_UDP_EX)       ? FM10K_MRQC_UDP_IPV6 : 0;

	/* If the mapping doesn't fit any supported, return */
	if (mrqc == 0)
		return -EINVAL;

	if (key != NULL)
		for (i = 0; i < FM10K_RSSRK_SIZE; ++i)
			FM10K_WRITE_REG(hw, FM10K_RSSRK(0, i), key[i]);

	FM10K_WRITE_REG(hw, FM10K_MRQC(0), mrqc);

	return 0;
}

static int
fm10k_rss_hash_conf_get(struct rte_eth_dev *dev,
	struct rte_eth_rss_conf *rss_conf)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *key = (uint32_t *)rss_conf->rss_key;
	uint32_t mrqc;
	uint64_t hf;
	int i;

	PMD_INIT_FUNC_TRACE();

	if (rss_conf->rss_key_len < FM10K_RSSRK_SIZE *
				FM10K_RSSRK_ENTRIES_PER_REG)
		return -EINVAL;

	if (key != NULL)
		for (i = 0; i < FM10K_RSSRK_SIZE; ++i)
			key[i] = FM10K_READ_REG(hw, FM10K_RSSRK(0, i));

	mrqc = FM10K_READ_REG(hw, FM10K_MRQC(0));
	hf = 0;
	hf |= (mrqc & FM10K_MRQC_IPV4)     ? ETH_RSS_IPV4              : 0;
	hf |= (mrqc & FM10K_MRQC_IPV6)     ? ETH_RSS_IPV6              : 0;
	hf |= (mrqc & FM10K_MRQC_IPV6)     ? ETH_RSS_IPV6_EX           : 0;
	hf |= (mrqc & FM10K_MRQC_TCP_IPV4) ? ETH_RSS_NONFRAG_IPV4_TCP  : 0;
	hf |= (mrqc & FM10K_MRQC_TCP_IPV6) ? ETH_RSS_NONFRAG_IPV6_TCP  : 0;
	hf |= (mrqc & FM10K_MRQC_TCP_IPV6) ? ETH_RSS_IPV6_TCP_EX       : 0;
	hf |= (mrqc & FM10K_MRQC_UDP_IPV4) ? ETH_RSS_NONFRAG_IPV4_UDP  : 0;
	hf |= (mrqc & FM10K_MRQC_UDP_IPV6) ? ETH_RSS_NONFRAG_IPV6_UDP  : 0;
	hf |= (mrqc & FM10K_MRQC_UDP_IPV6) ? ETH_RSS_IPV6_UDP_EX       : 0;

	rss_conf->rss_hf = hf;

	return 0;
}

static void
fm10k_dev_enable_intr_pf(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t int_map = FM10K_INT_MAP_IMMEDIATE;

	/* Bind all local non-queue interrupt to vector 0 */
	int_map |= 0;

	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_Mailbox), int_map);
	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_PCIeFault), int_map);
	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_SwitchUpDown), int_map);
	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_SwitchEvent), int_map);
	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_SRAM), int_map);
	FM10K_WRITE_REG(hw, FM10K_INT_MAP(fm10k_int_VFLR), int_map);

	/* Enable misc causes */
	FM10K_WRITE_REG(hw, FM10K_EIMR, FM10K_EIMR_ENABLE(PCA_FAULT) |
				FM10K_EIMR_ENABLE(THI_FAULT) |
				FM10K_EIMR_ENABLE(FUM_FAULT) |
				FM10K_EIMR_ENABLE(MAILBOX) |
				FM10K_EIMR_ENABLE(SWITCHREADY) |
				FM10K_EIMR_ENABLE(SWITCHNOTREADY) |
				FM10K_EIMR_ENABLE(SRAMERROR) |
				FM10K_EIMR_ENABLE(VFLR));

	/* Enable ITR 0 */
	FM10K_WRITE_REG(hw, FM10K_ITR(0), FM10K_ITR_AUTOMASK |
					FM10K_ITR_MASK_CLEAR);
	FM10K_WRITE_FLUSH(hw);
}

static void
fm10k_dev_enable_intr_vf(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t int_map = FM10K_INT_MAP_IMMEDIATE;

	/* Bind all local non-queue interrupt to vector 0 */
	int_map |= 0;

	/* Only INT 0 available, other 15 are reserved. */
	FM10K_WRITE_REG(hw, FM10K_VFINT_MAP, int_map);

	/* Enable ITR 0 */
	FM10K_WRITE_REG(hw, FM10K_VFITR(0), FM10K_ITR_AUTOMASK |
					FM10K_ITR_MASK_CLEAR);
	FM10K_WRITE_FLUSH(hw);
}

static int
fm10k_dev_handle_fault(struct fm10k_hw *hw, uint32_t eicr)
{
	struct fm10k_fault fault;
	int err;
	const char *estr = "Unknown error";

	/* Process PCA fault */
	if (eicr & FM10K_EIMR_PCA_FAULT) {
		err = fm10k_get_fault(hw, FM10K_PCA_FAULT, &fault);
		if (err)
			goto error;
		switch (fault.type) {
		case PCA_NO_FAULT:
			estr = "PCA_NO_FAULT"; break;
		case PCA_UNMAPPED_ADDR:
			estr = "PCA_UNMAPPED_ADDR"; break;
		case PCA_BAD_QACCESS_PF:
			estr = "PCA_BAD_QACCESS_PF"; break;
		case PCA_BAD_QACCESS_VF:
			estr = "PCA_BAD_QACCESS_VF"; break;
		case PCA_MALICIOUS_REQ:
			estr = "PCA_MALICIOUS_REQ"; break;
		case PCA_POISONED_TLP:
			estr = "PCA_POISONED_TLP"; break;
		case PCA_TLP_ABORT:
			estr = "PCA_TLP_ABORT"; break;
		default:
			goto error;
		}
		PMD_INIT_LOG(ERR, "%s: %s(%d) Addr:0x%"PRIx64" Spec: 0x%x",
			estr, fault.func ? "VF" : "PF", fault.func,
			fault.address, fault.specinfo);
	}

	/* Process THI fault */
	if (eicr & FM10K_EIMR_THI_FAULT) {
		err = fm10k_get_fault(hw, FM10K_THI_FAULT, &fault);
		if (err)
			goto error;
		switch (fault.type) {
		case THI_NO_FAULT:
			estr = "THI_NO_FAULT"; break;
		case THI_MAL_DIS_Q_FAULT:
			estr = "THI_MAL_DIS_Q_FAULT"; break;
		default:
			goto error;
		}
		PMD_INIT_LOG(ERR, "%s: %s(%d) Addr:0x%"PRIx64" Spec: 0x%x",
			estr, fault.func ? "VF" : "PF", fault.func,
			fault.address, fault.specinfo);
	}

	/* Process FUM fault */
	if (eicr & FM10K_EIMR_FUM_FAULT) {
		err = fm10k_get_fault(hw, FM10K_FUM_FAULT, &fault);
		if (err)
			goto error;
		switch (fault.type) {
		case FUM_NO_FAULT:
			estr = "FUM_NO_FAULT"; break;
		case FUM_UNMAPPED_ADDR:
			estr = "FUM_UNMAPPED_ADDR"; break;
		case FUM_POISONED_TLP:
			estr = "FUM_POISONED_TLP"; break;
		case FUM_BAD_VF_QACCESS:
			estr = "FUM_BAD_VF_QACCESS"; break;
		case FUM_ADD_DECODE_ERR:
			estr = "FUM_ADD_DECODE_ERR"; break;
		case FUM_RO_ERROR:
			estr = "FUM_RO_ERROR"; break;
		case FUM_QPRC_CRC_ERROR:
			estr = "FUM_QPRC_CRC_ERROR"; break;
		case FUM_CSR_TIMEOUT:
			estr = "FUM_CSR_TIMEOUT"; break;
		case FUM_INVALID_TYPE:
			estr = "FUM_INVALID_TYPE"; break;
		case FUM_INVALID_LENGTH:
			estr = "FUM_INVALID_LENGTH"; break;
		case FUM_INVALID_BE:
			estr = "FUM_INVALID_BE"; break;
		case FUM_INVALID_ALIGN:
			estr = "FUM_INVALID_ALIGN"; break;
		default:
			goto error;
		}
		PMD_INIT_LOG(ERR, "%s: %s(%d) Addr:0x%"PRIx64" Spec: 0x%x",
			estr, fault.func ? "VF" : "PF", fault.func,
			fault.address, fault.specinfo);
	}

	if (estr)
		return 0;
	return 0;
error:
	PMD_INIT_LOG(ERR, "Failed to handle fault event.");
	return err;
}

/**
 * PF interrupt handler triggered by NIC for handling specific interrupt.
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
fm10k_dev_interrupt_handler_pf(
			__rte_unused struct rte_intr_handle *handle,
			void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t cause, status;

	if (hw->mac.type != fm10k_mac_pf)
		return;

	cause = FM10K_READ_REG(hw, FM10K_EICR);

	/* Handle PCI fault cases */
	if (cause & FM10K_EICR_FAULT_MASK) {
		PMD_INIT_LOG(ERR, "INT: find fault!");
		fm10k_dev_handle_fault(hw, cause);
	}

	/* Handle switch up/down */
	if (cause & FM10K_EICR_SWITCHNOTREADY)
		PMD_INIT_LOG(ERR, "INT: Switch is not ready");

	if (cause & FM10K_EICR_SWITCHREADY)
		PMD_INIT_LOG(INFO, "INT: Switch is ready");

	/* Handle mailbox message */
	fm10k_mbx_lock(hw);
	hw->mbx.ops.process(hw, &hw->mbx);
	fm10k_mbx_unlock(hw);

	/* Handle SRAM error */
	if (cause & FM10K_EICR_SRAMERROR) {
		PMD_INIT_LOG(ERR, "INT: SRAM error on PEP");

		status = FM10K_READ_REG(hw, FM10K_SRAM_IP);
		/* Write to clear pending bits */
		FM10K_WRITE_REG(hw, FM10K_SRAM_IP, status);

		/* Todo: print out error message after shared code  updates */
	}

	/* Clear these 3 events if having any */
	cause &= FM10K_EICR_SWITCHNOTREADY | FM10K_EICR_MAILBOX |
		 FM10K_EICR_SWITCHREADY;
	if (cause)
		FM10K_WRITE_REG(hw, FM10K_EICR, cause);

	/* Re-enable interrupt from device side */
	FM10K_WRITE_REG(hw, FM10K_ITR(0), FM10K_ITR_AUTOMASK |
					FM10K_ITR_MASK_CLEAR);
	/* Re-enable interrupt from host side */
	rte_intr_enable(&(dev->pci_dev->intr_handle));
}

/**
 * VF interrupt handler triggered by NIC for handling specific interrupt.
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
fm10k_dev_interrupt_handler_vf(
			__rte_unused struct rte_intr_handle *handle,
			void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (hw->mac.type != fm10k_mac_vf)
		return;

	/* Handle mailbox message if lock is acquired */
	fm10k_mbx_lock(hw);
	hw->mbx.ops.process(hw, &hw->mbx);
	fm10k_mbx_unlock(hw);

	/* Re-enable interrupt from device side */
	FM10K_WRITE_REG(hw, FM10K_VFITR(0), FM10K_ITR_AUTOMASK |
					FM10K_ITR_MASK_CLEAR);
	/* Re-enable interrupt from host side */
	rte_intr_enable(&(dev->pci_dev->intr_handle));
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

static void
fm10k_close_mbx_service(struct fm10k_hw *hw)
{
	/* Disconnect from SM for PF device or PF for VF device */
	hw->mbx.ops.disconnect(hw, &hw->mbx);
}

static struct eth_dev_ops fm10k_eth_dev_ops = {
	.dev_configure		= fm10k_dev_configure,
	.dev_start		= fm10k_dev_start,
	.dev_stop		= fm10k_dev_stop,
	.dev_close		= fm10k_dev_close,
	.stats_get		= fm10k_stats_get,
	.stats_reset		= fm10k_stats_reset,
	.link_update		= fm10k_link_update,
	.dev_infos_get		= fm10k_dev_infos_get,
	.vlan_filter_set	= fm10k_vlan_filter_set,
	.rx_queue_start		= fm10k_dev_rx_queue_start,
	.rx_queue_stop		= fm10k_dev_rx_queue_stop,
	.tx_queue_start		= fm10k_dev_tx_queue_start,
	.tx_queue_stop		= fm10k_dev_tx_queue_stop,
	.rx_queue_setup		= fm10k_rx_queue_setup,
	.rx_queue_release	= fm10k_rx_queue_release,
	.tx_queue_setup		= fm10k_tx_queue_setup,
	.tx_queue_release	= fm10k_tx_queue_release,
	.reta_update		= fm10k_reta_update,
	.reta_query		= fm10k_reta_query,
	.rss_hash_update	= fm10k_rss_hash_update,
	.rss_hash_conf_get	= fm10k_rss_hash_conf_get,
};

static int
eth_fm10k_dev_init(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int diag;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &fm10k_eth_dev_ops;
	dev->rx_pkt_burst = &fm10k_recv_pkts;
	dev->tx_pkt_burst = &fm10k_xmit_pkts;

	if (dev->data->scattered_rx)
		dev->rx_pkt_burst = &fm10k_recv_scattered_pkts;

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

	/*PF/VF has different interrupt handling mechanism */
	if (hw->mac.type == fm10k_mac_pf) {
		/* register callback func to eal lib */
		rte_intr_callback_register(&(dev->pci_dev->intr_handle),
			fm10k_dev_interrupt_handler_pf, (void *)dev);

		/* enable MISC interrupt */
		fm10k_dev_enable_intr_pf(dev);
	} else { /* VF */
		rte_intr_callback_register(&(dev->pci_dev->intr_handle),
			fm10k_dev_interrupt_handler_vf, (void *)dev);

		fm10k_dev_enable_intr_vf(dev);
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

	/* enable uio intr after callback registered */
	rte_intr_enable(&(dev->pci_dev->intr_handle));

	return 0;
}

/*
 * The set of PCI devices this driver supports. This driver will enable both PF
 * and SRIOV-VF devices.
 */
static struct rte_pci_id pci_id_fm10k_map[] = {
#define RTE_PCI_DEV_ID_DECL_FM10K(vend, dev) { RTE_PCI_DEVICE(vend, dev) },
#define RTE_PCI_DEV_ID_DECL_FM10KVF(vend, dev) { RTE_PCI_DEVICE(vend, dev) },
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
