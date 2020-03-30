/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_COMMON_EFSYS_H
#define _SFC_COMMON_EFSYS_H

#include <stdbool.h>

#include <rte_spinlock.h>
#include <rte_byteorder.h>
#include <rte_debug.h>
#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_io.h>

#include "sfc_debug.h"
#include "sfc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EFSYS_HAS_UINT64 1
#define EFSYS_USE_UINT64 1
#define EFSYS_HAS_SSE2_M128 1

#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define EFSYS_IS_BIG_ENDIAN 1
#define EFSYS_IS_LITTLE_ENDIAN 0
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define EFSYS_IS_BIG_ENDIAN 0
#define EFSYS_IS_LITTLE_ENDIAN 1
#else
#error "Cannot determine system endianness"
#endif
#include "efx_types.h"


typedef bool boolean_t;

#ifndef B_FALSE
#define B_FALSE	false
#endif
#ifndef B_TRUE
#define B_TRUE	true
#endif

/*
 * RTE_MAX() and RTE_MIN() cannot be used since braced-group within
 * expression allowed only inside a function, but MAX() is used as
 * a number of elements in array.
 */
#ifndef MAX
#define MAX(v1, v2)	((v1) > (v2) ? (v1) : (v2))
#endif
#ifndef MIN
#define MIN(v1, v2)	((v1) < (v2) ? (v1) : (v2))
#endif

#ifndef ISP2
#define ISP2(x)			rte_is_power_of_2(x)
#endif

#define ENOTACTIVE	ENOTCONN

static inline void
prefetch_read_many(const volatile void *addr)
{
	rte_prefetch0(addr);
}

static inline void
prefetch_read_once(const volatile void *addr)
{
	rte_prefetch_non_temporal(addr);
}

/* Code inclusion options */


#define EFSYS_OPT_NAMES 1

/* Disable SFN5xxx/SFN6xxx since it requires specific support in the PMD */
#define EFSYS_OPT_SIENA 0
/* Enable SFN7xxx support */
#define EFSYS_OPT_HUNTINGTON 1
/* Enable SFN8xxx support */
#define EFSYS_OPT_MEDFORD 1
/* Enable SFN2xxx support */
#define EFSYS_OPT_MEDFORD2 1
#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
#define EFSYS_OPT_CHECK_REG 1
#else
#define EFSYS_OPT_CHECK_REG 0
#endif

/* MCDI is required for SFN7xxx and SFN8xx */
#define EFSYS_OPT_MCDI 1
#define EFSYS_OPT_MCDI_LOGGING 1
#define EFSYS_OPT_MCDI_PROXY_AUTH 1

#define EFSYS_OPT_MAC_STATS 1

#define EFSYS_OPT_LOOPBACK 1

#define EFSYS_OPT_MON_MCDI 0
#define EFSYS_OPT_MON_STATS 0

#define EFSYS_OPT_PHY_STATS 0
#define EFSYS_OPT_BIST 0
#define EFSYS_OPT_PHY_LED_CONTROL 0
#define EFSYS_OPT_PHY_FLAGS 0

#define EFSYS_OPT_VPD 0
#define EFSYS_OPT_NVRAM 0
#define EFSYS_OPT_BOOTCFG 0
#define EFSYS_OPT_IMAGE_LAYOUT 0

#define EFSYS_OPT_DIAG 0
#define EFSYS_OPT_RX_SCALE 1
#define EFSYS_OPT_QSTATS 0
/* Filters support is required for SFN7xxx and SFN8xx */
#define EFSYS_OPT_FILTER 1
#define EFSYS_OPT_RX_SCATTER 0

#define EFSYS_OPT_EV_PREFETCH 0

#define EFSYS_OPT_DECODE_INTR_FATAL 0

#define EFSYS_OPT_LICENSING 0

#define EFSYS_OPT_ALLOW_UNCONFIGURED_NIC 0

#define EFSYS_OPT_RX_PACKED_STREAM 0

#define EFSYS_OPT_RX_ES_SUPER_BUFFER 1

#define EFSYS_OPT_TUNNEL 1

#define EFSYS_OPT_FW_SUBVARIANT_AWARE 1

#define EFSYS_OPT_EVB 0

#define EFSYS_OPT_MCDI_PROXY_AUTH_SERVER 0

/* ID */

typedef struct __efsys_identifier_s efsys_identifier_t;


#define EFSYS_PROBE(_name)						\
	do { } while (0)

#define EFSYS_PROBE1(_name, _type1, _arg1)				\
	do { } while (0)

