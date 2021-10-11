/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <ethdev_driver.h>

#include "efx.h"

#include "sfc_log.h"
#include "sfc_debug.h"
#include "sfc_repr.h"
#include "sfc_ethdev_state.h"
#include "sfc_repr_proxy_api.h"
#include "sfc_switch.h"

/** Multi-process shared representor private data */
struct sfc_repr_shared {
	uint16_t		pf_port_id;
	uint16_t		repr_id;
	uint16_t		switch_domain_id;
	uint16_t		switch_port_id;
};

struct sfc_repr_rxq {
	/* Datapath members */
	struct rte_ring			*ring;
};

struct sfc_repr_txq {
	/* Datapath members */
	struct rte_ring			*ring;
	efx_mport_id_t			egress_mport;
};

/** Primary process representor private data */
struct sfc_repr {
	/**
	 * PMD setup and configuration is not thread safe. Since it is not
	 * performance sensitive, it is better to guarantee thread-safety
	 * and add device level lock. Adapter control operations which
	 * change its state should acquire the lock.
	 */
	rte_spinlock_t			lock;
	enum sfc_ethdev_state		state;
};

#define sfcr_err(sr, ...) \
	do {								\
		const struct sfc_repr *_sr = (sr);			\
									\
		(void)_sr;						\
		SFC_GENERIC_LOG(ERR, __VA_ARGS__);			\
	} while (0)

#define sfcr_warn(sr, ...) \
	do {								\
		const struct sfc_repr *_sr = (sr);			\
									\
		(void)_sr;						\
		SFC_GENERIC_LOG(WARNING, __VA_ARGS__);			\
	} while (0)

#define sfcr_info(sr, ...) \
	do {								\
		const struct sfc_repr *_sr = (sr);			\
									\
		(void)_sr;						\
		SFC_GENERIC_LOG(INFO,					\
				RTE_FMT("%s() "				\
				RTE_FMT_HEAD(__VA_ARGS__ ,),		\
				__func__,				\
				RTE_FMT_TAIL(__VA_ARGS__ ,)));		\
	} while (0)

static inline struct sfc_repr_shared *
sfc_repr_shared_by_eth_dev(struct rte_eth_dev *eth_dev)
{
	struct sfc_repr_shared *srs = eth_dev->data->dev_private;

	return srs;
}

static inline struct sfc_repr *
sfc_repr_by_eth_dev(struct rte_eth_dev *eth_dev)
{
	struct sfc_repr *sr = eth_dev->process_private;

	return sr;
}

/*
 * Add wrapper functions to acquire/release lock to be able to remove or
 * change the lock in one place.
 */

static inline void
sfc_repr_lock_init(struct sfc_repr *sr)
{
	rte_spinlock_init(&sr->lock);
}

#if defined(RTE_LIBRTE_SFC_EFX_DEBUG) || defined(RTE_ENABLE_ASSERT)

static inline int
sfc_repr_lock_is_locked(struct sfc_repr *sr)
{
	return rte_spinlock_is_locked(&sr->lock);
}

#endif

static inline void
sfc_repr_lock(struct sfc_repr *sr)
{
	rte_spinlock_lock(&sr->lock);
}

static inline void
sfc_repr_unlock(struct sfc_repr *sr)
{
	rte_spinlock_unlock(&sr->lock);
}

static inline void
sfc_repr_lock_fini(__rte_unused struct sfc_repr *sr)
{
	/* Just for symmetry of the API */
}

