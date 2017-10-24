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

#ifndef _RTE_FLOW_CLASSIFY_H_
#define _RTE_FLOW_CLASSIFY_H_

/**
 * @file
 *
 * RTE Flow Classify Library
 *
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This library provides flow record information with some measured properties.
 *
 * Application should define the flow and measurement criteria (action) for it.
 *
 * The Library doesn't maintain any flow records itself, instead flow
 * information is returned to upper layer only for given packets.
 *
 * It is application's responsibility to call rte_flow_classifier_query()
 * for a burst of packets, just after receiving them or before transmitting
 * them.
 * Application should provide the flow type interested in, measurement to apply
 * to that flow in rte_flow_classify_table_entry_add() API, and should provide
 * the rte_flow_classifier object and storage to put results in for the
 * rte_flow_classifier_query() API.
 *
 *  Usage:
 *  - application calls rte_flow_classifier_create() to create an
 *    rte_flow_classifier object.
 *  - application calls rte_flow_classify_table_create() to create a table
 *    in the rte_flow_classifier object.
 *  - application calls rte_flow_classify_table_entry_add() to add a rule to
 *    the table in the rte_flow_classifier object.
 *  - application calls rte_flow_classifier_query() in a polling manner,
 *    preferably after rte_eth_rx_burst(). This will cause the library to
 *    match packet information to flow information with some measurements.
 *  - rte_flow_classifier object can be destroyed when it is no longer needed
 *    with rte_flow_classifier_free()
 */

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_acl.h>
#include <rte_table_acl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int librte_flow_classify_logtype;

#define RTE_FLOW_CLASSIFY_LOG(level, fmt, args...) \
rte_log(RTE_LOG_ ## level, librte_flow_classify_logtype, "%s(): " fmt, \
	__func__, ## args)

/** Opaque data type for flow classifier */
struct rte_flow_classifier;

/** Opaque data type for flow classify rule */
struct rte_flow_classify_rule;

/** Flow classify rule type */
enum rte_flow_classify_rule_type {
	/** no type */
	RTE_FLOW_CLASSIFY_RULE_TYPE_NONE,
	/** IPv4 5tuple type */
	RTE_FLOW_CLASSIFY_RULE_TYPE_IPV4_5TUPLE,
};

/** Flow classify table type */
enum rte_flow_classify_table_type {
	/** no type */
	RTE_FLOW_CLASSIFY_TABLE_TYPE_NONE,
	/** ACL type */
	RTE_FLOW_CLASSIFY_TABLE_TYPE_ACL,
};

/**
 * Maximum number of tables allowed for any Flow Classifier instance.
 * The value of this parameter cannot be changed.
 */
#define RTE_FLOW_CLASSIFY_TABLE_MAX  64

/** Parameters for flow classifier creation */
struct rte_flow_classifier_params {
	/** flow classifier name */
	const char *name;

	/** CPU socket ID where memory for the flow classifier and its */
	/** elements (tables) should be allocated */
	int socket_id;

	/** Table type */
	enum rte_flow_classify_table_type type;
};

/** Parameters for table creation */
struct rte_flow_classify_table_params {
	/** Table operations (specific to each table type) */
	struct rte_table_ops *ops;

	/** Opaque param to be passed to the table create operation */
	void *arg_create;
};

/** IPv4 5-tuple data */
struct rte_flow_classify_ipv4_5tuple {
	uint32_t dst_ip;         /**< Destination IP address in big endian. */
	uint32_t dst_ip_mask;    /**< Mask of destination IP address. */
	uint32_t src_ip;         /**< Source IP address in big endian. */
	uint32_t src_ip_mask;    /**< Mask of destination IP address. */
	uint16_t dst_port;       /**< Destination port in big endian. */
	uint16_t dst_port_mask;  /**< Mask of destination port. */
	uint16_t src_port;       /**< Source Port in big endian. */
	uint16_t src_port_mask;  /**< Mask of source port. */
	uint8_t proto;           /**< L4 protocol. */
	uint8_t proto_mask;      /**< Mask of L4 protocol. */
};

/**
 * Flow stats
 *
 * For the count action, stats can be returned by the query API.
 *
 * Storage for stats is provided by application.
 */
struct rte_flow_classify_stats {
	void *stats;
};

struct rte_flow_classify_ipv4_5tuple_stats {
	/** count of packets that match IPv4 5tuple pattern */
	uint64_t counter1;
	/** IPv4 5tuple data */
	struct rte_flow_classify_ipv4_5tuple ipv4_5tuple;
};

/**
 * Flow classifier create
 *
 * @param params
 *   Parameters for flow classifier creation
 * @return
 *   Handle to flow classifier instance on success or NULL otherwise
 */
struct rte_flow_classifier *
rte_flow_classifier_create(struct rte_flow_classifier_params *params);

/**
 * Flow classifier free
 *
 * @param cls
 *   Handle to flow classifier instance
 * @return
 *   0 on success, error code otherwise
 */
int
rte_flow_classifier_free(struct rte_flow_classifier *cls);

/**
 * Flow classify table create
 *
 * @param cls
 *   Handle to flow classifier instance
 * @param params
 *   Parameters for flow_classify table creation
 * @param table_id
 *   Table ID. Valid only within the scope of table IDs of the current
 *   classifier. Only returned after a successful invocation.
 * @return
 *   0 on success, error code otherwise
 */
int
rte_flow_classify_table_create(struct rte_flow_classifier *cls,
		struct rte_flow_classify_table_params *params,
		uint32_t *table_id);

/**
 * Add a flow classify rule to the flow_classifer table.
 *
 * @param[in] cls
 *   Flow classifier handle
 * @param[in] table_id
 *   id of table
 * @param[out] key_found
 *  returns 1 if key present already, 0 otherwise.
 * @param[in] attr
 *   Flow rule attributes
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END pattern item).
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Structure
 *   initialised in case of error only.
 * @return
 *   A valid handle in case of success, NULL otherwise.
 */
struct rte_flow_classify_rule *
rte_flow_classify_table_entry_add(struct rte_flow_classifier *cls,
		uint32_t table_id,
		int *key_found,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

/**
 * Delete a flow classify rule from the flow_classifer table.
 *
 * @param[in] cls
 *   Flow classifier handle
 * @param[in] table_id
 *   id of table
 * @param[in] rule
 *   Flow classify rule
 * @return
 *   0 on success, error code otherwise.
 */
int
rte_flow_classify_table_entry_delete(struct rte_flow_classifier *cls,
		uint32_t table_id,
		struct rte_flow_classify_rule *rule);

/**
 * Query flow classifier for given rule.
 *
 * @param[in] cls
 *   Flow classifier handle
 * @param[in] table_id
 *   id of table
 * @param[in] pkts
 *   Pointer to packets to process
 * @param[in] nb_pkts
 *   Number of packets to process
 * @param[in] rule
 *   Flow classify rule
 * @param[in] stats
 *   Flow classify stats
 *
 * @return
 *   0 on success, error code otherwise.
 */
int
rte_flow_classifier_query(struct rte_flow_classifier *cls,
		uint32_t table_id,
		struct rte_mbuf **pkts,
		const uint16_t nb_pkts,
		struct rte_flow_classify_rule *rule,
		struct rte_flow_classify_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_FLOW_CLASSIFY_H_ */
