/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 */

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xen/sys/gntalloc.h>
#include <xen/sys/gntdev.h>
#include <xen/xen-compat.h>
#if __XEN_LATEST_INTERFACE_VERSION__ < 0x00040200
#include <xs.h>
#else
#include <xenstore.h>
#endif

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_log.h>
#include <rte_debug.h>

#include "xen_vhost.h"

/* xenstore handle */
static struct xs_handle *xs = NULL;

/* gntdev file descriptor to map grant pages */
static int d_fd = -1;

/*
 *  The grant node format in xenstore for vring/mpool is like:
 *  idx#_rx_vring_gref = "gref1#, gref2#, gref3#"
 *  idx#_mempool_gref  = "gref1#, gref2#, gref3#"
 *  each gref# is the grant reference for a shared page.
 *  In each shared page, we store the grant_node_item items.
 */
struct grant_node_item {
	uint32_t gref;
	uint32_t pfn;
} __attribute__((packed));

int cmdline_parse_etheraddr(void *tk, const char *srcbuf,
			    void *res);

/* Map grant ref refid at addr_ori*/
static void *
xen_grant_mmap(void *addr_ori, int domid, int refid, uint64_t *pindex)
{
	struct ioctl_gntdev_map_grant_ref arg;
	void *addr = NULL;
	int pg_sz = getpagesize();

	arg.count = 1;
	arg.refs[0].domid = domid;
	arg.refs[0].ref = refid;

	int rv = ioctl(d_fd, IOCTL_GNTDEV_MAP_GRANT_REF, &arg);
	if (rv) {
		RTE_LOG(ERR, XENHOST, "  %s: (%d,%d) %s (ioctl failed)\n", __func__,
				domid, refid, strerror(errno));
		return NULL;
	}

	if (addr_ori == NULL)
		addr = mmap(addr_ori, pg_sz, PROT_READ|PROT_WRITE, MAP_SHARED,
				d_fd, arg.index);
	else
		addr = mmap(addr_ori, pg_sz, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_FIXED,
				d_fd, arg.index);

	if (addr == MAP_FAILED) {
		RTE_LOG(ERR, XENHOST, "  %s: (%d, %d) %s (map failed)\n", __func__,
				domid, refid, strerror(errno));
		return NULL;
	}

	if (pindex)
		*pindex = arg.index;

	return addr;
}

/* Unmap one grant ref, and munmap must be called before this */
static int
xen_unmap_grant_ref(uint64_t index)
{
	struct ioctl_gntdev_unmap_grant_ref arg;
	int rv;

	arg.count = 1;
	arg.index = index;
	rv = ioctl(d_fd, IOCTL_GNTDEV_UNMAP_GRANT_REF, &arg);
	if (rv) {
		RTE_LOG(ERR, XENHOST, "  %s: index 0x%" PRIx64 "unmap failed\n", __func__, index);
		return -1;
	}
	return 0;
}

/*
 * Reserve a virtual address space.
 * On success, returns the pointer. On failure, returns NULL.
 */
static void *
get_xen_virtual(size_t size, size_t page_sz)
{
	void *addr;
	uintptr_t aligned_addr;

	addr = mmap(NULL, size + page_sz, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		RTE_LOG(ERR, XENHOST, "failed get a virtual area\n");
		return NULL;
	}

	aligned_addr = RTE_ALIGN_CEIL((uintptr_t)addr, page_sz);
	munmap(addr, aligned_addr - (uintptr_t)addr);
	munmap((void *)(aligned_addr + size), page_sz + (uintptr_t)addr - aligned_addr);
	addr = (void *)(aligned_addr);

	return addr;
}

static void
free_xen_virtual(void *addr, size_t size, size_t page_sz __rte_unused)
{
	if (addr)
		munmap(addr, size);
}

/*
 * Returns val str in xenstore.
 * @param path
 *  Full path string for key
 * @return
 *  Pointer to Val str, NULL on failure
 */
static char *
xen_read_node(char *path, uint32_t *len)
{
	char *buf;

	buf = xs_read(xs, XBT_NULL, path, len);
	return buf;
}

