/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BITFIELD_H_
#define _ROC_BITFIELD_H_

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define FIELD_PREP(mask, val) (((typeof(mask))(val) << __bf_shf(mask)) & (mask))

#define FIELD_GET(mask, reg)                                                   \
	((typeof(mask))(((reg) & (mask)) >> __bf_shf(mask)))

#endif /* _ROC_BITFIELD_H_ */
