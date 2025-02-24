/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_PLATFORM_H_
#define _ROC_PLATFORM_H_

#include <rte_compat.h>
#include <rte_alarm.h>
#include <rte_bitmap.h>
#include <bus_pci_driver.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_io.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_seqcount.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>
#include <rte_telemetry.h>

#include "eal_filesystem.h"

#include "roc_bits.h"

#if defined(__ARM_FEATURE_SVE)
#define PLT_CPU_FEATURE_PREAMBLE                                               \
	".arch_extension crc\n"                                                \
	".arch_extension lse\n"                                                \
	".arch_extension sve\n"
#else
#define PLT_CPU_FEATURE_PREAMBLE                                               \
	".arch_extension crc\n"                                                \
	".arch_extension lse\n"
#endif

#define PLT_ASSERT		 RTE_ASSERT
#define PLT_VERIFY		 RTE_VERIFY
#define PLT_MEMZONE_NAMESIZE	 RTE_MEMZONE_NAMESIZE
#define PLT_PTR_ADD		 RTE_PTR_ADD
#define PLT_PTR_SUB		 RTE_PTR_SUB
#define PLT_PTR_DIFF		 RTE_PTR_DIFF
#define PLT_MAX_RXTX_INTR_VEC_ID RTE_MAX_RXTX_INTR_VEC_ID
#define PLT_INTR_VEC_RXTX_OFFSET RTE_INTR_VEC_RXTX_OFFSET
#define PLT_MIN			 RTE_MIN
#define PLT_MAX			 RTE_MAX
#define PLT_DIM			 RTE_DIM
#define PLT_SET_USED		 RTE_SET_USED
#define PLT_SWAP		 RTE_SWAP
#define PLT_STATIC_ASSERT(s)	 _Static_assert(s, #s)
#define PLT_ALIGN		 RTE_ALIGN
#define PLT_ALIGN_MUL_CEIL	 RTE_ALIGN_MUL_CEIL
#define PLT_MODEL_MZ_NAME	 "roc_model_mz"
#define PLT_CACHE_LINE_SIZE	 RTE_CACHE_LINE_SIZE
#define BITMASK_ULL		 GENMASK_ULL
#define PLT_ALIGN_CEIL		 RTE_ALIGN_CEIL
#define PLT_ALIGN_FLOOR		 RTE_ALIGN_FLOOR
#define PLT_INIT		 RTE_INIT
#define PLT_MAX_ETHPORTS	 RTE_MAX_ETHPORTS
#define PLT_TAILQ_FOREACH_SAFE	 RTE_TAILQ_FOREACH_SAFE

#ifndef PLT_ETHER_ADDR_LEN
#define PLT_ETHER_ADDR_LEN RTE_ETHER_ADDR_LEN
#endif

#define PLT_DISABLE_TEMPLATE_FUNC 0
#if PLT_DISABLE_TEMPLATE_FUNC
#ifndef CNXK_DIS_TMPLT_FUNC
#define CNXK_DIS_TMPLT_FUNC
#endif
#endif

/* Cast to specific datatypes */
#define PLT_PTR_CAST(val) ((void *)(val))
#define PLT_U64_CAST(val) ((uint64_t)(val))
#define PLT_U32_CAST(val) ((uint32_t)(val))
#define PLT_U16_CAST(val) ((uint16_t)(val))

/* Add / Sub pointer with scalar and cast to uint64_t */
#define PLT_PTR_ADD_U64_CAST(__ptr, __x) PLT_U64_CAST(PLT_PTR_ADD(__ptr, __x))
#define PLT_PTR_SUB_U64_CAST(__ptr, __x) PLT_U64_CAST(PLT_PTR_SUB(__ptr, __x))

/** Divide ceil */
#define PLT_DIV_CEIL(x, y)			\
	__extension__ ({			\
		__typeof(x) __x = x;		\
		__typeof(y) __y = y;		\
		(__x + __y - 1) / __y;		\
	})