#define EFSYS_PROBE2(_name, _type1, _arg1, _type2, _arg2)		\
	do { } while (0)

#define EFSYS_PROBE3(_name, _type1, _arg1, _type2, _arg2,		\
		     _type3, _arg3)					\
	do { } while (0)

#define EFSYS_PROBE4(_name, _type1, _arg1, _type2, _arg2,		\
		     _type3, _arg3, _type4, _arg4)			\
	do { } while (0)

#define EFSYS_PROBE5(_name, _type1, _arg1, _type2, _arg2,		\
		     _type3, _arg3, _type4, _arg4, _type5, _arg5)	\
	do { } while (0)

#define EFSYS_PROBE6(_name, _type1, _arg1, _type2, _arg2,		\
		     _type3, _arg3, _type4, _arg4, _type5, _arg5,	\
		     _type6, _arg6)					\
	do { } while (0)

#define EFSYS_PROBE7(_name, _type1, _arg1, _type2, _arg2,		\
		     _type3, _arg3, _type4, _arg4, _type5, _arg5,	\
		     _type6, _arg6, _type7, _arg7)			\
	do { } while (0)


/* DMA */

typedef rte_iova_t efsys_dma_addr_t;

typedef struct efsys_mem_s {
	const struct rte_memzone	*esm_mz;
	/*
	 * Ideally it should have volatile qualifier to denote that
	 * the memory may be updated by someone else. However, it adds
	 * qualifier discard warnings when the pointer or its derivative
	 * is passed to memset() or rte_mov16().
	 * So, skip the qualifier here, but make sure that it is added
	 * below in access macros.
	 */
	void				*esm_base;
	efsys_dma_addr_t		esm_addr;
} efsys_mem_t;


#define EFSYS_MEM_ZERO(_esmp, _size)					\
	do {								\
		(void)memset((void *)(_esmp)->esm_base, 0, (_size));	\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_MEM_READD(_esmp, _offset, _edp)				\
	do {								\
		volatile uint8_t  *_base = (_esmp)->esm_base;		\
		volatile uint32_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_dword_t)));	\
									\
		_addr = (volatile uint32_t *)(_base + (_offset));	\
		(_edp)->ed_u32[0] = _addr[0];				\
									\
		EFSYS_PROBE2(mem_readl, unsigned int, (_offset),	\
					 uint32_t, (_edp)->ed_u32[0]);	\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_MEM_READQ(_esmp, _offset, _eqp)				\
	do {								\
		volatile uint8_t  *_base = (_esmp)->esm_base;		\
		volatile uint64_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_qword_t)));	\
									\
		_addr = (volatile uint64_t *)(_base + (_offset));	\
		(_eqp)->eq_u64[0] = _addr[0];				\
									\
		EFSYS_PROBE3(mem_readq, unsigned int, (_offset),	\
					 uint32_t, (_eqp)->eq_u32[1],	\
					 uint32_t, (_eqp)->eq_u32[0]);	\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_MEM_READO(_esmp, _offset, _eop)				\
	do {								\
		volatile uint8_t *_base = (_esmp)->esm_base;		\
		volatile __m128i *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_oword_t)));	\
									\
		_addr = (volatile __m128i *)(_base + (_offset));	\
		(_eop)->eo_u128[0] = _addr[0];				\
									\
		EFSYS_PROBE5(mem_reado, unsigned int, (_offset),	\
					 uint32_t, (_eop)->eo_u32[3],	\
					 uint32_t, (_eop)->eo_u32[2],	\
					 uint32_t, (_eop)->eo_u32[1],	\
					 uint32_t, (_eop)->eo_u32[0]);	\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)


#define EFSYS_MEM_WRITED(_esmp, _offset, _edp)				\
	do {								\
		volatile uint8_t  *_base = (_esmp)->esm_base;		\
		volatile uint32_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_dword_t)));	\
									\
		EFSYS_PROBE2(mem_writed, unsigned int, (_offset),	\
					 uint32_t, (_edp)->ed_u32[0]);	\
									\
		_addr = (volatile uint32_t *)(_base + (_offset));	\
		_addr[0] = (_edp)->ed_u32[0];				\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_MEM_WRITEQ(_esmp, _offset, _eqp)				\
	do {								\
		volatile uint8_t  *_base = (_esmp)->esm_base;		\
		volatile uint64_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_qword_t)));	\
									\
		EFSYS_PROBE3(mem_writeq, unsigned int, (_offset),	\
					 uint32_t, (_eqp)->eq_u32[1],	\
					 uint32_t, (_eqp)->eq_u32[0]);	\
									\
		_addr = (volatile uint64_t *)(_base + (_offset));	\
		_addr[0] = (_eqp)->eq_u64[0];				\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_MEM_WRITEO(_esmp, _offset, _eop)				\
	do {								\
		volatile uint8_t *_base = (_esmp)->esm_base;		\
		volatile __m128i *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_oword_t)));	\
									\
									\
		EFSYS_PROBE5(mem_writeo, unsigned int, (_offset),	\
					 uint32_t, (_eop)->eo_u32[3],	\
					 uint32_t, (_eop)->eo_u32[2],	\
					 uint32_t, (_eop)->eo_u32[1],	\
					 uint32_t, (_eop)->eo_u32[0]);	\
									\
		_addr = (volatile __m128i *)(_base + (_offset));	\
		_addr[0] = (_eop)->eo_u128[0];				\
									\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)


