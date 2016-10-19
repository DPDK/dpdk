/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __BCM_OSAL_H
#define __BCM_OSAL_H

#include <rte_byteorder.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_memcpy.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_ether.h>

/* Forward declaration */
struct ecore_dev;
struct ecore_hwfn;
struct ecore_vf_acquire_sw_info;
struct vf_pf_resc_request;
enum ecore_mcp_protocol_type;
union ecore_mcp_protocol_stats;

void qed_link_update(struct ecore_hwfn *hwfn);

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#undef __BIG_ENDIAN
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN
#endif
#else
#undef __LITTLE_ENDIAN
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN
#endif
#endif

#define OSAL_WARN(arg1, arg2, arg3, ...) (0)

/* Memory Types */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int16_t s16;
typedef int32_t s32;

typedef u16 __le16;
typedef u32 __le32;
typedef u32 OSAL_BE32;

#define osal_uintptr_t uintptr_t

typedef phys_addr_t dma_addr_t;

typedef rte_spinlock_t osal_spinlock_t;

typedef void *osal_dpc_t;

typedef size_t osal_size_t;

typedef intptr_t osal_int_ptr_t;

typedef int bool;
#define true 1
#define false 0

#define nothing do {} while (0)

/* Delays */

#define DELAY(x) rte_delay_us(x)
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000 * (x))
#define OSAL_UDELAY(time) usec_delay(time)
#define OSAL_MSLEEP(time) msec_delay(time)

/* Memory allocations and deallocations */

#define OSAL_NULL ((void *)0)
#define OSAL_ALLOC(dev, GFP, size) rte_malloc("qede", size, 0)
#define OSAL_ZALLOC(dev, GFP, size) rte_zmalloc("qede", size, 0)
#define OSAL_CALLOC(dev, GFP, num, size) rte_calloc("qede", num, size, 0)
#define OSAL_VALLOC(dev, size) rte_malloc("qede", size, 0)
#define OSAL_FREE(dev, memory) rte_free((void *)memory)
#define OSAL_VFREE(dev, memory) OSAL_FREE(dev, memory)
#define OSAL_MEM_ZERO(mem, size) bzero(mem, size)
#define OSAL_MEMCPY(dst, src, size) rte_memcpy(dst, src, size)
#define OSAL_MEMCMP(s1, s2, size) memcmp(s1, s2, size)
#define OSAL_MEMSET(dst, val, length) \
	memset(dst, val, length)

void *osal_dma_alloc_coherent(struct ecore_dev *, dma_addr_t *, size_t);

void *osal_dma_alloc_coherent_aligned(struct ecore_dev *, dma_addr_t *,
				      size_t, int);

#define OSAL_DMA_ALLOC_COHERENT(dev, phys, size) \
	osal_dma_alloc_coherent(dev, phys, size)

#define OSAL_DMA_ALLOC_COHERENT_ALIGNED(dev, phys, size, align) \
	osal_dma_alloc_coherent_aligned(dev, phys, size, align)

/* TODO: */
#define OSAL_DMA_FREE_COHERENT(dev, virt, phys, size) nothing

/* HW reads/writes */

#define DIRECT_REG_RD(_dev, _reg_addr) \
	(*((volatile u32 *) (_reg_addr)))

#define REG_RD(_p_hwfn, _reg_offset) \
	DIRECT_REG_RD(_p_hwfn,		\
			((u8 *)(uintptr_t)(_p_hwfn->regview) + (_reg_offset)))

#define DIRECT_REG_WR16(_reg_addr, _val) \
	(*((volatile u16 *)(_reg_addr)) = _val)

#define DIRECT_REG_WR(_dev, _reg_addr, _val) \
	(*((volatile u32 *)(_reg_addr)) = _val)

#define REG_WR(_p_hwfn, _reg_offset, _val) \
	DIRECT_REG_WR(NULL,  \
	((u8 *)((uintptr_t)(_p_hwfn->regview)) + (_reg_offset)), (u32)_val)

#define REG_WR16(_p_hwfn, _reg_offset, _val) \
	DIRECT_REG_WR16(((u8 *)(uintptr_t)(_p_hwfn->regview) + \
			(_reg_offset)), (u16)_val)

#define DOORBELL(_p_hwfn, _db_addr, _val) \
	DIRECT_REG_WR(_p_hwfn, \
	     ((u8 *)(uintptr_t)(_p_hwfn->doorbells) + (_db_addr)), (u32)_val)

/* Mutexes */

