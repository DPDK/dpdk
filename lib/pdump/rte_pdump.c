/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#include <stdlib.h>

#include <eal_export.h>
#include <rte_alarm.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_pause.h>
#include <rte_pcapng.h>

#include "rte_pdump.h"

RTE_LOG_REGISTER_DEFAULT(pdump_logtype, NOTICE);
#define RTE_LOGTYPE_PDUMP pdump_logtype

#define PDUMP_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, PDUMP, "%s(): ", __func__, __VA_ARGS__)

/* Used for the multi-process communication */
#define PDUMP_MP	"mp_pdump"

#define PDUMP_BURST_SIZE	32

/* Overly generous timeout for secondary to respond */
#define MP_TIMEOUT_S 5

enum pdump_operation {
	DISABLE = 1,
	ENABLE = 2
};

static const char *
pdump_opname(enum pdump_operation op)
{
	static char buf[32];

	if (op == DISABLE)
		return "disable";
	if (op == ENABLE)
		return "enable";

	snprintf(buf, sizeof(buf), "op%u", op);
	return buf;
}

/* Internal version number in request */
enum pdump_version {
	V1 = 1,		    /* no filtering or snap */
	V2 = 2,
};

struct pdump_request {
	uint16_t ver;
	uint16_t op;
	uint32_t flags;
	char device[RTE_DEV_NAME_MAX_LEN];
	uint16_t queue;
	struct rte_ring *ring;
	struct rte_mempool *mp;

	const struct rte_bpf_prm *prm;
	uint32_t snaplen;
};

struct pdump_response {
	uint16_t ver;
	uint16_t res_op;
	int32_t err_value;
};

struct pdump_bundle {
	struct rte_mp_msg msg;
	char peer[];
};

static struct pdump_rxtx_cbs {
	struct rte_ring *ring;
	struct rte_mempool *mp;
	const struct rte_eth_rxtx_callback *cb;
	const struct rte_bpf *filter;
	enum pdump_version ver;
	uint32_t snaplen;
	RTE_ATOMIC(uint32_t) use_count;
} rx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT],
tx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];


/*
 * The packet capture statistics keep track of packets
 * accepted, filtered and dropped. These are per-queue
 * and in memory between primary and secondary processes.
 */
static const char MZ_RTE_PDUMP_STATS[] = "rte_pdump_stats";
static struct {
	struct rte_pdump_stats rx[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];
	struct rte_pdump_stats tx[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];
	const struct rte_memzone *mz;
} *pdump_stats;

static void
pdump_cb_wait(struct pdump_rxtx_cbs *cbs)
{
	/* make sure the data loads happens before the use count load */
	rte_atomic_thread_fence(rte_memory_order_acquire);

	/* wait until use_count is even (not in use) */
	RTE_WAIT_UNTIL_MASKED(&cbs->use_count, 1, ==, 0, rte_memory_order_relaxed);
}

static __rte_always_inline void
pdump_cb_hold(struct pdump_rxtx_cbs *cbs)
{
	uint32_t count = cbs->use_count + 1;

	rte_atomic_store_explicit(&cbs->use_count, count, rte_memory_order_relaxed);

	/* prevent stores after this from happening before the use_count update */
	rte_atomic_thread_fence(rte_memory_order_release);
}

static __rte_always_inline void
pdump_cb_release(struct pdump_rxtx_cbs *cbs)
{
	uint32_t count = cbs->use_count + 1;

	/* Synchronizes-with the load acquire in pdump_cb_wait */
	rte_atomic_store_explicit(&cbs->use_count, count, rte_memory_order_release);
}

/* Create a clone of mbuf to be placed into ring. */
static void
pdump_copy_burst(uint16_t port_id, uint16_t queue_id,
		 enum rte_pcapng_direction direction,
		 struct rte_mbuf **pkts, uint16_t nb_pkts,
		 const struct pdump_rxtx_cbs *cbs,
		 struct rte_pdump_stats *stats)
{
	unsigned int i;
	int ring_enq;
	uint16_t d_pkts = 0;
	struct rte_mbuf *dup_bufs[PDUMP_BURST_SIZE]; /* duplicated packets */
	struct rte_ring *ring;
	struct rte_mempool *mp;
	struct rte_mbuf *p;
	uint64_t rcs[PDUMP_BURST_SIZE];		     /* filter result */

