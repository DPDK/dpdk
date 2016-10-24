/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 EZchip Semiconductor Ltd. All rights reserved.
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
 *     * Neither the name of EZchip Semiconductor nor the names of its
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

#include <unistd.h>

#include <rte_eal.h>
#include <rte_vdev.h>
#include <rte_eal_memconfig.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#include <arch/mpipe_xaui_def.h>
#include <arch/mpipe_gbe_def.h>

#include <gxio/mpipe.h>

#ifdef RTE_LIBRTE_MPIPE_PMD_DEBUG
#define PMD_DEBUG_RX(...)	RTE_LOG(DEBUG, PMD, __VA_ARGS__)
#define PMD_DEBUG_TX(...)	RTE_LOG(DEBUG, PMD, __VA_ARGS__)
#else
#define PMD_DEBUG_RX(...)
#define PMD_DEBUG_TX(...)
#endif

#define MPIPE_MAX_CHANNELS		128
#define MPIPE_TX_MAX_QUEUES		128
#define MPIPE_RX_MAX_QUEUES		16
#define MPIPE_TX_DESCS			512
#define MPIPE_RX_BUCKETS		256
#define MPIPE_RX_STACK_SIZE		65536
#define MPIPE_RX_IP_ALIGN		2
#define MPIPE_BSM_ALIGN			128

#define MPIPE_LINK_UPDATE_TIMEOUT	10	/*  s */
#define MPIPE_LINK_UPDATE_INTERVAL	100000	/* us */

struct mpipe_channel_config {
	int enable;
	int first_bucket;
	int num_buckets;
	int head_room;
	gxio_mpipe_rules_stacks_t stacks;
};

struct mpipe_context {
	rte_spinlock_t        lock;
	gxio_mpipe_context_t  context;
	struct mpipe_channel_config channels[MPIPE_MAX_CHANNELS];
};

/* Per-core local data. */
struct mpipe_local {
	int mbuf_push_debt[RTE_MAX_ETHPORTS];	/* Buffer push debt. */
} __rte_cache_aligned;

#define MPIPE_BUF_DEBT_THRESHOLD	32
static __thread struct mpipe_local mpipe_local;
static struct mpipe_context mpipe_contexts[GXIO_MPIPE_INSTANCE_MAX];
static int mpipe_instances;
static const char *drivername = "MPIPE PMD";

/* Per queue statistics. */
struct mpipe_queue_stats {
	uint64_t packets, bytes, errors, nomem;
};

/* Common tx/rx queue fields. */
struct mpipe_queue {
	struct mpipe_dev_priv *priv;	/* "priv" data of its device. */
	uint16_t nb_desc;		/* Number of tx descriptors. */
	uint16_t port_id;		/* Device index. */
	uint16_t stat_idx;		/* Queue stats index. */
	uint8_t queue_idx;		/* Queue index. */
	uint8_t link_status;		/* 0 = link down. */
	struct mpipe_queue_stats stats;	/* Stat data for the queue. */
};

/* Transmit queue description. */
struct mpipe_tx_queue {
	struct mpipe_queue q;		/* Common stuff. */
};

/* Receive queue description. */
struct mpipe_rx_queue {
	struct mpipe_queue q;		/* Common stuff. */
	gxio_mpipe_iqueue_t iqueue;	/* mPIPE iqueue. */
	gxio_mpipe_idesc_t *next_desc;	/* Next idesc to process. */
	int avail_descs;		/* Number of available descs. */
	void *rx_ring_mem;		/* DMA ring memory. */
};

struct mpipe_dev_priv {
	gxio_mpipe_context_t *context;	/* mPIPE context. */
	gxio_mpipe_link_t link;		/* mPIPE link for the device. */
	gxio_mpipe_equeue_t equeue;	/* mPIPE equeue. */
	unsigned equeue_size;		/* mPIPE equeue desc count. */
	int instance;			/* mPIPE instance. */
	int ering;			/* mPIPE eDMA ring. */
	int stack;			/* mPIPE buffer stack. */
	int channel;			/* Device channel. */
	int port_id;			/* DPDK port index. */
	struct rte_eth_dev *eth_dev;	/* DPDK device. */
	struct rte_mbuf **tx_comps;	/* TX completion array. */
	struct rte_mempool *rx_mpool;	/* mpool used by the rx queues. */
	unsigned rx_offset;		/* Receive head room. */
	unsigned rx_size_code;		/* mPIPE rx buffer size code. */
	int is_xaui:1,			/* Is this an xgbe or gbe? */
	    initialized:1,		/* Initialized port? */
	    running:1;			/* Running port? */
	struct ether_addr mac_addr;	/* MAC address. */
	unsigned nb_rx_queues;		/* Configured tx queues. */
	unsigned nb_tx_queues;		/* Configured rx queues. */
	int first_bucket;		/* mPIPE bucket start index. */
	int first_ring;			/* mPIPE notif ring start index. */
	int notif_group;		/* mPIPE notif group. */
	rte_atomic32_t dp_count __rte_cache_aligned;	/* DP Entry count. */
	int tx_stat_mapping[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	int rx_stat_mapping[RTE_ETHDEV_QUEUE_STAT_CNTRS];
};

#define mpipe_priv(dev)			\
	((struct mpipe_dev_priv*)(dev)->data->dev_private)

#define mpipe_name(priv)		\
	((priv)->eth_dev->data->name)

#define mpipe_rx_queue(priv, n)		\
	((struct mpipe_rx_queue *)(priv)->eth_dev->data->rx_queues[n])

#define mpipe_tx_queue(priv, n)		\
	((struct mpipe_tx_queue *)(priv)->eth_dev->data->tx_queues[n])

static void
mpipe_xmit_flush(struct mpipe_dev_priv *priv);

static void
mpipe_recv_flush(struct mpipe_dev_priv *priv);

static int mpipe_equeue_sizes[] = {
	[GXIO_MPIPE_EQUEUE_ENTRY_512]	= 512,
	[GXIO_MPIPE_EQUEUE_ENTRY_2K]	= 2048,
	[GXIO_MPIPE_EQUEUE_ENTRY_8K]	= 8192,
	[GXIO_MPIPE_EQUEUE_ENTRY_64K]	= 65536,
};

static int mpipe_iqueue_sizes[] = {
	[GXIO_MPIPE_IQUEUE_ENTRY_128]	= 128,
	[GXIO_MPIPE_IQUEUE_ENTRY_512]	= 512,
	[GXIO_MPIPE_IQUEUE_ENTRY_2K]	= 2048,
	[GXIO_MPIPE_IQUEUE_ENTRY_64K]	= 65536,
};

static int mpipe_buffer_sizes[] = {
	[GXIO_MPIPE_BUFFER_SIZE_128]	= 128,
	[GXIO_MPIPE_BUFFER_SIZE_256]	= 256,
	[GXIO_MPIPE_BUFFER_SIZE_512]	= 512,
	[GXIO_MPIPE_BUFFER_SIZE_1024]	= 1024,
	[GXIO_MPIPE_BUFFER_SIZE_1664]	= 1664,
	[GXIO_MPIPE_BUFFER_SIZE_4096]	= 4096,
	[GXIO_MPIPE_BUFFER_SIZE_10368]	= 10368,
	[GXIO_MPIPE_BUFFER_SIZE_16384]	= 16384,
};

static gxio_mpipe_context_t *
mpipe_context(int instance)
{
	if (instance < 0 || instance >= mpipe_instances)
		return NULL;
	return &mpipe_contexts[instance].context;
}

static int mpipe_channel_config(int instance, int channel,
				struct mpipe_channel_config *config)
{
	struct mpipe_channel_config *data;
	struct mpipe_context *context;
	gxio_mpipe_rules_t rules;
	int idx, rc = 0;

