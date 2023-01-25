/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPA_H_
#define _ROC_NPA_H_

#include <stdint.h>

#include "hw/npa.h"

#include "roc_bits.h"
#include "roc_constants.h"
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif
#include "roc_npa_dp.h"

#define ROC_AURA_OP_LIMIT_MASK (BIT_ULL(36) - 1)

#define ROC_NPA_MAX_BLOCK_SZ		   (128 * 1024)
#define ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS 512
#define ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS  15U
#define ROC_CN10K_NPA_BATCH_FREE_BURST_MAX 16U

/* This value controls how much of the present average resource level is used to
 * calculate the new resource level.
 */
#define ROC_NPA_AVG_CONT 0xE0

/* 16 CASP instructions can be outstanding in CN9k, but we use only 15
 * outstanding CASPs as we run out of registers.
 */
#define ROC_CN9K_NPA_BULK_ALLOC_MAX_PTRS 30

/*
 * Generate 64bit handle to have optimized alloc and free aura operation.
 * 0 - ROC_AURA_ID_MASK for storing the aura_id.
 * [ROC_AURA_ID_MASK+1, (2^64 - 1)] for storing the lf base address.
 * This scheme is valid when OS can give ROC_AURA_ID_MASK
 * aligned address for lf base address.
 */
static inline uint64_t
roc_npa_aura_handle_gen(uint32_t aura_id, uintptr_t addr)
{
	uint64_t val;

	val = aura_id & ROC_AURA_ID_MASK;
	return (uint64_t)addr | val;
}

static inline uint64_t
roc_npa_aura_op_alloc(uint64_t aura_handle, const int drop)
{
	uint64_t wdata = roc_npa_aura_handle_to_aura(aura_handle);
	int64_t *addr;

	if (drop)
		wdata |= BIT_ULL(63); /* DROP */

	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_ALLOCX(0));
	return roc_atomic64_add_nosync(wdata, addr);
}

static inline uint64_t
roc_npa_aura_op_cnt_get(uint64_t aura_handle)
{
	uint64_t wdata;
	int64_t *addr;
	uint64_t reg;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_CNT);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

static inline void
roc_npa_aura_op_cnt_set(uint64_t aura_handle, const int sign, uint64_t count)
{
	uint64_t reg = count & (BIT_ULL(36) - 1);

	if (sign)
		reg |= BIT_ULL(43); /* CNT_ADD */

	reg |= (roc_npa_aura_handle_to_aura(aura_handle) << 44);

	plt_write64(reg, roc_npa_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_CNT);
}

static inline uint64_t
roc_npa_aura_op_limit_get(uint64_t aura_handle)
{
	uint64_t wdata;
	int64_t *addr;
	uint64_t reg;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_LIMIT);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & ROC_AURA_OP_LIMIT_MASK;
}

static inline void
roc_npa_aura_op_limit_set(uint64_t aura_handle, uint64_t limit)
{
	uint64_t reg = limit & ROC_AURA_OP_LIMIT_MASK;

	reg |= (roc_npa_aura_handle_to_aura(aura_handle) << 44);

	plt_write64(reg, roc_npa_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_LIMIT);
}

static inline uint64_t
roc_npa_aura_op_available(uint64_t aura_handle)
{
	uint64_t wdata;
	uint64_t reg;
	int64_t *addr;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_POOL_OP_AVAILABLE);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

/* Wait for a given timeout, repeatedly checking whether the available
 * pointers has reached the given count. Returns the available pointer
 * count if it has reached the given count or if timeout has expired
 */
static inline uint32_t
roc_npa_aura_op_available_wait(uint64_t aura_handle, uint32_t count,
			       uint32_t tmo_ms)
{
#define OP_AVAIL_WAIT_MS_DEFAULT   (100)
#define OP_AVAIL_CHECK_INTERVAL_MS (1)
	uint32_t op_avail;
	int retry;

	tmo_ms = tmo_ms ? tmo_ms : OP_AVAIL_WAIT_MS_DEFAULT;

	retry = tmo_ms / OP_AVAIL_CHECK_INTERVAL_MS;
	op_avail = roc_npa_aura_op_available(aura_handle);
	while (retry && (op_avail < count)) {
		plt_delay_ms(OP_AVAIL_CHECK_INTERVAL_MS);
		op_avail = roc_npa_aura_op_available(aura_handle);
		retry--;
	}

	return op_avail;
}

