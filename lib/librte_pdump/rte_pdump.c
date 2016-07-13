/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_pci.h>

#include "rte_pdump.h"

#define SOCKET_PATH_VAR_RUN "/var/run"
#define SOCKET_PATH_HOME "HOME"
#define DPDK_DIR         "/.dpdk"
#define SOCKET_DIR       "/pdump_sockets"
#define SERVER_SOCKET "%s/pdump_server_socket"
#define CLIENT_SOCKET "%s/pdump_client_socket_%d_%u"
#define DEVICE_ID_SIZE 64
/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_PDUMP RTE_LOGTYPE_USER1

enum pdump_operation {
	DISABLE = 1,
	ENABLE = 2
};

enum pdump_version {
	V1 = 1
};

static pthread_t pdump_thread;
static int pdump_socket_fd;
static char server_socket_dir[PATH_MAX];
static char client_socket_dir[PATH_MAX];

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
	struct rte_eth_rxtx_callback *cb;
	void *filter;
} rx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT],
tx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];

static inline int
pdump_pktmbuf_copy_data(struct rte_mbuf *seg, const struct rte_mbuf *m)
{
	if (rte_pktmbuf_tailroom(seg) < m->data_len) {
		RTE_LOG(ERR, PDUMP,
			"User mempool: insufficient data_len of mbuf\n");
		return -EINVAL;
	}

	seg->port = m->port;
	seg->vlan_tci = m->vlan_tci;
	seg->hash = m->hash;
	seg->tx_offload = m->tx_offload;
	seg->ol_flags = m->ol_flags;
	seg->packet_type = m->packet_type;
	seg->vlan_tci_outer = m->vlan_tci_outer;
	seg->data_len = m->data_len;
	seg->pkt_len = seg->data_len;
	rte_memcpy(rte_pktmbuf_mtod(seg, void *),
			rte_pktmbuf_mtod(m, void *),
			rte_pktmbuf_data_len(seg));

	return 0;
}

static inline struct rte_mbuf *
pdump_pktmbuf_copy(struct rte_mbuf *m, struct rte_mempool *mp)
{
	struct rte_mbuf *m_dup, *seg, **prev;
	uint32_t pktlen;
	uint8_t nseg;

	m_dup = rte_pktmbuf_alloc(mp);
	if (unlikely(m_dup == NULL))
		return NULL;

	seg = m_dup;
	prev = &seg->next;
	pktlen = m->pkt_len;
	nseg = 0;

	do {
		nseg++;
		if (pdump_pktmbuf_copy_data(seg, m) < 0) {
			rte_pktmbuf_free(m_dup);
			return NULL;
		}
		*prev = seg;
		prev = &seg->next;
	} while ((m = m->next) != NULL &&
			(seg = rte_pktmbuf_alloc(mp)) != NULL);

	*prev = NULL;
	m_dup->nb_segs = nseg;
	m_dup->pkt_len = pktlen;

	/* Allocation of new indirect segment failed */
	if (unlikely(seg == NULL)) {
		rte_pktmbuf_free(m_dup);
		return NULL;
	}

	__rte_mbuf_sanity_check(m_dup, 1);
	return m_dup;
}

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
		p = pdump_pktmbuf_copy(pkts[i], mp);
		if (p)
			dup_bufs[d_pkts++] = p;
	}

	ring_enq = rte_ring_enqueue_burst(ring, (void *)dup_bufs, d_pkts);
	if (unlikely(ring_enq < d_pkts)) {
		RTE_LOG(DEBUG, PDUMP,
			"only %d of packets enqueued to ring\n", ring_enq);
		do {
			rte_pktmbuf_free(dup_bufs[ring_enq]);
		} while (++ring_enq < d_pkts);
	}
}

static uint16_t
pdump_rx(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
	struct rte_mbuf **pkts, uint16_t nb_pkts,
	uint16_t max_pkts __rte_unused,
	void *user_params)
{
	pdump_copy(pkts, nb_pkts, user_params);
	return nb_pkts;
}

static uint16_t
pdump_tx(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts, void *user_params)
{
	pdump_copy(pkts, nb_pkts, user_params);
	return nb_pkts;
}

