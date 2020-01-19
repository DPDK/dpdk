/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_API_H_
#define _IONIC_API_H_

#include "ionic.h"

int32_t ionic_init_mac(struct ionic_hw *hw);
int32_t ionic_set_mac_type(struct ionic_hw *hw);

#endif /* _IONIC_API_H_ */