	if (instance < 0 || instance >= mpipe_instances ||
	    channel < 0 || channel >= MPIPE_MAX_CHANNELS)
		return -EINVAL;

	context = &mpipe_contexts[instance];

	rte_spinlock_lock(&context->lock);

	gxio_mpipe_rules_init(&rules, &context->context);

	for (idx = 0; idx < MPIPE_MAX_CHANNELS; idx++) {
		data = (channel == idx) ? config : &context->channels[idx];

		if (!data->enable)
			continue;

		rc = gxio_mpipe_rules_begin(&rules, data->first_bucket,
					    data->num_buckets, &data->stacks);
		if (rc < 0) {
			goto done;
		}

		rc = gxio_mpipe_rules_add_channel(&rules, idx);
		if (rc < 0) {
			goto done;
		}

		rc = gxio_mpipe_rules_set_headroom(&rules, data->head_room);
		if (rc < 0) {
			goto done;
		}
	}

	rc = gxio_mpipe_rules_commit(&rules);
	if (rc == 0) {
		memcpy(&context->channels[channel], config, sizeof(*config));
	}

done:
	rte_spinlock_unlock(&context->lock);

	return rc;
}

static int
mpipe_get_size_index(int *array, int count, int size,
		     bool roundup)
{
	int i, last = -1;

	for (i = 0; i < count && array[i] < size; i++) {
		if (array[i])
			last = i;
	}

	if (roundup)
		return i < count ? (int)i : -ENOENT;
	else
		return last >= 0 ? last : -ENOENT;
}

static int
mpipe_calc_size(int *array, int count, int size)
{
	int index = mpipe_get_size_index(array, count, size, 1);
	return index < 0 ? index : array[index];
}

static int mpipe_equeue_size(int size)
{
	int result;
	result = mpipe_calc_size(mpipe_equeue_sizes,
				 RTE_DIM(mpipe_equeue_sizes), size);
	return result;
}

static int mpipe_iqueue_size(int size)
{
	int result;
	result = mpipe_calc_size(mpipe_iqueue_sizes,
				 RTE_DIM(mpipe_iqueue_sizes), size);
	return result;
}

static int mpipe_buffer_size_index(int size)
{
	int result;
	result = mpipe_get_size_index(mpipe_buffer_sizes,
				      RTE_DIM(mpipe_buffer_sizes), size, 0);
	return result;
}

static inline int
mpipe_dev_atomic_read_link_status(struct rte_eth_dev *dev,
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
mpipe_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				   struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static void
mpipe_infos_get(struct rte_eth_dev *dev __rte_unused,
		struct rte_eth_dev_info *dev_info)
{
	dev_info->min_rx_bufsize  = 128;
	dev_info->max_rx_pktlen   = 1518;
	dev_info->max_tx_queues   = MPIPE_TX_MAX_QUEUES;
	dev_info->max_rx_queues   = MPIPE_RX_MAX_QUEUES;
	dev_info->max_mac_addrs   = 1;
	dev_info->rx_offload_capa = 0;
	dev_info->tx_offload_capa = 0;
}

static int
mpipe_configure(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);

	if (dev->data->nb_tx_queues > MPIPE_TX_MAX_QUEUES) {
		RTE_LOG(ERR, PMD, "%s: Too many tx queues: %d > %d\n",
			mpipe_name(priv), dev->data->nb_tx_queues,
			MPIPE_TX_MAX_QUEUES);
		return -EINVAL;
	}
	priv->nb_tx_queues = dev->data->nb_tx_queues;

	if (dev->data->nb_rx_queues > MPIPE_RX_MAX_QUEUES) {
		RTE_LOG(ERR, PMD, "%s: Too many rx queues: %d > %d\n",
			mpipe_name(priv), dev->data->nb_rx_queues,
			MPIPE_RX_MAX_QUEUES);
	}
	priv->nb_rx_queues = dev->data->nb_rx_queues;

	return 0;
}

static inline int
mpipe_link_compare(struct rte_eth_link *link1,
		   struct rte_eth_link *link2)
{
	return (*(uint64_t *)link1 == *(uint64_t *)link2)
		? -1 : 0;
}

static int
mpipe_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	struct rte_eth_link old, new;
	int64_t state, speed;
	int count, rc;

	memset(&old, 0, sizeof(old));
	memset(&new, 0, sizeof(new));
	mpipe_dev_atomic_read_link_status(dev, &old);

	for (count = 0, rc = 0; count < MPIPE_LINK_UPDATE_TIMEOUT; count++) {
		if (!priv->initialized)
			break;

		state = gxio_mpipe_link_get_attr(&priv->link,
						 GXIO_MPIPE_LINK_CURRENT_STATE);
		if (state < 0)
			break;

		speed = state & GXIO_MPIPE_LINK_SPEED_MASK;

		new.link_autoneg = (dev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_AUTONEG);
		if (speed == GXIO_MPIPE_LINK_1G) {
			new.link_speed = ETH_SPEED_NUM_1G;
			new.link_duplex = ETH_LINK_FULL_DUPLEX;
			new.link_status = ETH_LINK_UP;
		} else if (speed == GXIO_MPIPE_LINK_10G) {
			new.link_speed = ETH_SPEED_NUM_10G;
			new.link_duplex = ETH_LINK_FULL_DUPLEX;
			new.link_status = ETH_LINK_UP;
		}

		rc = mpipe_link_compare(&old, &new);
		if (rc == 0 || !wait_to_complete)
			break;

		rte_delay_us(MPIPE_LINK_UPDATE_INTERVAL);
	}

	mpipe_dev_atomic_write_link_status(dev, &new);
	return rc;
}

static int
mpipe_set_link(struct rte_eth_dev *dev, int up)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	int rc;

	rc = gxio_mpipe_link_set_attr(&priv->link,
				      GXIO_MPIPE_LINK_DESIRED_STATE,
				      up ? GXIO_MPIPE_LINK_ANYSPEED : 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to set link %s.\n",
			mpipe_name(priv), up ? "up" : "down");
	} else {
		mpipe_link_update(dev, 0);
	}

	return rc;
}

static int
mpipe_set_link_up(struct rte_eth_dev *dev)
{
	return mpipe_set_link(dev, 1);
}

static int
mpipe_set_link_down(struct rte_eth_dev *dev)
{
	return mpipe_set_link(dev, 0);
}

