/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntnic_mod_reg.h"

static const struct adapter_ops *adapter_ops;

void register_adapter_ops(const struct adapter_ops *ops)
{
	adapter_ops = ops;
}

const struct adapter_ops *get_adapter_ops(void)
{
	if (adapter_ops == NULL)
		adapter_init();
	return adapter_ops;
}