static int
cal_pagenum(struct xen_gnt *gnt)
{
	unsigned int i;
	/*
	 * the items in the page are in the format of
	 * gref#,pfn#,...,gref#,pfn#
	 * FIXME, 0 is reserved by system, use it as terminator.
	 */
	for (i = 0; i < (PAGE_PFNNUM) / 2; i++) {
		if (gnt->gref_pfn[i * 2].gref <= 0)
			break;
	}

	return i;
}

/* Frees memory allocated to a grant node */
static void
xen_free_gntnode(struct xen_gntnode *gntnode)
{
	if (gntnode == NULL)
		return;
	if (gntnode->gnt_info)
		free(gntnode->gnt_info);
	free(gntnode);
}

/*
 * Parse a grant node.
 * @param domid
 *  Guest domain id.
 * @param path
 *  Full path string for a grant node, like for the following (key, val) pair
 *  idx#_mempool_gref = "gref#, gref#, gref#"
 *  path = 'local/domain/domid/control/dpdk/idx#_mempool_gref'
 *  gref# is a shared page contain packed (gref,pfn) entries
 * @return
 *  Returns the pointer to xen_gntnode
 */
static struct xen_gntnode *
parse_gntnode(int dom_id, char *path)
{
	char **gref_list = NULL;
	uint32_t i, len, gref_num;
	void *addr = NULL;
	char *buf = NULL;
	struct xen_gntnode *gntnode = NULL;
	struct xen_gnt *gnt = NULL;
	int pg_sz = getpagesize();
	char *end;
	uint64_t index;

	if ((buf = xen_read_node(path, &len)) == NULL)
		goto err;

	gref_list = malloc(MAX_GREF_PER_NODE * sizeof(char *));
	if (gref_list == NULL)
		goto err;

	gref_num = rte_strsplit(buf, len, gref_list, MAX_GREF_PER_NODE,
			XEN_GREF_SPLITTOKEN);
	if (gref_num == 0) {
		RTE_LOG(ERR, XENHOST, "  %s: invalid grant node format\n", __func__);
		goto err;
	}

	gntnode = (struct xen_gntnode *)calloc(1, sizeof(struct xen_gntnode));
	gnt = (struct xen_gnt *)calloc(gref_num, sizeof(struct xen_gnt));
	if (gnt == NULL || gntnode == NULL)
		goto err;

	for (i = 0; i < gref_num; i++) {
		errno = 0;
		gnt[i].gref = strtol(gref_list[i], &end, 0);
		if (errno != 0 || end == NULL || end == gref_list[i] ||
			(*end != '\0' &&  *end != XEN_GREF_SPLITTOKEN)) {
			RTE_LOG(ERR, XENHOST, "  %s: parse grant node item failed\n", __func__);
			goto err;
		}
		addr = xen_grant_mmap(NULL, dom_id, gnt[i].gref, &index);
		if (addr == NULL) {
			RTE_LOG(ERR, XENHOST, "  %s: map gref %u failed\n", __func__, gnt[i].gref);
			goto err;
		}
		RTE_LOG(INFO, XENHOST, "      %s: map gref %u to %p\n", __func__, gnt[i].gref, addr);
		memcpy(gnt[i].gref_pfn, addr, pg_sz);
		if (munmap(addr, pg_sz)) {
			RTE_LOG(INFO, XENHOST, "  %s: unmap gref %u failed\n", __func__, gnt[i].gref);
			goto err;
		}
		if (xen_unmap_grant_ref(index)) {
			RTE_LOG(INFO, XENHOST, "  %s: release gref %u failed\n", __func__, gnt[i].gref);
			goto err;
		}

	}

	gntnode->gnt_num  = gref_num;
	gntnode->gnt_info = gnt;

	free(buf);
	free(gref_list);
	return gntnode;

err:
	if (gnt)
		free(gnt);
	if (gntnode)
		free(gntnode);
	if (gref_list)
		free(gref_list);
	if (buf)
		free(buf);
	return NULL;
}

