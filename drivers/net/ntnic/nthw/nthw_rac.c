/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "rte_spinlock.h"
#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_rac.h"

#define RAB_DMA_WAIT (1000000)

#define RAB_READ (0x01)
#define RAB_WRITE (0x02)
#define RAB_ECHO (0x08)
#define RAB_COMPLETION (0x0F)

#define RAB_OPR_LO (28)

#define RAB_CNT_LO (20)
#define RAB_CNT_BW (8)

#define RAB_BUSID_LO (16)
#define RAB_BUSID_BW (4)

#define RAB_ADDR_BW (16)

nthw_rac_t *nthw_rac_new(void)
{
	nthw_rac_t *p = malloc(sizeof(nthw_rac_t));

	if (p)
		memset(p, 0, sizeof(nthw_rac_t));

	return p;
}

int nthw_rac_init(nthw_rac_t *p, nthw_fpga_t *p_fpga, struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_RAC, 0);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: RAC %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mp_mod_rac = p_mod;

	p->mn_param_rac_rab_interfaces =
		nthw_fpga_get_product_param(p->mp_fpga, NT_RAC_RAB_INTERFACES, 3);
	NT_LOG(DBG, NTHW, "%s: NT_RAC_RAB_INTERFACES=%d", p_adapter_id_str,
		p->mn_param_rac_rab_interfaces);

	p->mn_param_rac_rab_ob_update =
		nthw_fpga_get_product_param(p->mp_fpga, NT_RAC_RAB_OB_UPDATE, 0);
	NT_LOG(DBG, NTHW, "%s: NT_RAC_RAB_OB_UPDATE=%d", p_adapter_id_str,
		p->mn_param_rac_rab_ob_update);

	/* Optional dummy test registers */
	p->mp_reg_dummy0 = nthw_module_query_register(p->mp_mod_rac, RAC_DUMMY0);
	p->mp_reg_dummy1 = nthw_module_query_register(p->mp_mod_rac, RAC_DUMMY1);
	p->mp_reg_dummy2 = nthw_module_query_register(p->mp_mod_rac, RAC_DUMMY2);

	p->mp_reg_rab_init = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_INIT);
	p->mp_fld_rab_init = nthw_register_get_field(p->mp_reg_rab_init, RAC_RAB_INIT_RAB);
	p->mn_fld_rab_init_bw = nthw_field_get_bit_width(p->mp_fld_rab_init);
	p->mn_fld_rab_init_mask = nthw_field_get_mask(p->mp_fld_rab_init);

	/* RAC_RAB_INIT_RAB reg/field sanity checks: */
	RTE_ASSERT(p->mn_fld_rab_init_mask == ((1UL << p->mn_fld_rab_init_bw) - 1));
	RTE_ASSERT(p->mn_fld_rab_init_bw == p->mn_param_rac_rab_interfaces);

	p->mp_reg_dbg_ctrl = nthw_module_query_register(p->mp_mod_rac, RAC_DBG_CTRL);

	if (p->mp_reg_dbg_ctrl)
		p->mp_fld_dbg_ctrl = nthw_register_query_field(p->mp_reg_dbg_ctrl, RAC_DBG_CTRL_C);

	else
		p->mp_fld_dbg_ctrl = NULL;

	p->mp_reg_dbg_data = nthw_module_query_register(p->mp_mod_rac, RAC_DBG_DATA);

	if (p->mp_reg_dbg_data)
		p->mp_fld_dbg_data = nthw_register_query_field(p->mp_reg_dbg_data, RAC_DBG_DATA_D);

	p->mp_reg_rab_ib_data = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_IB_DATA);
	p->mp_fld_rab_ib_data = nthw_register_get_field(p->mp_reg_rab_ib_data, RAC_RAB_IB_DATA_D);

	p->mp_reg_rab_ob_data = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_OB_DATA);
	p->mp_fld_rab_ob_data = nthw_register_get_field(p->mp_reg_rab_ob_data, RAC_RAB_OB_DATA_D);

	p->mp_reg_rab_buf_free = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_BUF_FREE);
	p->mp_fld_rab_buf_free_ib_free =
		nthw_register_get_field(p->mp_reg_rab_buf_free, RAC_RAB_BUF_FREE_IB_FREE);
	p->mp_fld_rab_buf_free_ib_ovf =
		nthw_register_get_field(p->mp_reg_rab_buf_free, RAC_RAB_BUF_FREE_IB_OVF);
	p->mp_fld_rab_buf_free_ob_free =
		nthw_register_get_field(p->mp_reg_rab_buf_free, RAC_RAB_BUF_FREE_OB_FREE);
	p->mp_fld_rab_buf_free_ob_ovf =
		nthw_register_get_field(p->mp_reg_rab_buf_free, RAC_RAB_BUF_FREE_OB_OVF);
	p->mp_fld_rab_buf_free_timeout =
		nthw_register_get_field(p->mp_reg_rab_buf_free, RAC_RAB_BUF_FREE_TIMEOUT);

	p->mp_reg_rab_buf_used = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_BUF_USED);
	p->mp_fld_rab_buf_used_ib_used =
		nthw_register_get_field(p->mp_reg_rab_buf_used, RAC_RAB_BUF_USED_IB_USED);
	p->mp_fld_rab_buf_used_ob_used =
		nthw_register_get_field(p->mp_reg_rab_buf_used, RAC_RAB_BUF_USED_OB_USED);
	p->mp_fld_rab_buf_used_flush =
		nthw_register_get_field(p->mp_reg_rab_buf_used, RAC_RAB_BUF_USED_FLUSH);

	/*
	 * RAC_RAB_DMA regs are optional - only found in real
	 * NT4GA - not found in 9231/9232 and earlier
	 */
	p->mp_reg_rab_dma_ib_lo = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_IB_LO);
	p->mp_fld_rab_dma_ib_lo_phy_addr =
		nthw_register_get_field(p->mp_reg_rab_dma_ib_lo, RAC_RAB_DMA_IB_LO_PHYADDR);

	p->mp_reg_rab_dma_ib_hi = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_IB_HI);
	p->mp_fld_rab_dma_ib_hi_phy_addr =
		nthw_register_get_field(p->mp_reg_rab_dma_ib_hi, RAC_RAB_DMA_IB_HI_PHYADDR);

	p->mp_reg_rab_dma_ob_lo = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_OB_LO);
	p->mp_fld_rab_dma_ob_lo_phy_addr =
		nthw_register_get_field(p->mp_reg_rab_dma_ob_lo, RAC_RAB_DMA_OB_LO_PHYADDR);

	p->mp_reg_rab_dma_ob_hi = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_OB_HI);
	p->mp_fld_rab_dma_ob_hi_phy_addr =
		nthw_register_get_field(p->mp_reg_rab_dma_ob_hi, RAC_RAB_DMA_OB_HI_PHYADDR);

	p->mp_reg_rab_dma_ib_wr = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_IB_WR);
	p->mp_fld_rab_dma_ib_wr_ptr =
		nthw_register_get_field(p->mp_reg_rab_dma_ib_wr, RAC_RAB_DMA_IB_WR_PTR);

	p->mp_reg_rab_dma_ib_rd = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_IB_RD);
	p->mp_fld_rab_dma_ib_rd_ptr =
		nthw_register_get_field(p->mp_reg_rab_dma_ib_rd, RAC_RAB_DMA_IB_RD_PTR);

	p->mp_reg_rab_dma_ob_wr = nthw_module_get_register(p->mp_mod_rac, RAC_RAB_DMA_OB_WR);
	p->mp_fld_rab_dma_ob_wr_ptr =
		nthw_register_get_field(p->mp_reg_rab_dma_ob_wr, RAC_RAB_DMA_OB_WR_PTR);

	p->RAC_RAB_INIT_ADDR = nthw_register_get_address(p->mp_reg_rab_init);
	p->RAC_RAB_IB_DATA_ADDR = nthw_register_get_address(p->mp_reg_rab_ib_data);
	p->RAC_RAB_OB_DATA_ADDR = nthw_register_get_address(p->mp_reg_rab_ob_data);
	p->RAC_RAB_BUF_FREE_ADDR = nthw_register_get_address(p->mp_reg_rab_buf_free);
	p->RAC_RAB_BUF_USED_ADDR = nthw_register_get_address(p->mp_reg_rab_buf_used);

	/*
	 * RAC_RAB_DMA regs are optional - only found in real NT4GA - not found in 9231/9232 and
	 * earlier
	 */

	p->RAC_RAB_DMA_IB_LO_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ib_lo);
	p->RAC_RAB_DMA_IB_HI_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ib_hi);
	p->RAC_RAB_DMA_OB_LO_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ob_lo);
	p->RAC_RAB_DMA_OB_HI_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ob_hi);
	p->RAC_RAB_DMA_IB_RD_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ib_rd);
	p->RAC_RAB_DMA_OB_WR_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ob_wr);
	p->RAC_RAB_DMA_IB_WR_ADDR = nthw_register_get_address(p->mp_reg_rab_dma_ib_wr);

	p->RAC_RAB_BUF_FREE_IB_FREE_MASK = nthw_field_get_mask(p->mp_fld_rab_buf_free_ib_free);
	p->RAC_RAB_BUF_FREE_OB_FREE_MASK = nthw_field_get_mask(p->mp_fld_rab_buf_free_ob_free);
	p->RAC_RAB_BUF_USED_IB_USED_MASK = nthw_field_get_mask(p->mp_fld_rab_buf_used_ib_used);
	p->RAC_RAB_BUF_USED_OB_USED_MASK = nthw_field_get_mask(p->mp_fld_rab_buf_used_ob_used);

	p->RAC_RAB_BUF_USED_FLUSH_MASK = nthw_field_get_mask(p->mp_fld_rab_buf_used_flush);

	p->RAC_RAB_BUF_USED_OB_USED_LOW =
		nthw_field_get_bit_pos_low(p->mp_fld_rab_buf_used_ob_used);

	p->mp_reg_rab_nmb_rd = nthw_module_query_register(p->mp_mod_rac, RAC_NMB_RD_ADR);

	if (p->mp_reg_rab_nmb_rd)
		p->RAC_NMB_RD_ADR_ADDR = nthw_register_get_address(p->mp_reg_rab_nmb_rd);

	p->mp_reg_rab_nmb_data = nthw_module_query_register(p->mp_mod_rac, RAC_NMB_DATA);

	if (p->mp_reg_rab_nmb_data)
		p->RAC_NMB_DATA_ADDR = nthw_register_get_address(p->mp_reg_rab_nmb_data);

	p->mp_reg_rab_nmb_wr = nthw_module_query_register(p->mp_mod_rac, RAC_NMB_WR_ADR);

	if (p->mp_reg_rab_nmb_wr)
		p->RAC_NMB_WR_ADR_ADDR = nthw_register_get_address(p->mp_reg_rab_nmb_wr);

	p->mp_reg_rab_nmb_status = nthw_module_query_register(p->mp_mod_rac, RAC_NMB_STATUS);

	if (p->mp_reg_rab_nmb_status)
		p->RAC_NMB_STATUS_ADDR = nthw_register_get_address(p->mp_reg_rab_nmb_status);

	p->m_dma = NULL;

	{
		/*
		 * RAC is a primary communication channel - debug will be messy
		 * turn off debug by default - except for rac_rab_init
		 * NOTE: currently debug will not work - due to optimizations
		 */
		const int n_debug_mode = nthw_module_get_debug_mode(p->mp_mod_rac);

		if (n_debug_mode && n_debug_mode <= 0xff) {
			nthw_module_set_debug_mode(p->mp_mod_rac, 0);
			nthw_register_set_debug_mode(p->mp_reg_rab_init, n_debug_mode);
		}
	}

	rte_spinlock_init(&p->m_mutex);

	return 0;
}