#define	EFSYS_MEM_SIZE(_esmp)						\
	((_esmp)->esm_mz->len)

#define EFSYS_MEM_ADDR(_esmp)						\
	((_esmp)->esm_addr)

#define EFSYS_MEM_IS_NULL(_esmp)					\
	((_esmp)->esm_base == NULL)

#define EFSYS_MEM_PREFETCH(_esmp, _offset)				\
	do {								\
		volatile uint8_t *_base = (_esmp)->esm_base;		\
									\
		rte_prefetch0(_base + (_offset));			\
	} while (0)


/* BAR */

typedef struct efsys_bar_s {
	rte_spinlock_t		esb_lock;
	int			esb_rid;
	struct rte_pci_device	*esb_dev;
	/*
	 * Ideally it should have volatile qualifier to denote that
	 * the memory may be updated by someone else. However, it adds
	 * qualifier discard warnings when the pointer or its derivative
	 * is passed to memset() or rte_mov16().
	 * So, skip the qualifier here, but make sure that it is added
	 * below in access macros.
	 */
	void			*esb_base;
} efsys_bar_t;

#define SFC_BAR_LOCK_INIT(_esbp, _ifname)				\
	do {								\
		rte_spinlock_init(&(_esbp)->esb_lock);			\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)
#define SFC_BAR_LOCK_DESTROY(_esbp)	((void)0)
#define SFC_BAR_LOCK(_esbp)		rte_spinlock_lock(&(_esbp)->esb_lock)
#define SFC_BAR_UNLOCK(_esbp)		rte_spinlock_unlock(&(_esbp)->esb_lock)

#define EFSYS_BAR_READD(_esbp, _offset, _edp, _lock)			\
	do {								\
		volatile uint8_t  *_base = (_esbp)->esb_base;		\
		volatile uint32_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_dword_t)));	\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_LOCK(_esbp);				\
									\
		_addr = (volatile uint32_t *)(_base + (_offset));	\
		rte_rmb();						\
		(_edp)->ed_u32[0] = rte_read32_relaxed(_addr);		\
									\
		EFSYS_PROBE2(bar_readd, unsigned int, (_offset),	\
					 uint32_t, (_edp)->ed_u32[0]);	\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_UNLOCK(_esbp);				\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_BAR_READQ(_esbp, _offset, _eqp)				\
	do {								\
		volatile uint8_t  *_base = (_esbp)->esb_base;		\
		volatile uint64_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_qword_t)));	\
									\
		SFC_BAR_LOCK(_esbp);					\
									\
		_addr = (volatile uint64_t *)(_base + (_offset));	\
		rte_rmb();						\
		(_eqp)->eq_u64[0] = rte_read64_relaxed(_addr);		\
									\
		EFSYS_PROBE3(bar_readq, unsigned int, (_offset),	\
					 uint32_t, (_eqp)->eq_u32[1],	\
					 uint32_t, (_eqp)->eq_u32[0]);	\
									\
		SFC_BAR_UNLOCK(_esbp);					\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_BAR_READO(_esbp, _offset, _eop, _lock)			\
	do {								\
		volatile uint8_t *_base = (_esbp)->esb_base;		\
		volatile __m128i *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_oword_t)));	\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_LOCK(_esbp);				\
									\
		_addr = (volatile __m128i *)(_base + (_offset));	\
		rte_rmb();						\
		/* There is no rte_read128_relaxed() yet */		\
		(_eop)->eo_u128[0] = _addr[0];				\
									\
		EFSYS_PROBE5(bar_reado, unsigned int, (_offset),	\
					 uint32_t, (_eop)->eo_u32[3],	\
					 uint32_t, (_eop)->eo_u32[2],	\
					 uint32_t, (_eop)->eo_u32[1],	\
					 uint32_t, (_eop)->eo_u32[0]);	\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_UNLOCK(_esbp);				\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)


