/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "opae_ifpga_hw_api.h"
#include "ifpga_api.h"

int opae_manager_ifpga_get_prop(struct opae_manager *mgr,
				struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme;

	if (!mgr || !mgr->data)
		return -EINVAL;

	fme = mgr->data;

	return ifpga_get_prop(fme->parent, FEATURE_FIU_ID_FME, 0, prop);
}

int opae_manager_ifpga_set_prop(struct opae_manager *mgr,
				struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme;

	if (!mgr || !mgr->data)
		return -EINVAL;

	fme = mgr->data;

	return ifpga_set_prop(fme->parent, FEATURE_FIU_ID_FME, 0, prop);
}

int opae_manager_ifpga_get_info(struct opae_manager *mgr,
				struct fpga_fme_info *fme_info)
{
	struct ifpga_fme_hw *fme;

	if (!mgr || !mgr->data || !fme_info)
		return -EINVAL;

	fme = mgr->data;

	spinlock_lock(&fme->lock);
	fme_info->capability = fme->capability;
	spinlock_unlock(&fme->lock);

	return 0;
}

int opae_manager_ifpga_set_err_irq(struct opae_manager *mgr,
				   struct fpga_fme_err_irq_set *err_irq_set)
{
	struct ifpga_fme_hw *fme;

	if (!mgr || !mgr->data)
		return -EINVAL;

	fme = mgr->data;

	return ifpga_set_irq(fme->parent, FEATURE_FIU_ID_FME, 0,
			     IFPGA_FME_FEATURE_ID_GLOBAL_ERR, err_irq_set);
}

int opae_bridge_ifpga_get_prop(struct opae_bridge *br,
			       struct feature_prop *prop)
{
	struct ifpga_port_hw *port;

	if (!br || !br->data)
		return -EINVAL;

	port = br->data;

	return ifpga_get_prop(port->parent, FEATURE_FIU_ID_PORT,
			      port->port_id, prop);
}

int opae_bridge_ifpga_set_prop(struct opae_bridge *br,
			       struct feature_prop *prop)
{
	struct ifpga_port_hw *port;

	if (!br || !br->data)
		return -EINVAL;

	port = br->data;

	return ifpga_set_prop(port->parent, FEATURE_FIU_ID_PORT,
			      port->port_id, prop);
}

int opae_bridge_ifpga_get_info(struct opae_bridge *br,
			       struct fpga_port_info *port_info)
{
	struct ifpga_port_hw *port;

	if (!br || !br->data || !port_info)
		return -EINVAL;

	port = br->data;

	spinlock_lock(&port->lock);
	port_info->capability = port->capability;
	port_info->num_uafu_irqs = port->num_uafu_irqs;
	spinlock_unlock(&port->lock);

	return 0;
}

int opae_bridge_ifpga_get_region_info(struct opae_bridge *br,
				      struct fpga_port_region_info *info)
{
	struct ifpga_port_hw *port;

	if (!br || !br->data || !info)
		return -EINVAL;

	/* Only support STP region now */
	if (info->index != PORT_REGION_INDEX_STP)
		return -EINVAL;

	port = br->data;

	spinlock_lock(&port->lock);
	info->addr = port->stp_addr;
	info->size = port->stp_size;
	spinlock_unlock(&port->lock);

	return 0;
}

int opae_bridge_ifpga_set_err_irq(struct opae_bridge *br,
				  struct fpga_port_err_irq_set *err_irq_set)
{
	struct ifpga_port_hw *port;

	if (!br || !br->data)
		return -EINVAL;

	port = br->data;

	return ifpga_set_irq(port->parent, FEATURE_FIU_ID_PORT, port->port_id,
			     IFPGA_PORT_FEATURE_ID_ERROR, err_irq_set);
}
