/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef __T4_CHIP_TYPE_H__
#define __T4_CHIP_TYPE_H__

/*
 * All T4 and later chips have their PCI-E Device IDs encoded as 0xVFPP where:
 *
 *   V  = "4" for T4; "5" for T5, etc. or
 *   F  = "0" for PF 0..3; "4".."7" for PF4..7; and "8" for VFs
 *   PP = adapter product designation
 *
 * We use the "version" (V) of the adpater to code the Chip Version above.
 */
#define CHELSIO_PCI_ID_VER(devid) ((devid) >> 12)
#define CHELSIO_PCI_ID_FUNC(devid) (((devid) >> 8) & 0xf)
#define CHELSIO_PCI_ID_PROD(devid) ((devid) & 0xff)

#define CHELSIO_T4 0x4
#define CHELSIO_T5 0x5
#define CHELSIO_T6 0x6

#define CHELSIO_CHIP_CODE(version, revision) (((version) << 4) | (revision))
#define CHELSIO_CHIP_VERSION(code) (((code) >> 4) & 0xf)
#define CHELSIO_CHIP_RELEASE(code) ((code) & 0xf)

enum chip_type {
	T4_A1 = CHELSIO_CHIP_CODE(CHELSIO_T4, 1),
	T4_A2 = CHELSIO_CHIP_CODE(CHELSIO_T4, 2),
	T4_FIRST_REV	= T4_A1,
	T4_LAST_REV	= T4_A2,

	T5_A0 = CHELSIO_CHIP_CODE(CHELSIO_T5, 0),
	T5_A1 = CHELSIO_CHIP_CODE(CHELSIO_T5, 1),
	T5_FIRST_REV	= T5_A0,
	T5_LAST_REV	= T5_A1,

	T6_A0 = CHELSIO_CHIP_CODE(CHELSIO_T6, 0),
	T6_FIRST_REV    = T6_A0,
	T6_LAST_REV     = T6_A0,
};

static inline int is_t4(enum chip_type chip)
{
	return (CHELSIO_CHIP_VERSION(chip) == CHELSIO_T4);
}

static inline int is_t5(enum chip_type chip)
{
	return (CHELSIO_CHIP_VERSION(chip) == CHELSIO_T5);
}

static inline int is_t6(enum chip_type chip)
{
	return (CHELSIO_CHIP_VERSION(chip) == CHELSIO_T6);
}
#endif /* __T4_CHIP_TYPE_H__ */
