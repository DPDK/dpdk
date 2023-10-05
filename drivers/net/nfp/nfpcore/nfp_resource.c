/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_resource.h"

#include "nfp_crc.h"
#include "nfp_logs.h"
#include "nfp_mutex.h"
#include "nfp_target.h"

#define NFP_RESOURCE_TBL_TARGET         NFP_CPP_TARGET_MU
#define NFP_RESOURCE_TBL_BASE           0x8100000000ULL

/* NFP Resource Table self-identifier */
#define NFP_RESOURCE_TBL_NAME           "nfp.res"
#define NFP_RESOURCE_TBL_KEY            0x00000000 /* Special key for entry 0 */

#define NFP_RESOURCE_ENTRY_NAME_SZ      8

/* Resource table entry */
struct nfp_resource_entry {
	struct nfp_resource_entry_mutex {
		uint32_t owner;  /**< NFP CPP Lock, interface owner */
		uint32_t key;    /**< NFP CPP Lock, posix_crc32(name, 8) */
	} mutex;
	/* Memory region descriptor */
	struct nfp_resource_entry_region {
		/** ASCII, zero padded name */
		uint8_t  name[NFP_RESOURCE_ENTRY_NAME_SZ];
		uint8_t  reserved[5];
		uint8_t  cpp_action;  /**< CPP Action */
		uint8_t  cpp_token;   /**< CPP Token */
		uint8_t  cpp_target;  /**< CPP Target ID */
		/** 256-byte page offset into target's CPP address */
		uint32_t page_offset;
		uint32_t page_size;   /**< Size, in 256-byte pages */
	} region;
};

#define NFP_RESOURCE_TBL_SIZE       4096
#define NFP_RESOURCE_TBL_ENTRIES    (NFP_RESOURCE_TBL_SIZE /        \
					sizeof(struct nfp_resource_entry))

struct nfp_resource {
	char name[NFP_RESOURCE_ENTRY_NAME_SZ + 1];
	uint32_t cpp_id;
	uint64_t addr;
	uint64_t size;
	struct nfp_cpp_mutex *mutex;
};

static int
nfp_cpp_resource_find(struct nfp_cpp *cpp,
		struct nfp_resource *res)
{
	int ret;
	uint32_t i;
	uint32_t key;
	uint32_t cpp_id;
	struct nfp_resource_entry entry;
	char name_pad[NFP_RESOURCE_ENTRY_NAME_SZ + 2];

	cpp_id = NFP_CPP_ID(NFP_RESOURCE_TBL_TARGET, 3, 0);  /* Atomic read */

	memset(name_pad, 0, sizeof(name_pad));
	strlcpy(name_pad, res->name, sizeof(name_pad));

	/* Search for a matching entry */
	if (memcmp(name_pad, NFP_RESOURCE_TBL_NAME "\0\0\0\0\0\0\0\0", 8) == 0) {
		PMD_DRV_LOG(ERR, "Grabbing device lock not supported");
		return -EOPNOTSUPP;
	}

	key = nfp_crc32_posix(name_pad, NFP_RESOURCE_ENTRY_NAME_SZ);

	for (i = 0; i < NFP_RESOURCE_TBL_ENTRIES; i++) {
		uint64_t addr = NFP_RESOURCE_TBL_BASE +
				sizeof(struct nfp_resource_entry) * i;

		ret = nfp_cpp_read(cpp, cpp_id, addr, &entry, sizeof(entry));
		if (ret != sizeof(entry))
			return -EIO;

		if (entry.mutex.key != key)
			continue;

		/* Found key! */
		res->mutex = nfp_cpp_mutex_alloc(cpp, NFP_RESOURCE_TBL_TARGET,
				addr, key);
		res->cpp_id = NFP_CPP_ID(entry.region.cpp_target,
				entry.region.cpp_action,
				entry.region.cpp_token);
		res->addr = ((uint64_t)entry.region.page_offset) << 8;
		res->size = (uint64_t)entry.region.page_size << 8;

		return 0;
	}

	return -ENOENT;
}

static int
nfp_resource_try_acquire(struct nfp_cpp *cpp,
		struct nfp_resource *res,
		struct nfp_cpp_mutex *dev_mutex)
{
	int err;

