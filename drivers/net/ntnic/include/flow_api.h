/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_H_
#define _FLOW_API_H_

#include <rte_spinlock.h>

#include "ntlog.h"

#include "flow_api_engine.h"
#include "hw_mod_backend.h"
#include "stream_binary_flow_api.h"

/*
 * Flow NIC and Eth port device management
 */

struct hw_mod_resource_s {
	uint8_t *alloc_bm;      /* allocation bitmap */
	uint32_t *ref;  /* reference counter for each resource element */
	uint32_t resource_count;/* number of total available entries */
};

/*
 * Device Management API
 */
/**
 * A structure used to configure the Receive Side Scaling (RSS) feature
 * of an Ethernet port.
 */
struct nt_eth_rss_conf {
	/**
	 * In rte_eth_dev_rss_hash_conf_get(), the *rss_key_len* should be
	 * greater than or equal to the *hash_key_size* which get from
	 * rte_eth_dev_info_get() API. And the *rss_key* should contain at least
	 * *hash_key_size* bytes. If not meet these requirements, the query
	 * result is unreliable even if the operation returns success.
	 *
	 * In rte_eth_dev_rss_hash_update() or rte_eth_dev_configure(), if
	 * *rss_key* is not NULL, the *rss_key_len* indicates the length of the
	 * *rss_key* in bytes and it should be equal to *hash_key_size*.
	 * If *rss_key* is NULL, drivers are free to use a random or a default key.
	 */
	uint8_t rss_key[MAX_RSS_KEY_LEN];
	/**
	 * Indicates the type of packets or the specific part of packets to
	 * which RSS hashing is to be applied.
	 */
	uint64_t rss_hf;
	/**
	 * Hash algorithm.
	 */
	enum rte_eth_hash_function algorithm;
};

int nthw_sprint_rss_mask(char *str, uint16_t str_len, const char *prefix, uint64_t hash_mask);

struct flow_eth_dev {
	/* NIC that owns this port device */
	struct flow_nic_dev *ndev;
	/* NIC port id */
	uint8_t port;
	/* App assigned port_id - may be DPDK port_id */
	uint32_t port_id;

	/* 0th for exception */
	struct flow_queue_id_s rx_queue[FLOW_MAX_QUEUES + 1];

	/* VSWITCH has exceptions sent on queue 0 per design */
	int num_queues;

	/* QSL_HSH index if RSS needed QSL v6+ */
	int rss_target_id;

	/* The size of buffer for aged out flow list */
	uint32_t nb_aging_objects;

	struct flow_eth_dev *next;
};

/* registered NIC backends */
struct flow_nic_dev {
	uint8_t adapter_no;     /* physical adapter no in the host system */
	uint16_t ports; /* number of in-ports addressable on this NIC */
	/* flow profile this NIC is initially prepared for */
	enum flow_eth_dev_profile flow_profile;
	int flow_mgnt_prepared;

	struct hw_mod_resource_s res[RES_COUNT];/* raw NIC resource allocation table */
	void *km_res_handle;
	void *kcc_res_handle;

	void *flm_mtr_handle;
	void *group_handle;
	void *hw_db_handle;
	void *id_table_handle;

	uint32_t flow_unique_id_counter;
	/* linked list of all flows created on this NIC */
	struct flow_handle *flow_base;
	/* linked list of all FLM flows created on this NIC */
	struct flow_handle *flow_base_flm;
	rte_spinlock_t flow_mtx;

	/* NIC backend API */
	struct flow_api_backend_s be;
	/* linked list of created eth-port devices on this NIC */
	struct flow_eth_dev *eth_base;
	rte_spinlock_t mtx;

	/* RSS hashing configuration */
	struct nt_eth_rss_conf rss_conf;
	/* next NIC linked list */
	struct flow_nic_dev *next;
};

