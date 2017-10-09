/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_io.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

/* Initialization data for hash RX queues. */
const struct hash_rxq_init hash_rxq_init[] = {
	[HASH_RXQ_TCPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4 |
				IBV_RX_HASH_SRC_PORT_TCP |
				IBV_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_TCP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_FLOW_SPEC_TCP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV4],
	},
	[HASH_RXQ_UDPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4 |
				IBV_RX_HASH_SRC_PORT_UDP |
				IBV_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_UDP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_FLOW_SPEC_UDP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV4],
	},
	[HASH_RXQ_IPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4),
		.dpdk_rss_hf = (ETH_RSS_IPV4 |
				ETH_RSS_FRAG_IPV4),
		.flow_priority = 1,
		.flow_spec.ipv4 = {
			.type = IBV_FLOW_SPEC_IPV4,
			.size = sizeof(hash_rxq_init[0].flow_spec.ipv4),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_ETH],
	},
	[HASH_RXQ_TCPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6 |
				IBV_RX_HASH_SRC_PORT_TCP |
				IBV_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_TCP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_FLOW_SPEC_TCP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV6],
	},
	[HASH_RXQ_UDPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6 |
				IBV_RX_HASH_SRC_PORT_UDP |
				IBV_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_UDP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_FLOW_SPEC_UDP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV6],
	},
	[HASH_RXQ_IPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6),
		.dpdk_rss_hf = (ETH_RSS_IPV6 |
				ETH_RSS_FRAG_IPV6),
		.flow_priority = 1,
		.flow_spec.ipv6 = {
			.type = IBV_FLOW_SPEC_IPV6,
			.size = sizeof(hash_rxq_init[0].flow_spec.ipv6),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_ETH],
	},
	[HASH_RXQ_ETH] = {
		.hash_fields = 0,
		.dpdk_rss_hf = 0,
		.flow_priority = 2,
		.flow_spec.eth = {
			.type = IBV_FLOW_SPEC_ETH,
			.size = sizeof(hash_rxq_init[0].flow_spec.eth),
		},
		.underlayer = NULL,
	},
};

/* Number of entries in hash_rxq_init[]. */
const unsigned int hash_rxq_init_n = RTE_DIM(hash_rxq_init);

/* Initialization data for hash RX queue indirection tables. */
static const struct ind_table_init ind_table_init[] = {
	{
		.max_size = -1u, /* Superseded by HW limitations. */
		.hash_types =
			1 << HASH_RXQ_TCPV4 |
			1 << HASH_RXQ_UDPV4 |
			1 << HASH_RXQ_IPV4 |
			1 << HASH_RXQ_TCPV6 |
			1 << HASH_RXQ_UDPV6 |
			1 << HASH_RXQ_IPV6 |
			0,
		.hash_types_n = 6,
	},
	{
		.max_size = 1,
		.hash_types = 1 << HASH_RXQ_ETH,
		.hash_types_n = 1,
	},
};

#define IND_TABLE_INIT_N RTE_DIM(ind_table_init)

/* Default RSS hash key also used for ConnectX-3. */
uint8_t rss_hash_default_key[] = {
	0x2c, 0xc6, 0x81, 0xd1,
	0x5b, 0xdb, 0xf4, 0xf7,
	0xfc, 0xa2, 0x83, 0x19,
	0xdb, 0x1a, 0x3e, 0x94,
	0x6b, 0x9e, 0x38, 0xd9,
	0x2c, 0x9c, 0x03, 0xd1,
	0xad, 0x99, 0x44, 0xa7,
	0xd9, 0x56, 0x3d, 0x59,
	0x06, 0x3c, 0x25, 0xf3,
	0xfc, 0x1f, 0xdc, 0x2a,
};

/* Length of the default RSS hash key. */
const size_t rss_hash_default_key_len = sizeof(rss_hash_default_key);

/**
 * Populate flow steering rule for a given hash RX queue type using
 * information from hash_rxq_init[]. Nothing is written to flow_attr when
 * flow_attr_size is not large enough, but the required size is still returned.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] flow_attr
 *   Pointer to flow attribute structure to fill. Note that the allocated
 *   area must be larger and large enough to hold all flow specifications.
 * @param flow_attr_size
 *   Entire size of flow_attr and trailing room for flow specifications.
 * @param type
 *   Hash RX queue type to use for flow steering rule.
 *
 * @return
 *   Total size of the flow attribute buffer. No errors are defined.
 */
size_t
priv_flow_attr(struct priv *priv, struct ibv_flow_attr *flow_attr,
	       size_t flow_attr_size, enum hash_rxq_type type)
{
	size_t offset = sizeof(*flow_attr);
	const struct hash_rxq_init *init = &hash_rxq_init[type];

	assert(priv != NULL);
	assert((size_t)type < RTE_DIM(hash_rxq_init));
	do {
		offset += init->flow_spec.hdr.size;
		init = init->underlayer;
	} while (init != NULL);
	if (offset > flow_attr_size)
		return offset;
	flow_attr_size = offset;
	init = &hash_rxq_init[type];
	*flow_attr = (struct ibv_flow_attr){
		.type = IBV_FLOW_ATTR_NORMAL,
		/* Priorities < 3 are reserved for flow director. */
		.priority = init->flow_priority + 3,
		.num_of_specs = 0,
		.port = priv->port,
		.flags = 0,
	};
	do {
		offset -= init->flow_spec.hdr.size;
		memcpy((void *)((uintptr_t)flow_attr + offset),
		       &init->flow_spec,
		       init->flow_spec.hdr.size);
		++flow_attr->num_of_specs;
		init = init->underlayer;
	} while (init != NULL);
	return flow_attr_size;
}

/**
 * Convert hash type position in indirection table initializer to
 * hash RX queue type.
 *
 * @param table
 *   Indirection table initializer.
 * @param pos
 *   Hash type position.
 *
 * @return
 *   Hash RX queue type.
 */
static enum hash_rxq_type
hash_rxq_type_from_pos(const struct ind_table_init *table, unsigned int pos)
{
	enum hash_rxq_type type = HASH_RXQ_TCPV4;

	assert(pos < table->hash_types_n);
	do {
		if ((table->hash_types & (1 << type)) && (pos-- == 0))
			break;
		++type;
	} while (1);
	return type;
}

/**
 * Filter out disabled hash RX queue types from ind_table_init[].
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] table
 *   Output table.
 *
 * @return
 *   Number of table entries.
 */
static unsigned int
priv_make_ind_table_init(struct priv *priv,
			 struct ind_table_init (*table)[IND_TABLE_INIT_N])
{
	uint64_t rss_hf;
	unsigned int i;
	unsigned int j;
	unsigned int table_n = 0;
	/* Mandatory to receive frames not handled by normal hash RX queues. */
	unsigned int hash_types_sup = 1 << HASH_RXQ_ETH;

	rss_hf = priv->rss_hf;
	/* Process other protocols only if more than one queue. */
	if (priv->rxqs_n > 1)
		for (i = 0; (i != hash_rxq_init_n); ++i)
			if (rss_hf & hash_rxq_init[i].dpdk_rss_hf)
				hash_types_sup |= (1 << i);

