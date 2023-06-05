/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#include <rte_memzone.h>
#include <rte_errno.h>

#include "bcm_osal.h"
#include "ecore.h"
#include "ecore_hw.h"
#include "ecore_dev_api.h"
#include "ecore_iov_api.h"
#include "ecore_mcp_api.h"
#include "ecore_l2_api.h"
#include "../qede_sriov.h"

int osal_pf_vf_msg(struct ecore_hwfn *p_hwfn)
{
	int rc;

	rc = qed_schedule_iov(p_hwfn, QED_IOV_WQ_MSG_FLAG);
	if (rc) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Failed to schedule alarm handler rc=%d\n", rc);
	}

	return rc;
}

void osal_vf_flr_update(struct ecore_hwfn *p_hwfn)
{
	qed_schedule_iov(p_hwfn, QED_IOV_WQ_FLR_FLAG);
}

void osal_poll_mode_dpc(osal_int_ptr_t hwfn_cookie)
{
	struct ecore_hwfn *p_hwfn = (struct ecore_hwfn *)hwfn_cookie;

	if (!p_hwfn)
		return;

	OSAL_SPIN_LOCK(&p_hwfn->spq_lock);
	ecore_int_sp_dpc((osal_int_ptr_t)(p_hwfn));
	OSAL_SPIN_UNLOCK(&p_hwfn->spq_lock);
}

/* Array of memzone pointers */
static const struct rte_memzone **ecore_mz_mapping;
/* Counter to track current memzone allocated */
static uint16_t ecore_mz_count;

static uint32_t ref_cnt;

int ecore_mz_mapping_alloc(void)
{
	if (__atomic_fetch_add(&ref_cnt, 1, __ATOMIC_RELAXED) == 0) {
		ecore_mz_mapping = rte_calloc("ecore_mz_map",
				rte_memzone_max_get(), sizeof(struct rte_memzone *), 0);
	}

	if (!ecore_mz_mapping)
		return -ENOMEM;

	return 0;
}

void ecore_mz_mapping_free(void)
{
	if (__atomic_fetch_sub(&ref_cnt, 1, __ATOMIC_RELAXED) - 1 == 0) {
		if (ecore_mz_mapping)
			rte_free(ecore_mz_mapping);
		ecore_mz_mapping = NULL;
	}
}

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