static inline uint64_t
roc_npa_pool_op_performance_counter(uint64_t aura_handle, const int drop)
{
	union {
		uint64_t u;
		struct npa_aura_op_wdata_s s;
	} op_wdata;
	int64_t *addr;
	uint64_t reg;

	op_wdata.u = 0;
	op_wdata.s.aura = roc_npa_aura_handle_to_aura(aura_handle);
	if (drop)
		op_wdata.s.drop |= BIT_ULL(63); /* DROP */

	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_POOL_OP_PC);

	reg = roc_atomic64_add_nosync(op_wdata.u, addr);
	/*
	 * NPA_LF_POOL_OP_PC Read Data
	 *
	 * 63       49 48    48 47     0
	 * -----------------------------
	 * | Reserved | OP_ERR | OP_PC |
	 * -----------------------------
	 */

	if (reg & BIT_ULL(48) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFFFFF;
}

static inline int
roc_npa_aura_batch_alloc_issue(uint64_t aura_handle, uint64_t *buf,
			       unsigned int num, const int dis_wait,
			       const int drop)
{
	unsigned int i;
	int64_t *addr;
	uint64_t res;
	union {
		uint64_t u;
		struct npa_batch_alloc_compare_s compare_s;
	} cmp;

	if (num > ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS)
		return -1;

	/* Zero first word of every cache line */
	for (i = 0; i < num; i += (ROC_ALIGN / sizeof(uint64_t)))
		buf[i] = 0;

	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_BATCH_ALLOC);
	cmp.u = 0;
	cmp.compare_s.aura = roc_npa_aura_handle_to_aura(aura_handle);
	cmp.compare_s.drop = drop;
	cmp.compare_s.stype = ALLOC_STYPE_STF;
	cmp.compare_s.dis_wait = dis_wait;
	cmp.compare_s.count = num;

	res = roc_atomic64_casl(cmp.u, (uint64_t)buf, addr);
	if (res != ALLOC_RESULT_ACCEPTED && res != ALLOC_RESULT_NOCORE)
		return -1;

	return 0;
}

static inline void
roc_npa_batch_alloc_wait(uint64_t *cache_line)
{
	/* Batch alloc status code is updated in bits [5:6] of the first word
	 * of the 128 byte cache line.
	 */
	while (((__atomic_load_n(cache_line, __ATOMIC_RELAXED) >> 5) & 0x3) ==
	       ALLOC_CCODE_INVAL)
		;
}

static inline unsigned int
roc_npa_aura_batch_alloc_count(uint64_t *aligned_buf, unsigned int num,
			       unsigned int do_wait)
{
	unsigned int count, i;

	if (num > ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS)
		return 0;

	count = 0;
	/* Check each ROC cache line one by one */
	for (i = 0; i < num; i += (ROC_ALIGN >> 3)) {
		struct npa_batch_alloc_status_s *status;

		status = (struct npa_batch_alloc_status_s *)&aligned_buf[i];

		if (do_wait)
			roc_npa_batch_alloc_wait(&aligned_buf[i]);

		count += status->count;
	}

	return count;
}

static inline unsigned int
roc_npa_aura_batch_alloc_extract(uint64_t *buf, uint64_t *aligned_buf,
				 unsigned int num)
{
	unsigned int count, i;

	if (num > ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS)
		return 0;

	count = 0;
	/* Check each ROC cache line one by one */
	for (i = 0; i < num; i += (ROC_ALIGN >> 3)) {
		struct npa_batch_alloc_status_s *status;
		int line_count;

		status = (struct npa_batch_alloc_status_s *)&aligned_buf[i];

		roc_npa_batch_alloc_wait(&aligned_buf[i]);

		line_count = status->count;

		/* Clear the status from the cache line */
		status->ccode = 0;
		status->count = 0;

		/* 'Compress' the allocated buffers as there can
		 * be 'holes' at the end of the 128 byte cache
		 * lines.
		 */
		memmove(&buf[count], &aligned_buf[i],
			line_count * sizeof(uint64_t));

		count += line_count;
	}

	return count;
}

static inline void
roc_npa_aura_op_bulk_free(uint64_t aura_handle, uint64_t const *buf,
			  unsigned int num, const int fabs)
{
	unsigned int i;

	for (i = 0; i < num; i++) {
		const uint64_t inbuf = buf[i];

		roc_npa_aura_op_free(aura_handle, fabs, inbuf);
	}
}

