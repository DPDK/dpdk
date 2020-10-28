/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_NL_H_
#define RTE_PMD_MLX5_NL_H_

#include <linux/netlink.h>

#include <rte_ether.h>

#include "mlx5_common.h"


/* VLAN netdev for VLAN workaround. */
struct mlx5_nl_vlan_dev {
	uint32_t refcnt;
	uint32_t ifindex; /**< Own interface index. */
};

/*
 * Array of VLAN devices created on the base of VF
 * used for workaround in virtual environments.
 */
struct mlx5_nl_vlan_vmwa_context {
	int nl_socket;
	uint32_t vf_ifindex;
	rte_spinlock_t sl;
	struct mlx5_nl_vlan_dev vlan_dev[4096];
};

__rte_internal
int mlx5_nl_init(int protocol);
__rte_internal
int mlx5_nl_mac_addr_add(int nlsk_fd, unsigned int iface_idx, uint64_t *mac_own,
			 struct rte_ether_addr *mac, uint32_t index);
__rte_internal
int mlx5_nl_mac_addr_remove(int nlsk_fd, unsigned int iface_idx,
			    uint64_t *mac_own, struct rte_ether_addr *mac,
			    uint32_t index);
__rte_internal
void mlx5_nl_mac_addr_sync(int nlsk_fd, unsigned int iface_idx,
			   struct rte_ether_addr *mac_addrs, int n);
__rte_internal
void mlx5_nl_mac_addr_flush(int nlsk_fd, unsigned int iface_idx,
			    struct rte_ether_addr *mac_addrs, int n,
			    uint64_t *mac_own);
__rte_internal
int mlx5_nl_promisc(int nlsk_fd, unsigned int iface_idx, int enable);
__rte_internal
int mlx5_nl_allmulti(int nlsk_fd, unsigned int iface_idx, int enable);
__rte_internal
unsigned int mlx5_nl_portnum(int nl, const char *name);
__rte_internal
unsigned int mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex);
__rte_internal
int mlx5_nl_vf_mac_addr_modify(int nlsk_fd, unsigned int iface_idx,
			       struct rte_ether_addr *mac, int vf_index);
__rte_internal
int mlx5_nl_switch_info(int nl, unsigned int ifindex,
			struct mlx5_switch_info *info);

__rte_internal
void mlx5_nl_vlan_vmwa_delete(struct mlx5_nl_vlan_vmwa_context *vmwa,
			      uint32_t ifindex);
__rte_internal
uint32_t mlx5_nl_vlan_vmwa_create(struct mlx5_nl_vlan_vmwa_context *vmwa,
				  uint32_t ifindex, uint16_t tag);
__rte_internal
int mlx5_nl_devlink_family_id_get(int nlsk_fd);
__rte_internal
int mlx5_nl_enable_roce_get(int nlsk_fd, int family_id, const char *pci_addr,
			    int *enable);
__rte_internal
int mlx5_nl_driver_reload(int nlsk_fd, int family_id, const char *pci_addr);
__rte_internal
int mlx5_nl_enable_roce_set(int nlsk_fd, int family_id, const char *pci_addr,
			    int enable);

#endif /* RTE_PMD_MLX5_NL_H_ */