static int nthw_rac_get_rab_interface_count(const nthw_rac_t *p)
{
	return p->mn_param_rac_rab_interfaces;
}

/* private function for internal RAC operations -
 * improves log flexibility and prevents log flooding
 */
static void nthw_rac_reg_read32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t *p_data)
{
	*p_data = *(volatile uint32_t *)((uint8_t *)p_fpga_info->bar0_addr + reg_addr);
}

/* private function for internal RAC operations -
 * improves log flexibility and prevents log flooding
 */
static void nthw_rac_reg_write32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t n_data)
{
	*(volatile uint32_t *)((uint8_t *)p_fpga_info->bar0_addr + reg_addr) = n_data;
}

static inline int _nthw_rac_wait_for_rab_done(const nthw_rac_t *p, uint32_t address,
	uint32_t word_cnt)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t used = 0;
	uint32_t retry;

	for (retry = 0; retry < 100000; retry++) {
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR, &used);
		used = (used & p->RAC_RAB_BUF_USED_OB_USED_MASK) >>
			p->RAC_RAB_BUF_USED_OB_USED_LOW;

		if (used >= word_cnt)
			break;
	}

	if (used < word_cnt) {
		NT_LOG(ERR, NTHW, "%s: Fail rab bus r/w addr=0x%08X used=%x wordcount=%" PRIu32 "",
			p_adapter_id_str, address, used, word_cnt);
		return -1;
	}

	return 0;
}

