/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_SYM_CAPABILITIES_H_
#define _NITROX_SYM_CAPABILITIES_H_

#include <rte_cryptodev.h>

const struct rte_cryptodev_capabilities *nitrox_get_sym_capabilities(void);

#endif /* _NITROX_SYM_CAPABILITIES_H_ */