/*
 * This function maps grant node of vring or mbuf pool to a continous virtual address space,
 * and returns mapped address, pfn array, index array
 * @param gntnode
 *  Pointer to grant node
 * @param domid
 *  Guest domain id
 * @param ppfn
 *  Pointer to pfn array, caller should free this array
 * @param pgs
 *  Pointer to number of pages
 * @param ppindex
 *  Pointer to index array, used to release grefs when to free this node
 * @return
 *  Pointer to mapped virtual address, NULL on failure
 */
static void *
map_gntnode(struct xen_gntnode *gntnode, int domid, uint32_t **ppfn, uint32_t *pgs, uint64_t **ppindex)
{
	struct xen_gnt *gnt;
	uint32_t i, j;
	size_t total_pages = 0;
	void *addr;
	uint32_t *pfn;
	uint64_t *pindex;
	uint32_t pfn_num = 0;
	int pg_sz;

	if (gntnode == NULL)
		return NULL;

	pg_sz = getpagesize();
	for (i = 0; i < gntnode->gnt_num; i++) {
		gnt = gntnode->gnt_info + i;
		total_pages += cal_pagenum(gnt);
	}
	if ((addr = get_xen_virtual(total_pages * pg_sz, pg_sz)) == NULL) {
		RTE_LOG(ERR, XENHOST, "  %s: failed get_xen_virtual\n", __func__);
		return NULL;
	}
	pfn = calloc(total_pages, (size_t)sizeof(uint32_t));
	pindex = calloc(total_pages, (size_t)sizeof(uint64_t));
	if (pfn == NULL || pindex == NULL) {
		free_xen_virtual(addr, total_pages * pg_sz, pg_sz);
		free(pfn);
		free(pindex);
		return NULL;
	}

	RTE_LOG(INFO, XENHOST, "    %s: total pages:%zu, map to [%p, %p]\n", __func__, total_pages, addr, RTE_PTR_ADD(addr, total_pages * pg_sz - 1));
	for (i = 0; i < gntnode->gnt_num; i++) {
		gnt = gntnode->gnt_info + i;
		for (j = 0; j < (PAGE_PFNNUM) / 2; j++) {
			if ((gnt->gref_pfn[j * 2].gref) <= 0)
				goto _end;
			/*alternative: batch map, or through libxc*/
			if (xen_grant_mmap(RTE_PTR_ADD(addr, pfn_num * pg_sz),
					domid,
					gnt->gref_pfn[j * 2].gref,
					&pindex[pfn_num]) == NULL) {
				goto mmap_failed;
			}
			pfn[pfn_num] = gnt->gref_pfn[j * 2 + 1].pfn_num;
			pfn_num++;
		}
	}

mmap_failed:
	if (pfn_num)
		munmap(addr, pfn_num * pg_sz);
	for (i = 0; i < pfn_num; i++) {
		xen_unmap_grant_ref(pindex[i]);
	}
	free(pindex);
	free(pfn);
	return NULL;

_end:
	if (ppindex)
		*ppindex = pindex;
	else
		free(pindex);
	if (ppfn)
		*ppfn = pfn;
	else
		free(pfn);
	if (pgs)
		*pgs = total_pages;

	return addr;
}

static int
parse_mpool_va(struct xen_mempool *mempool)
{
	char path[PATH_MAX] = {0};
	char *buf;
	uint32_t len;
	char *end;
	int ret = -1;

	errno = 0;
	snprintf(path, sizeof(path),
		XEN_VM_ROOTNODE_FMT"/%d_"XEN_GVA_SUFFIX,
		mempool->dom_id, mempool->pool_idx);

	if((buf = xen_read_node(path, &len)) == NULL)
		goto out;
	mempool->gva = (void *)strtoul(buf, &end, 16);
	if (errno != 0 || end == NULL || end == buf || *end != '\0') {
		mempool->gva = NULL;
		goto out;
	}
	ret = 0;
out:
	if (buf)
		free(buf);
	return ret;
}

/*
 * map mbuf pool
 */