/*
 * NT_PCI_REG_P9xyz_RAC_RAB_INIT
 *
 * Initializes (resets) the programmable registers on the Register Access Buses (RAB).
 * This initialization must be performed by software as part of the driver load procedure.
 *
 * Bit n of this field initializes the programmable registers on RAB interface n.
 * Software must write one to the bit and then clear the bit again.
 *
 * All RAB module registers will be reset to their defaults.
 * This includes the product specific RESET module (eg RST9xyz)
 * As a consequence of this behavior the official reset sequence
 * must be excersised - as all RAB modules will be held in reset.
 */
int nthw_rac_rab_init(nthw_rac_t *p, uint32_t n_rab_intf_mask)
{
	/*
	 * Write rac_rab_init
	 * Perform operation twice - first to get trace of operation -
	 * second to get things done...
	 */
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	nthw_field_set_val_flush32(p->mp_fld_rab_init, n_rab_intf_mask);
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_INIT_ADDR, n_rab_intf_mask);
	return 0;
}

int nthw_rac_rab_reset(nthw_rac_t *p)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	(void)p_adapter_id_str;

	/* RAC RAB bus "flip/flip" reset */
	const int n_rac_rab_bus_count = nthw_rac_get_rab_interface_count(p);
	const int n_rac_rab_bus_mask = (1 << n_rac_rab_bus_count) - 1;

	NT_LOG(DBG, NTHW, "%s: NT_RAC_RAB_INTERFACES=%d (0x%02X)", p_adapter_id_str,
		n_rac_rab_bus_count, n_rac_rab_bus_mask);
	RTE_ASSERT(n_rac_rab_bus_count);
	RTE_ASSERT(n_rac_rab_bus_mask);

	/* RAC RAB bus "flip/flip" reset first stage - new impl (ref RMT#37020) */
	nthw_rac_rab_init(p, 0);
	nthw_rac_rab_init(p, n_rac_rab_bus_mask);
	nthw_rac_rab_init(p, n_rac_rab_bus_mask & ~0x01);

	return 0;
}