#define __plt_cache_aligned __rte_cache_aligned
#define __plt_always_inline __rte_always_inline
#define __plt_packed_begin  __rte_packed_begin
#define __plt_packed_end    __rte_packed_end
#define __plt_unused	    __rte_unused
#define __roc_api	    __rte_internal
#define plt_iova_t	    rte_iova_t

#define plt_pci_addr		    rte_pci_addr
#define plt_pci_device		    rte_pci_device
#define plt_pci_read_config	    rte_pci_read_config
#define plt_pci_find_ext_capability rte_pci_find_ext_capability
#define plt_sysfs_value_parse	    eal_parse_sysfs_value

#define plt_log2_u32	 rte_log2_u32
#define plt_cpu_to_be_16 rte_cpu_to_be_16
#define plt_be_to_cpu_16 rte_be_to_cpu_16
#define plt_cpu_to_be_32 rte_cpu_to_be_32
#define plt_be_to_cpu_32 rte_be_to_cpu_32
#define plt_cpu_to_be_64 rte_cpu_to_be_64
#define plt_be_to_cpu_64 rte_be_to_cpu_64

#define __plt_aligned	    __rte_aligned
#define plt_align32pow2	    rte_align32pow2
#define plt_align64pow2	    rte_align64pow2
#define plt_align32prevpow2 rte_align32prevpow2

#define plt_bitmap			rte_bitmap
#define plt_bitmap_init			rte_bitmap_init
#define plt_bitmap_reset		rte_bitmap_reset
#define plt_bitmap_free			rte_bitmap_free
#define plt_bitmap_clear		rte_bitmap_clear
#define plt_bitmap_set			rte_bitmap_set
#define plt_bitmap_get			rte_bitmap_get
#define plt_bitmap_scan_init		__rte_bitmap_scan_init
#define plt_bitmap_scan			rte_bitmap_scan
#define plt_bitmap_get_memory_footprint rte_bitmap_get_memory_footprint

#define plt_spinlock_t	     rte_spinlock_t
#define plt_spinlock_init    rte_spinlock_init
#define plt_spinlock_lock    rte_spinlock_lock
#define plt_spinlock_unlock  rte_spinlock_unlock
#define plt_spinlock_trylock rte_spinlock_trylock

#define plt_seqcount_t			rte_seqcount_t
#define plt_seqcount_init		rte_seqcount_init
#define plt_seqcount_read_begin		rte_seqcount_read_begin
#define plt_seqcount_read_retry		rte_seqcount_read_retry
#define plt_seqcount_write_begin	rte_seqcount_write_begin
#define plt_seqcount_write_end		rte_seqcount_write_end

#define plt_thread_t		     rte_thread_t
#define plt_intr_callback_register   rte_intr_callback_register
#define plt_intr_callback_unregister rte_intr_callback_unregister
#define plt_intr_disable	     rte_intr_disable
#define plt_thread_is_intr	     rte_thread_is_intr
#define plt_intr_callback_fn	     rte_intr_callback_fn
#define plt_thread_create_control    rte_thread_create_internal_control
#define plt_thread_join	             rte_thread_join

static inline bool
plt_thread_is_valid(plt_thread_t thr)
{
	return thr.opaque_id ? true : false;
}

