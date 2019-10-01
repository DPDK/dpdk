/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_SYM_H_
#define _NITROX_SYM_H_

struct nitrox_device;

int nitrox_sym_pmd_create(struct nitrox_device *ndev);
int nitrox_sym_pmd_destroy(struct nitrox_device *ndev);

#endif /* _NITROX_SYM_H_ */
