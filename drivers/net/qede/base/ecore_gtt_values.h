/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __PREVENT_PXP_GLOBAL_WIN__

static u32 pxp_global_win[] = {
	0,
	0,
	0x1c02, /* win 2: addr=0x1c02000, size=4096 bytes */
	0x1c80, /* win 3: addr=0x1c80000, size=4096 bytes */
	0x1d00, /* win 4: addr=0x1d00000, size=4096 bytes */
	0x1d01, /* win 5: addr=0x1d01000, size=4096 bytes */
	0x1d80, /* win 6: addr=0x1d80000, size=4096 bytes */
	0x1d81, /* win 7: addr=0x1d81000, size=4096 bytes */
	0x1d82, /* win 8: addr=0x1d82000, size=4096 bytes */
	0x1e00, /* win 9: addr=0x1e00000, size=4096 bytes */
	0x1e80, /* win 10: addr=0x1e80000, size=4096 bytes */
	0x1f00, /* win 11: addr=0x1f00000, size=4096 bytes */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

#endif /* __PREVENT_PXP_GLOBAL_WIN__ */
