/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include<rte_ethdev_driver.h>
#include <rte_bus_pci.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_wq.h"
#include "hinic_pmd_cmdq.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_niccfg.h"
#include "hinic_pmd_mbox.h"

#define HINIC_DEAULT_EQ_MSIX_PENDING_LIMIT		0
#define HINIC_DEAULT_EQ_MSIX_COALESC_TIMER_CFG		0xFF
#define HINIC_DEAULT_EQ_MSIX_RESEND_TIMER_CFG		7

#define HINIC_FLR_TIMEOUT				1000

#define FFM_RECORD_NUM_MAX				32

#define HINIC_DMA_ATTR_ENTRY_ST_SHIFT			0
#define HINIC_DMA_ATTR_ENTRY_AT_SHIFT			8
#define HINIC_DMA_ATTR_ENTRY_PH_SHIFT			10
#define HINIC_DMA_ATTR_ENTRY_NO_SNOOPING_SHIFT		12
#define HINIC_DMA_ATTR_ENTRY_TPH_EN_SHIFT		13

#define HINIC_DMA_ATTR_ENTRY_ST_MASK			0xFF
#define HINIC_DMA_ATTR_ENTRY_AT_MASK			0x3
#define HINIC_DMA_ATTR_ENTRY_PH_MASK			0x3
#define HINIC_DMA_ATTR_ENTRY_NO_SNOOPING_MASK		0x1
#define HINIC_DMA_ATTR_ENTRY_TPH_EN_MASK		0x1

