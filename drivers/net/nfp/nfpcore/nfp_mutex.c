/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_mutex.h"

#include <sched.h>

#include "nfp_logs.h"
#include "nfp_target.h"

/*
 * If you need more than 65536 recursive locks, please
 * rethink your code.
 */
#define MUTEX_DEPTH_MAX         0xffff

struct nfp_cpp_mutex {
	struct nfp_cpp *cpp;
	uint8_t target;
	uint16_t depth;
	uint64_t address;
	uint32_t key;
	uint32_t usage;
	struct nfp_cpp_mutex *prev, *next;
};

static inline uint32_t
nfp_mutex_locked(uint16_t interface)
{
	return (uint32_t)interface << 16 | 0x000f;
}

static inline uint32_t
nfp_mutex_unlocked(uint16_t interface)
{
	return (uint32_t)interface << 16 | 0x0000;
}

static inline uint16_t
nfp_mutex_owner(uint32_t val)
{
	return (val >> 16) & 0xffff;
}

static inline bool
nfp_mutex_is_locked(uint32_t val)
{
	return (val & 0xffff) == 0x000f;
}

static inline bool
nfp_mutex_is_unlocked(uint32_t val)
{
	return (val & 0xffff) == 0;
}

static int
nfp_cpp_mutex_validate(uint16_t interface,
		int *target,
		uint64_t address)
{
	/* Not permitted on invalid interfaces */
	if (NFP_CPP_INTERFACE_TYPE_of(interface) == NFP_CPP_INTERFACE_TYPE_INVALID)
		return -EINVAL;

	/* Address must be 64-bit aligned */
	if ((address & 7) != 0)
		return -EINVAL;

	if (*target != NFP_CPP_TARGET_MU)
		return -EINVAL;

	return 0;
}

/**
 * Initialize a mutex location
 *
 * The CPP target:address must point to a 64-bit aligned location, and
 * will initialize 64 bits of data at the location.
 *
 * This creates the initial mutex state, as locked by this
 * nfp_cpp_interface().
 *
 * This function should only be called when setting up
 * the initial lock state upon boot-up of the system.
 *
 * @param cpp
 *   NFP CPP handle
 * @param target
 *   NFP CPP target ID (ie NFP_CPP_TARGET_CLS or NFP_CPP_TARGET_MU)
 * @param address
 *   Offset into the address space of the NFP CPP target ID
 * @param key
 *   Unique 32-bit value for this mutex
 *
 * @return
 *   0 on success, or negative value on failure
 */
int
nfp_cpp_mutex_init(struct nfp_cpp *cpp,
		int target,
		uint64_t address,
		uint32_t key)
{
	int err;
	uint32_t muw = NFP_CPP_ID(target, 4, 0);    /* atomic_write */
	uint16_t interface = nfp_cpp_interface(cpp);

	err = nfp_cpp_mutex_validate(interface, &target, address);
	if (err < 0)
		return err;

	err = nfp_cpp_writel(cpp, muw, address + 4, key);
	if (err < 0)
		return err;

	err = nfp_cpp_writel(cpp, muw, address, nfp_mutex_locked(interface));
	if (err < 0)
		return err;

	return 0;
}

/**
 * Create a mutex handle from an address controlled by a MU Atomic engine
 *
 * The CPP target:address must point to a 64-bit aligned location, and
 * reserve 64 bits of data at the location for use by the handle.
 *
 * Only target/address pairs that point to entities that support the
 * MU Atomic Engine are supported.
 *
 * @param cpp
 *   NFP CPP handle
 * @param target
 *   NFP CPP target ID (ie NFP_CPP_TARGET_CLS or NFP_CPP_TARGET_MU)
 * @param address
 *   Offset into the address space of the NFP CPP target ID
 * @param key
 *   32-bit unique key (must match the key at this location)
 *
 * @return
 *   A non-NULL struct nfp_cpp_mutex * on success, NULL on failure.
 */
struct nfp_cpp_mutex *
nfp_cpp_mutex_alloc(struct nfp_cpp *cpp,
		int target,
		uint64_t address,
		uint32_t key)
{
	int err;
	uint32_t tmp;
	struct nfp_cpp_mutex *mutex;
	uint32_t mur = NFP_CPP_ID(target, 3, 0);    /* atomic_read */
	uint16_t interface = nfp_cpp_interface(cpp);

	err = nfp_cpp_mutex_validate(interface, &target, address);
	if (err < 0)
		return NULL;

	err = nfp_cpp_readl(cpp, mur, address + 4, &tmp);
	if (err < 0)
		return NULL;

	if (tmp != key)
		return NULL;

	mutex = calloc(1, sizeof(*mutex));
	if (mutex == NULL)
		return NULL;

	mutex->cpp = cpp;
	mutex->target = target;
	mutex->address = address;
	mutex->key = key;
	mutex->depth = 0;

	return mutex;
}

/**
 * Free a mutex handle - does not alter the lock state
 *
 * @param mutex
 *   NFP CPP Mutex handle
 */
void
nfp_cpp_mutex_free(struct nfp_cpp_mutex *mutex)
{
	free(mutex);
}

/**
 * Lock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex
 *   NFP CPP Mutex handle
 *
 * @return
 *   0 on success, or negative value on failure.
 */
int
nfp_cpp_mutex_lock(struct nfp_cpp_mutex *mutex)
{
	int err;
	time_t warn_at = time(NULL) + 15;

	while ((err = nfp_cpp_mutex_trylock(mutex)) != 0) {
		/* If err != -EBUSY, then the lock was damaged */
		if (err < 0 && err != -EBUSY)
			return err;

		if (time(NULL) >= warn_at) {
			PMD_DRV_LOG(WARNING, "Waiting for NFP mutex...");
			warn_at = time(NULL) + 60;
		}

		sched_yield();
	}

	return 0;
}

