/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * Parse the hwinfo table that the ARM firmware builds in the ARM scratch SRAM
 * after chip reset.
 *
 * Examples of the fields:
 *   me.count = 40
 *   me.mask = 0x7f_ffff_ffff
 *
 *   me.count is the total number of MEs on the system.
 *   me.mask is the bitmask of MEs that are available for application usage.
 *
 *   (ie, in this example, ME 39 has been reserved by boardconfig.)
 */

#include "nfp_hwinfo.h"

#include "nfp_crc.h"
#include "nfp_logs.h"
#include "nfp_resource.h"

#define HWINFO_SIZE_MIN    0x100

/*
 * The Hardware Info Table defines the properties of the system.
 *
 * HWInfo v1 Table (fixed size)
 *
 * 0x0000: uint32_t version        Hardware Info Table version (1.0)
 * 0x0004: uint32_t size           Total size of the table, including the
 *                                     CRC32 (IEEE 802.3)
 * 0x0008: uint32_t jumptab        Offset of key/value table
 * 0x000c: uint32_t keys           Total number of keys in the key/value table
 * NNNNNN:                         Key/value jump table and string data
 * (size - 4): uint32_t crc32      CRC32 (same as IEEE 802.3, POSIX csum, etc)
 *                                     CRC32("",0) = ~0, CRC32("a",1) = 0x48C279FE
 *
 * HWInfo v2 Table (variable size)
 *
 * 0x0000: uint32_t version        Hardware Info Table version (2.0)
 * 0x0004: uint32_t size           Current size of the data area, excluding CRC32
 * 0x0008: uint32_t limit          Maximum size of the table
 * 0x000c: uint32_t reserved       Unused, set to zero
 * NNNNNN:                         Key/value data
 * (size - 4): uint32_t crc32      CRC32 (same as IEEE 802.3, POSIX csum, etc)
 *                                     CRC32("",0) = ~0, CRC32("a",1) = 0x48C279FE
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
 * N+0: uint32_t key_1        Offset to the first key
 * N+4: uint32_t val_1        Offset to the first value
 * N+8: uint32_t key_2        Offset to the second key
 * N+c: uint32_t val_2        Offset to the second value
 * ...
 *
 * HWInfo v2 Key/Value Table
 * -------------------------
 *
 * Packed UTF8Z strings, ie 'key1\000value1\000key2\000value2\000'
 * Unsorted.
 *
 * Note: Only the HwInfo v2 Table be supported now.
 */

#define NFP_HWINFO_VERSION_1 ('H' << 24 | 'I' << 16 | 1 << 8 | 0 << 1 | 0)
#define NFP_HWINFO_VERSION_2 ('H' << 24 | 'I' << 16 | 2 << 8 | 0 << 1 | 0)
#define NFP_HWINFO_VERSION_UPDATING    RTE_BIT32(0)

struct nfp_hwinfo {
	uint8_t start[0];

	uint32_t version;
	uint32_t size;

	/* V2 specific fields */
	uint32_t limit;
	uint32_t resv;

	char data[];
};

static bool
nfp_hwinfo_is_updating(struct nfp_hwinfo *hwinfo)
{
	return hwinfo->version & NFP_HWINFO_VERSION_UPDATING;
}

static int
nfp_hwinfo_db_walk(struct nfp_hwinfo *hwinfo,
		uint32_t size)
{
	const char *key;
	const char *val;
	const char *end = hwinfo->data + size;

	for (key = hwinfo->data; *key != 0 && key < end;
			key = val + strlen(val) + 1) {
		val = key + strlen(key) + 1;
		if (val >= end) {
			PMD_DRV_LOG(ERR, "Bad HWINFO - overflowing value");
			return -EINVAL;
		}

		if (val + strlen(val) + 1 > end) {
			PMD_DRV_LOG(ERR, "Bad HWINFO - overflowing value");
			return -EINVAL;
		}
	}

	return 0;
}

