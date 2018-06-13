/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_SYM_H_
#define _QAT_SYM_H_

#include <rte_cryptodev_pmd.h>

#include "qat_common.h"

/*
 * This macro rounds up a number to a be a multiple of
 * the alignment when the alignment is a power of 2
 */
#define ALIGN_POW2_ROUNDUP(num, align) \
	(((num) + (align) - 1) & ~((align) - 1))
#define QAT_64_BTYE_ALIGN_MASK (~0x3f)

struct qat_sym_session;

struct qat_sym_op_cookie {
	struct qat_sgl qat_sgl_src;
	struct qat_sgl qat_sgl_dst;
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
};

int
qat_sym_build_request(void *in_op, uint8_t *out_msg,
		void *op_cookie, enum qat_device_gen qat_dev_gen);
int
qat_sym_process_response(void **op, uint8_t *resp,
		__rte_unused void *op_cookie,
		__rte_unused enum qat_device_gen qat_dev_gen);

#endif /* _QAT_SYM_H_ */
