/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_bus_pci.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <rte_mbuf.h>

#include "octeontx_mbox.h"
#include "octeontx_fpavf.h"

/* FPA Mbox Message */
#define IDENTIFY		0x0

#define FPA_CONFIGSET		0x1
#define FPA_CONFIGGET		0x2
#define FPA_START_COUNT		0x3
#define FPA_STOP_COUNT		0x4
#define FPA_ATTACHAURA		0x5
#define FPA_DETACHAURA		0x6
#define FPA_SETAURALVL		0x7
#define FPA_GETAURALVL		0x8

#define FPA_COPROC		0x1

/* fpa mbox struct */
struct octeontx_mbox_fpa_cfg {
	int		aid;
	uint64_t	pool_cfg;
	uint64_t	pool_stack_base;
	uint64_t	pool_stack_end;
	uint64_t	aura_cfg;
};

struct __attribute__((__packed__)) gen_req {
	uint32_t	value;
};

struct __attribute__((__packed__)) idn_req {
	uint8_t	domain_id;
};

struct __attribute__((__packed__)) gen_resp {
	uint16_t	domain_id;
	uint16_t	vfid;
};

struct __attribute__((__packed__)) dcfg_resp {
	uint8_t	sso_count;
	uint8_t	ssow_count;
	uint8_t	fpa_count;
	uint8_t	pko_count;
	uint8_t	tim_count;
	uint8_t	net_port_count;
	uint8_t	virt_port_count;
};

#define FPA_MAX_POOL	32
#define FPA_PF_PAGE_SZ	4096

#define FPA_LN_SIZE	128
#define FPA_ROUND_UP(x, size) \
	((((unsigned long)(x)) + size-1) & (~(size-1)))
#define FPA_OBJSZ_2_CACHE_LINE(sz)	(((sz) + RTE_CACHE_LINE_MASK) >> 7)
#define FPA_CACHE_LINE_2_OBJSZ(sz)	((sz) << 7)

#define POOL_ENA			(0x1 << 0)
#define POOL_DIS			(0x0 << 0)
#define POOL_SET_NAT_ALIGN		(0x1 << 1)
#define POOL_DIS_NAT_ALIGN		(0x0 << 1)
#define POOL_STYPE(x)			(((x) & 0x1) << 2)
#define POOL_LTYPE(x)			(((x) & 0x3) << 3)
#define POOL_BUF_OFFSET(x)		(((x) & 0x7fffULL) << 16)
#define POOL_BUF_SIZE(x)		(((x) & 0x7ffULL) << 32)

struct fpavf_res {
	void		*pool_stack_base;
	void		*bar0;
	uint64_t	stack_ln_ptr;
	uint16_t	domain_id;
	uint16_t	vf_id;	/* gpool_id */
	uint16_t	sz128;	/* Block size in cache lines */
	bool		is_inuse;
};

struct octeontx_fpadev {
	rte_spinlock_t lock;
	uint8_t	total_gpool_cnt;
	struct fpavf_res pool[FPA_VF_MAX];
};

static struct octeontx_fpadev fpadev;

int octeontx_logtype_fpavf;
int octeontx_logtype_fpavf_mbox;

RTE_INIT(otx_pool_init_log)
{
	octeontx_logtype_fpavf = rte_log_register("pmd.mempool.octeontx");
	if (octeontx_logtype_fpavf >= 0)
		rte_log_set_level(octeontx_logtype_fpavf, RTE_LOG_NOTICE);
}

/* lock is taken by caller */
static int
octeontx_fpa_gpool_alloc(unsigned int object_size)
{
	struct fpavf_res *res = NULL;
	uint16_t gpool;
	unsigned int sz128;

	sz128 = FPA_OBJSZ_2_CACHE_LINE(object_size);

	for (gpool = 0; gpool < FPA_VF_MAX; gpool++) {

		/* Skip VF that is not mapped Or _inuse */
		if ((fpadev.pool[gpool].bar0 == NULL) ||
		    (fpadev.pool[gpool].is_inuse == true))
			continue;

		res = &fpadev.pool[gpool];

		RTE_ASSERT(res->domain_id != (uint16_t)~0);
		RTE_ASSERT(res->vf_id != (uint16_t)~0);
		RTE_ASSERT(res->stack_ln_ptr != 0);

		if (res->sz128 == 0) {
			res->sz128 = sz128;

			fpavf_log_dbg("gpool %d blk_sz %d\n", gpool, sz128);
			return gpool;
		}
	}

	return -ENOSPC;
}