	/* Filter out entries whose protocols are not in the set. */
	for (i = 0, j = 0; (i != IND_TABLE_INIT_N); ++i) {
		unsigned int nb;
		unsigned int h;

		/* j is increased only if the table has valid protocols. */
		assert(j <= i);
		(*table)[j] = ind_table_init[i];
		(*table)[j].hash_types &= hash_types_sup;
		for (h = 0, nb = 0; (h != hash_rxq_init_n); ++h)
			if (((*table)[j].hash_types >> h) & 0x1)
				++nb;
		(*table)[i].hash_types_n = nb;
		if (nb) {
			++table_n;
			++j;
		}
	}
	return table_n;
}

/**
 * Initialize hash RX queues and indirection table.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_create_hash_rxqs(struct priv *priv)
{
	struct ibv_wq *wqs[priv->reta_idx_n];
	struct ind_table_init ind_table_init[IND_TABLE_INIT_N];
	unsigned int ind_tables_n =
		priv_make_ind_table_init(priv, &ind_table_init);
	unsigned int hash_rxqs_n = 0;
	struct hash_rxq (*hash_rxqs)[] = NULL;
	struct ibv_rwq_ind_table *(*ind_tables)[] = NULL;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	int err = 0;

	assert(priv->ind_tables == NULL);
	assert(priv->ind_tables_n == 0);
	assert(priv->hash_rxqs == NULL);
	assert(priv->hash_rxqs_n == 0);
	assert(priv->pd != NULL);
	assert(priv->ctx != NULL);
	if (priv->isolated)
		return 0;
	if (priv->rxqs_n == 0)
		return EINVAL;
	assert(priv->rxqs != NULL);
	if (ind_tables_n == 0) {
		ERROR("all hash RX queue types have been filtered out,"
		      " indirection table cannot be created");
		return EINVAL;
	}
	if (priv->rxqs_n & (priv->rxqs_n - 1)) {
		INFO("%u RX queues are configured, consider rounding this"
		     " number to the next power of two for better balancing",
		     priv->rxqs_n);
		DEBUG("indirection table extended to assume %u WQs",
		      priv->reta_idx_n);
	}
	for (i = 0; (i != priv->reta_idx_n); ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;

		rxq_ctrl = container_of((*priv->rxqs)[(*priv->reta_idx)[i]],
					struct mlx5_rxq_ctrl, rxq);
		wqs[i] = rxq_ctrl->ibv->wq;
	}
	/* Get number of hash RX queues to configure. */
	for (i = 0, hash_rxqs_n = 0; (i != ind_tables_n); ++i)
		hash_rxqs_n += ind_table_init[i].hash_types_n;
	DEBUG("allocating %u hash RX queues for %u WQs, %u indirection tables",
	      hash_rxqs_n, priv->rxqs_n, ind_tables_n);
	/* Create indirection tables. */
	ind_tables = rte_calloc(__func__, ind_tables_n,
				sizeof((*ind_tables)[0]), 0);
	if (ind_tables == NULL) {
		err = ENOMEM;
		ERROR("cannot allocate indirection tables container: %s",
		      strerror(err));
		goto error;
	}
	for (i = 0; (i != ind_tables_n); ++i) {
		struct ibv_rwq_ind_table_init_attr ind_init_attr = {
			.log_ind_tbl_size = 0, /* Set below. */
			.ind_tbl = wqs,
			.comp_mask = 0,
		};
		unsigned int ind_tbl_size = ind_table_init[i].max_size;
		struct ibv_rwq_ind_table *ind_table;

		if (priv->reta_idx_n < ind_tbl_size)
			ind_tbl_size = priv->reta_idx_n;
		ind_init_attr.log_ind_tbl_size = log2above(ind_tbl_size);
		errno = 0;
		ind_table = ibv_create_rwq_ind_table(priv->ctx,
						     &ind_init_attr);
		if (ind_table != NULL) {
			(*ind_tables)[i] = ind_table;
			continue;
		}
		/* Not clear whether errno is set. */
		err = (errno ? errno : EINVAL);
		ERROR("RX indirection table creation failed with error %d: %s",
		      err, strerror(err));
		goto error;
	}
	/* Allocate array that holds hash RX queues and related data. */
	hash_rxqs = rte_calloc(__func__, hash_rxqs_n,
			       sizeof((*hash_rxqs)[0]), 0);
	if (hash_rxqs == NULL) {
		err = ENOMEM;
		ERROR("cannot allocate hash RX queues container: %s",
		      strerror(err));
		goto error;
	}
	for (i = 0, j = 0, k = 0;
	     ((i != hash_rxqs_n) && (j != ind_tables_n));
	     ++i) {
		struct hash_rxq *hash_rxq = &(*hash_rxqs)[i];
		enum hash_rxq_type type =
			hash_rxq_type_from_pos(&ind_table_init[j], k);
		struct rte_eth_rss_conf *priv_rss_conf =
			(*priv->rss_conf)[type];
		struct ibv_rx_hash_conf hash_conf = {
			.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
			.rx_hash_key_len = (priv_rss_conf ?
					    priv_rss_conf->rss_key_len :
					    rss_hash_default_key_len),
			.rx_hash_key = (priv_rss_conf ?
					priv_rss_conf->rss_key :
					rss_hash_default_key),
			.rx_hash_fields_mask = hash_rxq_init[type].hash_fields,
		};
		struct ibv_qp_init_attr_ex qp_init_attr = {
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask = (IBV_QP_INIT_ATTR_PD |
				      IBV_QP_INIT_ATTR_IND_TABLE |
				      IBV_QP_INIT_ATTR_RX_HASH),
			.rx_hash_conf = hash_conf,
			.rwq_ind_tbl = (*ind_tables)[j],
			.pd = priv->pd,
		};

		DEBUG("using indirection table %u for hash RX queue %u type %d",
		      j, i, type);
		*hash_rxq = (struct hash_rxq){
			.priv = priv,
			.qp = ibv_create_qp_ex(priv->ctx, &qp_init_attr),
			.type = type,
		};
		if (hash_rxq->qp == NULL) {
			err = (errno ? errno : EINVAL);
			ERROR("Hash RX QP creation failure: %s",
			      strerror(err));
			goto error;
		}
		if (++k < ind_table_init[j].hash_types_n)
			continue;
		/* Switch to the next indirection table and reset hash RX
		 * queue type array index. */
		++j;
		k = 0;
	}
	priv->ind_tables = ind_tables;
	priv->ind_tables_n = ind_tables_n;
	priv->hash_rxqs = hash_rxqs;
	priv->hash_rxqs_n = hash_rxqs_n;
	assert(err == 0);
	return 0;
error:
	if (hash_rxqs != NULL) {
		for (i = 0; (i != hash_rxqs_n); ++i) {
			struct ibv_qp *qp = (*hash_rxqs)[i].qp;

			if (qp == NULL)
				continue;
			claim_zero(ibv_destroy_qp(qp));
		}
		rte_free(hash_rxqs);
	}
	if (ind_tables != NULL) {
		for (j = 0; (j != ind_tables_n); ++j) {
			struct ibv_rwq_ind_table *ind_table =
				(*ind_tables)[j];

			if (ind_table == NULL)
				continue;
			claim_zero(ibv_destroy_rwq_ind_table(ind_table));
		}
		rte_free(ind_tables);
	}
	return err;
}