	RTE_ASSERT(nb_pkts <= PDUMP_BURST_SIZE);

	if (cbs->filter)
		rte_bpf_exec_burst(cbs->filter, (void **)pkts, rcs, nb_pkts);

	ring = cbs->ring;
	mp = cbs->mp;
	for (i = 0; i < nb_pkts; i++) {
		/*
		 * This uses same BPF return value convention as socket filter
		 * and pcap_offline_filter.
		 * if program returns zero
		 * then packet doesn't match the filter (will be ignored).
		 */
		if (cbs->filter && rcs[i] == 0) {
			rte_atomic_fetch_add_explicit(&stats->filtered,
					   1, rte_memory_order_relaxed);
			continue;
		}

		/*
		 * If using pcapng then want to wrap packets
		 * otherwise a simple copy.
		 */
		if (cbs->ver == V2)
			p = rte_pcapng_copy(port_id, queue_id, pkts[i], mp, cbs->snaplen,
					    direction, NULL);
		else
			p = rte_pktmbuf_copy(pkts[i], mp, 0, cbs->snaplen);

		if (unlikely(p == NULL))
			rte_atomic_fetch_add_explicit(&stats->nombuf, 1, rte_memory_order_relaxed);
		else
			dup_bufs[d_pkts++] = p;
	}

	if (unlikely(d_pkts == 0))
		return;

	rte_atomic_fetch_add_explicit(&stats->accepted, d_pkts, rte_memory_order_relaxed);

	ring_enq = rte_ring_enqueue_burst(ring, (void *)&dup_bufs[0], d_pkts, NULL);
	if (unlikely(ring_enq < d_pkts)) {
		unsigned int drops = d_pkts - ring_enq;

		rte_atomic_fetch_add_explicit(&stats->ringfull, drops, rte_memory_order_relaxed);
		rte_pktmbuf_free_bulk(&dup_bufs[ring_enq], drops);
	}
}

/* Create a clone of mbuf to be placed into ring. */
static void
pdump_copy(uint16_t port_id, uint16_t queue_id,
	   enum rte_pcapng_direction direction,
	   struct rte_mbuf **pkts, uint16_t nb_pkts,
	   const struct pdump_rxtx_cbs *cbs,
	   struct rte_pdump_stats *stats)
{
	uint16_t offs = 0;

	do {
		uint16_t n = RTE_MIN(nb_pkts - offs, PDUMP_BURST_SIZE);

		pdump_copy_burst(port_id, queue_id, direction, &pkts[offs], n, cbs, stats);
		offs += n;
	} while (offs < nb_pkts);
}

static uint16_t
pdump_rx(uint16_t port, uint16_t queue,
	struct rte_mbuf **pkts, uint16_t nb_pkts,
	uint16_t max_pkts __rte_unused, void *user_params)
{
	struct pdump_rxtx_cbs *cbs = user_params;
	struct rte_pdump_stats *stats = &pdump_stats->rx[port][queue];

	pdump_cb_hold(cbs);
	pdump_copy(port, queue, RTE_PCAPNG_DIRECTION_IN,
		   pkts, nb_pkts, cbs, stats);
	pdump_cb_release(cbs);

	return nb_pkts;
}

static uint16_t
pdump_tx(uint16_t port, uint16_t queue,
		struct rte_mbuf **pkts, uint16_t nb_pkts, void *user_params)
{
	struct pdump_rxtx_cbs *cbs = user_params;
	struct rte_pdump_stats *stats = &pdump_stats->tx[port][queue];

	pdump_cb_hold(cbs);
	pdump_copy(port, queue, RTE_PCAPNG_DIRECTION_OUT,
		   pkts, nb_pkts, cbs, stats);
	pdump_cb_release(cbs);

	return nb_pkts;
}