typedef pthread_mutex_t osal_mutex_t;
#define OSAL_MUTEX_RELEASE(lock) pthread_mutex_unlock(lock)
#define OSAL_MUTEX_INIT(lock) pthread_mutex_init(lock, NULL)
#define OSAL_MUTEX_ACQUIRE(lock) pthread_mutex_lock(lock)
#define OSAL_MUTEX_ALLOC(hwfn, lock) nothing
#define OSAL_MUTEX_DEALLOC(lock) nothing

/* Spinlocks */

#define OSAL_SPIN_LOCK_INIT(lock) rte_spinlock_init(lock)
#define OSAL_SPIN_LOCK(lock) rte_spinlock_lock(lock)
#define OSAL_SPIN_UNLOCK(lock) rte_spinlock_unlock(lock)
#define OSAL_SPIN_LOCK_IRQSAVE(lock, flags) nothing
#define OSAL_SPIN_UNLOCK_IRQSAVE(lock, flags) nothing
#define OSAL_SPIN_LOCK_ALLOC(hwfn, lock) nothing
#define OSAL_SPIN_LOCK_DEALLOC(lock) nothing

/* DPC */

#define OSAL_DPC_ALLOC(hwfn) OSAL_ALLOC(hwfn, GFP, sizeof(osal_dpc_t))
#define OSAL_DPC_INIT(dpc, hwfn) nothing
#define OSAL_POLL_MODE_DPC(hwfn) nothing

/* Lists */

#define OSAL_LIST_SPLICE_INIT(new_list, list) nothing
#define OSAL_LIST_SPLICE_TAIL_INIT(new_list, list) nothing

typedef struct _osal_list_entry_t {
	struct _osal_list_entry_t *next, *prev;
} osal_list_entry_t;

typedef struct osal_list_t {
	osal_list_entry_t *head, *tail;
	unsigned long cnt;
} osal_list_t;

#define OSAL_LIST_INIT(list) \
	do {			\
		(list)->head = NULL;  \
		(list)->tail = NULL;  \
		(list)->cnt  = 0;	\
	} while (0)

#define OSAL_LIST_PUSH_HEAD(entry, list)		\
	do {						\
		(entry)->prev = (osal_list_entry_t *)0;		\
		(entry)->next = (list)->head;			\
		if ((list)->tail == (osal_list_entry_t *)0) {	\
			(list)->tail = (entry);			\
		} else {					\
			(list)->head->prev = (entry);		\
		}						\
		(list)->head = (entry);				\
		(list)->cnt++;					\
	} while (0)

#define OSAL_LIST_PUSH_TAIL(entry, list)	\
	do {					\
		(entry)->next = (osal_list_entry_t *)0; \
		(entry)->prev = (list)->tail;		\
		if ((list)->tail) {			\
			(list)->tail->next = (entry);	\
		} else {				\
			(list)->head = (entry);		\
		}					\
		(list)->tail = (entry);			\
		(list)->cnt++;				\
	} while (0)

#define OSAL_LIST_FIRST_ENTRY(list, type, field) \
	(type *)((list)->head)

#define OSAL_LIST_REMOVE_ENTRY(entry, list)			\
	do {							\
		if ((list)->head == (entry)) {				\
			if ((list)->head) {				\
				(list)->head = (list)->head->next;	\
			if ((list)->head) {				\
				(list)->head->prev = (osal_list_entry_t *)0;\
			} else {					\
				(list)->tail = (osal_list_entry_t *)0;	\
			}						\
			(list)->cnt--;					\
			}						\
		} else if ((list)->tail == (entry)) {			\
			if ((list)->tail) {				\
				(list)->tail = (list)->tail->prev;	\
			if ((list)->tail) {				\
				(list)->tail->next = (osal_list_entry_t *)0;\
			} else {					\
				(list)->head = (osal_list_entry_t *)0;	\
			}						\
			(list)->cnt--;					\
			}						\
		} else {						\
			(entry)->prev->next = (entry)->next;		\
			(entry)->next->prev = (entry)->prev;		\
			(list)->cnt--;					\
		}							\
	} while (0)

#define OSAL_LIST_IS_EMPTY(list) \
	((list)->cnt == 0)

#define OSAL_LIST_NEXT(entry, field, type) \
	(type *)((&((entry)->field))->next)

/* TODO: Check field, type order */

#define OSAL_LIST_FOR_EACH_ENTRY(entry, list, field, type) \
	for (entry = OSAL_LIST_FIRST_ENTRY(list, type, field); \
		entry;						\
		entry = OSAL_LIST_NEXT(entry, field, type))