/**
 * Clean up hash RX queues and indirection table.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_destroy_hash_rxqs(struct priv *priv)
{
	unsigned int i;

	DEBUG("destroying %u hash RX queues", priv->hash_rxqs_n);
	if (priv->hash_rxqs_n == 0) {
		assert(priv->hash_rxqs == NULL);
		assert(priv->ind_tables == NULL);
		return;
	}
	for (i = 0; (i != priv->hash_rxqs_n); ++i) {
		struct hash_rxq *hash_rxq = &(*priv->hash_rxqs)[i];
		unsigned int j, k;

		assert(hash_rxq->priv == priv);
		assert(hash_rxq->qp != NULL);
		for (j = 0; (j != RTE_DIM(hash_rxq->mac_flow)); ++j)
			for (k = 0; (k != RTE_DIM(hash_rxq->mac_flow[j])); ++k)
				assert(hash_rxq->mac_flow[j][k] == NULL);
		claim_zero(ibv_destroy_qp(hash_rxq->qp));
	}
	priv->hash_rxqs_n = 0;
	rte_free(priv->hash_rxqs);
	priv->hash_rxqs = NULL;
	for (i = 0; (i != priv->ind_tables_n); ++i) {
		struct ibv_rwq_ind_table *ind_table =
			(*priv->ind_tables)[i];

		assert(ind_table != NULL);
		claim_zero(ibv_destroy_rwq_ind_table(ind_table));
	}
	priv->ind_tables_n = 0;
	rte_free(priv->ind_tables);
	priv->ind_tables = NULL;
}

/**
 * Allocate RX queue elements.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
rxq_alloc_elts(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	const unsigned int sges_n = 1 << rxq_ctrl->rxq.sges_n;
	unsigned int elts_n = 1 << rxq_ctrl->rxq.elts_n;
	unsigned int i;
	int ret = 0;

	/* Iterate on segments. */
	for (i = 0; (i != elts_n); ++i) {
		struct rte_mbuf *buf;

		buf = rte_pktmbuf_alloc(rxq_ctrl->rxq.mp);
		if (buf == NULL) {
			ERROR("%p: empty mbuf pool", (void *)rxq_ctrl);
			ret = ENOMEM;
			goto error;
		}
		/* Headroom is reserved by rte_pktmbuf_alloc(). */
		assert(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		assert(rte_pktmbuf_data_len(buf) == 0);
		assert(rte_pktmbuf_pkt_len(buf) == 0);
		assert(!buf->next);
		/* Only the first segment keeps headroom. */
		if (i % sges_n)
			SET_DATA_OFF(buf, 0);
		PORT(buf) = rxq_ctrl->rxq.port_id;
		DATA_LEN(buf) = rte_pktmbuf_tailroom(buf);
		PKT_LEN(buf) = DATA_LEN(buf);
		NB_SEGS(buf) = 1;
		(*rxq_ctrl->rxq.elts)[i] = buf;
	}
	/* If Rx vector is activated. */
	if (rxq_check_vec_support(&rxq_ctrl->rxq) > 0) {
		struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
		struct rte_mbuf *mbuf_init = &rxq->fake_mbuf;
		int j;

		/* Initialize default rearm_data for vPMD. */
		mbuf_init->data_off = RTE_PKTMBUF_HEADROOM;
		rte_mbuf_refcnt_set(mbuf_init, 1);
		mbuf_init->nb_segs = 1;
		mbuf_init->port = rxq->port_id;
		/*
		 * prevent compiler reordering:
		 * rearm_data covers previous fields.
		 */
		rte_compiler_barrier();
		rxq->mbuf_initializer =
			*(uint64_t *)&mbuf_init->rearm_data;
		/* Padding with a fake mbuf for vectorized Rx. */
		for (j = 0; j < MLX5_VPMD_DESCS_PER_LOOP; ++j)
			(*rxq->elts)[elts_n + j] = &rxq->fake_mbuf;
	}
	DEBUG("%p: allocated and configured %u segments (max %u packets)",
	      (void *)rxq_ctrl, elts_n, elts_n / (1 << rxq_ctrl->rxq.sges_n));
	assert(ret == 0);
	return 0;
error:
	elts_n = i;
	for (i = 0; (i != elts_n); ++i) {
		if ((*rxq_ctrl->rxq.elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq_ctrl->rxq.elts)[i]);
		(*rxq_ctrl->rxq.elts)[i] = NULL;
	}
	DEBUG("%p: failed, freed everything", (void *)rxq_ctrl);
	assert(ret > 0);
	return ret;
}

/**
 * Free RX queue elements.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
	const uint16_t q_n = (1 << rxq->elts_n);
	const uint16_t q_mask = q_n - 1;
	uint16_t used = q_n - (rxq->rq_ci - rxq->rq_pi);
	uint16_t i;

	DEBUG("%p: freeing WRs", (void *)rxq_ctrl);
	if (rxq->elts == NULL)
		return;
	/**
	 * Some mbuf in the Ring belongs to the application.  They cannot be
	 * freed.
	 */
	if (rxq_check_vec_support(rxq) > 0) {
		for (i = 0; i < used; ++i)
			(*rxq->elts)[(rxq->rq_ci + i) & q_mask] = NULL;
		rxq->rq_pi = rxq->rq_ci;
	}
	for (i = 0; (i != (1u << rxq->elts_n)); ++i) {
		if ((*rxq->elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq->elts)[i]);
		(*rxq->elts)[i] = NULL;
	}
}

/**
 * Clean up a RX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
void
mlx5_rxq_cleanup(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	DEBUG("cleaning up %p", (void *)rxq_ctrl);
	if (rxq_ctrl->ibv)
		mlx5_priv_rxq_ibv_release(rxq_ctrl->priv, rxq_ctrl->ibv);
	memset(rxq_ctrl, 0, sizeof(*rxq_ctrl));
}

/**
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	int ret = 0;

	(void)conf;
	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;
	priv_lock(priv);
	if (!rte_is_power_of_2(desc)) {
		desc = 1 << log2above(desc);
		WARN("%p: increased number of descriptors in RX queue %u"
		     " to the next power of two (%d)",
		     (void *)dev, idx, desc);
	}
	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= priv->rxqs_n) {
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, priv->rxqs_n);
		priv_unlock(priv);
		return -EOVERFLOW;
	}
	if (!mlx5_priv_rxq_releasable(priv, idx)) {
		ret = EBUSY;
		ERROR("%p: unable to release queue index %u",
		      (void *)dev, idx);
		goto out;
	}
	mlx5_priv_rxq_release(priv, idx);
	rxq_ctrl = mlx5_priv_rxq_new(priv, idx, desc, socket, mp);
	if (!rxq_ctrl) {
		ERROR("%p: unable to allocate queue index %u",
		      (void *)dev, idx);
		ret = ENOMEM;
		goto out;
	}
	DEBUG("%p: adding RX queue %p to list",
	      (void *)dev, (void *)rxq_ctrl);
	(*priv->rxqs)[idx] = &rxq_ctrl->rxq;
out:
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to release a RX queue.
 *
 * @param dpdk_rxq
 *   Generic RX queue pointer.
 */
