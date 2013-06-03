/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its 
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
 * 
 */

#ifndef __INCLUDE_RTE_BITMAP_H__
#define __INCLUDE_RTE_BITMAP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Bitmap
 *
 * The bitmap component provides a mechanism to manage large arrays of bits
 * through bit get/set/clear and bit array scan operations.
 *
 * The bitmap scan operation is optimized for 64-bit CPUs using 64-byte cache
 * lines. The bitmap is hierarchically organized using two arrays (array1 and
 * array2), with each bit in array1 being associated with a full cache line
 * (512 bits) of bitmap bits, which are stored in array2: the bit in array1 is
 * set only when there is at least one bit set within its associated array2
 * bits, otherwise the bit in array1 is cleared. The read and write operations
 * for array1 and array2 are always done in slabs of 64 bits.
 *
 * This bitmap is not thread safe. For lock free operation on a specific bitmap
 * instance, a single writer thread performing bit set/clear operations is
 * allowed, only the writer thread can do bitmap scan operations, while there 
 * can be several reader threads performing bit get operations in parallel with
 * the writer thread. When the use of locking primitives is acceptable, the 
 * serialization of the bit set/clear and bitmap scan operations needs to be
 * enforced by the caller, while the bit get operation does not require locking
 * the bitmap.
 *
 ***/
 
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_branch_prediction.h>
#include <rte_prefetch.h>

#ifndef RTE_BITMAP_OPTIMIZATIONS
#define RTE_BITMAP_OPTIMIZATIONS		         1
#endif
#if RTE_BITMAP_OPTIMIZATIONS
#include <tmmintrin.h>
#endif

/** Number of elements in array1. Each element in array1 is a 64-bit slab. */
#ifndef RTE_BITMAP_ARRAY1_SIZE
#define RTE_BITMAP_ARRAY1_SIZE                   16
#endif

/* Slab */
#define RTE_BITMAP_SLAB_BIT_SIZE                 64
#define RTE_BITMAP_SLAB_BIT_SIZE_LOG2            6
#define RTE_BITMAP_SLAB_BIT_MASK                 (RTE_BITMAP_SLAB_BIT_SIZE - 1)

/* Cache line (CL) */
#define RTE_BITMAP_CL_BIT_SIZE                   (CACHE_LINE_SIZE * 8)
#define RTE_BITMAP_CL_BIT_SIZE_LOG2              9
#define RTE_BITMAP_CL_BIT_MASK                   (RTE_BITMAP_CL_BIT_SIZE - 1)

#define RTE_BITMAP_CL_SLAB_SIZE                  (RTE_BITMAP_CL_BIT_SIZE / RTE_BITMAP_SLAB_BIT_SIZE)
#define RTE_BITMAP_CL_SLAB_SIZE_LOG2             3
#define RTE_BITMAP_CL_SLAB_MASK                  (RTE_BITMAP_CL_SLAB_SIZE - 1)

/** Bitmap data structure */
struct rte_bitmap {
	uint64_t array1[RTE_BITMAP_ARRAY1_SIZE]; /**< Bitmap array1 */
	uint64_t *array2;                        /**< Bitmap array2 */
	uint32_t array1_size;                    /**< Number of 64-bit slabs in array1 that are actually used */
	uint32_t array2_size;                    /**< Number of 64-bit slabs in array2 */
	
	/* Context for the "scan next" operation */
	uint32_t index1;  /**< Bitmap scan: Index of current array1 slab */
	uint32_t offset1; /**< Bitmap scan: Offset of current bit within current array1 slab */
	uint32_t index2;  /**< Bitmap scan: Index of current array2 slab */
	uint32_t go2;     /**< Bitmap scan: Go/stop condition for current array2 cache line */
} __rte_cache_aligned;

static inline void
__rte_bitmap_index1_inc(struct rte_bitmap *bmp)
{
	bmp->index1 = (bmp->index1 + 1) & (RTE_BITMAP_ARRAY1_SIZE - 1);
}

