/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_NL_H_
#define RTE_PMD_MLX5_NL_H_

#include <linux/netlink.h>

#include <rte_compat.h>
#include <rte_ether.h>

#include "mlx5_common.h"
#include "mlx5_common_utils.h"

typedef void (mlx5_nl_event_cb)(struct nlmsghdr *hdr, void *user_data);

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

#define MLX5_NL_CMD_GET_IB_NAME (1 << 0)
#define MLX5_NL_CMD_GET_IB_INDEX (1 << 1)
#define MLX5_NL_CMD_GET_NET_INDEX (1 << 2)
#define MLX5_NL_CMD_GET_PORT_INDEX (1 << 3)
#define MLX5_NL_CMD_GET_PORT_STATE (1 << 4)
#define MLX5_NL_CMD_GET_EVENT_TYPE (1 << 5)

/** Data structure used by mlx5_nl_cmdget_cb(). */
struct mlx5_nl_port_info {
	const char *name; /**< IB device name (in). */
	uint32_t flags; /**< found attribute flags (out). */
	uint32_t ibindex; /**< IB device index (out). */
	uint32_t ifindex; /**< Network interface index (out). */
	uint32_t portnum; /**< IB device max port number (out). */
	uint16_t state; /**< IB device port state (out). */
	uint8_t event_type; /**< IB RDMA event type (out). */
};

#define MLX5_NL_RDMA_NETDEV_ATTACH_EVENT (1)
#define MLX5_NL_RDMA_NETDEV_DETACH_EVENT (2)

__rte_internal
int mlx5_nl_init(int protocol, int groups);
__rte_internal
int mlx5_nl_mac_addr_add(int nlsk_fd, unsigned int iface_idx,
			 struct rte_ether_addr *mac, uint32_t index);
__rte_internal
int mlx5_nl_mac_addr_remove(int nlsk_fd, unsigned int iface_idx,
			    struct rte_ether_addr *mac, uint32_t index);
__rte_internal
void mlx5_nl_mac_addr_sync(int nlsk_fd, unsigned int iface_idx,
			   struct rte_ether_addr *mac_addrs, int n);
__rte_internal
void mlx5_nl_mac_addr_flush(int nlsk_fd, unsigned int iface_idx,
			    struct rte_ether_addr *mac_addrs, int n,
			    uint64_t *mac_own,
			    bool vf);
__rte_internal
int mlx5_nl_promisc(int nlsk_fd, unsigned int iface_idx, int enable);
__rte_internal
int mlx5_nl_allmulti(int nlsk_fd, unsigned int iface_idx, int enable);
__rte_internal
unsigned int mlx5_nl_portnum(int nl, const char *name, struct mlx5_dev_info *dev_info);
__rte_internal
unsigned int mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex,
			     struct mlx5_dev_info *info);
__rte_internal
int mlx5_nl_port_state(int nl, const char *name, uint32_t pindex, struct mlx5_dev_info *dev_info);
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
int mlx5_nl_enable_roce_get(int nlsk_fd, int family_id, const char *pci_addr,
			    int *enable);
int mlx5_nl_enable_roce_set(int nlsk_fd, int family_id, const char *pci_addr,
			    int enable);

__rte_internal
int mlx5_nl_read_events(int nlsk_fd, mlx5_nl_event_cb *cb, void *cb_arg);
__rte_internal
int mlx5_nl_parse_link_status_update(struct nlmsghdr *hdr, uint32_t *ifindex);

__rte_internal
int mlx5_nl_devlink_esw_multiport_get(int nlsk_fd, int family_id,
				      const char *pci_addr, int *enable);

__rte_internal
int mlx5_nl_rdma_monitor_init(void);
__rte_internal
void mlx5_nl_rdma_monitor_info_get(struct nlmsghdr *hdr, struct mlx5_nl_port_info *data);
__rte_internal
int mlx5_nl_rdma_monitor_cap_get(int nl, uint8_t *cap);

__rte_internal
int mlx5_nl_get_mtu_bounds(int nl, unsigned int ifindex, uint16_t *min_mtu, uint16_t *max_mtu);

#endif /* RTE_PMD_MLX5_NL_H_ */