void
mlx5_rx_queue_release(void *dpdk_rxq)
{
	struct mlx5_rxq_data *rxq = (struct mlx5_rxq_data *)dpdk_rxq;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct priv *priv;

	if (mlx5_is_secondary())
		return;

	if (rxq == NULL)
		return;
	rxq_ctrl = container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	priv = rxq_ctrl->priv;
	priv_lock(priv);
	if (!mlx5_priv_rxq_releasable(priv, rxq_ctrl->rxq.stats.idx))
		rte_panic("Rx queue %p is still used by a flow and cannot be"
			  " removed\n", (void *)rxq_ctrl);
	mlx5_priv_rxq_release(priv, rxq_ctrl->rxq.stats.idx);
	priv_unlock(priv);
}

/**
 * Allocate queue vector and fill epoll fd list for Rx interrupts.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative on failure.
 */
int
priv_rx_intr_vec_enable(struct priv *priv)
{
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	unsigned int count = 0;
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	assert(!mlx5_is_secondary());
	if (!priv->dev->data->dev_conf.intr_conf.rxq)
		return 0;
	priv_rx_intr_vec_disable(priv);
	intr_handle->intr_vec = malloc(sizeof(intr_handle->intr_vec[rxqs_n]));
	if (intr_handle->intr_vec == NULL) {
		ERROR("failed to allocate memory for interrupt vector,"
		      " Rx interrupts will not be supported");
		return -ENOMEM;
	}
	intr_handle->type = RTE_INTR_HANDLE_EXT;
	for (i = 0; i != n; ++i) {
		/* This rxq ibv must not be released in this function. */
		struct mlx5_rxq_ibv *rxq_ibv = mlx5_priv_rxq_ibv_get(priv, i);
		int fd;
		int flags;
		int rc;

		/* Skip queues that cannot request interrupts. */
		if (!rxq_ibv || !rxq_ibv->channel) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		if (count >= RTE_MAX_RXTX_INTR_VEC_ID) {
			ERROR("too many Rx queues for interrupt vector size"
			      " (%d), Rx interrupts cannot be enabled",
			      RTE_MAX_RXTX_INTR_VEC_ID);
			priv_rx_intr_vec_disable(priv);
			return -1;
		}
		fd = rxq_ibv->channel->fd;
		flags = fcntl(fd, F_GETFL);
		rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		if (rc < 0) {
			ERROR("failed to make Rx interrupt file descriptor"
			      " %d non-blocking for queue index %d", fd, i);
			priv_rx_intr_vec_disable(priv);
			return -1;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = fd;
		count++;
	}
	if (!count)
		priv_rx_intr_vec_disable(priv);
	else
		intr_handle->nb_efd = count;
	return 0;
}

/**
 * Clean up Rx interrupts handler.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_rx_intr_vec_disable(struct priv *priv)
{
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);

	if (!priv->dev->data->dev_conf.intr_conf.rxq)
		return;
	for (i = 0; i != n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_rxq_data *rxq_data;

		if (intr_handle->intr_vec[i] == RTE_INTR_VEC_RXTX_OFFSET +
		    RTE_MAX_RXTX_INTR_VEC_ID)
			continue;
		/**
		 * Need to access directly the queue to release the reference
		 * kept in priv_rx_intr_vec_enable().
		 */
		rxq_data = (*priv->rxqs)[i];
		rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		mlx5_priv_rxq_ibv_release(priv, rxq_ctrl->ibv);
	}
	rte_intr_free_epoll_fd(intr_handle);
	free(intr_handle->intr_vec);
	intr_handle->nb_efd = 0;
	intr_handle->intr_vec = NULL;
}

/**
 *  MLX5 CQ notification .
 *
 *  @param rxq
 *     Pointer to receive queue structure.
 *  @param sq_n_rxq
 *     Sequence number per receive queue .
 */
static inline void
mlx5_arm_cq(struct mlx5_rxq_data *rxq, int sq_n_rxq)
{
	int sq_n = 0;
	uint32_t doorbell_hi;
	uint64_t doorbell;
	void *cq_db_reg = (char *)rxq->cq_uar + MLX5_CQ_DOORBELL;

	sq_n = sq_n_rxq & MLX5_CQ_SQN_MASK;
	doorbell_hi = sq_n << MLX5_CQ_SQN_OFFSET | (rxq->cq_ci & MLX5_CI_MASK);
	doorbell = (uint64_t)doorbell_hi << 32;
	doorbell |=  rxq->cqn;
	rxq->cq_db[MLX5_CQ_ARM_DB] = rte_cpu_to_be_32(doorbell_hi);
	rte_wmb();
	rte_write64(rte_cpu_to_be_64(doorbell), cq_db_reg);
}

/**
 * DPDK callback for Rx queue interrupt enable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Rx queue number.
 *
 * @return
 *   0 on success, negative on failure.
 */
int
mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct mlx5_rxq_data *rxq_data;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	int ret = 0;

	priv_lock(priv);
	rxq_data = (*priv->rxqs)[rx_queue_id];
	if (!rxq_data) {
		ret = EINVAL;
		goto exit;
	}
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (rxq_ctrl->irq) {
		struct mlx5_rxq_ibv *rxq_ibv;

		rxq_ibv = mlx5_priv_rxq_ibv_get(priv, rx_queue_id);
		if (!rxq_ibv) {
			ret = EINVAL;
			goto exit;
		}
		mlx5_arm_cq(rxq_data, rxq_data->cq_arm_sn);
		mlx5_priv_rxq_ibv_release(priv, rxq_ibv);
	}
exit:
	priv_unlock(priv);
	if (ret)
		WARN("unable to arm interrupt on rx queue %d", rx_queue_id);
	return -ret;
}

/**
 * DPDK callback for Rx queue interrupt disable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Rx queue number.
 *
 * @return
 *   0 on success, negative on failure.
 */
int
mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct mlx5_rxq_data *rxq_data;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct mlx5_rxq_ibv *rxq_ibv = NULL;
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret = 0;

	priv_lock(priv);
	rxq_data = (*priv->rxqs)[rx_queue_id];
	if (!rxq_data) {
		ret = EINVAL;
		goto exit;
	}
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (!rxq_ctrl->irq)
		goto exit;
	rxq_ibv = mlx5_priv_rxq_ibv_get(priv, rx_queue_id);
	if (!rxq_ibv) {
		ret = EINVAL;
		goto exit;
	}
	ret = ibv_get_cq_event(rxq_ibv->channel, &ev_cq, &ev_ctx);
	if (ret || ev_cq != rxq_ibv->cq) {
		ret = EINVAL;
		goto exit;
	}
	rxq_data->cq_arm_sn++;
	ibv_ack_cq_events(rxq_ibv->cq, 1);
