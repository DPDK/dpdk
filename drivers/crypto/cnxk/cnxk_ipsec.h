/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_IPSEC_H__
#define __CNXK_IPSEC_H__

#include <rte_security.h>
#include <rte_security_driver.h>

#include "roc_api.h"

extern struct rte_security_ops cnxk_sec_ops;

struct cnxk_cpt_inst_tmpl {
	uint64_t w2;
	uint64_t w4;
	uint64_t w7;
};

#endif /* __CNXK_IPSEC_H__ */