#define plt_intr_efd_counter_size_get	rte_intr_efd_counter_size_get
#define plt_intr_efd_counter_size_set	rte_intr_efd_counter_size_set
#define plt_intr_vec_list_index_get	rte_intr_vec_list_index_get
#define plt_intr_vec_list_index_set	rte_intr_vec_list_index_set
#define plt_intr_vec_list_alloc		rte_intr_vec_list_alloc
#define plt_intr_vec_list_free		rte_intr_vec_list_free
#define plt_intr_fd_set			rte_intr_fd_set
#define plt_intr_fd_get			rte_intr_fd_get
#define plt_intr_dev_fd_get		rte_intr_dev_fd_get
#define plt_intr_dev_fd_set		rte_intr_dev_fd_set
#define plt_intr_type_get		rte_intr_type_get
#define plt_intr_type_set		rte_intr_type_set
#define plt_intr_instance_alloc		rte_intr_instance_alloc
#define plt_intr_instance_dup		rte_intr_instance_dup
#define plt_intr_instance_free		rte_intr_instance_free
#define plt_intr_event_list_update	rte_intr_event_list_update
#define plt_intr_max_intr_get		rte_intr_max_intr_get
#define plt_intr_max_intr_set		rte_intr_max_intr_set
#define plt_intr_nb_efd_get		rte_intr_nb_efd_get
#define plt_intr_nb_efd_set		rte_intr_nb_efd_set
#define plt_intr_nb_intr_get		rte_intr_nb_intr_get
#define plt_intr_nb_intr_set		rte_intr_nb_intr_set
#define plt_intr_efds_index_get		rte_intr_efds_index_get
#define plt_intr_efds_index_set		rte_intr_efds_index_set
#define plt_intr_elist_index_get	rte_intr_elist_index_get
#define plt_intr_elist_index_set	rte_intr_elist_index_set
#define plt_is_aligned			rte_is_aligned

#define plt_alarm_set	 rte_eal_alarm_set
#define plt_alarm_cancel rte_eal_alarm_cancel

#define plt_intr_handle rte_intr_handle

#define plt_zmalloc(sz, align) rte_zmalloc("cnxk", sz, align)
#define plt_realloc	       rte_realloc
#define plt_free	       rte_free

#define plt_read64(addr) rte_read64_relaxed((volatile void *)(addr))
#define plt_write64(val, addr)                                                 \
	rte_write64_relaxed((val), (volatile void *)(addr))

#define plt_read32(addr) rte_read32_relaxed((volatile void *)(addr))
#define plt_write32(val, addr)                                                 \
	rte_write32_relaxed((val), (volatile void *)(addr))

#define plt_wmb()		rte_wmb()
#define plt_rmb()		rte_rmb()
#define plt_io_wmb()		rte_io_wmb()
#define plt_io_rmb()		rte_io_rmb()
#define plt_atomic_thread_fence rte_atomic_thread_fence

#define plt_bit_relaxed_get32   rte_bit_relaxed_get32
#define plt_bit_relaxed_set32   rte_bit_relaxed_set32
#define plt_bit_relaxed_clear32 rte_bit_relaxed_clear32

#define plt_bit_relaxed_get64   rte_bit_relaxed_get64
#define plt_bit_relaxed_set64   rte_bit_relaxed_set64
#define plt_bit_relaxed_clear64 rte_bit_relaxed_clear64

#define plt_popcount32		rte_popcount32
#define plt_popcount64		rte_popcount64
#define plt_clz32		rte_clz32
#define plt_ctz64		rte_ctz64

#define plt_mmap       mmap
#define PLT_PROT_READ  PROT_READ
#define PLT_PROT_WRITE PROT_WRITE
#define PLT_MAP_SHARED MAP_SHARED

#define plt_memzone	   rte_memzone
#define plt_memzone_lookup rte_memzone_lookup
#define plt_memzone_reserve_cache_align(name, sz)                              \
	rte_memzone_reserve_aligned(name, sz, 0, 0, RTE_CACHE_LINE_SIZE)
#define plt_memzone_free rte_memzone_free
#define plt_memzone_reserve_aligned(name, len, flags, align)                   \
	rte_memzone_reserve_aligned((name), (len), 0, (flags), (align))

#define plt_tsc_hz     rte_get_tsc_hz
#define plt_tsc_cycles rte_get_tsc_cycles
#define plt_delay_ms   rte_delay_ms
#define plt_delay_us   rte_delay_us

#define plt_lcore_id rte_lcore_id

#define plt_strlcpy rte_strlcpy

