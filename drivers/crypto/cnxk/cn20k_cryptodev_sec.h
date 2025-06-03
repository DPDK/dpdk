/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef __CN20K_CRYPTODEV_SEC_H__
#define __CN20K_CRYPTODEV_SEC_H__

#include <rte_common.h>
#include <rte_security.h>

#include "roc_constants.h"
#include "roc_cpt.h"

#include "cn20k_ipsec.h"

#define SEC_SESS_SIZE sizeof(struct rte_security_session)

void cn20k_sec_ops_override(void);
#endif /* __CN20K_CRYPTODEV_SEC_H__ */
