/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_IPSEC_GROUP_H_
#define _RTE_IPSEC_GROUP_H_

/**
 * @file rte_ipsec_group.h
 *
 * RTE IPsec support.
 * It is not recommended to include this file directly,
 * include <rte_ipsec.h> instead.
 * Contains helper functions to process completed crypto-ops
 * and group related packets by sessions they belong to.
 */


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Used to group mbufs by some id.
 * See below for particular usage.
 */
struct rte_ipsec_group {
	union {
		uint64_t val;
		void *ptr;
	} id; /**< grouped by value */
	struct rte_mbuf **m;  /**< start of the group */
	uint32_t cnt;         /**< number of entries in the group */
	int32_t rc;           /**< status code associated with the group */
};

/**
 * Take crypto-op as an input and extract pointer to related ipsec session.
 * @param cop
 *   The address of an input *rte_crypto_op* structure.
 * @return
 *   The pointer to the related *rte_ipsec_session* structure.
 */
static inline struct rte_ipsec_session *
rte_ipsec_ses_from_crypto(const struct rte_crypto_op *cop)
{
	const struct rte_security_session *ss;
	const struct rte_cryptodev_sym_session *cs;

	if (cop->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		ss = cop->sym[0].sec_session;
		return (void *)(uintptr_t)ss->opaque_data;
	} else if (cop->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		cs = cop->sym[0].session;
		return (void *)(uintptr_t)cs->opaque_data;
	}
	return NULL;
}

/**
 * Take as input completed crypto ops, extract related mbufs
 * and group them by rte_ipsec_session they belong to.
 * For mbuf which crypto-op wasn't completed successfully
 * PKT_RX_SEC_OFFLOAD_FAILED will be raised in ol_flags.
 * Note that mbufs with undetermined SA (session-less) are not freed
 * by the function, but are placed beyond mbufs for the last valid group.
 * It is a user responsibility to handle them further.
 * @param cop
 *   The address of an array of *num* pointers to the input *rte_crypto_op*
 *   structures.
 * @param mb
 *   The address of an array of *num* pointers to output *rte_mbuf* structures.
 * @param grp
 *   The address of an array of *num* to output *rte_ipsec_group* structures.
 * @param num
 *   The maximum number of crypto-ops to process.
 * @return
 *   Number of filled elements in *grp* array.
 */
static inline uint16_t
rte_ipsec_pkt_crypto_group(const struct rte_crypto_op *cop[],
	struct rte_mbuf *mb[], struct rte_ipsec_group grp[], uint16_t num)
{
	uint32_t i, j, k, n;
	void *ns, *ps;
	struct rte_mbuf *m, *dr[num];

	j = 0;
	k = 0;
	n = 0;
	ps = NULL;

	for (i = 0; i != num; i++) {

		m = cop[i]->sym[0].m_src;
		ns = cop[i]->sym[0].session;

		m->ol_flags |= PKT_RX_SEC_OFFLOAD;
		if (cop[i]->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
			m->ol_flags |= PKT_RX_SEC_OFFLOAD_FAILED;

		/* no valid session found */
		if (ns == NULL) {
			dr[k++] = m;
			continue;
		}

		/* different SA */
		if (ps != ns) {

			/*
			 * we already have an open group - finalize it,
			 * then open a new one.
			 */
			if (ps != NULL) {
				grp[n].id.ptr =
					rte_ipsec_ses_from_crypto(cop[i - 1]);
				grp[n].cnt = mb + j - grp[n].m;
				n++;
			}

			/* start new group */
			grp[n].m = mb + j;
			ps = ns;
		}

		mb[j++] = m;
	}

	/* finalise last group */
	if (ps != NULL) {
		grp[n].id.ptr = rte_ipsec_ses_from_crypto(cop[i - 1]);
		grp[n].cnt = mb + j - grp[n].m;
		n++;
	}

	/* copy mbufs with unknown session beyond recognised ones */
	if (k != 0 && k != num) {
		for (i = 0; i != k; i++)
			mb[j + i] = dr[i];
	}

	return n;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IPSEC_GROUP_H_ */
