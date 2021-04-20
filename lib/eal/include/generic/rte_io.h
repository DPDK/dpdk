/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_IO_H_
#define _RTE_IO_H_

/**
 * @file
 * I/O device memory operations
 *
 * This file defines the generic API for I/O device memory read/write operations
 */

#include <stdint.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_atomic.h>

#ifdef __DOXYGEN__

/**
 * Read a 8-bit value from I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint8_t
rte_read8_relaxed(const volatile void *addr);

/**
 * Read a 16-bit value from I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint16_t
rte_read16_relaxed(const volatile void *addr);

/**
 * Read a 32-bit value from I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint32_t
rte_read32_relaxed(const volatile void *addr);

/**
 * Read a 64-bit value from I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint64_t
rte_read64_relaxed(const volatile void *addr);

/**
 * Write a 8-bit value to I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */

static inline void
rte_write8_relaxed(uint8_t value, volatile void *addr);

/**
 * Write a 16-bit value to I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write16_relaxed(uint16_t value, volatile void *addr);

/**
 * Write a 32-bit value to I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write32_relaxed(uint32_t value, volatile void *addr);

/**
 * Write a 64-bit value to I/O device memory address *addr*.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write64_relaxed(uint64_t value, volatile void *addr);

/**
 * Read a 8-bit value from I/O device memory address *addr*.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint8_t
rte_read8(const volatile void *addr);

/**
 * Read a 16-bit value from I/O device memory address *addr*.
 *
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint16_t
rte_read16(const volatile void *addr);

/**
 * Read a 32-bit value from I/O device memory address *addr*.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint32_t
rte_read32(const volatile void *addr);

/**
 * Read a 64-bit value from I/O device memory address *addr*.
 *
 * @param addr
 *  I/O memory address to read the value from
 * @return
 *  read value
 */
static inline uint64_t
rte_read64(const volatile void *addr);

/**
 * Write a 8-bit value to I/O device memory address *addr*.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */

static inline void
rte_write8(uint8_t value, volatile void *addr);

/**
 * Write a 16-bit value to I/O device memory address *addr*.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write16(uint16_t value, volatile void *addr);

/**
 * Write a 32-bit value to I/O device memory address *addr*.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write32(uint32_t value, volatile void *addr);

/**
 * Write a 64-bit value to I/O device memory address *addr*.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
static inline void
rte_write64(uint64_t value, volatile void *addr);

/**
 * Write a 32-bit value to I/O device memory address addr using write
 * combining memory write protocol. Depending on the platform write combining
 * may not be available and/or may be treated as a hint and the behavior may
 * fallback to a regular store.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
__rte_experimental
static inline void
rte_write32_wc(uint32_t value, volatile void *addr);

/**
 * Write a 32-bit value to I/O device memory address addr using write
 * combining memory write protocol. Depending on the platform write combining
 * may not be available and/or may be treated as a hint and the behavior may
 * fallback to a regular store.
 *
 * The relaxed version does not have additional I/O memory barrier, useful in
 * accessing the device registers of integrated controllers which implicitly
 * strongly ordered with respect to memory access.
 *
 * @param value
 *  Value to write
 * @param addr
 *  I/O memory address to write the value to
 */
__rte_experimental
static inline void
rte_write32_wc_relaxed(uint32_t value, volatile void *addr);

#endif /* __DOXYGEN__ */

#ifndef RTE_OVERRIDE_IO_H

static __rte_always_inline uint8_t
rte_read8_relaxed(const volatile void *addr)
{
	return *(const volatile uint8_t *)addr;
}

static __rte_always_inline uint16_t
rte_read16_relaxed(const volatile void *addr)
{
	return *(const volatile uint16_t *)addr;
}

static __rte_always_inline uint32_t
rte_read32_relaxed(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

static __rte_always_inline uint64_t
rte_read64_relaxed(const volatile void *addr)
{
	return *(const volatile uint64_t *)addr;
}

static __rte_always_inline void
rte_write8_relaxed(uint8_t value, volatile void *addr)
{
	*(volatile uint8_t *)addr = value;
}

static __rte_always_inline void
rte_write16_relaxed(uint16_t value, volatile void *addr)
{
	*(volatile uint16_t *)addr = value;
}

static __rte_always_inline void
rte_write32_relaxed(uint32_t value, volatile void *addr)
{
	*(volatile uint32_t *)addr = value;
}

static __rte_always_inline void
rte_write64_relaxed(uint64_t value, volatile void *addr)
{
	*(volatile uint64_t *)addr = value;
}

static __rte_always_inline uint8_t
rte_read8(const volatile void *addr)
{
	uint8_t val;
	val = rte_read8_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint16_t
rte_read16(const volatile void *addr)
{
	uint16_t val;
	val = rte_read16_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint32_t
rte_read32(const volatile void *addr)
{
	uint32_t val;
	val = rte_read32_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint64_t
rte_read64(const volatile void *addr)
{
	uint64_t val;
	val = rte_read64_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline void
rte_write8(uint8_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write8_relaxed(value, addr);
}

static __rte_always_inline void
rte_write16(uint16_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write16_relaxed(value, addr);
}

static __rte_always_inline void
rte_write32(uint32_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write32_relaxed(value, addr);
}

static __rte_always_inline void
rte_write64(uint64_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write64_relaxed(value, addr);
}

#ifndef RTE_NATIVE_WRITE32_WC
static __rte_always_inline void
rte_write32_wc(uint32_t value, volatile void *addr)
{
	rte_write32(value, addr);
}

static __rte_always_inline void
rte_write32_wc_relaxed(uint32_t value, volatile void *addr)
{
	rte_write32_relaxed(value, addr);
}
#endif /* RTE_NATIVE_WRITE32_WC */

#endif /* RTE_OVERRIDE_IO_H */

#endif /* _RTE_IO_H_ */
