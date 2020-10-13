/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_REE_HW_H__
#define __OTX2_REE_HW_H__

/* REE BAR0*/
#define REE_AF_REEXM_MAX_MATCH		(0x80c8)

/* REE BAR02 */
#define REE_LF_MISC_INT                 (0x300)
#define REE_LF_DONE_INT                 (0x120)

#define REE_AF_QUEX_GMCTL(a)            (0x800 | (a) << 3)

#define REE_AF_INT_VEC_RAS          (0x0ull)
#define REE_AF_INT_VEC_RVU          (0x1ull)
#define REE_AF_INT_VEC_QUE_DONE     (0x2ull)
#define REE_AF_INT_VEC_AQ           (0x3ull)

/* ENUMS */

#define REE_LF_INT_VEC_QUE_DONE	(0x0ull)
#define REE_LF_INT_VEC_MISC		(0x1ull)

#endif /* __OTX2_REE_HW_H__*/