#define EFSYS_BAR_WRITED(_esbp, _offset, _edp, _lock)			\
	do {								\
		volatile uint8_t  *_base = (_esbp)->esb_base;		\
		volatile uint32_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_dword_t)));	\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_LOCK(_esbp);				\
									\
		EFSYS_PROBE2(bar_writed, unsigned int, (_offset),	\
					 uint32_t, (_edp)->ed_u32[0]);	\
									\
		_addr = (volatile uint32_t *)(_base + (_offset));	\
		rte_write32_relaxed((_edp)->ed_u32[0], _addr);		\
		rte_wmb();						\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_UNLOCK(_esbp);				\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_BAR_WRITEQ(_esbp, _offset, _eqp)				\
	do {								\
		volatile uint8_t  *_base = (_esbp)->esb_base;		\
		volatile uint64_t *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_qword_t)));	\
									\
		SFC_BAR_LOCK(_esbp);					\
									\
		EFSYS_PROBE3(bar_writeq, unsigned int, (_offset),	\
					 uint32_t, (_eqp)->eq_u32[1],	\
					 uint32_t, (_eqp)->eq_u32[0]);	\
									\
		_addr = (volatile uint64_t *)(_base + (_offset));	\
		rte_write64_relaxed((_eqp)->eq_u64[0], _addr);		\
		rte_wmb();						\
									\
		SFC_BAR_UNLOCK(_esbp);					\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/*
 * Guarantees 64bit aligned 64bit writes to write combined BAR mapping
 * (required by PIO hardware).
 *
 * Neither VFIO, nor UIO, nor NIC UIO (on FreeBSD) support
 * write-combined memory mapped to user-land, so just abort if used.
 */
#define EFSYS_BAR_WC_WRITEQ(_esbp, _offset, _eqp)			\
	do {								\
		rte_panic("Write-combined BAR access not supported");	\
	} while (B_FALSE)