#define HINIC_DMA_ATTR_ENTRY_SET(val, member)			\
		(((u32)(val) & HINIC_DMA_ATTR_ENTRY_##member##_MASK) << \
			HINIC_DMA_ATTR_ENTRY_##member##_SHIFT)

#define HINIC_DMA_ATTR_ENTRY_CLEAR(val, member)		\
		((val) & (~(HINIC_DMA_ATTR_ENTRY_##member##_MASK	\
			<< HINIC_DMA_ATTR_ENTRY_##member##_SHIFT)))

#define HINIC_PCIE_ST_DISABLE				0
#define HINIC_PCIE_AT_DISABLE				0
#define HINIC_PCIE_PH_DISABLE				0
#define PCIE_MSIX_ATTR_ENTRY				0

#define HINIC_HASH_FUNC					rte_jhash
#define HINIC_HASH_KEY_LEN				(sizeof(dma_addr_t))
#define HINIC_HASH_FUNC_INIT_VAL			0

static const char *__hw_to_char_fec[HILINK_FEC_MAX_TYPE] = {
	"RS-FEC", "BASE-FEC", "NO-FEC"};

static const char *__hw_to_char_port_type[LINK_PORT_MAX_TYPE] = {
	"Unknown", "Fibre", "Electric", "Direct Attach Copper", "AOC",
	"Back plane", "BaseT"
};

static const char *hinic_module_link_err[LINK_ERR_NUM] = {
	"Unrecognized module",
};

struct hinic_vf_dma_attr_table {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	func_dma_entry_num;
	u8	entry_idx;
	u8	st;
	u8	at;
	u8	ph;
	u8	no_snooping;
	u8	tph_en;
	u8	resv1[3];
};

/**
 * hinic_cpu_to_be32 - convert data to big endian 32 bit format
 * @data: the data to convert
 * @len: length of data to convert, must be Multiple of 4B
 */
void hinic_cpu_to_be32(void *data, u32 len)
{
	u32 i;
	u32 *mem = (u32 *)data;

	for (i = 0; i < (len >> 2); i++) {
		*mem = cpu_to_be32(*mem);
		mem++;
	}
}

/**
 * hinic_be32_to_cpu - convert data from big endian 32 bit format
 * @data: the data to convert
 * @len: length of data to convert, must be Multiple of 4B
 */
void hinic_be32_to_cpu(void *data, u32 len)
{
	u32 i;
	u32 *mem = (u32 *)data;

	for (i = 0; i < (len >> 2); i++) {
		*mem = be32_to_cpu(*mem);
		mem++;
	}
}

static void *
hinic_dma_mem_zalloc(struct hinic_hwdev *hwdev, size_t size,
		dma_addr_t *dma_handle, unsigned int flag, unsigned int align)
{
	int rc, alloc_cnt;
	const struct rte_memzone *mz;
	char z_name[RTE_MEMZONE_NAMESIZE];
	hash_sig_t sig;
	rte_iova_t iova;

	if (dma_handle == NULL || 0 == size)
		return NULL;

	alloc_cnt = rte_atomic32_add_return(&hwdev->os_dep.dma_alloc_cnt, 1);
	snprintf(z_name, sizeof(z_name), "%s_%d",
		 hwdev->pcidev_hdl->name, alloc_cnt);

	mz = rte_memzone_reserve_aligned(z_name, size, SOCKET_ID_ANY,
					 flag, align);
	if (!mz) {
		PMD_DRV_LOG(ERR, "Alloc dma able memory failed, errno: %d, ma_name: %s, size: 0x%zx",
			    rte_errno, z_name, size);
		return NULL;
	}

	iova = mz->iova;

	/* check if phys_addr already exist */
	sig = HINIC_HASH_FUNC(&iova, HINIC_HASH_KEY_LEN,
			      HINIC_HASH_FUNC_INIT_VAL);
	rc = rte_hash_lookup_with_hash(hwdev->os_dep.dma_addr_hash,
				       &iova, sig);
	if (rc >= 0) {
		PMD_DRV_LOG(ERR, "Dma addr: %p already in hash table, error: %d, mz_name: %s",
			(void *)iova, rc, z_name);
		goto phys_addr_hash_err;
	}

	/* record paddr in hash table */
	rte_spinlock_lock(&hwdev->os_dep.dma_hash_lock);
	rc = rte_hash_add_key_with_hash_data(hwdev->os_dep.dma_addr_hash,
					     &iova, sig,
					     (void *)(u64)mz);
	rte_spinlock_unlock(&hwdev->os_dep.dma_hash_lock);
	if (rc) {
		PMD_DRV_LOG(ERR, "Insert dma addr: %p hash failed, error: %d, mz_name: %s",
			(void *)iova, rc, z_name);
		goto phys_addr_hash_err;
	}
	*dma_handle = iova;
	memset(mz->addr, 0, size);

	return mz->addr;

phys_addr_hash_err:
	(void)rte_memzone_free(mz);

	return NULL;
}

static void
hinic_dma_mem_free(struct hinic_hwdev *hwdev, size_t size,
		   void *virt, dma_addr_t phys)
{
	int rc;
	struct rte_memzone *mz = NULL;
	struct rte_hash *hash;
	hash_sig_t sig;

	if (virt == NULL || phys == 0)
		return;

	hash = hwdev->os_dep.dma_addr_hash;
	sig = HINIC_HASH_FUNC(&phys, HINIC_HASH_KEY_LEN,
			      HINIC_HASH_FUNC_INIT_VAL);
	rc = rte_hash_lookup_with_hash_data(hash, &phys, sig, (void **)&mz);
	if (rc < 0) {
		PMD_DRV_LOG(ERR, "Can not find phys_addr: %p, error: %d",
			(void *)phys, rc);
		return;
	}

	if (virt != mz->addr || size > mz->len) {
		PMD_DRV_LOG(ERR, "Match mz_info failed: "
			"mz.name: %s, mz.phys: %p, mz.virt: %p, mz.len: %zu, "
			"phys: %p, virt: %p, size: %zu",
			mz->name, (void *)mz->iova, mz->addr, mz->len,
			(void *)phys, virt, size);
	}

	rte_spinlock_lock(&hwdev->os_dep.dma_hash_lock);
	(void)rte_hash_del_key_with_hash(hash, &phys, sig);
	rte_spinlock_unlock(&hwdev->os_dep.dma_hash_lock);

	(void)rte_memzone_free(mz);
}

void *dma_zalloc_coherent(void *hwdev, size_t size,
			  dma_addr_t *dma_handle, gfp_t flag)
{
	return hinic_dma_mem_zalloc(hwdev, size, dma_handle, flag,
				    RTE_CACHE_LINE_SIZE);
}

void *dma_zalloc_coherent_aligned(void *hwdev, size_t size,
				  dma_addr_t *dma_handle, gfp_t flag)
{
	return hinic_dma_mem_zalloc(hwdev, size, dma_handle, flag,
				    HINIC_PAGE_SIZE);
}

void *dma_zalloc_coherent_aligned256k(void *hwdev, size_t size,
				      dma_addr_t *dma_handle, gfp_t flag)
{
	return hinic_dma_mem_zalloc(hwdev, size, dma_handle, flag,
				    HINIC_PAGE_SIZE * 64);
}

void dma_free_coherent(void *hwdev, size_t size, void *virt, dma_addr_t phys)
{
	hinic_dma_mem_free(hwdev, size, virt, phys);
}

void dma_free_coherent_volatile(void *hwdev, size_t size,
				volatile void *virt, dma_addr_t phys)
{
	int rc;
	struct rte_memzone *mz = NULL;
	struct hinic_hwdev *dev = hwdev;
	struct rte_hash *hash;
	hash_sig_t sig;

	if (virt == NULL || phys == 0)
		return;

	hash = dev->os_dep.dma_addr_hash;
	sig = HINIC_HASH_FUNC(&phys, HINIC_HASH_KEY_LEN,
			      HINIC_HASH_FUNC_INIT_VAL);
	rc = rte_hash_lookup_with_hash_data(hash, &phys, sig, (void **)&mz);
	if (rc < 0) {
		PMD_DRV_LOG(ERR, "Can not find phys_addr: %p, error: %d",
			(void *)phys, rc);
		return;
	}

	if (virt != mz->addr || size > mz->len) {
		PMD_DRV_LOG(ERR, "Match mz_info failed: "
			"mz.name:%s, mz.phys:%p, mz.virt:%p, mz.len:%zu, "
			"phys:%p, virt:%p, size:%zu",
			mz->name, (void *)mz->iova, mz->addr, mz->len,
			(void *)phys, virt, size);
	}

	rte_spinlock_lock(&dev->os_dep.dma_hash_lock);
	(void)rte_hash_del_key_with_hash(hash, &phys, sig);
	rte_spinlock_unlock(&dev->os_dep.dma_hash_lock);

	(void)rte_memzone_free(mz);
}

struct dma_pool *dma_pool_create(const char *name, void *dev,
				 size_t size, size_t align, size_t boundary)
{
	struct pci_pool *pool;

	pool = rte_zmalloc(NULL, sizeof(*pool), HINIC_MEM_ALLOC_ALIGN_MIN);
	if (!pool)
		return NULL;

	rte_atomic32_set(&pool->inuse, 0);
	pool->elem_size = size;
	pool->align = align;
	pool->boundary = boundary;
	pool->hwdev = dev;
	strncpy(pool->name, name, (sizeof(pool->name) - 1));

	return pool;
}

void dma_pool_destroy(struct dma_pool *pool)
{
	if (!pool)
		return;

	if (rte_atomic32_read(&pool->inuse) != 0) {
		PMD_DRV_LOG(ERR, "Leak memory, dma_pool:%s, inuse_count:%d",
			    pool->name, rte_atomic32_read(&pool->inuse));
	}

	rte_free(pool);
}

void *dma_pool_alloc(struct pci_pool *pool, int flags, dma_addr_t *dma_addr)
{
	void *buf;

	buf = hinic_dma_mem_zalloc(pool->hwdev, pool->elem_size,
				   dma_addr, flags, (u32)pool->align);
	if (buf)
		rte_atomic32_inc(&pool->inuse);

	return buf;
}

void dma_pool_free(struct pci_pool *pool, void *vaddr, dma_addr_t dma)
{
	rte_atomic32_dec(&pool->inuse);
	hinic_dma_mem_free(pool->hwdev, pool->elem_size, vaddr, dma);
}

#define HINIC_MAX_DMA_ENTRIES		8192
int hinic_osdep_init(struct hinic_hwdev *hwdev)
{
	struct rte_hash_parameters dh_params = { 0 };
	struct rte_hash *paddr_hash = NULL;

	rte_atomic32_set(&hwdev->os_dep.dma_alloc_cnt, 0);
	rte_spinlock_init(&hwdev->os_dep.dma_hash_lock);

	dh_params.name = hwdev->pcidev_hdl->name;
	dh_params.entries = HINIC_MAX_DMA_ENTRIES;
	dh_params.key_len = HINIC_HASH_KEY_LEN;
	dh_params.hash_func = HINIC_HASH_FUNC;
	dh_params.hash_func_init_val = HINIC_HASH_FUNC_INIT_VAL;
	dh_params.socket_id = SOCKET_ID_ANY;

	paddr_hash = rte_hash_find_existing(dh_params.name);
	if (paddr_hash == NULL) {
		paddr_hash = rte_hash_create(&dh_params);
		if (paddr_hash == NULL) {
			PMD_DRV_LOG(ERR, "Create nic_dev phys_addr hash table failed");
			return -ENOMEM;
		}
	} else {
		PMD_DRV_LOG(INFO, "Using existing dma hash table %s",
			    dh_params.name);
	}
	hwdev->os_dep.dma_addr_hash = paddr_hash;

	return 0;
}

void hinic_osdep_deinit(struct hinic_hwdev *hwdev)
{
	uint32_t iter = 0;
	dma_addr_t key_pa;
	struct rte_memzone *data_mz = NULL;
	struct rte_hash *paddr_hash = hwdev->os_dep.dma_addr_hash;

	if (paddr_hash) {
		/* iterate through the hash table */
		while (rte_hash_iterate(paddr_hash, (const void **)&key_pa,
					(void **)&data_mz, &iter) >= 0) {
			if (data_mz) {
				PMD_DRV_LOG(WARNING, "Free leaked dma_addr: %p, mz: %s",
					(void *)key_pa, data_mz->name);
				(void)rte_memzone_free(data_mz);
			}
		}

		/* free phys_addr hash table */
		rte_hash_free(paddr_hash);
	}
}

/**
 * hinic_set_ci_table - set ci attribute table
 * @hwdev: the hardware interface of a nic device
 * @q_id: Queue id of SQ
 * @attr: Point to SQ CI attribute table
 * @return
 *   0 on success and ci attribute table is filled,
 *   negative error value otherwise.
 */
int hinic_set_ci_table(void *hwdev, u16 q_id, struct hinic_sq_attr *attr)
{
	struct hinic_cons_idx_attr cons_idx_attr;

	memset(&cons_idx_attr, 0, sizeof(cons_idx_attr));
	cons_idx_attr.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	cons_idx_attr.func_idx = hinic_global_func_id(hwdev);
	cons_idx_attr.dma_attr_off  = attr->dma_attr_off;
	cons_idx_attr.pending_limit = attr->pending_limit;
	cons_idx_attr.coalescing_time = attr->coalescing_time;
	if (attr->intr_en) {
		cons_idx_attr.intr_en = attr->intr_en;
		cons_idx_attr.intr_idx = attr->intr_idx;
	}

	cons_idx_attr.l2nic_sqn = attr->l2nic_sqn;
	cons_idx_attr.sq_id = q_id;
	cons_idx_attr.ci_addr = attr->ci_dma_base;

	return hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				      HINIC_MGMT_CMD_L2NIC_SQ_CI_ATTR_SET,
				      &cons_idx_attr, sizeof(cons_idx_attr),
				      NULL, NULL, 0);
}

/**
 * hinic_set_pagesize - set page size to vat table
 * @hwdev: the hardware interface of a nic device
 * @page_size: vat page size
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_set_pagesize(void *hwdev, u8 page_size)
{
	struct hinic_page_size cmd;

	if (page_size > HINIC_PAGE_SIZE_MAX) {
		PMD_DRV_LOG(ERR, "Invalid page_size %u, bigger than %u",
		       page_size, HINIC_PAGE_SIZE_MAX);
		return -EINVAL;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	cmd.func_idx = hinic_global_func_id(hwdev);
	cmd.ppf_idx = hinic_ppf_idx(hwdev);
	cmd.page_size = page_size;

	return hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
					HINIC_MGMT_CMD_PAGESIZE_SET,
					&cmd, sizeof(cmd),
					NULL, NULL, 0);
}

static int wait_for_flr_finish(struct hinic_hwif *hwif)
{
	unsigned long end;
	enum hinic_pf_status status;

	end = jiffies + msecs_to_jiffies(HINIC_FLR_TIMEOUT);
	do {
		status = hinic_get_pf_status(hwif);
		if (status == HINIC_PF_STATUS_FLR_FINISH_FLAG) {
			return 0;
		}

		rte_delay_ms(10);
	} while (time_before(jiffies, end));

	return -EFAULT;
}

#define HINIC_WAIT_CMDQ_IDLE_TIMEOUT		1000

static int wait_cmdq_stop(struct hinic_hwdev *hwdev)
{
	enum hinic_cmdq_type cmdq_type;
	struct hinic_cmdqs *cmdqs = hwdev->cmdqs;
	unsigned long end;
	int err = 0;

	if (!(cmdqs->status & HINIC_CMDQ_ENABLE))
		return 0;

	cmdqs->status &= ~HINIC_CMDQ_ENABLE;

	end = jiffies + msecs_to_jiffies(HINIC_WAIT_CMDQ_IDLE_TIMEOUT);
	do {
		err = 0;
		cmdq_type = HINIC_CMDQ_SYNC;
		for (; cmdq_type < HINIC_MAX_CMDQ_TYPES; cmdq_type++) {
			if (!hinic_cmdq_idle(&cmdqs->cmdq[cmdq_type])) {
				err = -EBUSY;
				break;
			}
		}

		if (!err)
			return 0;

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	cmdqs->status |= HINIC_CMDQ_ENABLE;

	return err;
}

static int hinic_vf_rx_tx_flush(struct hinic_hwdev *hwdev)
{
	struct hinic_clear_resource clr_res;
	int err;

	err = wait_cmdq_stop(hwdev);
	if (err) {
		PMD_DRV_LOG(WARNING, "Cmdq is still working");
		return err;
	}

	memset(&clr_res, 0, sizeof(clr_res));
	clr_res.func_idx = HINIC_HWIF_GLOBAL_IDX(hwdev->hwif);
	clr_res.ppf_idx  = HINIC_HWIF_PPF_IDX(hwdev->hwif);
	err = hinic_mbox_to_pf_no_ack(hwdev, HINIC_MOD_COMM,
		HINIC_MGMT_CMD_START_FLR, &clr_res, sizeof(clr_res));
	if (err)
		PMD_DRV_LOG(WARNING, "Notice flush message failed");

	/*
	 * PF firstly set VF doorbell flush csr to be disabled. After PF finish
	 * VF resources flush, PF will set VF doorbell flush csr to be enabled.
	 */
	err = wait_until_doorbell_flush_states(hwdev->hwif, DISABLE_DOORBELL);
	if (err)
		PMD_DRV_LOG(WARNING, "Wait doorbell flush disable timeout");

	err = wait_until_doorbell_flush_states(hwdev->hwif, ENABLE_DOORBELL);
	if (err)
		PMD_DRV_LOG(WARNING, "Wait doorbell flush enable timeout");

	err = hinic_reinit_cmdq_ctxts(hwdev);
	if (err)
		PMD_DRV_LOG(WARNING, "Reinit cmdq failed");

	return err;
}

/**
 * hinic_pf_rx_tx_flush - clean up hardware resource
 * @hwdev: the hardware interface of a nic device
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_pf_rx_tx_flush(struct hinic_hwdev *hwdev)
{
	struct hinic_hwif *hwif = hwdev->hwif;
	struct hinic_clear_doorbell clear_db;
	struct hinic_clear_resource clr_res;
	int err;

	rte_delay_ms(100);

	err = wait_cmdq_stop(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq is still working");
		return err;
	}

	hinic_disable_doorbell(hwif);
	memset(&clear_db, 0, sizeof(clear_db));
	clear_db.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	clear_db.func_idx = HINIC_HWIF_GLOBAL_IDX(hwif);
	clear_db.ppf_idx  = HINIC_HWIF_PPF_IDX(hwif);
	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_FLUSH_DOORBELL, &clear_db,
				     sizeof(clear_db), NULL, NULL, 0);
	if (err)
		PMD_DRV_LOG(WARNING, "Flush doorbell failed");

	hinic_set_pf_status(hwif, HINIC_PF_STATUS_FLR_START_FLAG);
	memset(&clr_res, 0, sizeof(clr_res));
	clr_res.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	clr_res.func_idx = HINIC_HWIF_GLOBAL_IDX(hwif);
	clr_res.ppf_idx  = HINIC_HWIF_PPF_IDX(hwif);

	err = hinic_msg_to_mgmt_no_ack(hwdev, HINIC_MOD_COMM,
				       HINIC_MGMT_CMD_START_FLR, &clr_res,
				       sizeof(clr_res), NULL, NULL);
	if (err)
		PMD_DRV_LOG(WARNING, "Notice flush message failed");

	err = wait_for_flr_finish(hwif);
	if (err)
		PMD_DRV_LOG(WARNING, "Wait firmware FLR timeout");

	hinic_enable_doorbell(hwif);

	err = hinic_reinit_cmdq_ctxts(hwdev);
	if (err)
		PMD_DRV_LOG(WARNING, "Reinit cmdq failed");

	return 0;
}

int hinic_func_rx_tx_flush(struct hinic_hwdev *hwdev)
{
	if (HINIC_FUNC_TYPE(hwdev) == TYPE_VF)
		return hinic_vf_rx_tx_flush(hwdev);
	else
		return hinic_pf_rx_tx_flush(hwdev);
}

/**
 * hinic_get_interrupt_cfg - get interrupt configuration from NIC
 * @hwdev: the hardware interface of a nic device
 * @interrupt_info: Information of Interrupt aggregation
 * Return: 0 on success, negative error value otherwise.
 */
static int hinic_get_interrupt_cfg(struct hinic_hwdev *hwdev,
				struct nic_interrupt_info *interrupt_info)
{
	struct hinic_msix_config msix_cfg;
	u16 out_size = sizeof(msix_cfg);
	int err;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	msix_cfg.func_id = hinic_global_func_id(hwdev);
	msix_cfg.msix_index = interrupt_info->msix_index;

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_MSI_CTRL_REG_RD_BY_UP,
				     &msix_cfg, sizeof(msix_cfg),
				     &msix_cfg, &out_size, 0);
	if (err || !out_size || msix_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Get interrupt config failed, ret: %d",
			msix_cfg.mgmt_msg_head.status);
		return -EINVAL;
	}

	interrupt_info->lli_credit_limit = msix_cfg.lli_credit_cnt;
	interrupt_info->lli_timer_cfg = msix_cfg.lli_tmier_cnt;
	interrupt_info->pending_limt = msix_cfg.pending_cnt;
	interrupt_info->coalesc_timer_cfg = msix_cfg.coalesct_timer_cnt;
	interrupt_info->resend_timer_cfg = msix_cfg.resend_timer_cnt;
	return 0;
}

/**
 * hinic_set_interrupt_cfg - set interrupt configuration to NIC
 * @hwdev: the hardware interface of a nic device
 * @interrupt_info: Information of Interrupt aggregation
 * Return: 0 on success, negative error value otherwise.
 */
int hinic_set_interrupt_cfg(struct hinic_hwdev *hwdev,
			    struct nic_interrupt_info interrupt_info)
{
	struct hinic_msix_config msix_cfg;
	struct nic_interrupt_info temp_info;
	u16 out_size = sizeof(msix_cfg);
	int err;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	msix_cfg.func_id = hinic_global_func_id(hwdev);
	msix_cfg.msix_index = (u16)interrupt_info.msix_index;

	temp_info.msix_index = interrupt_info.msix_index;

	err = hinic_get_interrupt_cfg(hwdev, &temp_info);
	if (err)
		return -EINVAL;

	msix_cfg.lli_credit_cnt = temp_info.lli_credit_limit;
	msix_cfg.lli_tmier_cnt = temp_info.lli_timer_cfg;
	msix_cfg.pending_cnt = temp_info.pending_limt;
	msix_cfg.coalesct_timer_cnt = temp_info.coalesc_timer_cfg;
	msix_cfg.resend_timer_cnt = temp_info.resend_timer_cfg;

	if (interrupt_info.lli_set) {
		msix_cfg.lli_credit_cnt = interrupt_info.lli_credit_limit;
		msix_cfg.lli_tmier_cnt = interrupt_info.lli_timer_cfg;
	}

	if (interrupt_info.interrupt_coalesc_set) {
		msix_cfg.pending_cnt = interrupt_info.pending_limt;
		msix_cfg.coalesct_timer_cnt = interrupt_info.coalesc_timer_cfg;
		msix_cfg.resend_timer_cnt = interrupt_info.resend_timer_cfg;
	}

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_MSI_CTRL_REG_WR_BY_UP,
				     &msix_cfg, sizeof(msix_cfg),
				     &msix_cfg, &out_size, 0);
	if (err || !out_size || msix_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Set interrupt config failed, ret: %d",
			msix_cfg.mgmt_msg_head.status);
		return -EINVAL;
	}

	return 0;
}

