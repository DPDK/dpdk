/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#include <eal_export.h>
#include "rte_hypervisor.h"

RTE_EXPORT_SYMBOL(rte_hypervisor_get)
enum rte_hypervisor
rte_hypervisor_get(void)
{
	return RTE_HYPERVISOR_UNKNOWN;
}
