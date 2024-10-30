/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_HW_DB_INLINE_H_
#define _FLOW_API_HW_DB_INLINE_H_

#include <stdint.h>

#include "flow_api.h"

struct hw_db_inline_cot_data {
	uint32_t matcher_color_contrib : 4;
	uint32_t frag_rcp : 4;
	uint32_t padding : 24;
};

/**/

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle);
void hw_db_inline_destroy(void *db_handle);

#endif	/* _FLOW_API_HW_DB_INLINE_H_ */