/**
 * init_aeqs_msix_attr - Init interrupt attributes of aeq
 * @hwdev: the hardware interface of a nic device
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int init_aeqs_msix_attr(void *hwdev)
{
	struct hinic_hwdev *nic_hwdev = hwdev;
	struct hinic_aeqs *aeqs = nic_hwdev->aeqs;
	struct nic_interrupt_info info = {0};
	struct hinic_eq *eq;
	u16 q_id;
	int err;

	info.lli_set = 0;
	info.interrupt_coalesc_set = 1;
	info.pending_limt = HINIC_DEAULT_EQ_MSIX_PENDING_LIMIT;
	info.coalesc_timer_cfg = HINIC_DEAULT_EQ_MSIX_COALESC_TIMER_CFG;
	info.resend_timer_cfg = HINIC_DEAULT_EQ_MSIX_RESEND_TIMER_CFG;

	for (q_id = 0; q_id < aeqs->num_aeqs; q_id++) {
		eq = &aeqs->aeq[q_id];
		info.msix_index = eq->eq_irq.msix_entry_idx;
		err = hinic_set_interrupt_cfg(hwdev, info);
		if (err) {
			PMD_DRV_LOG(ERR, "Set msix attr for aeq %d failed",
				    q_id);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * set_pf_dma_attr_entry - set the dma attributes for entry
 * @hwdev: the pointer to the private hardware device object
 * @entry_idx: the entry index in the dma table
 * @st: PCIE TLP steering tag
 * @at:	PCIE TLP AT field
 * @ph: PCIE TLP Processing Hint field
 * @no_snooping: PCIE TLP No snooping
 * @tph_en: PCIE TLP Processing Hint Enable
 */
