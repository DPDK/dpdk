/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_COMMON_H_
#define _IFPGA_COMMON_H_

int rte_ifpga_get_string_arg(const char *key __rte_unused,
	const char *value, void *extra_args);
int rte_ifpga_get_integer32_arg(const char *key __rte_unused,
	const char *value, void *extra_args);
int ifpga_get_integer64_arg(const char *key __rte_unused,
	const char *value, void *extra_args);
int ifpga_get_unsigned_long(const char *str, int base);
int ifpga_afu_id_cmp(const struct rte_afu_id *afu_id0,
	const struct rte_afu_id *afu_id1);

#endif /* _IFPGA_COMMON_H_ */
