/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_hypervisor.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_nl.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/**
 * DPDK callback to configure a VLAN filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   VLAN ID to filter.
 * @param on
 *   Toggle filter.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	DRV_LOG(DEBUG, "port %u %s VLAN filter ID %" PRIu16,
		dev->data->port_id, (on ? "enable" : "disable"), vlan_id);
	MLX5_ASSERT(priv->vlan_filter_n <= RTE_DIM(priv->vlan_filter));
	for (i = 0; (i != priv->vlan_filter_n); ++i)
		if (priv->vlan_filter[i] == vlan_id)
			break;
	/* Check if there's room for another VLAN filter. */
	if (i == RTE_DIM(priv->vlan_filter)) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (i < priv->vlan_filter_n) {
		MLX5_ASSERT(priv->vlan_filter_n != 0);
		/* Enabling an existing VLAN filter has no effect. */
		if (on)
			goto out;
		/* Remove VLAN filter from list. */
		--priv->vlan_filter_n;
		memmove(&priv->vlan_filter[i],
			&priv->vlan_filter[i + 1],
			sizeof(priv->vlan_filter[i]) *
			(priv->vlan_filter_n - i));
		priv->vlan_filter[priv->vlan_filter_n] = 0;
	} else {
		MLX5_ASSERT(i == priv->vlan_filter_n);
		/* Disabling an unknown VLAN filter has no effect. */
		if (!on)
			goto out;
		/* Add new VLAN filter. */
		priv->vlan_filter[priv->vlan_filter_n] = vlan_id;
		++priv->vlan_filter_n;
	}
out:
	if (dev->data->dev_started)
		return mlx5_traffic_restart(dev);
	return 0;
}

/**
 * Callback to set/reset VLAN stripping for a specific queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param queue
 *   RX queue index.
 * @param on
 *   Enable/disable VLAN stripping.
 */
void
mlx5_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[queue];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	uint16_t vlan_offloads =
		(on ? IBV_WQ_FLAGS_CVLAN_STRIPPING : 0) |
		0;
	int ret = 0;

	/* Validate hw support */
	if (!priv->config.hw_vlan_strip) {
		DRV_LOG(ERR, "port %u VLAN stripping is not supported",
			dev->data->port_id);
		return;
	}
	/* Validate queue number */
	if (queue >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u VLAN stripping, invalid queue number %d",
			dev->data->port_id, queue);
		return;
	}
	DRV_LOG(DEBUG, "port %u set VLAN offloads 0x%x for port %uqueue %d",
		dev->data->port_id, vlan_offloads, rxq->port_id, queue);
	if (!rxq_ctrl->obj) {
		/* Update related bits in RX queue. */
		rxq->vlan_strip = !!on;
		return;
	}
	if (rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_IBV) {
		mod = (struct ibv_wq_attr){
			.attr_mask = IBV_WQ_ATTR_FLAGS,
			.flags_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING,
			.flags = vlan_offloads,
		};
		ret = mlx5_glue->modify_wq(rxq_ctrl->obj->wq, &mod);
	} else if (rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
		struct mlx5_devx_modify_rq_attr rq_attr;

		memset(&rq_attr, 0, sizeof(rq_attr));
		rq_attr.rq_state = MLX5_RQC_STATE_RDY;
		rq_attr.state = MLX5_RQC_STATE_RDY;
		rq_attr.vsd = (on ? 0 : 1);
		rq_attr.modify_bitmask = MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
	}
	if (ret) {
		DRV_LOG(ERR, "port %u failed to modify object %d stripping "
			"mode: %s", dev->data->port_id,
			rxq_ctrl->obj->type, strerror(rte_errno));
		return;
	}
	/* Update related bits in RX queue. */
	rxq->vlan_strip = !!on;
}

/**
 * Callback to set/reset VLAN offloads for a port.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mask
 *   VLAN offload bit mask.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	if (mask & ETH_VLAN_STRIP_MASK) {
		int hw_vlan_strip = !!(dev->data->dev_conf.rxmode.offloads &
				       DEV_RX_OFFLOAD_VLAN_STRIP);

		if (!priv->config.hw_vlan_strip) {
			DRV_LOG(ERR, "port %u VLAN stripping is not supported",
				dev->data->port_id);
			return 0;
		}
		/* Run on every RX queue and set/reset VLAN stripping. */
		for (i = 0; (i != priv->rxqs_n); i++)
			mlx5_vlan_strip_queue_set(dev, i, hw_vlan_strip);
	}
	return 0;
}

/*
 * Release VLAN network device, created for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to release.
 */
void mlx5_vlan_vmwa_release(struct rte_eth_dev *dev,
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
	MLX5_ASSERT(vlan_dev[vlan->tag].refcnt);
	if (--vlan_dev[vlan->tag].refcnt == 0 &&
	    vlan_dev[vlan->tag].ifindex) {
		mlx5_nl_vlan_vmwa_delete(vmwa, vlan_dev[vlan->tag].ifindex);
		vlan_dev[vlan->tag].ifindex = 0;
	}
}

/**
 * Acquire VLAN interface with specified tag for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to acquire.
 */
void mlx5_vlan_vmwa_acquire(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_nl_vlan_vmwa_context *vmwa = priv->vmwa_context;
	struct mlx5_nl_vlan_dev *vlan_dev = &vmwa->vlan_dev[0];

	MLX5_ASSERT(!vlan->created);
	MLX5_ASSERT(priv->vmwa_context);
	if (vlan->created || !vmwa)
		return;
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
}

/*
 * Create per ethernet device VLAN VM workaround context
 */
struct mlx5_nl_vlan_vmwa_context *
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
	vmwa->nl_socket = mlx5_nl_init(NETLINK_ROUTE);
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
 */
void mlx5_vlan_vmwa_exit(struct mlx5_nl_vlan_vmwa_context *vmwa)
{
	unsigned int i;

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
