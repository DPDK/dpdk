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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/arch.h>
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_debug.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

/* Initialization data for hash RX queues. */
const struct hash_rxq_init hash_rxq_init[] = {
	[HASH_RXQ_TCPV4] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV4 |
				IBV_EXP_RX_HASH_DST_IPV4 |
				IBV_EXP_RX_HASH_SRC_PORT_TCP |
				IBV_EXP_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_TCP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_EXP_FLOW_SPEC_TCP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV4],
	},
	[HASH_RXQ_UDPV4] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV4 |
				IBV_EXP_RX_HASH_DST_IPV4 |
				IBV_EXP_RX_HASH_SRC_PORT_UDP |
				IBV_EXP_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_UDP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_EXP_FLOW_SPEC_UDP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV4],
	},
	[HASH_RXQ_IPV4] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV4 |
				IBV_EXP_RX_HASH_DST_IPV4),
		.dpdk_rss_hf = (ETH_RSS_IPV4 |
				ETH_RSS_FRAG_IPV4),
		.flow_priority = 1,
		.flow_spec.ipv4 = {
			.type = IBV_EXP_FLOW_SPEC_IPV4,
			.size = sizeof(hash_rxq_init[0].flow_spec.ipv4),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_ETH],
	},
	[HASH_RXQ_TCPV6] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV6 |
				IBV_EXP_RX_HASH_DST_IPV6 |
				IBV_EXP_RX_HASH_SRC_PORT_TCP |
				IBV_EXP_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_TCP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_EXP_FLOW_SPEC_TCP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV6],
	},
	[HASH_RXQ_UDPV6] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV6 |
				IBV_EXP_RX_HASH_DST_IPV6 |
				IBV_EXP_RX_HASH_SRC_PORT_UDP |
				IBV_EXP_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_UDP,
		.flow_priority = 0,
		.flow_spec.tcp_udp = {
			.type = IBV_EXP_FLOW_SPEC_UDP,
			.size = sizeof(hash_rxq_init[0].flow_spec.tcp_udp),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_IPV6],
	},
	[HASH_RXQ_IPV6] = {
		.hash_fields = (IBV_EXP_RX_HASH_SRC_IPV6 |
				IBV_EXP_RX_HASH_DST_IPV6),
		.dpdk_rss_hf = (ETH_RSS_IPV6 |
				ETH_RSS_FRAG_IPV6),
		.flow_priority = 1,
		.flow_spec.ipv6 = {
			.type = IBV_EXP_FLOW_SPEC_IPV6,
			.size = sizeof(hash_rxq_init[0].flow_spec.ipv6),
		},
		.underlayer = &hash_rxq_init[HASH_RXQ_ETH],
	},
	[HASH_RXQ_ETH] = {
		.hash_fields = 0,
		.dpdk_rss_hf = 0,
		.flow_priority = 2,
		.flow_spec.eth = {
			.type = IBV_EXP_FLOW_SPEC_ETH,
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
priv_flow_attr(struct priv *priv, struct ibv_exp_flow_attr *flow_attr,
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
	*flow_attr = (struct ibv_exp_flow_attr){
		.type = IBV_EXP_FLOW_ATTR_NORMAL,
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
	struct ibv_exp_wq *wqs[priv->reta_idx_n];
	struct ind_table_init ind_table_init[IND_TABLE_INIT_N];
	unsigned int ind_tables_n =
		priv_make_ind_table_init(priv, &ind_table_init);
	unsigned int hash_rxqs_n = 0;
	struct hash_rxq (*hash_rxqs)[] = NULL;
	struct ibv_exp_rwq_ind_table *(*ind_tables)[] = NULL;
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
		struct rxq_ctrl *rxq_ctrl;

		rxq_ctrl = container_of((*priv->rxqs)[(*priv->reta_idx)[i]],
					struct rxq_ctrl, rxq);
		wqs[i] = rxq_ctrl->wq;
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
		struct ibv_exp_rwq_ind_table_init_attr ind_init_attr = {
			.pd = priv->pd,
			.log_ind_tbl_size = 0, /* Set below. */
			.ind_tbl = wqs,
			.comp_mask = 0,
		};
		unsigned int ind_tbl_size = ind_table_init[i].max_size;
		struct ibv_exp_rwq_ind_table *ind_table;

		if (priv->reta_idx_n < ind_tbl_size)
			ind_tbl_size = priv->reta_idx_n;
		ind_init_attr.log_ind_tbl_size = log2above(ind_tbl_size);
		errno = 0;
		ind_table = ibv_exp_create_rwq_ind_table(priv->ctx,
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
		struct ibv_exp_rx_hash_conf hash_conf = {
			.rx_hash_function = IBV_EXP_RX_HASH_FUNC_TOEPLITZ,
			.rx_hash_key_len = (priv_rss_conf ?
					    priv_rss_conf->rss_key_len :
					    rss_hash_default_key_len),
			.rx_hash_key = (priv_rss_conf ?
					priv_rss_conf->rss_key :
					rss_hash_default_key),
			.rx_hash_fields_mask = hash_rxq_init[type].hash_fields,
			.rwq_ind_tbl = (*ind_tables)[j],
		};
		struct ibv_exp_qp_init_attr qp_init_attr = {
			.max_inl_recv = 0, /* Currently not supported. */
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask = (IBV_EXP_QP_INIT_ATTR_PD |
				      IBV_EXP_QP_INIT_ATTR_RX_HASH),
			.pd = priv->pd,
			.rx_hash_conf = &hash_conf,
			.port_num = priv->port,
		};

		DEBUG("using indirection table %u for hash RX queue %u type %d",
		      j, i, type);
		*hash_rxq = (struct hash_rxq){
			.priv = priv,
			.qp = ibv_exp_create_qp(priv->ctx, &qp_init_attr),
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
			struct ibv_exp_rwq_ind_table *ind_table =
				(*ind_tables)[j];

			if (ind_table == NULL)
				continue;
			claim_zero(ibv_exp_destroy_rwq_ind_table(ind_table));
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
		/* Also check that there are no remaining flows. */
		for (j = 0; (j != RTE_DIM(hash_rxq->special_flow)); ++j)
			for (k = 0;
			     (k != RTE_DIM(hash_rxq->special_flow[j]));
			     ++k)
				assert(hash_rxq->special_flow[j][k] == NULL);
		for (j = 0; (j != RTE_DIM(hash_rxq->mac_flow)); ++j)
			for (k = 0; (k != RTE_DIM(hash_rxq->mac_flow[j])); ++k)
				assert(hash_rxq->mac_flow[j][k] == NULL);
		claim_zero(ibv_destroy_qp(hash_rxq->qp));
	}
	priv->hash_rxqs_n = 0;
	rte_free(priv->hash_rxqs);
	priv->hash_rxqs = NULL;
	for (i = 0; (i != priv->ind_tables_n); ++i) {
		struct ibv_exp_rwq_ind_table *ind_table =
			(*priv->ind_tables)[i];

		assert(ind_table != NULL);
		claim_zero(ibv_exp_destroy_rwq_ind_table(ind_table));
	}
	priv->ind_tables_n = 0;
	rte_free(priv->ind_tables);
	priv->ind_tables = NULL;
}

/**
 * Check whether a given flow type is allowed.
 *
 * @param priv
 *   Pointer to private structure.
 * @param type
 *   Flow type to check.
 *
 * @return
 *   Nonzero if the given flow type is allowed.
 */
int
priv_allow_flow_type(struct priv *priv, enum hash_rxq_flow_type type)
{
	/* Only FLOW_TYPE_PROMISC is allowed when promiscuous mode
	 * has been requested. */
	if (priv->promisc_req)
		return type == HASH_RXQ_FLOW_TYPE_PROMISC;
	switch (type) {
	case HASH_RXQ_FLOW_TYPE_PROMISC:
		return !!priv->promisc_req;
	case HASH_RXQ_FLOW_TYPE_ALLMULTI:
		return !!priv->allmulti_req;
	case HASH_RXQ_FLOW_TYPE_BROADCAST:
	case HASH_RXQ_FLOW_TYPE_IPV6MULTI:
		/* If allmulti is enabled, broadcast and ipv6multi
		 * are unnecessary. */
		return !priv->allmulti_req;
	case HASH_RXQ_FLOW_TYPE_MAC:
		return 1;
	default:
		/* Unsupported flow type is not allowed. */
		return 0;
	}
	return 0;
}

/**
 * Automatically enable/disable flows according to configuration.
 *
 * @param priv
 *   Private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_rehash_flows(struct priv *priv)
{
	enum hash_rxq_flow_type i;

	for (i = HASH_RXQ_FLOW_TYPE_PROMISC;
			i != RTE_DIM((*priv->hash_rxqs)[0].special_flow);
			++i)
		if (!priv_allow_flow_type(priv, i)) {
			priv_special_flow_disable(priv, i);
		} else {
			int ret = priv_special_flow_enable(priv, i);

			if (ret)
				return ret;
		}
	if (priv_allow_flow_type(priv, HASH_RXQ_FLOW_TYPE_MAC))
		return priv_mac_addrs_enable(priv);
	priv_mac_addrs_disable(priv);
	return 0;
}

/**
 * Allocate RX queue elements.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 * @param[in] pool
 *   If not NULL, fetch buffers from this array instead of allocating them
 *   with rte_pktmbuf_alloc().
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_alloc_elts(struct rxq_ctrl *rxq_ctrl, unsigned int elts_n,
	       struct rte_mbuf *(*pool)[])
{
	const unsigned int sges_n = 1 << rxq_ctrl->rxq.sges_n;
	unsigned int i;
	int ret = 0;

	/* Iterate on segments. */
	for (i = 0; (i != elts_n); ++i) {
		struct rte_mbuf *buf;
		volatile struct mlx5_wqe_data_seg *scat =
			&(*rxq_ctrl->rxq.wqes)[i];

		if (pool != NULL) {
			buf = (*pool)[i];
			assert(buf != NULL);
			rte_pktmbuf_reset(buf);
			rte_pktmbuf_refcnt_update(buf, 1);
		} else
			buf = rte_pktmbuf_alloc(rxq_ctrl->rxq.mp);
		if (buf == NULL) {
			assert(pool == NULL);
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
		/* scat->addr must be able to store a pointer. */
		assert(sizeof(scat->addr) >= sizeof(uintptr_t));
		*scat = (struct mlx5_wqe_data_seg){
			.addr = htonll(rte_pktmbuf_mtod(buf, uintptr_t)),
			.byte_count = htonl(DATA_LEN(buf)),
			.lkey = htonl(rxq_ctrl->mr->lkey),
		};
		(*rxq_ctrl->rxq.elts)[i] = buf;
	}
	DEBUG("%p: allocated and configured %u segments (max %u packets)",
	      (void *)rxq_ctrl, elts_n, elts_n / (1 << rxq_ctrl->rxq.sges_n));
	assert(ret == 0);
	return 0;
error:
	assert(pool == NULL);
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
rxq_free_elts(struct rxq_ctrl *rxq_ctrl)
{
	unsigned int i;

	DEBUG("%p: freeing WRs", (void *)rxq_ctrl);
	if (rxq_ctrl->rxq.elts == NULL)
		return;

	for (i = 0; (i != (1u << rxq_ctrl->rxq.elts_n)); ++i) {
		if ((*rxq_ctrl->rxq.elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq_ctrl->rxq.elts)[i]);
		(*rxq_ctrl->rxq.elts)[i] = NULL;
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
rxq_cleanup(struct rxq_ctrl *rxq_ctrl)
{
	DEBUG("cleaning up %p", (void *)rxq_ctrl);
	rxq_free_elts(rxq_ctrl);
	if (rxq_ctrl->fdir_queue != NULL)
		priv_fdir_queue_destroy(rxq_ctrl->priv, rxq_ctrl->fdir_queue);
	if (rxq_ctrl->wq != NULL)
		claim_zero(ibv_exp_destroy_wq(rxq_ctrl->wq));
	if (rxq_ctrl->cq != NULL)
		claim_zero(ibv_destroy_cq(rxq_ctrl->cq));
	if (rxq_ctrl->channel != NULL)
		claim_zero(ibv_destroy_comp_channel(rxq_ctrl->channel));
	if (rxq_ctrl->mr != NULL)
		claim_zero(ibv_dereg_mr(rxq_ctrl->mr));
	memset(rxq_ctrl, 0, sizeof(*rxq_ctrl));
}

/**
 * Reconfigure RX queue buffers.
 *
 * rxq_rehash() does not allocate mbufs, which, if not done from the right
 * thread (such as a control thread), may corrupt the pool.
 * In case of failure, the queue is left untouched.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rxq_ctrl
 *   RX queue pointer.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
rxq_rehash(struct rte_eth_dev *dev, struct rxq_ctrl *rxq_ctrl)
{
	unsigned int elts_n = 1 << rxq_ctrl->rxq.elts_n;
	unsigned int i;
	struct ibv_exp_wq_attr mod;
	int err;

	DEBUG("%p: rehashing queue %p with %u SGE(s) per packet",
	      (void *)dev, (void *)rxq_ctrl, 1 << rxq_ctrl->rxq.sges_n);
	assert(!(elts_n % (1 << rxq_ctrl->rxq.sges_n)));
	/* From now on, any failure will render the queue unusable.
	 * Reinitialize WQ. */
	mod = (struct ibv_exp_wq_attr){
		.attr_mask = IBV_EXP_WQ_ATTR_STATE,
		.wq_state = IBV_EXP_WQS_RESET,
	};
	err = ibv_exp_modify_wq(rxq_ctrl->wq, &mod);
	if (err) {
		ERROR("%p: cannot reset WQ: %s", (void *)dev, strerror(err));
		assert(err > 0);
		return err;
	}
	/* Snatch mbufs from original queue. */
	claim_zero(rxq_alloc_elts(rxq_ctrl, elts_n, rxq_ctrl->rxq.elts));
	for (i = 0; i != elts_n; ++i) {
		struct rte_mbuf *buf = (*rxq_ctrl->rxq.elts)[i];

		assert(rte_mbuf_refcnt_read(buf) == 2);
		rte_pktmbuf_free_seg(buf);
	}
	/* Change queue state to ready. */
	mod = (struct ibv_exp_wq_attr){
		.attr_mask = IBV_EXP_WQ_ATTR_STATE,
		.wq_state = IBV_EXP_WQS_RDY,
	};
	err = ibv_exp_modify_wq(rxq_ctrl->wq, &mod);
	if (err) {
		ERROR("%p: WQ state to IBV_EXP_WQS_RDY failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	/* Update doorbell counter. */
	rxq_ctrl->rxq.rq_ci = elts_n >> rxq_ctrl->rxq.sges_n;
	rte_wmb();
	*rxq_ctrl->rxq.rq_db = htonl(rxq_ctrl->rxq.rq_ci);
error:
	assert(err >= 0);
	return err;
}

/**
 * Initialize RX queue.
 *
 * @param tmpl
 *   Pointer to RX queue control template.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static inline int
rxq_setup(struct rxq_ctrl *tmpl)
{
	struct ibv_cq *ibcq = tmpl->cq;
	struct ibv_mlx5_cq_info cq_info;
	struct mlx5_rwq *rwq = container_of(tmpl->wq, struct mlx5_rwq, wq);
	struct rte_mbuf *(*elts)[1 << tmpl->rxq.elts_n] =
		rte_calloc_socket("RXQ", 1, sizeof(*elts), 0, tmpl->socket);

	if (ibv_mlx5_exp_get_cq_info(ibcq, &cq_info)) {
		ERROR("Unable to query CQ info. check your OFED.");
		return ENOTSUP;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		ERROR("Wrong MLX5_CQE_SIZE environment variable value: "
		      "it should be set to %u", RTE_CACHE_LINE_SIZE);
		return EINVAL;
	}
	if (elts == NULL)
		return ENOMEM;
	tmpl->rxq.rq_db = rwq->rq.db;
	tmpl->rxq.cqe_n = log2above(cq_info.cqe_cnt);
	tmpl->rxq.cq_ci = 0;
	tmpl->rxq.rq_ci = 0;
	tmpl->rxq.cq_db = cq_info.dbrec;
	tmpl->rxq.wqes =
		(volatile struct mlx5_wqe_data_seg (*)[])
		(uintptr_t)rwq->rq.buff;
	tmpl->rxq.cqes =
		(volatile struct mlx5_cqe (*)[])
		(uintptr_t)cq_info.buf;
	tmpl->rxq.elts = elts;
	return 0;
}

/**
 * Configure a RX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
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
 *   0 on success, errno value on failure.
 */
int
rxq_ctrl_setup(struct rte_eth_dev *dev, struct rxq_ctrl *rxq_ctrl,
	       uint16_t desc, unsigned int socket,
	       const struct rte_eth_rxconf *conf, struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq_ctrl tmpl = {
		.priv = priv,
		.socket = socket,
		.rxq = {
			.elts_n = log2above(desc),
			.mp = mp,
			.rss_hash = priv->rxqs_n > 1,
		},
	};
	struct ibv_exp_wq_attr mod;
	union {
		struct ibv_exp_cq_init_attr cq;
		struct ibv_exp_wq_init_attr wq;
		struct ibv_exp_cq_attr cq_attr;
	} attr;
	unsigned int mb_len = rte_pktmbuf_data_room_size(mp);
	unsigned int cqe_n = desc - 1;
	struct rte_mbuf *(*elts)[desc] = NULL;
	int ret = 0;

	(void)conf; /* Thresholds configuration (ignored). */
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
	    (mb_len - RTE_PKTMBUF_HEADROOM)) {
		tmpl.rxq.sges_n = 0;
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
		tmpl.rxq.sges_n = sges_n;
		/* Make sure rxq.sges_n did not overflow. */
		size = mb_len * (1 << tmpl.rxq.sges_n);
		size -= RTE_PKTMBUF_HEADROOM;
		if (size < dev->data->dev_conf.rxmode.max_rx_pkt_len) {
			ERROR("%p: too many SGEs (%u) needed to handle"
			      " requested maximum packet size %u",
			      (void *)dev,
			      1 << sges_n,
			      dev->data->dev_conf.rxmode.max_rx_pkt_len);
			return EOVERFLOW;
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
	      (void *)dev, 1 << tmpl.rxq.sges_n);
	if (desc % (1 << tmpl.rxq.sges_n)) {
		ERROR("%p: number of RX queue descriptors (%u) is not a"
		      " multiple of SGEs per packet (%u)",
		      (void *)dev,
		      desc,
		      1 << tmpl.rxq.sges_n);
		return EINVAL;
	}
	/* Toggle RX checksum offload if hardware supports it. */
	if (priv->hw_csum)
		tmpl.rxq.csum = !!dev->data->dev_conf.rxmode.hw_ip_checksum;
	if (priv->hw_csum_l2tun)
		tmpl.rxq.csum_l2tun =
			!!dev->data->dev_conf.rxmode.hw_ip_checksum;
	/* Use the entire RX mempool as the memory region. */
	tmpl.mr = mlx5_mp2mr(priv->pd, mp);
	if (tmpl.mr == NULL) {
		ret = EINVAL;
		ERROR("%p: MR creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	if (dev->data->dev_conf.intr_conf.rxq) {
		tmpl.channel = ibv_create_comp_channel(priv->ctx);
		if (tmpl.channel == NULL) {
			dev->data->dev_conf.intr_conf.rxq = 0;
			ret = ENOMEM;
			ERROR("%p: Comp Channel creation failure: %s",
			(void *)dev, strerror(ret));
			goto error;
		}
	}
	attr.cq = (struct ibv_exp_cq_init_attr){
		.comp_mask = 0,
	};
	if (priv->cqe_comp) {
		attr.cq.comp_mask |= IBV_EXP_CQ_INIT_ATTR_FLAGS;
		attr.cq.flags |= IBV_EXP_CQ_COMPRESSED_CQE;
		cqe_n = (desc * 2) - 1; /* Double the number of CQEs. */
	}
	tmpl.cq = ibv_exp_create_cq(priv->ctx, cqe_n, NULL, tmpl.channel, 0,
				    &attr.cq);
	if (tmpl.cq == NULL) {
		ret = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	/* Configure VLAN stripping. */
	tmpl.rxq.vlan_strip = (priv->hw_vlan_strip &&
			       !!dev->data->dev_conf.rxmode.hw_vlan_strip);
	attr.wq = (struct ibv_exp_wq_init_attr){
		.wq_context = NULL, /* Could be useful in the future. */
		.wq_type = IBV_EXP_WQT_RQ,
		/* Max number of outstanding WRs. */
		.max_recv_wr = desc >> tmpl.rxq.sges_n,
		/* Max number of scatter/gather elements in a WR. */
		.max_recv_sge = 1 << tmpl.rxq.sges_n,
		.pd = priv->pd,
		.cq = tmpl.cq,
		.comp_mask =
			IBV_EXP_CREATE_WQ_VLAN_OFFLOADS |
			0,
		.vlan_offloads = (tmpl.rxq.vlan_strip ?
				  IBV_EXP_RECEIVE_WQ_CVLAN_STRIP :
				  0),
	};
	/* By default, FCS (CRC) is stripped by hardware. */
	if (dev->data->dev_conf.rxmode.hw_strip_crc) {
		tmpl.rxq.crc_present = 0;
	} else if (priv->hw_fcs_strip) {
		/* Ask HW/Verbs to leave CRC in place when supported. */
		attr.wq.flags |= IBV_EXP_CREATE_WQ_FLAG_SCATTER_FCS;
		attr.wq.comp_mask |= IBV_EXP_CREATE_WQ_FLAGS;
		tmpl.rxq.crc_present = 1;
	} else {
		WARN("%p: CRC stripping has been disabled but will still"
		     " be performed by hardware, make sure MLNX_OFED and"
		     " firmware are up to date",
		     (void *)dev);
		tmpl.rxq.crc_present = 0;
	}
	DEBUG("%p: CRC stripping is %s, %u bytes will be subtracted from"
	      " incoming frames to hide it",
	      (void *)dev,
	      tmpl.rxq.crc_present ? "disabled" : "enabled",
	      tmpl.rxq.crc_present << 2);
	if (!mlx5_getenv_int("MLX5_PMD_ENABLE_PADDING"))
		; /* Nothing else to do. */
	else if (priv->hw_padding) {
		INFO("%p: enabling packet padding on queue %p",
		     (void *)dev, (void *)rxq_ctrl);
		attr.wq.flags |= IBV_EXP_CREATE_WQ_FLAG_RX_END_PADDING;
		attr.wq.comp_mask |= IBV_EXP_CREATE_WQ_FLAGS;
	} else
		WARN("%p: packet padding has been requested but is not"
		     " supported, make sure MLNX_OFED and firmware are"
		     " up to date",
		     (void *)dev);

	tmpl.wq = ibv_exp_create_wq(priv->ctx, &attr.wq);
	if (tmpl.wq == NULL) {
		ret = (errno ? errno : EINVAL);
		ERROR("%p: WQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/*
	 * Make sure number of WRs*SGEs match expectations since a queue
	 * cannot allocate more than "desc" buffers.
	 */
	if (((int)attr.wq.max_recv_wr != (desc >> tmpl.rxq.sges_n)) ||
	    ((int)attr.wq.max_recv_sge != (1 << tmpl.rxq.sges_n))) {
		ERROR("%p: requested %u*%u but got %u*%u WRs*SGEs",
		      (void *)dev,
		      (desc >> tmpl.rxq.sges_n), (1 << tmpl.rxq.sges_n),
		      attr.wq.max_recv_wr, attr.wq.max_recv_sge);
		ret = EINVAL;
		goto error;
	}
	/* Save port ID. */
	tmpl.rxq.port_id = dev->data->port_id;
	DEBUG("%p: RTE port ID: %u", (void *)rxq_ctrl, tmpl.rxq.port_id);
	/* Change queue state to ready. */
	mod = (struct ibv_exp_wq_attr){
		.attr_mask = IBV_EXP_WQ_ATTR_STATE,
		.wq_state = IBV_EXP_WQS_RDY,
	};
	ret = ibv_exp_modify_wq(tmpl.wq, &mod);
	if (ret) {
		ERROR("%p: WQ state to IBV_EXP_WQS_RDY failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	ret = rxq_setup(&tmpl);
	if (ret) {
		ERROR("%p: cannot initialize RX queue structure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/* Reuse buffers from original queue if possible. */
	if (rxq_ctrl->rxq.elts_n) {
		assert(1 << rxq_ctrl->rxq.elts_n == desc);
		assert(rxq_ctrl->rxq.elts != tmpl.rxq.elts);
		ret = rxq_alloc_elts(&tmpl, desc, rxq_ctrl->rxq.elts);
	} else
		ret = rxq_alloc_elts(&tmpl, desc, NULL);
	if (ret) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/* Clean up rxq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old rxq just in case", (void *)rxq_ctrl);
	rxq_cleanup(rxq_ctrl);
	/* Move mbuf pointers to dedicated storage area in RX queue. */
	elts = (void *)(rxq_ctrl + 1);
	rte_memcpy(elts, tmpl.rxq.elts, sizeof(*elts));
#ifndef NDEBUG
	memset(tmpl.rxq.elts, 0x55, sizeof(*elts));
#endif
	rte_free(tmpl.rxq.elts);
	tmpl.rxq.elts = elts;
	*rxq_ctrl = tmpl;
	/* Update doorbell counter. */
	rxq_ctrl->rxq.rq_ci = desc >> rxq_ctrl->rxq.sges_n;
	rte_wmb();
	*rxq_ctrl->rxq.rq_db = htonl(rxq_ctrl->rxq.rq_ci);
	DEBUG("%p: rxq updated with %p", (void *)rxq_ctrl, (void *)&tmpl);
	assert(ret == 0);
	return 0;
error:
	elts = tmpl.rxq.elts;
	rxq_cleanup(&tmpl);
	rte_free(elts);
	assert(ret > 0);
	return ret;
}

/**
 * DPDK callback to configure a RX queue.
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
	struct rxq *rxq = (*priv->rxqs)[idx];
	struct rxq_ctrl *rxq_ctrl = container_of(rxq, struct rxq_ctrl, rxq);
	int ret;

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
	if (rxq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)rxq);
		if (priv->started) {
			priv_unlock(priv);
			return -EEXIST;
		}
		(*priv->rxqs)[idx] = NULL;
		rxq_cleanup(rxq_ctrl);
		/* Resize if rxq size is changed. */
		if (rxq_ctrl->rxq.elts_n != log2above(desc)) {
			rxq_ctrl = rte_realloc(rxq_ctrl,
					       sizeof(*rxq_ctrl) +
					       desc * sizeof(struct rte_mbuf *),
					       RTE_CACHE_LINE_SIZE);
			if (!rxq_ctrl) {
				ERROR("%p: unable to reallocate queue index %u",
					(void *)dev, idx);
				priv_unlock(priv);
				return -ENOMEM;
			}
		}
	} else {
		rxq_ctrl = rte_calloc_socket("RXQ", 1, sizeof(*rxq_ctrl) +
					     desc * sizeof(struct rte_mbuf *),
					     0, socket);
		if (rxq_ctrl == NULL) {
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			priv_unlock(priv);
			return -ENOMEM;
		}
	}
	ret = rxq_ctrl_setup(dev, rxq_ctrl, desc, socket, conf, mp);
	if (ret)
		rte_free(rxq_ctrl);
	else {
		rxq_ctrl->rxq.stats.idx = idx;
		DEBUG("%p: adding RX queue %p to list",
		      (void *)dev, (void *)rxq_ctrl);
		(*priv->rxqs)[idx] = &rxq_ctrl->rxq;
		/* Update receive callback. */
		priv_select_rx_function(priv);
	}
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
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct rxq_ctrl *rxq_ctrl;
	struct priv *priv;
	unsigned int i;

	if (mlx5_is_secondary())
		return;

	if (rxq == NULL)
		return;
	rxq_ctrl = container_of(rxq, struct rxq_ctrl, rxq);
	priv = rxq_ctrl->priv;
	priv_lock(priv);
	if (priv_flow_rxq_in_use(priv, rxq))
		rte_panic("Rx queue %p is still used by a flow and cannot be"
			  " removed\n", (void *)rxq_ctrl);
	for (i = 0; (i != priv->rxqs_n); ++i)
		if ((*priv->rxqs)[i] == rxq) {
			DEBUG("%p: removing RX queue %p from list",
			      (void *)priv->dev, (void *)rxq_ctrl);
			(*priv->rxqs)[i] = NULL;
			break;
		}
	rxq_cleanup(rxq_ctrl);
	rte_free(rxq_ctrl);
	priv_unlock(priv);
}

/**
 * DPDK callback for RX in secondary processes.
 *
 * This function configures all queues from primary process information
 * if necessary before reverting to the normal RX burst callback.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst_secondary_setup(void *dpdk_rxq, struct rte_mbuf **pkts,
			      uint16_t pkts_n)
{
	struct rxq *rxq = dpdk_rxq;
	struct rxq_ctrl *rxq_ctrl = container_of(rxq, struct rxq_ctrl, rxq);
	struct priv *priv = mlx5_secondary_data_setup(rxq_ctrl->priv);
	struct priv *primary_priv;
	unsigned int index;

	if (priv == NULL)
		return 0;
	primary_priv =
		mlx5_secondary_data[priv->dev->data->port_id].primary_priv;
	/* Look for queue index in both private structures. */
	for (index = 0; index != priv->rxqs_n; ++index)
		if (((*primary_priv->rxqs)[index] == rxq) ||
		    ((*priv->rxqs)[index] == rxq))
			break;
	if (index == priv->rxqs_n)
		return 0;
	rxq = (*priv->rxqs)[index];
	return priv->dev->rx_pkt_burst(rxq, pkts, pkts_n);
}

/**
 * Fill epoll fd list for rxq interrupts.
 *
 * @param priv
 *   Private structure.
 *
 * @return
 *   0 on success, negative on failure.
 */
int
priv_intr_efd_enable(struct priv *priv)
{
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	if (n == 0)
		return 0;
	if (n < rxqs_n) {
		WARN("rxqs num is larger than EAL max interrupt vector "
		     "%u > %u unable to supprt rxq interrupts",
		     rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
		return -EINVAL;
	}
	intr_handle->type = RTE_INTR_HANDLE_EXT;
	for (i = 0; i != n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		struct rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct rxq_ctrl, rxq);
		int fd = rxq_ctrl->channel->fd;
		int flags;
		int rc;

		flags = fcntl(fd, F_GETFL);
		rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		if (rc < 0) {
			WARN("failed to change rxq interrupt file "
			     "descriptor %d for queue index %d", fd, i);
			return -1;
		}
		intr_handle->efds[i] = fd;
	}
	intr_handle->nb_efd = n;
	return 0;
}

/**
 * Clean epoll fd list for rxq interrupts.
 *
 * @param priv
 *   Private structure.
 */
void
priv_intr_efd_disable(struct priv *priv)
{
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	rte_intr_free_epoll_fd(intr_handle);
}

/**
 * Create and init interrupt vector array.
 *
 * @param priv
 *   Private structure.
 *
 * @return
 *   0 on success, negative on failure.
 */
int
priv_create_intr_vec(struct priv *priv)
{
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int i;
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	if (rxqs_n == 0)
		return 0;
	intr_handle->intr_vec = (int *)
		rte_malloc("intr_vec", rxqs_n * sizeof(int), 0);
	if (intr_handle->intr_vec == NULL) {
		WARN("Failed to allocate memory for intr_vec "
		     "rxq interrupt will not be supported");
		return -ENOMEM;
	}
	for (i = 0; i != rxqs_n; ++i) {
		/* 1:1 mapping between rxq and interrupt. */
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + i;
	}
	return 0;
}

/**
 * Destroy init interrupt vector array.
 *
 * @param priv
 *   Private structure.
 *
 * @return
 *   0 on success, negative on failure.
 */
void
priv_destroy_intr_vec(struct priv *priv)
{
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	rte_free(intr_handle->intr_vec);
}