static void set_pf_dma_attr_entry(struct hinic_hwdev *hwdev, u32 entry_idx,
				  u8 st, u8 at, u8 ph,
				  enum hinic_pcie_nosnoop no_snooping,
				  enum hinic_pcie_tph tph_en)
{
	u32 addr, val, dma_attr_entry;

	/* Read Modify Write */
	addr = HINIC_CSR_DMA_ATTR_TBL_ADDR(entry_idx);

	val = hinic_hwif_read_reg(hwdev->hwif, addr);
	val = HINIC_DMA_ATTR_ENTRY_CLEAR(val, ST)	&
		HINIC_DMA_ATTR_ENTRY_CLEAR(val, AT)	&
		HINIC_DMA_ATTR_ENTRY_CLEAR(val, PH)	&
		HINIC_DMA_ATTR_ENTRY_CLEAR(val, NO_SNOOPING)	&
		HINIC_DMA_ATTR_ENTRY_CLEAR(val, TPH_EN);

	dma_attr_entry = HINIC_DMA_ATTR_ENTRY_SET(st, ST)	|
			 HINIC_DMA_ATTR_ENTRY_SET(at, AT)	|
			 HINIC_DMA_ATTR_ENTRY_SET(ph, PH)	|
			 HINIC_DMA_ATTR_ENTRY_SET(no_snooping, NO_SNOOPING) |
			 HINIC_DMA_ATTR_ENTRY_SET(tph_en, TPH_EN);

	val |= dma_attr_entry;
	hinic_hwif_write_reg(hwdev->hwif, addr, val);
}

static int set_vf_dma_attr_entry(struct hinic_hwdev *hwdev, u8 entry_idx,
				u8 st, u8 at, u8 ph,
				enum hinic_pcie_nosnoop no_snooping,
				enum hinic_pcie_tph tph_en)
{
	struct hinic_vf_dma_attr_table attr;

	memset(&attr, 0, sizeof(attr));
	attr.func_idx = hinic_global_func_id(hwdev);
	attr.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	attr.func_dma_entry_num = hinic_dma_attr_entry_num(hwdev);
	attr.entry_idx = entry_idx;
	attr.st = st;
	attr.at = at;
	attr.ph = ph;
	attr.no_snooping = no_snooping;
	attr.tph_en = tph_en;