exit:
	if (rxq_ibv)
		mlx5_priv_rxq_ibv_release(priv, rxq_ibv);
	priv_unlock(priv);
	if (ret)
		WARN("unable to disable interrupt on rx queue %d",
		     rx_queue_id);
	return -ret;
}

/**
 * Create the Rx queue Verbs object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object initialised if it can be created.
 */
struct mlx5_rxq_ibv*
mlx5_priv_rxq_ibv_new(struct priv *priv, uint16_t idx)
{
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	union {
		struct ibv_cq_init_attr_ex cq;
		struct ibv_wq_init_attr wq;
		struct ibv_cq_ex cq_attr;
	} attr;
	unsigned int cqe_n = (1 << rxq_data->elts_n) - 1;
	struct mlx5_rxq_ibv *tmpl;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_rwq rwq;
	unsigned int i;
	int ret = 0;
	struct mlx5dv_obj obj;

	assert(rxq_data);
	assert(!rxq_ctrl->ibv);
	tmpl = rte_calloc_socket(__func__, 1, sizeof(*tmpl), 0,
				 rxq_ctrl->socket);
	if (!tmpl) {
		ERROR("%p: cannot allocate verbs resources",
		       (void *)rxq_ctrl);
		goto error;
	}
	tmpl->rxq_ctrl = rxq_ctrl;
	/* Use the entire RX mempool as the memory region. */
	tmpl->mr = priv_mr_get(priv, rxq_data->mp);
	if (!tmpl->mr) {
		tmpl->mr = priv_mr_new(priv, rxq_data->mp);
		if (!tmpl->mr) {
			ERROR("%p: MR creation failure", (void *)rxq_ctrl);
			goto error;
		}
	}
	if (rxq_ctrl->irq) {
		tmpl->channel = ibv_create_comp_channel(priv->ctx);
		if (!tmpl->channel) {
			ERROR("%p: Comp Channel creation failure",
			      (void *)rxq_ctrl);
			goto error;
		}
	}
	attr.cq = (struct ibv_cq_init_attr_ex){
		.comp_mask = 0,
	};
	if (priv->cqe_comp) {
		attr.cq.comp_mask |= IBV_CQ_INIT_ATTR_MASK_FLAGS;
		attr.cq.flags |= MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (rxq_check_vec_support(rxq_data) < 0)
			cqe_n *= 2;
	}
	tmpl->cq = ibv_create_cq(priv->ctx, cqe_n, NULL, tmpl->channel, 0);
	if (tmpl->cq == NULL) {
		ERROR("%p: CQ creation failure", (void *)rxq_ctrl);
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.orig_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.orig_attr.max_sge);
	attr.wq = (struct ibv_wq_init_attr){
		.wq_context = NULL, /* Could be useful in the future. */
		.wq_type = IBV_WQT_RQ,
		/* Max number of outstanding WRs. */
		.max_wr = (1 << rxq_data->elts_n) >> rxq_data->sges_n,
		/* Max number of scatter/gather elements in a WR. */
		.max_sge = 1 << rxq_data->sges_n,
		.pd = priv->pd,
		.cq = tmpl->cq,
		.comp_mask =
			IBV_WQ_FLAGS_CVLAN_STRIPPING |
			0,
		.create_flags = (rxq_data->vlan_strip ?
				 IBV_WQ_FLAGS_CVLAN_STRIPPING :
				 0),
	};
	/* By default, FCS (CRC) is stripped by hardware. */
	if (rxq_data->crc_present) {
		attr.wq.create_flags |= IBV_WQ_FLAGS_SCATTER_FCS;
		attr.wq.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
	}
#ifdef HAVE_IBV_WQ_FLAG_RX_END_PADDING
	if (priv->hw_padding) {
		attr.wq.create_flags |= IBV_WQ_FLAG_RX_END_PADDING;
		attr.wq.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
	}
#endif
	tmpl->wq = ibv_create_wq(priv->ctx, &attr.wq);
	if (tmpl->wq == NULL) {
		ERROR("%p: WQ creation failure", (void *)rxq_ctrl);
		goto error;
	}
	/*
	 * Make sure number of WRs*SGEs match expectations since a queue
	 * cannot allocate more than "desc" buffers.
	 */
	if (((int)attr.wq.max_wr !=
	     ((1 << rxq_data->elts_n) >> rxq_data->sges_n)) ||
	    ((int)attr.wq.max_sge != (1 << rxq_data->sges_n))) {
		ERROR("%p: requested %u*%u but got %u*%u WRs*SGEs",
		      (void *)rxq_ctrl,
		      ((1 << rxq_data->elts_n) >> rxq_data->sges_n),
		      (1 << rxq_data->sges_n),
		      attr.wq.max_wr, attr.wq.max_sge);
		goto error;
	}
	/* Change queue state to ready. */
	mod = (struct ibv_wq_attr){
		.attr_mask = IBV_WQ_ATTR_STATE,
		.wq_state = IBV_WQS_RDY,
	};
	ret = ibv_modify_wq(tmpl->wq, &mod);
	if (ret) {
		ERROR("%p: WQ state to IBV_WQS_RDY failed",
		      (void *)rxq_ctrl);
		goto error;
	}
	obj.cq.in = tmpl->cq;
	obj.cq.out = &cq_info;
	obj.rwq.in = tmpl->wq;
	obj.rwq.out = &rwq;
	ret = mlx5dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_RWQ);
	if (ret != 0)
		goto error;
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		ERROR("Wrong MLX5_CQE_SIZE environment variable value: "
		      "it should be set to %u", RTE_CACHE_LINE_SIZE);
		goto error;
	}
	/* Fill the rings. */
	rxq_data->wqes = (volatile struct mlx5_wqe_data_seg (*)[])
		(uintptr_t)rwq.buf;
	for (i = 0; (i != (unsigned int)(1 << rxq_data->elts_n)); ++i) {
		struct rte_mbuf *buf = (*rxq_data->elts)[i];
		volatile struct mlx5_wqe_data_seg *scat = &(*rxq_data->wqes)[i];

		/* scat->addr must be able to store a pointer. */
		assert(sizeof(scat->addr) >= sizeof(uintptr_t));
		*scat = (struct mlx5_wqe_data_seg){
			.addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(buf,
								  uintptr_t)),
			.byte_count = rte_cpu_to_be_32(DATA_LEN(buf)),
			.lkey = tmpl->mr->lkey,
		};
	}
	rxq_data->rq_db = rwq.dbrec;
	rxq_data->cqe_n = log2above(cq_info.cqe_cnt);
	rxq_data->cq_ci = 0;
	rxq_data->rq_ci = 0;
	rxq_data->rq_pi = 0;
	rxq_data->zip = (struct rxq_zip){
		.ai = 0,
	};
	rxq_data->cq_db = cq_info.dbrec;
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)cq_info.buf;
	/* Update doorbell counter. */
	rxq_data->rq_ci = (1 << rxq_data->elts_n) >> rxq_data->sges_n;
	rte_wmb();
	*rxq_data->rq_db = rte_cpu_to_be_32(rxq_data->rq_ci);
	DEBUG("%p: rxq updated with %p", (void *)rxq_ctrl, (void *)&tmpl);
	rte_atomic32_inc(&tmpl->refcnt);
	DEBUG("%p: Verbs Rx queue %p: refcnt %d", (void *)priv,
	      (void *)tmpl, rte_atomic32_read(&tmpl->refcnt));
	LIST_INSERT_HEAD(&priv->rxqsibv, tmpl, next);
	return tmpl;