#define OSAL_LIST_FOR_EACH_ENTRY_SAFE(entry, tmp_entry, list, field, type) \
	 for (entry = OSAL_LIST_FIRST_ENTRY(list, type, field),	\
	  tmp_entry = (entry) ? OSAL_LIST_NEXT(entry, field, type) : NULL;    \
	  entry != NULL;						\
	  entry = (type *)tmp_entry,					 \
	  tmp_entry = (entry) ? OSAL_LIST_NEXT(entry, field, type) : NULL)

/* TODO: OSAL_LIST_INSERT_ENTRY_AFTER */
#define OSAL_LIST_INSERT_ENTRY_AFTER(new_entry, entry, list) \
	OSAL_LIST_PUSH_HEAD(new_entry, list)

/* PCI config space */

#define OSAL_PCI_READ_CONFIG_BYTE(dev, address, dst) nothing
#define OSAL_PCI_READ_CONFIG_WORD(dev, address, dst) nothing
#define OSAL_PCI_READ_CONFIG_DWORD(dev, address, dst) nothing
#define OSAL_PCI_FIND_EXT_CAPABILITY(dev, pcie_id) 0
#define OSAL_PCI_FIND_CAPABILITY(dev, pcie_id) 0
#define OSAL_PCI_WRITE_CONFIG_WORD(dev, address, val) nothing
#define OSAL_BAR_SIZE(dev, bar_id) 0

/* Barriers */

#define OSAL_MMIOWB(dev)		rte_wmb()
#define OSAL_BARRIER(dev)		rte_compiler_barrier()
#define OSAL_SMP_RMB(dev)		rte_rmb()
#define OSAL_SMP_WMB(dev)		rte_wmb()
#define OSAL_RMB(dev)			rte_rmb()
#define OSAL_WMB(dev)			rte_wmb()
#define OSAL_DMA_SYNC(dev, addr, length, is_post) nothing

#define OSAL_BITS_PER_BYTE		(8)
#define OSAL_BITS_PER_UL	(sizeof(unsigned long) * OSAL_BITS_PER_BYTE)
#define OSAL_BITS_PER_UL_MASK		(OSAL_BITS_PER_UL - 1)

/* Bitops */
void qede_set_bit(u32, unsigned long *);
#define OSAL_SET_BIT(bit, bitmap) \
	qede_set_bit(bit, bitmap)

void qede_clr_bit(u32, unsigned long *);
#define OSAL_CLEAR_BIT(bit, bitmap) \
	qede_clr_bit(bit, bitmap)

bool qede_test_bit(u32, unsigned long *);
#define OSAL_TEST_BIT(bit, bitmap) \
	qede_test_bit(bit, bitmap)

u32 qede_find_first_bit(unsigned long *, u32);
#define OSAL_FIND_FIRST_BIT(bitmap, length) \
	qede_find_first_bit(bitmap, length)

u32 qede_find_first_zero_bit(unsigned long *, u32);
#define OSAL_FIND_FIRST_ZERO_BIT(bitmap, length) \
	qede_find_first_zero_bit(bitmap, length)

#define OSAL_BUILD_BUG_ON(cond)		nothing
#define ETH_ALEN			ETHER_ADDR_LEN

#define OSAL_LINK_UPDATE(hwfn) qed_link_update(hwfn)
#define OSAL_DCBX_AEN(hwfn, mib_type) nothing

/* SR-IOV channel */

#define OSAL_VF_FLR_UPDATE(hwfn) nothing
#define OSAL_VF_SEND_MSG2PF(dev, done, msg, reply_addr, msg_size, reply_size) 0
#define OSAL_VF_CQE_COMPLETION(_dev_p, _cqe, _protocol)	(0)
#define OSAL_PF_VF_MSG(hwfn, vfid) 0
#define OSAL_IOV_CHK_UCAST(hwfn, vfid, params) 0
#define OSAL_IOV_POST_START_VPORT(hwfn, vf, vport_id, opaque_fid) nothing
#define OSAL_IOV_VF_ACQUIRE(hwfn, vfid) 0
#define OSAL_IOV_VF_CLEANUP(hwfn, vfid) nothing
#define OSAL_IOV_VF_VPORT_UPDATE(hwfn, vfid, p_params, p_mask) 0
#define OSAL_VF_UPDATE_ACQUIRE_RESC_RESP(_dev_p, _resc_resp) 0
#define OSAL_IOV_GET_OS_TYPE() 0
#define OSAL_IOV_VF_MSG_TYPE(hwfn, vfid, vf_msg_type) 0
#define OSAL_IOV_PF_RESP_TYPE(hwfn, vfid, pf_resp_type) 0

