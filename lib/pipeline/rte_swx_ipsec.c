/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_tailq.h>
#include <rte_eal_memconfig.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>
#include <rte_ipsec.h>

#include "rte_swx_ipsec.h"

#ifndef RTE_SWX_IPSEC_HUGE_PAGES_DISABLE

#include <rte_malloc.h>

static void *
env_calloc(size_t size, size_t alignment, int numa_node)
{
	return rte_zmalloc_socket(NULL, size, alignment, numa_node);
}

static void
env_free(void *start, size_t size __rte_unused)
{
	rte_free(start);
}

#else

#include <numa.h>

static void *
env_calloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	void *start;

	if (numa_available() == -1)
		return NULL;

	start = numa_alloc_onnode(size, numa_node);
	if (!start)
		return NULL;

	memset(start, 0, size);
	return start;
}

static void
env_free(void *start, size_t size)
{
	if ((numa_available() == -1) || !start)
		return;

	numa_free(start, size);
}

#endif

#ifndef RTE_SWX_IPSEC_POOL_CACHE_SIZE
#define RTE_SWX_IPSEC_POOL_CACHE_SIZE 256
#endif

/* The two crypto device mempools have their size set to the number of SAs. The mempool API requires
 * the mempool size to be at least 1.5 times the size of the mempool cache.
 */
#define N_SA_MIN (RTE_SWX_IPSEC_POOL_CACHE_SIZE * 1.5)

struct ipsec_sa {
	struct rte_ipsec_session s;
	int valid;
};

