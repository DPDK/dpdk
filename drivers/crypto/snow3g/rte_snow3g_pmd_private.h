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

#ifndef _RTE_SNOW3G_PMD_PRIVATE_H_
#define _RTE_SNOW3G_PMD_PRIVATE_H_

#include <sso_snow3g.h>

#define SNOW3G_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_SNOW3G_DEBUG
#define SNOW3G_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)

#define SNOW3G_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)
#else
#define SNOW3G_LOG_INFO(fmt, args...)
#define SNOW3G_LOG_DBG(fmt, args...)
#endif

/** private data structure for each virtual SNOW 3G device */
struct snow3g_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
};

/** SNOW 3G buffer queue pair */
struct snow3g_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_ops;
	/**< Ring for placing processed ops */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
} __rte_cache_aligned;

enum snow3g_operation {
	SNOW3G_OP_ONLY_CIPHER,
	SNOW3G_OP_ONLY_AUTH,
	SNOW3G_OP_CIPHER_AUTH,
	SNOW3G_OP_AUTH_CIPHER,
	SNOW3G_OP_NOT_SUPPORTED
};

/** SNOW 3G private session structure */
struct snow3g_session {
	enum snow3g_operation op;
	enum rte_crypto_auth_operation auth_op;
	sso_snow3g_key_schedule_t pKeySched_cipher;
	sso_snow3g_key_schedule_t pKeySched_hash;
} __rte_cache_aligned;


extern int
snow3g_set_session_parameters(struct snow3g_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_snow3g_pmd_ops;



#endif /* _RTE_SNOW3G_PMD_PRIVATE_H_ */