int nthw_rac_rab_setup(nthw_rac_t *p)
{
	int rc = 0;

	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	uint32_t n_dma_buf_size = 2L * RAB_DMA_BUF_CNT * sizeof(uint32_t);
	const size_t align_size = nthw_util_align_size(n_dma_buf_size);
	int numa_node = p_fpga_info->numa_node;
	uint64_t dma_addr;
	uint32_t buf;

	if (!p->m_dma) {
		struct nt_dma_s *vfio_dma;
		/* FPGA needs Page alignment (4K) */
		vfio_dma = nthw_dma_alloc(align_size, 0x1000, numa_node);

		if (vfio_dma == NULL) {
			NT_LOG(ERR, NTNIC, "nthw_dma_alloc failed");
			return -1;
		}

		p->m_dma_in_buf = (uint32_t *)vfio_dma->addr;
		p->m_dma_out_buf = p->m_dma_in_buf + RAB_DMA_BUF_CNT;
		p->m_dma = vfio_dma;
	}

	/* Setup DMA on the adapter */
	dma_addr = p->m_dma->iova;
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_DMA_IB_LO_ADDR, dma_addr & 0xffffffff);
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_DMA_IB_HI_ADDR,
		(uint32_t)(dma_addr >> 32) & 0xffffffff);
	dma_addr += RAB_DMA_BUF_CNT * sizeof(uint32_t);
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_DMA_OB_LO_ADDR, dma_addr & 0xffffffff);
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_DMA_OB_HI_ADDR,
		(uint32_t)(dma_addr >> 32) & 0xffffffff);

	/* Set initial value of internal pointers */
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_DMA_IB_RD_ADDR, &buf);
	p->m_dma_in_ptr_wr = (uint16_t)(buf / sizeof(uint32_t));
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_DMA_OB_WR_ADDR, &buf);
	p->m_dma_out_ptr_rd = (uint16_t)(buf / sizeof(uint32_t));
	p->m_in_free = RAB_DMA_BUF_CNT;

	return rc;
}

void nthw_rac_bar0_read32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t word_cnt, uint32_t *p_data)
{
	volatile const uint32_t *const src_addr =
		(uint32_t *)((uint8_t *)p_fpga_info->bar0_addr + reg_addr);

	for (uint32_t i = 0; i < word_cnt; i++)
		p_data[i] = src_addr[i];
}

void nthw_rac_bar0_write32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t word_cnt, const uint32_t *p_data)
{
	volatile uint32_t *const dst_addr =
		(uint32_t *)((uint8_t *)p_fpga_info->bar0_addr + reg_addr);

	for (uint32_t i = 0; i < word_cnt; i++)
		dst_addr[i] = p_data[i];
}

int nthw_rac_rab_dma_begin(nthw_rac_t *p)
{
	p->m_dma_active = true;

	return 0;
}