static int
pdump_register_rx_callbacks(enum pdump_version ver,
			    uint16_t end_q, uint16_t port, uint16_t queue,
			    struct rte_ring *ring, struct rte_mempool *mp,
			    struct rte_bpf *filter,
			    uint16_t operation, uint32_t snaplen)
{
	uint16_t qid;

	qid = (queue == RTE_PDUMP_ALL_QUEUES) ? 0 : queue;
	for (; qid < end_q; qid++) {
		struct pdump_rxtx_cbs *cbs = &rx_cbs[port][qid];

		if (operation == ENABLE) {
			if (cbs->cb) {
				PDUMP_LOG_LINE(ERR,
					"rx callback for port=%d queue=%d, already exists",
					port, qid);
				return -EEXIST;
			}
			cbs->use_count = 0;
			cbs->ver = ver;
			cbs->ring = ring;
			cbs->mp = mp;
			cbs->snaplen = snaplen;
			cbs->filter = filter;

			cbs->cb = rte_eth_add_first_rx_callback(port, qid,
								pdump_rx, cbs);
			if (cbs->cb == NULL) {
				PDUMP_LOG_LINE(ERR,
					"failed to add rx callback, errno=%d",
					rte_errno);
				return rte_errno;
			}

			memset(&pdump_stats->rx[port][qid], 0, sizeof(struct rte_pdump_stats));
		} else if (operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				PDUMP_LOG_LINE(ERR,
					"no existing rx callback for port=%d queue=%d",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_rx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				PDUMP_LOG_LINE(ERR,
					"failed to remove rx callback, errno=%d",
					-ret);
				return ret;
			}
			pdump_cb_wait(cbs);
			cbs->cb = NULL;
		}
	}

	return 0;
}

static int
pdump_register_tx_callbacks(enum pdump_version ver,
			    uint16_t end_q, uint16_t port, uint16_t queue,
			    struct rte_ring *ring, struct rte_mempool *mp,
			    struct rte_bpf *filter,
			    uint16_t operation, uint32_t snaplen)
{

	uint16_t qid;

	qid = (queue == RTE_PDUMP_ALL_QUEUES) ? 0 : queue;
	for (; qid < end_q; qid++) {
		struct pdump_rxtx_cbs *cbs = &tx_cbs[port][qid];

		if (operation == ENABLE) {
			if (cbs->cb) {
				PDUMP_LOG_LINE(ERR,
					"tx callback for port=%d queue=%d, already exists",
					port, qid);
				return -EEXIST;
			}
			cbs->use_count = 0;
			cbs->ver = ver;
			cbs->ring = ring;
			cbs->mp = mp;
			cbs->snaplen = snaplen;
			cbs->filter = filter;

			cbs->cb = rte_eth_add_tx_callback(port, qid, pdump_tx,
								cbs);
			if (cbs->cb == NULL) {
				PDUMP_LOG_LINE(ERR,
					"failed to add tx callback, errno=%d",
					rte_errno);
				return rte_errno;
			}
			memset(&pdump_stats->tx[port][qid], 0, sizeof(struct rte_pdump_stats));
		} else if (operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				PDUMP_LOG_LINE(ERR,
					"no existing tx callback for port=%d queue=%d",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_tx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				PDUMP_LOG_LINE(ERR,
					"failed to remove tx callback, errno=%d",
					-ret);
				return ret;
			}

			pdump_cb_wait(cbs);
			cbs->cb = NULL;
		}
	}

	return 0;
}

