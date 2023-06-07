/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_SWITCH_H
#define _SFC_SWITCH_H

#include <stdint.h>

#include "efx.h"

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Options for MAE switch port type */
enum sfc_mae_switch_port_type {
	/**
	 * The switch port is operated by a self-sufficient RTE ethdev
	 * and thus refers to its underlying PCIe function
	 */
	SFC_MAE_SWITCH_PORT_INDEPENDENT = 0,
	/**
	 * The switch port is operated by a representor RTE ethdev
	 * and thus refers to the represented PCIe function
	 */
	SFC_MAE_SWITCH_PORT_REPRESENTOR,
};

struct sfc_mae_switch_port_indep_data {
	bool					mae_admin;
};

struct sfc_mae_switch_port_repr_data {
	efx_pcie_interface_t			intf;
	uint16_t				pf;
	uint16_t				vf;
};

union sfc_mae_switch_port_data {
	struct sfc_mae_switch_port_indep_data	indep;
	struct sfc_mae_switch_port_repr_data	repr;
};

struct sfc_mae_switch_port_request {
	enum sfc_mae_switch_port_type		type;
	const efx_mport_sel_t			*entity_mportp;
	const efx_mport_sel_t			*ethdev_mportp;
	uint16_t				ethdev_port_id;
	union sfc_mae_switch_port_data		port_data;
};

typedef void (sfc_mae_switch_port_iterator_cb)(
		enum sfc_mae_switch_port_type type,
		const efx_mport_sel_t *ethdev_mportp,
		uint16_t ethdev_port_id,
		const efx_mport_sel_t *entity_mportp,
		uint16_t switch_port_id,
		union sfc_mae_switch_port_data *port_datap,
		void *user_datap);

int sfc_mae_switch_ports_iterate(uint16_t switch_domain_id,
				 sfc_mae_switch_port_iterator_cb *cb,
				 void *data);

int sfc_mae_assign_switch_domain(struct sfc_adapter *sa,
				 uint16_t *switch_domain_id);

int sfc_mae_switch_domain_controllers(uint16_t switch_domain_id,
				      const efx_pcie_interface_t **controllers,
				      size_t *nb_controllers);

int sfc_mae_switch_domain_map_controllers(uint16_t switch_domain_id,
					  efx_pcie_interface_t *controllers,
					  size_t nb_controllers);

int sfc_mae_switch_controller_from_mapping(
		const efx_pcie_interface_t *controllers,
		size_t nb_controllers,
		efx_pcie_interface_t intf,
		int *controller);

int sfc_mae_switch_domain_get_controller(uint16_t switch_domain_id,
				   efx_pcie_interface_t intf,
				   int *controller);

int sfc_mae_switch_domain_get_intf(uint16_t switch_domain_id,
				   int controller,
				   efx_pcie_interface_t *intf);

int sfc_mae_assign_switch_port(uint16_t switch_domain_id,
			       const struct sfc_mae_switch_port_request *req,
			       uint16_t *switch_port_id);

int sfc_mae_clear_switch_port(uint16_t switch_domain_id,
			      uint16_t switch_port_id);

/*
 * For user flows, allowed_mae_switch_port_types can only contain bit
 * SFC_MAE_SWITCH_PORT_INDEPENDENT, meaning that only those ethdevs
 * that have their own MAE m-ports can be accessed by a port-based
 * action. For driver-internal flows, this mask can also contain
 * bit SFC_MAE_SWITCH_PORT_REPRESENTOR to allow VF traffic to be
 * sent to the common MAE m-port of all such REPRESENTOR ports
 * via a port-based action, for default switch interconnection.
 */
int sfc_mae_switch_get_ethdev_mport(uint16_t switch_domain_id,
				    uint16_t ethdev_port_id,
				    unsigned int allowed_mae_switch_port_types,
				    efx_mport_sel_t *mport_sel);

int sfc_mae_switch_get_entity_mport(uint16_t switch_domain_id,
				    uint16_t ethdev_port_id,
				    efx_mport_sel_t *mport_sel);

int sfc_mae_switch_port_id_by_entity(uint16_t switch_domain_id,
				     const efx_mport_sel_t *entity_mportp,
				     enum sfc_mae_switch_port_type type,
				     uint16_t *switch_port_id);

int sfc_mae_get_switch_domain_admin(uint16_t switch_domain_id,
				    uint16_t *port_id);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_SWITCH_H */