static inline void
mpipe_dp_enter(struct mpipe_dev_priv *priv)
{
	__insn_mtspr(SPR_DSTREAM_PF, 0);
	rte_atomic32_inc(&priv->dp_count);
}

static inline void
mpipe_dp_exit(struct mpipe_dev_priv *priv)
{
	rte_atomic32_dec(&priv->dp_count);
}

static inline void
mpipe_dp_wait(struct mpipe_dev_priv *priv)
{
	while (rte_atomic32_read(&priv->dp_count) != 0) {
		rte_pause();
	}
}

static inline int
mpipe_mbuf_stack_index(struct mpipe_dev_priv *priv, struct rte_mbuf *mbuf)
{
	return (mbuf->port < RTE_MAX_ETHPORTS) ?
		mpipe_priv(&rte_eth_devices[mbuf->port])->stack :
		priv->stack;
}

static inline struct rte_mbuf *
mpipe_recv_mbuf(struct mpipe_dev_priv *priv, gxio_mpipe_idesc_t *idesc,
		int in_port)
{
	void *va = gxio_mpipe_idesc_get_va(idesc);
	uint16_t size = gxio_mpipe_idesc_get_xfer_size(idesc);
	struct rte_mbuf *mbuf = RTE_PTR_SUB(va, priv->rx_offset);

	rte_pktmbuf_reset(mbuf);
	mbuf->data_off = (uintptr_t)va - (uintptr_t)mbuf->buf_addr;
	mbuf->port     = in_port;
	mbuf->data_len = size;
	mbuf->pkt_len  = size;
	mbuf->hash.rss = gxio_mpipe_idesc_get_flow_hash(idesc);

	PMD_DEBUG_RX("%s: RX mbuf %p, buffer %p, buf_addr %p, size %d\n",
		     mpipe_name(priv), mbuf, va, mbuf->buf_addr, size);

	return mbuf;
}

static inline void
mpipe_recv_push(struct mpipe_dev_priv *priv, struct rte_mbuf *mbuf)
{
	const int offset = RTE_PKTMBUF_HEADROOM + MPIPE_RX_IP_ALIGN;
	void *buf_addr = RTE_PTR_ADD(mbuf->buf_addr, offset);

	gxio_mpipe_push_buffer(priv->context, priv->stack, buf_addr);
	PMD_DEBUG_RX("%s: Pushed mbuf %p, buffer %p into stack %d\n",
		     mpipe_name(priv), mbuf, buf_addr, priv->stack);
}

static inline void
mpipe_recv_fill_stack(struct mpipe_dev_priv *priv, int count)
{
	struct rte_mbuf *mbuf;
	int i;

	for (i = 0; i < count; i++) {
		mbuf = rte_mbuf_raw_alloc(priv->rx_mpool);
		if (!mbuf)
			break;
		mpipe_recv_push(priv, mbuf);
	}

	PMD_DEBUG_RX("%s: Filled %d/%d buffers\n", mpipe_name(priv), i, count);
}

static inline void
mpipe_recv_flush_stack(struct mpipe_dev_priv *priv)
{
	const int offset = priv->rx_offset & ~RTE_MEMPOOL_ALIGN_MASK;
	uint8_t in_port = priv->port_id;
	struct rte_mbuf *mbuf;
	void *va;

	while (1) {
		va = gxio_mpipe_pop_buffer(priv->context, priv->stack);
		if (!va)
			break;
		mbuf = RTE_PTR_SUB(va, offset);

		PMD_DEBUG_RX("%s: Flushing mbuf %p, va %p\n",
			     mpipe_name(priv), mbuf, va);

		mbuf->data_off    = (uintptr_t)va - (uintptr_t)mbuf->buf_addr;
		mbuf->refcnt      = 1;
		mbuf->nb_segs     = 1;
		mbuf->port        = in_port;
		mbuf->packet_type = 0;
		mbuf->data_len    = 0;
		mbuf->pkt_len     = 0;

		__rte_mbuf_raw_free(mbuf);
	}
}

static void
mpipe_register_segment(struct mpipe_dev_priv *priv, const struct rte_memseg *ms)
{
	size_t size = ms->hugepage_sz;
	uint8_t *addr, *end;
	int rc;

	for (addr = ms->addr, end = addr + ms->len; addr < end; addr += size) {
		rc = gxio_mpipe_register_page(priv->context, priv->stack, addr,
					      size, 0);
		if (rc < 0)
			break;
	}

	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Could not register memseg @%p, %d.\n",
			mpipe_name(priv), ms->addr, rc);
	} else {
		RTE_LOG(DEBUG, PMD, "%s: Registered segment %p - %p\n",
			mpipe_name(priv), ms->addr,
			RTE_PTR_ADD(ms->addr, ms->len - 1));
	}
}

static int
mpipe_recv_init(struct mpipe_dev_priv *priv)
{
	const struct rte_memseg *seg = rte_eal_get_physmem_layout();
	size_t stack_size;
	void *stack_mem;
	int rc;

	if (!priv->rx_mpool) {
		RTE_LOG(ERR, PMD, "%s: No buffer pool.\n",
			mpipe_name(priv));
		return -ENODEV;
	}

	/* Allocate one NotifRing for each queue. */
	rc = gxio_mpipe_alloc_notif_rings(priv->context, MPIPE_RX_MAX_QUEUES,
					  0, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate notif rings.\n",
			mpipe_name(priv));
		return rc;
	}
	priv->first_ring = rc;

	/* Allocate a NotifGroup. */
	rc = gxio_mpipe_alloc_notif_groups(priv->context, 1, 0, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate rx group.\n",
			mpipe_name(priv));
		return rc;
	}
	priv->notif_group = rc;

	/* Allocate required buckets. */
	rc = gxio_mpipe_alloc_buckets(priv->context, MPIPE_RX_BUCKETS, 0, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate buckets.\n",
			mpipe_name(priv));
		return rc;
	}
	priv->first_bucket = rc;

	rc = gxio_mpipe_alloc_buffer_stacks(priv->context, 1, 0, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate buffer stack.\n",
			mpipe_name(priv));
		return rc;
	}
	priv->stack = rc;

	while (seg && seg->addr)
		mpipe_register_segment(priv, seg++);

	stack_size = gxio_mpipe_calc_buffer_stack_bytes(MPIPE_RX_STACK_SIZE);
	stack_mem = rte_zmalloc(NULL, stack_size, 65536);
	if (!stack_mem) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate buffer memory.\n",
			mpipe_name(priv));
		return -ENOMEM;
	} else {
		RTE_LOG(DEBUG, PMD, "%s: Buffer stack memory %p - %p.\n",
			mpipe_name(priv), stack_mem,
			RTE_PTR_ADD(stack_mem, stack_size - 1));
	}

	rc = gxio_mpipe_init_buffer_stack(priv->context, priv->stack,
					  priv->rx_size_code, stack_mem,
					  stack_size, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to initialize buffer stack.\n",
			mpipe_name(priv));
		return rc;
	}

	return 0;
}

