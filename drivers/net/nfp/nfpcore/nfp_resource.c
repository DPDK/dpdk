/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <time.h>
#include <endian.h>

#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp6000/nfp6000.h"
#include "nfp_resource.h"
#include "nfp_crc.h"

#define NFP_RESOURCE_TBL_TARGET		NFP_CPP_TARGET_MU
#define NFP_RESOURCE_TBL_BASE		0x8100000000ULL

/* NFP Resource Table self-identifier */
#define NFP_RESOURCE_TBL_NAME		"nfp.res"
#define NFP_RESOURCE_TBL_KEY		0x00000000 /* Special key for entry 0 */

#define NFP_RESOURCE_ENTRY_NAME_SZ	8

/*
 * struct nfp_resource_entry - Resource table entry
 * @owner:		NFP CPP Lock, interface owner
 * @key:		NFP CPP Lock, posix_crc32(name, 8)
 * @region:		Memory region descriptor
 * @name:		ASCII, zero padded name
 * @reserved
 * @cpp_action:		CPP Action
 * @cpp_token:		CPP Token
 * @cpp_target:		CPP Target ID
 * @page_offset:	256-byte page offset into target's CPP address
 * @page_size:		size, in 256-byte pages
 */
struct nfp_resource_entry {
	struct nfp_resource_entry_mutex {
		uint32_t owner;
		uint32_t key;
	} mutex;
	struct nfp_resource_entry_region {
		uint8_t  name[NFP_RESOURCE_ENTRY_NAME_SZ];
		uint8_t  reserved[5];
		uint8_t  cpp_action;
		uint8_t  cpp_token;
		uint8_t  cpp_target;
		uint32_t page_offset;
		uint32_t page_size;
	} region;
};

#define NFP_RESOURCE_TBL_SIZE		4096
#define NFP_RESOURCE_TBL_ENTRIES	(int)(NFP_RESOURCE_TBL_SIZE /	\
					 sizeof(struct nfp_resource_entry))

struct nfp_resource {
	char name[NFP_RESOURCE_ENTRY_NAME_SZ + 1];
	uint32_t cpp_id;
	uint64_t addr;
	uint64_t size;
	struct nfp_cpp_mutex *mutex;
};

static int
nfp_cpp_resource_find(struct nfp_cpp *cpp, struct nfp_resource *res)
{
	char name_pad[NFP_RESOURCE_ENTRY_NAME_SZ + 2];
	struct nfp_resource_entry entry;
	uint32_t cpp_id, key;
	int ret, i;

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
		res->mutex =
			nfp_cpp_mutex_alloc(cpp,
					    NFP_RESOURCE_TBL_TARGET, addr, key);
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
nfp_resource_try_acquire(struct nfp_cpp *cpp, struct nfp_resource *res,
			 struct nfp_cpp_mutex *dev_mutex)
{
	int err;

	if (nfp_cpp_mutex_lock(dev_mutex))
		return -EINVAL;

	err = nfp_cpp_resource_find(cpp, res);
	if (err)
		goto err_unlock_dev;

	err = nfp_cpp_mutex_trylock(res->mutex);
	if (err)
		goto err_res_mutex_free;

	nfp_cpp_mutex_unlock(dev_mutex);

	return 0;

err_res_mutex_free:
	nfp_cpp_mutex_free(res->mutex);
err_unlock_dev:
	nfp_cpp_mutex_unlock(dev_mutex);

	return err;
}

/*
 * nfp_resource_acquire() - Acquire a resource handle
 * @cpp:	NFP CPP handle
 * @name:	Name of the resource
 *
 * NOTE: This function locks the acquired resource
 *
 * Return: NFP Resource handle, or ERR_PTR()
 */
struct nfp_resource *
nfp_resource_acquire(struct nfp_cpp *cpp, const char *name)
{
	struct nfp_cpp_mutex *dev_mutex;
	struct nfp_resource *res;
	int err;
	struct timespec wait;
	int count;

	res = malloc(sizeof(*res));
	if (res == NULL)
		return NULL;

	memset(res, 0, sizeof(*res));

	strncpy(res->name, name, NFP_RESOURCE_ENTRY_NAME_SZ);

	dev_mutex = nfp_cpp_mutex_alloc(cpp, NFP_RESOURCE_TBL_TARGET,
					NFP_RESOURCE_TBL_BASE,
					NFP_RESOURCE_TBL_KEY);
	if (dev_mutex == NULL) {
		free(res);
		return NULL;
	}

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;
	count = 0;

	for (;;) {
		err = nfp_resource_try_acquire(cpp, res, dev_mutex);
		if (err == 0)
			break;
		if (err != -EBUSY)
			goto err_free;

		if (count++ > 1000) {
			PMD_DRV_LOG(ERR, "Error: resource %s timed out", name);
			err = -EBUSY;
			goto err_free;
		}

		nanosleep(&wait, NULL);
	}

	nfp_cpp_mutex_free(dev_mutex);

	return res;

err_free:
	nfp_cpp_mutex_free(dev_mutex);
	free(res);
	return NULL;
}

/*
 * nfp_resource_release() - Release a NFP Resource handle
 * @res:	NFP Resource handle
 *
 * NOTE: This function implicitly unlocks the resource handle
 */
void
nfp_resource_release(struct nfp_resource *res)
{
	nfp_cpp_mutex_unlock(res->mutex);
	nfp_cpp_mutex_free(res->mutex);
	free(res);
}

/*
 * nfp_resource_cpp_id() - Return the cpp_id of a resource handle
 * @res:        NFP Resource handle
 *
 * Return: NFP CPP ID
 */
uint32_t
nfp_resource_cpp_id(const struct nfp_resource *res)
{
	return res->cpp_id;
}

/*
 * nfp_resource_name() - Return the name of a resource handle
 * @res:        NFP Resource handle
 *
 * Return: const char pointer to the name of the resource
 */
const char
*nfp_resource_name(const struct nfp_resource *res)
{
	return res->name;
}

/*
 * nfp_resource_address() - Return the address of a resource handle
 * @res:        NFP Resource handle
 *
 * Return: Address of the resource
 */
uint64_t
nfp_resource_address(const struct nfp_resource *res)
{
	return res->addr;
}

/*
 * nfp_resource_size() - Return the size in bytes of a resource handle
 * @res:        NFP Resource handle
 *
 * Return: Size of the resource in bytes
 */
uint64_t
nfp_resource_size(const struct nfp_resource *res)
{
	return res->size;
}