	return hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
					HINIC_MGMT_CMD_DMA_ATTR_SET,
					&attr, sizeof(attr), NULL, NULL, 0);
}

/**
 * dma_attr_table_init - initialize the the default dma attributes
 * @hwdev: the pointer to the private hardware device object
 */
static int dma_attr_table_init(struct hinic_hwdev *hwdev)
{
	int err = 0;

	if (HINIC_IS_VF(hwdev))
		err = set_vf_dma_attr_entry(hwdev, PCIE_MSIX_ATTR_ENTRY,
				HINIC_PCIE_ST_DISABLE, HINIC_PCIE_AT_DISABLE,
				HINIC_PCIE_PH_DISABLE, HINIC_PCIE_SNOOP,
				HINIC_PCIE_TPH_DISABLE);
	else
		set_pf_dma_attr_entry(hwdev, PCIE_MSIX_ATTR_ENTRY,
				HINIC_PCIE_ST_DISABLE, HINIC_PCIE_AT_DISABLE,
				HINIC_PCIE_PH_DISABLE, HINIC_PCIE_SNOOP,
				HINIC_PCIE_TPH_DISABLE);

	return err;
}

/**
 * hinic_init_attr_table - init dma and aeq msix attribute table
 * @hwdev: the pointer to the private hardware device object
 */
int hinic_init_attr_table(struct hinic_hwdev *hwdev)
{
	int err;

	err = dma_attr_table_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize dma attribute table failed, err: %d",
				err);
		return err;
	}

	err = init_aeqs_msix_attr(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize aeqs msix attribute failed, err: %d",
				err);
		return err;
	}

	return 0;
}

#define FAULT_SHOW_STR_LEN 16
static void fault_report_show(struct hinic_hwdev *hwdev,
			      struct hinic_fault_event *event)
{
	char fault_type[FAULT_TYPE_MAX][FAULT_SHOW_STR_LEN + 1] = {
		"chip", "ucode", "mem rd timeout", "mem wr timeout",
		"reg rd timeout", "reg wr timeout"};
	char fault_level[FAULT_LEVEL_MAX][FAULT_SHOW_STR_LEN + 1] = {
		"fatal", "reset", "flr", "general", "suggestion"};
	char type_str[FAULT_SHOW_STR_LEN + 1] = { 0 };
	char level_str[FAULT_SHOW_STR_LEN + 1] = { 0 };
	u8 err_level;

	PMD_DRV_LOG(WARNING, "Fault event report received, func_id: %d",
		 hinic_global_func_id(hwdev));

	if (event->type < FAULT_TYPE_MAX)
		strncpy(type_str, fault_type[event->type], FAULT_SHOW_STR_LEN);
	else
		strncpy(type_str, "unknown", FAULT_SHOW_STR_LEN);
	PMD_DRV_LOG(WARNING, "fault type:    %d [%s]",
		 event->type, type_str);
	PMD_DRV_LOG(WARNING, "fault val[0]:  0x%08x",
		 event->event.val[0]);
	PMD_DRV_LOG(WARNING, "fault val[1]:  0x%08x",
		 event->event.val[1]);
	PMD_DRV_LOG(WARNING, "fault val[2]:  0x%08x",
		 event->event.val[2]);
	PMD_DRV_LOG(WARNING, "fault val[3]:  0x%08x",
		 event->event.val[3]);

	switch (event->type) {
	case FAULT_TYPE_CHIP:
		err_level = event->event.chip.err_level;
		if (err_level < FAULT_LEVEL_MAX)
			strncpy(level_str, fault_level[err_level],
				FAULT_SHOW_STR_LEN);
		else
			strncpy(level_str, "unknown",
				FAULT_SHOW_STR_LEN);

		PMD_DRV_LOG(WARNING, "err_level:     %d [%s]",
			 err_level, level_str);

		if (err_level == FAULT_LEVEL_SERIOUS_FLR) {
			PMD_DRV_LOG(WARNING, "flr func_id:   %d",
				 event->event.chip.func_id);
		} else {
			PMD_DRV_LOG(WARNING, "node_id:       %d",
				 event->event.chip.node_id);
			PMD_DRV_LOG(WARNING, "err_type:      %d",
				 event->event.chip.err_type);
			PMD_DRV_LOG(WARNING, "err_csr_addr:  %d",
				 event->event.chip.err_csr_addr);
			PMD_DRV_LOG(WARNING, "err_csr_value: %d",
				 event->event.chip.err_csr_value);
		}
		break;
	case FAULT_TYPE_UCODE:
		PMD_DRV_LOG(WARNING, "cause_id:      %d",
			 event->event.ucode.cause_id);
		PMD_DRV_LOG(WARNING, "core_id:       %d",
			 event->event.ucode.core_id);
		PMD_DRV_LOG(WARNING, "c_id:          %d",
			 event->event.ucode.c_id);
		PMD_DRV_LOG(WARNING, "epc:           %d",
			 event->event.ucode.epc);
		break;
	case FAULT_TYPE_MEM_RD_TIMEOUT:
	case FAULT_TYPE_MEM_WR_TIMEOUT:
		PMD_DRV_LOG(WARNING, "err_csr_ctrl:  %d",
			 event->event.mem_timeout.err_csr_ctrl);
		PMD_DRV_LOG(WARNING, "err_csr_data:  %d",
			 event->event.mem_timeout.err_csr_data);
		PMD_DRV_LOG(WARNING, "ctrl_tab:      %d",
			 event->event.mem_timeout.ctrl_tab);
		PMD_DRV_LOG(WARNING, "mem_index:     %d",
			 event->event.mem_timeout.mem_index);
		break;
	case FAULT_TYPE_REG_RD_TIMEOUT:
	case FAULT_TYPE_REG_WR_TIMEOUT:
		PMD_DRV_LOG(WARNING, "err_csr:       %d",
			 event->event.reg_timeout.err_csr);
		break;
	default:
		break;
	}
}

static int resources_state_set(struct hinic_hwdev *hwdev,
			       enum hinic_res_state state)
{
	struct hinic_hwif *hwif = hwdev->hwif;
	struct hinic_cmd_set_res_state res_state;

	memset(&res_state, 0, sizeof(res_state));
	res_state.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	res_state.func_idx = HINIC_HWIF_GLOBAL_IDX(hwif);
	res_state.state = state;

	return hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				 HINIC_MGMT_CMD_RES_STATE_SET,
				 &res_state, sizeof(res_state), NULL, NULL, 0);
}

/**
 * hinic_activate_hwdev_state - Active host nic state and notify mgmt channel
 * that host nic is ready.
 * @hwdev: the hardware interface of a nic device
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_activate_hwdev_state(struct hinic_hwdev *hwdev)
{
	int rc = HINIC_OK;

	if (!hwdev)
		return -EINVAL;

	hinic_set_pf_status(hwdev->hwif, HINIC_PF_STATUS_ACTIVE_FLAG);

	rc = resources_state_set(hwdev, HINIC_RES_ACTIVE);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize resources state failed");
		return rc;
	}

	return 0;
}

/**
 * hinic_deactivate_hwdev_state - Deactivate host nic state and notify mgmt
 * channel that host nic is not ready.
 * @hwdev: the pointer to the private hardware device object
 */