static int
sfc_repr_check_conf(struct sfc_repr *sr, uint16_t nb_rx_queues,
		    const struct rte_eth_conf *conf)
{
	const struct rte_eth_rss_conf *rss_conf;
	int ret = 0;

	sfcr_info(sr, "entry");

	if (conf->link_speeds != 0) {
		sfcr_err(sr, "specific link speeds not supported");
		ret = -EINVAL;
	}

	switch (conf->rxmode.mq_mode) {
	case ETH_MQ_RX_RSS:
		if (nb_rx_queues != 1) {
			sfcr_err(sr, "Rx RSS is not supported with %u queues",
				 nb_rx_queues);
			ret = -EINVAL;
			break;
		}

		rss_conf = &conf->rx_adv_conf.rss_conf;
		if (rss_conf->rss_key != NULL || rss_conf->rss_key_len != 0 ||
		    rss_conf->rss_hf != 0) {
			sfcr_err(sr, "Rx RSS configuration is not supported");
			ret = -EINVAL;
		}
		break;
	case ETH_MQ_RX_NONE:
		break;
	default:
		sfcr_err(sr, "Rx mode MQ modes other than RSS not supported");
		ret = -EINVAL;
		break;
	}

	if (conf->txmode.mq_mode != ETH_MQ_TX_NONE) {
		sfcr_err(sr, "Tx mode MQ modes not supported");
		ret = -EINVAL;
	}

	if (conf->lpbk_mode != 0) {
		sfcr_err(sr, "loopback not supported");
		ret = -EINVAL;
	}

	if (conf->dcb_capability_en != 0) {
		sfcr_err(sr, "priority-based flow control not supported");
		ret = -EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		sfcr_err(sr, "Flow Director not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.lsc != 0) {
		sfcr_err(sr, "link status change interrupt not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.rxq != 0) {
		sfcr_err(sr, "receive queue interrupt not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.rmv != 0) {
		sfcr_err(sr, "remove interrupt not supported");
		ret = -EINVAL;
	}

	sfcr_info(sr, "done %d", ret);

	return ret;
}


static int
sfc_repr_configure(struct sfc_repr *sr, uint16_t nb_rx_queues,
		   const struct rte_eth_conf *conf)
{
	int ret;

	sfcr_info(sr, "entry");

	SFC_ASSERT(sfc_repr_lock_is_locked(sr));

	ret = sfc_repr_check_conf(sr, nb_rx_queues, conf);
	if (ret != 0)
		goto fail_check_conf;

	sr->state = SFC_ETHDEV_CONFIGURED;

	sfcr_info(sr, "done");

	return 0;

fail_check_conf:
	sfcr_info(sr, "failed %s", rte_strerror(-ret));
	return ret;
}

static int
sfc_repr_dev_configure(struct rte_eth_dev *dev)
{
	struct sfc_repr *sr = sfc_repr_by_eth_dev(dev);
	struct rte_eth_dev_data *dev_data = dev->data;
	int ret;

	sfcr_info(sr, "entry n_rxq=%u n_txq=%u",
		  dev_data->nb_rx_queues, dev_data->nb_tx_queues);

	sfc_repr_lock(sr);
	switch (sr->state) {
	case SFC_ETHDEV_CONFIGURED:
		/* FALLTHROUGH */
	case SFC_ETHDEV_INITIALIZED:
		ret = sfc_repr_configure(sr, dev_data->nb_rx_queues,
					 &dev_data->dev_conf);
		break;
	default:
		sfcr_err(sr, "unexpected adapter state %u to configure",
			 sr->state);
		ret = -EINVAL;
		break;
	}
	sfc_repr_unlock(sr);

	sfcr_info(sr, "done %s", rte_strerror(-ret));

	return ret;
}

static int
sfc_repr_dev_infos_get(struct rte_eth_dev *dev,
		       struct rte_eth_dev_info *dev_info)
{
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);

	dev_info->device = dev->device;

	dev_info->max_rx_queues = SFC_REPR_RXQ_MAX;
	dev_info->max_tx_queues = SFC_REPR_TXQ_MAX;
	dev_info->default_rxconf.rx_drop_en = 1;
	dev_info->switch_info.domain_id = srs->switch_domain_id;
	dev_info->switch_info.port_id = srs->switch_port_id;

	return 0;
}

static int
sfc_repr_ring_create(uint16_t pf_port_id, uint16_t repr_id,
		     const char *type_name, uint16_t qid, uint16_t nb_desc,
		     unsigned int socket_id, struct rte_ring **ring)
{
	char ring_name[RTE_RING_NAMESIZE];
	int ret;

	ret = snprintf(ring_name, sizeof(ring_name), "sfc_%u_repr_%u_%sq%u",
		       pf_port_id, repr_id, type_name, qid);
	if (ret >= (int)sizeof(ring_name))
		return -ENAMETOOLONG;

	/*
	 * Single producer/consumer rings are used since the API for Tx/Rx
	 * packet burst for representors are guaranteed to be called from
	 * a single thread, and the user of the other end (representor proxy)
	 * is also single-threaded.
	 */
	*ring = rte_ring_create(ring_name, nb_desc, socket_id,
			       RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (*ring == NULL)
		return -rte_errno;

	return 0;
}

static int
sfc_repr_rx_qcheck_conf(struct sfc_repr *sr,
			const struct rte_eth_rxconf *rx_conf)
{
	int ret = 0;

	sfcr_info(sr, "entry");

	if (rx_conf->rx_thresh.pthresh != 0 ||
	    rx_conf->rx_thresh.hthresh != 0 ||
	    rx_conf->rx_thresh.wthresh != 0) {
		sfcr_warn(sr,
			"RxQ prefetch/host/writeback thresholds are not supported");
	}

	if (rx_conf->rx_free_thresh != 0)
		sfcr_warn(sr, "RxQ free threshold is not supported");

	if (rx_conf->rx_drop_en == 0)
		sfcr_warn(sr, "RxQ drop disable is not supported");

	if (rx_conf->rx_deferred_start) {
		sfcr_err(sr, "Deferred start is not supported");
		ret = -EINVAL;
	}

	sfcr_info(sr, "done: %s", rte_strerror(-ret));

	return ret;
}

static int
sfc_repr_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			uint16_t nb_rx_desc, unsigned int socket_id,
			__rte_unused const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mb_pool)
{
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	struct sfc_repr *sr = sfc_repr_by_eth_dev(dev);
	struct sfc_repr_rxq *rxq;
	int ret;

	sfcr_info(sr, "entry");

	ret = sfc_repr_rx_qcheck_conf(sr, rx_conf);
	if (ret != 0)
		goto fail_check_conf;

	ret = -ENOMEM;
	rxq = rte_zmalloc_socket("sfc-repr-rxq", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL) {
		sfcr_err(sr, "%s() failed to alloc RxQ", __func__);
		goto fail_rxq_alloc;
	}

	ret = sfc_repr_ring_create(srs->pf_port_id, srs->repr_id,
				   "rx", rx_queue_id, nb_rx_desc,
				   socket_id, &rxq->ring);
	if (ret != 0) {
		sfcr_err(sr, "%s() failed to create ring", __func__);
		goto fail_ring_create;
	}

	ret = sfc_repr_proxy_add_rxq(srs->pf_port_id, srs->repr_id,
				     rx_queue_id, rxq->ring, mb_pool);
	if (ret != 0) {
		SFC_ASSERT(ret > 0);
		ret = -ret;
		sfcr_err(sr, "%s() failed to add proxy RxQ", __func__);
		goto fail_proxy_add_rxq;
	}

	dev->data->rx_queues[rx_queue_id] = rxq;

	sfcr_info(sr, "done");

	return 0;

fail_proxy_add_rxq:
	rte_ring_free(rxq->ring);

fail_ring_create:
	rte_free(rxq);

fail_rxq_alloc:
fail_check_conf:
	sfcr_err(sr, "%s() failed: %s", __func__, rte_strerror(-ret));
	return ret;
}

static void
sfc_repr_rx_queue_release(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	struct sfc_repr_rxq *rxq = dev->data->rx_queues[rx_queue_id];

	sfc_repr_proxy_del_rxq(srs->pf_port_id, srs->repr_id, rx_queue_id);
	rte_ring_free(rxq->ring);
	rte_free(rxq);
}

static int
sfc_repr_tx_qcheck_conf(struct sfc_repr *sr,
			const struct rte_eth_txconf *tx_conf)
{
	int ret = 0;

	sfcr_info(sr, "entry");

	if (tx_conf->tx_rs_thresh != 0)
		sfcr_warn(sr, "RS bit in transmit descriptor is not supported");

	if (tx_conf->tx_free_thresh != 0)
		sfcr_warn(sr, "TxQ free threshold is not supported");

	if (tx_conf->tx_thresh.pthresh != 0 ||
	    tx_conf->tx_thresh.hthresh != 0 ||
	    tx_conf->tx_thresh.wthresh != 0) {
		sfcr_warn(sr,
			"prefetch/host/writeback thresholds are not supported");
	}

	if (tx_conf->tx_deferred_start) {
		sfcr_err(sr, "Deferred start is not supported");
		ret = -EINVAL;
	}

	sfcr_info(sr, "done: %s", rte_strerror(-ret));

	return ret;
}

static int
sfc_repr_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			uint16_t nb_tx_desc, unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf)
{
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	struct sfc_repr *sr = sfc_repr_by_eth_dev(dev);
	struct sfc_repr_txq *txq;
	int ret;

	sfcr_info(sr, "entry");

	ret = sfc_repr_tx_qcheck_conf(sr, tx_conf);
	if (ret != 0)
		goto fail_check_conf;

	ret = -ENOMEM;
	txq = rte_zmalloc_socket("sfc-repr-txq", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	ret = sfc_repr_ring_create(srs->pf_port_id, srs->repr_id,
				   "tx", tx_queue_id, nb_tx_desc,
				   socket_id, &txq->ring);
	if (ret != 0)
		goto fail_ring_create;

	ret = sfc_repr_proxy_add_txq(srs->pf_port_id, srs->repr_id,
				     tx_queue_id, txq->ring,
				     &txq->egress_mport);
	if (ret != 0)
		goto fail_proxy_add_txq;

	dev->data->tx_queues[tx_queue_id] = txq;

	sfcr_info(sr, "done");

	return 0;

fail_proxy_add_txq:
	rte_ring_free(txq->ring);

fail_ring_create:
	rte_free(txq);

fail_txq_alloc:
fail_check_conf:
	sfcr_err(sr, "%s() failed: %s", __func__, rte_strerror(-ret));
	return ret;
}

static void
sfc_repr_tx_queue_release(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	struct sfc_repr_txq *txq = dev->data->tx_queues[tx_queue_id];

	sfc_repr_proxy_del_txq(srs->pf_port_id, srs->repr_id, tx_queue_id);
	rte_ring_free(txq->ring);
	rte_free(txq);
}

static void
sfc_repr_close(struct sfc_repr *sr)
{
	SFC_ASSERT(sfc_repr_lock_is_locked(sr));

	SFC_ASSERT(sr->state == SFC_ETHDEV_CONFIGURED);
	sr->state = SFC_ETHDEV_CLOSING;

	/* Put representor close actions here */

	sr->state = SFC_ETHDEV_INITIALIZED;
}

static int
sfc_repr_dev_close(struct rte_eth_dev *dev)
{
	struct sfc_repr *sr = sfc_repr_by_eth_dev(dev);
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	unsigned int i;

	sfcr_info(sr, "entry");

	sfc_repr_lock(sr);
	switch (sr->state) {
	case SFC_ETHDEV_CONFIGURED:
		sfc_repr_close(sr);
		SFC_ASSERT(sr->state == SFC_ETHDEV_INITIALIZED);
		/* FALLTHROUGH */
	case SFC_ETHDEV_INITIALIZED:
		break;
	default:
		sfcr_err(sr, "unexpected adapter state %u on close", sr->state);
		break;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		sfc_repr_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		sfc_repr_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}

	/*
	 * Cleanup all resources.
	 * Rollback primary process sfc_repr_eth_dev_init() below.
	 */

	(void)sfc_repr_proxy_del_port(srs->pf_port_id, srs->repr_id);

	dev->dev_ops = NULL;

	sfc_repr_unlock(sr);
	sfc_repr_lock_fini(sr);

	sfcr_info(sr, "done");

	free(sr);

	return 0;
}

static const struct eth_dev_ops sfc_repr_dev_ops = {
	.dev_configure			= sfc_repr_dev_configure,
	.dev_close			= sfc_repr_dev_close,
	.dev_infos_get			= sfc_repr_dev_infos_get,
	.rx_queue_setup			= sfc_repr_rx_queue_setup,
	.rx_queue_release		= sfc_repr_rx_queue_release,
	.tx_queue_setup			= sfc_repr_tx_queue_setup,
	.tx_queue_release		= sfc_repr_tx_queue_release,
};


struct sfc_repr_init_data {
	uint16_t		pf_port_id;
	uint16_t		repr_id;
	uint16_t		switch_domain_id;
	efx_mport_sel_t		mport_sel;
};

static int
sfc_repr_assign_mae_switch_port(uint16_t switch_domain_id,
				const struct sfc_mae_switch_port_request *req,
				uint16_t *switch_port_id)
{
	int rc;

	rc = sfc_mae_assign_switch_port(switch_domain_id, req, switch_port_id);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_repr_eth_dev_init(struct rte_eth_dev *dev, void *init_params)
{
	const struct sfc_repr_init_data *repr_data = init_params;
	struct sfc_repr_shared *srs = sfc_repr_shared_by_eth_dev(dev);
	struct sfc_mae_switch_port_request switch_port_request;
	efx_mport_sel_t ethdev_mport_sel;
	struct sfc_repr *sr;
	int ret;

	/*
	 * Currently there is no mport we can use for representor's
	 * ethdev. Use an invalid one for now. This way representors
	 * can be instantiated.
	 */
	efx_mae_mport_invalid(&ethdev_mport_sel);

	memset(&switch_port_request, 0, sizeof(switch_port_request));
	switch_port_request.type = SFC_MAE_SWITCH_PORT_REPRESENTOR;
	switch_port_request.ethdev_mportp = &ethdev_mport_sel;
	switch_port_request.entity_mportp = &repr_data->mport_sel;
	switch_port_request.ethdev_port_id = dev->data->port_id;

	ret = sfc_repr_assign_mae_switch_port(repr_data->switch_domain_id,
					      &switch_port_request,
					      &srs->switch_port_id);
	if (ret != 0) {
		SFC_GENERIC_LOG(ERR,
			"%s() failed to assign MAE switch port (domain id %u)",
			__func__, repr_data->switch_domain_id);
		goto fail_mae_assign_switch_port;
	}

	ret = sfc_repr_proxy_add_port(repr_data->pf_port_id,
				      repr_data->repr_id,
				      dev->data->port_id,
				      &repr_data->mport_sel);
	if (ret != 0) {
		SFC_GENERIC_LOG(ERR, "%s() failed to add repr proxy port",
				__func__);
		SFC_ASSERT(ret > 0);
		ret = -ret;
		goto fail_create_port;
	}

	/*
	 * Allocate process private data from heap, since it should not
	 * be located in shared memory allocated using rte_malloc() API.
	 */
	sr = calloc(1, sizeof(*sr));
	if (sr == NULL) {
		ret = -ENOMEM;
		goto fail_alloc_sr;
	}

	sfc_repr_lock_init(sr);
	sfc_repr_lock(sr);

	dev->process_private = sr;

	srs->pf_port_id = repr_data->pf_port_id;
	srs->repr_id = repr_data->repr_id;
	srs->switch_domain_id = repr_data->switch_domain_id;

	dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	dev->data->representor_id = srs->repr_id;
	dev->data->backer_port_id = srs->pf_port_id;

	dev->data->mac_addrs = rte_zmalloc("sfcr", RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		ret = -ENOMEM;
		goto fail_mac_addrs;
	}

	dev->dev_ops = &sfc_repr_dev_ops;

	sr->state = SFC_ETHDEV_INITIALIZED;
	sfc_repr_unlock(sr);

	return 0;

fail_mac_addrs:
	sfc_repr_unlock(sr);
	free(sr);

fail_alloc_sr:
	(void)sfc_repr_proxy_del_port(repr_data->pf_port_id,
				      repr_data->repr_id);

fail_create_port:
fail_mae_assign_switch_port:
	SFC_GENERIC_LOG(ERR, "%s() failed: %s", __func__, rte_strerror(-ret));
	return ret;
}

int
sfc_repr_create(struct rte_eth_dev *parent, uint16_t representor_id,
		uint16_t switch_domain_id, const efx_mport_sel_t *mport_sel)
{
	struct sfc_repr_init_data repr_data;
	char name[RTE_ETH_NAME_MAX_LEN];
	int ret;

	if (snprintf(name, sizeof(name), "net_%s_representor_%u",
		     parent->device->name, representor_id) >=
			(int)sizeof(name)) {
		SFC_GENERIC_LOG(ERR, "%s() failed name too long", __func__);
		return -ENAMETOOLONG;
	}

	memset(&repr_data, 0, sizeof(repr_data));
	repr_data.pf_port_id = parent->data->port_id;
	repr_data.repr_id = representor_id;
	repr_data.switch_domain_id = switch_domain_id;
	repr_data.mport_sel = *mport_sel;

	ret = rte_eth_dev_create(parent->device, name,
				  sizeof(struct sfc_repr_shared),
				  NULL, NULL,
				  sfc_repr_eth_dev_init, &repr_data);
	if (ret != 0)
		SFC_GENERIC_LOG(ERR, "%s() failed to create device", __func__);

	SFC_GENERIC_LOG(INFO, "%s() done: %s", __func__, rte_strerror(-ret));

	return ret;
}
