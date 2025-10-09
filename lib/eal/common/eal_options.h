/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 6WIND S.A.
 */

#ifndef EAL_OPTIONS_H
#define EAL_OPTIONS_H

#include "getopt.h"

struct rte_tel_data;

int eal_parse_log_options(void);
int eal_parse_args(void);
int eal_option_device_parse(void);
int eal_adjust_config(struct internal_config *internal_cfg);
int eal_cleanup_config(struct internal_config *internal_cfg);
enum rte_proc_type_t eal_proc_type_detect(void);
int eal_plugins_init(void);
int eal_save_args(int argc, char **argv);
void eal_clean_saved_args(void);
int handle_eal_info_request(const char *cmd, const char *params __rte_unused,
		struct rte_tel_data *d);
__rte_internal
int rte_eal_parse_coremask(const char *coremask, rte_cpuset_t *cpuset, bool limit_range);

#endif /* EAL_OPTIONS_H */