static int
set_pdump_rxtx_cbs(const struct pdump_request *p)
{
	uint16_t nb_rx_q = 0, nb_tx_q = 0, end_q, queue;
	uint16_t port;
	int ret = 0;
	struct rte_bpf *filter = NULL;
	uint32_t flags;
	uint16_t operation;
	struct rte_ring *ring;
	struct rte_mempool *mp;

	/* Check for possible DPDK version mismatch */
	if (!(p->ver == V1 || p->ver == V2)) {
		PDUMP_LOG_LINE(ERR,
			  "incorrect client version %u", p->ver);
		return -EINVAL;
	}

	if (p->prm) {
		if (p->prm->prog_arg.type != RTE_BPF_ARG_PTR_MBUF) {
			PDUMP_LOG_LINE(ERR,
				  "invalid BPF program type: %u",
				  p->prm->prog_arg.type);
			return -EINVAL;
		}

		filter = rte_bpf_load(p->prm);
		if (filter == NULL) {
			PDUMP_LOG_LINE(ERR, "cannot load BPF filter: %s",
				  rte_strerror(rte_errno));
			return -rte_errno;
		}
	}

	flags = p->flags;
	operation = p->op;
	queue = p->queue;
	ring = p->ring;
	mp = p->mp;

	ret = rte_eth_dev_get_port_by_name(p->device, &port);
	if (ret < 0) {
		PDUMP_LOG_LINE(ERR,
			  "failed to get port id for device id=%s",
			  p->device);
		return -EINVAL;
	}

	/* validation if packet capture is for all queues */
	if (queue == RTE_PDUMP_ALL_QUEUES) {
		struct rte_eth_dev_info dev_info;

		ret = rte_eth_dev_info_get(port, &dev_info);
		if (ret != 0) {
			PDUMP_LOG_LINE(ERR,
				"Error during getting device (port %u) info: %s",
				port, strerror(-ret));
			return ret;
		}

		nb_rx_q = dev_info.nb_rx_queues;
		nb_tx_q = dev_info.nb_tx_queues;
		if (nb_rx_q == 0 && flags & RTE_PDUMP_FLAG_RX) {
			PDUMP_LOG_LINE(ERR,
				"number of rx queues cannot be 0");
			return -EINVAL;
		}
		if (nb_tx_q == 0 && flags & RTE_PDUMP_FLAG_TX) {
			PDUMP_LOG_LINE(ERR,
				"number of tx queues cannot be 0");
			return -EINVAL;
		}
		if ((nb_tx_q == 0 || nb_rx_q == 0) &&
			flags == RTE_PDUMP_FLAG_RXTX) {
			PDUMP_LOG_LINE(ERR,
				"both tx&rx queues must be non zero");
			return -EINVAL;
		}
	}

	/* register RX callback */
	if (flags & RTE_PDUMP_FLAG_RX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_rx_q : queue + 1;
		ret = pdump_register_rx_callbacks(p->ver, end_q, port, queue,
						  ring, mp, filter,
						  operation, p->snaplen);
		if (ret < 0)
			return ret;
	}

	/* register TX callback */
	if (flags & RTE_PDUMP_FLAG_TX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_tx_q : queue + 1;
		ret = pdump_register_tx_callbacks(p->ver, end_q, port, queue,
						  ring, mp, filter,
						  operation, p->snaplen);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static void
pdump_request_to_secondary(const struct pdump_request *req)
{
	struct rte_mp_msg mp_req = { };
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = MP_TIMEOUT_S, .tv_nsec = 0};

	PDUMP_LOG_LINE(DEBUG, "forward req %s to secondary", pdump_opname(req->op));

	memcpy(mp_req.param, req, sizeof(*req));
	strlcpy(mp_req.name, PDUMP_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*req);

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) != 0)
		PDUMP_LOG_LINE(ERR, "rte_mp_request_sync failed");

	else if (mp_reply.nb_sent != mp_reply.nb_received)
		PDUMP_LOG_LINE(ERR, "not all secondary's replied (sent %u recv %u)",
			       mp_reply.nb_sent, mp_reply.nb_received);

	free(mp_reply.msgs);
}

/* Allocate temporary storage for passing state to the alarm thread for deferred handling */
static struct pdump_bundle *
pdump_bundle_alloc(const struct rte_mp_msg *mp_msg, const char *peer)
{
	struct pdump_bundle *bundle;
	size_t peer_len = strlen(peer) + 1;

	/* peer is the unix domain socket path */
	bundle = malloc(sizeof(*bundle) + peer_len);
	if (bundle == NULL)
		return NULL;

	bundle->msg = *mp_msg;
	memcpy(bundle->peer, peer, peer_len);
	return bundle;
}

/* Send response to peer */
static int
pdump_send_response(const struct pdump_request *req, int result, const void *peer)
{
	struct rte_mp_msg mp_resp = { };
	struct pdump_response *resp = (struct pdump_response *)mp_resp.param;
	int ret;

	if (req) {
		resp->ver = req->ver;
		resp->res_op = req->op;
	}
	resp->err_value = result;

	rte_strscpy(mp_resp.name, PDUMP_MP, RTE_MP_MAX_NAME_LEN);
	mp_resp.len_param = sizeof(*resp);

	ret = rte_mp_reply(&mp_resp, peer);
	if (ret != 0)
		PDUMP_LOG_LINE(ERR, "failed to send response: %s",
			  strerror(rte_errno));
	return ret;
}

/* Callback from MP request handler in secondary process */
static int
pdump_handle_primary_request(const struct rte_mp_msg *mp_msg, const void *peer)
{
	const struct pdump_request *req = NULL;
	int ret;

	if (mp_msg->len_param != sizeof(*req)) {
		PDUMP_LOG_LINE(ERR, "invalid request from primary");
		ret = -EINVAL;
	} else {
		req = (const struct pdump_request *)mp_msg->param;
		PDUMP_LOG_LINE(DEBUG, "secondary pdump %s", pdump_opname(req->op));

		/* Can just do it now, no need for interrupt thread */
		ret = set_pdump_rxtx_cbs(req);
	}

	return pdump_send_response(req, ret, peer);

}