/**
 * Unlock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex
 *   NFP CPP Mutex handle
 *
 * @return
 *   0 on success, or negative value on failure
 */
int
nfp_cpp_mutex_unlock(struct nfp_cpp_mutex *mutex)
{
	int err;
	uint32_t key;
	uint32_t value;
	struct nfp_cpp *cpp = mutex->cpp;
	uint16_t interface = nfp_cpp_interface(cpp);
	uint32_t muw = NFP_CPP_ID(mutex->target, 4, 0);    /* atomic_write */
	uint32_t mur = NFP_CPP_ID(mutex->target, 3, 0);    /* atomic_read */

	if (mutex->depth > 1) {
		mutex->depth--;
		return 0;
	}

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address + 4, &key);
	if (err < 0)
		return err;

	if (key != mutex->key)
		return -EPERM;

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address, &value);
	if (err < 0)
		return err;

	if (value != nfp_mutex_locked(interface))
		return -EACCES;

	err = nfp_cpp_writel(cpp, muw, mutex->address,
			nfp_mutex_unlocked(interface));
	if (err < 0)
		return err;

	mutex->depth = 0;

	return 0;
}

/**
 * Attempt to lock a mutex handle, using the NFP MU Atomic Engine
 *
 * Valid lock states:
 *      0x....0000      - Unlocked
 *      0x....000f      - Locked
 *
 * @param mutex
 *   NFP CPP Mutex handle
 *
 * @return
 *   0 if the lock succeeded, negative value on failure.
 */
int
nfp_cpp_mutex_trylock(struct nfp_cpp_mutex *mutex)
{
	int err;
	uint32_t key;
	uint32_t tmp;
	uint32_t value;
	struct nfp_cpp *cpp = mutex->cpp;
	uint32_t mur = NFP_CPP_ID(mutex->target, 3, 0);    /* atomic_read */
	uint32_t muw = NFP_CPP_ID(mutex->target, 4, 0);    /* atomic_write */
	uint32_t mus = NFP_CPP_ID(mutex->target, 5, 3);    /* test_set_imm */

	if (mutex->depth > 0) {
		if (mutex->depth == MUTEX_DEPTH_MAX)
			return -E2BIG;

		mutex->depth++;
		return 0;
	}

	/* Verify that the lock marker is not damaged */
	err = nfp_cpp_readl(cpp, mur, mutex->address + 4, &key);
	if (err < 0)
		return err;

	if (key != mutex->key)
		return -EPERM;

	/*
	 * Compare against the unlocked state, and if true,
	 * write the interface id into the top 16 bits, and
	 * mark as locked.
	 */
	value = nfp_mutex_locked(nfp_cpp_interface(cpp));

	/*
	 * We use test_set_imm here, as it implies a read
	 * of the current state, and sets the bits in the
	 * bytemask of the command to 1s. Since the mutex
	 * is guaranteed to be 64-bit aligned, the bytemask
	 * of this 32-bit command is ensured to be 8'b00001111,
	 * which implies that the lower 4 bits will be set to
	 * ones regardless of the initial state.
	 *
	 * Since this is a 'Readback' operation, with no Pull
	 * data, we can treat this as a normal Push (read)
	 * atomic, which returns the original value.
	 */
	err = nfp_cpp_readl(cpp, mus, mutex->address, &tmp);
	if (err < 0)
		return err;

	/* Was it unlocked? */
	if (nfp_mutex_is_unlocked(tmp)) {
		/*
		 * The read value can only be 0x....0000 in the unlocked state.
		 * If there was another contending for this lock, then
		 * the lock state would be 0x....000f
		 *
		 * Write our owner ID into the lock.
		 * While not strictly necessary, this helps with
		 * debug and bookkeeping.
		 */
		err = nfp_cpp_writel(cpp, muw, mutex->address, value);
		if (err < 0)
			return err;

		mutex->depth = 1;
		return 0;
	}

	/* Already locked by us? Success! */
	if (tmp == value) {
		mutex->depth = 1;
		return 0;
	}

	return nfp_mutex_is_locked(tmp) ? -EBUSY : -EINVAL;
}

/**
 * Release lock if held by local system.
 * Extreme care is advised, call only when no local lock users can exist.
 *
 * @param cpp
 *   NFP CPP handle
 * @param target
 *   NFP CPP target ID (ie NFP_CPP_TARGET_CLS or NFP_CPP_TARGET_MU)
 * @param address
 *   Offset into the address space of the NFP CPP target ID
 *
 * @return
 *   - (0) if the lock was OK
 *   - (1) if locked by us
 *   - (-errno) on invalid mutex
 */
int
nfp_cpp_mutex_reclaim(struct nfp_cpp *cpp,
		int target,
		uint64_t address)
{
	int err;
	uint32_t tmp;
	uint16_t interface = nfp_cpp_interface(cpp);
	const uint32_t mur = NFP_CPP_ID(target, 3, 0);    /* atomic_read */
	const uint32_t muw = NFP_CPP_ID(target, 4, 0);    /* atomic_write */

	err = nfp_cpp_mutex_validate(interface, &target, address);
	if (err != 0)
		return err;

	/* Check lock */
	err = nfp_cpp_readl(cpp, mur, address, &tmp);
	if (err < 0)
		return err;

	if (nfp_mutex_is_unlocked(tmp) || nfp_mutex_owner(tmp) != interface)
		return 0;

	/* Bust the lock */
	err = nfp_cpp_writel(cpp, muw, address, nfp_mutex_unlocked(interface));
	if (err < 0)
		return err;

	return 1;
}
