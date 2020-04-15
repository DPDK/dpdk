/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_RM_H_
#define TF_RM_H_

#include "tf_resources.h"
#include "tf_core.h"

struct tf;
struct tf_session;

/**
 * Resource query single entry
 */
struct tf_rm_query_entry {
	/** Minimum guaranteed number of elements */
	uint16_t min;
	/** Maximum non-guaranteed number of elements */
	uint16_t max;
};

/**
 * Resource query array of HW entities
 */
struct tf_rm_hw_query {
	/** array of HW resource entries */
	struct tf_rm_query_entry hw_query[TF_RESC_TYPE_HW_MAX];
};

#endif /* TF_RM_H_ */
