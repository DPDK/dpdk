/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

/*
 * date: Mon Mar  9 02:37:53 2020
 * version: 0.0
 */

#ifndef ULP_TEMPLATE_DB_H_
#define ULP_TEMPLATE_DB_H_

#define BNXT_ULP_MAX_NUM_DEVICES 4

enum bnxt_ulp_byte_order {
	BNXT_ULP_BYTE_ORDER_BE,
	BNXT_ULP_BYTE_ORDER_LE,
	BNXT_ULP_BYTE_ORDER_LAST
};

enum bnxt_ulp_device_id {
	BNXT_ULP_DEVICE_ID_WH_PLUS,
	BNXT_ULP_DEVICE_ID_THOR,
	BNXT_ULP_DEVICE_ID_STINGRAY,
	BNXT_ULP_DEVICE_ID_STINGRAY2,
	BNXT_ULP_DEVICE_ID_LAST
};

enum bnxt_ulp_sym {
	BNXT_ULP_SYM_LITTLE_ENDIAN = 1,
	BNXT_ULP_SYM_YES = 1
};

#endif /* _ULP_TEMPLATE_DB_H_ */