static int
map_mempoolnode(struct xen_gntnode *gntnode,
			struct xen_mempool *mempool)
{
	if (gntnode == NULL || mempool == NULL)
		return -1;

	mempool->hva =
		map_gntnode(gntnode, mempool->dom_id, &mempool->mempfn_tbl, &mempool->mempfn_num, &mempool->pindex);

	RTE_LOG(INFO, XENHOST, "  %s: map mempool at %p\n", __func__, (void *)mempool->hva);
	if (mempool->hva)
		return 0;
	else {
		return -1;
	}
}

void
cleanup_mempool(struct xen_mempool *mempool)
{
	int pg_sz = getpagesize();
	uint32_t i;

	if (mempool->hva)
		munmap(mempool->hva, mempool->mempfn_num * pg_sz);
	mempool->hva = NULL;

	if (mempool->pindex) {
		RTE_LOG(INFO, XENHOST, "  %s: unmap dom %02u mempool%02u %u grefs\n",
			__func__,
			mempool->dom_id,
			mempool->pool_idx,
			mempool->mempfn_num);
		for (i = 0; i < mempool->mempfn_num; i ++) {
			xen_unmap_grant_ref(mempool->pindex[i]);
		}
	}
	mempool->pindex = NULL;

	if (mempool->mempfn_tbl)
		free(mempool->mempfn_tbl);
	mempool->mempfn_tbl = NULL;
}

/*
 * process mempool node idx#_mempool_gref, idx = 0, 1, 2...
 * untill we encounter a node that doesn't exist.
 */
int
parse_mempoolnode(struct xen_guest *guest)
{
	uint32_t i, len;
	char path[PATH_MAX] = {0};
	struct xen_gntnode *gntnode = NULL;
	struct xen_mempool *mempool = NULL;
	char *buf;

	bzero(&guest->mempool, MAX_XENVIRT_MEMPOOL * sizeof(guest->mempool[0]));
	guest->pool_num = 0;

	while (1) {
		/* check if null terminated */
		snprintf(path, sizeof(path),
			XEN_VM_ROOTNODE_FMT"/%d_"XEN_MEMPOOL_SUFFIX,
			guest->dom_id,
			guest->pool_num);

		if ((buf = xen_read_node(path, &len)) != NULL) {
			/* this node exists */
			free(buf);
		} else {
			if (guest->pool_num == 0) {
				RTE_LOG(ERR, PMD, "no mempool found\n");
				return -1;
			}
			break;
		}

		mempool = &guest->mempool[guest->pool_num];
		mempool->dom_id = guest->dom_id;
		mempool->pool_idx = guest->pool_num;

		RTE_LOG(INFO, XENHOST, "  %s: mempool %u parse gntnode %s\n", __func__, guest->pool_num, path);
		gntnode = parse_gntnode(guest->dom_id, path);
		if (gntnode == NULL)
			goto err;

		if (parse_mpool_va(mempool))
			goto err;

		RTE_LOG(INFO, XENHOST, "  %s: mempool %u map gntnode %s\n", __func__, guest->pool_num, path);
		if (map_mempoolnode(gntnode, mempool))
			goto err;

		xen_free_gntnode(gntnode);
		guest->pool_num++;
	}

	return 0;
err:
	if (gntnode)
		xen_free_gntnode(gntnode);
	for (i = 0; i <  MAX_XENVIRT_MEMPOOL ; i++) {
		cleanup_mempool(&guest->mempool[i]);
	}
	/* reinitialise mempool */
	bzero(&guest->mempool, MAX_XENVIRT_MEMPOOL * sizeof(guest->mempool[0]));
	return -1;
}

