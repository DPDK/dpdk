/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef RTE_PDCP_GROUP_H
#define RTE_PDCP_GROUP_H

/**
 * @file rte_pdcp_group.h
 *
 * RTE PDCP grouping support.
 * It is not recommended to include this file directly, include <rte_pdcp.h>
 * instead.
 * Provides helper functions to process completed crypto-ops and group related
 * packets by sessions they belong to.
 */

#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Group packets belonging to same PDCP entity.
 */
struct rte_pdcp_group {
	union {
		uint64_t val;
		void *ptr;
	} id; /**< Grouped by value */
	struct rte_mbuf **m;  /**< Start of the group */
	uint32_t cnt;         /**< Number of entries in the group */
	int32_t rc;           /**< Status code associated with the group */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Take crypto-op as an input and extract pointer to related PDCP entity.
 * @param cop
 *   The address of an input *rte_crypto_op* structure.
 * @return
 *   The pointer to the related *rte_pdcp_entity* structure.
 */
static inline struct rte_pdcp_entity *
rte_pdcp_en_from_cop(const struct rte_crypto_op *cop)
{
	void *sess = cop->sym[0].session;

	return (struct rte_pdcp_entity *)(uintptr_t)
		rte_cryptodev_sym_session_opaque_data_get(sess);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Take as input completed crypto ops, extract related mbufs and group them by
 * *rte_pdcp_entity* they belong to. Mbuf for which the crypto operation has
 * failed would be flagged using *RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED* flag
 * in rte_mbuf.ol_flags. The crypto_ops would be freed after the grouping.
 *
 * Note that application must ensure only crypto-ops prepared by lib_pdcp is
 * provided back to @see rte_pdcp_pkt_crypto_group().
 *
 * @param cop
 *   The address of an array of *num* pointers to the input *rte_crypto_op*
 *   structures.
 * @param[out] mb
 *   The address of an array of *num* pointers to output *rte_mbuf* structures.
 * @param[out] grp
 *   The address of an array of *num* to output *rte_pdcp_group* structures.
 * @param num
 *   The maximum number of crypto-ops to process.
 * @return
 *   Number of filled elements in *grp* array.
 */
static inline uint16_t
rte_pdcp_pkt_crypto_group(struct rte_crypto_op *cop[], struct rte_mbuf *mb[],
			  struct rte_pdcp_group grp[], uint16_t num)
{
	uint32_t i, j = 0, n = 0;
	void *ns, *ps = NULL;
	struct rte_mbuf *m;

	for (i = 0; i != num; i++) {
		m = cop[i]->sym[0].m_src;
		ns = cop[i]->sym[0].session;

		m->ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD;
		if (cop[i]->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
			m->ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;

		/* Different entity */
		if (ps != ns) {

			/* Finalize open group and start a new one */
			if (ps != NULL) {
				grp[n].cnt = mb + j - grp[n].m;
				n++;
			}

			/* Start new group */
			grp[n].m = mb + j;
			ps = ns;
			grp[n].id.ptr =	rte_pdcp_en_from_cop(cop[i]);
		}

		mb[j++] = m;
		rte_crypto_op_free(cop[i]);
	}

	/* Finalize last group */
	if (ps != NULL) {
		grp[n].cnt = mb + j - grp[n].m;
		n++;
	}

	return n;
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PDCP_GROUP_H */
