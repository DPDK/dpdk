/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_EVDEV_H__
#define __OTX2_EVDEV_H__

#include <rte_eventdev.h>
#include <eventdev_pmd.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_ethdev.h"
#include "otx2_mempool.h"
#include "otx2_tim_evdev.h"

#define EVENTDEV_NAME_OCTEONTX2_PMD event_octeontx2

#define sso_func_trace otx2_sso_dbg

#define OTX2_SSO_MAX_VHGRP                  RTE_EVENT_MAX_QUEUES_PER_DEV
#define OTX2_SSO_MAX_VHWS                   (UINT8_MAX)
#define OTX2_SSO_FC_NAME                    "otx2_evdev_xaq_fc"
#define OTX2_SSO_SQB_LIMIT                  (0x180)
#define OTX2_SSO_XAQ_SLACK                  (8)
#define OTX2_SSO_XAQ_CACHE_CNT              (0x7)
#define OTX2_SSO_WQE_SG_PTR                 (9)

/* SSO LF register offsets (BAR2) */
#define SSO_LF_GGRP_OP_ADD_WORK0            (0x0ull)
#define SSO_LF_GGRP_OP_ADD_WORK1            (0x8ull)

#define SSO_LF_GGRP_QCTL                    (0x20ull)
#define SSO_LF_GGRP_EXE_DIS                 (0x80ull)
#define SSO_LF_GGRP_INT                     (0x100ull)
#define SSO_LF_GGRP_INT_W1S                 (0x108ull)
#define SSO_LF_GGRP_INT_ENA_W1S             (0x110ull)
#define SSO_LF_GGRP_INT_ENA_W1C             (0x118ull)
#define SSO_LF_GGRP_INT_THR                 (0x140ull)
#define SSO_LF_GGRP_INT_CNT                 (0x180ull)
#define SSO_LF_GGRP_XAQ_CNT                 (0x1b0ull)
#define SSO_LF_GGRP_AQ_CNT                  (0x1c0ull)
#define SSO_LF_GGRP_AQ_THR                  (0x1e0ull)
#define SSO_LF_GGRP_MISC_CNT                (0x200ull)

/* SSOW LF register offsets (BAR2) */
#define SSOW_LF_GWS_LINKS                   (0x10ull)
#define SSOW_LF_GWS_PENDWQP                 (0x40ull)
#define SSOW_LF_GWS_PENDSTATE               (0x50ull)
#define SSOW_LF_GWS_NW_TIM                  (0x70ull)
#define SSOW_LF_GWS_GRPMSK_CHG              (0x80ull)
#define SSOW_LF_GWS_INT                     (0x100ull)
#define SSOW_LF_GWS_INT_W1S                 (0x108ull)
#define SSOW_LF_GWS_INT_ENA_W1S             (0x110ull)
#define SSOW_LF_GWS_INT_ENA_W1C             (0x118ull)
#define SSOW_LF_GWS_TAG                     (0x200ull)
#define SSOW_LF_GWS_WQP                     (0x210ull)
#define SSOW_LF_GWS_SWTP                    (0x220ull)
#define SSOW_LF_GWS_PENDTAG                 (0x230ull)
#define SSOW_LF_GWS_OP_ALLOC_WE             (0x400ull)
#define SSOW_LF_GWS_OP_GET_WORK             (0x600ull)
#define SSOW_LF_GWS_OP_SWTAG_FLUSH          (0x800ull)
#define SSOW_LF_GWS_OP_SWTAG_UNTAG          (0x810ull)
#define SSOW_LF_GWS_OP_SWTP_CLR             (0x820ull)
#define SSOW_LF_GWS_OP_UPD_WQP_GRP0         (0x830ull)
#define SSOW_LF_GWS_OP_UPD_WQP_GRP1         (0x838ull)
#define SSOW_LF_GWS_OP_DESCHED              (0x880ull)
#define SSOW_LF_GWS_OP_DESCHED_NOSCH        (0x8c0ull)
#define SSOW_LF_GWS_OP_SWTAG_DESCHED        (0x980ull)
#define SSOW_LF_GWS_OP_SWTAG_NOSCHED        (0x9c0ull)
#define SSOW_LF_GWS_OP_CLR_NSCHED0          (0xa00ull)
#define SSOW_LF_GWS_OP_CLR_NSCHED1          (0xa08ull)
#define SSOW_LF_GWS_OP_SWTP_SET             (0xc00ull)
#define SSOW_LF_GWS_OP_SWTAG_NORM           (0xc10ull)
#define SSOW_LF_GWS_OP_SWTAG_FULL0          (0xc20ull)
#define SSOW_LF_GWS_OP_SWTAG_FULL1          (0xc28ull)
#define SSOW_LF_GWS_OP_GWC_INVAL            (0xe00ull)