static int
xen_map_vringflag(struct xen_vring *vring)
{
	char path[PATH_MAX] = {0};
	char *buf;
	uint32_t len,gref;
	int pg_sz = getpagesize();
	char *end;

	snprintf(path, sizeof(path),
		XEN_VM_ROOTNODE_FMT"/%d_"XEN_VRINGFLAG_SUFFIX,
		vring->dom_id, vring->virtio_idx);

	if((buf = xen_read_node(path, &len)) == NULL)
		goto err;

	errno = 0;
	gref = strtol(buf, &end, 0);
	if (errno != 0 || end == NULL || end == buf) {
		goto err;
	}
	vring->flag = xen_grant_mmap(0, vring->dom_id, gref, &vring->flag_index);
	if (vring->flag == NULL || *vring->flag == 0)
		goto err;

	free(buf);
	return 0;
err:
	if (buf)
		free(buf);
	if (vring->flag) {
		munmap(vring->flag, pg_sz);
		vring->flag = NULL;
		xen_unmap_grant_ref(vring->flag_index);
	}
	return -1;
}


static int
xen_map_rxvringnode(struct xen_gntnode *gntnode,
				struct xen_vring *vring)
{
	vring->rxvring_addr =
		map_gntnode(gntnode, vring->dom_id, &vring->rxpfn_tbl, &vring->rxpfn_num, &vring->rx_pindex);
	RTE_LOG(INFO, XENHOST, "  %s: map rx vring at %p\n", __func__, (void *)vring->rxvring_addr);
	if (vring->rxvring_addr)
		return 0;
	else
		return -1;
}

static int
xen_map_txvringnode(struct xen_gntnode *gntnode,
				struct xen_vring *vring)
{
	vring->txvring_addr =
		map_gntnode(gntnode, vring->dom_id, &vring->txpfn_tbl, &vring->txpfn_num, &vring->tx_pindex);
	RTE_LOG(INFO, XENHOST, "  %s: map tx vring at %p\n", __func__, (void *)vring->txvring_addr);
	if (vring->txvring_addr)
		return 0;
	else
		return -1;
}

void
cleanup_vring(struct xen_vring *vring)
{
	int pg_sz = getpagesize();
	uint32_t i;

	RTE_LOG(INFO, XENHOST, "  %s: cleanup dom %u vring %u\n", __func__, vring->dom_id, vring->virtio_idx);
	if (vring->rxvring_addr) {
		munmap(vring->rxvring_addr, vring->rxpfn_num * pg_sz);
		RTE_LOG(INFO, XENHOST, "  %s: unmap rx vring [%p, %p]\n",
			__func__,
			vring->rxvring_addr,
			RTE_PTR_ADD(vring->rxvring_addr,
			vring->rxpfn_num * pg_sz - 1));
	}
	vring->rxvring_addr = NULL;


	if (vring->rx_pindex) {
		RTE_LOG(INFO, XENHOST, "  %s: unmap rx vring %u grefs\n", __func__, vring->rxpfn_num);
		for (i = 0; i < vring->rxpfn_num; i++) {
			xen_unmap_grant_ref(vring->rx_pindex[i]);
		}
	}
	vring->rx_pindex = NULL;

	if (vring->rxpfn_tbl)
		free(vring->rxpfn_tbl);
	vring->rxpfn_tbl = NULL;

	if (vring->txvring_addr) {
		munmap(vring->txvring_addr, vring->txpfn_num * pg_sz);
		RTE_LOG(INFO, XENHOST, "  %s: unmap tx vring [%p, %p]\n",
			__func__,
			vring->txvring_addr,
			RTE_PTR_ADD(vring->txvring_addr,
			vring->txpfn_num * pg_sz - 1));
	}
	vring->txvring_addr = NULL;

	if (vring->tx_pindex) {
		RTE_LOG(INFO, XENHOST, "  %s: unmap tx vring %u grefs\n", __func__, vring->txpfn_num);
		for (i = 0; i < vring->txpfn_num; i++) {
			xen_unmap_grant_ref(vring->tx_pindex[i]);
		}
	}
	vring->tx_pindex = NULL;

	if (vring->txpfn_tbl)
		free(vring->txpfn_tbl);
	vring->txpfn_tbl = NULL;

	if (vring->flag) {
		if (!munmap((void *)vring->flag, pg_sz))
			RTE_LOG(INFO, XENHOST, "  %s: unmap flag page at %p\n", __func__, vring->flag);
		if (!xen_unmap_grant_ref(vring->flag_index))
			RTE_LOG(INFO, XENHOST, "  %s: release flag ref index 0x%" PRIx64 "\n", __func__, vring->flag_index);
	}
	vring->flag = NULL;
	return;
}