static int
mpipe_xmit_init(struct mpipe_dev_priv *priv)
{
	size_t ring_size;
	void *ring_mem;
	int rc;

	/* Allocate eDMA ring. */
	rc = gxio_mpipe_alloc_edma_rings(priv->context, 1, 0, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to alloc tx ring.\n",
			mpipe_name(priv));
		return rc;
	}
	priv->ering = rc;

	rc = mpipe_equeue_size(MPIPE_TX_DESCS);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Cannot allocate %d equeue descs.\n",
			mpipe_name(priv), (int)MPIPE_TX_DESCS);
		return -ENOMEM;
	}
	priv->equeue_size = rc;

	/* Initialize completion array. */
	ring_size = sizeof(priv->tx_comps[0]) * priv->equeue_size;
	priv->tx_comps = rte_zmalloc(NULL, ring_size, RTE_CACHE_LINE_SIZE);
	if (!priv->tx_comps) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate egress comps.\n",
			mpipe_name(priv));
		return -ENOMEM;
	}

	/* Allocate eDMA ring memory. */
	ring_size = sizeof(gxio_mpipe_edesc_t) * priv->equeue_size;
	ring_mem = rte_zmalloc(NULL, ring_size, ring_size);
	if (!ring_mem) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate egress descs.\n",
			mpipe_name(priv));
		return -ENOMEM;
	} else {
		RTE_LOG(DEBUG, PMD, "%s: eDMA ring memory %p - %p.\n",
			mpipe_name(priv), ring_mem,
			RTE_PTR_ADD(ring_mem, ring_size - 1));
	}

	/* Initialize eDMA ring. */
	rc = gxio_mpipe_equeue_init(&priv->equeue, priv->context, priv->ering,
				    priv->channel, ring_mem, ring_size, 0);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to init equeue\n",
			mpipe_name(priv));
		return rc;
	}

	return 0;
}

static int
mpipe_link_init(struct mpipe_dev_priv *priv)
{
	int rc;

	/* Open the link. */
	rc = gxio_mpipe_link_open(&priv->link, priv->context,
				  mpipe_name(priv), GXIO_MPIPE_LINK_AUTO_NONE);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to open link.\n",
			mpipe_name(priv));
		return rc;
	}

	/* Get the channel index. */
	rc = gxio_mpipe_link_channel(&priv->link);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Bad channel\n",
			mpipe_name(priv));
		return rc;
	}
	priv->channel = rc;

	return 0;
}

static int
mpipe_init(struct mpipe_dev_priv *priv)
{
	int rc;

	if (priv->initialized)
		return 0;

	rc = mpipe_recv_init(priv);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to init rx.\n",
			mpipe_name(priv));
		return rc;
	}

	rc = mpipe_xmit_init(priv);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to init tx.\n",
			mpipe_name(priv));
		rte_free(priv);
		return rc;
	}

	priv->initialized = 1;

	return 0;
}

static int
mpipe_start(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	struct mpipe_channel_config config;
	struct mpipe_rx_queue *rx_queue;
	struct rte_eth_link eth_link;
	unsigned queue, buffers = 0;
	size_t ring_size;
	void *ring_mem;
	int rc;

	memset(&eth_link, 0, sizeof(eth_link));
	mpipe_dev_atomic_write_link_status(dev, &eth_link);

	rc = mpipe_init(priv);
	if (rc < 0)
		return rc;

	/* Initialize NotifRings. */
	for (queue = 0; queue < priv->nb_rx_queues; queue++) {
		rx_queue = mpipe_rx_queue(priv, queue);
		ring_size = rx_queue->q.nb_desc * sizeof(gxio_mpipe_idesc_t);

		ring_mem = rte_malloc(NULL, ring_size, ring_size);
		if (!ring_mem) {
			RTE_LOG(ERR, PMD, "%s: Failed to alloc rx descs.\n",
				mpipe_name(priv));
			return -ENOMEM;
		} else {
			RTE_LOG(DEBUG, PMD, "%s: iDMA ring %d memory %p - %p.\n",
				mpipe_name(priv), queue, ring_mem,
				RTE_PTR_ADD(ring_mem, ring_size - 1));
		}

		rc = gxio_mpipe_iqueue_init(&rx_queue->iqueue, priv->context,
					    priv->first_ring + queue, ring_mem,
					    ring_size, 0);
		if (rc < 0) {
			RTE_LOG(ERR, PMD, "%s: Failed to init rx queue.\n",
				mpipe_name(priv));
			return rc;
		}

		rx_queue->rx_ring_mem = ring_mem;
		buffers += rx_queue->q.nb_desc;
	}

	/* Initialize ingress NotifGroup and buckets. */
	rc = gxio_mpipe_init_notif_group_and_buckets(priv->context,
			priv->notif_group, priv->first_ring, priv->nb_rx_queues,
			priv->first_bucket, MPIPE_RX_BUCKETS,
			GXIO_MPIPE_BUCKET_STATIC_FLOW_AFFINITY);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to init group and buckets.\n",
			mpipe_name(priv));
		return rc;
	}

	/* Configure the classifier to deliver packets from this port. */
	config.enable = 1;
	config.first_bucket = priv->first_bucket;
	config.num_buckets = MPIPE_RX_BUCKETS;
	memset(&config.stacks, 0xff, sizeof(config.stacks));
	config.stacks.stacks[priv->rx_size_code] = priv->stack;
	config.head_room = priv->rx_offset & RTE_MEMPOOL_ALIGN_MASK;

	rc = mpipe_channel_config(priv->instance, priv->channel,
				  &config);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to setup classifier.\n",
			mpipe_name(priv));
		return rc;
	}

	/* Fill empty buffers into the buffer stack. */
	mpipe_recv_fill_stack(priv, buffers);

	/* Bring up the link. */
	mpipe_set_link_up(dev);

	/* Start xmit/recv on queues. */
	for (queue = 0; queue < priv->nb_tx_queues; queue++)
		mpipe_tx_queue(priv, queue)->q.link_status = ETH_LINK_UP;
	for (queue = 0; queue < priv->nb_rx_queues; queue++)
		mpipe_rx_queue(priv, queue)->q.link_status = ETH_LINK_UP;
	priv->running = 1;

	return 0;
}

static void
mpipe_stop(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	struct mpipe_channel_config config;
	unsigned queue;
	int rc;

	for (queue = 0; queue < priv->nb_tx_queues; queue++)
		mpipe_tx_queue(priv, queue)->q.link_status = ETH_LINK_DOWN;
	for (queue = 0; queue < priv->nb_rx_queues; queue++)
		mpipe_rx_queue(priv, queue)->q.link_status = ETH_LINK_DOWN;

	/* Make sure the link_status writes land. */
	rte_wmb();

	/*
	 * Wait for link_status change to register with straggling datapath
	 * threads.
	 */
	mpipe_dp_wait(priv);

	/* Bring down the link. */
	mpipe_set_link_down(dev);

	/* Remove classifier rules. */
	memset(&config, 0, sizeof(config));
	rc = mpipe_channel_config(priv->instance, priv->channel,
				  &config);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to stop classifier.\n",
			mpipe_name(priv));
	}

	/* Flush completed xmit packets. */
	mpipe_xmit_flush(priv);

	/* Flush buffer stacks. */
	mpipe_recv_flush(priv);

	priv->running = 0;
}

