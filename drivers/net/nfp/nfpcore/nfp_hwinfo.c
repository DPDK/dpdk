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

#include <stdio.h>
#include <time.h>

#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp6000/nfp6000.h"
#include "nfp_resource.h"
#include "nfp_hwinfo.h"
#include "nfp_crc.h"

static int
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

	header = (void *)db;
	if (nfp_hwinfo_is_updating(header))
		goto exit_free;

	if (header->version != NFP_HWINFO_VERSION_2) {
		PMD_DRV_LOG(ERR, "Unknown HWInfo version: %#08x",
				header->version);
		goto exit_free;
	}

	/* NULL-terminate for safety */
	db[*cpp_size] = '\0';

	return (void *)db;
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
