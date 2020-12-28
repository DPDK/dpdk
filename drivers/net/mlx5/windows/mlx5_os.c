/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_windows.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"

/**
 * Get mlx5 device attributes.
 *
 * @param ctx
 *   Pointer to device context.
 *
 * @param device_attr
 *   Pointer to mlx5 device attributes.
 *
 * @return
 *   0 on success, non zero error number otherwise
 */
int
mlx5_os_get_dev_attr(void *ctx, struct mlx5_dev_attr *device_attr)
{
	struct mlx5_context *mlx5_ctx;
	struct mlx5_hca_attr hca_attr;
	void *pv_iseg = NULL;
	u32 cb_iseg = 0;
	int err = 0;

	if (!ctx)
		return -EINVAL;
	mlx5_ctx = (struct mlx5_context *)ctx;
	memset(device_attr, 0, sizeof(*device_attr));
	err = mlx5_devx_cmd_query_hca_attr(mlx5_ctx, &hca_attr);
	if (err) {
		DRV_LOG(ERR, "Failed to get device hca_cap");
		return err;
	}
	device_attr->max_cq = 1 << hca_attr.log_max_cq;
	device_attr->max_qp = 1 << hca_attr.log_max_qp;
	device_attr->max_qp_wr = 1 << hca_attr.log_max_qp_sz;
	device_attr->max_cqe = 1 << hca_attr.log_max_cq_sz;
	device_attr->max_mr = 1 << hca_attr.log_max_mrw_sz;
	device_attr->max_pd = 1 << hca_attr.log_max_pd;
	device_attr->max_srq = 1 << hca_attr.log_max_srq;
	device_attr->max_srq_wr = 1 << hca_attr.log_max_srq_sz;
	if (hca_attr.rss_ind_tbl_cap) {
		device_attr->max_rwq_indirection_table_size =
			1 << hca_attr.rss_ind_tbl_cap;
	}
	pv_iseg = mlx5_glue->query_hca_iseg(mlx5_ctx, &cb_iseg);
	if (pv_iseg == NULL) {
		DRV_LOG(ERR, "Failed to get device hca_iseg");
		return errno;
	}
	if (!err) {
		snprintf(device_attr->fw_ver, 64, "%x.%x.%04x",
			MLX5_GET(initial_seg, pv_iseg, fw_rev_major),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_minor),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_subminor));
	}
	return err;
}

/**
 * Set the completion channel file descriptor interrupt as non-blocking.
 * Currently it has no support under Windows.
 *
 * @param[in] rxq_obj
 *   Pointer to RQ channel object, which includes the channel fd
 *
 * @param[out] fd
 *   The file descriptor (representing the intetrrupt) used in this channel.
 *
 * @return
 *   0 on successfully setting the fd to non-blocking, non-zero otherwise.
 */
int
mlx5_os_set_nonblock_channel_fd(int fd)
{
	(void)fd;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * This function should share events between multiple ports of single IB
 * device.  Currently it has no support under Windows.
 *
 * @param sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_install(struct mlx5_dev_ctx_shared *sh)
{
	(void)sh;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * This function should share events between multiple ports of single IB
 * device.  Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_uninstall(struct mlx5_dev_ctx_shared *sh)
{
	(void)sh;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * Read statistics by a named counter.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 * @param[in] ctr_name
 *   Pointer to the name of the statistic counter to read
 * @param[out] stat
 *   Pointer to read statistic value.
 * @return
 *   0 on success and stat is valud, 1 if failed to read the value
 *   rte_errno is set.
 *
 */
int
mlx5_os_read_dev_stat(struct mlx5_priv *priv, const char *ctr_name,
		      uint64_t *stat)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(ctr_name);
	RTE_SET_USED(stat);
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Flush device MAC addresses
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 */
void
mlx5_os_mac_addr_flush(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * Remove a MAC address from device
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_os_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	(void)dev;
	(void)(index);
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * Adds a MAC address to the device
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise
 */
int
mlx5_os_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
		     uint32_t index)
{
	(void)index;
	struct rte_ether_addr lmac;

	if (mlx5_get_mac(dev, &lmac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s)",
			dev->data->port_id, strerror(rte_errno));
		return rte_errno;
	}
	if (!rte_is_same_ether_addr(&lmac, mac)) {
		DRV_LOG(ERR,
			"adding new mac address to device is unsupported");
		return -ENOTSUP;
	}
	return 0;
}

/**
 * Modify a VF MAC address
 * Currently it has no support under Windows.
 *
 * @param priv
 *   Pointer to device private data.
 * @param mac_addr
 *   MAC address to modify into.
 * @param iface_idx
 *   Net device interface index
 * @param vf_index
 *   VF index
 *
 * @return
 *   0 on success, a negative errno value otherwise
 */
int
mlx5_os_vf_mac_addr_modify(struct mlx5_priv *priv,
			   unsigned int iface_idx,
			   struct rte_ether_addr *mac_addr,
			   int vf_index)
{
	(void)priv;
	(void)iface_idx;
	(void)mac_addr;
	(void)vf_index;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Set device promiscuous mode
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   0 - promiscuous is disabled, otherwise - enabled
 *
 * @return
 *   0 on success, a negative error value otherwise
 */
int
mlx5_os_set_promisc(struct rte_eth_dev *dev, int enable)
{
	(void)dev;
	(void)enable;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Set device allmulti mode
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   0 - all multicase is disabled, otherwise - enabled
 *
 * @return
 *   0 on success, a negative error value otherwise
 */
int
mlx5_os_set_allmulti(struct rte_eth_dev *dev, int enable)
{
	(void)dev;
	(void)enable;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops = {0};

const struct eth_dev_ops mlx5_os_dev_ops = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_close = mlx5_dev_close,
	.mtu_set = mlx5_dev_set_mtu,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.read_clock = mlx5_read_clock,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.reta_update = mlx5_dev_rss_reta_update,
	.reta_query = mlx5_dev_rss_reta_query,
	.rss_hash_update = mlx5_rss_hash_update,
	.rss_hash_conf_get = mlx5_rss_hash_conf_get,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.udp_tunnel_port_add  = mlx5_udp_tunnel_port_add,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};

/* Available operations from secondary process. */
const struct eth_dev_ops mlx5_os_dev_sec_ops = {0};

/* Available operations in flow isolated mode. */
const struct eth_dev_ops mlx5_os_dev_ops_isolate = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_close = mlx5_dev_close,
	.mtu_set = mlx5_dev_set_mtu,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};