static void
mpipe_close(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	if (priv->running)
		mpipe_stop(dev);
}

static void
mpipe_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	struct mpipe_tx_queue *tx_queue;
	struct mpipe_rx_queue *rx_queue;
	unsigned i;
	uint16_t idx;

	memset(stats, 0, sizeof(*stats));

	for (i = 0; i < priv->nb_tx_queues; i++) {
		tx_queue = mpipe_tx_queue(priv, i);

		stats->opackets += tx_queue->q.stats.packets;
		stats->obytes   += tx_queue->q.stats.bytes;
		stats->oerrors  += tx_queue->q.stats.errors;

		idx = tx_queue->q.stat_idx;
		if (idx != (uint16_t)-1) {
			stats->q_opackets[idx] += tx_queue->q.stats.packets;
			stats->q_obytes[idx]   += tx_queue->q.stats.bytes;
			stats->q_errors[idx]   += tx_queue->q.stats.errors;
		}
	}

	for (i = 0; i < priv->nb_rx_queues; i++) {
		rx_queue = mpipe_rx_queue(priv, i);

		stats->ipackets  += rx_queue->q.stats.packets;
		stats->ibytes    += rx_queue->q.stats.bytes;
		stats->ierrors   += rx_queue->q.stats.errors;
		stats->rx_nombuf += rx_queue->q.stats.nomem;

		idx = rx_queue->q.stat_idx;
		if (idx != (uint16_t)-1) {
			stats->q_ipackets[idx] += rx_queue->q.stats.packets;
			stats->q_ibytes[idx]   += rx_queue->q.stats.bytes;
			stats->q_errors[idx]   += rx_queue->q.stats.errors;
		}
	}
}

static void
mpipe_stats_reset(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	struct mpipe_tx_queue *tx_queue;
	struct mpipe_rx_queue *rx_queue;
	unsigned i;

	for (i = 0; i < priv->nb_tx_queues; i++) {
		tx_queue = mpipe_tx_queue(priv, i);
		memset(&tx_queue->q.stats, 0, sizeof(tx_queue->q.stats));
	}

	for (i = 0; i < priv->nb_rx_queues; i++) {
		rx_queue = mpipe_rx_queue(priv, i);
		memset(&rx_queue->q.stats, 0, sizeof(rx_queue->q.stats));
	}
}

static int
mpipe_queue_stats_mapping_set(struct rte_eth_dev *dev, uint16_t queue_id,
			      uint8_t stat_idx, uint8_t is_rx)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);

	if (is_rx) {
		priv->rx_stat_mapping[stat_idx] = queue_id;
	} else {
		priv->tx_stat_mapping[stat_idx] = queue_id;
	}

	return 0;
}

static int
mpipe_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		     uint16_t nb_desc, unsigned int socket_id __rte_unused,
		     const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct mpipe_tx_queue *tx_queue = dev->data->tx_queues[queue_idx];
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	uint16_t idx;

	tx_queue = rte_realloc(tx_queue, sizeof(*tx_queue),
			       RTE_CACHE_LINE_SIZE);
	if (!tx_queue) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate TX queue.\n",
			mpipe_name(priv));
		return -ENOMEM;
	}

	memset(&tx_queue->q, 0, sizeof(tx_queue->q));
	tx_queue->q.priv = priv;
	tx_queue->q.queue_idx = queue_idx;
	tx_queue->q.port_id = dev->data->port_id;
	tx_queue->q.nb_desc = nb_desc;

	tx_queue->q.stat_idx = -1;
	for (idx = 0; idx < RTE_ETHDEV_QUEUE_STAT_CNTRS; idx++) {
		if (priv->tx_stat_mapping[idx] == queue_idx)
			tx_queue->q.stat_idx = idx;
	}

	dev->data->tx_queues[queue_idx] = tx_queue;

	return 0;
}

static void
mpipe_tx_queue_release(void *_txq)
{
	rte_free(_txq);
}

static int
mpipe_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		     uint16_t nb_desc, unsigned int socket_id __rte_unused,
		     const struct rte_eth_rxconf *rx_conf __rte_unused,
		     struct rte_mempool *mp)
{
	struct mpipe_rx_queue *rx_queue = dev->data->rx_queues[queue_idx];
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	uint16_t idx;
	int size, rc;

	rc = mpipe_iqueue_size(nb_desc);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Cannot allocate %d iqueue descs.\n",
			mpipe_name(priv), (int)nb_desc);
		return -ENOMEM;
	}

	if (rc != nb_desc) {
		RTE_LOG(WARNING, PMD, "%s: Extending RX descs from %d to %d.\n",
			mpipe_name(priv), (int)nb_desc, rc);
		nb_desc = rc;
	}

	size = sizeof(*rx_queue);
	rx_queue = rte_realloc(rx_queue, size, RTE_CACHE_LINE_SIZE);
	if (!rx_queue) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate RX queue.\n",
			mpipe_name(priv));
		return -ENOMEM;
	}

	memset(&rx_queue->q, 0, sizeof(rx_queue->q));
	rx_queue->q.priv = priv;
	rx_queue->q.nb_desc = nb_desc;
	rx_queue->q.port_id = dev->data->port_id;
	rx_queue->q.queue_idx = queue_idx;

	if (!priv->rx_mpool) {
		int size = (rte_pktmbuf_data_room_size(mp) -
			    RTE_PKTMBUF_HEADROOM -
			    MPIPE_RX_IP_ALIGN);

		priv->rx_offset = (sizeof(struct rte_mbuf) +
				   rte_pktmbuf_priv_size(mp) +
				   RTE_PKTMBUF_HEADROOM +
				   MPIPE_RX_IP_ALIGN);
		if (size < 0) {
			RTE_LOG(ERR, PMD, "%s: Bad buffer size %d.\n",
				mpipe_name(priv),
				rte_pktmbuf_data_room_size(mp));
			return -ENOMEM;
		}

		priv->rx_size_code = mpipe_buffer_size_index(size);
		priv->rx_mpool = mp;
	}

	if (priv->rx_mpool != mp) {
		RTE_LOG(WARNING, PMD, "%s: Ignoring multiple buffer pools.\n",
			mpipe_name(priv));
	}

	rx_queue->q.stat_idx = -1;
	for (idx = 0; idx < RTE_ETHDEV_QUEUE_STAT_CNTRS; idx++) {
		if (priv->rx_stat_mapping[idx] == queue_idx)
			rx_queue->q.stat_idx = idx;
	}

	dev->data->rx_queues[queue_idx] = rx_queue;

	return 0;
}

static void
mpipe_rx_queue_release(void *_rxq)
{
	rte_free(_rxq);
}