static inline uint64_t
__rte_bitmap_mask1_get(struct rte_bitmap *bmp)
{
	return ((~1lu) << bmp->offset1);
}

static inline void
__rte_bitmap_index2_set(struct rte_bitmap *bmp)
{
	bmp->index2 = (((bmp->index1 << RTE_BITMAP_SLAB_BIT_SIZE_LOG2) + bmp->offset1) << RTE_BITMAP_CL_SLAB_SIZE_LOG2);
}

#if RTE_BITMAP_OPTIMIZATIONS

static inline int 
rte_bsf64(uint64_t slab, uint32_t *pos)
{
	if (likely(slab == 0)) {
		return 0;
	}

	*pos = __builtin_ctzll(slab);
	return 1;
}

#else

static inline int 
rte_bsf64(uint64_t slab, uint32_t *pos)
{
	uint64_t mask;
	uint32_t i;
	
	if (likely(slab == 0)) {
		return 0;
	}

	for (i = 0, mask = 1; i < RTE_BITMAP_SLAB_BIT_SIZE; i ++, mask <<= 1) {
		if (unlikely(slab & mask)) {
			*pos = i;
			return 1;
		}
	}
	
	return 0;
}

#endif

static inline void
__rte_bitmap_scan_init(struct rte_bitmap *bmp)
{
	bmp->index1 = RTE_BITMAP_ARRAY1_SIZE - 1;
	bmp->offset1 = RTE_BITMAP_SLAB_BIT_SIZE - 1;
	__rte_bitmap_index2_set(bmp);
	bmp->index2 += RTE_BITMAP_CL_SLAB_SIZE;

	bmp->go2 = 0;
}

/**
 * Bitmap initialization
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param array2
 *   Base address of pre-allocated array2
 * @param n_bits
 *   Number of pre-allocated bits in array2. Must be non-zero and multiple of 512.
 * @return
 *   0 upon success, error code otherwise
 */
static inline int 
rte_bitmap_init(struct rte_bitmap *bmp, uint8_t *array2, uint32_t n_bits)
{
	uint32_t array1_size, array2_size;

	/* Check input arguments */
	if ((bmp == NULL) || 
	    (array2 == NULL) || (((uintptr_t) array2) & CACHE_LINE_MASK) ||
		(n_bits == 0) || (n_bits & RTE_BITMAP_CL_BIT_MASK)){
		return -1;
	}

	array2_size = n_bits / RTE_BITMAP_SLAB_BIT_SIZE;
	array1_size = ((n_bits / RTE_BITMAP_CL_BIT_SIZE) + (RTE_BITMAP_SLAB_BIT_SIZE - 1)) / RTE_BITMAP_SLAB_BIT_SIZE;
	if (array1_size > RTE_BITMAP_ARRAY1_SIZE){
		return -1;
	}
	
	/* Setup bitmap */
	memset(bmp, 0, sizeof(struct rte_bitmap));
	bmp->array2 = (uint64_t *) array2;
	bmp->array1_size = array1_size;
	bmp->array2_size = array2_size;
	__rte_bitmap_scan_init(bmp);
	
	return 0;
}

/**
 * Bitmap free
 *
 * @param bmp
 *   Handle to bitmap instance
 * @return
 *   0 upon success, error code otherwise
 */
static inline int
rte_bitmap_free(struct rte_bitmap *bmp)
{
	/* Check input arguments */
	if (bmp == NULL) {
		return -1;
	}
	
	return 0;
}

/**
 * Bitmap reset
 *
 * @param bmp
 *   Handle to bitmap instance
 */
static inline void
rte_bitmap_reset(struct rte_bitmap *bmp)
{
	memset(bmp->array1, 0, sizeof(bmp->array1));
	memset(bmp->array2, 0, bmp->array2_size * sizeof(uint64_t));
	__rte_bitmap_scan_init(bmp);
}

/**
 * Bitmap location prefetch into CPU L1 cache
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   Bit position
 * @return
 *   0 upon success, error code otherwise
 */
