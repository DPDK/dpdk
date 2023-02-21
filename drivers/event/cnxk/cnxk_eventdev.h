/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_EVENTDEV_H__
#define __CNXK_EVENTDEV_H__

#include <string.h>

#include <cryptodev_pmd.h>
#include <rte_devargs.h>
#include <rte_ethdev.h>
#include <rte_event_crypto_adapter.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_kvargs.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include <eventdev_pmd_pci.h>

#include "cnxk_eventdev_dp.h"

#include "roc_platform.h"
#include "roc_sso.h"

#include "cnxk_tim_evdev.h"

#define CNXK_SSO_XAE_CNT   "xae_cnt"
#define CNXK_SSO_GGRP_QOS  "qos"
#define CNXK_SSO_FORCE_BP  "force_rx_bp"
#define CN9K_SSO_SINGLE_WS "single_ws"
#define CN10K_SSO_GW_MODE  "gw_mode"
#define CN10K_SSO_STASH	   "stash"

#define NSEC2USEC(__ns)		((__ns) / 1E3)
#define USEC2NSEC(__us)		((__us)*1E3)
#define NSEC2TICK(__ns, __freq) (((__ns) * (__freq)) / 1E9)

#define CN9K_SSOW_GET_BASE_ADDR(_GW) ((_GW)-SSOW_LF_GWS_OP_GET_WORK0)

#define CN10K_GW_MODE_NONE     0
#define CN10K_GW_MODE_PREF     1
#define CN10K_GW_MODE_PREF_WFE 2

#define CNXK_QOS_NORMALIZE(val, min, max, cnt)                                 \
	(min + val / ((max + cnt - 1) / cnt))
#define CNXK_SSO_FLUSH_RETRY_MAX 0xfff

#define CNXK_VALID_DEV_OR_ERR_RET(dev, drv_name)                               \
	do {                                                                   \
		if (strncmp(dev->driver->name, drv_name, strlen(drv_name)))    \
			return -EINVAL;                                        \
	} while (0)

typedef void *(*cnxk_sso_init_hws_mem_t)(void *dev, uint8_t port_id);
typedef void (*cnxk_sso_hws_setup_t)(void *dev, void *ws, uintptr_t grp_base);
typedef void (*cnxk_sso_hws_release_t)(void *dev, void *ws);
typedef int (*cnxk_sso_link_t)(void *dev, void *ws, uint16_t *map,
			       uint16_t nb_link);
typedef int (*cnxk_sso_unlink_t)(void *dev, void *ws, uint16_t *map,
				 uint16_t nb_link);
typedef void (*cnxk_handle_event_t)(void *arg, struct rte_event ev);
typedef void (*cnxk_sso_hws_reset_t)(void *arg, void *ws);
typedef int (*cnxk_sso_hws_flush_t)(void *ws, uint8_t queue_id, uintptr_t base,
				    cnxk_handle_event_t fn, void *arg);

struct cnxk_sso_qos {
	uint16_t queue;
	uint16_t taq_prcnt;
	uint16_t iaq_prcnt;
};

struct cnxk_sso_stash {
	uint16_t queue;
	uint16_t stash_offset;
	uint16_t stash_length;
};

