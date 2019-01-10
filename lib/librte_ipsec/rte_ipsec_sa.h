/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_IPSEC_SA_H_
#define _RTE_IPSEC_SA_H_

/**
 * @file rte_ipsec_sa.h
 * @b EXPERIMENTAL: this API may change without prior notice
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

	/**
	 * window size to enable sequence replay attack handling.
	 * replay checking is disabled if the window size is 0.
	 */
	uint32_t replay_win_sz;
};

/**
 * SA type is an 64-bit value that contain the following information:
 * - IP version (IPv4/IPv6)
 * - IPsec proto (ESP/AH)
 * - inbound/outbound
 * - mode (TRANSPORT/TUNNEL)
 * - for TUNNEL outer IP version (IPv4/IPv6)
 * ...
 */

enum {
	RTE_SATP_LOG2_IPV,
	RTE_SATP_LOG2_PROTO,
	RTE_SATP_LOG2_DIR,
	RTE_SATP_LOG2_MODE,
	RTE_SATP_LOG2_NUM
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

/**
 * get type of given SA
 * @return
 *   SA type value.
 */
uint64_t __rte_experimental
rte_ipsec_sa_type(const struct rte_ipsec_sa *sa);

/**
 * Calculate required SA size based on provided input parameters.
 * @param prm
 *   Parameters that wil be used to initialise SA object.
 * @return
 *   - Actual size required for SA with given parameters.
 *   - -EINVAL if the parameters are invalid.
 */
int __rte_experimental
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
int __rte_experimental
rte_ipsec_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	uint32_t size);

/**
 * cleanup SA
 * @param sa
 *   Pointer to SA object to de-initialize.
 */
void __rte_experimental
rte_ipsec_sa_fini(struct rte_ipsec_sa *sa);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IPSEC_SA_H_ */
