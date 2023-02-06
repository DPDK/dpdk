/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <errno.h>

#include <malloc.h>
#include <time.h>
#include <sched.h>

#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp6000/nfp6000.h"

#define MUTEX_LOCKED(interface)  ((((uint32_t)(interface)) << 16) | 0x000f)
#define MUTEX_UNLOCK(interface)  (0                               | 0x0000)

#define MUTEX_IS_LOCKED(value)   (((value) & 0xffff) == 0x000f)
#define MUTEX_IS_UNLOCKED(value) (((value) & 0xffff) == 0x0000)
#define MUTEX_INTERFACE(value)   (((value) >> 16) & 0xffff)

/*
 * If you need more than 65536 recursive locks, please
 * rethink your code.
 */
#define MUTEX_DEPTH_MAX         0xffff

struct nfp_cpp_mutex {
	struct nfp_cpp *cpp;
	uint8_t target;
	uint16_t depth;
	unsigned long long address;
	uint32_t key;
	unsigned int usage;
	struct nfp_cpp_mutex *prev, *next;
};

static int
_nfp_cpp_mutex_validate(uint32_t model, int *target, unsigned long long address)
{
	/* Address must be 64-bit aligned */
	if (address & 7)
		return NFP_ERRNO(EINVAL);

	if (NFP_CPP_MODEL_IS_6000(model)) {
		if (*target != NFP_CPP_TARGET_MU)
			return NFP_ERRNO(EINVAL);
	} else {
		return NFP_ERRNO(EINVAL);
	}

	return 0;
}

/*
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
 * @param mutex     NFP CPP Mutex handle
 * @param target    NFP CPP target ID (ie NFP_CPP_TARGET_CLS or
 *		    NFP_CPP_TARGET_MU)
 * @param address   Offset into the address space of the NFP CPP target ID
 * @param key       Unique 32-bit value for this mutex
 *
 * @return 0 on success, or -1 on failure (and set errno accordingly).
 */
int
nfp_cpp_mutex_init(struct nfp_cpp *cpp, int target, unsigned long long address,
		   uint32_t key)
{
	uint32_t model = nfp_cpp_model(cpp);
	uint32_t muw = NFP_CPP_ID(target, 4, 0);	/* atomic_write */
	int err;

	err = _nfp_cpp_mutex_validate(model, &target, address);
	if (err < 0)
		return err;

	err = nfp_cpp_writel(cpp, muw, address + 4, key);
	if (err < 0)
		return err;

	err =
	    nfp_cpp_writel(cpp, muw, address + 0,
			   MUTEX_LOCKED(nfp_cpp_interface(cpp)));
	if (err < 0)
		return err;

	return 0;
}

/*
 * Create a mutex handle from an address controlled by a MU Atomic engine
 *
 * The CPP target:address must point to a 64-bit aligned location, and
 * reserve 64 bits of data at the location for use by the handle.
 *
 * Only target/address pairs that point to entities that support the
 * MU Atomic Engine are supported.
 *
 * @param cpp       NFP CPP handle
 * @param target    NFP CPP target ID (ie NFP_CPP_TARGET_CLS or
 *		    NFP_CPP_TARGET_MU)
 * @param address   Offset into the address space of the NFP CPP target ID
 * @param key       32-bit unique key (must match the key at this location)
 *
 * @return      A non-NULL struct nfp_cpp_mutex * on success, NULL on failure.
 */
struct nfp_cpp_mutex *
nfp_cpp_mutex_alloc(struct nfp_cpp *cpp, int target,
		     unsigned long long address, uint32_t key)
{
	uint32_t model = nfp_cpp_model(cpp);
	struct nfp_cpp_mutex *mutex;
	uint32_t mur = NFP_CPP_ID(target, 3, 0);	/* atomic_read */
	int err;
	uint32_t tmp;

	/* Look for cached mutex */
	for (mutex = cpp->mutex_cache; mutex; mutex = mutex->next) {
		if (mutex->target == target && mutex->address == address)
			break;
	}

	if (mutex) {
		if (mutex->key == key) {
			mutex->usage++;
			return mutex;
		}

		/* If the key doesn't match... */
		return NFP_ERRPTR(EEXIST);
	}

	err = _nfp_cpp_mutex_validate(model, &target, address);
	if (err < 0)
		return NULL;

	err = nfp_cpp_readl(cpp, mur, address + 4, &tmp);
	if (err < 0)
		return NULL;

	if (tmp != key)
		return NFP_ERRPTR(EEXIST);

	mutex = calloc(sizeof(*mutex), 1);
	if (mutex == NULL)
		return NFP_ERRPTR(ENOMEM);

	mutex->cpp = cpp;
	mutex->target = target;
	mutex->address = address;
	mutex->key = key;
	mutex->depth = 0;
	mutex->usage = 1;

	/* Add mutex to the cache */
	if (cpp->mutex_cache) {
		cpp->mutex_cache->prev = mutex;
		mutex->next = cpp->mutex_cache;
		cpp->mutex_cache = mutex;
	} else {
		cpp->mutex_cache = mutex;
	}

	return mutex;
}

struct nfp_cpp *
nfp_cpp_mutex_cpp(struct nfp_cpp_mutex *mutex)
{
	return mutex->cpp;
}

uint32_t
nfp_cpp_mutex_key(struct nfp_cpp_mutex *mutex)
{
	return mutex->key;
}

uint16_t
nfp_cpp_mutex_owner(struct nfp_cpp_mutex *mutex)
{
	uint32_t mur = NFP_CPP_ID(mutex->target, 3, 0);	/* atomic_read */
	uint32_t value, key;
	int err;

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address, &value);
	if (err < 0)
		return err;

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address + 4, &key);
	if (err < 0)
		return err;

	if (key != mutex->key)
		return NFP_ERRNO(EPERM);

	if (MUTEX_IS_LOCKED(value) == 0)
		return 0;

	return MUTEX_INTERFACE(value);
}