#define OTX2_SSOW_GET_BASE_ADDR(_GW)        ((_GW) - SSOW_LF_GWS_OP_GET_WORK)
#define OTX2_SSOW_TT_FROM_TAG(x)	    (((x) >> 32) & SSO_TT_EMPTY)
#define OTX2_SSOW_GRP_FROM_TAG(x)	    (((x) >> 36) & 0x3ff)

#define NSEC2USEC(__ns)			((__ns) / 1E3)
#define USEC2NSEC(__us)                 ((__us) * 1E3)
#define NSEC2TICK(__ns, __freq)		(((__ns) * (__freq)) / 1E9)
#define TICK2NSEC(__tck, __freq)	(((__tck) * 1E9) / (__freq))

enum otx2_sso_lf_type {
	SSO_LF_GGRP,
	SSO_LF_GWS
};

union otx2_sso_event {
	uint64_t get_work0;
	struct {
		uint32_t flow_id:20;
		uint32_t sub_event_type:8;
		uint32_t event_type:4;
		uint8_t op:2;
		uint8_t rsvd:4;
		uint8_t sched_type:2;
		uint8_t queue_id;
		uint8_t priority;
		uint8_t impl_opaque;
	};
} __rte_aligned(64);

enum {
	SSO_SYNC_ORDERED,
	SSO_SYNC_ATOMIC,
	SSO_SYNC_UNTAGGED,
	SSO_SYNC_EMPTY
};

struct otx2_sso_qos {
	uint8_t queue;
	uint8_t xaq_prcnt;
	uint8_t taq_prcnt;
	uint8_t iaq_prcnt;
};

struct otx2_sso_evdev {
	OTX2_DEV; /* Base class */
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
	uint64_t *fc_mem;
	uint64_t xaq_lmt;
	uint64_t nb_xaq_cfg;
	rte_iova_t fc_iova;
	struct rte_mempool *xaq_pool;
	uint64_t rx_offloads;
	uint64_t tx_offloads;
	uint64_t adptr_xae_cnt;
	uint16_t rx_adptr_pool_cnt;
	uint64_t *rx_adptr_pools;
	uint16_t max_port_id;
	uint16_t tim_adptr_ring_cnt;
	uint16_t *timer_adptr_rings;
	uint64_t *timer_adptr_sz;
	/* Dev args */
	uint8_t dual_ws;
	uint32_t xae_cnt;
	uint8_t qos_queue_cnt;
	uint8_t force_rx_bp;
	struct otx2_sso_qos *qos_parse_data;
	/* HW const */
	uint32_t xae_waes;
	uint32_t xaq_buf_size;
	uint32_t iue;
	/* MSIX offsets */
	uint16_t sso_msixoff[OTX2_SSO_MAX_VHGRP];
	uint16_t ssow_msixoff[OTX2_SSO_MAX_VHWS];
	/* PTP timestamp */
	struct otx2_timesync_info *tstamp;
} __rte_cache_aligned;

#define OTX2_SSOGWS_OPS                                                        \
	/* WS ops */                                                           \
	uintptr_t getwrk_op;                                                   \
	uintptr_t tag_op;                                                      \
	uintptr_t wqp_op;                                                      \
	uintptr_t swtag_flush_op;                                              \
	uintptr_t swtag_norm_op;                                               \
	uintptr_t swtag_desched_op;

/* Event port aka GWS */
struct otx2_ssogws {
	/* Get Work Fastpath data */
	OTX2_SSOGWS_OPS;
	/* PTP timestamp */
	struct otx2_timesync_info *tstamp;
	void *lookup_mem;
	uint8_t swtag_req;
	uint8_t port;
	/* Add Work Fastpath data */
	uint64_t xaq_lmt __rte_cache_aligned;
	uint64_t *fc_mem;
	uintptr_t grps_base[OTX2_SSO_MAX_VHGRP];
	/* Tx Fastpath data */
	uint64_t base __rte_cache_aligned;
	uint8_t tx_adptr_data[];
} __rte_cache_aligned;

struct otx2_ssogws_state {
	OTX2_SSOGWS_OPS;
};