#define MPIPE_XGBE_ENA_HASH_MULTI	\
	(1UL << MPIPE_XAUI_RECEIVE_CONFIGURATION__ENA_HASH_MULTI_SHIFT)
#define MPIPE_XGBE_ENA_HASH_UNI		\
	(1UL << MPIPE_XAUI_RECEIVE_CONFIGURATION__ENA_HASH_UNI_SHIFT)
#define MPIPE_XGBE_COPY_ALL		\
	(1UL << MPIPE_XAUI_RECEIVE_CONFIGURATION__COPY_ALL_SHIFT)
#define MPIPE_GBE_ENA_MULTI_HASH	\
	(1UL << MPIPE_GBE_NETWORK_CONFIGURATION__MULTI_HASH_ENA_SHIFT)
#define MPIPE_GBE_ENA_UNI_HASH		\
	(1UL << MPIPE_GBE_NETWORK_CONFIGURATION__UNI_HASH_ENA_SHIFT)
#define MPIPE_GBE_COPY_ALL		\
	(1UL << MPIPE_GBE_NETWORK_CONFIGURATION__COPY_ALL_SHIFT)

static void
mpipe_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	int64_t reg;
	int addr;

	if (priv->is_xaui) {
		addr = MPIPE_XAUI_RECEIVE_CONFIGURATION;
		reg  = gxio_mpipe_link_mac_rd(&priv->link, addr);
		reg &= ~MPIPE_XGBE_ENA_HASH_MULTI;
		reg &= ~MPIPE_XGBE_ENA_HASH_UNI;
		reg |=  MPIPE_XGBE_COPY_ALL;
		gxio_mpipe_link_mac_wr(&priv->link, addr, reg);
	} else {
		addr = MPIPE_GBE_NETWORK_CONFIGURATION;
		reg  = gxio_mpipe_link_mac_rd(&priv->link, addr);
		reg &= ~MPIPE_GBE_ENA_MULTI_HASH;
		reg &= ~MPIPE_GBE_ENA_UNI_HASH;
		reg |=  MPIPE_GBE_COPY_ALL;
		gxio_mpipe_link_mac_wr(&priv->link, addr, reg);
	}
}

static void
mpipe_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct mpipe_dev_priv *priv = mpipe_priv(dev);
	int64_t reg;
	int addr;

	if (priv->is_xaui) {
		addr = MPIPE_XAUI_RECEIVE_CONFIGURATION;
		reg  = gxio_mpipe_link_mac_rd(&priv->link, addr);
		reg |=  MPIPE_XGBE_ENA_HASH_MULTI;
		reg |=  MPIPE_XGBE_ENA_HASH_UNI;
		reg &= ~MPIPE_XGBE_COPY_ALL;
		gxio_mpipe_link_mac_wr(&priv->link, addr, reg);
	} else {
		addr = MPIPE_GBE_NETWORK_CONFIGURATION;
		reg  = gxio_mpipe_link_mac_rd(&priv->link, addr);
		reg |=  MPIPE_GBE_ENA_MULTI_HASH;
		reg |=  MPIPE_GBE_ENA_UNI_HASH;
		reg &= ~MPIPE_GBE_COPY_ALL;
		gxio_mpipe_link_mac_wr(&priv->link, addr, reg);
	}
}

static const struct eth_dev_ops mpipe_dev_ops = {
	.dev_infos_get	         = mpipe_infos_get,
	.dev_configure	         = mpipe_configure,
	.dev_start	         = mpipe_start,
	.dev_stop	         = mpipe_stop,
	.dev_close	         = mpipe_close,
	.stats_get	         = mpipe_stats_get,
	.stats_reset	         = mpipe_stats_reset,
	.queue_stats_mapping_set = mpipe_queue_stats_mapping_set,
	.tx_queue_setup	         = mpipe_tx_queue_setup,
	.rx_queue_setup	         = mpipe_rx_queue_setup,
	.tx_queue_release	 = mpipe_tx_queue_release,
	.rx_queue_release	 = mpipe_rx_queue_release,
	.link_update	         = mpipe_link_update,
	.dev_set_link_up         = mpipe_set_link_up,
	.dev_set_link_down       = mpipe_set_link_down,
	.promiscuous_enable      = mpipe_promiscuous_enable,
	.promiscuous_disable     = mpipe_promiscuous_disable,
};

static inline void
mpipe_xmit_null(struct mpipe_dev_priv *priv, int64_t start, int64_t end)
{
	gxio_mpipe_edesc_t null_desc = { { .bound = 1, .ns = 1 } };
	gxio_mpipe_equeue_t *equeue = &priv->equeue;
	int64_t slot;

	for (slot = start; slot < end; slot++) {
		gxio_mpipe_equeue_put_at(equeue, null_desc, slot);
	}
}

static void
mpipe_xmit_flush(struct mpipe_dev_priv *priv)
{
	gxio_mpipe_equeue_t *equeue = &priv->equeue;
	int64_t slot;

	/* Post a dummy descriptor and wait for its return. */
	slot = gxio_mpipe_equeue_reserve(equeue, 1);
	if (slot < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to reserve stop slot.\n",
			mpipe_name(priv));
		return;
	}

	mpipe_xmit_null(priv, slot, slot + 1);

	while (!gxio_mpipe_equeue_is_complete(equeue, slot, 1)) {
		rte_pause();
	}

	for (slot = 0; slot < priv->equeue_size; slot++) {
		if (priv->tx_comps[slot])
			rte_pktmbuf_free_seg(priv->tx_comps[slot]);
	}
}

static void
mpipe_recv_flush(struct mpipe_dev_priv *priv)
{
	uint8_t in_port = priv->port_id;
	struct mpipe_rx_queue *rx_queue;
	gxio_mpipe_iqueue_t *iqueue;
	gxio_mpipe_idesc_t idesc;
	struct rte_mbuf *mbuf;
	unsigned queue;

	/* Release packets on the buffer stack. */
	mpipe_recv_flush_stack(priv);

	/* Flush packets sitting in recv queues. */
	for (queue = 0; queue < priv->nb_rx_queues; queue++) {
		rx_queue = mpipe_rx_queue(priv, queue);
		iqueue = &rx_queue->iqueue;
		while (gxio_mpipe_iqueue_try_get(iqueue, &idesc) >= 0) {
			/* Skip idesc with the 'buffer error' bit set. */
			if (idesc.be)
				continue;
			mbuf = mpipe_recv_mbuf(priv, &idesc, in_port);
			rte_pktmbuf_free(mbuf);
		}
		rte_free(rx_queue->rx_ring_mem);
	}
}