error:
	if (tmpl->wq)
		claim_zero(ibv_destroy_wq(tmpl->wq));
	if (tmpl->cq)
		claim_zero(ibv_destroy_cq(tmpl->cq));
	if (tmpl->channel)
		claim_zero(ibv_destroy_comp_channel(tmpl->channel));
	if (tmpl->mr)
		priv_mr_release(priv, tmpl->mr);
	return NULL;
}

/**
 * Get an Rx queue Verbs object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object if it exists.
 */
struct mlx5_rxq_ibv*
mlx5_priv_rxq_ibv_get(struct priv *priv, uint16_t idx)
{
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (idx >= priv->rxqs_n)
		return NULL;
	if (!rxq_data)
		return NULL;
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (rxq_ctrl->ibv) {
		priv_mr_get(priv, rxq_data->mp);
		rte_atomic32_inc(&rxq_ctrl->ibv->refcnt);
		DEBUG("%p: Verbs Rx queue %p: refcnt %d", (void *)priv,
		      (void *)rxq_ctrl->ibv,
		      rte_atomic32_read(&rxq_ctrl->ibv->refcnt));
	}
	return rxq_ctrl->ibv;
}

/**
 * Release an Rx verbs queue object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param rxq_ibv
 *   Verbs Rx queue object.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_priv_rxq_ibv_release(struct priv *priv, struct mlx5_rxq_ibv *rxq_ibv)
{
	int ret;

	assert(rxq_ibv);
	assert(rxq_ibv->wq);
	assert(rxq_ibv->cq);
	assert(rxq_ibv->mr);
	ret = priv_mr_release(priv, rxq_ibv->mr);
	if (!ret)
		rxq_ibv->mr = NULL;
	DEBUG("%p: Verbs Rx queue %p: refcnt %d", (void *)priv,
	      (void *)rxq_ibv, rte_atomic32_read(&rxq_ibv->refcnt));
	if (rte_atomic32_dec_and_test(&rxq_ibv->refcnt)) {
		rxq_free_elts(rxq_ibv->rxq_ctrl);
		claim_zero(ibv_destroy_wq(rxq_ibv->wq));
		claim_zero(ibv_destroy_cq(rxq_ibv->cq));
		if (rxq_ibv->channel)
			claim_zero(ibv_destroy_comp_channel(rxq_ibv->channel));
		LIST_REMOVE(rxq_ibv, next);
		rte_free(rxq_ibv);
		return 0;
	}
	return EBUSY;
}

/**
 * Verify the Verbs Rx queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_rxq_ibv_verify(struct priv *priv)
{
	int ret = 0;
	struct mlx5_rxq_ibv *rxq_ibv;

	LIST_FOREACH(rxq_ibv, &priv->rxqsibv, next) {
		DEBUG("%p: Verbs Rx queue %p still referenced", (void *)priv,
		      (void *)rxq_ibv);
		++ret;
	}
	return ret;
}

/**
 * Return true if a single reference exists on the object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param rxq_ibv
 *   Verbs Rx queue object.
 */
int
mlx5_priv_rxq_ibv_releasable(struct priv *priv, struct mlx5_rxq_ibv *rxq_ibv)
{
	(void)priv;
	assert(rxq_ibv);
	return (rte_atomic32_read(&rxq_ibv->refcnt) == 1);
}

/**
 * Create a DPDK Rx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 *
 * @return
 *   A DPDK queue object on success.
 */
struct mlx5_rxq_ctrl*
mlx5_priv_rxq_new(struct priv *priv, uint16_t idx, uint16_t desc,
		  unsigned int socket, struct rte_mempool *mp)
{
	struct rte_eth_dev *dev = priv->dev;
	struct mlx5_rxq_ctrl *tmpl;
	const uint16_t desc_n =
		desc + priv->rx_vec_en * MLX5_VPMD_DESCS_PER_LOOP;
	unsigned int mb_len = rte_pktmbuf_data_room_size(mp);

	tmpl = rte_calloc_socket("RXQ", 1,
				 sizeof(*tmpl) +
				 desc_n * sizeof(struct rte_mbuf *),
				 0, socket);
	if (!tmpl)
		return NULL;
	if (priv->dev->data->dev_conf.intr_conf.rxq)
		tmpl->irq = 1;
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
	    (mb_len - RTE_PKTMBUF_HEADROOM)) {
		tmpl->rxq.sges_n = 0;
	} else if (dev->data->dev_conf.rxmode.enable_scatter) {
		unsigned int size =
			RTE_PKTMBUF_HEADROOM +
			dev->data->dev_conf.rxmode.max_rx_pkt_len;
		unsigned int sges_n;

		/*
		 * Determine the number of SGEs needed for a full packet
		 * and round it to the next power of two.
		 */
		sges_n = log2above((size / mb_len) + !!(size % mb_len));
		tmpl->rxq.sges_n = sges_n;
		/* Make sure rxq.sges_n did not overflow. */
		size = mb_len * (1 << tmpl->rxq.sges_n);
		size -= RTE_PKTMBUF_HEADROOM;
		if (size < dev->data->dev_conf.rxmode.max_rx_pkt_len) {
			ERROR("%p: too many SGEs (%u) needed to handle"
			      " requested maximum packet size %u",
			      (void *)dev,
			      1 << sges_n,
			      dev->data->dev_conf.rxmode.max_rx_pkt_len);
			goto error;
		}
	} else {
		WARN("%p: the requested maximum Rx packet size (%u) is"
		     " larger than a single mbuf (%u) and scattered"
		     " mode has not been requested",
		     (void *)dev,
		     dev->data->dev_conf.rxmode.max_rx_pkt_len,
		     mb_len - RTE_PKTMBUF_HEADROOM);
	}
	DEBUG("%p: maximum number of segments per packet: %u",
	      (void *)dev, 1 << tmpl->rxq.sges_n);
	if (desc % (1 << tmpl->rxq.sges_n)) {
		ERROR("%p: number of RX queue descriptors (%u) is not a"
		      " multiple of SGEs per packet (%u)",
		      (void *)dev,
		      desc,
		      1 << tmpl->rxq.sges_n);
		goto error;
	}
	/* Toggle RX checksum offload if hardware supports it. */
	if (priv->hw_csum)
		tmpl->rxq.csum = !!dev->data->dev_conf.rxmode.hw_ip_checksum;
	if (priv->hw_csum_l2tun)
		tmpl->rxq.csum_l2tun =
			!!dev->data->dev_conf.rxmode.hw_ip_checksum;
	/* Configure VLAN stripping. */
	tmpl->rxq.vlan_strip = (priv->hw_vlan_strip &&
			       !!dev->data->dev_conf.rxmode.hw_vlan_strip);
	/* By default, FCS (CRC) is stripped by hardware. */
	if (dev->data->dev_conf.rxmode.hw_strip_crc) {
		tmpl->rxq.crc_present = 0;
	} else if (priv->hw_fcs_strip) {
		tmpl->rxq.crc_present = 1;
	} else {
		WARN("%p: CRC stripping has been disabled but will still"
		     " be performed by hardware, make sure MLNX_OFED and"
		     " firmware are up to date",
		     (void *)dev);
		tmpl->rxq.crc_present = 0;
	}
	DEBUG("%p: CRC stripping is %s, %u bytes will be subtracted from"
	      " incoming frames to hide it",
	      (void *)dev,
	      tmpl->rxq.crc_present ? "disabled" : "enabled",
	      tmpl->rxq.crc_present << 2);
	/* Save port ID. */
	tmpl->rxq.rss_hash = priv->rxqs_n > 1;
	tmpl->rxq.port_id = dev->data->port_id;
	tmpl->priv = priv;
	tmpl->rxq.mp = mp;
	tmpl->rxq.stats.idx = idx;
	tmpl->rxq.elts_n = log2above(desc);
	tmpl->rxq.elts =
		(struct rte_mbuf *(*)[1 << tmpl->rxq.elts_n])(tmpl + 1);
	rte_atomic32_inc(&tmpl->refcnt);
	DEBUG("%p: Rx queue %p: refcnt %d", (void *)priv,
	      (void *)tmpl, rte_atomic32_read(&tmpl->refcnt));
	LIST_INSERT_HEAD(&priv->rxqsctrl, tmpl, next);
	return tmpl;
