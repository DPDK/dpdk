/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include "bnxt.h"
#include "bnxt_vnic.h"
#include "bnxt_tf_common.h"
#include "ulp_port_db.h"

static uint32_t
ulp_port_db_allocate_ifindex(struct bnxt_ulp_port_db *port_db)
{
	uint32_t idx = 1;

	while (idx < port_db->ulp_intf_list_size &&
	       port_db->ulp_intf_list[idx].type != BNXT_ULP_INTF_TYPE_INVALID)
		idx++;

	if (idx >= port_db->ulp_intf_list_size) {
		BNXT_TF_DBG(ERR, "Port DB interface list is full\n");
		return 0;
	}
	return idx;
}

/*
 * Initialize the port database. Memory is allocated in this
 * call and assigned to the port database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_port_db_init(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = rte_zmalloc("bnxt_ulp_port_db",
			      sizeof(struct bnxt_ulp_port_db), 0);
	if (!port_db) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate memory for port db\n");
		return -ENOMEM;
	}

	/* Attach the port database to the ulp context. */
	bnxt_ulp_cntxt_ptr2_port_db_set(ulp_ctxt, port_db);

	/* index 0 is not being used hence add 1 to size */
	port_db->ulp_intf_list_size = BNXT_PORT_DB_MAX_INTF_LIST + 1;
	/* Allocate the port tables */
	port_db->ulp_intf_list = rte_zmalloc("bnxt_ulp_port_db_intf_list",
					     port_db->ulp_intf_list_size *
					     sizeof(struct ulp_interface_info),
					     0);
	if (!port_db->ulp_intf_list) {
		BNXT_TF_DBG(ERR,
			    "Failed to allocate mem for port interface list\n");
		goto error_free;
	}
	return 0;

error_free:
	ulp_port_db_deinit(ulp_ctxt);
	return -ENOMEM;
}

/*
 * Deinitialize the port database. Memory is deallocated in
 * this call.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success.
 */
int32_t	ulp_port_db_deinit(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* Detach the flow database from the ulp context. */
	bnxt_ulp_cntxt_ptr2_port_db_set(ulp_ctxt, NULL);

	/* Free up all the memory. */
	rte_free(port_db->ulp_intf_list);
	rte_free(port_db);
	return 0;
}

/*
 * Update the port database.This api is called when the port
 * details are available during the startup.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * bp [in]. ptr to the device function.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_port_db_dev_port_intf_update(struct bnxt_ulp_context *ulp_ctxt,
					 struct rte_eth_dev *eth_dev)
{
	uint32_t port_id = eth_dev->data->port_id;
	struct ulp_phy_port_info *port_data;
	struct bnxt_ulp_port_db *port_db;
	struct ulp_interface_info *intf;
	uint32_t ifindex;
	int32_t rc;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	rc = ulp_port_db_dev_port_to_ulp_index(ulp_ctxt, port_id, &ifindex);
	if (rc == -ENOENT) {
		/* port not found, allocate one */
		ifindex = ulp_port_db_allocate_ifindex(port_db);
		if (!ifindex)
			return -ENOMEM;
		port_db->dev_port_list[port_id] = ifindex;
	} else if (rc == -EINVAL) {
		return -EINVAL;
	}

	/* update the interface details */
	intf = &port_db->ulp_intf_list[ifindex];

	intf->type = bnxt_get_interface_type(port_id);

	intf->func_id = bnxt_get_fw_func_id(port_id);
	intf->func_svif = bnxt_get_svif(port_id, 1);
	intf->func_spif = bnxt_get_phy_port_id(port_id);
	intf->func_parif = bnxt_get_parif(port_id);
	intf->default_vnic = bnxt_get_vnic_id(port_id);
	intf->phy_port_id = bnxt_get_phy_port_id(port_id);

	if (intf->type == BNXT_ULP_INTF_TYPE_PF) {
		port_data = &port_db->phy_port_list[intf->phy_port_id];
		port_data->port_svif = bnxt_get_svif(port_id, 0);
		port_data->port_spif = bnxt_get_phy_port_id(port_id);
		port_data->port_parif = bnxt_get_parif(port_id);
		port_data->port_vport = bnxt_get_vport(port_id);
	}

	return 0;
}