/* lock is taken by caller */
static __rte_always_inline uintptr_t
octeontx_fpa_gpool2handle(uint16_t gpool)
{
	struct fpavf_res *res = NULL;

	RTE_ASSERT(gpool < FPA_VF_MAX);

	res = &fpadev.pool[gpool];
	return (uintptr_t)res->bar0 | gpool;
}

static __rte_always_inline bool
octeontx_fpa_handle_valid(uintptr_t handle)
{
	struct fpavf_res *res = NULL;
	uint8_t gpool;
	int i;
	bool ret = false;

	if (unlikely(!handle))
		return ret;

	/* get the gpool */
	gpool = octeontx_fpa_bufpool_gpool(handle);

	/* get the bar address */
	handle &= ~(uint64_t)FPA_GPOOL_MASK;
	for (i = 0; i < FPA_VF_MAX; i++) {
		if ((uintptr_t)fpadev.pool[i].bar0 != handle)
			continue;

		/* validate gpool */
		if (gpool != i)
			return false;

		res = &fpadev.pool[i];

		if (res->sz128 == 0 || res->domain_id == (uint16_t)~0 ||
		    res->stack_ln_ptr == 0)
			ret = false;
		else
			ret = true;
		break;
	}

	return ret;
}

static int
octeontx_fpapf_pool_setup(unsigned int gpool, unsigned int buf_size,
			  signed short buf_offset, unsigned int max_buf_count)
{
	void *memptr = NULL;
	rte_iova_t phys_addr;
	unsigned int memsz;
	struct fpavf_res *fpa = NULL;
	uint64_t reg;
	struct octeontx_mbox_hdr hdr;
	struct dcfg_resp resp;
	struct octeontx_mbox_fpa_cfg cfg;
	int ret = -1;

	fpa = &fpadev.pool[gpool];
	memsz = FPA_ROUND_UP(max_buf_count / fpa->stack_ln_ptr, FPA_LN_SIZE) *
			FPA_LN_SIZE;

	/* Round-up to page size */
	memsz = (memsz + FPA_PF_PAGE_SZ - 1) & ~(uintptr_t)(FPA_PF_PAGE_SZ-1);
	memptr = rte_malloc(NULL, memsz, RTE_CACHE_LINE_SIZE);
	if (memptr == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	/* Configure stack */
	fpa->pool_stack_base = memptr;
	phys_addr = rte_malloc_virt2iova(memptr);

	buf_size /= FPA_LN_SIZE;

	/* POOL setup */
	hdr.coproc = FPA_COPROC;
	hdr.msg = FPA_CONFIGSET;
	hdr.vfid = fpa->vf_id;
	hdr.res_code = 0;

	buf_offset /= FPA_LN_SIZE;
	reg = POOL_BUF_SIZE(buf_size) | POOL_BUF_OFFSET(buf_offset) |
		POOL_LTYPE(0x2) | POOL_STYPE(0) | POOL_SET_NAT_ALIGN |
		POOL_ENA;

	cfg.aid = FPA_AURA_IDX(gpool);
	cfg.pool_cfg = reg;
	cfg.pool_stack_base = phys_addr;
	cfg.pool_stack_end = phys_addr + memsz;
	cfg.aura_cfg = (1 << 9);

	ret = octeontx_mbox_send(&hdr, &cfg,
					sizeof(struct octeontx_mbox_fpa_cfg),
					&resp, sizeof(resp));
	if (ret < 0) {
		ret = -EACCES;
		goto err;
	}

	fpavf_log_dbg(" vfid %d gpool %d aid %d pool_cfg 0x%x pool_stack_base %" PRIx64 " pool_stack_end %" PRIx64" aura_cfg %" PRIx64 "\n",
		      fpa->vf_id, gpool, cfg.aid, (unsigned int)cfg.pool_cfg,
		      cfg.pool_stack_base, cfg.pool_stack_end, cfg.aura_cfg);

	/* Now pool is in_use */
	fpa->is_inuse = true;

err:
	if (ret < 0)
		rte_free(memptr);

	return ret;
}

static int
octeontx_fpapf_pool_destroy(unsigned int gpool_index)
{
	struct octeontx_mbox_hdr hdr;
	struct dcfg_resp resp;
	struct octeontx_mbox_fpa_cfg cfg;
	struct fpavf_res *fpa = NULL;
	int ret = -1;

	fpa = &fpadev.pool[gpool_index];

	hdr.coproc = FPA_COPROC;
	hdr.msg = FPA_CONFIGSET;
	hdr.vfid = fpa->vf_id;
	hdr.res_code = 0;

	/* reset and free the pool */
	cfg.aid = 0;
	cfg.pool_cfg = 0;
	cfg.pool_stack_base = 0;
	cfg.pool_stack_end = 0;
	cfg.aura_cfg = 0;

	ret = octeontx_mbox_send(&hdr, &cfg,
					sizeof(struct octeontx_mbox_fpa_cfg),
					&resp, sizeof(resp));
	if (ret < 0) {
		ret = -EACCES;
		goto err;
	}

	ret = 0;
err:
	/* anycase free pool stack memory */
	rte_free(fpa->pool_stack_base);
	fpa->pool_stack_base = NULL;
	return ret;
}

static int
octeontx_fpapf_aura_attach(unsigned int gpool_index)
{
	struct octeontx_mbox_hdr hdr;
	struct dcfg_resp resp;
	struct octeontx_mbox_fpa_cfg cfg;
	int ret = 0;

	if (gpool_index >= FPA_MAX_POOL) {
		ret = -EINVAL;
		goto err;
	}
	hdr.coproc = FPA_COPROC;
	hdr.msg = FPA_ATTACHAURA;
	hdr.vfid = gpool_index;
	hdr.res_code = 0;
	memset(&cfg, 0x0, sizeof(struct octeontx_mbox_fpa_cfg));
	cfg.aid = FPA_AURA_IDX(gpool_index);

	ret = octeontx_mbox_send(&hdr, &cfg,
					sizeof(struct octeontx_mbox_fpa_cfg),
					&resp, sizeof(resp));
	if (ret < 0) {
		fpavf_log_err("Could not attach fpa ");
		fpavf_log_err("aura %d to pool %d. Err=%d. FuncErr=%d\n",
			      FPA_AURA_IDX(gpool_index), gpool_index, ret,
			      hdr.res_code);
		ret = -EACCES;
		goto err;
	}
err:
	return ret;
}

static int
octeontx_fpapf_aura_detach(unsigned int gpool_index)
{
	struct octeontx_mbox_fpa_cfg cfg = {0};
	struct octeontx_mbox_hdr hdr = {0};
	int ret = 0;

	if (gpool_index >= FPA_MAX_POOL) {
		ret = -EINVAL;
		goto err;
	}

	cfg.aid = FPA_AURA_IDX(gpool_index);
	hdr.coproc = FPA_COPROC;
	hdr.msg = FPA_DETACHAURA;
	hdr.vfid = gpool_index;
	ret = octeontx_mbox_send(&hdr, &cfg, sizeof(cfg), NULL, 0);
	if (ret < 0) {
		fpavf_log_err("Couldn't detach FPA aura %d Err=%d FuncErr=%d\n",
			      FPA_AURA_IDX(gpool_index), ret,
			      hdr.res_code);
		ret = -EINVAL;
	}

err:
	return ret;
}

int
octeontx_fpavf_pool_set_range(uintptr_t handle, unsigned long memsz,
			  void *memva, uint16_t gpool)
{
	uint64_t va_end;

	if (unlikely(!handle))
		return -ENODEV;

	va_end = (uintptr_t)memva + memsz;
	va_end &= ~RTE_CACHE_LINE_MASK;

	/* VHPOOL setup */
	fpavf_write64((uintptr_t)memva,
			 (void *)((uintptr_t)handle +
			 FPA_VF_VHPOOL_START_ADDR(gpool)));
	fpavf_write64(va_end,
			 (void *)((uintptr_t)handle +
			 FPA_VF_VHPOOL_END_ADDR(gpool)));
	return 0;
}

static int
octeontx_fpapf_start_count(uint16_t gpool_index)
{
	int ret = 0;
	struct octeontx_mbox_hdr hdr = {0};

	if (gpool_index >= FPA_MAX_POOL) {
		ret = -EINVAL;
		goto err;
	}

	hdr.coproc = FPA_COPROC;
	hdr.msg = FPA_START_COUNT;
	hdr.vfid = gpool_index;
	ret = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (ret < 0) {
		fpavf_log_err("Could not start buffer counting for ");
		fpavf_log_err("FPA pool %d. Err=%d. FuncErr=%d\n",
			      gpool_index, ret, hdr.res_code);
		ret = -EINVAL;
		goto err;
	}

err:
	return ret;
}

static __rte_always_inline int
octeontx_fpavf_free(unsigned int gpool)
{
	int ret = 0;

	if (gpool >= FPA_MAX_POOL) {
		ret = -EINVAL;
		goto err;
	}

	/* Pool is free */
	fpadev.pool[gpool].is_inuse = false;

err:
	return ret;
}

static __rte_always_inline int
octeontx_gpool_free(uint16_t gpool)
{
	if (fpadev.pool[gpool].sz128 != 0) {
		fpadev.pool[gpool].sz128 = 0;
		return 0;
	}
	return -EINVAL;
}

/*
 * Return buffer size for a given pool
 */
int
octeontx_fpa_bufpool_block_size(uintptr_t handle)
{
	struct fpavf_res *res = NULL;
	uint8_t gpool;

	if (unlikely(!octeontx_fpa_handle_valid(handle)))
		return -EINVAL;

	/* get the gpool */
	gpool = octeontx_fpa_bufpool_gpool(handle);
	res = &fpadev.pool[gpool];
	return FPA_CACHE_LINE_2_OBJSZ(res->sz128);
}

int
octeontx_fpa_bufpool_free_count(uintptr_t handle)
{
	uint64_t cnt, limit, avail;
	uint8_t gpool;
	uint16_t gaura;
	uintptr_t pool_bar;

	if (unlikely(!octeontx_fpa_handle_valid(handle)))
		return -EINVAL;

	/* get the gpool */
	gpool = octeontx_fpa_bufpool_gpool(handle);
	/* get the aura */
	gaura = octeontx_fpa_bufpool_gaura(handle);

	/* Get pool bar address from handle */
	pool_bar = handle & ~(uint64_t)FPA_GPOOL_MASK;

	cnt = fpavf_read64((void *)((uintptr_t)pool_bar +
				FPA_VF_VHAURA_CNT(gaura)));
	limit = fpavf_read64((void *)((uintptr_t)pool_bar +
				FPA_VF_VHAURA_CNT_LIMIT(gaura)));

	avail = fpavf_read64((void *)((uintptr_t)pool_bar +
				FPA_VF_VHPOOL_AVAILABLE(gpool)));

	return RTE_MIN(avail, (limit - cnt));
}

uintptr_t
octeontx_fpa_bufpool_create(unsigned int object_size, unsigned int object_count,
				unsigned int buf_offset, int node_id)
{
	unsigned int gpool;
	unsigned int gaura;
	uintptr_t gpool_handle;
	uintptr_t pool_bar;
	int res;

	RTE_SET_USED(node_id);
	RTE_BUILD_BUG_ON(sizeof(struct rte_mbuf) > OCTEONTX_FPAVF_BUF_OFFSET);

	object_size = RTE_CACHE_LINE_ROUNDUP(object_size);
	if (object_size > FPA_MAX_OBJ_SIZE) {
		errno = EINVAL;
		goto error_end;
	}

	rte_spinlock_lock(&fpadev.lock);
	res = octeontx_fpa_gpool_alloc(object_size);

	/* Bail if failed */
	if (unlikely(res < 0)) {
		errno = res;
		goto error_unlock;
	}

	/* get fpavf */
	gpool = res;

	/* get pool handle */
	gpool_handle = octeontx_fpa_gpool2handle(gpool);
	if (!octeontx_fpa_handle_valid(gpool_handle)) {
		errno = ENOSPC;
		goto error_gpool_free;
	}

	/* Get pool bar address from handle */
	pool_bar = gpool_handle & ~(uint64_t)FPA_GPOOL_MASK;

	res = octeontx_fpapf_pool_setup(gpool, object_size, buf_offset,
					object_count);
	if (res < 0) {
		errno = res;
		goto error_gpool_free;
	}

	/* populate AURA fields */
	res = octeontx_fpapf_aura_attach(gpool);
	if (res < 0) {
		errno = res;
		goto error_pool_destroy;
	}

	gaura = FPA_AURA_IDX(gpool);

	/* Release lock */
	rte_spinlock_unlock(&fpadev.lock);

	/* populate AURA registers */
	fpavf_write64(object_count, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHAURA_CNT(gaura)));
	fpavf_write64(object_count, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHAURA_CNT_LIMIT(gaura)));
	fpavf_write64(object_count + 1, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHAURA_CNT_THRESHOLD(gaura)));

	octeontx_fpapf_start_count(gpool);

	return gpool_handle;

