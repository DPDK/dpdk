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
