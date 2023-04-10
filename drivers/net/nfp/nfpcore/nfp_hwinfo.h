/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_HWINFO_H__
#define __NFP_HWINFO_H__

#include <inttypes.h>

#define HWINFO_SIZE_MIN	0x100

/*
 * The Hardware Info Table defines the properties of the system.
 *
 * HWInfo v1 Table (fixed size)
 *
 * 0x0000: uint32_t version	        Hardware Info Table version (1.0)
 * 0x0004: uint32_t size	        Total size of the table, including the
 *					CRC32 (IEEE 802.3)
 * 0x0008: uint32_t jumptab	        Offset of key/value table
 * 0x000c: uint32_t keys	        Total number of keys in the key/value
 *					table
 * NNNNNN:				Key/value jump table and string data
 * (size - 4): uint32_t crc32	CRC32 (same as IEEE 802.3, POSIX csum, etc)
 *				CRC32("",0) = ~0, CRC32("a",1) = 0x48C279FE
 *
 * HWInfo v2 Table (variable size)
 *
 * 0x0000: uint32_t version	        Hardware Info Table version (2.0)
 * 0x0004: uint32_t size	        Current size of the data area, excluding
 *					CRC32
 * 0x0008: uint32_t limit	        Maximum size of the table
 * 0x000c: uint32_t reserved	        Unused, set to zero
 * NNNNNN:			Key/value data
 * (size - 4): uint32_t crc32	CRC32 (same as IEEE 802.3, POSIX csum, etc)
 *				CRC32("",0) = ~0, CRC32("a",1) = 0x48C279FE
 *
 * If the HWInfo table is in the process of being updated, the low bit of
 * version will be set.
 *
 * HWInfo v1 Key/Value Table
 * -------------------------
 *
 *  The key/value table is a set of offsets to ASCIIZ strings which have
 *  been strcmp(3) sorted (yes, please use bsearch(3) on the table).
 *
 *  All keys are guaranteed to be unique.
 *
 * N+0:	uint32_t key_1		Offset to the first key
 * N+4:	uint32_t val_1		Offset to the first value
 * N+8: uint32_t key_2		Offset to the second key
 * N+c: uint32_t val_2		Offset to the second value
 * ...
 *
 * HWInfo v2 Key/Value Table
 * -------------------------
 *
 * Packed UTF8Z strings, ie 'key1\000value1\000key2\000value2\000'
 *
 * Unsorted.
 */

#define NFP_HWINFO_VERSION_1 ('H' << 24 | 'I' << 16 | 1 << 8 | 0 << 1 | 0)
#define NFP_HWINFO_VERSION_2 ('H' << 24 | 'I' << 16 | 2 << 8 | 0 << 1 | 0)
#define NFP_HWINFO_VERSION_UPDATING	RTE_BIT32(0)

struct nfp_hwinfo {
	uint8_t start[0];

	uint32_t version;
	uint32_t size;

	/* v2 specific fields */
	uint32_t limit;
	uint32_t resv;

	char data[];
};

struct nfp_hwinfo *nfp_hwinfo_read(struct nfp_cpp *cpp);

const char *nfp_hwinfo_lookup(struct nfp_hwinfo *hwinfo, const char *lookup);

#endif