/* Callback from the alarm handler (in interrupt thread) which does actual change */
static void
__pdump_request(void *param)
{
	struct pdump_bundle *bundle = param;
	struct rte_mp_msg *msg = &bundle->msg;
	const struct pdump_request *req =
		(const struct pdump_request *)msg->param;
	int ret;

	PDUMP_LOG_LINE(DEBUG, "primary pdump %s", pdump_opname(req->op));

	ret = set_pdump_rxtx_cbs(req);
	ret = pdump_send_response(req, ret, bundle->peer);

	/* Primary process is responsible for broadcasting request to all secondaries */
	if (ret == 0)
		pdump_request_to_secondary(req);

	free(bundle);
}

/* Callback from MP request handler in primary process */
static int
pdump_handle_secondary_request(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct pdump_bundle *bundle = NULL;
	const struct pdump_request *req = NULL;
	int ret;

	if (mp_msg->len_param != sizeof(*req)) {
		PDUMP_LOG_LINE(ERR, "invalid request from secondary");
		ret = -EINVAL;
		goto error;
	}

	req = (const struct pdump_request *)mp_msg->param;

	bundle = pdump_bundle_alloc(mp_msg, peer);
	if (bundle == NULL) {
		PDUMP_LOG_LINE(ERR, "not enough memory");
		ret = -ENOMEM;
		goto error;
	}

	/*
	 * We are in IPC callback thread, sync IPC is not possible
	 * since sending to secondary would cause livelock.
	 * Delegate the task to interrupt thread.
	 */
	ret = rte_eal_alarm_set(1, __pdump_request, bundle);
	if (ret != 0)
		goto error;
	return 0;

error:
	free(bundle);
	return pdump_send_response(req, ret, peer);
}

RTE_EXPORT_SYMBOL(rte_pdump_init)
int
rte_pdump_init(void)
{
	const struct rte_memzone *mz;
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = rte_mp_action_register(PDUMP_MP, pdump_handle_secondary_request);
		if (ret && rte_errno != ENOTSUP)
			return -1;

		mz = rte_memzone_reserve(MZ_RTE_PDUMP_STATS, sizeof(*pdump_stats),
					 SOCKET_ID_ANY, 0);
		if (mz == NULL) {
			PDUMP_LOG_LINE(ERR, "cannot allocate pdump statistics");
			rte_mp_action_unregister(PDUMP_MP);
			rte_errno = ENOMEM;
			return -1;
		}
	} else {
		ret = rte_mp_action_register(PDUMP_MP, pdump_handle_primary_request);
		if (ret && rte_errno != ENOTSUP)
			return -1;

		mz = rte_memzone_lookup(MZ_RTE_PDUMP_STATS);
		if (mz == NULL) {
			PDUMP_LOG_LINE(ERR, "cannot find pdump statistics");
			rte_mp_action_unregister(PDUMP_MP);
			rte_errno = ENOENT;
			return -1;
		}
	}

	pdump_stats = mz->addr;
	pdump_stats->mz = mz;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_pdump_uninit)
int
rte_pdump_uninit(void)
{
	rte_mp_action_unregister(PDUMP_MP);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY && pdump_stats != NULL) {
		rte_memzone_free(pdump_stats->mz);
		pdump_stats = NULL;
	}

	return 0;
}

