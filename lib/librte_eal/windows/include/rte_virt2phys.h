/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

/**
 * @file virt2phys driver interface
 */

/**
 * Driver device interface GUID {539c2135-793a-4926-afec-d3a1b61bbc8a}.
 */
DEFINE_GUID(GUID_DEVINTERFACE_VIRT2PHYS,
	0x539c2135, 0x793a, 0x4926,
	0xaf, 0xec, 0xd3, 0xa1, 0xb6, 0x1b, 0xbc, 0x8a);

/**
 * Driver device type for IO control codes.
 */
#define VIRT2PHYS_DEVTYPE 0x8000

/**
 * Translate a valid non-paged virtual address to a physical address.
 *
 * Note: A physical address zero (0) is reported if input address
 * is paged out or not mapped. However, if input is a valid mapping
 * of I/O port 0x0000, output is also zero. There is no way
 * to distinguish between these cases by return value only.
 *
 * Input: a non-paged virtual address (PVOID).
 *
 * Output: the corresponding physical address (LARGE_INTEGER).
 */
#define IOCTL_VIRT2PHYS_TRANSLATE CTL_CODE( \
	VIRT2PHYS_DEVTYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