static int
pdump_get_dombdf(char *device_id, char *domBDF, size_t len)
{
	int ret;
	struct rte_pci_addr dev_addr = {0};

	/* identify if device_id is pci address or name */
	ret = eal_parse_pci_DomBDF(device_id, &dev_addr);
	if (ret < 0)
		return -1;

	if (dev_addr.domain)
		ret = snprintf(domBDF, len, "%u:%u:%u.%u", dev_addr.domain,
				dev_addr.bus, dev_addr.devid,
				dev_addr.function);
	else
		ret = snprintf(domBDF, len, "%u:%u.%u", dev_addr.bus,
				dev_addr.devid,
				dev_addr.function);

	return ret;
}

static int
pdump_regitser_rx_callbacks(uint16_t end_q, uint8_t port, uint16_t queue,
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
				RTE_LOG(ERR, PDUMP,
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
				RTE_LOG(ERR, PDUMP,
					"failed to add rx callback, errno=%d\n",
					rte_errno);
				return rte_errno;
			}
		}
		if (cbs && operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				RTE_LOG(ERR, PDUMP,
					"failed to delete non existing rx "
					"callback for port=%d and queue=%d\n",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_rx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				RTE_LOG(ERR, PDUMP,
					"failed to remove rx callback, errno=%d\n",
					rte_errno);
				return ret;
			}
			cbs->cb = NULL;
		}
	}

	return 0;
}

static int
pdump_regitser_tx_callbacks(uint16_t end_q, uint8_t port, uint16_t queue,
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
				RTE_LOG(ERR, PDUMP,
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
				RTE_LOG(ERR, PDUMP,
					"failed to add tx callback, errno=%d\n",
					rte_errno);
				return rte_errno;
			}
		}
		if (cbs && operation == DISABLE) {
			int ret;

			if (cbs->cb == NULL) {
				RTE_LOG(ERR, PDUMP,
					"failed to delete non existing tx "
					"callback for port=%d and queue=%d\n",
					port, qid);
				return -EINVAL;
			}
			ret = rte_eth_remove_tx_callback(port, qid, cbs->cb);
			if (ret < 0) {
				RTE_LOG(ERR, PDUMP,
					"failed to remove tx callback, errno=%d\n",
					rte_errno);
				return ret;
			}
			cbs->cb = NULL;
		}
	}

	return 0;
}

