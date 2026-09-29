/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _ETHDEV_SFF_TELEMETRY_H_
#define _ETHDEV_SFF_TELEMETRY_H_

#include <rte_telemetry.h>

int eth_dev_handle_port_module_eeprom(const char *cmd __rte_unused,
				      const char *params,
				      struct rte_tel_data *d);

#endif /* _ETHDEV_SFF_TELEMETRY_H_ */