static void nthw_rac_rab_dma_activate(nthw_rac_t *p)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const uint32_t completion = RAB_COMPLETION << RAB_OPR_LO;

	/* Write completion word */
	p->m_dma_in_buf[p->m_dma_in_ptr_wr] = completion;
	p->m_dma_in_ptr_wr = (uint16_t)((p->m_dma_in_ptr_wr + 1) & (RAB_DMA_BUF_CNT - 1));

	/* Clear output completion word */
	p->m_dma_out_buf[p->m_dma_out_ptr_rd] = 0;

	/* Update DMA pointer and start transfer */
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_DMA_IB_WR_ADDR,
		(uint32_t)(p->m_dma_in_ptr_wr * sizeof(uint32_t)));
}

static int nthw_rac_rab_dma_wait(nthw_rac_t *p)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	const uint32_t completion = RAB_COMPLETION << RAB_OPR_LO;
	uint32_t i;

	for (i = 0; i < RAB_DMA_WAIT; i++) {
		nthw_os_wait_usec_poll(1);

		if ((p->m_dma_out_buf[p->m_dma_out_ptr_rd] & completion) == completion)
			break;
	}

	if (i == RAB_DMA_WAIT) {
		NT_LOG(ERR, NTHW, "%s: RAB: Unexpected value of completion (0x%08X)",
			p_adapter_id_str, p->m_dma_out_buf[p->m_dma_out_ptr_rd]);
		return -1;
	}

	p->m_dma_out_ptr_rd = (uint16_t)((p->m_dma_out_ptr_rd + 1) & (RAB_DMA_BUF_CNT - 1));
	p->m_in_free = RAB_DMA_BUF_CNT;

	return 0;
}

int nthw_rac_rab_dma_commit(nthw_rac_t *p)
{
	int ret;

	nthw_rac_rab_dma_activate(p);
	ret = nthw_rac_rab_dma_wait(p);

	p->m_dma_active = false;

	return ret;
}

uint32_t nthw_rac_rab_get_free(nthw_rac_t *p)
{
	if (!p->m_dma_active) {
		/* Expecting mutex not to be locked! */
		RTE_ASSERT(0);      /* alert developer that something is wrong */
		return -1;
	}

	return p->m_in_free;
}

int nthw_rac_rab_write32_dma(nthw_rac_t *p, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, const uint32_t *p_data)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;

	if (word_cnt == 0 || word_cnt > 256) {
		NT_LOG(ERR, NTHW,
			"%s: Failed rab dma write length check - bus: %d addr: 0x%08X wordcount: %"
			PRIu32 " - inBufFree: 0x%08X",
			p_adapter_id_str, bus_id, address, word_cnt, p->m_in_free);
		RTE_ASSERT(0);      /* alert developer that something is wrong */
		return -1;
	}

	if (p->m_in_free < (word_cnt + 3)) {
		/*
		 * No more memory available.
		 * nthw_rac_rab_dma_commit() needs to be called to start and finish pending
		 * transfers.
		 */
		return -1;
	}

	p->m_in_free -= (word_cnt + 1);

	/* Write the command word */
	p->m_dma_in_buf[p->m_dma_in_ptr_wr] = (RAB_WRITE << RAB_OPR_LO) |
		((word_cnt & ((1 << RAB_CNT_BW) - 1)) << RAB_CNT_LO) | (bus_id << RAB_BUSID_LO) |
		address;
	p->m_dma_in_ptr_wr = (uint16_t)((p->m_dma_in_ptr_wr + 1) & (RAB_DMA_BUF_CNT - 1));

	for (uint32_t i = 0; i < word_cnt; i++) {
		p->m_dma_in_buf[p->m_dma_in_ptr_wr] = p_data[i];
		p->m_dma_in_ptr_wr = (uint16_t)((p->m_dma_in_ptr_wr + 1) & (RAB_DMA_BUF_CNT - 1));
	}

	return 0;
}

int nthw_rac_rab_read32_dma(nthw_rac_t *p, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, struct dma_buf_ptr *buf_ptr)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;

	if (word_cnt == 0 || word_cnt > 256) {
		NT_LOG(ERR, NTHW,
			"%s: Failed rab dma read length check - bus: %d addr: 0x%08X wordcount: %"
			PRIu32 " - inBufFree: 0x%08X",
			p_adapter_id_str, bus_id, address, word_cnt, p->m_in_free);
		RTE_ASSERT(0);      /* alert developer that something is wrong */
		return -1;
	}

	if (p->m_in_free < 3) {
		/*
		 * No more memory available.
		 * nthw_rac_rab_dma_commit() needs to be called to start and finish pending
		 * transfers.
		 */
		return -1;
	}

	p->m_in_free -= 1;

	/* Write the command word */
	p->m_dma_in_buf[p->m_dma_in_ptr_wr] = (RAB_READ << RAB_OPR_LO) |
		((word_cnt & ((1 << RAB_CNT_BW) - 1)) << RAB_CNT_LO) | (bus_id << RAB_BUSID_LO) |
		address;
	p->m_dma_in_ptr_wr = (uint16_t)((p->m_dma_in_ptr_wr + 1) & (RAB_DMA_BUF_CNT - 1));

	buf_ptr->index = p->m_dma_out_ptr_rd;
	buf_ptr->size = RAB_DMA_BUF_CNT;
	buf_ptr->base = p->m_dma_out_buf;
	p->m_dma_out_ptr_rd =
		(uint16_t)((p->m_dma_out_ptr_rd + word_cnt) & (RAB_DMA_BUF_CNT - 1U));

	return 0;
}

