/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ethdev.h>
#include "e1000_logs.h"

RTE_LOG_REGISTER_SUFFIX(e1000_logtype_init, init, NOTICE)
RTE_LOG_REGISTER_SUFFIX(e1000_logtype_driver, driver, NOTICE)
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(e1000_logtype_rx, rx, DEBUG)
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(e1000_logtype_tx, tx, DEBUG)
#endif