static int
xen_parse_etheraddr(struct xen_vring *vring)
{
	char path[PATH_MAX] = {0};
	char *buf;
	uint32_t len;
	int ret = -1;

	snprintf(path, sizeof(path),
		XEN_VM_ROOTNODE_FMT"/%d_"XEN_ADDR_SUFFIX,
		vring->dom_id, vring->virtio_idx);

	if ((buf = xen_read_node(path, &len)) == NULL)
		goto out;

	if (cmdline_parse_etheraddr(NULL, buf, &vring->addr) < 0)
		goto out;
	ret = 0;
out:
	if (buf)
		free(buf);
	return ret;
}


int
parse_vringnode(struct xen_guest *guest, uint32_t virtio_idx)
{
	char path[PATH_MAX] = {0};
	struct xen_gntnode *rx_gntnode = NULL;
	struct xen_gntnode *tx_gntnode = NULL;
	struct xen_vring *vring = NULL;

	/*check if null terminated */
	snprintf(path, sizeof(path),
		XEN_VM_ROOTNODE_FMT"/%d_"XEN_RXVRING_SUFFIX,
		guest->dom_id,
		virtio_idx);

	RTE_LOG(INFO, XENHOST, "  %s: virtio %u parse rx gntnode %s\n", __func__, virtio_idx, path);
	rx_gntnode = parse_gntnode(guest->dom_id, path);
	if (rx_gntnode == NULL)
		goto err;

	/*check if null terminated */
	snprintf(path, sizeof(path),
		XEN_VM_ROOTNODE_FMT"/%d_"XEN_TXVRING_SUFFIX,
		guest->dom_id,
		virtio_idx);

	RTE_LOG(INFO, XENHOST, "  %s: virtio %u parse tx gntnode %s\n", __func__, virtio_idx, path);
	tx_gntnode = parse_gntnode(guest->dom_id, path);
	if (tx_gntnode == NULL)
		goto err;

	vring = &guest->vring[virtio_idx];
	bzero(vring, sizeof(*vring));
	vring->dom_id = guest->dom_id;
	vring->virtio_idx = virtio_idx;

	if (xen_parse_etheraddr(vring) != 0)
		goto err;

	RTE_LOG(INFO, XENHOST, "  %s: virtio %u map rx gntnode %s\n", __func__, virtio_idx, path);
	if (xen_map_rxvringnode(rx_gntnode, vring) != 0)
		goto err;

	RTE_LOG(INFO, XENHOST, "  %s: virtio %u map tx gntnode %s\n", __func__, virtio_idx, path);
	if (xen_map_txvringnode(tx_gntnode, vring) != 0)
		goto err;

	if (xen_map_vringflag(vring) != 0)
		goto err;

	guest->vring_num++;

	xen_free_gntnode(rx_gntnode);
	xen_free_gntnode(tx_gntnode);

	return 0;

err:
	if (rx_gntnode)
		xen_free_gntnode(rx_gntnode);
	if (tx_gntnode)
		xen_free_gntnode(tx_gntnode);
	if (vring) {
		cleanup_vring(vring);
		bzero(vring, sizeof(*vring));
	}
	return -1;
}

/*
 * Open xen grant dev driver
 * @return
 *  0 on success, -1 on failure.
 */
static int
xen_grant_init(void)
{
	d_fd = open(XEN_GNTDEV_FNAME, O_RDWR);

	return d_fd == -1? (-1): (0);
}

/*
 * Initialise xenstore handle and open grant dev driver.
 * @return
 *  0 on success, -1 on failure.
 */
int
xenhost_init(void)
{
	xs = xs_daemon_open();
	if (xs == NULL) {
		rte_panic("failed initialize xen daemon handler");
		return -1;
	}
	if (xen_grant_init())
		return -1;
	return 0;
}