struct otx2_ssogws_dual {
	/* Get Work Fastpath data */
	struct otx2_ssogws_state ws_state[2]; /* Ping and Pong */
	/* PTP timestamp */
	struct otx2_timesync_info *tstamp;
	void *lookup_mem;
	uint8_t swtag_req;
	uint8_t vws; /* Ping pong bit */
	uint8_t port;
	/* Add Work Fastpath data */
	uint64_t xaq_lmt __rte_cache_aligned;
	uint64_t *fc_mem;
	uintptr_t grps_base[OTX2_SSO_MAX_VHGRP];
	/* Tx Fastpath data */
	uint64_t base[2] __rte_cache_aligned;
	uint8_t tx_adptr_data[];
} __rte_cache_aligned;

static inline struct otx2_sso_evdev *
sso_pmd_priv(const struct rte_eventdev *event_dev)
{
	return event_dev->data->dev_private;
}

struct otx2_ssogws_cookie {
	const struct rte_eventdev *event_dev;
	bool configured;
};

static inline struct otx2_ssogws_cookie *
ssogws_get_cookie(void *ws)
{
	return (struct otx2_ssogws_cookie *)
		((uint8_t *)ws - RTE_CACHE_LINE_SIZE);
}

static const union mbuf_initializer mbuf_init = {
	.fields = {
		.data_off = RTE_PKTMBUF_HEADROOM,
		.refcnt = 1,
		.nb_segs = 1,
		.port = 0
	}
};

static __rte_always_inline void
otx2_wqe_to_mbuf(uint64_t get_work1, const uint64_t mbuf, uint8_t port_id,
		 const uint32_t tag, const uint32_t flags,
		 const void * const lookup_mem)
{
	struct nix_wqe_hdr_s *wqe = (struct nix_wqe_hdr_s *)get_work1;
	uint64_t val = mbuf_init.value | (uint64_t)port_id << 48;

	if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
		val |= NIX_TIMESYNC_RX_OFFSET;

	otx2_nix_cqe_to_mbuf((struct nix_cqe_hdr_s *)wqe, tag,
			     (struct rte_mbuf *)mbuf, lookup_mem,
			      val, flags);

}

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

#define SSO_RX_ADPTR_ENQ_FASTPATH_FUNC	NIX_RX_FASTPATH_MODES
#define SSO_TX_ADPTR_ENQ_FASTPATH_FUNC	NIX_TX_FASTPATH_MODES

/* Single WS API's */
uint16_t otx2_ssogws_enq(void *port, const struct rte_event *ev);
uint16_t otx2_ssogws_enq_burst(void *port, const struct rte_event ev[],
			       uint16_t nb_events);
uint16_t otx2_ssogws_enq_new_burst(void *port, const struct rte_event ev[],
				   uint16_t nb_events);
uint16_t otx2_ssogws_enq_fwd_burst(void *port, const struct rte_event ev[],
				   uint16_t nb_events);

/* Dual WS API's */
uint16_t otx2_ssogws_dual_enq(void *port, const struct rte_event *ev);
uint16_t otx2_ssogws_dual_enq_burst(void *port, const struct rte_event ev[],
				    uint16_t nb_events);
uint16_t otx2_ssogws_dual_enq_new_burst(void *port, const struct rte_event ev[],
					uint16_t nb_events);
uint16_t otx2_ssogws_dual_enq_fwd_burst(void *port, const struct rte_event ev[],
					uint16_t nb_events);

/* Auto generated API's */
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
uint16_t otx2_ssogws_deq_ ##name(void *port, struct rte_event *ev,	       \
				 uint64_t timeout_ticks);		       \
uint16_t otx2_ssogws_deq_burst_ ##name(void *port, struct rte_event ev[],      \
				       uint16_t nb_events,		       \
				       uint64_t timeout_ticks);		       \
uint16_t otx2_ssogws_deq_timeout_ ##name(void *port,			       \
					 struct rte_event *ev,		       \
					 uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_deq_timeout_burst_ ##name(void *port,		       \
					       struct rte_event ev[],	       \
					       uint16_t nb_events,	       \
					       uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_deq_seg_ ##name(void *port, struct rte_event *ev,	       \
				     uint64_t timeout_ticks);		       \
uint16_t otx2_ssogws_deq_seg_burst_ ##name(void *port,			       \
					   struct rte_event ev[],	       \
					   uint16_t nb_events,		       \
					   uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_deq_seg_timeout_ ##name(void *port,		       \
					     struct rte_event *ev,	       \
					     uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_deq_seg_timeout_burst_ ##name(void *port,		       \
						   struct rte_event ev[],      \
						   uint16_t nb_events,	       \
						   uint64_t timeout_ticks);    \
									       \
