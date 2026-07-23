/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_IPSEC_H__
#define __CNXK_IPSEC_H__

#include <rte_security.h>
#include <rte_security_driver.h>

#include "cnxk_security.h"
#include "roc_cpt.h"
#include "roc_ie_on.h"
#include "roc_ie_ot.h"
#include "roc_ie_ow.h"
#include "roc_model.h"

extern struct rte_security_ops cnxk_sec_ops;

struct cnxk_cpt_inst_tmpl {
	uint64_t w2;
	uint64_t w4;
	uint64_t w7;
};

#endif /* __CNXK_IPSEC_H__ */