void hinic_deactivate_hwdev_state(struct hinic_hwdev *hwdev)
{
	int rc = HINIC_OK;

	if (!hwdev)
		return;

	rc = resources_state_set(hwdev, HINIC_RES_CLEAN);
	if (rc)
		PMD_DRV_LOG(ERR, "Deinit resources state failed");

	hinic_set_pf_status(hwdev->hwif, HINIC_PF_STATUS_INIT);
}

int hinic_get_board_info(void *hwdev, struct hinic_board_info *info)
{
	struct hinic_comm_board_info board_info;
	u16 out_size = sizeof(board_info);
	int err;

	if (!hwdev || !info)
		return -EINVAL;

	memset(&board_info, 0, sizeof(board_info));
	board_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_GET_BOARD_INFO,
				     &board_info, sizeof(board_info),
				     &board_info, &out_size, 0);
	if (err || board_info.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR, "Failed to get board info, err: %d, status: 0x%x, out size: 0x%x",
			err, board_info.mgmt_msg_head.status, out_size);
		return -EFAULT;
	}

	memcpy(info, &board_info.info, sizeof(*info));
	return 0;
}

/**
 * hinic_l2nic_reset - Restore the initial state of NIC
 * @hwdev: the hardware interface of a nic device
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_l2nic_reset(struct hinic_hwdev *hwdev)
{
	struct hinic_hwif *hwif = hwdev->hwif;
	struct hinic_l2nic_reset l2nic_reset;
	int err = 0;

	err = hinic_set_vport_enable(hwdev, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Set vport disable failed");
		return err;
	}

	rte_delay_ms(100);

	memset(&l2nic_reset, 0, sizeof(l2nic_reset));
	l2nic_reset.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	l2nic_reset.func_id = HINIC_HWIF_GLOBAL_IDX(hwif);
	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_L2NIC_RESET,
				     &l2nic_reset, sizeof(l2nic_reset),
				     NULL, NULL, 0);
	if (err || l2nic_reset.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Reset L2NIC resources failed");
		return -EFAULT;
	}

	return 0;
}

static void
hinic_show_sw_watchdog_timeout_info(void *buf_in, u16 in_size,
				    void *buf_out, u16 *out_size)
{
	struct hinic_mgmt_watchdog_info *watchdog_info;
	u32 *dump_addr, *reg, stack_len, i, j;

	if (in_size != sizeof(*watchdog_info)) {
		PMD_DRV_LOG(ERR, "Invalid mgmt watchdog report, length: %d, should be %zu",
			in_size, sizeof(*watchdog_info));
		return;
	}

	watchdog_info = (struct hinic_mgmt_watchdog_info *)buf_in;

	PMD_DRV_LOG(ERR, "Mgmt deadloop time: 0x%x 0x%x, task id: 0x%x, sp: 0x%x",
		watchdog_info->curr_time_h, watchdog_info->curr_time_l,
		watchdog_info->task_id, watchdog_info->sp);
	PMD_DRV_LOG(ERR, "Stack current used: 0x%x, peak used: 0x%x, overflow flag: 0x%x, top: 0x%x, bottom: 0x%x",
		watchdog_info->curr_used, watchdog_info->peak_used,
		watchdog_info->is_overflow, watchdog_info->stack_top,
		watchdog_info->stack_bottom);

	PMD_DRV_LOG(ERR, "Mgmt pc: 0x%08x, lr: 0x%08x, cpsr:0x%08x",
		watchdog_info->pc, watchdog_info->lr, watchdog_info->cpsr);

	PMD_DRV_LOG(ERR, "Mgmt register info");

	for (i = 0; i < 3; i++) {
		reg = watchdog_info->reg + (u64)(u32)(4 * i);
		PMD_DRV_LOG(ERR, "0x%08x 0x%08x 0x%08x 0x%08x",
			*(reg), *(reg + 1), *(reg + 2), *(reg + 3));
	}

	PMD_DRV_LOG(ERR, "0x%08x", watchdog_info->reg[12]);

	if (watchdog_info->stack_actlen <= 1024) {
		stack_len = watchdog_info->stack_actlen;
	} else {
		PMD_DRV_LOG(ERR, "Oops stack length: 0x%x is wrong",
			watchdog_info->stack_actlen);
		stack_len = 1024;
	}

	PMD_DRV_LOG(ERR, "Mgmt dump stack, 16Bytes per line(start from sp)");
	for (i = 0; i < (stack_len / 16); i++) {
		dump_addr = (u32 *)(watchdog_info->data + ((u64)(u32)(i * 16)));
		PMD_DRV_LOG(ERR, "0x%08x 0x%08x 0x%08x 0x%08x",
			*dump_addr, *(dump_addr + 1), *(dump_addr + 2),
			*(dump_addr + 3));
	}

	for (j = 0; j < ((stack_len % 16) / 4); j++) {
		dump_addr = (u32 *)(watchdog_info->data +
			    ((u64)(u32)(i * 16 + j * 4)));
		PMD_DRV_LOG(ERR, "0x%08x", *dump_addr);
	}

	*out_size = sizeof(*watchdog_info);
	watchdog_info = (struct hinic_mgmt_watchdog_info *)buf_out;
	watchdog_info->mgmt_msg_head.status = 0;
}

static void hinic_show_pcie_dfx_info(struct hinic_hwdev *hwdev,
				     void *buf_in, u16 in_size,
				     void *buf_out, u16 *out_size)
{
	struct hinic_pcie_dfx_ntc *notice_info =
		(struct hinic_pcie_dfx_ntc *)buf_in;
	struct hinic_pcie_dfx_info dfx_info;
	u16 size = 0;
	u16 cnt = 0;
	u32 num = 0;
	u32 i, j;
	int err;
	u32 *reg;

	if (in_size != sizeof(*notice_info)) {
		PMD_DRV_LOG(ERR, "Invalid pcie dfx notice info, length: %d, should be %zu.",
			in_size, sizeof(*notice_info));
		return;
	}

	((struct hinic_pcie_dfx_ntc *)buf_out)->mgmt_msg_head.status = 0;
	*out_size = sizeof(*notice_info);
	memset(&dfx_info, 0, sizeof(dfx_info));
	num = (u32)(notice_info->len / 1024);
	PMD_DRV_LOG(INFO, "INFO LEN: %d", notice_info->len);
	PMD_DRV_LOG(INFO, "PCIE DFX:");
	dfx_info.host_id = 0;
	dfx_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	for (i = 0; i < num; i++) {
		dfx_info.offset = i * MAX_PCIE_DFX_BUF_SIZE;
		if (i == (num - 1))
			dfx_info.last = 1;
		size = sizeof(dfx_info);
		err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
					     HINIC_MGMT_CMD_PCIE_DFX_GET,
					     &dfx_info, sizeof(dfx_info),
					     &dfx_info, &size, 0);
		if (err || dfx_info.mgmt_msg_head.status || !size) {
			PMD_DRV_LOG(ERR, "Failed to get pcie dfx info, err: %d, status: 0x%x, out size: 0x%x",
				err, dfx_info.mgmt_msg_head.status, size);
			return;
		}

		reg = (u32 *)dfx_info.data;
		for (j = 0; j < 256; j = j + 8) {
			PMD_DRV_LOG(ERR, "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
				cnt, reg[j], reg[(u32)(j + 1)],
				reg[(u32)(j + 2)], reg[(u32)(j + 3)],
				reg[(u32)(j + 4)], reg[(u32)(j + 5)],
				reg[(u32)(j + 6)], reg[(u32)(j + 7)]);
			cnt = cnt + 32;
		}
		memset(dfx_info.data, 0, MAX_PCIE_DFX_BUF_SIZE);
	}
}

static void
hinic_show_ffm_info(struct hinic_hwdev *hwdev, void *buf_in, u16 in_size)
{
	struct ffm_intr_info *intr;

	if (in_size != sizeof(struct ffm_intr_info)) {
		PMD_DRV_LOG(ERR, "Invalid input buffer len, length: %d, should be %zu.",
			in_size, sizeof(struct ffm_intr_info));
		return;
	}

	if (hwdev->ffm_num < FFM_RECORD_NUM_MAX) {
		hwdev->ffm_num++;
		intr = (struct ffm_intr_info *)buf_in;
		PMD_DRV_LOG(WARNING, "node_id(%d),err_csr_addr(0x%x),err_csr_val(0x%x),err_level(0x%x),err_type(0x%x)",
			    intr->node_id,
			    intr->err_csr_addr,
			    intr->err_csr_value,
			    intr->err_level,
			    intr->err_type);
	}
}

void hinic_comm_async_event_handle(struct hinic_hwdev *hwdev, u8 cmd,
				   void *buf_in, u16 in_size,
				   void *buf_out, u16 *out_size)
{
	struct hinic_cmd_fault_event *fault_event, *ret_fault_event;

	if (!hwdev)
		return;

	*out_size = 0;

	switch (cmd) {
	case HINIC_MGMT_CMD_FAULT_REPORT:
		if (in_size != sizeof(*fault_event)) {
			PMD_DRV_LOG(ERR, "Invalid fault event report, length: %d, should be %zu",
				in_size, sizeof(*fault_event));
			return;
		}

		fault_event = (struct hinic_cmd_fault_event *)buf_in;
		fault_report_show(hwdev, &fault_event->event);

		if (hinic_func_type(hwdev) != TYPE_VF) {
			ret_fault_event =
				(struct hinic_cmd_fault_event *)buf_out;
			ret_fault_event->mgmt_msg_head.status = 0;
			*out_size = sizeof(*ret_fault_event);
		}
		break;

	case HINIC_MGMT_CMD_WATCHDOG_INFO:
		hinic_show_sw_watchdog_timeout_info(buf_in, in_size,
						    buf_out, out_size);
		break;

	case HINIC_MGMT_CMD_PCIE_DFX_NTC:
		hinic_show_pcie_dfx_info(hwdev, buf_in, in_size,
					 buf_out, out_size);
		break;

	case HINIC_MGMT_CMD_FFM_SET:
		hinic_show_ffm_info(hwdev, buf_in, in_size);
		break;

	default:
		break;
	}
}

static void
hinic_cable_status_event(u8 cmd, void *buf_in, __rte_unused u16 in_size,
			 void *buf_out, u16 *out_size)
{
	struct hinic_cable_plug_event *plug_event;
	struct hinic_link_err_event *link_err;

	if (cmd == HINIC_PORT_CMD_CABLE_PLUG_EVENT) {
		plug_event = (struct hinic_cable_plug_event *)buf_in;
		PMD_DRV_LOG(INFO, "Port module event: Cable %s",
			 plug_event->plugged ? "plugged" : "unplugged");

		*out_size = sizeof(*plug_event);
		plug_event = (struct hinic_cable_plug_event *)buf_out;
		plug_event->mgmt_msg_head.status = 0;
	} else if (cmd == HINIC_PORT_CMD_LINK_ERR_EVENT) {
		link_err = (struct hinic_link_err_event *)buf_in;
		if (link_err->err_type >= LINK_ERR_NUM) {
			PMD_DRV_LOG(ERR, "Link failed, Unknown type: 0x%x",
				link_err->err_type);
		} else {
			PMD_DRV_LOG(INFO, "Link failed, type: 0x%x: %s",
				 link_err->err_type,
				 hinic_module_link_err[link_err->err_type]);
		}

		*out_size = sizeof(*link_err);
		link_err = (struct hinic_link_err_event *)buf_out;
		link_err->mgmt_msg_head.status = 0;
	}
}

static int hinic_link_event_process(struct hinic_hwdev *hwdev,
				    struct rte_eth_dev *eth_dev, u8 status)
{
	uint32_t port_speed[LINK_SPEED_MAX] = {ETH_SPEED_NUM_10M,
					ETH_SPEED_NUM_100M, ETH_SPEED_NUM_1G,
					ETH_SPEED_NUM_10G, ETH_SPEED_NUM_25G,
					ETH_SPEED_NUM_40G, ETH_SPEED_NUM_100G};
	struct nic_port_info port_info;
	struct rte_eth_link link;
	int rc = HINIC_OK;

	if (!status) {
		link.link_status = ETH_LINK_DOWN;
		link.link_speed = 0;
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
		link.link_autoneg = ETH_LINK_FIXED;
	} else {
		link.link_status = ETH_LINK_UP;

		memset(&port_info, 0, sizeof(port_info));
		rc = hinic_get_port_info(hwdev, &port_info);
		if (rc) {
			link.link_speed = ETH_SPEED_NUM_NONE;
			link.link_duplex = ETH_LINK_FULL_DUPLEX;
			link.link_autoneg = ETH_LINK_FIXED;
		} else {
			link.link_speed = port_speed[port_info.speed %
						LINK_SPEED_MAX];
			link.link_duplex = port_info.duplex;
			link.link_autoneg = port_info.autoneg_state;
		}
	}
	(void)rte_eth_linkstatus_set(eth_dev, &link);

	return rc;
}

static void hinic_lsc_process(struct hinic_hwdev *hwdev,
			      struct rte_eth_dev *rte_dev, u8 status)
{
	int ret;

	ret = hinic_link_event_process(hwdev, rte_dev, status);
	/* check if link has changed, notify callback */
	if (ret == 0)
		_rte_eth_dev_callback_process(rte_dev,
					      RTE_ETH_EVENT_INTR_LSC,
					      NULL);
}