	if (nfp_cpp_mutex_lock(dev_mutex) != 0) {
		PMD_DRV_LOG(ERR, "RESOURCE - CPP mutex lock failed");
		return -EINVAL;
	}

	err = nfp_cpp_resource_find(cpp, res);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "RESOURCE - CPP resource find failed");
		goto err_unlock_dev;
	}

	err = nfp_cpp_mutex_trylock(res->mutex);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "RESOURCE - CPP mutex trylock failed");
		goto err_res_mutex_free;
	}

	nfp_cpp_mutex_unlock(dev_mutex);

	return 0;

err_res_mutex_free:
	nfp_cpp_mutex_free(res->mutex);
err_unlock_dev:
	nfp_cpp_mutex_unlock(dev_mutex);

	return err;
}

/**
 * Acquire a resource handle
 *
 * Note: This function locks the acquired resource.
 *
 * @param cpp
 *   NFP CPP handle
 * @param name
 *   Name of the resource
 *
 * @return
 *   NFP Resource handle, or NULL
 */
struct nfp_resource *
nfp_resource_acquire(struct nfp_cpp *cpp,
		const char *name)
{
	int err;
	uint16_t count = 0;
	struct timespec wait;
	struct nfp_resource *res;
	struct nfp_cpp_mutex *dev_mutex;

	res = malloc(sizeof(*res));
	if (res == NULL)
		return NULL;

	memset(res, 0, sizeof(*res));

	strncpy(res->name, name, NFP_RESOURCE_ENTRY_NAME_SZ);

	dev_mutex = nfp_cpp_mutex_alloc(cpp, NFP_RESOURCE_TBL_TARGET,
			NFP_RESOURCE_TBL_BASE, NFP_RESOURCE_TBL_KEY);
	if (dev_mutex == NULL) {
		PMD_DRV_LOG(ERR, "RESOURCE - CPP mutex alloc failed");
		goto err_free;
	}

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;    /* 1ms */

	for (;;) {
		err = nfp_resource_try_acquire(cpp, res, dev_mutex);
		if (err == 0)
			break;
		if (err != -EBUSY) {
			PMD_DRV_LOG(ERR, "RESOURCE - try acquire failed");
			goto mutex_free;
		}

		if (count++ > 1000) {    /* 1ms * 1000 = 1s */
			PMD_DRV_LOG(ERR, "Error: resource %s timed out", name);
			goto mutex_free;
		}

		nanosleep(&wait, NULL);
	}

	nfp_cpp_mutex_free(dev_mutex);

	return res;

mutex_free:
	nfp_cpp_mutex_free(dev_mutex);
err_free:
	free(res);
	return NULL;
}

/**
 * Release a NFP Resource handle
 *
 * NOTE: This function implicitly unlocks the resource handle.
 *
 * @param res
 *   NFP Resource handle
 */
void
nfp_resource_release(struct nfp_resource *res)
{
	nfp_cpp_mutex_unlock(res->mutex);
	nfp_cpp_mutex_free(res->mutex);
	free(res);
}

/**
 * Return the cpp_id of a resource handle
 *
 * @param res
 *   NFP Resource handle
 *
 * @return
 *   NFP CPP ID
 */
uint32_t
nfp_resource_cpp_id(const struct nfp_resource *res)
{
	return res->cpp_id;
}

/**
 * Return the name of a resource handle
 *
 * @param res
 *   NFP Resource handle
 *
 * @return
 *   Const char pointer to the name of the resource
 */
const char *
nfp_resource_name(const struct nfp_resource *res)
{
	return res->name;
}

/**
 * Return the address of a resource handle
 *
 * @param res
 *   NFP Resource handle
 *
 * @return
 *   Address of the resource
 */
uint64_t
nfp_resource_address(const struct nfp_resource *res)
{
	return res->addr;
}

/**
 * Return the size in bytes of a resource handle
 *
 * @param res
 *   NFP Resource handle
 *
 * @return
 *   Size of the resource in bytes
 */
uint64_t
nfp_resource_size(const struct nfp_resource *res)
{
	return res->size;
}
