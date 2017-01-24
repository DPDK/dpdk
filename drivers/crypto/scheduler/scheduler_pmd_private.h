/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#ifndef _SCHEDULER_PMD_PRIVATE_H
#define _SCHEDULER_PMD_PRIVATE_H

#include <rte_hash.h>
#include <rte_reorder.h>
#include <rte_cryptodev_scheduler.h>

/**< Maximum number of bonded devices per devices */
#ifndef MAX_SLAVES_NUM
#define MAX_SLAVES_NUM				(8)
#endif

#define PER_SLAVE_BUFF_SIZE			(256)

#define CS_LOG_ERR(fmt, args...)					\
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",		\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_CRYPTO_SCHEDULER_DEBUG
#define CS_LOG_INFO(fmt, args...)					\
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",	\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)

#define CS_LOG_DBG(fmt, args...)					\
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",	\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)
#else
#define CS_LOG_INFO(fmt, args...)
#define CS_LOG_DBG(fmt, args...)
#endif

struct scheduler_slave {
	uint8_t dev_id;
	uint16_t qp_id;
	uint32_t nb_inflight_cops;

	enum rte_cryptodev_type dev_type;
};

struct scheduler_ctx {
	void *private_ctx;
	/**< private scheduler context pointer */

	struct rte_cryptodev_capabilities *capabilities;
	uint32_t nb_capabilities;

	uint32_t max_nb_queue_pairs;

	struct scheduler_slave slaves[MAX_SLAVES_NUM];
	uint32_t nb_slaves;

	enum rte_cryptodev_scheduler_mode mode;

	struct rte_cryptodev_scheduler_ops ops;

	uint8_t reordering_enabled;

	char name[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN];
	char description[RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN];
} __rte_cache_aligned;

struct scheduler_qp_ctx {
	void *private_qp_ctx;

	rte_cryptodev_scheduler_burst_enqueue_t schedule_enqueue;
	rte_cryptodev_scheduler_burst_dequeue_t schedule_dequeue;

	struct rte_reorder_buffer *reorder_buf;
	uint32_t seqn;
} __rte_cache_aligned;

struct scheduler_session {
	struct rte_cryptodev_sym_session *sessions[MAX_SLAVES_NUM];
};

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops;

#endif /* _SCHEDULER_PMD_PRIVATE_H */