struct cnxk_sso_evdev {
	struct roc_sso sso;
	uint8_t max_event_queues;
	uint8_t max_event_ports;
	uint8_t is_timeout_deq;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint8_t configured;
	uint32_t deq_tmo_ns;
	uint32_t min_dequeue_timeout_ns;
	uint32_t max_dequeue_timeout_ns;
	int32_t max_num_events;
	uint64_t xaq_lmt;
	rte_iova_t fc_iova;
	uint64_t rx_offloads;
	uint64_t tx_offloads;
	uint64_t adptr_xae_cnt;
	uint16_t rx_adptr_pool_cnt;
	uint64_t *rx_adptr_pools;
	uint64_t *tx_adptr_data;
	size_t tx_adptr_data_sz;
	uint16_t max_port_id;
	uint16_t max_queue_id[RTE_MAX_ETHPORTS];
	uint8_t tx_adptr_configured;
	uint32_t tx_adptr_active_mask;
	uint16_t tim_adptr_ring_cnt;
	uint16_t *timer_adptr_rings;
	uint64_t *timer_adptr_sz;
	uint16_t vec_pool_cnt;
	uint64_t *vec_pools;
	struct cnxk_timesync_info *tstamp[RTE_MAX_ETHPORTS];
	/* Dev args */
	uint32_t xae_cnt;
	uint16_t qos_queue_cnt;
	struct cnxk_sso_qos *qos_parse_data;
	uint8_t force_ena_bp;
	/* CN9K */
	uint8_t dual_ws;
	/* CN10K */
	uint8_t gw_mode;
	uint16_t stash_cnt;
	struct cnxk_sso_stash *stash_parse_data;
	/* Crypto adapter */
	uint8_t is_ca_internal_port;
} __rte_cache_aligned;

/* Event port a.k.a GWS */
struct cn9k_sso_hws {
	uint64_t base;
	uint64_t gw_wdata;
	void *lookup_mem;
	uint8_t swtag_req;
	uint8_t hws_id;
	/* PTP timestamp */
	struct cnxk_timesync_info **tstamp;
	/* Add Work Fastpath data */
	uint64_t xaq_lmt __rte_cache_aligned;
	uint64_t *fc_mem;
	uintptr_t grp_base;
	/* Tx Fastpath data */
	uint64_t lso_tun_fmt __rte_cache_aligned;
	uint8_t tx_adptr_data[];
} __rte_cache_aligned;

struct cn9k_sso_hws_dual {
	uint64_t base[2]; /* Ping and Pong */
	uint64_t gw_wdata;
	void *lookup_mem;
	uint8_t swtag_req;
	uint8_t vws; /* Ping pong bit */
	uint8_t hws_id;
	/* PTP timestamp */
	struct cnxk_timesync_info **tstamp;
	/* Add Work Fastpath data */
	uint64_t xaq_lmt __rte_cache_aligned;
	uint64_t *fc_mem;
	uintptr_t grp_base;
	/* Tx Fastpath data */
	uint64_t lso_tun_fmt __rte_cache_aligned;
	uint8_t tx_adptr_data[];
} __rte_cache_aligned;

struct cnxk_sso_hws_cookie {
	const struct rte_eventdev *event_dev;
	bool configured;
} __rte_cache_aligned;

static inline int
parse_kvargs_flag(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	*(uint8_t *)opaque = !!atoi(value);
	return 0;
}

static inline int
parse_kvargs_value(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	*(uint32_t *)opaque = (uint32_t)atoi(value);
	return 0;
}

static inline struct cnxk_sso_evdev *
cnxk_sso_pmd_priv(const struct rte_eventdev *event_dev)
{
	return event_dev->data->dev_private;
}

static inline struct cnxk_sso_hws_cookie *
cnxk_sso_hws_get_cookie(void *ws)
{
	return RTE_PTR_SUB(ws, sizeof(struct cnxk_sso_hws_cookie));
}

/* Configuration functions */
int cnxk_sso_xae_reconfigure(struct rte_eventdev *event_dev);
int cnxk_sso_xaq_allocate(struct cnxk_sso_evdev *dev);
void cnxk_sso_updt_xae_cnt(struct cnxk_sso_evdev *dev, void *data,
			   uint32_t event_type);

/* Common ops API. */
int cnxk_sso_init(struct rte_eventdev *event_dev);
int cnxk_sso_fini(struct rte_eventdev *event_dev);
int cnxk_sso_remove(struct rte_pci_device *pci_dev);
void cnxk_sso_info_get(struct cnxk_sso_evdev *dev,
		       struct rte_event_dev_info *dev_info);
int cnxk_sso_dev_validate(const struct rte_eventdev *event_dev);
int cnxk_setup_event_ports(const struct rte_eventdev *event_dev,
			   cnxk_sso_init_hws_mem_t init_hws_mem,
			   cnxk_sso_hws_setup_t hws_setup);
