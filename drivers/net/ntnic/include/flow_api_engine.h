/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_ENGINE_H_
#define _FLOW_API_ENGINE_H_

/*
 * Resource management
 */
#define BIT_CONTAINER_8_ALIGN(x) (((x) + 7) / 8)

/*
 * Resource management
 * These are free resources in FPGA
 * Other FPGA memory lists are linked to one of these
 * and will implicitly follow them
 */
enum res_type_e {
	RES_QUEUE,
	RES_CAT_CFN,
	RES_CAT_COT,
	RES_CAT_EXO,
	RES_CAT_LEN,
	RES_KM_FLOW_TYPE,
	RES_KM_CATEGORY,
	RES_HSH_RCP,
	RES_PDB_RCP,
	RES_QSL_RCP,
	RES_QSL_QST,
	RES_SLC_LR_RCP,

	RES_FLM_FLOW_TYPE,
	RES_FLM_RCP,
	RES_TPE_RCP,
	RES_TPE_EXT,
	RES_TPE_RPL,
	RES_SCRUB_RCP,
	RES_COUNT,
	RES_INVALID
};

void km_free_ndev_resource_management(void **handle);

void kcc_free_ndev_resource_management(void **handle);

#endif  /* _FLOW_API_ENGINE_H_ */