static inline unsigned int
roc_npa_aura_op_batch_alloc(uint64_t aura_handle, uint64_t *buf,
			    uint64_t *aligned_buf, unsigned int num,
			    const int dis_wait, const int drop,
			    const int partial)
{
	unsigned int count, chunk, num_alloc;

	/* The buffer should be 128 byte cache line aligned */
	if (((uint64_t)aligned_buf & (ROC_ALIGN - 1)) != 0)
		return 0;

	count = 0;
	while (num) {
		chunk = (num > ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS) ?
				      ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS :
				      num;

		if (roc_npa_aura_batch_alloc_issue(aura_handle, aligned_buf,
						   chunk, dis_wait, drop))
			break;

		num_alloc = roc_npa_aura_batch_alloc_extract(buf, aligned_buf,
							     chunk);

		count += num_alloc;
		buf += num_alloc;
		num -= num_alloc;

		if (num_alloc != chunk)
			break;
	}

	/* If the requested number of pointers was not allocated and if partial
	 * alloc is not desired, then free allocated pointers.
	 */
	if (unlikely(num != 0 && !partial)) {
		roc_npa_aura_op_bulk_free(aura_handle, buf - count, count, 1);
		count = 0;
	}

	return count;
}

static inline void
roc_npa_aura_batch_free(uint64_t aura_handle, uint64_t const *buf,
			unsigned int num, const int fabs, uint64_t lmt_addr,
			uint64_t lmt_id)
{
	uint64_t addr, tar_addr, free0;
	volatile uint64_t *lmt_data;
	unsigned int i;

	lmt_data = (uint64_t *)lmt_addr;

	addr = roc_npa_aura_handle_to_base(aura_handle) +
	       NPA_LF_AURA_BATCH_FREE0;

	/*
	 * NPA_LF_AURA_BATCH_FREE0
	 *
	 * 63   63 62  33 32       32 31  20 19    0
	 * -----------------------------------------
	 * | FABS | Rsvd | COUNT_EOT | Rsvd | AURA |
	 * -----------------------------------------
	 */
	free0 = roc_npa_aura_handle_to_aura(aura_handle);
	free0 |= ((uint64_t)!!fabs << 63);
	free0 |= ((uint64_t)(num & 0x1) << 32);

	/* tar_addr[4:6] is LMTST size-1 in units of 128b */
	tar_addr = addr | ((num >> 1) << 4);

	lmt_data[0] = free0;
	for (i = 0; i < num; i++)
		lmt_data[i + 1] = buf[i];

	roc_lmt_submit_steorl(lmt_id, tar_addr);
	plt_io_wmb();
}

static inline void
roc_npa_aura_batch_free_burst(uint64_t aura_handle, uint64_t const *buf,
			      unsigned int num, const int fabs,
			      uint64_t lmt_addr, uint64_t lmt_id)
{
	uint64_t addr, tar_addr, free0, send_data, lmtline;
	uint64_t *lmt_data;

	/* 63   52 51  20 19   7 6           4 3  0
	 * ----------------------------------------
	 * | RSVD | ADDR | RSVD | LMTST SZ(0) | 0 |
	 * ----------------------------------------
	 */
	addr = roc_npa_aura_handle_to_base(aura_handle) +
	       NPA_LF_AURA_BATCH_FREE0;
	tar_addr = addr | (0x7 << 4);

	/* 63   63 62  33 32       32 31  20 19    0
	 * -----------------------------------------
	 * | FABS | Rsvd | COUNT_EOT | Rsvd | AURA |
	 * -----------------------------------------
	 */
	free0 = roc_npa_aura_handle_to_aura(aura_handle);
	free0 |= ((uint64_t)!!fabs << 63);
	free0 |= (0x1UL << 32);

	/* Fill the lmt lines */
	lmt_data = (uint64_t *)lmt_addr;
	lmtline = 0;
	while (num) {
		lmt_data[lmtline * 16] = free0;
		memcpy(&lmt_data[(lmtline * 16) + 1], buf,
		       ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS * sizeof(uint64_t));
		lmtline++;
		num -= ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS;
		buf += ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS;
	}

	/* 63                           19 18  16 15   12 11  11 10      0
	 * ---------------------------------------------------------------
	 * | LMTST SZ(15) ... LMTST SZ(1) | Rsvd | CNTM1 | Rsvd | LMT_ID |
	 * ---------------------------------------------------------------
	 */
	send_data = lmt_id | ((lmtline - 1) << 12) | (0x1FFFFFFFFFFFUL << 19);
	roc_lmt_submit_steorl(send_data, tar_addr);
	plt_io_wmb();
}