error_pool_destroy:
	octeontx_fpavf_free(gpool);
	octeontx_fpapf_pool_destroy(gpool);
error_gpool_free:
	octeontx_gpool_free(gpool);
error_unlock:
	rte_spinlock_unlock(&fpadev.lock);
error_end:
	return (uintptr_t)NULL;
}

/*
 * Destroy a buffer pool.
 */
int
octeontx_fpa_bufpool_destroy(uintptr_t handle, int node_id)
{
	void **node, **curr, *head = NULL;
	uint64_t sz;
	uint64_t cnt, avail;
	uint8_t gpool;
	uint16_t gaura;
	uintptr_t pool_bar;
	int ret;

	RTE_SET_USED(node_id);

	/* Wait for all outstanding writes to be committed */
	rte_smp_wmb();

	if (unlikely(!octeontx_fpa_handle_valid(handle)))
		return -EINVAL;

	/* get the pool */
	gpool = octeontx_fpa_bufpool_gpool(handle);
	/* get the aura */
	gaura = octeontx_fpa_bufpool_gaura(handle);

	/* Get pool bar address from handle */
	pool_bar = handle & ~(uint64_t)FPA_GPOOL_MASK;

	 /* Check for no outstanding buffers */
	cnt = fpavf_read64((void *)((uintptr_t)pool_bar +
					FPA_VF_VHAURA_CNT(gaura)));
	if (cnt) {
		fpavf_log_dbg("buffer exist in pool cnt %" PRId64 "\n", cnt);
		return -EBUSY;
	}

	rte_spinlock_lock(&fpadev.lock);

	avail = fpavf_read64((void *)((uintptr_t)pool_bar +
				FPA_VF_VHPOOL_AVAILABLE(gpool)));

	/* Prepare to empty the entire POOL */
	fpavf_write64(avail, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHAURA_CNT_LIMIT(gaura)));
	fpavf_write64(avail + 1, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHAURA_CNT_THRESHOLD(gaura)));

	/* Empty the pool */
	/* Invalidate the POOL */
	octeontx_gpool_free(gpool);

	/* Process all buffers in the pool */
	while (avail--) {

		/* Yank a buffer from the pool */
		node = (void *)(uintptr_t)
			fpavf_read64((void *)
				    (pool_bar + FPA_VF_VHAURA_OP_ALLOC(gaura)));

		if (node == NULL) {
			fpavf_log_err("GAURA[%u] missing %" PRIx64 " buf\n",
				      gaura, avail);
			break;
		}

		/* Imsert it into an ordered linked list */
		for (curr = &head; curr[0] != NULL; curr = curr[0]) {
			if ((uintptr_t)node <= (uintptr_t)curr[0])
				break;
		}
		node[0] = curr[0];
		curr[0] = node;
	}

	/* Verify the linked list to be a perfect series */
	sz = octeontx_fpa_bufpool_block_size(handle) << 7;
	for (curr = head; curr != NULL && curr[0] != NULL;
		curr = curr[0]) {
		if (curr == curr[0] ||
			((uintptr_t)curr != ((uintptr_t)curr[0] - sz))) {
			fpavf_log_err("POOL# %u buf sequence err (%p vs. %p)\n",
				      gpool, curr, curr[0]);
		}
	}

	/* Disable pool operation */
	fpavf_write64(~0ul, (void *)((uintptr_t)pool_bar +
			 FPA_VF_VHPOOL_START_ADDR(gpool)));
	fpavf_write64(~0ul, (void *)((uintptr_t)pool_bar +
			FPA_VF_VHPOOL_END_ADDR(gpool)));

	(void)octeontx_fpapf_pool_destroy(gpool);

	/* Deactivate the AURA */
	fpavf_write64(0, (void *)((uintptr_t)pool_bar +
			FPA_VF_VHAURA_CNT_LIMIT(gaura)));
	fpavf_write64(0, (void *)((uintptr_t)pool_bar +
			FPA_VF_VHAURA_CNT_THRESHOLD(gaura)));

	ret = octeontx_fpapf_aura_detach(gpool);
	if (ret) {
		fpavf_log_err("Failed to dettach gaura %u. error code=%d\n",
			      gpool, ret);
	}

	/* Free VF */
	(void)octeontx_fpavf_free(gpool);

	rte_spinlock_unlock(&fpadev.lock);
	return 0;
}