uint16_t otx2_ssogws_dual_deq_ ##name(void *port, struct rte_event *ev,	       \
				      uint64_t timeout_ticks);		       \
uint16_t otx2_ssogws_dual_deq_burst_ ##name(void *port,			       \
					    struct rte_event ev[],	       \
					    uint16_t nb_events,		       \
					    uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_dual_deq_timeout_ ##name(void *port,		       \
					      struct rte_event *ev,	       \
					      uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_dual_deq_timeout_burst_ ##name(void *port,		       \
						    struct rte_event ev[],     \
						    uint16_t nb_events,	       \
						    uint64_t timeout_ticks);   \
uint16_t otx2_ssogws_dual_deq_seg_ ##name(void *port, struct rte_event *ev,    \
					  uint64_t timeout_ticks);	       \
uint16_t otx2_ssogws_dual_deq_seg_burst_ ##name(void *port,		       \
						struct rte_event ev[],	       \
						uint16_t nb_events,	       \
						uint64_t timeout_ticks);       \
uint16_t otx2_ssogws_dual_deq_seg_timeout_ ##name(void *port,		       \
						  struct rte_event *ev,	       \
						  uint64_t timeout_ticks);     \
uint16_t otx2_ssogws_dual_deq_seg_timeout_burst_ ##name(void *port,	       \
							struct rte_event ev[], \
							uint16_t nb_events,    \
						       uint64_t timeout_ticks);\

SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			     \
uint16_t otx2_ssogws_tx_adptr_enq_ ## name(void *port, struct rte_event ev[],\
					   uint16_t nb_events);		     \
uint16_t otx2_ssogws_tx_adptr_enq_seg_ ## name(void *port,		     \
					       struct rte_event ev[],	     \
					       uint16_t nb_events);	     \
uint16_t otx2_ssogws_dual_tx_adptr_enq_ ## name(void *port,		     \
						struct rte_event ev[],	     \
						uint16_t nb_events);	     \
uint16_t otx2_ssogws_dual_tx_adptr_enq_seg_ ## name(void *port,		     \
						    struct rte_event ev[],   \
						    uint16_t nb_events);     \

SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T

void sso_updt_xae_cnt(struct otx2_sso_evdev *dev, void *data,
		      uint32_t event_type);
int sso_xae_reconfigure(struct rte_eventdev *event_dev);
void sso_fastpath_fns_set(struct rte_eventdev *event_dev);

int otx2_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
				 const struct rte_eth_dev *eth_dev,
				 uint32_t *caps);
int otx2_sso_rx_adapter_queue_add(const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf);
int otx2_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t rx_queue_id);
int otx2_sso_rx_adapter_start(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev);
int otx2_sso_rx_adapter_stop(const struct rte_eventdev *event_dev,
			     const struct rte_eth_dev *eth_dev);
int otx2_sso_tx_adapter_caps_get(const struct rte_eventdev *dev,
				 const struct rte_eth_dev *eth_dev,
				 uint32_t *caps);
int otx2_sso_tx_adapter_queue_add(uint8_t id,
				  const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t tx_queue_id);

int otx2_sso_tx_adapter_queue_del(uint8_t id,
				  const struct rte_eventdev *event_dev,
				  const struct rte_eth_dev *eth_dev,
				  int32_t tx_queue_id);

/* Event crypto adapter API's */
int otx2_ca_caps_get(const struct rte_eventdev *dev,
		     const struct rte_cryptodev *cdev, uint32_t *caps);

int otx2_ca_qp_add(const struct rte_eventdev *dev,
		   const struct rte_cryptodev *cdev, int32_t queue_pair_id,
		   const struct rte_event *event);

int otx2_ca_qp_del(const struct rte_eventdev *dev,
		   const struct rte_cryptodev *cdev, int32_t queue_pair_id);

/* Clean up API's */
typedef void (*otx2_handle_event_t)(void *arg, struct rte_event ev);
void ssogws_flush_events(struct otx2_ssogws *ws, uint8_t queue_id,
			 uintptr_t base, otx2_handle_event_t fn, void *arg);
void ssogws_reset(struct otx2_ssogws *ws);
/* Selftest */
int otx2_sso_selftest(void);
/* Init and Fini API's */
int otx2_sso_init(struct rte_eventdev *event_dev);
int otx2_sso_fini(struct rte_eventdev *event_dev);
/* IRQ handlers */
int sso_register_irqs(const struct rte_eventdev *event_dev);
void sso_unregister_irqs(const struct rte_eventdev *event_dev);

#endif /* __OTX2_EVDEV_H__ */