inline u32 qede_find_first_zero_bit(u32 *addr, u32 limit)
{
	u32 i;
	u32 nwords = 0;
	OSAL_BUILD_BUG_ON(!limit);
	nwords = (limit - 1) / OSAL_BITS_PER_UL + 1;
	for (i = 0; i < nwords && ~(addr[i]) == 0; i++);
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

	if (ecore_mz_count >= rte_memzone_max_get()) {
		DP_ERR(p_dev, "Memzone allocation count exceeds %zu\n",
		       rte_memzone_max_get());
		*phys = 0;
		return OSAL_NULL;
	}

	OSAL_MEM_ZERO(mz_name, sizeof(*mz_name));
	snprintf(mz_name, sizeof(mz_name), "%lx",
					(unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = rte_get_main_lcore();
	socket_id = rte_lcore_to_socket_id(core_id);
	mz = rte_memzone_reserve_aligned(mz_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
	if (!mz) {
		DP_ERR(p_dev, "Unable to allocate DMA memory "
		       "of size %zu bytes - %s\n",
		       size, rte_strerror(rte_errno));
		*phys = 0;
		return OSAL_NULL;
	}
	*phys = mz->iova;
	ecore_mz_mapping[ecore_mz_count++] = mz;
	DP_VERBOSE(p_dev, ECORE_MSG_SP,
		   "Allocated dma memory size=%zu phys=0x%lx"
		   " virt=%p core=%d\n",
		   mz->len, (unsigned long)mz->iova, mz->addr, core_id);
	return mz->addr;
}

void *osal_dma_alloc_coherent_aligned(struct ecore_dev *p_dev,
				      dma_addr_t *phys, size_t size, int align)
{
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t core_id = rte_lcore_id();
	unsigned int socket_id;

	if (ecore_mz_count >= rte_memzone_max_get()) {
		DP_ERR(p_dev, "Memzone allocation count exceeds %zu\n",
		       rte_memzone_max_get());
		*phys = 0;
		return OSAL_NULL;
	}

	OSAL_MEM_ZERO(mz_name, sizeof(*mz_name));
	snprintf(mz_name, sizeof(mz_name), "%lx",
					(unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = rte_get_main_lcore();
	socket_id = rte_lcore_to_socket_id(core_id);
	mz = rte_memzone_reserve_aligned(mz_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, align);
	if (!mz) {
		DP_ERR(p_dev, "Unable to allocate DMA memory "
		       "of size %zu bytes - %s\n",
		       size, rte_strerror(rte_errno));
		*phys = 0;
		return OSAL_NULL;
	}
	*phys = mz->iova;
	ecore_mz_mapping[ecore_mz_count++] = mz;
	DP_VERBOSE(p_dev, ECORE_MSG_SP,
		   "Allocated aligned dma memory size=%zu phys=0x%lx"
		   " virt=%p core=%d\n",
		   mz->len, (unsigned long)mz->iova, mz->addr, core_id);
	return mz->addr;
}

void osal_dma_free_mem(struct ecore_dev *p_dev, dma_addr_t phys)
{
	uint16_t j;

	for (j = 0 ; j < ecore_mz_count; j++) {
		if (phys == ecore_mz_mapping[j]->iova) {
			DP_VERBOSE(p_dev, ECORE_MSG_SP,
				"Free memzone %s\n", ecore_mz_mapping[j]->name);
			rte_memzone_free(ecore_mz_mapping[j]);
			while (j < ecore_mz_count - 1) {
				ecore_mz_mapping[j] = ecore_mz_mapping[j + 1];
				j++;
			}
			ecore_mz_count--;
			return;
		}
	}

	DP_ERR(p_dev, "Unexpected memory free request\n");
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

		/* @DPDK */
		stats->lan_stats.ucast_rx_pkts = lan_stats.common.rx_ucast_pkts;
		stats->lan_stats.ucast_tx_pkts = lan_stats.common.tx_ucast_pkts;

		stats->lan_stats.fcs_err = -1;
	} else {
		DP_INFO(edev, "Statistics request type %d not supported\n",
		       type);
	}
}

static void qede_hw_err_handler(void *dev, enum ecore_hw_err_type err_type)
{
	struct ecore_dev *edev = dev;

	switch (err_type) {
	case ECORE_HW_ERR_FAN_FAIL:
		break;

	case ECORE_HW_ERR_MFW_RESP_FAIL:
	case ECORE_HW_ERR_HW_ATTN:
	case ECORE_HW_ERR_DMAE_FAIL:
	case ECORE_HW_ERR_RAMROD_FAIL:
	case ECORE_HW_ERR_FW_ASSERT:
		OSAL_SAVE_FW_DUMP(0); /* Using port 0 as default port_id */
		break;

	default:
		DP_NOTICE(edev, false, "Unknown HW error [%d]\n", err_type);
		return;
	}
}

void
qede_hw_err_notify(struct ecore_hwfn *p_hwfn, enum ecore_hw_err_type err_type)
{
	char err_str[64];

	switch (err_type) {
	case ECORE_HW_ERR_FAN_FAIL:
		strcpy(err_str, "Fan Failure");
		break;
	case ECORE_HW_ERR_MFW_RESP_FAIL:
		strcpy(err_str, "MFW Response Failure");
		break;
	case ECORE_HW_ERR_HW_ATTN:
		strcpy(err_str, "HW Attention");
		break;
	case ECORE_HW_ERR_DMAE_FAIL:
		strcpy(err_str, "DMAE Failure");
		break;
	case ECORE_HW_ERR_RAMROD_FAIL:
		strcpy(err_str, "Ramrod Failure");
		break;
	case ECORE_HW_ERR_FW_ASSERT:
		strcpy(err_str, "FW Assertion");
		break;
	default:
		strcpy(err_str, "Unknown");
	}

	DP_ERR(p_hwfn, "HW error occurred [%s]\n", err_str);

	qede_hw_err_handler(p_hwfn->p_dev, err_type);

	ecore_int_attn_clr_enable(p_hwfn->p_dev, true);
}

u32 qede_crc32(u32 crc, u8 *ptr, u32 length)
{
	int i;

	while (length--) {
		crc ^= *ptr++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

void qed_set_platform_str(struct ecore_hwfn *p_hwfn,
			  char *buf_str, u32 buf_size)
{
	snprintf(buf_str, buf_size, "%s.", rte_version());
}