struct ipsec_pkts_in {
	struct rte_mbuf *pkts[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct ipsec_sa *sa[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_ipsec_group groups[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_crypto_op *group_cops[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_crypto_op *cops[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	uint32_t n_cops;
};

struct ipsec_pkts_out {
	struct rte_crypto_op *cops[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_mbuf *group_pkts[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_ipsec_group groups[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	struct rte_mbuf *pkts[RTE_SWX_IPSEC_BURST_SIZE_MAX];
	uint32_t n_pkts;
};

struct rte_swx_ipsec {
	/*
	 * Parameters.
	 */

	/* IPsec instance name. */
	char name[RTE_SWX_IPSEC_NAME_SIZE];

	/* Input packet queue. */
	struct rte_ring *ring_in;

	/* Output packet queue. */
	struct rte_ring *ring_out;

	/* Crypto device ID. */
	uint8_t dev_id;

	/* Crypto device queue pair ID. */
	uint16_t qp_id;

	/* Burst sizes. */
	struct rte_swx_ipsec_burst_size bsz;

	/* SA table size. */
	size_t n_sa_max;

	/*
	 * Internals.
	 */
	/* Crypto device buffer pool for sessions. */
	struct rte_mempool *mp_session;

	/* Pre-crypto packets. */
	struct ipsec_pkts_in in;

	/* Post-crypto packets. */
	struct ipsec_pkts_out out;

	/* Crypto device enqueue threshold. */
	uint32_t crypto_wr_threshold;

	/* Packets currently under crypto device processing. */
	uint32_t n_pkts_crypto;

	/* List of free SADB positions. */
	uint32_t *sa_free_id;

	/* Number of elements in the SADB list of free positions. */
	size_t n_sa_free_id;

	/* Allocated memory total size in bytes. */
	size_t total_size;

	/* Flag for registration to the global list of instances. */
	int registered;

	/*
	 * Table memory.
	 */
	uint8_t memory[] __rte_cache_aligned;
};

static inline struct ipsec_sa *
ipsec_sa_get(struct rte_swx_ipsec *ipsec, uint32_t sa_id)
{
	struct ipsec_sa *sadb = (struct ipsec_sa *)ipsec->memory;

	return &sadb[sa_id & (ipsec->n_sa_max - 1)];
}

/* Global list of instances. */
TAILQ_HEAD(rte_swx_ipsec_list, rte_tailq_entry);

static struct rte_tailq_elem rte_swx_ipsec_tailq = {
	.name = "RTE_SWX_IPSEC",
};

EAL_REGISTER_TAILQ(rte_swx_ipsec_tailq)

struct rte_swx_ipsec *
rte_swx_ipsec_find(const char *name)
{
	struct rte_swx_ipsec_list *ipsec_list;
	struct rte_tailq_entry *te = NULL;

	if (!name ||
	    !name[0] ||
	    (strnlen(name, RTE_SWX_IPSEC_NAME_SIZE) == RTE_SWX_IPSEC_NAME_SIZE))
		return NULL;

	ipsec_list = RTE_TAILQ_CAST(rte_swx_ipsec_tailq.head, rte_swx_ipsec_list);

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, ipsec_list, next) {
		struct rte_swx_ipsec *ipsec = (struct rte_swx_ipsec *)te->data;

		if (!strncmp(name, ipsec->name, sizeof(ipsec->name))) {
			rte_mcfg_tailq_read_unlock();
			return ipsec;
		}
	}

	rte_mcfg_tailq_read_unlock();
	return NULL;
}

static int
ipsec_register(struct rte_swx_ipsec *ipsec)
{
	struct rte_swx_ipsec_list *ipsec_list;
	struct rte_tailq_entry *te = NULL;

	ipsec_list = RTE_TAILQ_CAST(rte_swx_ipsec_tailq.head, rte_swx_ipsec_list);

	rte_mcfg_tailq_write_lock();

	TAILQ_FOREACH(te, ipsec_list, next) {
		struct rte_swx_ipsec *elem = (struct rte_swx_ipsec *)te->data;

		if (!strncmp(ipsec->name, elem->name, sizeof(ipsec->name))) {
			rte_mcfg_tailq_write_unlock();
			return -EEXIST;
		}
	}

	te = calloc(1, sizeof(struct rte_tailq_entry));
	if (!te) {
		rte_mcfg_tailq_write_unlock();
		return -ENOMEM;
	}

	te->data = (void *)ipsec;
	TAILQ_INSERT_TAIL(ipsec_list, te, next);
	rte_mcfg_tailq_write_unlock();
	return 0;
}

static void
ipsec_unregister(struct rte_swx_ipsec *ipsec)
{
	struct rte_swx_ipsec_list *ipsec_list;
	struct rte_tailq_entry *te = NULL;

	ipsec_list = RTE_TAILQ_CAST(rte_swx_ipsec_tailq.head, rte_swx_ipsec_list);

	rte_mcfg_tailq_write_lock();

	TAILQ_FOREACH(te, ipsec_list, next) {
		if (te->data == (void *)ipsec) {
			TAILQ_REMOVE(ipsec_list, te, next);
			rte_mcfg_tailq_write_unlock();
			free(te);
			return;
		}
	}

	rte_mcfg_tailq_write_unlock();
}

static void
ipsec_session_free(struct rte_swx_ipsec *ipsec, struct rte_ipsec_session *s);

void
rte_swx_ipsec_free(struct rte_swx_ipsec *ipsec)
{
	size_t i;

	if (!ipsec)
		return;

	/* Remove the current instance from the global list. */
	if (ipsec->registered)
		ipsec_unregister(ipsec);

	/* SADB. */
	for (i = 0; i < ipsec->n_sa_max; i++) {
		struct ipsec_sa *sa = ipsec_sa_get(ipsec, i);

		if (!sa->valid)
			continue;

		/* SA session. */
		ipsec_session_free(ipsec, &sa->s);
	}

	/* Crypto device buffer pools. */
	rte_mempool_free(ipsec->mp_session);

	/* IPsec object memory. */
	env_free(ipsec, ipsec->total_size);
}

int
rte_swx_ipsec_create(struct rte_swx_ipsec **ipsec_out,
		     const char *name,
		     struct rte_swx_ipsec_params *params,
		     int numa_node)
{
	char resource_name[RTE_SWX_IPSEC_NAME_SIZE];
	struct rte_swx_ipsec *ipsec = NULL;
	struct rte_ring *ring_in, *ring_out;
	struct rte_cryptodev_info dev_info;
	size_t n_sa_max, sadb_offset, sadb_size, sa_free_id_offset, sa_free_id_size, total_size, i;
	uint32_t dev_session_size;
	int dev_id, status = 0;

	/* Check input parameters. */
	if (!ipsec_out ||
	    !name ||
	    !name[0] ||
	    (strnlen((name), RTE_SWX_IPSEC_NAME_SIZE) == RTE_SWX_IPSEC_NAME_SIZE) ||
	    !params ||
	    (params->bsz.ring_rd > RTE_SWX_IPSEC_BURST_SIZE_MAX) ||
	    (params->bsz.ring_wr > RTE_SWX_IPSEC_BURST_SIZE_MAX) ||
	    (params->bsz.crypto_wr > RTE_SWX_IPSEC_BURST_SIZE_MAX) ||
	    (params->bsz.crypto_rd > RTE_SWX_IPSEC_BURST_SIZE_MAX) ||
	    !params->n_sa_max) {
		status = -EINVAL;
		goto error;
	}

	ring_in = rte_ring_lookup(params->ring_in_name);
	if (!ring_in) {
		status = -EINVAL;
		goto error;
	}

	ring_out = rte_ring_lookup(params->ring_out_name);
	if (!ring_out) {
		status = -EINVAL;
		goto error;
	}

	dev_id = rte_cryptodev_get_dev_id(params->crypto_dev_name);
	if (dev_id == -1) {
		status = -EINVAL;
		goto error;
	}

	rte_cryptodev_info_get(dev_id, &dev_info);
	if (params->crypto_dev_queue_pair_id >= dev_info.max_nb_queue_pairs) {
		status = -EINVAL;
		goto error;
	}

	/* Memory allocation. */
	n_sa_max = rte_align64pow2(RTE_MAX(params->n_sa_max, N_SA_MIN));

	sadb_offset = sizeof(struct rte_swx_ipsec);
	sadb_size = RTE_CACHE_LINE_ROUNDUP(n_sa_max * sizeof(struct ipsec_sa));

	sa_free_id_offset = sadb_offset + sadb_size;
	sa_free_id_size = RTE_CACHE_LINE_ROUNDUP(n_sa_max * sizeof(uint32_t));

	total_size = sa_free_id_offset + sa_free_id_size;
	ipsec = env_calloc(total_size, RTE_CACHE_LINE_SIZE, numa_node);
	if (!ipsec) {
		status = -ENOMEM;
		goto error;
	}

	/* Initialization. */
	strcpy(ipsec->name, name);
	ipsec->ring_in = ring_in;
	ipsec->ring_out = ring_out;
	ipsec->dev_id = (uint8_t)dev_id;
	ipsec->qp_id = params->crypto_dev_queue_pair_id;
	memcpy(&ipsec->bsz, &params->bsz, sizeof(struct rte_swx_ipsec_burst_size));
	ipsec->n_sa_max = n_sa_max;

	ipsec->crypto_wr_threshold = params->bsz.crypto_wr * 3 / 4;

	ipsec->sa_free_id = (uint32_t *)&ipsec->memory[sa_free_id_offset];
	for (i = 0; i < n_sa_max; i++)
		ipsec->sa_free_id[i] = n_sa_max - 1 - i;
	ipsec->n_sa_free_id = n_sa_max;

	ipsec->total_size = total_size;

	/* Crypto device memory pools. */
	dev_session_size = rte_cryptodev_sym_get_private_session_size((uint8_t)dev_id);

	snprintf(resource_name, sizeof(resource_name), "%s_mp", name);
	ipsec->mp_session = rte_cryptodev_sym_session_pool_create(resource_name,
		n_sa_max, /* number of pool elements */
		dev_session_size, /* pool element size */
		RTE_SWX_IPSEC_POOL_CACHE_SIZE, /* pool cache size */
		0, /* pool element private data size */
		numa_node);
	if (!ipsec->mp_session) {
		status = -ENOMEM;
		goto error;
	}

	/* Add the current instance to the global list. */
	status = ipsec_register(ipsec);
	if (status)
		goto error;

	ipsec->registered = 1;

	*ipsec_out = ipsec;
	return 0;

error:
	rte_swx_ipsec_free(ipsec);
	return status;
}

static inline int
ipsec_sa_group(struct rte_swx_ipsec *ipsec, int n_pkts)
{
	struct ipsec_sa *sa;
	struct rte_ipsec_group *g;
	int n_groups, n_pkts_in_group, i;

	sa = ipsec->in.sa[0];

	g = &ipsec->in.groups[0];
	g->id.ptr = sa;
	g->m = &ipsec->in.pkts[0];
	n_pkts_in_group = 1;
	n_groups = 1;

	for (i = 1; i < n_pkts; i++) {
		struct ipsec_sa *sa_new = ipsec->in.sa[i];

		/* Same SA => Add the current pkt to the same group. */
		if (sa_new == sa) {
			n_pkts_in_group++;
			continue;
		}

		/* Different SA => Close the current group & add the current pkt to a new group. */
		g->cnt = n_pkts_in_group;
		sa = sa_new;

		g++;
		g->id.ptr = sa;
		g->m = &ipsec->in.pkts[i];
		n_pkts_in_group = 1;
		n_groups++;
	}

	/* Close the last group. */
	g->cnt = n_pkts_in_group;

	return n_groups;
}

static inline void
ipsec_crypto_enqueue(struct rte_swx_ipsec *ipsec, uint16_t n_cops)
{
	struct rte_crypto_op **dst0 = ipsec->in.cops, **dst;
	struct rte_crypto_op **src = ipsec->in.group_cops;

	uint32_t n_pkts_crypto = ipsec->n_pkts_crypto;
	uint32_t n_dst = ipsec->in.n_cops;
	uint32_t n_dst_max = ipsec->bsz.crypto_wr;
	uint32_t n_dst_avail = n_dst_max - n_dst;
	uint32_t n_src = n_cops;
	uint32_t i;

	dst = &dst0[n_dst];

	/* Shortcut: If no elements in DST and enough elements in SRC, then simply use SRC directly
	 * instead of moving the SRC to DST first and then using DST.
	 */
	if (!n_dst && n_src >= ipsec->crypto_wr_threshold) {
		uint16_t n_ok;

		n_ok = rte_cryptodev_enqueue_burst(ipsec->dev_id, ipsec->qp_id, src, n_src);
		ipsec->n_pkts_crypto = n_pkts_crypto + n_ok;

		for (i = n_ok; i < n_src; i++) {
			struct rte_crypto_op *cop = src[i];
			struct rte_mbuf *m = cop->sym->m_src;

			rte_pktmbuf_free(m);
		}

		return;
	}

	/* Move from SRC to DST. Every time DST gets full, send burst from DST. */
	for ( ; n_src >= n_dst_avail; ) {
		uint32_t n_ok;

		/* Move from SRC to DST. */
		for (i = 0; i < n_dst_avail; i++)
			*dst++ = *src++;

		n_src -= n_dst_avail;

		/* DST full: send burst from DST. */
		n_ok = rte_cryptodev_enqueue_burst(ipsec->dev_id, ipsec->qp_id, dst0, n_dst_max);
		n_pkts_crypto += n_ok;

		for (i = n_ok ; i < n_dst_max; i++) {
			struct rte_crypto_op *cop = dst0[i];
			struct rte_mbuf *m = cop->sym->m_src;

			rte_pktmbuf_free(m);
		}

		/* Next iteration. */
		dst = dst0;
		n_dst = 0;
		n_dst_avail = n_dst_max;
	}

	ipsec->n_pkts_crypto = n_pkts_crypto;

	/* Move from SRC to DST. Not enough elements in SRC to get DST full. */
	for (i = 0; i < n_src; i++)
		*dst++ = *src++;

	n_dst += n_src;

	ipsec->in.n_cops = n_dst;
}

/**
 * Packet buffer anatomy:
 *
 * +----------+---------+--------------------------------------------------------------------------+
 * | Offset   | Size    | Description                                                              |
 * | (Byte #) | (Bytes) |                                                                          |
 * +==========+=========+==========================================================================+
 * | 0        | 128     | Meta-data: struct rte_mbuf.                                              |
 * |          |         | The buf_addr field points to the start of the packet section.            |
 * +----------+---------+--------------------------------------------------------------------------+
 * | 128      | 128     | Meta-data: struct ipsec_mbuf (see below).                                |
 * +----------+---------+--------------------------------------------------------------------------+
 * | 256      |         | Packet section.                                                          |
 * |          |         | The first packet byte is placed at the offset indicated by the struct    |
 * |          |         | rte_mbuf::data_off field relative to the start of the packet section.    |
 * +----------+---------+--------------------------------------------------------------------------+
 */
struct ipsec_mbuf {
	struct ipsec_sa *sa;
	struct rte_crypto_op cop;
	struct rte_crypto_sym_op sym_cop;
	uint8_t buffer[32]; /* The crypto IV is placed here. */
};

/* Offset from the start of the struct ipsec_mbuf::cop where the crypto IV will be placed. */
#define IV_OFFSET (sizeof(struct rte_crypto_op) + sizeof(struct rte_crypto_sym_op))

#define META_LENGTH sizeof(struct rte_swx_ipsec_input_packet_metadata)

static inline void
rte_swx_ipsec_pre_crypto(struct rte_swx_ipsec *ipsec)
{
	int n_pkts, n_groups, i;

	/* Read packets from the input ring. */
	n_pkts = rte_ring_sc_dequeue_burst(ipsec->ring_in,
					   (void **)ipsec->in.pkts,
					   ipsec->bsz.ring_rd,
					   NULL);
	if (!n_pkts)
		return;

	/* Get the SA for each packet. */
	for (i = 0; i < n_pkts; i++) {
		struct rte_mbuf *m = ipsec->in.pkts[i];
		struct rte_swx_ipsec_input_packet_metadata *meta;
		struct rte_ipv4_hdr *ipv4_hdr;
		uint32_t sa_id;

		meta = rte_pktmbuf_mtod(m, struct rte_swx_ipsec_input_packet_metadata *);
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, META_LENGTH);

		/* Read the SA ID from the IPsec meta-data placed at the front of the IP packet. */
		sa_id = ntohl(meta->sa_id);

		/* Consume the IPsec meta-data. */
		m->data_off += META_LENGTH;
		m->data_len -= META_LENGTH;
		m->pkt_len -= META_LENGTH;

		/* Set the fields required by the IPsec library. */
		m->l2_len = 0;
		m->l3_len = (ipv4_hdr->version_ihl >> 4 == 4) ?
			sizeof(struct rte_ipv4_hdr) :
			sizeof(struct rte_ipv6_hdr);

		/* Get the SA. */
		ipsec->in.sa[i] = ipsec_sa_get(ipsec, sa_id);
	}

	/* Group packets that share the same SA. */
	n_groups = ipsec_sa_group(ipsec, n_pkts);

	/* Write each group of packets sharing the same SA to the crypto device. */
	for (i = 0; i < n_groups; i++) {
		struct rte_ipsec_group *g = &ipsec->in.groups[i];
		struct ipsec_sa *sa = g->id.ptr;
		struct rte_ipsec_session *s = &sa->s;
		uint32_t j;
		uint16_t n_pkts_ok;

		/* Prepare the crypto ops for the current group. */
		for (j = 0; j < g->cnt; j++) {
			struct rte_mbuf *m = g->m[j];
			struct ipsec_mbuf *priv = rte_mbuf_to_priv(m);

			priv->sa = sa;
			ipsec->in.group_cops[j] = &priv->cop;
		}

		n_pkts_ok = rte_ipsec_pkt_crypto_prepare(s, g->m, ipsec->in.group_cops, g->cnt);

		for (j = n_pkts_ok; j < g->cnt; j++) {
			struct rte_mbuf *m = g->m[j];

			rte_pktmbuf_free(m);
		}

		/* Write the crypto ops of the current group to the crypto device. */
		ipsec_crypto_enqueue(ipsec, n_pkts_ok);
	}
}

static inline void
ipsec_ring_enqueue(struct rte_swx_ipsec *ipsec, struct rte_ipsec_group *g, uint32_t n_pkts)
{
	struct rte_mbuf **dst0 = ipsec->out.pkts, **dst;
	struct rte_mbuf **src = g->m;

	uint32_t n_dst = ipsec->out.n_pkts;
	uint32_t n_dst_max = ipsec->bsz.ring_wr;
	uint32_t n_dst_avail = n_dst_max - n_dst;
	uint32_t n_src = n_pkts;
	uint32_t i;

	dst = &dst0[n_dst];

	/* Move from SRC to DST. Every time DST gets full, send burst from DST. */
	for ( ; n_src >= n_dst_avail; ) {
		uint32_t n_ok;

		/* Move from SRC to DST. */
		for (i = 0; i < n_dst_avail; i++)
			*dst++ = *src++;

		n_src -= n_dst_avail;

		/* DST full: send burst from DST. */
		n_ok = rte_ring_sp_enqueue_burst(ipsec->ring_out, (void **)dst0, n_dst_max, NULL);

		for (i = n_ok ; i < n_dst_max; i++) {
			struct rte_mbuf *m = dst[i];

			rte_pktmbuf_free(m);
		}

		/* Next iteration. */
		dst = dst0;
		n_dst = 0;
		n_dst_avail = n_dst_max;
	}

	/* Move from SRC to DST. Not enough elements in SRC to get DST full. */
	for (i = 0; i < n_src; i++)
		*dst++ = *src++;

	n_dst += n_src;

	ipsec->out.n_pkts = n_dst;
}

static inline void
rte_swx_ipsec_post_crypto(struct rte_swx_ipsec *ipsec)
{
	uint32_t n_pkts_crypto = ipsec->n_pkts_crypto, n_pkts, ng, i;

	/* Read the crypto ops from the crypto device. */
	if (!n_pkts_crypto)
		return;

	n_pkts = rte_cryptodev_dequeue_burst(ipsec->dev_id,
					     ipsec->qp_id,
					     ipsec->out.cops,
					     ipsec->bsz.crypto_rd);
	if (!n_pkts)
		return;

	ipsec->n_pkts_crypto = n_pkts_crypto - n_pkts;

	/* Group packets that share the same SA. */
	ng = rte_ipsec_pkt_crypto_group((const struct rte_crypto_op **)(uintptr_t)ipsec->out.cops,
					      ipsec->out.group_pkts,
					      ipsec->out.groups,
					      n_pkts);

	/* Perform post-crypto IPsec processing for each group of packets that share the same SA.
	 * Write each group of packets to the output ring.
	 */
	for (i = 0, n_pkts = 0; i < ng; i++) {
		struct rte_ipsec_group *g = &ipsec->out.groups[i];
		struct rte_ipsec_session *s = g->id.ptr;
		uint32_t n_pkts_ok, j;

		/* Perform post-crypto IPsec processing for the current group. */
		n_pkts_ok = rte_ipsec_pkt_process(s, g->m, g->cnt);

		for (j = n_pkts_ok; j < g->cnt; j++) {
			struct rte_mbuf *m = g->m[j];

			rte_pktmbuf_free(m);
		}

		/* Write the packets of the current group to the output ring. */
		ipsec_ring_enqueue(ipsec, g, n_pkts_ok);
	}
}

void
rte_swx_ipsec_run(struct rte_swx_ipsec *ipsec)
{
	rte_swx_ipsec_pre_crypto(ipsec);
	rte_swx_ipsec_post_crypto(ipsec);
}

/**
 * IPsec Control Plane API
 */
struct cipher_alg {
	const char *name;
	enum rte_crypto_cipher_algorithm alg;
	uint32_t iv_size;
	uint32_t block_size;
	uint32_t key_size;
};

struct auth_alg {
	const char *name;
	enum rte_crypto_auth_algorithm alg;
	uint32_t iv_size;
	uint32_t digest_size;
	uint32_t key_size;
};

struct aead_alg {
	const char *name;
	enum rte_crypto_aead_algorithm alg;
	uint32_t iv_size;
	uint32_t block_size;
	uint32_t digest_size;
	uint32_t key_size;
	uint32_t aad_size;
};

static struct cipher_alg cipher_algs[] = {
	[0] = {
		.name = "null",
		.alg = RTE_CRYPTO_CIPHER_NULL,
		.iv_size = 0,
		.block_size = 4,
		.key_size = 0,
	},

	[1] = {
		.name = "aes-cbc-128",
		.alg = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_size = 16,
		.block_size = 16,
		.key_size = 16,
	},

	[2] = {
		.name = "aes-cbc-192",
		.alg = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_size = 16,
		.block_size = 16,
		.key_size = 24,
	},

	[3] = {
		.name = "aes-cbc-256",
		.alg = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_size = 16,
		.block_size = 16,
		.key_size = 32,
	},

	[4] = {
		.name = "aes-ctr-128",
		.alg = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 20,
	},

	[5] = {
		.name = "aes-ctr-192",
		.alg = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_size = 16,
		.block_size = 16,
		.key_size = 28,
	},

	[6] = {
		.name = "aes-ctr-256",
		.alg = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_size = 16,
		.block_size = 16,
		.key_size = 36,
	},

	[7] = {
		.name = "3des-cbc",
		.alg = RTE_CRYPTO_CIPHER_3DES_CBC,
		.iv_size = 8,
		.block_size = 8,
		.key_size = 24,
	},

	[8] = {
		.name = "des-cbc",
		.alg = RTE_CRYPTO_CIPHER_DES_CBC,
		.iv_size = 8,
		.block_size = 8,
		.key_size = 8,
	},
};

static struct auth_alg auth_algs[] = {
	[0] = {
		.name = "null",
		.alg = RTE_CRYPTO_AUTH_NULL,
		.iv_size = 0,
		.digest_size = 0,
		.key_size = 0,
	},

	[1] = {
		.name = "sha1-hmac",
		.alg = RTE_CRYPTO_AUTH_SHA1_HMAC,
		.iv_size = 0,
		.digest_size = 12,
		.key_size = 20,
	},

	[2] = {
		.name = "sha256-hmac",
		.alg = RTE_CRYPTO_AUTH_SHA256_HMAC,
		.iv_size = 0,
		.digest_size = 16,
		.key_size = 32,
	},

	[3] = {
		.name = "sha384-hmac",
		.alg = RTE_CRYPTO_AUTH_SHA384_HMAC,
		.iv_size = 0,
		.digest_size = 24,
		.key_size = 48,
	},

	[4] = {
		.name = "sha512-hmac",
		.alg = RTE_CRYPTO_AUTH_SHA512_HMAC,
		.iv_size = 0,
		.digest_size = 32,
		.key_size = 64,
	},

	[5] = {
		.name = "aes-gmac",
		.alg = RTE_CRYPTO_AUTH_AES_GMAC,
		.iv_size = 8,
		.digest_size = 16,
		.key_size = 20,
	},

	[6] = {
		.name = "aes-xcbc-mac-96",
		.alg = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
		.iv_size = 0,
		.digest_size = 12,
		.key_size = 16,
	},
};

static struct aead_alg aead_algs[] = {
	[0] = {
		.name = "aes-gcm-128",
		.alg = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 20,
		.digest_size = 16,
		.aad_size = 8,
	},

	[1] = {
		.name = "aes-gcm-192",
		.alg = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 28,
		.digest_size = 16,
		.aad_size = 8,
	},

	[2] = {
		.name = "aes-gcm-256",
		.alg = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 36,
		.digest_size = 16,
		.aad_size = 8,
	},

	[3] = {
		.name = "aes-ccm-128",
		.alg = RTE_CRYPTO_AEAD_AES_CCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 20,
		.digest_size = 16,
		.aad_size = 8,
	},

	[4] = {
		.name = "aes-ccm-192",
		.alg = RTE_CRYPTO_AEAD_AES_CCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 28,
		.digest_size = 16,
		.aad_size = 8,
	},

	[5] = {
		.name = "aes-ccm-256",
		.alg = RTE_CRYPTO_AEAD_AES_CCM,
		.iv_size = 8,
		.block_size = 4,
		.key_size = 36,
		.digest_size = 16,
		.aad_size = 8,
	},

	[6] = {
		.name = "chacha20-poly1305",
		.alg = RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
		.iv_size = 12,
		.block_size = 64,
		.key_size = 36,
		.digest_size = 16,
		.aad_size = 8,
	},
};

static struct cipher_alg *
cipher_alg_find(const char *name)
{
	size_t i;

	for (i = 0; i < RTE_DIM(cipher_algs); i++) {
		struct cipher_alg *alg = &cipher_algs[i];

		if (!strcmp(name, alg->name))
			return alg;
	}

	return NULL;
}

static struct cipher_alg *
cipher_alg_find_by_id(enum rte_crypto_cipher_algorithm alg_id, uint32_t key_size)
{
	size_t i;

	for (i = 0; i < RTE_DIM(cipher_algs); i++) {
		struct cipher_alg *alg = &cipher_algs[i];

		if (alg->alg == alg_id && alg->key_size == key_size)
			return alg;
	}

	return NULL;
}

static struct auth_alg *
auth_alg_find(const char *name)
{
	size_t i;

	for (i = 0; i < RTE_DIM(auth_algs); i++) {
		struct auth_alg *alg = &auth_algs[i];

		if (!strcmp(name, alg->name))
			return alg;
	}

	return NULL;
}

static struct auth_alg *
auth_alg_find_by_id(enum rte_crypto_auth_algorithm alg_id, uint32_t key_size)
{
	size_t i;

	for (i = 0; i < RTE_DIM(auth_algs); i++) {
		struct auth_alg *alg = &auth_algs[i];

		if (alg->alg == alg_id && alg->key_size == key_size)
			return alg;
	}

	return NULL;
}

static struct aead_alg *
aead_alg_find(const char *name)
{
	size_t i;

	for (i = 0; i < RTE_DIM(aead_algs); i++) {
		struct aead_alg *alg = &aead_algs[i];

		if (!strcmp(name, alg->name))
			return alg;
	}

	return NULL;
}

static struct aead_alg *
aead_alg_find_by_id(enum rte_crypto_aead_algorithm alg_id, uint32_t key_size)
{
	size_t i;

	for (i = 0; i < RTE_DIM(aead_algs); i++) {
		struct aead_alg *alg = &aead_algs[i];

		if (alg->alg == alg_id && alg->key_size == key_size)
			return alg;
	}

	return NULL;
}

static int
char_to_hex(char c, uint8_t *val)
{
	if (c >= '0' && c <= '9') {
		*val = c - '0';
		return 0;
	}

	if (c >= 'A' && c <= 'F') {
		*val = c - 'A' + 10;
		return 0;
	}

	if (c >= 'a' && c <= 'f') {
		*val = c - 'a' + 10;
		return 0;
	}

	return -EINVAL;
}

static int
hex_string_parse(char *src, uint8_t *dst, uint32_t n_dst_bytes)
{
	uint32_t i;

	/* Check input arguments. */
	if (!src || !src[0] || !dst || !n_dst_bytes)
		return -EINVAL;

	/* Skip any leading "0x" or "0X" in the src string. */
	if ((src[0] == '0') && (src[1] == 'x' || src[1] == 'X'))
		src += 2;

	/* Convert each group of two hex characters in the src string to one byte in dst array. */
	for (i = 0; i < n_dst_bytes; i++) {
		uint8_t a, b;
		int status;

		status = char_to_hex(*src, &a);
		if (status)
			return status;
		src++;

		status = char_to_hex(*src, &b);
		if (status)
			return status;
		src++;

		dst[i] = a * 16 + b;
	}

	/* Check for the end of the src string. */
	if (*src)
		return -EINVAL;

	return 0;
}

static int
token_is_comment(const char *token)
{
	if ((token[0] == '#') ||
	    (token[0] == ';') ||
	    ((token[0] == '/') && (token[1] == '/')))
		return 1; /* TRUE. */

	return 0; /* FALSE. */
}

#define MAX_TOKENS 64

#define CHECK(condition, msg)          \
do {                                   \
	if (!(condition)) {            \
		if (errmsg)            \
			*errmsg = msg; \
		goto error;            \
	}                              \
} while (0)

struct rte_swx_ipsec_sa_params *
rte_swx_ipsec_sa_read(struct rte_swx_ipsec *ipsec __rte_unused,
		      const char *string,
		      int *is_blank_or_comment,
		      const char **errmsg)
{
	char *token_array[MAX_TOKENS], **t;
	struct rte_swx_ipsec_sa_params *p = NULL;
	char *s0 = NULL, *s;
	uint32_t n_tokens = 0;
	int blank_or_comment = 0;

	/* Check input arguments. */
	CHECK(string && string[0], "NULL input");

	/* Memory allocation. */
	s0 = strdup(string);
	p = calloc(1, sizeof(struct rte_swx_ipsec_sa_params));
	CHECK(s0 && p, "Not enough memory");

	/* Parse the string into tokens. */
	for (s = s0; ; ) {
		char *token;

		token = strtok_r(s, " \f\n\r\t\v", &s);
		if (!token || token_is_comment(token))
			break;

		CHECK(n_tokens < RTE_DIM(token_array), "Too many tokens");

		token_array[n_tokens] = token;
		n_tokens++;
	}

	t = token_array;
	if (!n_tokens) {
		blank_or_comment = 1;
		goto error;
	}

	/*
	 * Crypto operation.
	 */
	if (!strcmp(t[0], "encrypt"))
		p->encrypt = 1;
	else if (!strcmp(t[0], "decrypt"))
		p->encrypt = 0;
	else
		CHECK(0, "Missing \"encrypt\"/\"decrypt\" keyword");

	t++;
	n_tokens--;

	/*
	 * Crypto parameters.
	 */
	CHECK(n_tokens >= 2, "Not enough tokens");

	if (!strcmp(t[0], "cipher")) {
		struct cipher_alg *cipher_alg;
		struct auth_alg *auth_alg;
		uint32_t key_size;

		p->crypto.is_aead = 0;

		/* cipher. */
		cipher_alg = cipher_alg_find(t[1]);
		CHECK(cipher_alg, "Unsupported cipher algorithm");

		key_size = cipher_alg->key_size;
		p->crypto.cipher_auth.cipher.alg = cipher_alg->alg;
		p->crypto.cipher_auth.cipher.key_size = key_size;

		t += 2;
		n_tokens -= 2;

		if (key_size) {
			int status;

			CHECK(n_tokens >= 2, "Not enough tokens");
			CHECK(!strcmp(t[0], "key"), "Missing cipher \"key\" keyword");
			CHECK(key_size <= RTE_DIM(p->crypto.cipher_auth.cipher.key),
				"Cipher algorithm key too big");

			status = hex_string_parse(t[1], p->crypto.cipher_auth.cipher.key, key_size);
			CHECK(!status, "Cipher key invalid format");

			t += 2;
			n_tokens -= 2;
		}

		/* authentication. */
		CHECK(n_tokens >= 2, "Not enough tokens");
		CHECK(!strcmp(t[0], "auth"), "Missing \"auth\" keyword");

		auth_alg = auth_alg_find(t[1]);
		CHECK(auth_alg, "Unsupported authentication algorithm");

		key_size = auth_alg->key_size;
		p->crypto.cipher_auth.auth.alg = auth_alg->alg;
		p->crypto.cipher_auth.auth.key_size = key_size;

		t += 2;
		n_tokens -= 2;

		if (key_size) {
			int status;

			CHECK(n_tokens >= 2, "Not enough tokens");
			CHECK(!strcmp(t[0], "key"), "Missing authentication \"key\" keyword");
			CHECK(key_size <= RTE_DIM(p->crypto.cipher_auth.auth.key),
				"Authentication algorithm key too big");

			status = hex_string_parse(t[1], p->crypto.cipher_auth.auth.key, key_size);
			CHECK(!status, "Authentication key invalid format");

			t += 2;
			n_tokens -= 2;
		}
	} else if (!strcmp(t[0], "aead")) {
		struct aead_alg *alg;
		uint32_t key_size;
		int status;

		p->crypto.is_aead = 1;

		CHECK(n_tokens >= 4, "Not enough tokens");
		alg = aead_alg_find(t[1]);
		CHECK(alg, "Unsupported AEAD algorithm");

		key_size = alg->key_size;
		p->crypto.aead.alg = alg->alg;
		p->crypto.aead.key_size = key_size;

		CHECK(!strcmp(t[2], "key"), "Missing AEAD \"key\" keyword");
		CHECK(key_size <= RTE_DIM(p->crypto.aead.key),
			"AEAD algorithm key too big");

		status = hex_string_parse(t[3], p->crypto.aead.key, key_size);
		CHECK(!status, "AEAD key invalid format");

		t += 4;
		n_tokens -= 4;
	} else
		CHECK(0, "Missing \"cipher\"/\"aead\" keyword");

	/*
	 * Packet ecapsulation parameters.
	 */
	CHECK(n_tokens >= 4, "Not enough tokens");
	CHECK(!strcmp(t[0], "esp"), "Missing \"esp\" keyword");
	CHECK(!strcmp(t[1], "spi"), "Missing \"spi\" keyword");

	p->encap.esp.spi = strtoul(t[2], &t[2], 0);
	CHECK(!t[2][0], "ESP SPI field invalid format");

	t += 3;
	n_tokens -= 3;

	if (!strcmp(t[0], "tunnel")) {
		p->encap.tunnel_mode = 1;

		CHECK(n_tokens >= 6, "Not enough tokens");

		if (!strcmp(t[1], "ipv4")) {
			uint32_t addr;

			p->encap.tunnel_ipv4 = 1;

			CHECK(!strcmp(t[2], "srcaddr"), "Missing \"srcaddr\" keyword");

			addr = strtoul(t[3], &t[3], 0);
			CHECK(!t[3][0], "Tunnel IPv4 source address invalid format");
			p->encap.tunnel.ipv4.src_addr.s_addr = htonl(addr);

			CHECK(!strcmp(t[4], "dstaddr"), "Missing \"dstaddr\" keyword");

			addr = strtoul(t[5], &t[5], 0);
			CHECK(!t[5][0], "Tunnel IPv4 destination address invalid format");
			p->encap.tunnel.ipv4.dst_addr.s_addr = htonl(addr);

			t += 6;
			n_tokens -= 6;
		} else if (!strcmp(t[1], "ipv6")) {
			int status;

			p->encap.tunnel_ipv4 = 0;

			CHECK(!strcmp(t[2], "srcaddr"), "Missing \"srcaddr\" keyword");

			status = hex_string_parse(t[3],
						  p->encap.tunnel.ipv6.src_addr.s6_addr,
						  16);
			CHECK(!status, "Tunnel IPv6 source address invalid format");

			CHECK(!strcmp(t[4], "dstaddr"), "Missing \"dstaddr\" keyword");

			status = hex_string_parse(t[5],
						  p->encap.tunnel.ipv6.dst_addr.s6_addr,
						  16);
			CHECK(!status, "Tunnel IPv6 destination address invalid format");

			t += 6;
			n_tokens -= 6;
		} else
			CHECK(0, "Missing \"ipv4\"/\"ipv6\" keyword");
	} else if (!strcmp(t[0], "transport")) {
		p->encap.tunnel_mode = 0;

		t++;
		n_tokens--;
	} else
		CHECK(0, "Missing \"tunnel\"/\"transport\" keyword");

	/*
	 * Any other parameters.
	 */
	CHECK(!n_tokens, "Unexpected trailing tokens");

	free(s0);
	return p;

error:
	free(p);
	free(s0);
	if (is_blank_or_comment)
		*is_blank_or_comment = blank_or_comment;
	return NULL;
}

static void
tunnel_ipv4_header_set(struct rte_ipv4_hdr *h, struct rte_swx_ipsec_sa_params *p)
{
	struct rte_ipv4_hdr ipv4_hdr = {
		.version_ihl = 0x45,
		.type_of_service = 0,
		.total_length = 0, /* Cannot be pre-computed. */
		.packet_id = 0,
		.fragment_offset = 0,
		.time_to_live = 64,
		.next_proto_id = IPPROTO_ESP,
		.hdr_checksum = 0, /* Cannot be pre-computed. */
		.src_addr = p->encap.tunnel.ipv4.src_addr.s_addr,
		.dst_addr = p->encap.tunnel.ipv4.dst_addr.s_addr,
	};

	memcpy(h, &ipv4_hdr, sizeof(ipv4_hdr));
}

static void
tunnel_ipv6_header_set(struct rte_ipv6_hdr *h, struct rte_swx_ipsec_sa_params *p)
{
	struct rte_ipv6_hdr ipv6_hdr = {
		.vtc_flow = 0x60000000,
		.payload_len = 0, /* Cannot be pre-computed. */
		.proto = IPPROTO_ESP,
		.hop_limits = 64,
		.src_addr = {0},
		.dst_addr = {0},
	};

	memcpy(h, &ipv6_hdr, sizeof(ipv6_hdr));
	memcpy(h->src_addr, p->encap.tunnel.ipv6.src_addr.s6_addr, 16);
	memcpy(h->dst_addr, p->encap.tunnel.ipv6.dst_addr.s6_addr, 16);
}

/* IPsec library SA parameters. */
static struct rte_crypto_sym_xform *
crypto_xform_get(struct rte_swx_ipsec_sa_params *p,
		struct rte_crypto_sym_xform *xform,
		uint32_t *salt_out)
{
	if (p->crypto.is_aead) {
		struct aead_alg *alg;
		uint32_t key_size, salt, iv_length;

		alg = aead_alg_find_by_id(p->crypto.aead.alg, p->crypto.aead.key_size);
		if (!alg)
			return NULL;

		/* salt and salt-related key size adjustment. */
		key_size = p->crypto.aead.key_size - 4;
		memcpy(&salt, &p->crypto.aead.key[key_size], 4);

		/* IV length. */
		iv_length = 12;
		if (p->crypto.aead.alg == RTE_CRYPTO_AEAD_AES_CCM)
			iv_length = 11;

		/* xform. */
		xform[0].type = RTE_CRYPTO_SYM_XFORM_AEAD;
		xform[0].aead.op = p->encrypt ?
			RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;
		xform[0].aead.algo = p->crypto.aead.alg;
		xform[0].aead.key.data = p->crypto.aead.key;
		xform[0].aead.key.length = key_size;
		xform[0].aead.iv.offset = IV_OFFSET;
		xform[0].aead.iv.length = iv_length;
		xform[0].aead.digest_length = alg->digest_size;
		xform[0].aead.aad_length = alg->aad_size;
		xform[0].next = NULL;

		*salt_out = salt;
		return &xform[0];
	} else {
		struct cipher_alg *cipher_alg;
		struct auth_alg *auth_alg;
		uint32_t cipher_key_size, auth_key_size, salt, auth_iv_length;

		cipher_alg = cipher_alg_find_by_id(p->crypto.cipher_auth.cipher.alg,
						   p->crypto.cipher_auth.cipher.key_size);
		if (!cipher_alg)
			return NULL;

		auth_alg = auth_alg_find_by_id(p->crypto.cipher_auth.auth.alg,
					       p->crypto.cipher_auth.auth.key_size);
		if (!auth_alg)
			return NULL;

		/* salt and salt-related key size adjustment. */
		cipher_key_size = p->crypto.cipher_auth.cipher.key_size;
		auth_key_size = p->crypto.cipher_auth.auth.key_size;

		switch (p->crypto.cipher_auth.cipher.alg) {
		case RTE_CRYPTO_CIPHER_AES_CBC:
		case RTE_CRYPTO_CIPHER_3DES_CBC:
			salt = (uint32_t)rand();
			break;

		case RTE_CRYPTO_CIPHER_AES_CTR:
			cipher_key_size -= 4;
			memcpy(&salt, &p->crypto.cipher_auth.cipher.key[cipher_key_size], 4);
			break;

		default:
			salt = 0;
		}

		if (p->crypto.cipher_auth.auth.alg == RTE_CRYPTO_AUTH_AES_GMAC) {
			auth_key_size -= 4;
			memcpy(&salt, &p->crypto.cipher_auth.auth.key[auth_key_size], 4);
		}

		/* IV length. */
		auth_iv_length = cipher_alg->iv_size;
		if (p->crypto.cipher_auth.auth.alg == RTE_CRYPTO_AUTH_AES_GMAC)
			auth_iv_length = 12;

		/* xform. */
		if (p->encrypt) {
			xform[0].type = RTE_CRYPTO_SYM_XFORM_CIPHER;
			xform[0].cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
			xform[0].cipher.algo = p->crypto.cipher_auth.cipher.alg;
			xform[0].cipher.key.data = p->crypto.cipher_auth.cipher.key;
			xform[0].cipher.key.length = cipher_key_size;
			xform[0].cipher.iv.offset = IV_OFFSET;
			xform[0].cipher.iv.length = cipher_alg->iv_size;
			xform[0].cipher.dataunit_len = 0;
			xform[0].next = &xform[1];

			xform[1].type = RTE_CRYPTO_SYM_XFORM_AUTH;
			xform[1].auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
			xform[1].auth.algo = p->crypto.cipher_auth.auth.alg;
			xform[1].auth.key.data = p->crypto.cipher_auth.auth.key;
			xform[1].auth.key.length = auth_key_size;
			xform[1].auth.iv.offset = IV_OFFSET;
			xform[1].auth.iv.length = auth_iv_length;
			xform[1].auth.digest_length = auth_alg->digest_size;
			xform[1].next = NULL;
		} else {
			xform[0].type = RTE_CRYPTO_SYM_XFORM_AUTH;
			xform[0].auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
			xform[0].auth.algo = p->crypto.cipher_auth.auth.alg;
			xform[0].auth.key.data = p->crypto.cipher_auth.auth.key;
			xform[0].auth.key.length = auth_key_size;
			xform[0].auth.iv.offset = IV_OFFSET;
			xform[0].auth.iv.length = auth_iv_length;
			xform[0].auth.digest_length = auth_alg->digest_size;
			xform[0].next = &xform[1];

			xform[1].type = RTE_CRYPTO_SYM_XFORM_CIPHER;
			xform[1].cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
			xform[1].cipher.algo = p->crypto.cipher_auth.cipher.alg;
			xform[1].cipher.key.data = p->crypto.cipher_auth.cipher.key;
			xform[1].cipher.key.length = cipher_key_size;
			xform[1].cipher.iv.offset = IV_OFFSET;
			xform[1].cipher.iv.length = cipher_alg->iv_size;
			xform[1].cipher.dataunit_len = 0;
			xform[1].next = NULL;
		}

		*salt_out = salt;

		if (p->crypto.cipher_auth.auth.alg == RTE_CRYPTO_AUTH_AES_GMAC) {
			if (p->encrypt)
				return &xform[1];

			xform[0].next = NULL;
			return &xform[0];
		}

		return &xform[0];
	}
}

static void
ipsec_xform_get(struct rte_swx_ipsec_sa_params *p,
		struct rte_security_ipsec_xform *ipsec_xform,
		uint32_t salt)
{
	ipsec_xform->spi = p->encap.esp.spi;

	ipsec_xform->salt = salt;

	ipsec_xform->options.esn = 0;
	ipsec_xform->options.udp_encap = 0;
	ipsec_xform->options.copy_dscp = 1;
	ipsec_xform->options.copy_flabel = 0;
	ipsec_xform->options.copy_df = 0;
	ipsec_xform->options.dec_ttl = 0;
	ipsec_xform->options.ecn = 1;
	ipsec_xform->options.stats = 0;
	ipsec_xform->options.iv_gen_disable = 0;
	ipsec_xform->options.tunnel_hdr_verify = 0;
	ipsec_xform->options.udp_ports_verify = 0;
	ipsec_xform->options.ip_csum_enable = 0;
	ipsec_xform->options.l4_csum_enable = 0;
	ipsec_xform->options.ip_reassembly_en = 0;
	ipsec_xform->options.reserved_opts = 0;

	ipsec_xform->direction = p->encrypt ?
		RTE_SECURITY_IPSEC_SA_DIR_EGRESS :
		RTE_SECURITY_IPSEC_SA_DIR_INGRESS;

	ipsec_xform->proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;

	ipsec_xform->mode = p->encap.tunnel_mode ?
		RTE_SECURITY_IPSEC_SA_MODE_TUNNEL :
		RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT;

	ipsec_xform->tunnel.type = p->encap.tunnel_ipv4 ?
		RTE_SECURITY_IPSEC_TUNNEL_IPV4 :
		RTE_SECURITY_IPSEC_TUNNEL_IPV6;

	if (p->encap.tunnel_mode) {
		if (p->encap.tunnel_ipv4) {
			ipsec_xform->tunnel.ipv4.src_ip = p->encap.tunnel.ipv4.src_addr;
			ipsec_xform->tunnel.ipv4.dst_ip = p->encap.tunnel.ipv4.dst_addr;
			ipsec_xform->tunnel.ipv4.dscp = 0;
			ipsec_xform->tunnel.ipv4.df = 0;
			ipsec_xform->tunnel.ipv4.ttl = 64;
		} else {
			ipsec_xform->tunnel.ipv6.src_addr = p->encap.tunnel.ipv6.src_addr;
			ipsec_xform->tunnel.ipv6.dst_addr = p->encap.tunnel.ipv6.dst_addr;
			ipsec_xform->tunnel.ipv6.dscp = 0;
			ipsec_xform->tunnel.ipv6.flabel = 0;
			ipsec_xform->tunnel.ipv6.hlimit = 64;
		}
	}

	ipsec_xform->life.packets_soft_limit = 0;
	ipsec_xform->life.bytes_soft_limit = 0;
	ipsec_xform->life.packets_hard_limit = 0;
	ipsec_xform->life.bytes_hard_limit = 0;

	ipsec_xform->replay_win_sz = 0;

	ipsec_xform->esn.value = 0;

	ipsec_xform->udp.dport = 0;
	ipsec_xform->udp.sport = 0;
}

static int
ipsec_sa_prm_get(struct rte_swx_ipsec_sa_params *p,
		 struct rte_ipsec_sa_prm *sa_prm,
		 struct rte_ipv4_hdr *ipv4_hdr,
		 struct rte_ipv6_hdr *ipv6_hdr,
		 struct rte_crypto_sym_xform *crypto_xform)
{
	uint32_t salt;

	memset(sa_prm, 0, sizeof(*sa_prm)); /* Better to be safe than sorry. */

	sa_prm->userdata = 0; /* Not used. */

	sa_prm->flags = 0; /* Flag RTE_IPSEC_SAFLAG_SQN_ATOM not enabled. */

	/*
	 * crypto_xform.
	 */
	sa_prm->crypto_xform = crypto_xform_get(p, crypto_xform, &salt);
	if (!sa_prm->crypto_xform)
		return -EINVAL;

	/*
	 * ipsec_xform.
	 */
	ipsec_xform_get(p, &sa_prm->ipsec_xform, salt);

	/*
	 * tunnel / transport.
	 *
	 * Currently, the input IP packet type is assumed to be IPv4. To support both IPv4 and IPv6,
	 * the input packet type should be added to the SA configuration parameters.
	 */
	if (p->encap.tunnel_mode) {
		if (p->encap.tunnel_ipv4) {
			sa_prm->tun.hdr_len = sizeof(struct rte_ipv4_hdr);
			sa_prm->tun.hdr_l3_off = 0;
			sa_prm->tun.next_proto = IPPROTO_IPIP; /* IPv4. */
			sa_prm->tun.hdr = ipv4_hdr;
		} else {
			sa_prm->tun.hdr_len = sizeof(struct rte_ipv6_hdr);
			sa_prm->tun.hdr_l3_off = 0;
			sa_prm->tun.next_proto = IPPROTO_IPIP; /* IPv4. */
			sa_prm->tun.hdr = ipv6_hdr;
		}
	} else {
		sa_prm->trs.proto = IPPROTO_IPIP; /* IPv4. */
	}

	return 0;
}

static int
ipsec_session_create(struct rte_swx_ipsec *ipsec,
		     struct rte_swx_ipsec_sa_params *p,
		     struct rte_ipsec_session *s)
{
	struct rte_ipv4_hdr ipv4_hdr;
	struct rte_ipv6_hdr ipv6_hdr;
	struct rte_crypto_sym_xform crypto_xform[2];
	struct rte_ipsec_sa_prm sa_prm;
	struct rte_ipsec_sa *sa = NULL;
	struct rte_cryptodev_sym_session *crypto_session = NULL;
	int sa_size;
	int sa_valid = 0, status = 0;

	tunnel_ipv4_header_set(&ipv4_hdr, p);
	tunnel_ipv6_header_set(&ipv6_hdr, p);

	/* IPsec library SA setup. */
	status = ipsec_sa_prm_get(p, &sa_prm, &ipv4_hdr, &ipv6_hdr, crypto_xform);
	if (status)
		goto error;

	sa_size = rte_ipsec_sa_size(&sa_prm);
	if (sa_size < 0) {
		status = sa_size;
		goto error;
	}
	if (!sa_size) {
		status = -EINVAL;
		goto error;
	}

	sa = calloc(1, sa_size);
	if (!sa) {
		status = -ENOMEM;
		goto error;
	}

	sa_size = rte_ipsec_sa_init(sa, &sa_prm, sa_size);
	if (sa_size < 0) {
		status = sa_size;
		goto error;
	}
	if (!sa_size) {
		status = -EINVAL;
		goto error;
	}

	sa_valid = 1;

	/* Cryptodev library session setup. */
	crypto_session = rte_cryptodev_sym_session_create(ipsec->dev_id,
							  sa_prm.crypto_xform,
							  ipsec->mp_session);
	if (!crypto_session) {
		status = -ENOMEM;
		goto error;
	}

	/* IPsec library session setup. */
	s->sa = sa;
	s->type = RTE_SECURITY_ACTION_TYPE_NONE;
	s->crypto.ses = crypto_session;
	s->crypto.dev_id = ipsec->dev_id;
	s->pkt_func.prepare.async = NULL;
	s->pkt_func.process = NULL;

	status = rte_ipsec_session_prepare(s);
	if (status)
		goto error;

	return 0;

error:
	/* sa. */
	if (sa_valid)
		rte_ipsec_sa_fini(sa);

	free(sa);

	/* crypto_session. */
	if (crypto_session)
		rte_cryptodev_sym_session_free(ipsec->dev_id, crypto_session);

	/* s. */
	memset(s, 0, sizeof(*s));

	return status;
}

static void
ipsec_session_free(struct rte_swx_ipsec *ipsec,
		   struct rte_ipsec_session *s)
{
	if (!s)
		return;

	/* IPsec library SA. */
	if (s->sa)
		rte_ipsec_sa_fini(s->sa);
	free(s->sa);

	/* Cryptodev library session. */
	if (s->crypto.ses)
		rte_cryptodev_sym_session_free(ipsec->dev_id, s->crypto.ses);

	/* IPsec library session. */
	memset(s, 0, sizeof(*s));
}

int
rte_swx_ipsec_sa_add(struct rte_swx_ipsec *ipsec,
		     struct rte_swx_ipsec_sa_params *sa_params,
		     uint32_t *id)
{
	struct ipsec_sa *sa;
	uint32_t sa_id;
	int status;

	/* Check the input parameters. */
	if (!ipsec || !sa_params || !id)
		return -EINVAL;

	/* Allocate a free SADB entry. */
	if (!ipsec->n_sa_free_id)
		return -ENOSPC;

	sa_id = ipsec->sa_free_id[ipsec->n_sa_free_id - 1];
	ipsec->n_sa_free_id--;

	/* Acquire the SA resources. */
	sa = ipsec_sa_get(ipsec, sa_id);

	status = ipsec_session_create(ipsec, sa_params, &sa->s);
	if (status) {
		/* Free the allocated SADB entry. */
		ipsec->sa_free_id[ipsec->n_sa_free_id] = sa_id;
		ipsec->n_sa_free_id++;

		return status;
	}

	/* Validate the new SA. */
	sa->valid = 1;
	*id = sa_id;

	return 0;
}

void
rte_swx_ipsec_sa_delete(struct rte_swx_ipsec *ipsec,
			uint32_t sa_id)
{
	struct ipsec_sa *sa;

	/* Check the input parameters. */
	if (!ipsec || (sa_id >= ipsec->n_sa_max))
		return;

	/* Release the SA resources. */
	sa = ipsec_sa_get(ipsec, sa_id);

	ipsec_session_free(ipsec, &sa->s);

	/* Free the SADB entry. */
	ipsec->sa_free_id[ipsec->n_sa_free_id] = sa_id;
	ipsec->n_sa_free_id++;

	/* Invalidate the SA. */
	sa->valid = 0;
}
