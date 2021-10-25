/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>

#include <rte_common.h>
#include <rte_spinlock.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_switch.h"

/**
 * Switch port registry entry.
 *
 * Drivers aware of RTE switch domains also have to maintain RTE switch
 * port IDs for RTE ethdev instances they operate. These IDs are supposed
 * to stand for physical interconnect entities, in example, PCIe functions.
 *
 * In terms of MAE, a physical interconnect entity can be referred to using
 * an MPORT selector, that is, a 32-bit value. RTE switch port IDs, in turn,
 * are 16-bit values, so indirect mapping has to be maintained:
 *
 * +--------------------+          +---------------------------------------+
 * | RTE switch port ID |  ------  |         MAE switch port entry         |
 * +--------------------+          |         ---------------------         |
 *                                 |                                       |
 *                                 | Entity (PCIe function) MPORT selector |
 *                                 |                   +                   |
 *                                 |  Port type (independent/representor)  |
 *                                 +---------------------------------------+
 *
 * This mapping comprises a port type to ensure that RTE switch port ID
 * of a represented entity and that of its representor are different in
 * the case when the entity gets plugged into DPDK and not into a guest.
 *
 * Entry data also comprises RTE ethdev's own MPORT. This value
 * coincides with the entity MPORT in the case of independent ports.
 * In the case of representors, this ID is not a selector and refers
 * to an allocatable object (that is, it's likely to change on RTE
 * ethdev replug). Flow API backend must use this value rather
 * than entity_mport to support flow rule action PORT_ID.
 */
struct sfc_mae_switch_port {
	TAILQ_ENTRY(sfc_mae_switch_port)	switch_domain_ports;

	/** RTE ethdev MPORT */
	efx_mport_sel_t				ethdev_mport;
	/** RTE ethdev port ID */
	uint16_t				ethdev_port_id;

	/** Entity (PCIe function) MPORT selector */
	efx_mport_sel_t				entity_mport;
	/** Port type (independent/representor) */
	enum sfc_mae_switch_port_type		type;
	/** RTE switch port ID */
	uint16_t				id;

	union sfc_mae_switch_port_data		data;
};

TAILQ_HEAD(sfc_mae_switch_ports, sfc_mae_switch_port);

/**
 * Switch domain registry entry.
 *
 * Even if an RTE ethdev instance gets unplugged, the corresponding
 * entry in the switch port registry will not be removed because the
 * entity (PCIe function) MPORT is static and cannot change. If this
 * RTE ethdev gets plugged back, the entry will be reused, and
 * RTE switch port ID will be the same.
 */
struct sfc_mae_switch_domain {
	TAILQ_ENTRY(sfc_mae_switch_domain)	entries;

	/** HW switch ID */
	struct sfc_hw_switch_id			*hw_switch_id;
	/** The number of ports in the switch port registry */
	unsigned int				nb_ports;
	/** Switch port registry */
	struct sfc_mae_switch_ports		ports;
	/** RTE switch domain ID allocated for a group of devices */
	uint16_t				id;
	/** DPDK controller -> EFX interface mapping */
	efx_pcie_interface_t			*controllers;
	/** Number of DPDK controllers and EFX interfaces */
	size_t					nb_controllers;
	/** MAE admin port */
	struct sfc_mae_switch_port		*mae_admin_port;
};

TAILQ_HEAD(sfc_mae_switch_domains, sfc_mae_switch_domain);

/**
 * MAE representation of RTE switch infrastructure.
 *
 * It is possible that an RTE flow API client tries to insert a rule
 * referencing an RTE ethdev deployed on top of a different physical
 * device (it may belong to the same vendor or not). This particular
 * driver/engine cannot support this and has to turn down such rules.
 *
 * Technically, it's HW switch identifier which, if queried for each
 * RTE ethdev instance, indicates relationship between the instances.
 * In the meantime, RTE flow API clients also need to somehow figure
 * out relationship between RTE ethdev instances in advance.
 *
 * The concept of RTE switch domains resolves this issue. The driver
 * maintains a static list of switch domains which is easy to browse,
 * and each RTE ethdev fills RTE switch parameters in device
 * information structure which is made available to clients.
 *
 * Even if all RTE ethdev instances belonging to a switch domain get
 * unplugged, the corresponding entry in the switch domain registry
 * will not be removed because the corresponding HW switch exists
 * regardless of its ports being plugged to DPDK or kept aside.
 * If a port gets plugged back to DPDK, the corresponding
 * RTE ethdev will indicate the same RTE switch domain ID.
 */