error:
	rte_free(tmpl);
	return NULL;
}

/**
 * Get a Rx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   A pointer to the queue if it exists.
 */
struct mlx5_rxq_ctrl*
mlx5_priv_rxq_get(struct priv *priv, uint16_t idx)
{
	struct mlx5_rxq_ctrl *rxq_ctrl = NULL;

	if ((*priv->rxqs)[idx]) {
		rxq_ctrl = container_of((*priv->rxqs)[idx],
					struct mlx5_rxq_ctrl,
					rxq);

		mlx5_priv_rxq_ibv_get(priv, idx);
		rte_atomic32_inc(&rxq_ctrl->refcnt);
		DEBUG("%p: Rx queue %p: refcnt %d", (void *)priv,
		      (void *)rxq_ctrl, rte_atomic32_read(&rxq_ctrl->refcnt));
	}
	return rxq_ctrl;
}

/**
 * Release a Rx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_priv_rxq_release(struct priv *priv, uint16_t idx)
{
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (!(*priv->rxqs)[idx])
		return 0;
	rxq_ctrl = container_of((*priv->rxqs)[idx], struct mlx5_rxq_ctrl, rxq);
	assert(rxq_ctrl->priv);
	if (rxq_ctrl->ibv) {
		int ret;

		ret = mlx5_priv_rxq_ibv_release(rxq_ctrl->priv, rxq_ctrl->ibv);
		if (!ret)
			rxq_ctrl->ibv = NULL;
	}
	DEBUG("%p: Rx queue %p: refcnt %d", (void *)priv,
	      (void *)rxq_ctrl, rte_atomic32_read(&rxq_ctrl->refcnt));
	if (rte_atomic32_dec_and_test(&rxq_ctrl->refcnt)) {
		LIST_REMOVE(rxq_ctrl, next);
		rte_free(rxq_ctrl);
		(*priv->rxqs)[idx] = NULL;
		return 0;
	}
	return EBUSY;
}

/**
 * Verify if the queue can be released.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   1 if the queue can be released.
 */
