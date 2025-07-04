/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_RAC_H__
#define __NTHW_RAC_H__

#include "nt_util.h"

#define RAB_DMA_BUF_CNT (0x4000)

typedef uint8_t nthw_rab_bus_id_t;

struct nthw_rac {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_rac;

	rte_spinlock_t m_mutex;

	int mn_param_rac_rab_interfaces;
	int mn_param_rac_rab_ob_update;

	nthw_register_t *mp_reg_dummy0;
	nthw_register_t *mp_reg_dummy1;
	nthw_register_t *mp_reg_dummy2;

	nthw_register_t *mp_reg_rab_init;
	nthw_field_t *mp_fld_rab_init;

	int mn_fld_rab_init_bw;
	uint32_t mn_fld_rab_init_mask;

	nthw_register_t *mp_reg_dbg_ctrl;
	nthw_field_t *mp_fld_dbg_ctrl;

	nthw_register_t *mp_reg_dbg_data;
	nthw_field_t *mp_fld_dbg_data;

	nthw_register_t *mp_reg_rab_ib_data;
	nthw_field_t *mp_fld_rab_ib_data;

	nthw_register_t *mp_reg_rab_ob_data;
	nthw_field_t *mp_fld_rab_ob_data;

	nthw_register_t *mp_reg_rab_buf_free;
	nthw_field_t *mp_fld_rab_buf_free_ib_free;
	nthw_field_t *mp_fld_rab_buf_free_ib_ovf;
	nthw_field_t *mp_fld_rab_buf_free_ob_free;
	nthw_field_t *mp_fld_rab_buf_free_ob_ovf;
	nthw_field_t *mp_fld_rab_buf_free_timeout;

	nthw_register_t *mp_reg_rab_buf_used;
	nthw_field_t *mp_fld_rab_buf_used_ib_used;
	nthw_field_t *mp_fld_rab_buf_used_ob_used;
	nthw_field_t *mp_fld_rab_buf_used_flush;

	nthw_register_t *mp_reg_rab_dma_ib_lo;
	nthw_field_t *mp_fld_rab_dma_ib_lo_phy_addr;

	nthw_register_t *mp_reg_rab_dma_ib_hi;
	nthw_field_t *mp_fld_rab_dma_ib_hi_phy_addr;

	nthw_register_t *mp_reg_rab_dma_ob_hi;
	nthw_field_t *mp_fld_rab_dma_ob_hi_phy_addr;

	nthw_register_t *mp_reg_rab_dma_ob_lo;
	nthw_field_t *mp_fld_rab_dma_ob_lo_phy_addr;

	nthw_register_t *mp_reg_rab_dma_ib_wr;
	nthw_field_t *mp_fld_rab_dma_ib_wr_ptr;

	nthw_register_t *mp_reg_rab_dma_ib_rd;
	nthw_field_t *mp_fld_rab_dma_ib_rd_ptr;

	nthw_register_t *mp_reg_rab_dma_ob_wr;
	nthw_field_t *mp_fld_rab_dma_ob_wr_ptr;

	nthw_register_t *mp_reg_rab_nmb_rd;
	nthw_register_t *mp_reg_rab_nmb_data;
	nthw_register_t *mp_reg_rab_nmb_wr;
	nthw_register_t *mp_reg_rab_nmb_status;

	uint32_t RAC_RAB_INIT_ADDR;
	uint32_t RAC_RAB_IB_DATA_ADDR;
	uint32_t RAC_RAB_OB_DATA_ADDR;
	uint32_t RAC_RAB_BUF_FREE_ADDR;
	uint32_t RAC_RAB_BUF_USED_ADDR;

	uint32_t RAC_RAB_DMA_IB_LO_ADDR;
	uint32_t RAC_RAB_DMA_IB_HI_ADDR;
	uint32_t RAC_RAB_DMA_OB_LO_ADDR;
	uint32_t RAC_RAB_DMA_OB_HI_ADDR;
	uint32_t RAC_RAB_DMA_IB_RD_ADDR;
	uint32_t RAC_RAB_DMA_OB_WR_ADDR;
	uint32_t RAC_RAB_DMA_IB_WR_ADDR;

	uint32_t RAC_RAB_BUF_FREE_IB_FREE_MASK;
	uint32_t RAC_RAB_BUF_FREE_OB_FREE_MASK;
	uint32_t RAC_RAB_BUF_USED_IB_USED_MASK;
	uint32_t RAC_RAB_BUF_USED_OB_USED_MASK;
	uint32_t RAC_RAB_BUF_USED_FLUSH_MASK;

	uint32_t RAC_RAB_BUF_USED_OB_USED_LOW;

	uint32_t RAC_NMB_RD_ADR_ADDR;
	uint32_t RAC_NMB_DATA_ADDR;
	uint32_t RAC_NMB_WR_ADR_ADDR;
	uint32_t RAC_NMB_STATUS_ADDR;

	bool m_dma_active;

	struct nt_dma_s *m_dma;

	volatile uint32_t *m_dma_in_buf;
	volatile uint32_t *m_dma_out_buf;

	uint16_t m_dma_out_ptr_rd;
	uint16_t m_dma_in_ptr_wr;
	uint32_t m_in_free;
};

typedef struct nthw_rac nthw_rac_t;
typedef struct nthw_rac nthw_rac;

struct dma_buf_ptr {
	uint32_t size;
	uint32_t index;
	volatile uint32_t *base;
};

nthw_rac_t *nthw_rac_new(void);
int nthw_rac_init(nthw_rac_t *p, nthw_fpga_t *p_fpga, struct fpga_info_s *p_fpga_info);

int nthw_rac_rab_init(nthw_rac_t *p, uint32_t n_rab_intf_mask);

int nthw_rac_rab_setup(nthw_rac_t *p);

int nthw_rac_rab_reset(nthw_rac_t *p);

int nthw_rac_rab_write32(nthw_rac_t *p, bool trc, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, const uint32_t *p_data);
int nthw_rac_rab_write32_dma(nthw_rac_t *p, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, const uint32_t *p_data);
int nthw_rac_rab_read32(nthw_rac_t *p, bool trc, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, uint32_t *p_data);
int nthw_rac_rab_read32_dma(nthw_rac_t *p, nthw_rab_bus_id_t bus_id, uint32_t address,
	uint32_t word_cnt, struct dma_buf_ptr *buf_ptr);

uint32_t nthw_rac_rab_get_free(nthw_rac_t *p);

int nthw_rac_rab_flush(nthw_rac_t *p);

int nthw_rac_rab_dma_begin(nthw_rac_t *p);
int nthw_rac_rab_dma_commit(nthw_rac_t *p);

void nthw_rac_bar0_read32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t word_cnt, uint32_t *p_data);
void nthw_rac_bar0_write32(const struct fpga_info_s *p_fpga_info, uint32_t reg_addr,
	uint32_t word_cnt, const uint32_t *p_data);

#endif	/* __NTHW_RAC_H__ */