static inline void
rte_bitmap_prefetch0(struct rte_bitmap *bmp, uint32_t pos)
{
	uint64_t *slab2;
	uint32_t index2;
	
	index2 = pos >> RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
	slab2 = bmp->array2 + index2;
	rte_prefetch0((void *) slab2);
}

/**
 * Bitmap bit get
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   Bit position
 * @return
 *   0 when bit is cleared, non-zero when bit is set
 */
static inline uint64_t
rte_bitmap_get(struct rte_bitmap *bmp, uint32_t pos)
{
	uint64_t *slab2;
	uint32_t index2, offset2;
	
	index2 = pos >> RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
	offset2 = pos & RTE_BITMAP_SLAB_BIT_MASK;
	slab2 = bmp->array2 + index2;
	return ((*slab2) & (1lu << offset2));
}

/**
 * Bitmap bit set
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   Bit position
 */
static inline void
rte_bitmap_set(struct rte_bitmap *bmp, uint32_t pos)
{
	uint64_t *slab1, *slab2;
	uint32_t index1, index2, offset1, offset2;
	
	/* Set bit in array2 slab and set bit in array1 slab */
	index2 = pos >> RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
	offset2 = pos & RTE_BITMAP_SLAB_BIT_MASK;
	index1 = pos >> (RTE_BITMAP_SLAB_BIT_SIZE_LOG2 + RTE_BITMAP_CL_BIT_SIZE_LOG2);
	offset1 = (pos >> RTE_BITMAP_CL_BIT_SIZE_LOG2) & RTE_BITMAP_SLAB_BIT_MASK;
	slab2 = bmp->array2 + index2;
	slab1 = bmp->array1 + index1;
	
	*slab2 |= 1lu << offset2;
	*slab1 |= 1lu << offset1;
}

/**
 * Bitmap slab set
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   Bit position identifying the array2 slab
 * @param slab
 *   Value to be assigned to the 64-bit slab in array2
 */
static inline void
rte_bitmap_set_slab(struct rte_bitmap *bmp, uint32_t pos, uint64_t slab)
{
	uint64_t *slab1, *slab2;
	uint32_t index1, index2, offset1;
	
	/* Set bits in array2 slab and set bit in array1 slab */
	index2 = pos >> RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
	index1 = pos >> (RTE_BITMAP_SLAB_BIT_SIZE_LOG2 + RTE_BITMAP_CL_BIT_SIZE_LOG2);
	offset1 = (pos >> RTE_BITMAP_CL_BIT_SIZE_LOG2) & RTE_BITMAP_SLAB_BIT_MASK;
	slab2 = bmp->array2 + index2;
	slab1 = bmp->array1 + index1;
	
	*slab2 |= slab;
	*slab1 |= 1lu << offset1;
}

static inline uint64_t
__rte_bitmap_line_not_empty(uint64_t *slab2)
{
	uint64_t v1, v2, v3, v4;
	
	v1 = slab2[0] | slab2[1];
	v2 = slab2[2] | slab2[3];
	v3 = slab2[4] | slab2[5];
	v4 = slab2[6] | slab2[7];
	v1 |= v2;
	v3 |= v4;
	
	return (v1 | v3);
}

/**
 * Bitmap bit clear
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   Bit position
 */
static inline void
rte_bitmap_clear(struct rte_bitmap *bmp, uint32_t pos)
{
	uint64_t *slab1, *slab2;
	uint32_t index1, index2, offset1, offset2;

	/* Clear bit in array2 slab */
	index2 = pos >> RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
	offset2 = pos & RTE_BITMAP_SLAB_BIT_MASK;
	slab2 = bmp->array2 + index2;
	
	/* Return if array2 slab is not all-zeros */
	*slab2 &= ~(1lu << offset2);
	if (*slab2){
		return;
	}
	
	/* Check the entire cache line of array2 for all-zeros */
	index2 &= ~ RTE_BITMAP_CL_SLAB_MASK;
	slab2 = bmp->array2 + index2;
	if (__rte_bitmap_line_not_empty(slab2)) {
		return;
	}
	
	/* The array2 cache line is all-zeros, so clear bit in array1 slab */
	index1 = pos >> (RTE_BITMAP_SLAB_BIT_SIZE_LOG2 + RTE_BITMAP_CL_BIT_SIZE_LOG2);
	offset1 = (pos >> RTE_BITMAP_CL_BIT_SIZE_LOG2) & RTE_BITMAP_SLAB_BIT_MASK;
	slab1 = bmp->array1 + index1;
	*slab1 &= ~(1lu << offset1);

	return;
}

