/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "rte_pdump.h"

#define DEVICE_ID_SIZE 64

RTE_LOG_REGISTER(pdump_logtype, lib.pdump, NOTICE);

/* Macro for printing using RTE_LOG */
#define PDUMP_LOG(level, fmt, args...)				\
	rte_log(RTE_LOG_ ## level, pdump_logtype, "%s(): " fmt,	\
		__func__, ## args)

/* Used for the multi-process communication */
#define PDUMP_MP	"mp_pdump"

enum pdump_operation {
	DISABLE = 1,
	ENABLE = 2
};

enum pdump_version {
	V1 = 1
};

struct pdump_request {
	uint16_t ver;
	uint16_t op;
	uint32_t flags;
	union pdump_data {
		struct enable_v1 {
			char device[DEVICE_ID_SIZE];
			uint16_t queue;
			struct rte_ring *ring;
			struct rte_mempool *mp;
			void *filter;
		} en_v1;
		struct disable_v1 {
			char device[DEVICE_ID_SIZE];
			uint16_t queue;
			struct rte_ring *ring;
			struct rte_mempool *mp;
			void *filter;
		} dis_v1;
	} data;
};

struct pdump_response {
	uint16_t ver;
	uint16_t res_op;
	int32_t err_value;
};

static struct pdump_rxtx_cbs {
	struct rte_ring *ring;
	struct rte_mempool *mp;
	const struct rte_eth_rxtx_callback *cb;
	void *filter;
} rx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT],
tx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];


static inline void
pdump_copy(struct rte_mbuf **pkts, uint16_t nb_pkts, void *user_params)
{
	unsigned i;
	int ring_enq;
	uint16_t d_pkts = 0;
	struct rte_mbuf *dup_bufs[nb_pkts];
	struct pdump_rxtx_cbs *cbs;
	struct rte_ring *ring;
	struct rte_mempool *mp;
	struct rte_mbuf *p;

	cbs  = user_params;
	ring = cbs->ring;
	mp = cbs->mp;
	for (i = 0; i < nb_pkts; i++) {
		p = rte_pktmbuf_copy(pkts[i], mp, 0, UINT32_MAX);
		if (p)
			dup_bufs[d_pkts++] = p;
	}

	ring_enq = rte_ring_enqueue_burst(ring, (void *)dup_bufs, d_pkts, NULL);
	if (unlikely(ring_enq < d_pkts)) {
		PDUMP_LOG(DEBUG,
			"only %d of packets enqueued to ring\n", ring_enq);
		do {
			rte_pktmbuf_free(dup_bufs[ring_enq]);
		} while (++ring_enq < d_pkts);
	}
}

static uint16_t
pdump_rx(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
	struct rte_mbuf **pkts, uint16_t nb_pkts,
	uint16_t max_pkts __rte_unused,
	void *user_params)
{
	pdump_copy(pkts, nb_pkts, user_params);
	return nb_pkts;
}

static uint16_t
pdump_tx(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts, void *user_params)
{
	pdump_copy(pkts, nb_pkts, user_params);
	return nb_pkts;
}

static int
pdump_register_rx_callbacks(uint16_t end_q, uint16_t port, uint16_t queue,
				struct rte_ring *ring, struct rte_mempool *mp,
				uint16_t operation)
{
	uint16_t qid;
	struct pdump_rxtx_cbs *cbs = NULL;

	qid = (queue == RTE_PDUMP_ALL_QUEUES) ? 0 : queue;
	for (; qid < end_q; qid++) {
		cbs = &rx_cbs[port][qid];
		if (cbs && operation == ENABLE) {
			if (cbs->cb) {
				PDUMP_LOG(ERR,
					"failed to add rx callback for port=%d "
					"and queue=%d, callback already exists\n",
					port, qid);
				return -EEXIST;
			}
			cbs->ring = ring;
			cbs->mp = mp;
			cbs->cb = rte_eth_add_first_rx_callback(port, qid,
								pdump_rx, cbs);
			if (cbs->cb == NULL) {
				PDUMP_LOG(ERR,
					"failed to add rx callback, errno=%d\n",
					rte_errno);
				return rte_errno;
			}
		}
		if (cbs && operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				PDUMP_LOG(ERR,
					"failed to delete non existing rx "
					"callback for port=%d and queue=%d\n",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_rx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				PDUMP_LOG(ERR,
					"failed to remove rx callback, errno=%d\n",
					-ret);
				return ret;
			}
			cbs->cb = NULL;
		}
	}

	return 0;
}

