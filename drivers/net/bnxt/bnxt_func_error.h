/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_FUNC_ERROR_H_
#define _BNXT_FUNC_ERROR_H_

#include "bnxt.h"

#define BNXT_DEVARG_FUNC_ERROR_SUPPORT "func-error-support"
#define BNXT_DEVARG_FUNC_ERROR_SUPPORT_INVALID(val)	((val) > 1)

struct bnxt_func_error_info {
	uint8_t __rte_atomic support;
	unsigned int __rte_atomic recovery_pending;
	uint64_t __rte_atomic recovery_count;
	uint64_t __rte_atomic oerrors;
	unsigned int __rte_atomic poll_scheduled;
};

int bnxt_func_error_info_alloc(struct bnxt *bp);
void bnxt_func_error_info_free(struct bnxt *bp);

void bnxt_func_error_trigger_recovery(struct bnxt *bp);
void bnxt_func_error_recovery_cb(void *arg);
void bnxt_func_error_clear_pending(struct bnxt *bp);
void bnxt_schedule_func_error_poll(struct bnxt *bp);
void bnxt_cancel_func_error_poll(struct bnxt *bp);
int bnxt_parse_devarg_func_error_support(const char *key, const char *value,
					  void *opaque_arg);

#endif /* _BNXT_FUNC_ERROR_H_ */
