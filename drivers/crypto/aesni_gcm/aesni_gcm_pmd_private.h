/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#ifndef _RTE_AESNI_GCM_PMD_PRIVATE_H_
#define _RTE_AESNI_GCM_PMD_PRIVATE_H_

#include "aesni_gcm_ops.h"

#define GCM_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_AESNI_MB_DEBUG
#define GCM_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD), \
			__func__, __LINE__, ## args)

#define GCM_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD), \
			__func__, __LINE__, ## args)
#else
#define GCM_LOG_INFO(fmt, args...)
#define GCM_LOG_DBG(fmt, args...)
#endif


/** private data structure for each virtual AESNI GCM device */
struct aesni_gcm_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
};

struct aesni_gcm_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
} __rte_cache_aligned;


enum aesni_gcm_operation {
	AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION,
	AESNI_GCM_OP_AUTHENTICATED_DECRYPTION
};

enum aesni_gcm_key {
	AESNI_GCM_KEY_128,
	AESNI_GCM_KEY_256
};

/** AESNI GCM private session structure */
struct aesni_gcm_session {
	enum aesni_gcm_operation op;
	/**< GCM operation type */
	enum aesni_gcm_key key;
	/**< GCM key type */
	struct gcm_data gdata __rte_cache_aligned;
	/**< GCM parameters */
};


/**
 * Setup GCM session parameters
 * @param	sess	aesni gcm session structure
 * @param	xform	crypto transform chain
 *
 * @return
 * - On success returns 0
 * - On failure returns error code < 0
 */
extern int
aesni_gcm_set_session_parameters(struct aesni_gcm_session *sess,
		const struct rte_crypto_sym_xform *xform);


/**
 * Device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_aesni_gcm_pmd_ops;


#endif /* _RTE_AESNI_GCM_PMD_PRIVATE_H_ */
