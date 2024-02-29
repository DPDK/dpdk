/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021, NVIDIA CORPORATION & AFFILIATES.
 */

#ifndef _RTE_VF_RPC_H_
#define _RTE_VF_RPC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Device specific rpc lib
 */

#include <stdint.h>
#include <rte_compat.h>
#include <rte_dev.h>
#include <rte_ether.h>

struct vdpa_vf_params {
	char vf_name[RTE_DEV_NAME_MAX_LEN];
	uint32_t prov_flags;
	uint32_t msix_num;
	uint32_t queue_num;
	uint32_t queue_size;
	uint64_t features;
	uint32_t mtu;
	struct rte_ether_addr mac;
	char vm_uuid[RTE_UUID_STRLEN];
	bool configured;
};

enum vdpa_vf_prov_flags {
	VDPA_VF_MSIX_NUM,
	VDPA_VF_QUEUE_NUM,
	VDPA_VF_QUEUE_SIZE,
	VDPA_VF_FEATURES,
	VDPA_VF_MTU,
	VDPA_VF_MAC,
};

struct vdpa_debug_vf_params {
	uint32_t test_type;
	union {
		uint32_t test_mode;
		uint32_t lm_status;
	};
};
enum vdpa_vf_debug_test_type {
	VDPA_DEBUG_CMD_INVALID,
	VDPA_DEBUG_CMD_RUNNING = 1,
	VDPA_DEBUG_CMD_QUIESCED,
	VDPA_DEBUG_CMD_FREEZED,
	VDPA_DEBUG_CMD_START_LOGGING,
	VDPA_DEBUG_CMD_STOP_LOGGING,
	VDPA_DEBUG_CMD_GET_STATUS,
	VDPA_DEBUG_CMD_MAX_INVALID,
};

int
rte_vdpa_vf_dev_add(char *vf_name, const char *vm_uuid, struct vdpa_vf_params *vf_params);

int
rte_vdpa_vf_dev_remove(const char *vf_name);

int
rte_vdpa_get_vf_list(const char *pf_name, struct vdpa_vf_params *vf_params,
		int max_vf_num);

int
rte_vdpa_get_vf_info(const char *vf_name, struct vdpa_vf_params *vf_params);

int
rte_vdpa_vf_dev_debug(const char *pf_name,
		struct vdpa_debug_vf_params *vf_debug_params);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VF_RPC_H_ */