#define EFSYS_BAR_WRITEO(_esbp, _offset, _eop, _lock)			\
	do {								\
		volatile uint8_t *_base = (_esbp)->esb_base;		\
		volatile __m128i *_addr;				\
									\
		_NOTE(CONSTANTCONDITION);				\
		SFC_ASSERT(EFX_IS_P2ALIGNED(size_t, _offset,		\
					    sizeof(efx_oword_t)));	\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_LOCK(_esbp);				\
									\
		EFSYS_PROBE5(bar_writeo, unsigned int, (_offset),	\
					 uint32_t, (_eop)->eo_u32[3],	\
					 uint32_t, (_eop)->eo_u32[2],	\
					 uint32_t, (_eop)->eo_u32[1],	\
					 uint32_t, (_eop)->eo_u32[0]);	\
									\
		_addr = (volatile __m128i *)(_base + (_offset));	\
		/* There is no rte_write128_relaxed() yet */		\
		_addr[0] = (_eop)->eo_u128[0];				\
		rte_wmb();						\
									\
		_NOTE(CONSTANTCONDITION);				\
		if (_lock)						\
			SFC_BAR_UNLOCK(_esbp);				\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/* Use the standard octo-word write for doorbell writes */
#define EFSYS_BAR_DOORBELL_WRITEO(_esbp, _offset, _eop)			\
	do {								\
		EFSYS_BAR_WRITEO((_esbp), (_offset), (_eop), B_FALSE);	\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/* SPIN */

#define EFSYS_SPIN(_us)							\
	do {								\
		rte_delay_us(_us);					\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_SLEEP EFSYS_SPIN

/* BARRIERS */

#define EFSYS_MEM_READ_BARRIER()	rte_rmb()
#define EFSYS_PIO_WRITE_BARRIER()	rte_io_wmb()

/* DMA SYNC */

/*
 * DPDK does not provide any DMA syncing API, and no PMD drivers
 * have any traces of explicit DMA syncing.
 * DMA mapping is assumed to be coherent.
 */

#define EFSYS_DMA_SYNC_FOR_KERNEL(_esmp, _offset, _size)	((void)0)

/* Just avoid store and compiler (impliciltly) reordering */
#define EFSYS_DMA_SYNC_FOR_DEVICE(_esmp, _offset, _size)	rte_wmb()

/* TIMESTAMP */

typedef uint64_t efsys_timestamp_t;

#define EFSYS_TIMESTAMP(_usp)						\
	do {								\
		*(_usp) = rte_get_timer_cycles() * 1000000 /		\
			rte_get_timer_hz();				\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/* KMEM */

#define EFSYS_KMEM_ALLOC(_esip, _size, _p)				\
	do {								\
		(_esip) = (_esip);					\
		(_p) = rte_zmalloc("sfc", (_size), 0);			\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_KMEM_FREE(_esip, _size, _p)				\
	do {								\
		(void)(_esip);						\
		(void)(_size);						\
		rte_free((_p));						\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/* LOCK */

typedef rte_spinlock_t efsys_lock_t;

#define SFC_EFSYS_LOCK_INIT(_eslp, _ifname, _label)	\
	rte_spinlock_init((_eslp))
#define SFC_EFSYS_LOCK_DESTROY(_eslp) ((void)0)
#define SFC_EFSYS_LOCK(_eslp)				\
	rte_spinlock_lock((_eslp))
#define SFC_EFSYS_UNLOCK(_eslp)				\
	rte_spinlock_unlock((_eslp))
#define SFC_EFSYS_LOCK_ASSERT_OWNED(_eslp)		\
	SFC_ASSERT(rte_spinlock_is_locked((_eslp)))

typedef int efsys_lock_state_t;

#define EFSYS_LOCK_MAGIC	0x000010c4

#define EFSYS_LOCK(_lockp, _state)				\
	do {							\
		SFC_EFSYS_LOCK(_lockp);				\
		(_state) = EFSYS_LOCK_MAGIC;			\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_UNLOCK(_lockp, _state)				\
	do {							\
		SFC_ASSERT((_state) == EFSYS_LOCK_MAGIC);	\
		SFC_EFSYS_UNLOCK(_lockp);			\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

/* STAT */

typedef uint64_t	efsys_stat_t;

#define EFSYS_STAT_INCR(_knp, _delta)				\
	do {							\
		*(_knp) += (_delta);				\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_STAT_DECR(_knp, _delta)				\
	do {							\
		*(_knp) -= (_delta);				\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_STAT_SET(_knp, _val)				\
	do {							\
		*(_knp) = (_val);				\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_STAT_SET_QWORD(_knp, _valp)			\
	do {							\
		*(_knp) = rte_le_to_cpu_64((_valp)->eq_u64[0]);	\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_STAT_SET_DWORD(_knp, _valp)			\
	do {							\
		*(_knp) = rte_le_to_cpu_32((_valp)->ed_u32[0]);	\
		_NOTE(CONSTANTCONDITION);			\
	} while (B_FALSE)

#define EFSYS_STAT_INCR_QWORD(_knp, _valp)				\
	do {								\
		*(_knp) += rte_le_to_cpu_64((_valp)->eq_u64[0]);	\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

#define EFSYS_STAT_SUBR_QWORD(_knp, _valp)				\
	do {								\
		*(_knp) -= rte_le_to_cpu_64((_valp)->eq_u64[0]);	\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)

/* ERR */

#if EFSYS_OPT_DECODE_INTR_FATAL
#define EFSYS_ERR(_esip, _code, _dword0, _dword1)			\
	do {								\
		(void)(_esip);						\
		SFC_GENERIC_LOG(ERR, "FATAL ERROR #%u (0x%08x%08x)",	\
			(_code), (_dword0), (_dword1));			\
		_NOTE(CONSTANTCONDITION);				\
	} while (B_FALSE)
#endif

/* ASSERT */

/* RTE_VERIFY from DPDK treats expressions with % operator incorrectly,
 * so we re-implement it here
 */
#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
#define EFSYS_ASSERT(_exp)						\
	do {								\
		if (unlikely(!(_exp)))					\
			rte_panic("line %d\tassert \"%s\" failed\n",	\
				  __LINE__, (#_exp));			\
	} while (0)
#else
#define EFSYS_ASSERT(_exp)		(void)(_exp)
#endif

#define EFSYS_ASSERT3(_x, _op, _y, _t)	EFSYS_ASSERT((_t)(_x) _op (_t)(_y))

#define EFSYS_ASSERT3U(_x, _op, _y)	EFSYS_ASSERT3(_x, _op, _y, uint64_t)
#define EFSYS_ASSERT3S(_x, _op, _y)	EFSYS_ASSERT3(_x, _op, _y, int64_t)
#define EFSYS_ASSERT3P(_x, _op, _y)	EFSYS_ASSERT3(_x, _op, _y, uintptr_t)

/* ROTATE */

#define EFSYS_HAS_ROTL_DWORD	0

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_COMMON_EFSYS_H */