static int
set_pdump_rxtx_cbs(struct pdump_request *p)
{
	uint16_t nb_rx_q, nb_tx_q = 0, end_q, queue;
	uint8_t port;
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
			RTE_LOG(ERR, PDUMP,
				"failed to get potid for device id=%s\n",
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
			RTE_LOG(ERR, PDUMP,
				"failed to get potid for device id=%s\n",
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

		rte_eth_dev_info_get(port, &dev_info);
		nb_rx_q = dev_info.nb_rx_queues;
		nb_tx_q = dev_info.nb_tx_queues;
		if (nb_rx_q == 0 && flags & RTE_PDUMP_FLAG_RX) {
			RTE_LOG(ERR, PDUMP,
				"number of rx queues cannot be 0\n");
			return -EINVAL;
		}
		if (nb_tx_q == 0 && flags & RTE_PDUMP_FLAG_TX) {
			RTE_LOG(ERR, PDUMP,
				"number of tx queues cannot be 0\n");
			return -EINVAL;
		}
		if ((nb_tx_q == 0 || nb_rx_q == 0) &&
			flags == RTE_PDUMP_FLAG_RXTX) {
			RTE_LOG(ERR, PDUMP,
				"both tx&rx queues must be non zero\n");
			return -EINVAL;
		}
	}

	/* register RX callback */
	if (flags & RTE_PDUMP_FLAG_RX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_rx_q : queue + 1;
		ret = pdump_regitser_rx_callbacks(end_q, port, queue, ring, mp,
							operation);
		if (ret < 0)
			return ret;
	}

	/* register TX callback */
	if (flags & RTE_PDUMP_FLAG_TX) {
		end_q = (queue == RTE_PDUMP_ALL_QUEUES) ? nb_tx_q : queue + 1;
		ret = pdump_regitser_tx_callbacks(end_q, port, queue, ring, mp,
							operation);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/* get socket path (/var/run if root, $HOME otherwise) */
static int
pdump_get_socket_path(char *buffer, int bufsz, enum rte_pdump_socktype type)
{
	char dpdk_dir[PATH_MAX] = {0};
	char dir[PATH_MAX] = {0};
	char *dir_home = NULL;
	int ret = 0;

	if (type == RTE_PDUMP_SOCKET_SERVER && server_socket_dir[0] != 0)
		snprintf(dir, sizeof(dir), "%s", server_socket_dir);
	else if (type == RTE_PDUMP_SOCKET_CLIENT && client_socket_dir[0] != 0)
		snprintf(dir, sizeof(dir), "%s", client_socket_dir);
	else {
		if (getuid() != 0) {
			dir_home = getenv(SOCKET_PATH_HOME);
			if (!dir_home) {
				RTE_LOG(ERR, PDUMP,
					"Failed to get environment variable"
					" value for %s, %s:%d\n",
					SOCKET_PATH_HOME, __func__, __LINE__);
				return -1;
			}
			snprintf(dpdk_dir, sizeof(dpdk_dir), "%s%s",
					dir_home, DPDK_DIR);
		} else
			snprintf(dpdk_dir, sizeof(dpdk_dir), "%s%s",
					SOCKET_PATH_VAR_RUN, DPDK_DIR);

		mkdir(dpdk_dir, 700);
		snprintf(dir, sizeof(dir), "%s%s",
					dpdk_dir, SOCKET_DIR);
	}

	ret =  mkdir(dir, 700);
	/* if user passed socket path is invalid, return immediately */
	if (ret < 0 && errno != EEXIST) {
		RTE_LOG(ERR, PDUMP,
			"Failed to create dir:%s:%s\n", dir,
			strerror(errno));
		rte_errno = errno;
		return -1;
	}

	if (type == RTE_PDUMP_SOCKET_SERVER)
		snprintf(buffer, bufsz, SERVER_SOCKET, dir);
	else
		snprintf(buffer, bufsz, CLIENT_SOCKET, dir, getpid(),
				rte_sys_gettid());

	return 0;
}

static int
pdump_create_server_socket(void)
{
	int ret, socket_fd;
	struct sockaddr_un addr;
	socklen_t addr_len;

	ret = pdump_get_socket_path(addr.sun_path, sizeof(addr.sun_path),
				RTE_PDUMP_SOCKET_SERVER);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to get server socket path: %s:%d\n",
			__func__, __LINE__);
		return -1;
	}
	addr.sun_family = AF_UNIX;

	/* remove if file already exists */
	unlink(addr.sun_path);

	/* set up a server socket */
	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to create server socket: %s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		return -1;
	}

	addr_len = sizeof(struct sockaddr_un);
	ret = bind(socket_fd, (struct sockaddr *) &addr, addr_len);
	if (ret) {
		RTE_LOG(ERR, PDUMP,
			"Failed to bind to server socket: %s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		close(socket_fd);
		return -1;
	}

	/* save the socket in local configuration */
	pdump_socket_fd = socket_fd;

	return 0;
}

static __attribute__((noreturn)) void *
pdump_thread_main(__rte_unused void *arg)
{
	struct sockaddr_un cli_addr;
	socklen_t cli_len;
	struct pdump_request cli_req;
	struct pdump_response resp;
	int n;
	int ret = 0;

	/* host thread, never break out */
	for (;;) {
		/* recv client requests */
		cli_len = sizeof(cli_addr);
		n = recvfrom(pdump_socket_fd, &cli_req,
				sizeof(struct pdump_request), 0,
				(struct sockaddr *)&cli_addr, &cli_len);
		if (n < 0) {
			RTE_LOG(ERR, PDUMP,
				"failed to recv from client:%s, %s:%d\n",
				strerror(errno), __func__, __LINE__);
			continue;
		}

		ret = set_pdump_rxtx_cbs(&cli_req);

		resp.ver = cli_req.ver;
		resp.res_op = cli_req.op;
		resp.err_value = ret;
		n = sendto(pdump_socket_fd, &resp,
				sizeof(struct pdump_response),
				0, (struct sockaddr *)&cli_addr, cli_len);
		if (n < 0) {
			RTE_LOG(ERR, PDUMP,
				"failed to send to client:%s, %s:%d\n",
				strerror(errno), __func__, __LINE__);
		}
	}
}

int
rte_pdump_init(const char *path)
{
	int ret = 0;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

	ret = rte_pdump_set_socket_dir(path, RTE_PDUMP_SOCKET_SERVER);
	if (ret != 0)
		return -1;

	ret = pdump_create_server_socket();
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to create server socket:%s:%d\n",
			__func__, __LINE__);
		return -1;
	}

	/* create the host thread to wait/handle pdump requests */
	ret = pthread_create(&pdump_thread, NULL, pdump_thread_main, NULL);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to create the pdump thread:%s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		return -1;
	}
	/* Set thread_name for aid in debugging. */
	snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "pdump-thread");
	ret = rte_thread_setname(pdump_thread, thread_name);
	if (ret != 0) {
		RTE_LOG(DEBUG, PDUMP,
			"Failed to set thread name for pdump handling\n");
	}

	return 0;
}