enum flow_nic_err_msg_e {
	ERR_SUCCESS = 0,
	ERR_FAILED = 1,
	ERR_MEMORY = 2,
	ERR_OUTPUT_TOO_MANY = 3,
	ERR_RSS_TOO_MANY_QUEUES = 4,
	ERR_VLAN_TYPE_NOT_SUPPORTED = 5,
	ERR_VXLAN_HEADER_NOT_ACCEPTED = 6,
	ERR_VXLAN_POP_INVALID_RECIRC_PORT = 7,
	ERR_VXLAN_POP_FAILED_CREATING_VTEP = 8,
	ERR_MATCH_VLAN_TOO_MANY = 9,
	ERR_MATCH_INVALID_IPV6_HDR = 10,
	ERR_MATCH_TOO_MANY_TUNNEL_PORTS = 11,
	ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM = 12,
	ERR_MATCH_FAILED_BY_HW_LIMITS = 13,
	ERR_MATCH_RESOURCE_EXHAUSTION = 14,
	ERR_MATCH_FAILED_TOO_COMPLEX = 15,
	ERR_ACTION_REPLICATION_FAILED = 16,
	ERR_ACTION_OUTPUT_RESOURCE_EXHAUSTION = 17,
	ERR_ACTION_TUNNEL_HEADER_PUSH_OUTPUT_LIMIT = 18,
	ERR_ACTION_INLINE_MOD_RESOURCE_EXHAUSTION = 19,
	ERR_ACTION_RETRANSMIT_RESOURCE_EXHAUSTION = 20,
	ERR_ACTION_FLOW_COUNTER_EXHAUSTION = 21,
	ERR_ACTION_INTERNAL_RESOURCE_EXHAUSTION = 22,
	ERR_INTERNAL_QSL_COMPARE_FAILED = 23,
	ERR_INTERNAL_CAT_FUNC_REUSE_FAILED = 24,
	ERR_MATCH_ENTROPHY_FAILED = 25,
	ERR_MATCH_CAM_EXHAUSTED = 26,
	ERR_INTERNAL_VIRTUAL_PORT_CREATION_FAILED = 27,
	ERR_ACTION_UNSUPPORTED = 28,
	ERR_REMOVE_FLOW_FAILED = 29,
	ERR_ACTION_NO_OUTPUT_DEFINED_USE_DEFAULT = 30,
	ERR_ACTION_NO_OUTPUT_QUEUE_FOUND = 31,
	ERR_MATCH_UNSUPPORTED_ETHER_TYPE = 32,
	ERR_OUTPUT_INVALID = 33,
	ERR_MATCH_PARTIAL_OFFLOAD_NOT_SUPPORTED = 34,
	ERR_MATCH_CAT_CAM_EXHAUSTED = 35,
	ERR_MATCH_KCC_KEY_CLASH = 36,
	ERR_MATCH_CAT_CAM_FAILED = 37,
	ERR_PARTIAL_FLOW_MARK_TOO_BIG = 38,
	ERR_FLOW_PRIORITY_VALUE_INVALID = 39,
	ERR_ACTION_MULTIPLE_PORT_ID_UNSUPPORTED = 40,
	ERR_RSS_TOO_LONG_KEY = 41,
	ERR_ACTION_AGE_UNSUPPORTED_GROUP_0 = 42,
	ERR_MSG_NO_MSG = 43,
	ERR_MSG_END
};

void nthw_flow_nic_set_error(enum flow_nic_err_msg_e msg, struct rte_flow_error *error);

/*
 * Resources
 */

extern const char *nthw_dbg_res_descr[];

#define flow_nic_set_bit(arr, x)                                                                  \
	do {                                                                                      \
		uint8_t *_temp_arr = (arr);                                                       \
		size_t _temp_x = (x);                                                             \
		_temp_arr[_temp_x / 8] =                                                          \
			(uint8_t)(_temp_arr[_temp_x / 8] | (uint8_t)(1 << (_temp_x % 8)));        \
	} while (0)

#define flow_nic_unset_bit(arr, x)                                                                \
	do {                                                                                      \
		size_t _temp_x = (x);                                                             \
		arr[_temp_x / 8] &= (uint8_t)(~(1 << (_temp_x % 8)));                             \
	} while (0)

#define flow_nic_is_bit_set(arr, x)                                                               \
	({                                                                                        \
		size_t _temp_x = (x);                                                             \
		(arr[_temp_x / 8] & (uint8_t)(1 << (_temp_x % 8)));                               \
	})

#define flow_nic_mark_resource_used(_ndev, res_type, index)                                       \
	do {                                                                                      \
		struct flow_nic_dev *_temp_ndev = (_ndev);                                        \
		typeof(res_type) _temp_res_type = (res_type);                                     \
		size_t _temp_index = (index);                                                     \
		NT_LOG(DBG, FILTER, "mark resource used: %s idx %zu",                             \
		       nthw_dbg_res_descr[_temp_res_type], _temp_index);                          \
		RTE_ASSERT(flow_nic_is_bit_set(_temp_ndev->res[_temp_res_type].alloc_bm,          \
					   _temp_index) == 0);                                    \
		flow_nic_set_bit(_temp_ndev->res[_temp_res_type].alloc_bm, _temp_index);          \
	} while (0)

#define flow_nic_mark_resource_unused(_ndev, res_type, index)                                     \
	do {                                                                                      \
		typeof(res_type) _temp_res_type = (res_type);                                 \
		size_t _temp_index = (index);                                                     \
		NT_LOG(DBG, FILTER, "mark resource unused: %s idx %zu",                         \
		       nthw_dbg_res_descr[_temp_res_type], _temp_index);                          \
		flow_nic_unset_bit((_ndev)->res[_temp_res_type].alloc_bm, _temp_index);           \
	} while (0)

#define flow_nic_is_resource_used(_ndev, res_type, index)                                         \
	(!!flow_nic_is_bit_set((_ndev)->res[res_type].alloc_bm, index))

int nthw_flow_nic_alloc_resource(struct flow_nic_dev *ndev, enum res_type_e res_type,
	uint32_t alignment);

int nthw_flow_nic_alloc_resource_config(struct flow_nic_dev *ndev, enum res_type_e res_type,
	unsigned int num, uint32_t alignment);
void nthw_flow_nic_free_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int idx);

int nthw_flow_nic_ref_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int index);
int nthw_flow_nic_deref_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int index);

int nthw_flow_get_flm_stats(struct flow_nic_dev *ndev, uint64_t *data, uint64_t size);
int nthw_flow_get_ifr_stats(struct flow_nic_dev *ndev, uint64_t *data, uint8_t port_count);

#endif