u32 qede_unzip_data(struct ecore_hwfn *p_hwfn, u32 input_len,
		   u8 *input_buf, u32 max_size, u8 *unzip_buf);
void qede_vf_fill_driver_data(struct ecore_hwfn *, struct vf_pf_resc_request *,
			      struct ecore_vf_acquire_sw_info *);
#define OSAL_VF_FILL_ACQUIRE_RESC_REQ(_dev_p, _resc_req, _os_info) \
	qede_vf_fill_driver_data(_dev_p, _resc_req, _os_info)

#define OSAL_UNZIP_DATA(p_hwfn, input_len, buf, max_size, unzip_buf) \
	qede_unzip_data(p_hwfn, input_len, buf, max_size, unzip_buf)

/* TODO: */
#define OSAL_SCHEDULE_RECOVERY_HANDLER(hwfn) nothing
#define OSAL_HW_ERROR_OCCURRED(hwfn, err_type) nothing

#define OSAL_NVM_IS_ACCESS_ENABLED(hwfn) (1)
#define OSAL_NUM_ACTIVE_CPU()	0

/* Utility functions */

#define RTE_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_UP(size, to_what) RTE_DIV_ROUND_UP(size, to_what)
#define RTE_ROUNDUP(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define ROUNDUP(value, to_what) RTE_ROUNDUP((value), (to_what))

unsigned long qede_log2_align(unsigned long n);
#define OSAL_ROUNDUP_POW_OF_TWO(val) \
	qede_log2_align(val)

u32 qede_osal_log2(u32);
#define OSAL_LOG2(val) \
	qede_osal_log2(val)

#define PRINT(format, ...) printf
#define PRINT_ERR(format, ...) PRINT

#define OFFSETOF(str, field) __builtin_offsetof(str, field)
#define OSAL_ASSERT(is_assert) assert(is_assert)
#define OSAL_BEFORE_PF_START(file, engine) nothing
#define OSAL_AFTER_PF_STOP(file, engine) nothing

/* Endian macros */
#define OSAL_CPU_TO_BE32(val) rte_cpu_to_be_32(val)
#define OSAL_BE32_TO_CPU(val) rte_be_to_cpu_32(val)
#define OSAL_CPU_TO_LE32(val) rte_cpu_to_le_32(val)
#define OSAL_CPU_TO_LE16(val) rte_cpu_to_le_16(val)
#define OSAL_LE32_TO_CPU(val) rte_le_to_cpu_32(val)
#define OSAL_LE16_TO_CPU(val) rte_le_to_cpu_16(val)
#define OSAL_CPU_TO_BE64(val) rte_cpu_to_be_64(val)

#define OSAL_ARRAY_SIZE(arr) RTE_DIM(arr)
#define OSAL_SPRINTF(name, pattern, ...) \
	sprintf(name, pattern, ##__VA_ARGS__)
#define OSAL_SNPRINTF(buf, size, format, ...) \
	snprintf(buf, size, format, ##__VA_ARGS__)
#define OSAL_STRLEN(string) strlen(string)
#define OSAL_STRCPY(dst, string) strcpy(dst, string)
#define OSAL_STRNCPY(dst, string, len) strncpy(dst, string, len)
#define OSAL_STRCMP(str1, str2) strcmp(str1, str2)

#define OSAL_INLINE inline
#define OSAL_REG_ADDR(_p_hwfn, _offset) \
		(void *)((u8 *)(uintptr_t)(_p_hwfn->regview) + (_offset))
#define OSAL_PAGE_SIZE 4096
#define OSAL_IOMEM volatile
#define OSAL_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define OSAL_MIN_T(type, __min1, __min2)	\
	((type)(__min1) < (type)(__min2) ? (type)(__min1) : (type)(__min2))
#define OSAL_MAX_T(type, __max1, __max2)	\
	((type)(__max1) > (type)(__max2) ? (type)(__max1) : (type)(__max2))

void qede_get_mcp_proto_stats(struct ecore_dev *, enum ecore_mcp_protocol_type,
			      union ecore_mcp_protocol_stats *);
#define	OSAL_GET_PROTOCOL_STATS(dev, type, stats) \
	qede_get_mcp_proto_stats(dev, type, stats)

#define	OSAL_SLOWPATH_IRQ_REQ(p_hwfn) (0)

#endif /* __BCM_OSAL_H */
