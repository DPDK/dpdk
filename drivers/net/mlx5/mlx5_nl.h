/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_NL_H_
#define RTE_PMD_MLX5_NL_H_

#include <linux/netlink.h>


/* Recognized Infiniband device physical port name types. */
enum mlx5_nl_phys_port_name_type {
	MLX5_PHYS_PORT_NAME_TYPE_NOTSET = 0, /* Not set. */
	MLX5_PHYS_PORT_NAME_TYPE_LEGACY, /* before kernel ver < 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UPLINK, /* p0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_PFVF, /* pf0vf0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN, /* Unrecognized. */
};

/** Switch information returned by mlx5_nl_switch_info(). */
struct mlx5_switch_info {
	uint32_t master:1; /**< Master device. */
	uint32_t representor:1; /**< Representor device. */
	enum mlx5_nl_phys_port_name_type name_type; /** < Port name type. */
	int32_t pf_num; /**< PF number (valid for pfxvfx format only). */
	int32_t port_name; /**< Representor port name. */
	uint64_t switch_id; /**< Switch identifier. */
};

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
	uint32_t nl_sn;
	uint32_t vf_ifindex;
	struct mlx5_nl_vlan_dev vlan_dev[4096];
};


int mlx5_nl_init(int protocol);
int mlx5_nl_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			 uint32_t index);
int mlx5_nl_mac_addr_remove(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			    uint32_t index);
void mlx5_nl_mac_addr_sync(struct rte_eth_dev *dev);
void mlx5_nl_mac_addr_flush(struct rte_eth_dev *dev);
int mlx5_nl_promisc(struct rte_eth_dev *dev, int enable);
int mlx5_nl_allmulti(struct rte_eth_dev *dev, int enable);
unsigned int mlx5_nl_portnum(int nl, const char *name);
unsigned int mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex);
int mlx5_nl_vf_mac_addr_modify(struct rte_eth_dev *dev,
			       struct rte_ether_addr *mac, int vf_index);
int mlx5_nl_switch_info(int nl, unsigned int ifindex,
			struct mlx5_switch_info *info);

void mlx5_nl_vlan_vmwa_delete(struct mlx5_nl_vlan_vmwa_context *vmwa,
			   uint32_t ifindex);
uint32_t mlx5_nl_vlan_vmwa_create(struct mlx5_nl_vlan_vmwa_context *vmwa,
				  uint32_t ifindex, uint16_t tag);

#endif /* RTE_PMD_MLX5_NL_H_ */