static int
pdump_register_tx_callbacks(uint16_t end_q, uint16_t port, uint16_t queue,
				struct rte_ring *ring, struct rte_mempool *mp,
				uint16_t operation)
{

	uint16_t qid;
	struct pdump_rxtx_cbs *cbs = NULL;

	qid = (queue == RTE_PDUMP_ALL_QUEUES) ? 0 : queue;
	for (; qid < end_q; qid++) {
		cbs = &tx_cbs[port][qid];
		if (cbs && operation == ENABLE) {
			if (cbs->cb) {
				PDUMP_LOG(ERR,
					"failed to add tx callback for port=%d "
					"and queue=%d, callback already exists\n",
					port, qid);
				return -EEXIST;
			}
			cbs->ring = ring;
			cbs->mp = mp;
			cbs->cb = rte_eth_add_tx_callback(port, qid, pdump_tx,
								cbs);
			if (cbs->cb == NULL) {
				PDUMP_LOG(ERR,
					"failed to add tx callback, errno=%d\n",
					rte_errno);
				return rte_errno;
			}
		}
		if (cbs && operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				PDUMP_LOG(ERR,
					"failed to delete non existing tx "
					"callback for port=%d and queue=%d\n",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_tx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				PDUMP_LOG(ERR,
					"failed to remove tx callback, errno=%d\n",
					-ret);
				return ret;
			}
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
	uint32_t flags;
	uint16_t operation;
	struct rte_ring *ring;
	struct rte_mempool *mp;

	flags = p->flags;
	operation = p->op;
	if (operation == ENABLE) {
		ret = rte_eth_dev_get_port_by_name(p->data.en_v1.device,
				&port);
		if (ret < 0) {
			PDUMP_LOG(ERR,
				"failed to get port id for device id=%s\n",
				p->data.en_v1.device);
			return -EINVAL;
		}
		queue = p->data.en_v1.queue;
		ring = p->data.en_v1.ring;
		mp = p->data.en_v1.mp;
	} else {
		ret = rte_eth_dev_get_port_by_name(p->data.dis_v1.device,
				&port);
		if (ret < 0) {
			PDUMP_LOG(ERR,
				"failed to get port id for device id=%s\n",
				p->data.dis_v1.device);
			return -EINVAL;
		}
		queue = p->data.dis_v1.queue;
		ring = p->data.dis_v1.ring;
		mp = p->data.dis_v1.mp;
	}

	/* validation if packet capture is for all queues */
	if (queue == RTE_PDUMP_ALL_QUEUES) {
		struct rte_eth_dev_info dev_info;

		ret = rte_eth_dev_info_get(port, &dev_info);
		if (ret != 0) {
			PDUMP_LOG(ERR,
				"Error during getting device (port %u) info: %s\n",
				port, strerror(-ret));
			return ret;
		}

		nb_rx_q = dev_info.nb_rx_queues;
		nb_tx_q = dev_info.nb_tx_queues;
		if (nb_rx_q == 0 && flags & RTE_PDUMP_FLAG_RX) {
			PDUMP_LOG(ERR,
				"number of rx queues cannot be 0\n");
			return -EINVAL;
		}
		if (nb_tx_q == 0 && flags & RTE_PDUMP_FLAG_TX) {
			PDUMP_LOG(ERR,
				"number of tx queues cannot be 0\n");
			return -EINVAL;
		}
		if ((nb_tx_q == 0 || nb_rx_q == 0) &&
			flags == RTE_PDUMP_FLAG_RXTX) {
			PDUMP_LOG(ERR,
				"both tx&rx queues must be non zero\n");
			return -EINVAL;
		}
	}

	/* register RX callback */
	if (flags & RTE_PDUMP_FLAG_RX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_rx_q : queue + 1;
		ret = pdump_register_rx_callbacks(end_q, port, queue, ring, mp,
							operation);
		if (ret < 0)
			return ret;
	}

	/* register TX callback */
	if (flags & RTE_PDUMP_FLAG_TX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_tx_q : queue + 1;
		ret = pdump_register_tx_callbacks(end_q, port, queue, ring, mp,
							operation);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int
pdump_server(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_resp;
	const struct pdump_request *cli_req;
	struct pdump_response *resp = (struct pdump_response *)&mp_resp.param;

	/* recv client requests */
	if (mp_msg->len_param != sizeof(*cli_req)) {
		PDUMP_LOG(ERR, "failed to recv from client\n");
		resp->err_value = -EINVAL;
	} else {
		cli_req = (const struct pdump_request *)mp_msg->param;
		resp->ver = cli_req->ver;
		resp->res_op = cli_req->op;
		resp->err_value = set_pdump_rxtx_cbs(cli_req);
	}

	strlcpy(mp_resp.name, PDUMP_MP, RTE_MP_MAX_NAME_LEN);
	mp_resp.len_param = sizeof(*resp);
	mp_resp.num_fds = 0;
	if (rte_mp_reply(&mp_resp, peer) < 0) {
		PDUMP_LOG(ERR, "failed to send to client:%s\n",
			  strerror(rte_errno));
		return -1;
	}

	return 0;
}

int
rte_pdump_init(void)
{
	int ret = rte_mp_action_register(PDUMP_MP, pdump_server);
	if (ret && rte_errno != ENOTSUP)
		return -1;
	return 0;
}

int
rte_pdump_uninit(void)
{
	rte_mp_action_unregister(PDUMP_MP);

	return 0;
}

static int
pdump_validate_ring_mp(struct rte_ring *ring, struct rte_mempool *mp)
{
	if (ring == NULL || mp == NULL) {
		PDUMP_LOG(ERR, "NULL ring or mempool\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (mp->flags & MEMPOOL_F_SP_PUT || mp->flags & MEMPOOL_F_SC_GET) {
		PDUMP_LOG(ERR, "mempool with either SP or SC settings"
		" is not valid for pdump, should have MP and MC settings\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (rte_ring_is_prod_single(ring) || rte_ring_is_cons_single(ring)) {
		PDUMP_LOG(ERR, "ring with either SP or SC settings"
		" is not valid for pdump, should have MP and MC settings\n");
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_validate_flags(uint32_t flags)
{
	if (flags != RTE_PDUMP_FLAG_RX && flags != RTE_PDUMP_FLAG_TX &&
		flags != RTE_PDUMP_FLAG_RXTX) {
		PDUMP_LOG(ERR,
			"invalid flags, should be either rx/tx/rxtx\n");
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_validate_port(uint16_t port, char *name)
{
	int ret = 0;

	if (port >= RTE_MAX_ETHPORTS) {
		PDUMP_LOG(ERR, "Invalid port id %u\n", port);
		rte_errno = EINVAL;
		return -1;
	}

	ret = rte_eth_dev_get_name_by_port(port, name);
	if (ret < 0) {
		PDUMP_LOG(ERR, "port %u to name mapping failed\n",
			  port);
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_prepare_client_request(char *device, uint16_t queue,
				uint32_t flags,
				uint16_t operation,
				struct rte_ring *ring,
				struct rte_mempool *mp,
				void *filter)
{
	int ret = -1;
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct pdump_request *req = (struct pdump_request *)mp_req.param;
	struct pdump_response *resp;

	req->ver = 1;
	req->flags = flags;
	req->op = operation;
	if ((operation & ENABLE) != 0) {
		strlcpy(req->data.en_v1.device, device,
			sizeof(req->data.en_v1.device));
		req->data.en_v1.queue = queue;
		req->data.en_v1.ring = ring;
		req->data.en_v1.mp = mp;
		req->data.en_v1.filter = filter;
	} else {
		strlcpy(req->data.dis_v1.device, device,
			sizeof(req->data.dis_v1.device));
		req->data.dis_v1.queue = queue;
		req->data.dis_v1.ring = NULL;
		req->data.dis_v1.mp = NULL;
		req->data.dis_v1.filter = NULL;
	}

	strlcpy(mp_req.name, PDUMP_MP, RTE_MP_MAX_NAME_LEN);
	mp_req.len_param = sizeof(*req);
	mp_req.num_fds = 0;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0) {
		mp_rep = &mp_reply.msgs[0];
		resp = (struct pdump_response *)mp_rep->param;
		rte_errno = resp->err_value;
		if (!resp->err_value)
			ret = 0;
		free(mp_reply.msgs);
	}

	if (ret < 0)
		PDUMP_LOG(ERR,
			"client request for pdump enable/disable failed\n");
	return ret;
}

int
rte_pdump_enable(uint16_t port, uint16_t queue, uint32_t flags,
			struct rte_ring *ring,
			struct rte_mempool *mp,
			void *filter)
{

	int ret = 0;
	char name[DEVICE_ID_SIZE];

	ret = pdump_validate_port(port, name);
	if (ret < 0)
		return ret;
	ret = pdump_validate_ring_mp(ring, mp);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(name, queue, flags,
						ENABLE, ring, mp, filter);

	return ret;
}

int
rte_pdump_enable_by_deviceid(char *device_id, uint16_t queue,
				uint32_t flags,
				struct rte_ring *ring,
				struct rte_mempool *mp,
				void *filter)
{
	int ret = 0;

	ret = pdump_validate_ring_mp(ring, mp);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(device_id, queue, flags,
						ENABLE, ring, mp, filter);

	return ret;
}

int
rte_pdump_disable(uint16_t port, uint16_t queue, uint32_t flags)
{
	int ret = 0;
	char name[DEVICE_ID_SIZE];

	ret = pdump_validate_port(port, name);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(name, queue, flags,
						DISABLE, NULL, NULL, NULL);

	return ret;
}

int
rte_pdump_disable_by_deviceid(char *device_id, uint16_t queue,
				uint32_t flags)
{
	int ret = 0;

	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	ret = pdump_prepare_client_request(device_id, queue, flags,
						DISABLE, NULL, NULL, NULL);

	return ret;
}