void hinic_l2nic_async_event_handle(struct hinic_hwdev *hwdev,
				    void *param, u8 cmd,
				    void *buf_in, u16 in_size,
				    void *buf_out, u16 *out_size)
{
	struct hinic_port_link_status *in_link;
	struct rte_eth_dev *eth_dev;

	if (!hwdev)
		return;

	*out_size = 0;

	switch (cmd) {
	case HINIC_PORT_CMD_LINK_STATUS_REPORT:
		eth_dev = param;
		in_link = (struct hinic_port_link_status *)buf_in;
		PMD_DRV_LOG(INFO, "Link status event report, dev_name: %s, port_id: %d, link_status: %s",
			 eth_dev->data->name, eth_dev->data->port_id,
			 in_link->link ? "UP" : "DOWN");

		hinic_lsc_process(hwdev, eth_dev, in_link->link);
		break;

	case HINIC_PORT_CMD_CABLE_PLUG_EVENT:
	case HINIC_PORT_CMD_LINK_ERR_EVENT:
		hinic_cable_status_event(cmd, buf_in, in_size,
					 buf_out, out_size);
		break;

	case HINIC_PORT_CMD_MGMT_RESET:
		PMD_DRV_LOG(WARNING, "Mgmt is reset");
		break;

	default:
		PMD_DRV_LOG(ERR, "Unsupported event %d to process",
			cmd);
		break;
	}
}

