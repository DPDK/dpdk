/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_H_
#define _FLOW_API_H_

#include <pthread.h>

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
int flow_delete_eth_dev(struct flow_eth_dev *eth_dev);

struct flow_eth_dev {
	/* NIC that owns this port device */
	struct flow_nic_dev *ndev;
	/* NIC port id */
	uint8_t port;

	/* 0th for exception */
	struct flow_queue_id_s rx_queue[FLOW_MAX_QUEUES + 1];

	/* VSWITCH has exceptions sent on queue 0 per design */
	int num_queues;

	struct flow_eth_dev *next;
};

/* registered NIC backends */
struct flow_nic_dev {
	uint8_t adapter_no;     /* physical adapter no in the host system */
	uint16_t ports; /* number of in-ports addressable on this NIC */

	struct hw_mod_resource_s res[RES_COUNT];/* raw NIC resource allocation table */
	void *km_res_handle;
	void *kcc_res_handle;

	uint32_t flow_unique_id_counter;
	/* linked list of all flows created on this NIC */
	struct flow_handle *flow_base;

	/* NIC backend API */
	struct flow_api_backend_s be;
	/* linked list of created eth-port devices on this NIC */
	struct flow_eth_dev *eth_base;
	pthread_mutex_t mtx;

	/* next NIC linked list */
	struct flow_nic_dev *next;
};

/*
 * Resources
 */

extern const char *dbg_res_descr[];

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

#define flow_nic_mark_resource_unused(_ndev, res_type, index)                                     \
	do {                                                                                      \
		typeof(res_type) _temp_res_type = (res_type);                                 \
		size_t _temp_index = (index);                                                     \
		NT_LOG(DBG, FILTER, "mark resource unused: %s idx %zu",                         \
		       dbg_res_descr[_temp_res_type], _temp_index);                               \
		flow_nic_unset_bit((_ndev)->res[_temp_res_type].alloc_bm, _temp_index);           \
	} while (0)

#define flow_nic_is_resource_used(_ndev, res_type, index)                                         \
	(!!flow_nic_is_bit_set((_ndev)->res[res_type].alloc_bm, index))

void flow_nic_free_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int idx);

int flow_nic_deref_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int index);

#endif