int
nfp_cpp_mutex_target(struct nfp_cpp_mutex *mutex)
{
	return mutex->target;
}

uint64_t
nfp_cpp_mutex_address(struct nfp_cpp_mutex *mutex)
{
	return mutex->address;
}

/*
 * Free a mutex handle - does not alter the lock state
 *
 * @param mutex     NFP CPP Mutex handle
 */
void
nfp_cpp_mutex_free(struct nfp_cpp_mutex *mutex)
{
	mutex->usage--;
	if (mutex->usage > 0)
		return;

	/* Remove mutex from the cache */
	if (mutex->next)
		mutex->next->prev = mutex->prev;
	if (mutex->prev)
		mutex->prev->next = mutex->next;

	/* If mutex->cpp == NULL, something broke */
	if (mutex->cpp && mutex == mutex->cpp->mutex_cache)
		mutex->cpp->mutex_cache = mutex->next;

	free(mutex);
}

/*
 * Lock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex     NFP CPP Mutex handle
 *
 * @return 0 on success, or -1 on failure (and set errno accordingly).
 */
int
nfp_cpp_mutex_lock(struct nfp_cpp_mutex *mutex)
{
	int err;
	time_t warn_at = time(NULL) + 15;

	while ((err = nfp_cpp_mutex_trylock(mutex)) != 0) {
		/* If errno != EBUSY, then the lock was damaged */
		if (err < 0 && errno != EBUSY)
			return err;
		if (time(NULL) >= warn_at) {
			PMD_DRV_LOG(ERR, "Warning: waiting for NFP mutex usage:%u depth:%hd] target:%d addr:%llx key:%08x]",
				    mutex->usage, mutex->depth, mutex->target,
				    mutex->address, mutex->key);
			warn_at = time(NULL) + 60;
		}
		sched_yield();
	}
	return 0;
}

/*
 * Unlock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex     NFP CPP Mutex handle
 *
 * @return 0 on success, or -1 on failure (and set errno accordingly).
 */
int
nfp_cpp_mutex_unlock(struct nfp_cpp_mutex *mutex)
{
	uint32_t muw = NFP_CPP_ID(mutex->target, 4, 0);	/* atomic_write */
	uint32_t mur = NFP_CPP_ID(mutex->target, 3, 0);	/* atomic_read */
	struct nfp_cpp *cpp = mutex->cpp;
	uint32_t key, value;
	uint16_t interface = nfp_cpp_interface(cpp);
	int err;

	if (mutex->depth > 1) {
		mutex->depth--;
		return 0;
	}

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address, &value);
	if (err < 0)
		goto exit;

	err = nfp_cpp_readl(mutex->cpp, mur, mutex->address + 4, &key);
	if (err < 0)
		goto exit;

	if (key != mutex->key) {
		err = NFP_ERRNO(EPERM);
		goto exit;
	}

	if (value != MUTEX_LOCKED(interface)) {
		err = NFP_ERRNO(EACCES);
		goto exit;
	}

	err = nfp_cpp_writel(cpp, muw, mutex->address, MUTEX_UNLOCK(interface));
	if (err < 0)
		goto exit;

	mutex->depth = 0;

exit:
	return err;
}

/*
 * Attempt to lock a mutex handle, using the NFP MU Atomic Engine
 *
 * Valid lock states:
 *
 *      0x....0000      - Unlocked
 *      0x....000f      - Locked
 *
 * @param mutex     NFP CPP Mutex handle
 * @return      0 if the lock succeeded, -1 on failure (and errno set
 *		appropriately).
 */
int
nfp_cpp_mutex_trylock(struct nfp_cpp_mutex *mutex)
{
	uint32_t mur = NFP_CPP_ID(mutex->target, 3, 0);	/* atomic_read */
	uint32_t muw = NFP_CPP_ID(mutex->target, 4, 0);	/* atomic_write */
	uint32_t mus = NFP_CPP_ID(mutex->target, 5, 3);	/* test_set_imm */
	uint32_t key, value, tmp;
	struct nfp_cpp *cpp = mutex->cpp;
	int err;

	if (mutex->depth > 0) {
		if (mutex->depth == MUTEX_DEPTH_MAX)
			return NFP_ERRNO(E2BIG);

		mutex->depth++;
		return 0;
	}

	/* Verify that the lock marker is not damaged */
	err = nfp_cpp_readl(cpp, mur, mutex->address + 4, &key);
	if (err < 0)
		goto exit;

	if (key != mutex->key) {
		err = NFP_ERRNO(EPERM);
		goto exit;
	}

	/*
	 * Compare against the unlocked state, and if true,
	 * write the interface id into the top 16 bits, and
	 * mark as locked.
	 */
	value = MUTEX_LOCKED(nfp_cpp_interface(cpp));

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
		goto exit;

	/* Was it unlocked? */
	if (MUTEX_IS_UNLOCKED(tmp)) {
		/*
		 * The read value can only be 0x....0000 in the unlocked state.
		 * If there was another contending for this lock, then
		 * the lock state would be 0x....000f
		 *
		 * Write our owner ID into the lock
		 * While not strictly necessary, this helps with
		 * debug and bookkeeping.
		 */
		err = nfp_cpp_writel(cpp, muw, mutex->address, value);
		if (err < 0)
			goto exit;

		mutex->depth = 1;
		goto exit;
	}

	/* Already locked by us? Success! */
	if (tmp == value) {
		mutex->depth = 1;
		goto exit;
	}

	err = NFP_ERRNO(MUTEX_IS_LOCKED(tmp) ? EBUSY : EINVAL);

exit:
	return err;
}
