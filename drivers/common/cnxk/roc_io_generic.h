/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IO_GENERIC_H_
#define _ROC_IO_GENERIC_H_

#define ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id)	  (lmt_id = 0)
#define ROC_LMT_CPT_BASE_ID_GET(lmt_addr, lmt_id) (lmt_id = 0)

#define roc_load_pair(val0, val1, addr)                                        \
	do {                                                                   \
		val0 = plt_read64((void *)(addr));                             \
		val1 = plt_read64((uint8_t *)(addr) + 8);                      \
	} while (0)

#define roc_store_pair(val0, val1, addr)                                       \
	do {                                                                   \
		plt_write64(val0, (void *)(addr));                             \
		plt_write64(val1, (((uint8_t *)(addr)) + 8));                  \
	} while (0)

#define roc_prefetch_store_keep(ptr)                                           \
	do {                                                                   \
	} while (0)

static __plt_always_inline void
roc_atomic128_cas_noreturn(uint64_t swap0, uint64_t swap1, uint64_t ptr)
{
	PLT_SET_USED(swap0);
	PLT_SET_USED(swap1);
	PLT_SET_USED(ptr);
}

static __plt_always_inline uint64_t
roc_atomic64_cas(uint64_t compare, uint64_t swap, int64_t *ptr)
{
	PLT_SET_USED(swap);
	PLT_SET_USED(ptr);

	return compare;
}

static __plt_always_inline uint64_t
roc_atomic64_casl(uint64_t compare, uint64_t swap, int64_t *ptr)
{
	PLT_SET_USED(swap);
	PLT_SET_USED(ptr);

	return compare;
}

static inline uint64_t
roc_atomic64_add_nosync(int64_t incr, int64_t *ptr)
{
	PLT_SET_USED(ptr);
	PLT_SET_USED(incr);

	return 0;
}

static inline uint64_t
roc_atomic64_add_sync(int64_t incr, int64_t *ptr)
{
	PLT_SET_USED(ptr);
	PLT_SET_USED(incr);

	return 0;
}

static inline uint64_t
roc_lmt_submit_ldeor(plt_iova_t io_address)
{
	PLT_SET_USED(io_address);

	return 0;
}

static __plt_always_inline uint64_t
roc_lmt_submit_ldeorl(plt_iova_t io_address)
{
	PLT_SET_USED(io_address);

	return 0;
}

static inline void
roc_lmt_submit_steor(uint64_t data, plt_iova_t io_address)
{
	PLT_SET_USED(data);
	PLT_SET_USED(io_address);
}

static inline void
roc_lmt_submit_steorl(uint64_t data, plt_iova_t io_address)
{
	PLT_SET_USED(data);
	PLT_SET_USED(io_address);
}

static __plt_always_inline void
roc_lmt_mov(void *out, const void *in, const uint32_t lmtext)
{
	PLT_SET_USED(in);
	PLT_SET_USED(lmtext);
	memset(out, 0, sizeof(__uint128_t) * (lmtext ? lmtext > 1 ? 4 : 3 : 2));
}

static __plt_always_inline void
roc_lmt_mov64(void *out, const void *in)
{
	PLT_SET_USED(out);
	PLT_SET_USED(in);
}

static __plt_always_inline void
roc_lmt_mov_nv(void *out, const void *in, const uint32_t lmtext)
{
	PLT_SET_USED(in);
	PLT_SET_USED(lmtext);
	memset(out, 0, sizeof(__uint128_t) * (lmtext ? lmtext > 1 ? 4 : 3 : 2));
}

static __plt_always_inline void
roc_lmt_mov_seg(void *out, const void *in, const uint16_t segdw)
{
	PLT_SET_USED(out);
	PLT_SET_USED(in);
	PLT_SET_USED(segdw);
}

static __plt_always_inline void
roc_lmt_mov_one(void *out, const void *in)
{
	PLT_SET_USED(out);
	PLT_SET_USED(in);
}

static __plt_always_inline void
roc_lmt_mov_seg_nv(void *out, const void *in, const uint16_t segdw)
{
	PLT_SET_USED(out);
	PLT_SET_USED(in);
	PLT_SET_USED(segdw);
}

static __plt_always_inline void
roc_atf_ret(void)
{
}

#endif /* _ROC_IO_GENERIC_H_ */