int
mlx5_priv_rxq_releasable(struct priv *priv, uint16_t idx)
{
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (!(*priv->rxqs)[idx])
		return -1;
	rxq_ctrl = container_of((*priv->rxqs)[idx], struct mlx5_rxq_ctrl, rxq);
	return (rte_atomic32_read(&rxq_ctrl->refcnt) == 1);
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_rxq_verify(struct priv *priv)
{
	struct mlx5_rxq_ctrl *rxq_ctrl;
	int ret = 0;

	LIST_FOREACH(rxq_ctrl, &priv->rxqsctrl, next) {
		DEBUG("%p: Rx Queue %p still referenced", (void *)priv,
		      (void *)rxq_ctrl);
		++ret;
	}
	return ret;
}

/**
 * Create an indirection table.
 *
 * @param priv
 *   Pointer to private structure.
 * @param queues
 *   Queues entering in the indirection table.
 * @param queues_n
 *   Number of queues in the array.
 *
 * @return
 *   A new indirection table.
 */
struct mlx5_ind_table_ibv*
mlx5_priv_ind_table_ibv_new(struct priv *priv, uint16_t queues[],
			    uint16_t queues_n)
{
	struct mlx5_ind_table_ibv *ind_tbl;
	const unsigned int wq_n = rte_is_power_of_2(queues_n) ?
		log2above(queues_n) :
		priv->ind_table_max_size;
	struct ibv_wq *wq[1 << wq_n];
	unsigned int i;
	unsigned int j;

	ind_tbl = rte_calloc(__func__, 1, sizeof(*ind_tbl) +
			     queues_n * sizeof(uint16_t), 0);
	if (!ind_tbl)
		return NULL;
	for (i = 0; i != queues_n; ++i) {
		struct mlx5_rxq_ctrl *rxq =
			mlx5_priv_rxq_get(priv, queues[i]);

		if (!rxq)
			goto error;
		wq[i] = rxq->ibv->wq;
		ind_tbl->queues[i] = queues[i];
	}
	ind_tbl->queues_n = queues_n;
	/* Finalise indirection table. */
	for (j = 0; i != (unsigned int)(1 << wq_n); ++i, ++j)
		wq[i] = wq[j];
	ind_tbl->ind_table = ibv_create_rwq_ind_table(
		priv->ctx,
		&(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = wq_n,
			.ind_tbl = wq,
			.comp_mask = 0,
		});
	if (!ind_tbl->ind_table)
		goto error;
	rte_atomic32_inc(&ind_tbl->refcnt);
	LIST_INSERT_HEAD(&priv->ind_tbls, ind_tbl, next);
	DEBUG("%p: Indirection table %p: refcnt %d", (void *)priv,
	      (void *)ind_tbl, rte_atomic32_read(&ind_tbl->refcnt));
	return ind_tbl;
error:
	rte_free(ind_tbl);
	DEBUG("%p cannot create indirection table", (void *)priv);
	return NULL;
}

/**
 * Get an indirection table.
 *
 * @param priv
 *   Pointer to private structure.
 * @param queues
 *   Queues entering in the indirection table.
 * @param queues_n
 *   Number of queues in the array.
 *
 * @return
 *   An indirection table if found.
 */
struct mlx5_ind_table_ibv*
mlx5_priv_ind_table_ibv_get(struct priv *priv, uint16_t queues[],
			    uint16_t queues_n)
{
	struct mlx5_ind_table_ibv *ind_tbl;

	LIST_FOREACH(ind_tbl, &priv->ind_tbls, next) {
		if ((ind_tbl->queues_n == queues_n) &&
		    (memcmp(ind_tbl->queues, queues,
			    ind_tbl->queues_n * sizeof(ind_tbl->queues[0]))
		     == 0))
			break;
	}
	if (ind_tbl) {
		unsigned int i;

		rte_atomic32_inc(&ind_tbl->refcnt);
		DEBUG("%p: Indirection table %p: refcnt %d", (void *)priv,
		      (void *)ind_tbl, rte_atomic32_read(&ind_tbl->refcnt));
		for (i = 0; i != ind_tbl->queues_n; ++i)
			mlx5_priv_rxq_get(priv, ind_tbl->queues[i]);
	}
	return ind_tbl;
}

/**
 * Release an indirection table.
 *
 * @param priv
 *   Pointer to private structure.
 * @param ind_table
 *   Indirection table to release.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_priv_ind_table_ibv_release(struct priv *priv,
				struct mlx5_ind_table_ibv *ind_tbl)
{
	unsigned int i;

	DEBUG("%p: Indirection table %p: refcnt %d", (void *)priv,
	      (void *)ind_tbl, rte_atomic32_read(&ind_tbl->refcnt));
	if (rte_atomic32_dec_and_test(&ind_tbl->refcnt))
		claim_zero(ibv_destroy_rwq_ind_table(ind_tbl->ind_table));
	for (i = 0; i != ind_tbl->queues_n; ++i)
		claim_nonzero(mlx5_priv_rxq_release(priv, ind_tbl->queues[i]));
	if (!rte_atomic32_read(&ind_tbl->refcnt)) {
		LIST_REMOVE(ind_tbl, next);
		rte_free(ind_tbl);
		return 0;
	}
	return EBUSY;
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_ind_table_ibv_verify(struct priv *priv)
{
	struct mlx5_ind_table_ibv *ind_tbl;
	int ret = 0;

	LIST_FOREACH(ind_tbl, &priv->ind_tbls, next) {
		DEBUG("%p: Verbs indirection table %p still referenced",
		      (void *)priv, (void *)ind_tbl);
		++ret;
	}
	return ret;
}

/**
 * Create an Rx Hash queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param rss_key
 *   RSS key for the Rx hash queue.
 * @param rss_key_len
 *   RSS key length.
 * @param hash_fields
 *   Verbs protocol hash field to make the RSS on.
 * @param queues
 *   Queues entering in hash queue.
 * @param queues_n
 *   Number of queues.
 *
 * @return
 *   An hash Rx queue on success.
 */
struct mlx5_hrxq*
mlx5_priv_hrxq_new(struct priv *priv, uint8_t *rss_key, uint8_t rss_key_len,
		   uint64_t hash_fields, uint16_t queues[], uint16_t queues_n)
{
	struct mlx5_hrxq *hrxq;
	struct mlx5_ind_table_ibv *ind_tbl;
	struct ibv_qp *qp;

	ind_tbl = mlx5_priv_ind_table_ibv_get(priv, queues, queues_n);
	if (!ind_tbl)
		ind_tbl = mlx5_priv_ind_table_ibv_new(priv, queues, queues_n);
	if (!ind_tbl)
		return NULL;
	qp = ibv_create_qp_ex(
		priv->ctx,
		&(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_QP_INIT_ATTR_PD |
				IBV_QP_INIT_ATTR_IND_TABLE |
				IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_key_len,
				.rx_hash_key = rss_key,
				.rx_hash_fields_mask = hash_fields,
			},
			.rwq_ind_tbl = ind_tbl->ind_table,
			.pd = priv->pd,
		});
	if (!qp)
		goto error;
	hrxq = rte_calloc(__func__, 1, sizeof(*hrxq) + rss_key_len, 0);
	if (!hrxq)
		goto error;
	hrxq->ind_table = ind_tbl;
	hrxq->qp = qp;
	hrxq->rss_key_len = rss_key_len;
	hrxq->hash_fields = hash_fields;
	memcpy(hrxq->rss_key, rss_key, rss_key_len);
	rte_atomic32_inc(&hrxq->refcnt);
	LIST_INSERT_HEAD(&priv->hrxqs, hrxq, next);
	DEBUG("%p: Hash Rx queue %p: refcnt %d", (void *)priv,
	      (void *)hrxq, rte_atomic32_read(&hrxq->refcnt));
	return hrxq;
error:
	mlx5_priv_ind_table_ibv_release(priv, ind_tbl);
	if (qp)
		claim_zero(ibv_destroy_qp(qp));
	return NULL;
}

/**
 * Get an Rx Hash queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param rss_conf
 *   RSS configuration for the Rx hash queue.
 * @param queues
 *   Queues entering in hash queue.
 * @param queues_n
 *   Number of queues.
 *
 * @return
 *   An hash Rx queue on success.
 */
struct mlx5_hrxq*
mlx5_priv_hrxq_get(struct priv *priv, uint8_t *rss_key, uint8_t rss_key_len,
		   uint64_t hash_fields, uint16_t queues[], uint16_t queues_n)
{
	struct mlx5_hrxq *hrxq;

	LIST_FOREACH(hrxq, &priv->hrxqs, next) {
		struct mlx5_ind_table_ibv *ind_tbl;

		if (hrxq->rss_key_len != rss_key_len)
			continue;
		if (memcmp(hrxq->rss_key, rss_key, rss_key_len))
			continue;
		if (hrxq->hash_fields != hash_fields)
			continue;
		ind_tbl = mlx5_priv_ind_table_ibv_get(priv, queues, queues_n);
		if (!ind_tbl)
			continue;
		if (ind_tbl != hrxq->ind_table) {
			mlx5_priv_ind_table_ibv_release(priv, ind_tbl);
			continue;
		}
		rte_atomic32_inc(&hrxq->refcnt);
		DEBUG("%p: Hash Rx queue %p: refcnt %d", (void *)priv,
		      (void *)hrxq, rte_atomic32_read(&hrxq->refcnt));
		return hrxq;
	}
	return NULL;
}

/**
 * Release the hash Rx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param hrxq
 *   Pointer to Hash Rx queue to release.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_priv_hrxq_release(struct priv *priv, struct mlx5_hrxq *hrxq)
{
	DEBUG("%p: Hash Rx queue %p: refcnt %d", (void *)priv,
	      (void *)hrxq, rte_atomic32_read(&hrxq->refcnt));
	if (rte_atomic32_dec_and_test(&hrxq->refcnt)) {
		claim_zero(ibv_destroy_qp(hrxq->qp));
		mlx5_priv_ind_table_ibv_release(priv, hrxq->ind_table);
		LIST_REMOVE(hrxq, next);
		rte_free(hrxq);
		return 0;
	}
	claim_nonzero(mlx5_priv_ind_table_ibv_release(priv, hrxq->ind_table));
	return EBUSY;
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_hrxq_ibv_verify(struct priv *priv)
{
	struct mlx5_hrxq *hrxq;
	int ret = 0;

	LIST_FOREACH(hrxq, &priv->hrxqs, next) {
		DEBUG("%p: Verbs Hash Rx queue %p still referenced",
		      (void *)priv, (void *)hrxq);
		++ret;
	}
	return ret;
}