static inline void
roc_npa_aura_op_batch_free(uint64_t aura_handle, uint64_t const *buf,
			   unsigned int num, const int fabs, uint64_t lmt_addr,
			   uint64_t lmt_id)
{
	unsigned int max_burst, chunk, bnum;

	max_burst = ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS *
		    ROC_CN10K_NPA_BATCH_FREE_BURST_MAX;
	bnum = num / ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS;
	bnum *= ROC_CN10K_NPA_BATCH_FREE_MAX_PTRS;
	num -= bnum;

	while (bnum) {
		chunk = (bnum >= max_burst) ? max_burst : bnum;
		roc_npa_aura_batch_free_burst(aura_handle, buf, chunk, fabs,
					      lmt_addr, lmt_id);
		buf += chunk;
		bnum -= chunk;
	}

	if (num)
		roc_npa_aura_batch_free(aura_handle, buf, num, fabs, lmt_addr,
					lmt_id);
}

static inline unsigned int
roc_npa_aura_bulk_alloc(uint64_t aura_handle, uint64_t *buf, unsigned int num,
			const int drop)
{
#if defined(__aarch64__)
	uint64_t wdata = roc_npa_aura_handle_to_aura(aura_handle);
	unsigned int i, count;
	uint64_t addr;

	if (drop)
		wdata |= BIT_ULL(63); /* DROP */

	addr = roc_npa_aura_handle_to_base(aura_handle) +
	       NPA_LF_AURA_OP_ALLOCX(0);

	switch (num) {
	case 30:
		asm volatile(
			".arch_extension lse\n"
			"mov v18.d[0], %[dst]\n"
			"mov v18.d[1], %[loc]\n"
			"mov v19.d[0], %[wdata]\n"
			"mov v19.d[1], x30\n"
			"mov v20.d[0], x24\n"
			"mov v20.d[1], x25\n"
			"mov v21.d[0], x26\n"
			"mov v21.d[1], x27\n"
			"mov v22.d[0], x28\n"
			"mov v22.d[1], x29\n"
			"mov x28, v19.d[0]\n"
			"mov x29, v19.d[0]\n"
			"mov x30, v18.d[1]\n"
			"casp x0, x1, x28, x29, [x30]\n"
			"casp x2, x3, x28, x29, [x30]\n"
			"casp x4, x5, x28, x29, [x30]\n"
			"casp x6, x7, x28, x29, [x30]\n"
			"casp x8, x9, x28, x29, [x30]\n"
			"casp x10, x11, x28, x29, [x30]\n"
			"casp x12, x13, x28, x29, [x30]\n"
			"casp x14, x15, x28, x29, [x30]\n"
			"casp x16, x17, x28, x29, [x30]\n"
			"casp x18, x19, x28, x29, [x30]\n"
			"casp x20, x21, x28, x29, [x30]\n"
			"casp x22, x23, x28, x29, [x30]\n"
			"casp x24, x25, x28, x29, [x30]\n"
			"casp x26, x27, x28, x29, [x30]\n"
			"casp x28, x29, x28, x29, [x30]\n"
			"mov x30, v18.d[0]\n"
			"stp x0, x1, [x30]\n"
			"stp x2, x3, [x30, #16]\n"
			"stp x4, x5, [x30, #32]\n"
			"stp x6, x7, [x30, #48]\n"
			"stp x8, x9, [x30, #64]\n"
			"stp x10, x11, [x30, #80]\n"
			"stp x12, x13, [x30, #96]\n"
			"stp x14, x15, [x30, #112]\n"
			"stp x16, x17, [x30, #128]\n"
			"stp x18, x19, [x30, #144]\n"
			"stp x20, x21, [x30, #160]\n"
			"stp x22, x23, [x30, #176]\n"
			"stp x24, x25, [x30, #192]\n"
			"stp x26, x27, [x30, #208]\n"
			"stp x28, x29, [x30, #224]\n"
			"mov %[dst], v18.d[0]\n"
			"mov %[loc], v18.d[1]\n"
			"mov %[wdata], v19.d[0]\n"
			"mov x30, v19.d[1]\n"
			"mov x24, v20.d[0]\n"
			"mov x25, v20.d[1]\n"
			"mov x26, v21.d[0]\n"
			"mov x27, v21.d[1]\n"
			"mov x28, v22.d[0]\n"
			"mov x29, v22.d[1]\n"
			:
			: [wdata] "r"(wdata), [loc] "r"(addr), [dst] "r"(buf)
			: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6",
			  "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14",
			  "x15", "x16", "x17", "x18", "x19", "x20", "x21",
			  "x22", "x23", "v18", "v19", "v20", "v21", "v22");
		break;
	case 16:
		asm volatile(
			".arch_extension lse\n"
			"mov x16, %[wdata]\n"
			"mov x17, %[wdata]\n"
			"casp x0, x1, x16, x17, [%[loc]]\n"
			"casp x2, x3, x16, x17, [%[loc]]\n"
			"casp x4, x5, x16, x17, [%[loc]]\n"
			"casp x6, x7, x16, x17, [%[loc]]\n"
			"casp x8, x9, x16, x17, [%[loc]]\n"
			"casp x10, x11, x16, x17, [%[loc]]\n"
			"casp x12, x13, x16, x17, [%[loc]]\n"
			"casp x14, x15, x16, x17, [%[loc]]\n"
			"stp x0, x1, [%[dst]]\n"
			"stp x2, x3, [%[dst], #16]\n"
			"stp x4, x5, [%[dst], #32]\n"
			"stp x6, x7, [%[dst], #48]\n"
			"stp x8, x9, [%[dst], #64]\n"
			"stp x10, x11, [%[dst], #80]\n"
			"stp x12, x13, [%[dst], #96]\n"
			"stp x14, x15, [%[dst], #112]\n"
			:
			: [wdata] "r"(wdata), [dst] "r"(buf), [loc] "r"(addr)
			: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6",
			  "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14",
			  "x15", "x16", "x17");
		break;
	case 8:
		asm volatile(
			".arch_extension lse\n"
			"mov x16, %[wdata]\n"
			"mov x17, %[wdata]\n"
			"casp x0, x1, x16, x17, [%[loc]]\n"
			"casp x2, x3, x16, x17, [%[loc]]\n"
			"casp x4, x5, x16, x17, [%[loc]]\n"
			"casp x6, x7, x16, x17, [%[loc]]\n"
			"stp x0, x1, [%[dst]]\n"
			"stp x2, x3, [%[dst], #16]\n"
			"stp x4, x5, [%[dst], #32]\n"
			"stp x6, x7, [%[dst], #48]\n"
			:
			: [wdata] "r"(wdata), [dst] "r"(buf), [loc] "r"(addr)
			: "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6",
			  "x7", "x16", "x17");
		break;
	case 4:
		asm volatile(
			".arch_extension lse\n"
			"mov x16, %[wdata]\n"
			"mov x17, %[wdata]\n"
			"casp x0, x1, x16, x17, [%[loc]]\n"
			"casp x2, x3, x16, x17, [%[loc]]\n"
			"stp x0, x1, [%[dst]]\n"
			"stp x2, x3, [%[dst], #16]\n"
			:
			: [wdata] "r"(wdata), [dst] "r"(buf), [loc] "r"(addr)
			: "memory", "x0", "x1", "x2", "x3", "x16", "x17");
		break;
	case 2:
		asm volatile(
			".arch_extension lse\n"
			"mov x16, %[wdata]\n"
			"mov x17, %[wdata]\n"
			"casp x0, x1, x16, x17, [%[loc]]\n"
			"stp x0, x1, [%[dst]]\n"
			:
			: [wdata] "r"(wdata), [dst] "r"(buf), [loc] "r"(addr)
			: "memory", "x0", "x1", "x16", "x17");
		break;
	case 1:
		buf[0] = roc_npa_aura_op_alloc(aura_handle, drop);
		return !!buf[0];
	}

	/* Pack the pointers */
	for (i = 0, count = 0; i < num; i++)
		if (buf[i])
			buf[count++] = buf[i];

	return count;
#else
	unsigned int i, count;

	for (i = 0, count = 0; i < num; i++) {
		buf[count] = roc_npa_aura_op_alloc(aura_handle, drop);
		if (buf[count])
			count++;
	}

	return count;
#endif
}

