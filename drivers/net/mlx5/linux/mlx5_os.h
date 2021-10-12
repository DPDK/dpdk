/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_OS_H_
#define RTE_PMD_MLX5_OS_H_

#include <net/if.h>

/* verb enumerations translations to local enums. */
enum {
	MLX5_FS_NAME_MAX = IBV_SYSFS_NAME_MAX + 1,
	MLX5_FS_PATH_MAX = IBV_SYSFS_PATH_MAX + 1
};

/* Maximal data of sendmsg message(in bytes). */
#define MLX5_SENDMSG_MAX 64

#define MLX5_NAMESIZE IF_NAMESIZE

int mlx5_auxiliary_get_ifindex(const char *sf_name);


enum mlx5_sw_parsing_offloads {
#ifdef HAVE_IBV_MLX5_MOD_SWP
	MLX5_SW_PARSING_CAP      = MLX5DV_SW_PARSING,
	MLX5_SW_PARSING_CSUM_CAP = MLX5DV_SW_PARSING_CSUM,
	MLX5_SW_PARSING_TSO_CAP  = MLX5DV_SW_PARSING_LSO,
#else
	MLX5_SW_PARSING_CAP      = 0,
	MLX5_SW_PARSING_CSUM_CAP = 0,
	MLX5_SW_PARSING_TSO_CAP  = 0,
#endif
};

enum mlx5_tunnel_offloads {
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	MLX5_TUNNELED_OFFLOADS_VXLAN_CAP  =
		MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_VXLAN,
	MLX5_TUNNELED_OFFLOADS_GRE_CAP    =
		MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GRE,
	MLX5_TUNNELED_OFFLOADS_GENEVE_CAP =
		MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GENEVE,
#else
	MLX5_TUNNELED_OFFLOADS_VXLAN_CAP  = 0,
	MLX5_TUNNELED_OFFLOADS_GRE_CAP	  = 0,
	MLX5_TUNNELED_OFFLOADS_GENEVE_CAP = 0,
#endif
};
#endif /* RTE_PMD_MLX5_OS_H_ */
