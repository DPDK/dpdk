/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
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

	*switch_port_id = port->id;

	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return 0;

fail_mem_alloc:
fail_find_switch_domain_by_id:
	rte_spinlock_unlock(&sfc_mae_switch.lock);
	return rc;
}

/* This function expects to be called only when the lock is held */
static int
sfc_mae_find_switch_port_by_ethdev(uint16_t switch_domain_id,
				   uint16_t ethdev_port_id,
				   efx_mport_sel_t *mport_sel)
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
			*mport_sel = port->ethdev_mport;
			return 0;
		}
	}

	return ENOENT;
}

int
sfc_mae_switch_port_by_ethdev(uint16_t switch_domain_id,
			      uint16_t ethdev_port_id,
			      efx_mport_sel_t *mport_sel)
{
	int rc;

	rte_spinlock_lock(&sfc_mae_switch.lock);
	rc = sfc_mae_find_switch_port_by_ethdev(switch_domain_id,
						ethdev_port_id, mport_sel);
	rte_spinlock_unlock(&sfc_mae_switch.lock);

	return rc;
}