static inline unsigned int
roc_npa_aura_op_bulk_alloc(uint64_t aura_handle, uint64_t *buf,
			   unsigned int num, const int drop, const int partial)
{
	unsigned int chunk, count, num_alloc;

	count = 0;
	while (num) {
		chunk = (num >= ROC_CN9K_NPA_BULK_ALLOC_MAX_PTRS) ?
				      ROC_CN9K_NPA_BULK_ALLOC_MAX_PTRS :
				      plt_align32prevpow2(num);

		num_alloc =
			roc_npa_aura_bulk_alloc(aura_handle, buf, chunk, drop);

		count += num_alloc;
		buf += num_alloc;
		num -= num_alloc;

		if (unlikely(num_alloc != chunk))
			break;
	}

	/* If the requested number of pointers was not allocated and if partial
	 * alloc is not desired, then free allocated pointers.
	 */
	if (unlikely(num != 0 && !partial)) {
		roc_npa_aura_op_bulk_free(aura_handle, buf - count, count, 1);
		count = 0;
	}

	return count;
}

struct roc_npa {
	struct plt_pci_device *pci_dev;

#define ROC_NPA_MEM_SZ (1 * 1024)
	uint8_t reserved[ROC_NPA_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

int __roc_api roc_npa_dev_init(struct roc_npa *roc_npa);
int __roc_api roc_npa_dev_fini(struct roc_npa *roc_npa);

/* Flags to pool create */
#define ROC_NPA_ZERO_AURA_F BIT(0)

/* Enumerations */
enum roc_npa_buf_type {
	/* Aura used for normal pkts */
	ROC_NPA_BUF_TYPE_PACKET = 0,
	/* Aura used for ipsec pkts */
	ROC_NPA_BUF_TYPE_PACKET_IPSEC,
	/* Aura used as vwqe for normal pkts */
	ROC_NPA_BUF_TYPE_VWQE,
	/* Aura used as vwqe for ipsec pkts */
	ROC_NPA_BUF_TYPE_VWQE_IPSEC,
	/* Aura used as SQB for SQ */
	ROC_NPA_BUF_TYPE_SQB,
	/* Aura used for general buffer */
	ROC_NPA_BUF_TYPE_BUF,
	/* Aura used for timeout pool */
	ROC_NPA_BUF_TYPE_TIMEOUT,
	ROC_NPA_BUF_TYPE_END,
};

/* NPA pool */
int __roc_api roc_npa_pool_create(uint64_t *aura_handle, uint32_t block_size,
				  uint32_t block_count, struct npa_aura_s *aura,
				  struct npa_pool_s *pool, uint32_t flags);
int __roc_api roc_npa_aura_limit_modify(uint64_t aura_handle,
					uint16_t aura_limit);
int __roc_api roc_npa_pool_destroy(uint64_t aura_handle);
int __roc_api roc_npa_pool_range_update_check(uint64_t aura_handle);
void __roc_api roc_npa_aura_op_range_set(uint64_t aura_handle,
					 uint64_t start_iova,
					 uint64_t end_iova);
uint64_t __roc_api roc_npa_zero_aura_handle(void);
int __roc_api roc_npa_buf_type_update(uint64_t aura_handle, enum roc_npa_buf_type type, int cnt);
uint64_t __roc_api roc_npa_buf_type_mask(uint64_t aura_handle);
uint64_t __roc_api roc_npa_buf_type_limit_get(uint64_t type_mask);

/* Init callbacks */
typedef int (*roc_npa_lf_init_cb_t)(struct plt_pci_device *pci_dev);
int __roc_api roc_npa_lf_init_cb_register(roc_npa_lf_init_cb_t cb);

/* Debug */
int __roc_api roc_npa_ctx_dump(void);
int __roc_api roc_npa_dump(void);

/* Reset operation performance counter. */
int __roc_api roc_npa_pool_op_pc_reset(uint64_t aura_handle);

int __roc_api roc_npa_aura_drop_set(uint64_t aura_handle, uint64_t limit,
				    bool ena);

void __roc_api roc_npa_dev_lock(void);
void __roc_api roc_npa_dev_unlock(void);

#endif /* _ROC_NPA_H_ */