void cnxk_sso_restore_links(const struct rte_eventdev *event_dev,
			    cnxk_sso_link_t link_fn);
void cnxk_sso_queue_def_conf(struct rte_eventdev *event_dev, uint8_t queue_id,
			     struct rte_event_queue_conf *queue_conf);
int cnxk_sso_queue_setup(struct rte_eventdev *event_dev, uint8_t queue_id,
			 const struct rte_event_queue_conf *queue_conf);
void cnxk_sso_queue_release(struct rte_eventdev *event_dev, uint8_t queue_id);
int cnxk_sso_queue_attribute_set(struct rte_eventdev *event_dev,
				 uint8_t queue_id, uint32_t attr_id,
				 uint64_t attr_value);
void cnxk_sso_port_def_conf(struct rte_eventdev *event_dev, uint8_t port_id,
			    struct rte_event_port_conf *port_conf);
int cnxk_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
			cnxk_sso_hws_setup_t hws_setup_fn);
int cnxk_sso_timeout_ticks(struct rte_eventdev *event_dev, uint64_t ns,
			   uint64_t *tmo_ticks);
int cnxk_sso_start(struct rte_eventdev *event_dev,
		   cnxk_sso_hws_reset_t reset_fn,
		   cnxk_sso_hws_flush_t flush_fn);
void cnxk_sso_stop(struct rte_eventdev *event_dev,
		   cnxk_sso_hws_reset_t reset_fn,
		   cnxk_sso_hws_flush_t flush_fn);
int cnxk_sso_close(struct rte_eventdev *event_dev, cnxk_sso_unlink_t unlink_fn);
int cnxk_sso_selftest(const char *dev_name);
void cnxk_sso_dump(struct rte_eventdev *event_dev, FILE *f);

/* Stats API. */
int cnxk_sso_xstats_get_names(const struct rte_eventdev *event_dev,
			      enum rte_event_dev_xstats_mode mode,
			      uint8_t queue_port_id,
			      struct rte_event_dev_xstats_name *xstats_names,
			      uint64_t *ids, unsigned int size);
int cnxk_sso_xstats_get(const struct rte_eventdev *event_dev,
			enum rte_event_dev_xstats_mode mode,
			uint8_t queue_port_id, const uint64_t ids[],
			uint64_t values[], unsigned int n);
int cnxk_sso_xstats_reset(struct rte_eventdev *event_dev,
			  enum rte_event_dev_xstats_mode mode,
			  int16_t queue_port_id, const uint64_t ids[],
			  uint32_t n);

/* CN9K */
void cn9k_sso_set_rsrc(void *arg);

/* Common adapter ops */
int cnxk_sso_rx_adapter_queue_add(
	const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
	int32_t rx_queue_id,
	const struct rte_event_eth_rx_adapter_queue_conf *queue_conf);
int cnxk_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t rx_queue_id);
int cnxk_sso_rx_adapter_start(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev);
int cnxk_sso_rx_adapter_stop(const struct rte_eventdev *event_dev,
			     const struct rte_eth_dev *eth_dev);
int cnxk_sso_tx_adapter_queue_add(const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t tx_queue_id);
int cnxk_sso_tx_adapter_queue_del(const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t tx_queue_id);
int cnxk_sso_tx_adapter_start(uint8_t id, const struct rte_eventdev *event_dev);
int cnxk_sso_tx_adapter_stop(uint8_t id, const struct rte_eventdev *event_dev);
int cnxk_sso_tx_adapter_free(uint8_t id, const struct rte_eventdev *event_dev);
int cnxk_crypto_adapter_qp_add(const struct rte_eventdev *event_dev,
			       const struct rte_cryptodev *cdev, int32_t queue_pair_id,
			       const struct rte_event_crypto_adapter_queue_conf *conf);
int cnxk_crypto_adapter_qp_del(const struct rte_cryptodev *cdev, int32_t queue_pair_id);

#endif /* __CNXK_EVENTDEV_H__ */