static int
pdump_validate_ring_mp(struct rte_ring *ring, struct rte_mempool *mp)
{
	if (ring == NULL || mp == NULL) {
		PDUMP_LOG_LINE(ERR, "NULL ring or mempool");
		rte_errno = EINVAL;
		return -1;
	}
	if (mp->flags & RTE_MEMPOOL_F_SP_PUT ||
	    mp->flags & RTE_MEMPOOL_F_SC_GET) {
		PDUMP_LOG_LINE(ERR,
			  "mempool with SP or SC set not valid for pdump,"
			  "must have MP and MC set");
		rte_errno = EINVAL;
		return -1;
	}
	if (rte_ring_is_prod_single(ring) || rte_ring_is_cons_single(ring)) {
		PDUMP_LOG_LINE(ERR,
			  "ring with SP or SC set is not valid for pdump,"
			  "must have MP and MC set");
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_validate_flags(uint32_t flags)
{
	if ((flags & RTE_PDUMP_FLAG_RXTX) == 0) {
		PDUMP_LOG_LINE(ERR,
			"invalid flags, should be either rx/tx/rxtx");
		rte_errno = EINVAL;
		return -1;
	}

	/* mask off the flags we know about */
	if (flags & ~(RTE_PDUMP_FLAG_RXTX | RTE_PDUMP_FLAG_PCAPNG)) {
		PDUMP_LOG_LINE(ERR,
			  "unknown flags: %#x", flags);
		rte_errno = ENOTSUP;
		return -1;
	}

	return 0;
}

static int
pdump_validate_port(uint16_t port, char *name)
{
	int ret = 0;

	if (port >= RTE_MAX_ETHPORTS) {
		PDUMP_LOG_LINE(ERR, "Invalid port id %u", port);
		rte_errno = EINVAL;
		return -1;
	}

	ret = rte_eth_dev_get_name_by_port(port, name);
	if (ret < 0) {
		PDUMP_LOG_LINE(ERR, "port %u to name mapping failed",
			  port);
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_prepare_client_request(const char *device, uint16_t queue,
			     uint32_t flags, uint32_t snaplen,
			     uint16_t operation,
			     struct rte_ring *ring,
			     struct rte_mempool *mp,
			     const struct rte_bpf_prm *prm)
{
	int ret = -1;
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = MP_TIMEOUT_S, .tv_nsec = 0};
	struct pdump_request *req = (struct pdump_request *)mp_req.param;
	struct pdump_response *resp;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* FIXME */
		PDUMP_LOG_LINE(ERR,
			  "pdump enable/disable not allowed in primary process");
		return -EINVAL;
	}

	memset(req, 0, sizeof(*req));

	req->ver = (flags & RTE_PDUMP_FLAG_PCAPNG) ? V2 : V1;
	req->flags = flags & RTE_PDUMP_FLAG_RXTX;
	req->op = operation;
	req->queue = queue;
	rte_strscpy(req->device, device, sizeof(req->device));

	if ((operation & ENABLE) != 0) {
		req->ring = ring;
		req->mp = mp;
		req->prm = prm;
		req->snaplen = snaplen;
	}

	rte_strscpy(mp_req.name, PDUMP_MP, RTE_MP_MAX_NAME_LEN);
	mp_req.len_param = sizeof(*req);
	mp_req.num_fds = 0;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0) {
		mp_rep = &mp_reply.msgs[0];
		resp = (struct pdump_response *)mp_rep->param;
		if (resp->err_value == 0)
			ret = 0;
		else
			rte_errno = -resp->err_value;
		free(mp_reply.msgs);
	}

	if (ret < 0)
		PDUMP_LOG_LINE(ERR,
			"client request for pdump enable/disable failed");
	return ret;
}

/*
 * There are two versions of this function, because although original API
 * left place holder for future filter, it never checked the value.
 * Therefore the API can't depend on application passing a non
 * bogus value.
 */
static int
pdump_enable(uint16_t port, uint16_t queue,
	     uint32_t flags, uint32_t snaplen,
	     struct rte_ring *ring, struct rte_mempool *mp,
	     const struct rte_bpf_prm *prm)
{
	int ret;
	char name[RTE_DEV_NAME_MAX_LEN];

	ret = pdump_validate_port(port, name);
	if (ret < 0)
		return ret;
	ret = pdump_validate_ring_mp(ring, mp);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	if (snaplen == 0)
		snaplen = UINT32_MAX;

	return pdump_prepare_client_request(name, queue, flags, snaplen,
					    ENABLE, ring, mp, prm);
}

RTE_EXPORT_SYMBOL(rte_pdump_enable)
int
rte_pdump_enable(uint16_t port, uint16_t queue, uint32_t flags,
		 struct rte_ring *ring,
		 struct rte_mempool *mp,
		 void *filter __rte_unused)
{
	return pdump_enable(port, queue, flags, 0,
			    ring, mp, NULL);
}

RTE_EXPORT_SYMBOL(rte_pdump_enable_bpf)
int
rte_pdump_enable_bpf(uint16_t port, uint16_t queue,
		     uint32_t flags, uint32_t snaplen,
		     struct rte_ring *ring,
		     struct rte_mempool *mp,
		     const struct rte_bpf_prm *prm)
{
	return pdump_enable(port, queue, flags, snaplen,
			    ring, mp, prm);
}

static int
pdump_enable_by_deviceid(const char *device_id, uint16_t queue,
			 uint32_t flags, uint32_t snaplen,
			 struct rte_ring *ring,
			 struct rte_mempool *mp,
			 const struct rte_bpf_prm *prm)
{
	int ret;

	ret = pdump_validate_ring_mp(ring, mp);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	if (snaplen == 0)
		snaplen = UINT32_MAX;

	return pdump_prepare_client_request(device_id, queue, flags, snaplen,
					    ENABLE, ring, mp, prm);
}

RTE_EXPORT_SYMBOL(rte_pdump_enable_by_deviceid)
int
rte_pdump_enable_by_deviceid(char *device_id, uint16_t queue,
			     uint32_t flags,
			     struct rte_ring *ring,
			     struct rte_mempool *mp,
			     void *filter __rte_unused)
{
	return pdump_enable_by_deviceid(device_id, queue, flags, 0,
					ring, mp, NULL);
}

RTE_EXPORT_SYMBOL(rte_pdump_enable_bpf_by_deviceid)
int
rte_pdump_enable_bpf_by_deviceid(const char *device_id, uint16_t queue,
				 uint32_t flags, uint32_t snaplen,
				 struct rte_ring *ring,
				 struct rte_mempool *mp,
				 const struct rte_bpf_prm *prm)
{
	return pdump_enable_by_deviceid(device_id, queue, flags, snaplen,
					ring, mp, prm);
}

RTE_EXPORT_SYMBOL(rte_pdump_disable)
int
rte_pdump_disable(uint16_t port, uint16_t queue, uint32_t flags)
{
	int ret = 0;
	char name[RTE_DEV_NAME_MAX_LEN];

	ret = pdump_validate_port(port, name);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(name, queue, flags, 0,
					   DISABLE, NULL, NULL, NULL);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_pdump_disable_by_deviceid)
int
rte_pdump_disable_by_deviceid(char *device_id, uint16_t queue,
				uint32_t flags)
{
	int ret = 0;

	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(device_id, queue, flags, 0,
					   DISABLE, NULL, NULL, NULL);

	return ret;
}

static void
pdump_sum_stats(uint16_t port, uint16_t nq,
		struct rte_pdump_stats stats[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT],
		struct rte_pdump_stats *total)
{
	uint64_t *sum = (uint64_t *)total;
	unsigned int i;
	uint64_t val;
	uint16_t qid;

	for (qid = 0; qid < nq; qid++) {
		const RTE_ATOMIC(uint64_t) *perq = (const uint64_t __rte_atomic *)&stats[port][qid];

		for (i = 0; i < sizeof(*total) / sizeof(uint64_t); i++) {
			val = rte_atomic_load_explicit(&perq[i], rte_memory_order_relaxed);
			sum[i] += val;
		}
	}
}

RTE_EXPORT_SYMBOL(rte_pdump_stats)
int
rte_pdump_stats(uint16_t port, struct rte_pdump_stats *stats)
{
	struct rte_eth_dev_info dev_info;
	const struct rte_memzone *mz;
	int ret;

	memset(stats, 0, sizeof(*stats));
	ret = rte_eth_dev_info_get(port, &dev_info);
	if (ret != 0) {
		PDUMP_LOG_LINE(ERR,
			  "Error during getting device (port %u) info: %s",
			  port, strerror(-ret));
		return ret;
	}

	if (pdump_stats == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* rte_pdump_init was not called */
			PDUMP_LOG_LINE(ERR, "pdump stats not initialized");
			rte_errno = EINVAL;
			return -1;
		}

		/* secondary process looks up the memzone */
		mz = rte_memzone_lookup(MZ_RTE_PDUMP_STATS);
		if (mz == NULL) {
			/* rte_pdump_init was not called in primary process?? */
			PDUMP_LOG_LINE(ERR, "can not find pdump stats");
			rte_errno = EINVAL;
			return -1;
		}
		pdump_stats = mz->addr;
	}

	pdump_sum_stats(port, dev_info.nb_rx_queues, pdump_stats->rx, stats);
	pdump_sum_stats(port, dev_info.nb_tx_queues, pdump_stats->tx, stats);
	return 0;
}