struct sfc_mae_switch {
	/** A lock to protect the whole structure */
	rte_spinlock_t			lock;
	/** Switch domain registry */
	struct sfc_mae_switch_domains	domains;
};

static struct sfc_mae_switch sfc_mae_switch = {
	.lock = RTE_SPINLOCK_INITIALIZER,
	.domains = TAILQ_HEAD_INITIALIZER(sfc_mae_switch.domains),
};


/* This function expects to be called only when the lock is held */
static struct sfc_mae_switch_domain *
sfc_mae_find_switch_domain_by_id(uint16_t switch_domain_id)
{
	struct sfc_mae_switch_domain *domain;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	TAILQ_FOREACH(domain, &sfc_mae_switch.domains, entries) {
		if (domain->id == switch_domain_id)
			return domain;
	}

	return NULL;
}

int
sfc_mae_switch_ports_iterate(uint16_t switch_domain_id,
			     sfc_mae_switch_port_iterator_cb *cb,
			     void *data)
{
	struct sfc_mae_switch_domain *domain;
	struct sfc_mae_switch_port *port;

	if (cb == NULL)
		return EINVAL;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL) {
		rte_spinlock_unlock(&sfc_mae_switch.lock);
		return EINVAL;
	}

	TAILQ_FOREACH(port, &domain->ports, switch_domain_ports) {
		cb(port->type, &port->ethdev_mport, port->ethdev_port_id,
		   &port->entity_mport, port->id, &port->data, data);
	}

	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return 0;
}

/* This function expects to be called only when the lock is held */
static struct sfc_mae_switch_domain *
sfc_mae_find_switch_domain_by_hw_switch_id(const struct sfc_hw_switch_id *id)
{
	struct sfc_mae_switch_domain *domain;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	TAILQ_FOREACH(domain, &sfc_mae_switch.domains, entries) {
		if (sfc_hw_switch_ids_equal(domain->hw_switch_id, id))
			return domain;
	}

	return NULL;
}

int
sfc_mae_assign_switch_domain(struct sfc_adapter *sa,
			     uint16_t *switch_domain_id)
{
	struct sfc_hw_switch_id *hw_switch_id;
	struct sfc_mae_switch_domain *domain;
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	rc = sfc_hw_switch_id_init(sa, &hw_switch_id);
	if (rc != 0)
		goto fail_hw_switch_id_init;

	domain = sfc_mae_find_switch_domain_by_hw_switch_id(hw_switch_id);
	if (domain != NULL) {
		sfc_hw_switch_id_fini(sa, hw_switch_id);
		goto done;
	}

	domain = rte_zmalloc("sfc_mae_switch_domain", sizeof(*domain), 0);
	if (domain == NULL) {
		rc = ENOMEM;
		goto fail_mem_alloc;
	}

	/*
	 * This code belongs to driver init path, that is, negation is
	 * done at the end of the path by sfc_eth_dev_init(). RTE APIs
	 * negate error codes, so drop negation here.
	 */
	rc = -rte_eth_switch_domain_alloc(&domain->id);
	if (rc != 0)
		goto fail_domain_alloc;

	domain->hw_switch_id = hw_switch_id;

	TAILQ_INIT(&domain->ports);

	TAILQ_INSERT_TAIL(&sfc_mae_switch.domains, domain, entries);

done:
	*switch_domain_id = domain->id;

	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return 0;

fail_domain_alloc:
	rte_free(domain);

fail_mem_alloc:
	sfc_hw_switch_id_fini(sa, hw_switch_id);

fail_hw_switch_id_init:
	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return rc;
}

int
sfc_mae_switch_domain_controllers(uint16_t switch_domain_id,
				  const efx_pcie_interface_t **controllers,
				  size_t *nb_controllers)
{
	struct sfc_mae_switch_domain *domain;

	if (controllers == NULL || nb_controllers == NULL)
		return EINVAL;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL) {
		rte_spinlock_unlock(&sfc_mae_switch.lock);
		return EINVAL;
	}

	*controllers = domain->controllers;
	*nb_controllers = domain->nb_controllers;

	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return 0;
}

