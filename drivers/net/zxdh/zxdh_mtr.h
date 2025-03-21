/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_MTR_H
#define ZXDH_MTR_H

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>
#include <rte_mtr.h>

#include "zxdh_np.h"

#define HW_PROFILE_MAX            512
#define ZXDH_MAX_MTR_NUM          2048
#define ZXDH_MAX_POLICY_NUM  ZXDH_MAX_MTR_NUM
#define MAX_MTR_PROFILE_NUM  HW_PROFILE_MAX
#define ZXDH_INGRESS              1
#define ZXDH_EGRESS              2

#define MP_FREE_OBJ_FUNC(mp, obj) rte_mempool_put(mp, obj)

struct zxdh_mtr_res {
	rte_spinlock_t hw_plcr_res_lock;
	uint32_t hw_profile_refcnt[HW_PROFILE_MAX];
	struct rte_mtr_meter_profile profile[HW_PROFILE_MAX];
};

extern struct zxdh_mtr_res g_mtr_res;
extern struct zxdh_shared_data *zxdh_shared_data;

enum rte_mtr_policer_action {
	ZXDH_MTR_POLICER_ACTION_COLOR_GREEN = 0,
	ZXDH_MTR_POLICER_ACTION_COLOR_YELLOW,
	ZXDH_MTR_POLICER_ACTION_COLOR_RED,
	ZXDH_MTR_POLICER_ACTION_DROP,
};

union zxdh_offload_profile_cfg {
	ZXDH_STAT_CAR_PKT_PROFILE_CFG_T p_car_pkt_profile_cfg;
	ZXDH_STAT_CAR_PROFILE_CFG_T     p_car_byte_profile_cfg;
};

/* meter profile structure. */
struct zxdh_meter_profile {
	TAILQ_ENTRY(zxdh_meter_profile) next; /* Pointer to the next flow meter structure. */
	uint16_t dpdk_port_id;
	uint16_t hw_profile_owner_vport;
	uint16_t meter_profile_id;            /* software Profile id. */
	uint16_t hw_profile_id;               /* hardware Profile id. */
	struct rte_mtr_meter_profile profile; /* Profile detail. */
	union zxdh_offload_profile_cfg plcr_param;
	uint32_t ref_cnt;                     /* used count. */
};
TAILQ_HEAD(zxdh_mtr_profile_list, zxdh_meter_profile);

struct zxdh_meter_policy {
	TAILQ_ENTRY(zxdh_meter_policy) next;
	uint16_t policy_id;
	uint16_t ref_cnt;
	uint16_t dpdk_port_id;
	uint16_t rsv;
	struct rte_mtr_meter_policy_params policy;
};
TAILQ_HEAD(zxdh_mtr_policy_list, zxdh_meter_policy);

struct zxdh_meter_action {
	enum rte_mtr_policer_action action[RTE_COLORS];
	uint64_t stats_mask;
};

struct zxdh_mtr_object {
	TAILQ_ENTRY(zxdh_mtr_object) next;
	uint8_t direction:1, /* 0:ingress, 1:egress */
			shared:1,
			enable:1,
			rsv:5;
	uint8_t rsv8;
	uint16_t port_id;
	uint16_t vfid;
	uint16_t meter_id;
	uint16_t mtr_ref_cnt;
	uint16_t rsv16;
	struct zxdh_meter_profile *profile;
	struct zxdh_meter_policy *policy;
	struct zxdh_meter_action  mtr_action;
};
TAILQ_HEAD(zxdh_mtr_list, zxdh_mtr_object);

struct zxdh_mtr_stats {
	uint64_t n_pkts_dropped;
	uint64_t n_bytes_dropped;
};

struct zxdh_ifc_mtr_stats_bits {
	uint8_t n_pkts_dropped[0x40];
	uint8_t n_bytes_dropped[0x40];
};

struct zxdh_hw_mtr_stats {
	uint32_t n_pkts_dropped_hi;
	uint32_t n_pkts_dropped_lo;
	uint32_t n_bytes_dropped_hi;
	uint32_t n_bytes_dropped_lo;
};

int zxdh_meter_ops_get(struct rte_eth_dev *dev, void *arg);
void zxdh_mtr_release(struct rte_eth_dev *dev);
void zxdh_mtr_policy_res_free(struct rte_mempool *mtr_policy_mp, struct zxdh_meter_policy *policy);
int zxdh_hw_profile_unref(struct rte_eth_dev *dev,
		uint8_t car_type,
		uint16_t hw_profile_id,
		struct rte_mtr_error *error);
int zxdh_hw_profile_alloc_direct(struct rte_eth_dev *dev, ZXDH_PROFILE_TYPE car_type,
		uint64_t *hw_profile_id, struct rte_mtr_error *error);
int zxdh_hw_profile_ref(uint16_t hw_profile_id);

#endif /* ZXDH_MTR_H */
