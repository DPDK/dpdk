/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_API_H_
#define _IONIC_API_H_

#include "ionic.h"

int32_t ionic_init_mac(struct ionic_hw *hw);
int32_t ionic_set_mac_type(struct ionic_hw *hw);

#endif /* _IONIC_API_H_ */