int
rte_pdump_uninit(void)
{
	int ret;

	ret = pthread_cancel(pdump_thread);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to cancel the pdump thread:%s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		return -1;
	}

	ret = close(pdump_socket_fd);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to close server socket: %s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		return -1;
	}

	struct sockaddr_un addr;

	ret = pdump_get_socket_path(addr.sun_path, sizeof(addr.sun_path),
				RTE_PDUMP_SOCKET_SERVER);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to get server socket path: %s:%d\n",
			__func__, __LINE__);
		return -1;
	}
	ret = unlink(addr.sun_path);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to remove server socket addr: %s, %s:%d\n",
			strerror(errno), __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int
pdump_create_client_socket(struct pdump_request *p)
{
	int ret, socket_fd;
	int pid;
	int n;
	struct pdump_response server_resp;
	struct sockaddr_un addr, serv_addr, from;
	socklen_t addr_len, serv_len;

	pid = getpid();

	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0) {
		RTE_LOG(ERR, PDUMP,
			"client socket(): %s:pid(%d):tid(%u), %s:%d\n",
			strerror(errno), pid, rte_sys_gettid(),
			__func__, __LINE__);
		rte_errno = errno;
		return -1;
	}

	ret = pdump_get_socket_path(addr.sun_path, sizeof(addr.sun_path),
				RTE_PDUMP_SOCKET_CLIENT);
	if (ret != 0) {
		RTE_LOG(ERR, PDUMP,
			"Failed to get client socket path: %s:%d\n",
			__func__, __LINE__);
		rte_errno = errno;
		goto exit;
	}
	addr.sun_family = AF_UNIX;
	addr_len = sizeof(struct sockaddr_un);

	do {
		ret = bind(socket_fd, (struct sockaddr *) &addr, addr_len);
		if (ret) {
			RTE_LOG(ERR, PDUMP,
				"client bind(): %s, %s:%d\n",
				strerror(errno), __func__, __LINE__);
			rte_errno = errno;
			break;
		}

		serv_len = sizeof(struct sockaddr_un);
		memset(&serv_addr, 0, sizeof(serv_addr));
		ret = pdump_get_socket_path(serv_addr.sun_path,
					sizeof(serv_addr.sun_path),
					RTE_PDUMP_SOCKET_SERVER);
		if (ret != 0) {
			RTE_LOG(ERR, PDUMP,
				"Failed to get server socket path: %s:%d\n",
				__func__, __LINE__);
			rte_errno = errno;
			break;
		}
		serv_addr.sun_family = AF_UNIX;

		n =  sendto(socket_fd, p, sizeof(struct pdump_request), 0,
				(struct sockaddr *)&serv_addr, serv_len);
		if (n < 0) {
			RTE_LOG(ERR, PDUMP,
				"failed to send to server:%s, %s:%d\n",
				strerror(errno), __func__, __LINE__);
			rte_errno = errno;
			ret = -1;
			break;
		}

		n = recvfrom(socket_fd, &server_resp,
				sizeof(struct pdump_response), 0,
				(struct sockaddr *)&from, &serv_len);
		if (n < 0) {
			RTE_LOG(ERR, PDUMP,
				"failed to recv from server:%s, %s:%d\n",
				strerror(errno), __func__, __LINE__);
			rte_errno = errno;
			ret = -1;
			break;
		}
		ret = server_resp.err_value;
	} while (0);

exit:
	close(socket_fd);
	unlink(addr.sun_path);
	return ret;
}