/*
 * Api to get the ulp ifindex for a given device port.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in].device port id
 * ifindex [out] ulp ifindex
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_dev_port_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				  uint32_t port_id,
				  uint32_t *ifindex)
{
	struct bnxt_ulp_port_db *port_db;

	*ifindex = 0;
	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= RTE_MAX_ETHPORTS) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (!port_db->dev_port_list[port_id])
		return -ENOENT;

	*ifindex = port_db->dev_port_list[port_id];
	return 0;
}

/*
 * Api to get the function id for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * func_id [out] the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_function_id_get(struct bnxt_ulp_context *ulp_ctxt,
			    uint32_t ifindex,
			    uint16_t *func_id)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	*func_id =  port_db->ulp_intf_list[ifindex].func_id;
	return 0;
}

/*
 * Api to get the svif for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * dir [in] the direction for the flow.
 * svif [out] the svif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_svif_get(struct bnxt_ulp_context *ulp_ctxt,
		     uint32_t ifindex,
		     uint32_t dir,
		     uint16_t *svif)
{
	struct bnxt_ulp_port_db *port_db;
	uint16_t phy_port_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (dir == ULP_DIR_EGRESS) {
		*svif = port_db->ulp_intf_list[ifindex].func_svif;
	} else {
		phy_port_id = port_db->ulp_intf_list[ifindex].phy_port_id;
		*svif = port_db->phy_port_list[phy_port_id].port_svif;
	}

	return 0;
}

/*
 * Api to get the spif for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * dir [in] the direction for the flow.
 * spif [out] the spif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_spif_get(struct bnxt_ulp_context *ulp_ctxt,
		     uint32_t ifindex,
		     uint32_t dir,
		     uint16_t *spif)
{
	struct bnxt_ulp_port_db *port_db;
	uint16_t phy_port_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (dir == ULP_DIR_EGRESS) {
		*spif = port_db->ulp_intf_list[ifindex].func_spif;
	} else {
		phy_port_id = port_db->ulp_intf_list[ifindex].phy_port_id;
		*spif = port_db->phy_port_list[phy_port_id].port_spif;
	}

	return 0;
}

/*
 * Api to get the parif for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * dir [in] the direction for the flow.
 * parif [out] the parif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_parif_get(struct bnxt_ulp_context *ulp_ctxt,
		     uint32_t ifindex,
		     uint32_t dir,
		     uint16_t *parif)
{
	struct bnxt_ulp_port_db *port_db;
	uint16_t phy_port_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (dir == ULP_DIR_EGRESS) {
		*parif = port_db->ulp_intf_list[ifindex].func_parif;
	} else {
		phy_port_id = port_db->ulp_intf_list[ifindex].phy_port_id;
		*parif = port_db->phy_port_list[phy_port_id].port_parif;
	}

	return 0;
}

/*
 * Api to get the vnic id for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * vnic [out] the vnic of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_default_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			     uint32_t ifindex,
			     uint16_t *vnic)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	*vnic = port_db->ulp_intf_list[ifindex].default_vnic;
	return 0;
}

/*
 * Api to get the vport id for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 * vport [out] the port of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_vport_get(struct bnxt_ulp_context *ulp_ctxt,
		      uint32_t ifindex, uint16_t *vport)
{
	struct bnxt_ulp_port_db *port_db;
	uint16_t phy_port_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	phy_port_id = port_db->ulp_intf_list[ifindex].phy_port_id;
	*vport = port_db->phy_port_list[phy_port_id].port_vport;
	return 0;
}