#define PLT_TEL_INT_VAL              RTE_TEL_INT_VAL
#define PLT_TEL_STRING_VAL           RTE_TEL_STRING_VAL
#define plt_tel_data                 rte_tel_data
#define plt_tel_data_start_array     rte_tel_data_start_array
#define plt_tel_data_add_array_int   rte_tel_data_add_array_int
#define plt_tel_data_add_array_string rte_tel_data_add_array_string
#define plt_tel_data_start_dict      rte_tel_data_start_dict
#define plt_tel_data_add_dict_int    rte_tel_data_add_dict_int
#define plt_tel_data_add_dict_ptr(d, n, v)			\
	rte_tel_data_add_dict_uint(d, n, (uint64_t)v)
#define plt_tel_data_add_dict_string rte_tel_data_add_dict_string
#define plt_tel_data_add_dict_u64    rte_tel_data_add_dict_uint
#define plt_telemetry_register_cmd   rte_telemetry_register_cmd
#define __plt_atomic __rte_atomic

/* Log */
extern int cnxk_logtype_base;
#define RTE_LOGTYPE_base cnxk_logtype_base
extern int cnxk_logtype_mbox;
#define RTE_LOGTYPE_mbox cnxk_logtype_mbox
extern int cnxk_logtype_cpt;
#define RTE_LOGTYPE_cpt cnxk_logtype_cpt
extern int cnxk_logtype_ml;
#define RTE_LOGTYPE_ml cnxk_logtype_ml
extern int cnxk_logtype_npa;
#define RTE_LOGTYPE_npa cnxk_logtype_npa
extern int cnxk_logtype_nix;
#define RTE_LOGTYPE_nix cnxk_logtype_nix
extern int cnxk_logtype_npc;
#define RTE_LOGTYPE_npc cnxk_logtype_npc
extern int cnxk_logtype_sso;
#define RTE_LOGTYPE_sso cnxk_logtype_sso
extern int cnxk_logtype_tim;
#define RTE_LOGTYPE_tim cnxk_logtype_tim
extern int cnxk_logtype_tm;
#define RTE_LOGTYPE_tm cnxk_logtype_tm
extern int cnxk_logtype_ree;
#define RTE_LOGTYPE_ree cnxk_logtype_ree
extern int cnxk_logtype_dpi;
#define RTE_LOGTYPE_dpi cnxk_logtype_dpi
extern int cnxk_logtype_rep;
#define RTE_LOGTYPE_rep cnxk_logtype_rep
extern int cnxk_logtype_esw;
#define RTE_LOGTYPE_esw cnxk_logtype_esw

#define RTE_LOGTYPE_CNXK cnxk_logtype_base