static void print_cable_info(struct hinic_link_info *info)
{
	char tmp_str[512] = {0};
	char tmp_vendor[17] = {0};
	const char *port_type = "Unknown port type";
	int i;

	if (info->cable_absent) {
		PMD_DRV_LOG(INFO, "Cable unpresent");
		return;
	}

	if (info->port_type < LINK_PORT_MAX_TYPE)
		port_type = __hw_to_char_port_type[info->port_type];
	else
		PMD_DRV_LOG(INFO, "Unknown port type: %u",
			 info->port_type);
	if (info->port_type == LINK_PORT_FIBRE) {
		if (info->port_sub_type == FIBRE_SUBTYPE_SR)
			port_type = "Fibre-SR";
		else if (info->port_sub_type == FIBRE_SUBTYPE_LR)
			port_type = "Fibre-LR";
	}

	for (i = sizeof(info->vendor_name) - 1; i >= 0; i--) {
		if (info->vendor_name[i] == ' ')
			info->vendor_name[i] = '\0';
		else
			break;
	}

	memcpy(tmp_vendor, info->vendor_name, sizeof(info->vendor_name));
	snprintf(tmp_str, sizeof(tmp_str),
		 "Vendor: %s, %s, %s, length: %um, max_speed: %uGbps",
		 tmp_vendor, info->sfp_type ? "SFP" : "QSFP", port_type,
		 info->cable_length, info->cable_max_speed);
	if (info->port_type != LINK_PORT_COPPER)
		snprintf(tmp_str + strlen(tmp_str),
			 sizeof(tmp_str) - strlen(tmp_str),
			 ", Temperature: %u", info->cable_temp);

	PMD_DRV_LOG(INFO, "Cable information: %s", tmp_str);
}

static void print_hi30_status(struct hinic_link_info *info)
{
	struct hi30_ffe_data *ffe_data;
	struct hi30_ctle_data *ctle_data;

	ffe_data = (struct hi30_ffe_data *)info->hi30_ffe;
	ctle_data = (struct hi30_ctle_data *)info->hi30_ctle;

	PMD_DRV_LOG(INFO, "TX_FFE: PRE2=%s%d; PRE1=%s%d; MAIN=%d; POST1=%s%d; POST1X=%s%d",
		 (ffe_data->PRE1 & 0x10) ? "-" : "",
		 (int)(ffe_data->PRE1 & 0xf),
		 (ffe_data->PRE2 & 0x10) ? "-" : "",
		 (int)(ffe_data->PRE2 & 0xf),
		 (int)ffe_data->MAIN,
		 (ffe_data->POST1 & 0x10) ? "-" : "",
		 (int)(ffe_data->POST1 & 0xf),
		 (ffe_data->POST2 & 0x10) ? "-" : "",
		 (int)(ffe_data->POST2 & 0xf));
	PMD_DRV_LOG(INFO, "RX_CTLE: Gain1~3=%u %u %u; Boost1~3=%u %u %u; Zero1~3=%u %u %u; Squelch1~3=%u %u %u",
		 ctle_data->ctlebst[0], ctle_data->ctlebst[1],
		 ctle_data->ctlebst[2], ctle_data->ctlecmband[0],
		 ctle_data->ctlecmband[1], ctle_data->ctlecmband[2],
		 ctle_data->ctlermband[0], ctle_data->ctlermband[1],
		 ctle_data->ctlermband[2], ctle_data->ctleza[0],
		 ctle_data->ctleza[1], ctle_data->ctleza[2]);
}

static void print_link_info(struct hinic_link_info *info,
			    enum hilink_info_print_event type)
{
	const char *fec = "None";

	if (info->fec < HILINK_FEC_MAX_TYPE)
		fec = __hw_to_char_fec[info->fec];
	else
		PMD_DRV_LOG(INFO, "Unknown fec type: %u",
			 info->fec);

	if (type == HILINK_EVENT_LINK_UP || !info->an_state) {
		PMD_DRV_LOG(INFO, "Link information: speed %dGbps, %s, autoneg %s",
			 info->speed, fec, info->an_state ? "on" : "off");
	} else {
		PMD_DRV_LOG(INFO, "Link information: antoneg: %s",
			 info->an_state ? "on" : "off");
	}
}

static const char *hilink_info_report_type[HILINK_EVENT_MAX_TYPE] = {
	"", "link up", "link down", "cable plugged"
};

static void hinic_print_hilink_info(void *buf_in, u16 in_size,
				    void *buf_out, u16 *out_size)
{
	struct hinic_hilink_link_info *hilink_info =
		(struct hinic_hilink_link_info *)buf_in;
	struct hinic_link_info *info;
	enum hilink_info_print_event type;

	if (in_size != sizeof(*hilink_info)) {
		PMD_DRV_LOG(ERR, "Invalid hilink info message size %d, should be %zu",
			in_size, sizeof(*hilink_info));
		return;
	}

	((struct hinic_hilink_link_info *)buf_out)->mgmt_msg_head.status = 0;
	*out_size = sizeof(*hilink_info);

	info = &hilink_info->info;
	type = hilink_info->info_type;

	if (type < HILINK_EVENT_LINK_UP || type >= HILINK_EVENT_MAX_TYPE) {
		PMD_DRV_LOG(INFO, "Invalid hilink info report, type: %d",
			 type);
		return;
	}

	PMD_DRV_LOG(INFO, "Hilink info report after %s",
		 hilink_info_report_type[type]);

	print_cable_info(info);

	print_link_info(info, type);

	print_hi30_status(info);

	if (type == HILINK_EVENT_LINK_UP)
		return;

	if (type == HILINK_EVENT_CABLE_PLUGGED) {
		PMD_DRV_LOG(INFO, "alos: %u, rx_los: %u",
			 info->alos, info->rx_los);
		return;
	}

	PMD_DRV_LOG(INFO, "PMA ctrl: %s, MAC tx %s, MAC rx %s, PMA debug inforeg: 0x%x, PMA signal ok reg: 0x%x, RF/LF status reg: 0x%x",
		 info->pma_status ? "on" : "off",
		 info->mac_tx_en ? "enable" : "disable",
		 info->mac_rx_en ? "enable" : "disable", info->pma_dbg_info_reg,
		 info->pma_signal_ok_reg, info->rf_lf_status_reg);
	PMD_DRV_LOG(INFO, "alos: %u, rx_los: %u, PCS block counter reg: 0x%x,PCS link: 0x%x, MAC link: 0x%x PCS_err_cnt: 0x%x",
		 info->alos, info->rx_los, info->pcs_err_blk_cnt_reg,
		 info->pcs_link_reg, info->mac_link_reg, info->pcs_err_cnt);
}

void hinic_hilink_async_event_handle(struct hinic_hwdev *hwdev, u8 cmd,
				     void *buf_in, u16 in_size,
				     void *buf_out, u16 *out_size)
{
	if (!hwdev)
		return;

	*out_size = 0;

	switch (cmd) {
	case HINIC_HILINK_CMD_GET_LINK_INFO:
		hinic_print_hilink_info(buf_in, in_size, buf_out,
					out_size);
		break;

	default:
		PMD_DRV_LOG(ERR, "Unsupported event %d to process",
			cmd);
		break;
	}
}