static void
octeontx_fpavf_setup(void)
{
	uint8_t i;
	static bool init_once;

	if (!init_once) {
		rte_spinlock_init(&fpadev.lock);
		fpadev.total_gpool_cnt = 0;

		for (i = 0; i < FPA_VF_MAX; i++) {

			fpadev.pool[i].domain_id = ~0;
			fpadev.pool[i].stack_ln_ptr = 0;
			fpadev.pool[i].sz128 = 0;
			fpadev.pool[i].bar0 = NULL;
			fpadev.pool[i].pool_stack_base = NULL;
			fpadev.pool[i].is_inuse = false;
		}
		init_once = 1;
	}
}

static int
octeontx_fpavf_identify(void *bar0)
{
	uint64_t val;
	uint16_t domain_id;
	uint16_t vf_id;
	uint64_t stack_ln_ptr;

	val = fpavf_read64((void *)((uintptr_t)bar0 +
				FPA_VF_VHAURA_CNT_THRESHOLD(0)));

	domain_id = (val >> 8) & 0xffff;
	vf_id = (val >> 24) & 0xffff;

	stack_ln_ptr = fpavf_read64((void *)((uintptr_t)bar0 +
					FPA_VF_VHPOOL_THRESHOLD(0)));
	if (vf_id >= FPA_VF_MAX) {
		fpavf_log_err("vf_id(%d) greater than max vf (32)\n", vf_id);
		return -1;
	}

	if (fpadev.pool[vf_id].is_inuse) {
		fpavf_log_err("vf_id %d is_inuse\n", vf_id);
		return -1;
	}

	fpadev.pool[vf_id].domain_id = domain_id;
	fpadev.pool[vf_id].vf_id = vf_id;
	fpadev.pool[vf_id].bar0 = bar0;
	fpadev.pool[vf_id].stack_ln_ptr = stack_ln_ptr;

	/* SUCCESS */
	return vf_id;
}

/* FPAVF pcie device aka mempool probe */
static int
fpavf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	uint8_t *idreg;
	int res;
	struct fpavf_res *fpa = NULL;

	RTE_SET_USED(pci_drv);
	RTE_SET_USED(fpa);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL) {
		fpavf_log_err("Empty bars %p ", pci_dev->mem_resource[0].addr);
		return -ENODEV;
	}
	idreg = pci_dev->mem_resource[0].addr;

	octeontx_fpavf_setup();

	res = octeontx_fpavf_identify(idreg);
	if (res < 0)
		return -1;

	fpa = &fpadev.pool[res];
	fpadev.total_gpool_cnt++;
	rte_wmb();

	fpavf_log_dbg("total_fpavfs %d bar0 %p domain %d vf %d stk_ln_ptr 0x%x",
		       fpadev.total_gpool_cnt, fpa->bar0, fpa->domain_id,
		       fpa->vf_id, (unsigned int)fpa->stack_ln_ptr);

	return 0;
}

static const struct rte_pci_id pci_fpavf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_FPA_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_fpavf = {
	.id_table = pci_fpavf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = fpavf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_fpavf, pci_fpavf);
