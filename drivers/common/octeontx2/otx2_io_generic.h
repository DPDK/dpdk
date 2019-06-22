/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_IO_GENERIC_H_
#define _OTX2_IO_GENERIC_H_

#define otx2_load_pair(val0, val1, addr)			\
do {								\
	val0 = rte_read64_relaxed((void *)(addr));		\
	val1 = rte_read64_relaxed((uint8_t *)(addr) + 8);	\
} while (0)

#define otx2_store_pair(val0, val1, addr)			\
do {								\
	rte_write64_relaxed(val0, (void *)(addr));		\
	rte_write64_relaxed(val1, (((uint8_t *)(addr)) + 8));	\
} while (0)

#define otx2_prefetch_store_keep(ptr) do {} while (0)

static inline uint64_t
otx2_atomic64_add_nosync(int64_t incr, int64_t *ptr)
{
	RTE_SET_USED(ptr);
	RTE_SET_USED(incr);

	return 0;
}

static inline uint64_t
otx2_atomic64_add_sync(int64_t incr, int64_t *ptr)
{
	RTE_SET_USED(ptr);
	RTE_SET_USED(incr);

	return 0;
}

static inline int64_t
otx2_lmt_submit(uint64_t io_address)
{
	RTE_SET_USED(io_address);

	return 0;
}

static __rte_always_inline void
otx2_lmt_mov(void *out, const void *in, const uint32_t lmtext)
{
	RTE_SET_USED(out);
	RTE_SET_USED(in);
	RTE_SET_USED(lmtext);
}

static __rte_always_inline void
otx2_lmt_mov_seg(void *out, const void *in, const uint16_t segdw)
{
	RTE_SET_USED(out);
	RTE_SET_USED(in);
	RTE_SET_USED(segdw);
}
#endif /* _OTX2_IO_GENERIC_H_ */
