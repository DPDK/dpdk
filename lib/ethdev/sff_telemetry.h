/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _ETHDEV_SFF_TELEMETRY_H_
#define _ETHDEV_SFF_TELEMETRY_H_

#include <rte_telemetry.h>

#define SFF_ITEM_VAL_COMPOSE_SIZE 64

/* SFF-8079 Optics diagnostics */
void sff_8079_show_all(const uint8_t *data, struct rte_tel_data *d);

/* SFF-8472 Optics diagnostics */
void sff_8472_show_all(const uint8_t *data, struct rte_tel_data *d);

/* SFF-8636 Optics diagnostics */
void sff_8636_show_all(const uint8_t *data, uint32_t eeprom_len, struct rte_tel_data *d);

int eth_dev_handle_port_module_eeprom(const char *cmd __rte_unused,
				      const char *params,
				      struct rte_tel_data *d);

void ssf_add_dict_string(struct rte_tel_data *d, const char *name_str,
			 const char *value_str);

#endif /* _ETHDEV_SFF_TELEMETRY_H_ */
