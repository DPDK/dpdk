/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include <rte_memzone.h>
#include <rte_errno.h>

#include "bcm_osal.h"
#include "ecore.h"
#include "ecore_hw.h"
#include "ecore_iov_api.h"
#include "ecore_mcp_api.h"
#include "ecore_l2_api.h"


unsigned long qede_log2_align(unsigned long n)
{
	unsigned long ret = n ? 1 : 0;
	unsigned long _n = n >> 1;

	while (_n) {
		_n >>= 1;
		ret <<= 1;
	}

	if (ret < n)
		ret <<= 1;

	return ret;
}

u32 qede_osal_log2(u32 val)
{
	u32 log = 0;

	while (val >>= 1)
		log++;

	return log;
}

inline void qede_set_bit(u32 nr, unsigned long *addr)
{
	__sync_fetch_and_or(addr, (1UL << nr));
}

inline void qede_clr_bit(u32 nr, unsigned long *addr)
{
	__sync_fetch_and_and(addr, ~(1UL << nr));
}

inline bool qede_test_bit(u32 nr, unsigned long *addr)
{
	bool res;

	rte_mb();
	res = ((*addr) & (1UL << nr)) != 0;
	rte_mb();
	return res;
}

static inline u32 qede_ffb(unsigned long word)
{
	unsigned long first_bit;

	first_bit = __builtin_ffsl(word);
	return first_bit ? (first_bit - 1) : OSAL_BITS_PER_UL;
}

inline u32 qede_find_first_bit(unsigned long *addr, u32 limit)
{
	u32 i;
	u32 nwords = 0;
	OSAL_BUILD_BUG_ON(!limit);
	nwords = (limit - 1) / OSAL_BITS_PER_UL + 1;
	for (i = 0; i < nwords; i++)
		if (addr[i] != 0)
			break;

	return (i == nwords) ? limit : i * OSAL_BITS_PER_UL + qede_ffb(addr[i]);
}

static inline u32 qede_ffz(unsigned long word)
{
	unsigned long first_zero;

	first_zero = __builtin_ffsl(~word);
	return first_zero ? (first_zero - 1) : OSAL_BITS_PER_UL;
}

inline u32 qede_find_first_zero_bit(unsigned long *addr, u32 limit)
{
	u32 i;
	u32 nwords = 0;
	OSAL_BUILD_BUG_ON(!limit);
	nwords = (limit - 1) / OSAL_BITS_PER_UL + 1;
	for (i = 0; i < nwords; i++)
		if (~(addr[i] != 0))
			break;
	return (i == nwords) ? limit : i * OSAL_BITS_PER_UL + qede_ffz(addr[i]);
}

void qede_vf_fill_driver_data(struct ecore_hwfn *hwfn,
			      __rte_unused struct vf_pf_resc_request *resc_req,
			      struct ecore_vf_acquire_sw_info *vf_sw_info)
{
	vf_sw_info->os_type = VFPF_ACQUIRE_OS_LINUX_USERSPACE;
	vf_sw_info->override_fw_version = 1;
}

void *osal_dma_alloc_coherent(struct ecore_dev *p_dev,
			      dma_addr_t *phys, size_t size)
{
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t core_id = rte_lcore_id();
	unsigned int socket_id;

	OSAL_MEM_ZERO(mz_name, sizeof(*mz_name));
	snprintf(mz_name, sizeof(mz_name) - 1, "%lx",
					(unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = 0;
	socket_id = rte_lcore_to_socket_id(core_id);
	mz = rte_memzone_reserve_aligned(mz_name, size,
					 socket_id, 0, RTE_CACHE_LINE_SIZE);
	if (!mz) {
		DP_ERR(p_dev, "Unable to allocate DMA memory "
		       "of size %zu bytes - %s\n",
		       size, rte_strerror(rte_errno));
		*phys = 0;
		return OSAL_NULL;
	}
	*phys = mz->phys_addr;
	DP_VERBOSE(p_dev, ECORE_MSG_PROBE,
		   "size=%zu phys=0x%" PRIx64 " virt=%p on socket=%u\n",
		   mz->len, mz->phys_addr, mz->addr, socket_id);
	return mz->addr;
}

void *osal_dma_alloc_coherent_aligned(struct ecore_dev *p_dev,
				      dma_addr_t *phys, size_t size, int align)
{
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t core_id = rte_lcore_id();
	unsigned int socket_id;

	OSAL_MEM_ZERO(mz_name, sizeof(*mz_name));
	snprintf(mz_name, sizeof(mz_name) - 1, "%lx",
					(unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = 0;
	socket_id = rte_lcore_to_socket_id(core_id);
	mz = rte_memzone_reserve_aligned(mz_name, size, socket_id, 0, align);
	if (!mz) {
		DP_ERR(p_dev, "Unable to allocate DMA memory "
		       "of size %zu bytes - %s\n",
		       size, rte_strerror(rte_errno));
		*phys = 0;
		return OSAL_NULL;
	}
	*phys = mz->phys_addr;
	DP_VERBOSE(p_dev, ECORE_MSG_PROBE,
		   "aligned memory size=%zu phys=0x%" PRIx64 " virt=%p core=%d\n",
		   mz->len, mz->phys_addr, mz->addr, core_id);
	return mz->addr;
}

#ifdef CONFIG_ECORE_ZIPPED_FW
u32 qede_unzip_data(struct ecore_hwfn *p_hwfn, u32 input_len,
		    u8 *input_buf, u32 max_size, u8 *unzip_buf)
{
	int rc;

	p_hwfn->stream->next_in = input_buf;
	p_hwfn->stream->avail_in = input_len;
	p_hwfn->stream->next_out = unzip_buf;
	p_hwfn->stream->avail_out = max_size;

	rc = inflateInit2(p_hwfn->stream, MAX_WBITS);

	if (rc != Z_OK) {
		DP_ERR(p_hwfn,
			   "zlib init failed, rc = %d\n", rc);
		return 0;
	}

	rc = inflate(p_hwfn->stream, Z_FINISH);
	inflateEnd(p_hwfn->stream);

	if (rc != Z_OK && rc != Z_STREAM_END) {
		DP_ERR(p_hwfn,
			   "FW unzip error: %s, rc=%d\n", p_hwfn->stream->msg,
			   rc);
		return 0;
	}

	return p_hwfn->stream->total_out / 4;
}
#endif

void
qede_get_mcp_proto_stats(struct ecore_dev *edev,
			 enum ecore_mcp_protocol_type type,
			 union ecore_mcp_protocol_stats *stats)
{
	struct ecore_eth_stats lan_stats;

	if (type == ECORE_MCP_LAN_STATS) {
		ecore_get_vport_stats(edev, &lan_stats);
		stats->lan_stats.ucast_rx_pkts = lan_stats.rx_ucast_pkts;
		stats->lan_stats.ucast_tx_pkts = lan_stats.tx_ucast_pkts;
		stats->lan_stats.fcs_err = -1;
	} else {
		DP_INFO(edev, "Statistics request type %d not supported\n",
		       type);
	}
}