static inline uint16_t
mpipe_do_xmit(struct mpipe_tx_queue *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t nb_pkts)
{
	struct mpipe_dev_priv *priv = tx_queue->q.priv;
	gxio_mpipe_equeue_t *equeue = &priv->equeue;
	unsigned nb_bytes = 0;
	unsigned nb_sent = 0;
	int nb_slots, i;
	uint8_t port_id;

	PMD_DEBUG_TX("Trying to transmit %d packets on %s:%d.\n",
		     nb_pkts, mpipe_name(tx_queue->q.priv),
		     tx_queue->q.queue_idx);

	/* Optimistic assumption that we need exactly one slot per packet. */
	nb_slots = RTE_MIN(nb_pkts, MPIPE_TX_DESCS / 2);

	do {
		struct rte_mbuf *mbuf = NULL, *pkt = NULL;
		int64_t slot;

		/* Reserve eDMA ring slots. */
		slot = gxio_mpipe_equeue_try_reserve_fast(equeue, nb_slots);
		if (unlikely(slot < 0)) {
			break;
		}

		for (i = 0; i < nb_slots; i++) {
			unsigned idx = (slot + i) & (priv->equeue_size - 1);
			rte_prefetch0(priv->tx_comps[idx]);
		}

		/* Fill up slots with descriptor and completion info. */
		for (i = 0; i < nb_slots; i++) {
			unsigned idx = (slot + i) & (priv->equeue_size - 1);
			gxio_mpipe_edesc_t desc;
			struct rte_mbuf *next;

			/* Starting on a new packet? */
			if (likely(!mbuf)) {
				int room = nb_slots - i;

				pkt = mbuf = tx_pkts[nb_sent];

				/* Bail out if we run out of descs. */
				if (unlikely(pkt->nb_segs > room))
					break;

				nb_sent++;
			}

			/* We have a segment to send. */
			next = mbuf->next;

			if (priv->tx_comps[idx])
				rte_pktmbuf_free_seg(priv->tx_comps[idx]);

			port_id = (mbuf->port < RTE_MAX_ETHPORTS) ?
						mbuf->port : priv->port_id;
			desc = (gxio_mpipe_edesc_t) { {
				.va        = rte_pktmbuf_mtod(mbuf, uintptr_t),
				.xfer_size = rte_pktmbuf_data_len(mbuf),
				.bound     = next ? 0 : 1,
				.stack_idx = mpipe_mbuf_stack_index(priv, mbuf),
				.size      = priv->rx_size_code,
			} };
			if (mpipe_local.mbuf_push_debt[port_id] > 0) {
				mpipe_local.mbuf_push_debt[port_id]--;
				desc.hwb = 1;
				priv->tx_comps[idx] = NULL;
			} else
				priv->tx_comps[idx] = mbuf;

			nb_bytes += mbuf->data_len;
			gxio_mpipe_equeue_put_at(equeue, desc, slot + i);

			PMD_DEBUG_TX("%s:%d: Sending packet %p, len %d\n",
				     mpipe_name(priv),
				     tx_queue->q.queue_idx,
				     rte_pktmbuf_mtod(mbuf, void *),
				     rte_pktmbuf_data_len(mbuf));

			mbuf = next;
		}

		if (unlikely(nb_sent < nb_pkts)) {

			/* Fill remaining slots with null descriptors. */
			mpipe_xmit_null(priv, slot + i, slot + nb_slots);

			/*
			 * Calculate exact number of descriptors needed for
			 * the next go around.
			 */
			nb_slots = 0;
			for (i = nb_sent; i < nb_pkts; i++) {
				nb_slots += tx_pkts[i]->nb_segs;
			}

			nb_slots = RTE_MIN(nb_slots, MPIPE_TX_DESCS / 2);
		}
	} while (nb_sent < nb_pkts);

	tx_queue->q.stats.packets += nb_sent;
	tx_queue->q.stats.bytes   += nb_bytes;

	return nb_sent;
}

static inline uint16_t
mpipe_do_recv(struct mpipe_rx_queue *rx_queue, struct rte_mbuf **rx_pkts,
	      uint16_t nb_pkts)
{
	struct mpipe_dev_priv *priv = rx_queue->q.priv;
	gxio_mpipe_iqueue_t *iqueue = &rx_queue->iqueue;
	gxio_mpipe_idesc_t *first_idesc, *idesc, *last_idesc;
	uint8_t in_port = rx_queue->q.port_id;
	const unsigned look_ahead = 8;
	int room = nb_pkts, rc = 0;
	unsigned nb_packets = 0;
	unsigned nb_dropped = 0;
	unsigned nb_nomem = 0;
	unsigned nb_bytes = 0;
	unsigned nb_descs, i;

	while (room && !rc) {
		if (rx_queue->avail_descs < room) {
			rc = gxio_mpipe_iqueue_try_peek(iqueue,
							&rx_queue->next_desc);
			rx_queue->avail_descs = rc < 0 ? 0 : rc;
		}

		if (unlikely(!rx_queue->avail_descs)) {
			break;
		}

		nb_descs = RTE_MIN(room, rx_queue->avail_descs);

		first_idesc = rx_queue->next_desc;
		last_idesc  = first_idesc + nb_descs;

		rx_queue->next_desc   += nb_descs;
		rx_queue->avail_descs -= nb_descs;

		for (i = 1; i < look_ahead; i++) {
			rte_prefetch0(first_idesc + i);
		}

		PMD_DEBUG_RX("%s:%d: Trying to receive %d packets\n",
			     mpipe_name(rx_queue->q.priv),
			     rx_queue->q.queue_idx,
			     nb_descs);

		for (idesc = first_idesc; idesc < last_idesc; idesc++) {
			struct rte_mbuf *mbuf;

			PMD_DEBUG_RX("%s:%d: processing idesc %d/%d\n",
				     mpipe_name(priv),
				     rx_queue->q.queue_idx,
				     nb_packets, nb_descs);

			rte_prefetch0(idesc + look_ahead);

			PMD_DEBUG_RX("%s:%d: idesc %p, %s%s%s%s%s%s%s%s%s%s"
				     "size: %d, bkt: %d, chan: %d, ring: %d, sqn: %lu, va: %lu\n",
				     mpipe_name(priv),
				     rx_queue->q.queue_idx,
				     idesc,
				     idesc->me ? "me, " : "",
				     idesc->tr ? "tr, " : "",
				     idesc->ce ? "ce, " : "",
				     idesc->ct ? "ct, " : "",
				     idesc->cs ? "cs, " : "",
				     idesc->nr ? "nr, " : "",
				     idesc->sq ? "sq, " : "",
				     idesc->ts ? "ts, " : "",
				     idesc->ps ? "ps, " : "",
				     idesc->be ? "be, " : "",
				     idesc->l2_size,
				     idesc->bucket_id,
				     idesc->channel,
				     idesc->notif_ring,
				     (unsigned long)idesc->packet_sqn,
				     (unsigned long)idesc->va);

			if (unlikely(gxio_mpipe_idesc_has_error(idesc))) {
				nb_dropped++;
				gxio_mpipe_iqueue_drop(iqueue, idesc);
				PMD_DEBUG_RX("%s:%d: Descriptor error\n",
					     mpipe_name(rx_queue->q.priv),
					     rx_queue->q.queue_idx);
				continue;
			}

			if (mpipe_local.mbuf_push_debt[in_port] <
					MPIPE_BUF_DEBT_THRESHOLD)
				mpipe_local.mbuf_push_debt[in_port]++;
			else {
				mbuf = rte_mbuf_raw_alloc(priv->rx_mpool);
				if (unlikely(!mbuf)) {
					nb_nomem++;
					gxio_mpipe_iqueue_drop(iqueue, idesc);
					PMD_DEBUG_RX("%s:%d: alloc failure\n",
					     mpipe_name(rx_queue->q.priv),
					     rx_queue->q.queue_idx);
					continue;
				}

				mpipe_recv_push(priv, mbuf);
			}

			/* Get and setup the mbuf for the received packet. */
			mbuf = mpipe_recv_mbuf(priv, idesc, in_port);

			/* Update results and statistics counters. */
			rx_pkts[nb_packets] = mbuf;
			nb_bytes += mbuf->pkt_len;
			nb_packets++;
		}

		/*
		 * We release the ring in bursts, but do not track and release
		 * buckets.  This therefore breaks dynamic flow affinity, but
		 * we always operate in static affinity mode, and so we're OK
		 * with this optimization.
		 */
		gxio_mpipe_iqueue_advance(iqueue, nb_descs);
		gxio_mpipe_credit(iqueue->context, iqueue->ring, -1, nb_descs);

		/*
		 * Go around once more if we haven't yet peeked the queue, and
		 * if we have more room to receive.
		 */
		room = nb_pkts - nb_packets;
	}

	rx_queue->q.stats.packets += nb_packets;
	rx_queue->q.stats.bytes   += nb_bytes;
	rx_queue->q.stats.errors  += nb_dropped;
	rx_queue->q.stats.nomem   += nb_nomem;

	PMD_DEBUG_RX("%s:%d: RX: %d/%d pkts/bytes, %d/%d drops/nomem\n",
		     mpipe_name(rx_queue->q.priv), rx_queue->q.queue_idx,
		     nb_packets, nb_bytes, nb_dropped, nb_nomem);

	return nb_packets;
}