int nthw_rac_rab_write32(nthw_rac_t *p, bool trc, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, const uint32_t *p_data)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t buf_used;
	uint32_t buf_free;
	uint32_t in_buf_free;
	uint32_t out_buf_free;
	int res = 0;

	if (address > (1 << RAB_ADDR_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal address: value too large %" PRIu32 " - max %d",
			p_adapter_id_str, address, (1 << RAB_ADDR_BW));
		return -1;
	}

	if (bus_id > (1 << RAB_BUSID_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal bus id: value too large %" PRIu32 " - max %d",
			p_adapter_id_str, bus_id, (1 << RAB_BUSID_BW));
		return -1;
	}

	if (word_cnt == 0) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal word count: value is zero (%" PRIu32 ")",
			p_adapter_id_str, word_cnt);
		return -1;
	}

	if (word_cnt > (1 << RAB_CNT_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal word count: value too large %d - max %d",
			p_adapter_id_str, word_cnt, (1 << RAB_CNT_BW));
		return -1;
	}

	rte_spinlock_lock(&p->m_mutex);

	if (p->m_dma_active) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal operation: DMA enabled", p_adapter_id_str);
		res = -1;
		goto exit_unlock_res;
	}

	/* Read buffer free register */
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, &buf_free);

	in_buf_free = buf_free & p->RAC_RAB_BUF_FREE_IB_FREE_MASK;
	out_buf_free = (buf_free & p->RAC_RAB_BUF_FREE_OB_FREE_MASK) >> 16;

	/* Read buffer used register */
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR, &buf_used);

	buf_used =
		buf_used & (p->RAC_RAB_BUF_USED_IB_USED_MASK | p->RAC_RAB_BUF_USED_OB_USED_MASK);

	/*
	 * Verify that output buffer can hold one completion word,
	 * input buffer can hold the number of words to be written +
	 * one write and one completion command
	 * and that the input and output "used" buffer is 0
	 */
	if (out_buf_free >= 1 && in_buf_free >= word_cnt + 2 && buf_used == 0) {
		const uint32_t rab_oper_cmpl = (RAB_COMPLETION << RAB_OPR_LO);
		uint32_t rab_echo_oper_cmpl;
		uint32_t word_cnt_expected = 1;
		uint32_t rab_oper_wr;
		uint32_t i;

		rab_oper_wr = (RAB_WRITE << RAB_OPR_LO) |
			((word_cnt & ((1 << RAB_CNT_BW) - 1)) << RAB_CNT_LO) |
			(bus_id << RAB_BUSID_LO) | address;

		if (trc) {
			rab_oper_wr |= (RAB_ECHO << RAB_OPR_LO);
			word_cnt_expected += word_cnt + 1;
		}

		/* Write command */
		nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_IB_DATA_ADDR, rab_oper_wr);

		/* Write data to input buffer */
		for (i = 0; i < word_cnt; i++)
			nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_IB_DATA_ADDR, p_data[i]);

		/* Write completion command */
		nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_IB_DATA_ADDR, rab_oper_cmpl);

		/* Wait until done */
		if (_nthw_rac_wait_for_rab_done(p, address, word_cnt_expected)) {
			res = -1;
			goto exit_unlock_res;
		}

		if (trc) {
			uint32_t rab_echo_oper_wr;
			nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR,
				&rab_echo_oper_wr);

			if (p->mn_param_rac_rab_ob_update)
				nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, 0);

			if (rab_oper_wr != rab_echo_oper_wr) {
				NT_LOG(ERR, NTHW,
					"%s: expected rab read echo oper (0x%08X) - read (0x%08X)",
					p_adapter_id_str, rab_oper_wr, rab_echo_oper_wr);
			}
		}

		{
			/* Read data from output buffer */
			uint32_t data;
			char *tmp_string;

			if (trc) {
				tmp_string = nthw_log_helper_str_alloc("Register::write");
				nthw_log_helper_str_add(tmp_string,
					"(Dev: NA, Bus: RAB%u, Addr: 0x%08X, Cnt: %d, Data:",
					bus_id, address, word_cnt);
			}

			for (i = 0; i < word_cnt; i++) {
				nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, &data);

				if (p->mn_param_rac_rab_ob_update) {
					nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR,
						0);
				}

				if (trc)
					nthw_log_helper_str_add(tmp_string, " 0x%08X", data);
			}

			if (trc) {
				nthw_log_helper_str_add(tmp_string, ")");
				NT_LOG(DBG, NTHW, "%s", tmp_string);
				nthw_log_helper_str_free(tmp_string);
			}
		}

		/* Read completion from out buffer */
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, &rab_echo_oper_cmpl);

		if (p->mn_param_rac_rab_ob_update)
			nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, 0);

		if (rab_echo_oper_cmpl != rab_oper_cmpl) {
			NT_LOG(ERR, NTHW,
				"%s: RAB: Unexpected value of completion (0x%08X)- inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
				p_adapter_id_str, rab_echo_oper_cmpl, in_buf_free, out_buf_free,
				buf_used);
			res = -1;
			goto exit_unlock_res;
		}

		/* Read buffer free register */
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, &buf_free);

		if (buf_free & 0x80000000) {
			/* Clear Timeout and overflow bits */
			nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, 0x0);
			NT_LOG(ERR, NTHW,
				"%s: RAB: timeout - Access outside register - bus: %d addr: 0x%08X - inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
				p_adapter_id_str, bus_id, address, in_buf_free, out_buf_free,
				buf_used);
			res = -1;
			goto exit_unlock_res;
		}

		res = 0;
		goto exit_unlock_res;

	} else {
		NT_LOG(ERR, NTHW,
			"%s: RAB: Fail rab bus buffer check - bus: %d addr: 0x%08X wordcount: %d - inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
			p_adapter_id_str, bus_id, address, word_cnt, in_buf_free, out_buf_free,
			buf_used);
		res = -1;
		goto exit_unlock_res;
	}

