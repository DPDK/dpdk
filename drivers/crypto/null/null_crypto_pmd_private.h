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

#ifndef _NULL_CRYPTO_PMD_PRIVATE_H_
#define _NULL_CRYPTO_PMD_PRIVATE_H_

#include "rte_config.h"

#define NULL_CRYPTO_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_NULL_CRYPTO_DEBUG
#define NULL_CRYPTO_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)

#define NULL_CRYPTO_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)
#else
#define NULL_CRYPTO_LOG_INFO(fmt, args...)
#define NULL_CRYPTO_LOG_DBG(fmt, args...)
#endif


/** private data structure for each NULL crypto device */
struct null_crypto_private {
	unsigned max_nb_qpairs;		/**< Max number of queue pairs */
	unsigned max_nb_sessions;	/**< Max number of sessions */
};

/** NULL crypto queue pair */
struct null_crypto_qp {
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


/** NULL crypto private session structure */
struct null_crypto_session {
	uint32_t reserved;
} __rte_cache_aligned;

/** Set and validate NULL crypto session parameters */
extern int
null_crypto_set_session_parameters(struct null_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *null_crypto_pmd_ops;

#endif /* _NULL_CRYPTO_PMD_PRIVATE_H_ */
