/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_logs.h"

#include <rte_ethdev.h>

RTE_LOG_REGISTER_SUFFIX(nfp_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_driver, driver, NOTICE);
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_cpp, cpp, NOTICE);

#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_rx, rx, DEBUG)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_tx, tx, DEBUG)
#endif