int
sfc_mae_switch_domain_map_controllers(uint16_t switch_domain_id,
				      efx_pcie_interface_t *controllers,
				      size_t nb_controllers)
{
	struct sfc_mae_switch_domain *domain;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL) {
		rte_spinlock_unlock(&sfc_mae_switch.lock);
		return EINVAL;
	}

	/* Controller mapping may be set only once */
	if (domain->controllers != NULL) {
		rte_spinlock_unlock(&sfc_mae_switch.lock);
		return EINVAL;
	}

	domain->controllers = controllers;
	domain->nb_controllers = nb_controllers;

	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return 0;
}

int
sfc_mae_switch_controller_from_mapping(const efx_pcie_interface_t *controllers,
				       size_t nb_controllers,
				       efx_pcie_interface_t intf,
				       int *controller)
{
	size_t i;

	if (controllers == NULL)
		return ENOENT;

	for (i = 0; i < nb_controllers; i++) {
		if (controllers[i] == intf) {
			*controller = i;
			return 0;
		}
	}

	return ENOENT;
}

int
sfc_mae_switch_domain_get_controller(uint16_t switch_domain_id,
				     efx_pcie_interface_t intf,
				     int *controller)
{
	const efx_pcie_interface_t *controllers;
	size_t nb_controllers;
	int rc;

	rc = sfc_mae_switch_domain_controllers(switch_domain_id, &controllers,
					       &nb_controllers);
	if (rc != 0)
		return rc;

	return sfc_mae_switch_controller_from_mapping(controllers,
						      nb_controllers,
						      intf,
						      controller);
}

int sfc_mae_switch_domain_get_intf(uint16_t switch_domain_id,
				   int controller,
				   efx_pcie_interface_t *intf)
{
	const efx_pcie_interface_t *controllers;
	size_t nb_controllers;
	int rc;

	rc = sfc_mae_switch_domain_controllers(switch_domain_id, &controllers,
					       &nb_controllers);
	if (rc != 0)
		return rc;

	if (controllers == NULL)
		return ENOENT;

	if ((size_t)controller > nb_controllers)
		return EINVAL;

	*intf = controllers[controller];

	return 0;
}

/* This function expects to be called only when the lock is held */
static struct sfc_mae_switch_port *
sfc_mae_find_switch_port_by_entity(const struct sfc_mae_switch_domain *domain,
				   const efx_mport_sel_t *entity_mportp,
				   enum sfc_mae_switch_port_type type)
{
	struct sfc_mae_switch_port *port;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	TAILQ_FOREACH(port, &domain->ports, switch_domain_ports) {
		if (port->entity_mport.sel == entity_mportp->sel &&
		    port->type == type)
			return port;
	}

	return NULL;
}

/* This function expects to be called only when the lock is held */
static int
sfc_mae_find_switch_port_id_by_entity(uint16_t switch_domain_id,
				      const efx_mport_sel_t *entity_mportp,
				      enum sfc_mae_switch_port_type type,
				      uint16_t *switch_port_id)
{
	struct sfc_mae_switch_domain *domain;
	struct sfc_mae_switch_port *port;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL)
		return EINVAL;

	port = sfc_mae_find_switch_port_by_entity(domain, entity_mportp, type);
	if (port == NULL)
		return ENOENT;

	*switch_port_id = port->id;
	return 0;
}

int
sfc_mae_assign_switch_port(uint16_t switch_domain_id,
			   const struct sfc_mae_switch_port_request *req,
			   uint16_t *switch_port_id)
{
	struct sfc_mae_switch_domain *domain;
	struct sfc_mae_switch_port *port;
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL) {
		rc = EINVAL;
		goto fail_find_switch_domain_by_id;
	}

	port = sfc_mae_find_switch_port_by_entity(domain, req->entity_mportp,
						  req->type);
	if (port != NULL)
		goto done;

	port = rte_zmalloc("sfc_mae_switch_port", sizeof(*port), 0);
	if (port == NULL) {
		rc = ENOMEM;
		goto fail_mem_alloc;
	}

	port->entity_mport.sel = req->entity_mportp->sel;
	port->type = req->type;

	port->id = (domain->nb_ports++);

	TAILQ_INSERT_TAIL(&domain->ports, port, switch_domain_ports);

