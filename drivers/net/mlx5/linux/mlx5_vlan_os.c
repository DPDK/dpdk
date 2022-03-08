/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

/*
 * Not needed by this file; included to work around the lack of off_t
 * definition for mlx5dv.h with unpatched rdma-core versions.
 */
#include <sys/types.h>

#include <ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_hypervisor.h>

#include <mlx5.h>
#include <mlx5_nl.h>
#include <mlx5_malloc.h>

/*
 * Release VLAN network device, created for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to release.
 */
void
mlx5_vlan_vmwa_release(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_nl_vlan_vmwa_context *vmwa = priv->vmwa_context;
	struct mlx5_nl_vlan_dev *vlan_dev = &vmwa->vlan_dev[0];

	MLX5_ASSERT(vlan->created);
	MLX5_ASSERT(priv->vmwa_context);
	if (!vlan->created || !vmwa)
		return;
	vlan->created = 0;
	rte_spinlock_lock(&vmwa->sl);
	MLX5_ASSERT(vlan_dev[vlan->tag].refcnt);
	if (--vlan_dev[vlan->tag].refcnt == 0 &&
	    vlan_dev[vlan->tag].ifindex) {
		mlx5_nl_vlan_vmwa_delete(vmwa, vlan_dev[vlan->tag].ifindex);
		vlan_dev[vlan->tag].ifindex = 0;
	}
	rte_spinlock_unlock(&vmwa->sl);
}

/**
 * Acquire VLAN interface with specified tag for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to acquire.
 */
void
mlx5_vlan_vmwa_acquire(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_nl_vlan_vmwa_context *vmwa = priv->vmwa_context;
	struct mlx5_nl_vlan_dev *vlan_dev = &vmwa->vlan_dev[0];

	MLX5_ASSERT(!vlan->created);
	MLX5_ASSERT(priv->vmwa_context);
	if (vlan->created || !vmwa)
		return;
	rte_spinlock_lock(&vmwa->sl);
	if (vlan_dev[vlan->tag].refcnt == 0) {
		MLX5_ASSERT(!vlan_dev[vlan->tag].ifindex);
		vlan_dev[vlan->tag].ifindex =
			mlx5_nl_vlan_vmwa_create(vmwa, vmwa->vf_ifindex,
						 vlan->tag);
	}
	if (vlan_dev[vlan->tag].ifindex) {
		vlan_dev[vlan->tag].refcnt++;
		vlan->created = 1;
	}
	rte_spinlock_unlock(&vmwa->sl);
}

/*
 * Create per ethernet device VLAN VM workaround context
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param ifindex
 *   Interface index.
 *
 * @Return
 *   Pointer to mlx5_nl_vlan_vmwa_context
 */
void *
mlx5_vlan_vmwa_init(struct rte_eth_dev *dev, uint32_t ifindex)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	struct mlx5_nl_vlan_vmwa_context *vmwa;
	enum rte_hypervisor hv_type;

	/* Do not engage workaround over PF. */
	if (!config->vf)
		return NULL;
	/* Check whether there is desired virtual environment */
	hv_type = rte_hypervisor_get();
	switch (hv_type) {
	case RTE_HYPERVISOR_UNKNOWN:
	case RTE_HYPERVISOR_VMWARE:
		/*
		 * The "white list" of configurations
		 * to engage the workaround.
		 */
		break;
	default:
		/*
		 * The configuration is not found in the "white list".
		 * We should not engage the VLAN workaround.
		 */
		return NULL;
	}
	vmwa = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*vmwa), sizeof(uint32_t),
			   SOCKET_ID_ANY);
	if (!vmwa) {
		DRV_LOG(WARNING,
			"Can not allocate memory"
			" for VLAN workaround context");
		return NULL;
	}
	rte_spinlock_init(&vmwa->sl);
	vmwa->nl_socket = mlx5_nl_init(NETLINK_ROUTE, 0);
	if (vmwa->nl_socket < 0) {
		DRV_LOG(WARNING,
			"Can not create Netlink socket"
			" for VLAN workaround context");
		mlx5_free(vmwa);
		return NULL;
	}
	vmwa->vf_ifindex = ifindex;
	/* Cleanup for existing VLAN devices. */
	return vmwa;
}

/*
 * Destroy per ethernet device VLAN VM workaround context
 *
 * @param dev
 *   Pointer to VM context
 */
void
mlx5_vlan_vmwa_exit(void *vmctx)
{
	unsigned int i;

	struct mlx5_nl_vlan_vmwa_context *vmwa = vmctx;
	/* Delete all remaining VLAN devices. */
	for (i = 0; i < RTE_DIM(vmwa->vlan_dev); i++) {
		if (vmwa->vlan_dev[i].ifindex)
			mlx5_nl_vlan_vmwa_delete(vmwa,
						 vmwa->vlan_dev[i].ifindex);
	}
	if (vmwa->nl_socket >= 0)
		close(vmwa->nl_socket);
	mlx5_free(vmwa);
}
