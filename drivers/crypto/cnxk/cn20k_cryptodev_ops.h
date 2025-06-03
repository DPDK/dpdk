/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef _CN20K_CRYPTODEV_OPS_H_
#define _CN20K_CRYPTODEV_OPS_H_

#include <cryptodev_pmd.h>
#include <rte_compat.h>
#include <rte_cryptodev.h>
#include <rte_eventdev.h>

#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

#include "cnxk_cryptodev.h"

extern struct rte_cryptodev_ops cn20k_cpt_ops;

#endif /* _CN20K_CRYPTODEV_OPS_H_ */
