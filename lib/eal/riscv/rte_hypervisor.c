/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#include "rte_hypervisor.h"

enum rte_hypervisor
rte_hypervisor_get(void)
{
	return RTE_HYPERVISOR_UNKNOWN;
}
