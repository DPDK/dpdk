/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_PORT_DB_H_
#define _ULP_PORT_DB_H_

#include "bnxt_ulp.h"

#define BNXT_PORT_DB_MAX_INTF_LIST		256

/* enumeration of the interface types */
enum bnxt_ulp_intf_type {
	BNXT_ULP_INTF_TYPE_INVALID = 0,
	BNXT_ULP_INTF_TYPE_PF = 1,
	BNXT_ULP_INTF_TYPE_VF,
	BNXT_ULP_INTF_TYPE_PF_REP,
	BNXT_ULP_INTF_TYPE_VF_REP,
	BNXT_ULP_INTF_TYPE_LAST
};

/* Structure for the Port database resource information. */
struct ulp_interface_info {
	enum bnxt_ulp_intf_type	type;
	uint16_t		func_id;
	uint16_t		func_svif;
	uint16_t		port_svif;
	uint16_t		default_vnic;
	uint8_t			mac_addr[RTE_ETHER_ADDR_LEN];
	/* back pointer to the bnxt driver, it is null for rep ports */
	struct bnxt		*bp;
};

/* Structure for the Port database */
struct bnxt_ulp_port_db {
	struct ulp_interface_info	*ulp_intf_list;
	uint32_t			ulp_intf_list_size;

	/* dpdk device external port list */
	uint16_t			dev_port_list[RTE_MAX_ETHPORTS];
};

/*
 * Initialize the port database. Memory is allocated in this
 * call and assigned to the port database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_port_db_init(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Deinitialize the port database. Memory is deallocated in
 * this call.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success.
 */
int32_t	ulp_port_db_deinit(struct bnxt_ulp_context *ulp_ctxt);

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
					 struct bnxt *bp);

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
				  uint32_t *ifindex);

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
			    uint16_t *func_id);

/*
 * Api to get the svid for a given ulp ifindex.
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
		     uint16_t *svif);

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
			     uint16_t *vnic);

#endif /* _ULP_PORT_DB_H_ */
