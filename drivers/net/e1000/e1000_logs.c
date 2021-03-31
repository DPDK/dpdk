/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ethdev.h>
#include "e1000_logs.h"

RTE_LOG_REGISTER(e1000_logtype_init, pmd.net.e1000.init, NOTICE)
RTE_LOG_REGISTER(e1000_logtype_driver, pmd.net.e1000.driver, NOTICE)
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER(e1000_logtype_rx, pmd.net.e1000.rx, DEBUG)
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER(e1000_logtype_tx, pmd.net.e1000.tx, DEBUG)
#endif
