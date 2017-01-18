/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Cavium networks. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_IO_H_
#define _RTE_IO_H_

#include <rte_atomic.h>

/**
 * @file
 * I/O device memory operations
 *
 * This file defines the generic API for I/O device memory read/write operations
 */

#include <stdint.h>
#include <rte_common.h>
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

#endif /* __DOXYGEN__ */

#ifndef RTE_OVERRIDE_IO_H

static inline uint8_t __attribute__((always_inline))
rte_read8_relaxed(const volatile void *addr)
{
	return *(const volatile uint8_t *)addr;
}

static inline uint16_t __attribute__((always_inline))
rte_read16_relaxed(const volatile void *addr)
{
	return *(const volatile uint16_t *)addr;
}

static inline uint32_t __attribute__((always_inline))
rte_read32_relaxed(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

static inline uint64_t __attribute__((always_inline))
rte_read64_relaxed(const volatile void *addr)
{
	return *(const volatile uint64_t *)addr;
}

static inline void __attribute__((always_inline))
rte_write8_relaxed(uint8_t value, volatile void *addr)
{
	*(volatile uint8_t *)addr = value;
}

static inline void __attribute__((always_inline))
rte_write16_relaxed(uint16_t value, volatile void *addr)
{
	*(volatile uint16_t *)addr = value;
}

static inline void __attribute__((always_inline))
rte_write32_relaxed(uint32_t value, volatile void *addr)
{
	*(volatile uint32_t *)addr = value;
}

static inline void __attribute__((always_inline))
rte_write64_relaxed(uint64_t value, volatile void *addr)
{
	*(volatile uint64_t *)addr = value;
}

static inline uint8_t __attribute__((always_inline))
rte_read8(const volatile void *addr)
{
	uint8_t val;
	val = rte_read8_relaxed(addr);
	rte_io_rmb();
	return val;
}

static inline uint16_t __attribute__((always_inline))
rte_read16(const volatile void *addr)
{
	uint16_t val;
	val = rte_read16_relaxed(addr);
	rte_io_rmb();
	return val;
}

static inline uint32_t  __attribute__((always_inline))
rte_read32(const volatile void *addr)
{
	uint32_t val;
	val = rte_read32_relaxed(addr);
	rte_io_rmb();
	return val;
}

static inline uint64_t __attribute__((always_inline))
rte_read64(const volatile void *addr)
{
	uint64_t val;
	val = rte_read64_relaxed(addr);
	rte_io_rmb();
	return val;
}

static inline void __attribute__((always_inline))
rte_write8(uint8_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write8_relaxed(value, addr);
}

static inline void __attribute__((always_inline))
rte_write16(uint16_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write16_relaxed(value, addr);
}

static inline void __attribute__((always_inline))
rte_write32(uint32_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write32_relaxed(value, addr);
}

static inline void __attribute__((always_inline))
rte_write64(uint64_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write64_relaxed(value, addr);
}

#endif /* RTE_OVERRIDE_IO_H */

#endif /* _RTE_IO_H_ */