static inline int
__rte_bitmap_scan_search(struct rte_bitmap *bmp)
{
	uint64_t value1;
	uint32_t i;
	
	/* Check current array1 slab */
	value1 = bmp->array1[bmp->index1];
	value1 &= __rte_bitmap_mask1_get(bmp);
	
	if (rte_bsf64(value1, &bmp->offset1)) {
		return 1;
	}
	
	__rte_bitmap_index1_inc(bmp);
	bmp->offset1 = 0;
	
	/* Look for another array1 slab */
	for (i = 0; i < RTE_BITMAP_ARRAY1_SIZE; i ++, __rte_bitmap_index1_inc(bmp)) {
		value1 = bmp->array1[bmp->index1];
		
		if (rte_bsf64(value1, &bmp->offset1)) {
			return 1;
		}
	}
	
	return 0;
}

static inline void
__rte_bitmap_scan_read_init(struct rte_bitmap *bmp)
{
	__rte_bitmap_index2_set(bmp);
	bmp->go2 = 1;
	rte_prefetch1((void *)(bmp->array2 + bmp->index2 + 8));
}

static inline int
__rte_bitmap_scan_read(struct rte_bitmap *bmp, uint32_t *pos, uint64_t *slab)
{
	uint64_t *slab2;
	
	slab2 = bmp->array2 + bmp->index2;
	for ( ; bmp->go2 ; bmp->index2 ++, slab2 ++, bmp->go2 = bmp->index2 & RTE_BITMAP_CL_SLAB_MASK) {
		if (*slab2) {
			*pos = bmp->index2 << RTE_BITMAP_SLAB_BIT_SIZE_LOG2;
			*slab = *slab2;
			
			bmp->index2 ++;
			slab2 ++;
			bmp->go2 = bmp->index2 & RTE_BITMAP_CL_SLAB_MASK;
			return 1;
		}
	}
	
	return 0;
}

/**
 * Bitmap scan (with automatic wrap-around)
 *
 * @param bmp
 *   Handle to bitmap instance
 * @param pos
 *   When function call returns 1, pos contains the position of the next set
 *   bit, otherwise not modified
 * @param slab
 *   When function call returns 1, slab contains the value of the entire 64-bit
 *   slab where the bit indicated by pos is located. Slabs are always 64-bit
 *   aligned, so the position of the first bit of the slab (this bit is not 
 *   necessarily set) is pos / 64. Once a slab has been returned by the bitmap
 *   scan operation, the internal pointers of the bitmap are updated to point
 *   after this slab, so the same slab will not be returned again if it 
 *   contains more than one bit which is set. When function call returns 0,
 *   slab is not modified.
 * @return
 *   0 if there is no bit set in the bitmap, 1 otherwise
 */
static inline int
rte_bitmap_scan(struct rte_bitmap *bmp, uint32_t *pos, uint64_t *slab)
{
	/* Return data from current array2 line if available */
	if (__rte_bitmap_scan_read(bmp, pos, slab)) {
		return 1;
	}
	
	/* Look for non-empty array2 line */
	if (__rte_bitmap_scan_search(bmp)) {
		__rte_bitmap_scan_read_init(bmp);
		__rte_bitmap_scan_read(bmp, pos, slab);
		return 1;
	}
	
	/* Empty bitmap */
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_BITMAP_H__ */