static int
nfp_hwinfo_db_validate(struct nfp_hwinfo *db,
		uint32_t len)
{
	uint32_t *crc;
	uint32_t size;
	uint32_t new_crc;

	size = db->size;
	if (size > len) {
		PMD_DRV_LOG(ERR, "Unsupported hwinfo size %u > %u", size, len);
		return -EINVAL;
	}

	size -= sizeof(uint32_t);
	new_crc = nfp_crc32_posix((char *)db, size);
	crc = (uint32_t *)(db->start + size);
	if (new_crc != *crc) {
		PMD_DRV_LOG(ERR, "CRC mismatch, calculated %#x, expected %#x",
				new_crc, *crc);
		return -EINVAL;
	}

	return nfp_hwinfo_db_walk(db, size);
}

static struct nfp_hwinfo *
nfp_hwinfo_try_fetch(struct nfp_cpp *cpp,
		size_t *cpp_size)
{
	int err;
	void *res;
	uint8_t *db;
	uint32_t cpp_id;
	uint64_t cpp_addr;
	struct nfp_hwinfo *header;

	res = nfp_resource_acquire(cpp, NFP_RESOURCE_NFP_HWINFO);
	if (res == NULL) {
		PMD_DRV_LOG(ERR, "HWInfo - acquire resource failed");
		return NULL;
	}

	cpp_id = nfp_resource_cpp_id(res);
	cpp_addr = nfp_resource_address(res);
	*cpp_size = nfp_resource_size(res);

	nfp_resource_release(res);

	if (*cpp_size < HWINFO_SIZE_MIN)
		return NULL;

	db = malloc(*cpp_size + 1);
	if (db == NULL)
		return NULL;

	err = nfp_cpp_read(cpp, cpp_id, cpp_addr, db, *cpp_size);
	if (err != (int)*cpp_size) {
		PMD_DRV_LOG(ERR, "HWInfo - CPP read error %d", err);
		goto exit_free;
	}

	header = (struct nfp_hwinfo *)db;
	if (nfp_hwinfo_is_updating(header))
		goto exit_free;

	if (header->version != NFP_HWINFO_VERSION_2) {
		PMD_DRV_LOG(ERR, "Unknown HWInfo version: %#08x",
				header->version);
		goto exit_free;
	}

	/* NULL-terminate for safety */
	db[*cpp_size] = '\0';

	return (struct nfp_hwinfo *)db;

exit_free:
	free(db);
	return NULL;
}

static struct nfp_hwinfo *
nfp_hwinfo_fetch(struct nfp_cpp *cpp,
		size_t *hwdb_size)
{
	int count = 0;
	struct timespec wait;
	struct nfp_hwinfo *db;

	wait.tv_sec = 0;
	wait.tv_nsec = 10000000;    /* 10ms */

	for (;;) {
		db = nfp_hwinfo_try_fetch(cpp, hwdb_size);
		if (db != NULL)
			return db;

		nanosleep(&wait, NULL);
		if (count++ > 200) {    /* 10ms * 200 = 2s */
			PMD_DRV_LOG(ERR, "NFP access error");
			return NULL;
		}
	}
}

struct nfp_hwinfo *
nfp_hwinfo_read(struct nfp_cpp *cpp)
{
	int err;
	size_t hwdb_size = 0;
	struct nfp_hwinfo *db;

	db = nfp_hwinfo_fetch(cpp, &hwdb_size);
	if (db == NULL)
		return NULL;

	err = nfp_hwinfo_db_validate(db, hwdb_size);
	if (err != 0) {
		free(db);
		return NULL;
	}

	return db;
}

/**
 * Find a value in the HWInfo table by name
 *
 * @param hwinfo
 *   NFP HWInfo table
 * @param lookup
 *   HWInfo name to search for
 *
 * @return
 *   Value of the HWInfo name, or NULL
 */
const char *
nfp_hwinfo_lookup(struct nfp_hwinfo *hwinfo,
		const char *lookup)
{
	const char *key;
	const char *val;
	const char *end;

	if (hwinfo == NULL || lookup == NULL)
		return NULL;

	end = hwinfo->data + hwinfo->size - sizeof(uint32_t);

	for (key = hwinfo->data; *key != 0 && key < end;
			key = val + strlen(val) + 1) {
		val = key + strlen(key) + 1;

		if (strcmp(key, lookup) == 0)
			return val;
	}

	return NULL;
}