static int
pdump_validate_ring_mp(struct rte_ring *ring, struct rte_mempool *mp)
{
	if (ring == NULL || mp == NULL) {
		RTE_LOG(ERR, PDUMP, "NULL ring or mempool are passed %s:%d\n",
			__func__, __LINE__);
		rte_errno = EINVAL;
		return -1;
	}
	if (mp->flags & MEMPOOL_F_SP_PUT || mp->flags & MEMPOOL_F_SC_GET) {
		RTE_LOG(ERR, PDUMP, "mempool with either SP or SC settings"
		" is not valid for pdump, should have MP and MC settings\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (ring->prod.sp_enqueue || ring->cons.sc_dequeue) {
		RTE_LOG(ERR, PDUMP, "ring with either SP or SC settings"
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
		RTE_LOG(ERR, PDUMP,
			"invalid flags, should be either rx/tx/rxtx\n");
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
pdump_validate_port(uint8_t port, char *name)
{
	int ret = 0;

	if (port >= RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, PDUMP, "Invalid port id %u, %s:%d\n", port,
			__func__, __LINE__);
		rte_errno = EINVAL;
		return -1;
	}

	ret = rte_eth_dev_get_name_by_port(port, name);
	if (ret < 0) {
		RTE_LOG(ERR, PDUMP,
			"port id to name mapping failed for port id=%u, %s:%d\n",
			port, __func__, __LINE__);
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
	int ret;
	struct pdump_request req = {.ver = 1,};

	req.flags = flags;
	req.op =  operation;
	if ((operation & ENABLE) != 0) {
		snprintf(req.data.en_v1.device, sizeof(req.data.en_v1.device),
				"%s", device);
		req.data.en_v1.queue = queue;
		req.data.en_v1.ring = ring;
		req.data.en_v1.mp = mp;
		req.data.en_v1.filter = filter;
	} else {
		snprintf(req.data.dis_v1.device, sizeof(req.data.dis_v1.device),
				"%s", device);
		req.data.dis_v1.queue = queue;
		req.data.dis_v1.ring = NULL;
		req.data.dis_v1.mp = NULL;
		req.data.dis_v1.filter = NULL;
	}

	ret = pdump_create_client_socket(&req);
	if (ret < 0) {
		RTE_LOG(ERR, PDUMP,
			"client request for pdump enable/disable failed\n");
		rte_errno = ret;
		return -1;
	}

	return 0;
}

int
rte_pdump_enable(uint8_t port, uint16_t queue, uint32_t flags,
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
	char domBDF[DEVICE_ID_SIZE];

	ret = pdump_validate_ring_mp(ring, mp);
	if (ret < 0)
		return ret;
	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	if (pdump_get_dombdf(device_id, domBDF, sizeof(domBDF)) > 0)
		ret = pdump_prepare_client_request(domBDF, queue, flags,
						ENABLE, ring, mp, filter);
	else
		ret = pdump_prepare_client_request(device_id, queue, flags,
						ENABLE, ring, mp, filter);

	return ret;
}

int
rte_pdump_disable(uint8_t port, uint16_t queue, uint32_t flags)
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
	char domBDF[DEVICE_ID_SIZE];

	ret = pdump_validate_flags(flags);
	if (ret < 0)
		return ret;

	if (pdump_get_dombdf(device_id, domBDF, sizeof(domBDF)) > 0)
		ret = pdump_prepare_client_request(domBDF, queue, flags,
						DISABLE, NULL, NULL, NULL);
	else
		ret = pdump_prepare_client_request(device_id, queue, flags,
						DISABLE, NULL, NULL, NULL);

	return ret;
}

int
rte_pdump_set_socket_dir(const char *path, enum rte_pdump_socktype type)
{
	int ret, count;

	if (path != NULL) {
		if (type == RTE_PDUMP_SOCKET_SERVER) {
			count = sizeof(server_socket_dir);
			ret = snprintf(server_socket_dir, count, "%s", path);
		} else {
			count = sizeof(client_socket_dir);
			ret = snprintf(client_socket_dir, count, "%s", path);
		}

		if (ret < 0  || ret >= count) {
			RTE_LOG(ERR, PDUMP,
					"Invalid socket path:%s:%d\n",
					__func__, __LINE__);
			if (type == RTE_PDUMP_SOCKET_SERVER)
				server_socket_dir[0] = 0;
			else
				client_socket_dir[0] = 0;
			return -EINVAL;
		}
	}

	return 0;
}
