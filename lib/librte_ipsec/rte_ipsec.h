/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2020 Intel Corporation
 */

#ifndef _RTE_IPSEC_H_
#define _RTE_IPSEC_H_

/**
 * @file rte_ipsec.h
 *
 * RTE IPsec support.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * librte_ipsec provides a framework for data-path IPsec protocol
 * processing (ESP/AH).
 */

#include <rte_ipsec_sa.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_ipsec_session;

/**
 * IPsec session specific functions that will be used to:
 * - prepare - for input mbufs and given IPsec session prepare crypto ops
 *   that can be enqueued into the cryptodev associated with given session
 *   (see *rte_ipsec_pkt_crypto_prepare* below for more details).
 * - process - finalize processing of packets after crypto-dev finished
 *   with them or process packets that are subjects to inline IPsec offload
 *   (see rte_ipsec_pkt_process for more details).
 */
struct rte_ipsec_sa_pkt_func {
	union {
		uint16_t (*async)(const struct rte_ipsec_session *ss,
				struct rte_mbuf *mb[],
				struct rte_crypto_op *cop[],
				uint16_t num);
		uint16_t (*sync)(const struct rte_ipsec_session *ss,
				struct rte_mbuf *mb[],
				uint16_t num);
	} prepare;
	uint16_t (*process)(const struct rte_ipsec_session *ss,
				struct rte_mbuf *mb[],
				uint16_t num);
};

/**
 * rte_ipsec_session is an aggregate structure that defines particular
 * IPsec Security Association IPsec (SA) on given security/crypto device:
 * - pointer to the SA object
 * - security session action type
 * - pointer to security/crypto session, plus other related data
 * - session/device specific functions to prepare/process IPsec packets.
 */
struct rte_ipsec_session {
	/**
	 * SA that session belongs to.
	 * Note that multiple sessions can belong to the same SA.
	 */
	struct rte_ipsec_sa *sa;
	/** session action type */
	enum rte_security_session_action_type type;
	/** session and related data */
	union {
		struct {
			struct rte_cryptodev_sym_session *ses;
			uint8_t dev_id;
		} crypto;
		struct {
			struct rte_security_session *ses;
			struct rte_security_ctx *ctx;
			uint32_t ol_flags;
		} security;
	};
	/** functions to prepare/process IPsec packets */
	struct rte_ipsec_sa_pkt_func pkt_func;
} __rte_cache_aligned;

/**
 * Checks that inside given rte_ipsec_session crypto/security fields
 * are filled correctly and setups function pointers based on these values.
 * Expects that all fields except IPsec processing function pointers
 * (*pkt_func*) will be filled correctly by caller.
 * @param ss
 *   Pointer to the *rte_ipsec_session* object
 * @return
 *   - Zero if operation completed successfully.
 *   - -EINVAL if the parameters are invalid.
 */
__rte_experimental
int
rte_ipsec_session_prepare(struct rte_ipsec_session *ss);

/**
 * For input mbufs and given IPsec session prepare crypto ops that can be
 * enqueued into the cryptodev associated with given session.
 * expects that for each input packet:
 *      - l2_len, l3_len are setup correctly
 * Note that erroneous mbufs are not freed by the function,
 * but are placed beyond last valid mbuf in the *mb* array.
 * It is a user responsibility to handle them further.
 * @param ss
 *   Pointer to the *rte_ipsec_session* object the packets belong to.
 * @param mb
 *   The address of an array of *num* pointers to *rte_mbuf* structures
 *   which contain the input packets.
 * @param cop
 *   The address of an array of *num* pointers to the output *rte_crypto_op*
 *   structures.
 * @param num
 *   The maximum number of packets to process.
 * @return
 *   Number of successfully processed packets, with error code set in rte_errno.
 */
__rte_experimental
static inline uint16_t
rte_ipsec_pkt_crypto_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], struct rte_crypto_op *cop[], uint16_t num)
{
	return ss->pkt_func.prepare.async(ss, mb, cop, num);
}

__rte_experimental
static inline uint16_t
rte_ipsec_pkt_cpu_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	return ss->pkt_func.prepare.sync(ss, mb, num);
}

/**
 * Finalise processing of packets after crypto-dev finished with them or
 * process packets that are subjects to inline IPsec offload.
 * Expects that for each input packet:
 *      - l2_len, l3_len are setup correctly
 * Output mbufs will be:
 * inbound - decrypted & authenticated, ESP(AH) related headers removed,
 * *l2_len* and *l3_len* fields are updated.
 * outbound - appropriate mbuf fields (ol_flags, tx_offloads, etc.)
 * properly setup, if necessary - IP headers updated, ESP(AH) fields added,
 * Note that erroneous mbufs are not freed by the function,
 * but are placed beyond last valid mbuf in the *mb* array.
 * It is a user responsibility to handle them further.
 * @param ss
 *   Pointer to the *rte_ipsec_session* object the packets belong to.
 * @param mb
 *   The address of an array of *num* pointers to *rte_mbuf* structures
 *   which contain the input packets.
 * @param num
 *   The maximum number of packets to process.
 * @return
 *   Number of successfully processed packets, with error code set in rte_errno.
 */
__rte_experimental
static inline uint16_t
rte_ipsec_pkt_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	return ss->pkt_func.process(ss, mb, num);
}

#include <rte_ipsec_group.h>

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IPSEC_H_ */
