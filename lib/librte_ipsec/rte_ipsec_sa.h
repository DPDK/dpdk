/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_IPSEC_SA_H_
#define _RTE_IPSEC_SA_H_

/**
 * @file rte_ipsec_sa.h
 *
 * Defines API to manage IPsec Security Association (SA) objects.
 */

#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_security.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An opaque structure to represent Security Association (SA).
 */
struct rte_ipsec_sa;

/**
 * SA initialization parameters.
 */
struct rte_ipsec_sa_prm {

	uint64_t userdata; /**< provided and interpreted by user */
	uint64_t flags;  /**< see RTE_IPSEC_SAFLAG_* below */
	/** ipsec configuration */
	struct rte_security_ipsec_xform ipsec_xform;
	/** crypto session configuration */
	struct rte_crypto_sym_xform *crypto_xform;
	union {
		struct {
			uint8_t hdr_len;     /**< tunnel header len */
			uint8_t hdr_l3_off;  /**< offset for IPv4/IPv6 header */
			uint8_t next_proto;  /**< next header protocol */
			const void *hdr;     /**< tunnel header template */
		} tun; /**< tunnel mode related parameters */
		struct {
			uint8_t proto;  /**< next header protocol */
		} trs; /**< transport mode related parameters */
	};
};

/**
 * Indicates that SA will(/will not) need an 'atomic' access
 * to sequence number and replay window.
 * 'atomic' here means:
 * functions:
 *  - rte_ipsec_pkt_crypto_prepare
 *  - rte_ipsec_pkt_process
 * can be safely used in MT environment, as long as the user can guarantee
 * that they obey multiple readers/single writer model for SQN+replay_window
 * operations.
 * To be more specific:
 * for outbound SA there are no restrictions.
 * for inbound SA the caller has to guarantee that at any given moment
 * only one thread is executing rte_ipsec_pkt_process() for given SA.
 * Note that it is caller responsibility to maintain correct order
 * of packets to be processed.
 * In other words - it is a caller responsibility to serialize process()
 * invocations.
 */
#define	RTE_IPSEC_SAFLAG_SQN_ATOM	(1ULL << 0)

/**
 * SA type is an 64-bit value that contain the following information:
 * - IP version (IPv4/IPv6)
 * - IPsec proto (ESP/AH)
 * - inbound/outbound
 * - mode (TRANSPORT/TUNNEL)
 * - for TUNNEL outer IP version (IPv4/IPv6)
 * - are SA SQN operations 'atomic'
 * - ESN enabled/disabled
 * ...
 */

enum {
	RTE_SATP_LOG2_IPV,
	RTE_SATP_LOG2_PROTO,
	RTE_SATP_LOG2_DIR,
	RTE_SATP_LOG2_MODE,
	RTE_SATP_LOG2_SQN = RTE_SATP_LOG2_MODE + 2,
	RTE_SATP_LOG2_ESN,
	RTE_SATP_LOG2_ECN,
	RTE_SATP_LOG2_DSCP
};

#define RTE_IPSEC_SATP_IPV_MASK		(1ULL << RTE_SATP_LOG2_IPV)
#define RTE_IPSEC_SATP_IPV4		(0ULL << RTE_SATP_LOG2_IPV)
#define RTE_IPSEC_SATP_IPV6		(1ULL << RTE_SATP_LOG2_IPV)

#define RTE_IPSEC_SATP_PROTO_MASK	(1ULL << RTE_SATP_LOG2_PROTO)
#define RTE_IPSEC_SATP_PROTO_AH		(0ULL << RTE_SATP_LOG2_PROTO)
#define RTE_IPSEC_SATP_PROTO_ESP	(1ULL << RTE_SATP_LOG2_PROTO)

#define RTE_IPSEC_SATP_DIR_MASK		(1ULL << RTE_SATP_LOG2_DIR)
#define RTE_IPSEC_SATP_DIR_IB		(0ULL << RTE_SATP_LOG2_DIR)
#define RTE_IPSEC_SATP_DIR_OB		(1ULL << RTE_SATP_LOG2_DIR)

#define RTE_IPSEC_SATP_MODE_MASK	(3ULL << RTE_SATP_LOG2_MODE)
#define RTE_IPSEC_SATP_MODE_TRANS	(0ULL << RTE_SATP_LOG2_MODE)
#define RTE_IPSEC_SATP_MODE_TUNLV4	(1ULL << RTE_SATP_LOG2_MODE)
#define RTE_IPSEC_SATP_MODE_TUNLV6	(2ULL << RTE_SATP_LOG2_MODE)

#define RTE_IPSEC_SATP_SQN_MASK		(1ULL << RTE_SATP_LOG2_SQN)
#define RTE_IPSEC_SATP_SQN_RAW		(0ULL << RTE_SATP_LOG2_SQN)
#define RTE_IPSEC_SATP_SQN_ATOM		(1ULL << RTE_SATP_LOG2_SQN)

#define RTE_IPSEC_SATP_ESN_MASK		(1ULL << RTE_SATP_LOG2_ESN)
#define RTE_IPSEC_SATP_ESN_DISABLE	(0ULL << RTE_SATP_LOG2_ESN)
#define RTE_IPSEC_SATP_ESN_ENABLE	(1ULL << RTE_SATP_LOG2_ESN)

#define RTE_IPSEC_SATP_ECN_MASK		(1ULL << RTE_SATP_LOG2_ECN)
#define RTE_IPSEC_SATP_ECN_DISABLE	(0ULL << RTE_SATP_LOG2_ECN)
#define RTE_IPSEC_SATP_ECN_ENABLE	(1ULL << RTE_SATP_LOG2_ECN)

#define RTE_IPSEC_SATP_DSCP_MASK	(1ULL << RTE_SATP_LOG2_DSCP)
#define RTE_IPSEC_SATP_DSCP_DISABLE	(0ULL << RTE_SATP_LOG2_DSCP)
#define RTE_IPSEC_SATP_DSCP_ENABLE	(1ULL << RTE_SATP_LOG2_DSCP)

/**
 * get type of given SA
 * @return
 *   SA type value.
 */
uint64_t
rte_ipsec_sa_type(const struct rte_ipsec_sa *sa);

/**
 * Calculate required SA size based on provided input parameters.
 * @param prm
 *   Parameters that will be used to initialise SA object.
 * @return
 *   - Actual size required for SA with given parameters.
 *   - -EINVAL if the parameters are invalid.
 */
int
rte_ipsec_sa_size(const struct rte_ipsec_sa_prm *prm);

/**
 * initialise SA based on provided input parameters.
 * @param sa
 *   SA object to initialise.
 * @param prm
 *   Parameters used to initialise given SA object.
 * @param size
 *   size of the provided buffer for SA.
 * @return
 *   - Actual size of SA object if operation completed successfully.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if the size of the provided buffer is not big enough.
 */
int
rte_ipsec_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	uint32_t size);

/**
 * cleanup SA
 * @param sa
 *   Pointer to SA object to de-initialize.
 */
void
rte_ipsec_sa_fini(struct rte_ipsec_sa *sa);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IPSEC_SA_H_ */