done:
	port->ethdev_mport = *req->ethdev_mportp;
	port->ethdev_port_id = req->ethdev_port_id;

	memcpy(&port->data, &req->port_data,
	       sizeof(port->data));

	switch (req->type) {
	case SFC_MAE_SWITCH_PORT_INDEPENDENT:
		if (port->data.indep.mae_admin) {
			SFC_ASSERT(domain->mae_admin_port == NULL);
			domain->mae_admin_port = port;
		}
		break;
	case SFC_MAE_SWITCH_PORT_REPRESENTOR:
		break;
	default:
		SFC_ASSERT(B_FALSE);
	}

	*switch_port_id = port->id;

	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return 0;

fail_mem_alloc:
fail_find_switch_domain_by_id:
	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return rc;
}

int
sfc_mae_clear_switch_port(uint16_t switch_domain_id,
			  uint16_t switch_port_id)
{
	struct sfc_mae_switch_domain *domain;

	rte_spinlock_lock(&sfc_mae_switch.lock);

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL) {
		rte_spinlock_unlock(&sfc_mae_switch.lock);
		return EINVAL;
	}

	if (domain->mae_admin_port != NULL &&
	    domain->mae_admin_port->id == switch_port_id) {
		domain->mae_admin_port->data.indep.mae_admin = B_FALSE;
		domain->mae_admin_port = NULL;
	}

	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return 0;
}

/* This function expects to be called only when the lock is held */
static int
sfc_mae_find_switch_port_by_ethdev(uint16_t switch_domain_id,
				   uint16_t ethdev_port_id,
				   struct sfc_mae_switch_port **switch_port)
{
	struct sfc_mae_switch_domain *domain;
	struct sfc_mae_switch_port *port;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	if (ethdev_port_id == RTE_MAX_ETHPORTS)
		return EINVAL;

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL)
		return EINVAL;

	TAILQ_FOREACH(port, &domain->ports, switch_domain_ports) {
		if (port->ethdev_port_id == ethdev_port_id) {
			*switch_port = port;
			return 0;
		}
	}

	return ENOENT;
}

int
sfc_mae_switch_get_ethdev_mport(uint16_t switch_domain_id,
				uint16_t ethdev_port_id,
				efx_mport_sel_t *mport_sel)
{
	struct sfc_mae_switch_port *port;
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);
	rc = sfc_mae_find_switch_port_by_ethdev(switch_domain_id,
						ethdev_port_id, &port);
	if (rc != 0)
		goto unlock;

	if (port->type != SFC_MAE_SWITCH_PORT_INDEPENDENT) {
		/*
		 * The ethdev is a "VF representor". It does not own
		 * a dedicated m-port suitable for use in flow rules.
		 */
		rc = ENOTSUP;
		goto unlock;
	}

	*mport_sel = port->ethdev_mport;

unlock:
	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return rc;
}

int
sfc_mae_switch_get_entity_mport(uint16_t switch_domain_id,
				uint16_t ethdev_port_id,
				efx_mport_sel_t *mport_sel)
{
	static struct sfc_mae_switch_port *port;
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);
	rc = sfc_mae_find_switch_port_by_ethdev(switch_domain_id,
						ethdev_port_id, &port);
	if (rc != 0)
		goto unlock;

	if (port->type == SFC_MAE_SWITCH_PORT_INDEPENDENT &&
	    !port->data.indep.mae_admin) {
		/* See sfc_mae_assign_entity_mport() */
		rc = ENOTSUP;
		goto unlock;
	}

	*mport_sel = port->entity_mport;

unlock:
	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return rc;
}

int
sfc_mae_switch_port_id_by_entity(uint16_t switch_domain_id,
				 const efx_mport_sel_t *entity_mportp,
				 enum sfc_mae_switch_port_type type,
				 uint16_t *switch_port_id)
{
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);
	rc = sfc_mae_find_switch_port_id_by_entity(switch_domain_id,
						   entity_mportp, type,
						   switch_port_id);
	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return rc;
}

static int
sfc_mae_get_switch_domain_admin_locked(uint16_t switch_domain_id,
				       uint16_t *port_id)
{
	struct sfc_mae_switch_domain *domain;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_mae_switch.lock));

	domain = sfc_mae_find_switch_domain_by_id(switch_domain_id);
	if (domain == NULL)
		return EINVAL;

	if (domain->mae_admin_port != NULL) {
		*port_id = domain->mae_admin_port->ethdev_port_id;
		return 0;
	}

	return ENOENT;
}

int
sfc_mae_get_switch_domain_admin(uint16_t switch_domain_id,
				uint16_t *port_id)
{
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);
	rc = sfc_mae_get_switch_domain_admin_locked(switch_domain_id, port_id);
	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return rc;
}
