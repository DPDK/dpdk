/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_PORT_DB_H_
#define _ULP_PORT_DB_H_

#include "bnxt_ulp.h"

#define BNXT_PORT_DB_MAX_INTF_LIST		256
#define BNXT_PORT_DB_MAX_FUNC			2048
#define BNXT_ULP_FREE_PARIF_BASE		11

enum bnxt_ulp_svif_type {
	BNXT_ULP_DRV_FUNC_SVIF = 0,
	BNXT_ULP_VF_FUNC_SVIF,
	BNXT_ULP_PHY_PORT_SVIF
};

enum bnxt_ulp_spif_type {
	BNXT_ULP_DRV_FUNC_SPIF = 0,
	BNXT_ULP_VF_FUNC_SPIF,
	BNXT_ULP_PHY_PORT_SPIF
};

enum bnxt_ulp_parif_type {
	BNXT_ULP_DRV_FUNC_PARIF = 0,
	BNXT_ULP_VF_FUNC_PARIF,
	BNXT_ULP_PHY_PORT_PARIF
};

enum bnxt_ulp_vnic_type {
	BNXT_ULP_DRV_FUNC_VNIC = 0,
	BNXT_ULP_VF_FUNC_VNIC
};

enum bnxt_ulp_fid_type {
	BNXT_ULP_DRV_FUNC_FID,
	BNXT_ULP_VF_FUNC_FID
};

struct ulp_func_if_info {
	uint16_t		func_valid;
	uint16_t		func_svif;
	uint16_t		func_spif;
	uint16_t		func_parif;
	uint16_t		func_vnic;
	uint8_t			func_mac[RTE_ETHER_ADDR_LEN];
	uint16_t		func_parent_vnic;
	uint8_t			func_parent_mac[RTE_ETHER_ADDR_LEN];
	uint16_t		phy_port_id;
	uint16_t		ifindex;
};

/* Structure for the Port database resource information. */
struct ulp_interface_info {
	enum bnxt_ulp_intf_type	type;
	uint16_t		drv_func_id;
	uint16_t		vf_func_id;
};

struct ulp_phy_port_info {
	uint16_t	port_valid;
	uint16_t	port_svif;
	uint16_t	port_spif;
	uint16_t	port_parif;
	uint16_t	port_vport;
};

/* Structure for the Port database */
struct bnxt_ulp_port_db {
	struct ulp_interface_info	*ulp_intf_list;
	uint32_t			ulp_intf_list_size;

	/* dpdk device external port list */
	uint16_t			dev_port_list[RTE_MAX_ETHPORTS];
	struct ulp_phy_port_info	*phy_port_list;
	uint16_t			phy_port_cnt;
	struct ulp_func_if_info		ulp_func_id_tbl[BNXT_PORT_DB_MAX_FUNC];
};

/*
 * Initialize the port database. Memory is allocated in this
 * call and assigned to the port database.
 *
 * ulp_ctxt [in] Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t	ulp_port_db_init(struct bnxt_ulp_context *ulp_ctxt, uint8_t port_cnt);

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
					 struct rte_eth_dev *eth_dev);

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
				  uint32_t port_id, uint32_t *ifindex);

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
			    uint32_t ifindex, uint32_t fid_type,
			    uint16_t *func_id);

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
		     uint32_t ifindex, uint32_t dir, uint16_t *svif);

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
		     uint32_t ifindex, uint32_t dir, uint16_t *spif);


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
		      uint32_t ifindex, uint32_t dir, uint16_t *parif);

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
			     uint32_t ifindex, uint32_t vnic_type,
			     uint16_t *vnic);

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
		      uint32_t ifindex,	uint16_t *vport);

/*
 * Api to get the vport for a given physical port.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * phy_port [in] physical port index
 * out_port [out] the port of the given physical index
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_phy_port_vport_get(struct bnxt_ulp_context *ulp_ctxt,
			       uint32_t phy_port,
			       uint16_t *out_port);

/*
 * Api to get the svif for a given physical port.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * phy_port [in] physical port index
 * svif [out] the svif of the given physical index
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_phy_port_svif_get(struct bnxt_ulp_context *ulp_ctxt,
			      uint32_t phy_port,
			      uint16_t *svif);

/*
 * Api to get the port type for a given ulp ifindex.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * ifindex [in] ulp ifindex
 *
 * Returns port type.
 */
enum bnxt_ulp_intf_type
ulp_port_db_port_type_get(struct bnxt_ulp_context *ulp_ctxt,
			  uint32_t ifindex);

/*
 * Api to get the ulp ifindex for a given function id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * func_id [in].device func id
 * ifindex [out] ulp ifindex
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_dev_func_id_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				     uint32_t func_id, uint32_t *ifindex);

/*
 * Api to get the function id for a given port id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] dpdk port id
 * func_id [out] the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_port_func_id_get(struct bnxt_ulp_context *ulp_ctxt,
			     uint16_t port_id, uint16_t *func_id);

/*
 * Api to get the parent mac address for a given port id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] device port id
 * mac_addr [out] mac address
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_parent_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
				uint32_t port_id, uint8_t **mac_addr);

/*
 * Api to get the mac address for a given port id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] device port id
 * mac_addr [out] mac address
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_drv_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
			     uint32_t port_id, uint8_t **mac_addr);

/*
 * Api to get the parent vnic for a given port id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] device port id
 * vnic [out] parent vnic
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_parent_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			    uint32_t port_id, uint8_t **vnic);

/*
 * Api to get the phy port for a given port id.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] device port id
 * phy_port [out] phy_port of the dpdk port_id
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_port_db_phy_port_get(struct bnxt_ulp_context *ulp_ctxt,
			 uint32_t port_id, uint16_t *phy_port);

#endif /* _ULP_PORT_DB_H_ */