static uint16_t
mpipe_recv_pkts(void *_rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct mpipe_rx_queue *rx_queue = _rxq;
	uint16_t result = 0;

	if (rx_queue) {
		mpipe_dp_enter(rx_queue->q.priv);
		if (likely(rx_queue->q.link_status))
			result = mpipe_do_recv(rx_queue, rx_pkts, nb_pkts);
		mpipe_dp_exit(rx_queue->q.priv);
	}

	return result;
}

static uint16_t
mpipe_xmit_pkts(void *_txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct mpipe_tx_queue *tx_queue = _txq;
	uint16_t result = 0;

	if (tx_queue) {
		mpipe_dp_enter(tx_queue->q.priv);
		if (likely(tx_queue->q.link_status))
			result = mpipe_do_xmit(tx_queue, tx_pkts, nb_pkts);
		mpipe_dp_exit(tx_queue->q.priv);
	}

	return result;
}

static int
mpipe_link_mac(const char *ifname, uint8_t *mac)
{
	int rc, idx;
	char name[GXIO_MPIPE_LINK_NAME_LEN];

	for (idx = 0, rc = 0; !rc; idx++) {
		rc = gxio_mpipe_link_enumerate_mac(idx, name, mac);
		if (!rc && !strncmp(name, ifname, GXIO_MPIPE_LINK_NAME_LEN))
			return 0;
	}
	return -ENODEV;
}

static int
rte_pmd_mpipe_probe(const char *ifname,
		      const char *params __rte_unused)
{
	gxio_mpipe_context_t *context;
	struct rte_eth_dev *eth_dev;
	struct mpipe_dev_priv *priv;
	int instance, rc;
	uint8_t *mac;

	/* Get the mPIPE instance that the device belongs to. */
	instance = gxio_mpipe_link_instance(ifname);
	context = mpipe_context(instance);
	if (!context) {
		RTE_LOG(ERR, PMD, "%s: No device for link.\n", ifname);
		return -ENODEV;
	}

	priv = rte_zmalloc(NULL, sizeof(*priv), 0);
	if (!priv) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate priv.\n", ifname);
		return -ENOMEM;
	}

	memset(&priv->tx_stat_mapping, 0xff, sizeof(priv->tx_stat_mapping));
	memset(&priv->rx_stat_mapping, 0xff, sizeof(priv->rx_stat_mapping));
	priv->context = context;
	priv->instance = instance;
	priv->is_xaui = (strncmp(ifname, "xgbe", 4) == 0);
	priv->channel = -1;

	mac = priv->mac_addr.addr_bytes;
	rc = mpipe_link_mac(ifname, mac);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to enumerate link.\n", ifname);
		rte_free(priv);
		return -ENODEV;
	}

	eth_dev = rte_eth_dev_allocate(ifname);
	if (!eth_dev) {
		RTE_LOG(ERR, PMD, "%s: Failed to allocate device.\n", ifname);
		rte_free(priv);
		return -ENOMEM;
	}

	RTE_LOG(INFO, PMD, "%s: Initialized mpipe device"
		"(mac %02x:%02x:%02x:%02x:%02x:%02x).\n",
		ifname, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	priv->eth_dev = eth_dev;
	priv->port_id = eth_dev->data->port_id;
	eth_dev->data->dev_private = priv;
	eth_dev->data->mac_addrs = &priv->mac_addr;

	eth_dev->data->dev_flags = 0;
	eth_dev->data->kdrv = RTE_KDRV_NONE;
	eth_dev->driver = NULL;
	eth_dev->data->drv_name = drivername;
	eth_dev->data->numa_node = instance;

	eth_dev->dev_ops      = &mpipe_dev_ops;
	eth_dev->rx_pkt_burst = &mpipe_recv_pkts;
	eth_dev->tx_pkt_burst = &mpipe_xmit_pkts;

	rc = mpipe_link_init(priv);
	if (rc < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to init link.\n",
			mpipe_name(priv));
		return rc;
	}

	return 0;
}

static struct rte_vdev_driver pmd_mpipe_xgbe_drv = {
	.probe = rte_pmd_mpipe_probe,
};

static struct rte_vdev_driver pmd_mpipe_gbe_drv = {
	.probe = rte_pmd_mpipe_probe,
};

RTE_PMD_REGISTER_VDEV(net_mpipe_xgbe, pmd_mpipe_xgbe_drv);
RTE_PMD_REGISTER_ALIAS(net_mpipe_xgbe, xgbe);
RTE_PMD_REGISTER_VDEV(net_mpipe_gbe, pmd_mpipe_gbe_drv);
RTE_PMD_REGISTER_ALIAS(net_mpipe_gbe, gbe);

static void __attribute__((constructor, used))
mpipe_init_contexts(void)
{
	struct mpipe_context *context;
	int rc, instance;

	for (instance = 0; instance < GXIO_MPIPE_INSTANCE_MAX; instance++) {
		context = &mpipe_contexts[instance];

		rte_spinlock_init(&context->lock);
		rc = gxio_mpipe_init(&context->context, instance);
		if (rc < 0)
			break;
	}

	mpipe_instances = instance;
}