exit_unlock_res:
	rte_spinlock_unlock(&p->m_mutex);
	return res;
}

int nthw_rac_rab_read32(nthw_rac_t *p, bool trc, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, uint32_t *p_data)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t buf_used;
	uint32_t buf_free;
	uint32_t in_buf_free;
	uint32_t out_buf_free;
	int res = 0;

	rte_spinlock_lock(&p->m_mutex);

	if (address > (1 << RAB_ADDR_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal address: value too large %" PRIu32 " - max %d",
			p_adapter_id_str, address, (1 << RAB_ADDR_BW));
		res = -1;
		goto exit_unlock_res;
	}

	if (bus_id > (1 << RAB_BUSID_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal bus id: value too large %" PRIu32 " - max %d",
			p_adapter_id_str, bus_id, (1 << RAB_BUSID_BW));
		res = -1;
		goto exit_unlock_res;
	}

	if (word_cnt == 0) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal word count: value is zero (%" PRIu32 ")",
			p_adapter_id_str, word_cnt);
		res = -1;
		goto exit_unlock_res;
	}

	if (word_cnt > (1 << RAB_CNT_BW)) {
		NT_LOG(ERR, NTHW, "%s: RAB: Illegal word count: value too large %" PRIu32 " - max %d",
			p_adapter_id_str, word_cnt, (1 << RAB_CNT_BW));
		res = -1;
		goto exit_unlock_res;
	}

	/* Read buffer free register */
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, &buf_free);

	in_buf_free = buf_free & p->RAC_RAB_BUF_FREE_IB_FREE_MASK;
	out_buf_free = (buf_free & p->RAC_RAB_BUF_FREE_OB_FREE_MASK) >> 16;

	/* Read buffer used register */
	nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR, &buf_used);

	buf_used =
		buf_used & (p->RAC_RAB_BUF_USED_IB_USED_MASK | p->RAC_RAB_BUF_USED_OB_USED_MASK);

	/*
	 * Verify that output buffer can hold the number of words to be read,
	 * input buffer can hold one read command
	 * and that the input and output "used" buffer is 0
	 */
	if (out_buf_free >= word_cnt && in_buf_free >= 1 && buf_used == 0) {
		const uint32_t rab_oper_cmpl = (RAB_COMPLETION << RAB_OPR_LO);
		uint32_t rab_read_oper_cmpl;
		uint32_t word_cnt_expected = word_cnt + 1;
		uint32_t rab_oper_rd;

		rab_oper_rd = (RAB_READ << RAB_OPR_LO) |
			((word_cnt & ((1 << RAB_CNT_BW) - 1)) << RAB_CNT_LO) |
			(bus_id << RAB_BUSID_LO) | address;

		if (trc) {
			rab_oper_rd |= (RAB_ECHO << RAB_OPR_LO);
			word_cnt_expected++;
		}

		/* Write command */
		nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_IB_DATA_ADDR, rab_oper_rd);

		/* Write completion command */
		nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_IB_DATA_ADDR, rab_oper_cmpl);

		/* Wait until done */
		if (_nthw_rac_wait_for_rab_done(p, address, word_cnt_expected)) {
			res = -1;
			goto exit_unlock_res;
		}

		if (trc) {
			uint32_t rab_echo_oper_rd;
			nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR,
				&rab_echo_oper_rd);

			if (p->mn_param_rac_rab_ob_update)
				nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, 0);

			if (rab_oper_rd != rab_echo_oper_rd) {
				NT_LOG(ERR, NTHW,
					"%s: RAB: expected rab read echo oper (0x%08X) - read (0x%08X)",
					p_adapter_id_str, rab_oper_rd, rab_echo_oper_rd);
			}
		}

		{
			/* Read data from output buffer */
			uint32_t i;

			for (i = 0; i < word_cnt; i++) {
				nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR,
					&p_data[i]);

				if (p->mn_param_rac_rab_ob_update) {
					nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR,
						0);
				}
			}

			if (trc) {
				char *tmp_string = nthw_log_helper_str_alloc("Register::read");
				nthw_log_helper_str_add(tmp_string,
					"(Dev: NA, Bus: RAB%u, Addr: 0x%08X, Cnt: %d, Data:",
					bus_id, address, word_cnt);

				for (i = 0; i < word_cnt; i++)
					nthw_log_helper_str_add(tmp_string, " 0x%08X", p_data[i]);

				nthw_log_helper_str_add(tmp_string, ")");
				NT_LOG(DBG, NTHW, "%s", tmp_string);
				nthw_log_helper_str_free(tmp_string);
			}
		}

		/* Read completion from out buffer */
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, &rab_read_oper_cmpl);

		if (p->mn_param_rac_rab_ob_update)
			nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_OB_DATA_ADDR, 0);

		if (rab_read_oper_cmpl != rab_oper_cmpl) {
			NT_LOG(ERR, NTHW,
				"%s: RAB: Unexpected value of completion (0x%08X)- inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
				p_adapter_id_str, rab_read_oper_cmpl, in_buf_free, out_buf_free,
				buf_used);
			res = -1;
			goto exit_unlock_res;
		}

		/* Read buffer free register */
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, &buf_free);

		if (buf_free & 0x80000000) {
			/* Clear Timeout and overflow bits */
			nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, 0x0);
			NT_LOG(ERR, NTHW,
				"%s: RAB: timeout - Access outside register - bus: %d addr: 0x%08X - inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
				p_adapter_id_str, bus_id, address, in_buf_free, out_buf_free,
				buf_used);
			res = -1;
			goto exit_unlock_res;
		}

		res = 0;
		goto exit_unlock_res;

	} else {
		NT_LOG(ERR, NTHW,
			"%s: RAB: Fail rab bus buffer check - bus: %d addr: 0x%08X wordcount: %d - inBufFree: 0x%08X, outBufFree: 0x%08X, bufUsed: 0x%08X",
			p_adapter_id_str, bus_id, address, word_cnt, in_buf_free, out_buf_free,
			buf_used);
		res = -1;
		goto exit_unlock_res;
	}

exit_unlock_res:
	rte_spinlock_unlock(&p->m_mutex);
	return res;
}

int nthw_rac_rab_flush(nthw_rac_t *p)
{
	const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t data = 0;
	uint32_t retry;
	int res = 0;

	rte_spinlock_lock(&p->m_mutex);

	/* Set the flush bit */
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR,
		p->RAC_RAB_BUF_USED_FLUSH_MASK);

	/* Reset BUF FREE register */
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_BUF_FREE_ADDR, 0x0);

	/* Wait until OB_USED and IB_USED are 0 */
	for (retry = 0; retry < 100000; retry++) {
		nthw_rac_reg_read32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR, &data);

		if ((data & 0xFFFFFFFF) == p->RAC_RAB_BUF_USED_FLUSH_MASK)
			break;
	}

	if (data != p->RAC_RAB_BUF_USED_FLUSH_MASK) {
		NT_LOG(ERR, NTHW, "%s: RAB: Rab bus flush error.", p_adapter_id_str);
		res = -1;
	}

	/* Clear flush bit when done */
	nthw_rac_reg_write32(p_fpga_info, p->RAC_RAB_BUF_USED_ADDR, 0x0);

	rte_spinlock_unlock(&p->m_mutex);
	return res;
}