#define plt_err(...) \
	RTE_LOG_LINE_PREFIX(ERR, CNXK, "%s():%u ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)
#define plt_info(...) RTE_LOG_LINE(INFO, CNXK, __VA_ARGS__)
#define plt_warn(...) RTE_LOG_LINE(WARNING, CNXK, __VA_ARGS__)
#define plt_print(...) RTE_LOG_LINE(INFO, CNXK, __VA_ARGS__)
#define plt_dump(fmt, ...)      fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define plt_dump_no_nl(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

/**
 * Log debug message if given subsystem logging is enabled.
 */
#define plt_dbg(subsystem, ...) \
	RTE_LOG_LINE_PREFIX(DEBUG, subsystem, "%s():%u ", __func__ RTE_LOG_COMMA __LINE__, \
		__VA_ARGS__)

#define plt_base_dbg(fmt, ...)	plt_dbg(base, fmt, ##__VA_ARGS__)
#define plt_cpt_dbg(fmt, ...)	plt_dbg(cpt, fmt, ##__VA_ARGS__)
#define plt_mbox_dbg(fmt, ...)	plt_dbg(mbox, fmt, ##__VA_ARGS__)
#define plt_ml_dbg(fmt, ...)	plt_dbg(ml, fmt, ##__VA_ARGS__)
#define plt_npa_dbg(fmt, ...)	plt_dbg(npa, fmt, ##__VA_ARGS__)
#define plt_nix_dbg(fmt, ...)	plt_dbg(nix, fmt, ##__VA_ARGS__)
#define plt_npc_dbg(fmt, ...)	plt_dbg(npc, fmt, ##__VA_ARGS__)
#define plt_sso_dbg(fmt, ...)	plt_dbg(sso, fmt, ##__VA_ARGS__)
#define plt_tim_dbg(fmt, ...)	plt_dbg(tim, fmt, ##__VA_ARGS__)
#define plt_tm_dbg(fmt, ...)	plt_dbg(tm, fmt, ##__VA_ARGS__)
#define plt_ree_dbg(fmt, ...)	plt_dbg(ree, fmt, ##__VA_ARGS__)
#define plt_dpi_dbg(fmt, ...)	plt_dbg(dpi, fmt, ##__VA_ARGS__)
#define plt_rep_dbg(fmt, ...)	plt_dbg(rep, fmt, ##__VA_ARGS__)
#define plt_esw_dbg(fmt, ...)	plt_dbg(esw, fmt, ##__VA_ARGS__)

/* Datapath logs */
#define plt_dp_err(...) \
	RTE_LOG_DP_LINE_PREFIX(ERR, CNXK, "%s():%u ", __func__ RTE_LOG_COMMA __LINE__, \
		__VA_ARGS__)
#define plt_dp_info(...) \
	RTE_LOG_DP_LINE_PREFIX(INFO, CNXK, "%s():%u ", __func__ RTE_LOG_COMMA __LINE__, \
		__VA_ARGS__)
#define plt_dp_dbg(...) \
	RTE_LOG_DP_LINE_PREFIX(DEBUG, CNXK, "%s():%u ", __func__ RTE_LOG_COMMA __LINE__, \
		__VA_ARGS__)

#ifdef __cplusplus
#define CNXK_PCI_ID(subsystem_dev, dev)                                        \
{                                                                      \
	RTE_CLASS_ANY_ID, PCI_VENDOR_ID_CAVIUM, (dev), RTE_PCI_ANY_ID, \
	(subsystem_dev),                                       \
}
#else
#define CNXK_PCI_ID(subsystem_dev, dev)                                        \
{                                                                      \
	.class_id = RTE_CLASS_ANY_ID,                                  \
	.vendor_id = PCI_VENDOR_ID_CAVIUM, .device_id = (dev),         \
	.subsystem_vendor_id = RTE_PCI_ANY_ID,                         \
	.subsystem_device_id = (subsystem_dev),                        \
}
#endif

int plt_irq_register(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
		     unsigned int vec);
void plt_irq_unregister(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb, void *data,
			unsigned int vec);
int plt_irq_reconfigure(struct plt_intr_handle *intr_handle, uint16_t max_intr);
int plt_irq_disable(struct plt_intr_handle *intr_handle);

/* Device memory does not support unaligned access, instruct compiler to
 * not optimize the memory access when working with mailbox memory.
 */
#ifndef __io
#define __io volatile
#endif

__rte_internal
int roc_plt_init(void);

__rte_internal
uint16_t roc_plt_control_lmt_id_get(void);
__rte_internal
uint16_t roc_plt_lmt_validate(void);

/* Init callbacks */
typedef int (*roc_plt_init_cb_t)(void);
int __roc_api roc_plt_init_cb_register(roc_plt_init_cb_t cb);

static inline const void *
plt_lmt_region_reserve_aligned(const char *name, size_t len, uint32_t align)
{
	/* To ensure returned memory is physically contiguous, bounding
	 * the start and end address in 2M range.
	 */
	return rte_memzone_reserve_bounded(name, len, SOCKET_ID_ANY,
					   RTE_MEMZONE_IOVA_CONTIG,
					   align, RTE_PGSIZE_2M);
}

#endif /* _ROC_PLATFORM_H_ */
