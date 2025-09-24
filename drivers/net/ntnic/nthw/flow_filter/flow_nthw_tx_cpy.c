/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_tx_cpy.h"

void nthw_tx_cpy_set_debug_mode(struct tx_cpy_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_tx_cpy, n_debug_mode);
}

struct tx_cpy_nthw *nthw_tx_cpy_new(void)
{
	struct tx_cpy_nthw *p = malloc(sizeof(struct tx_cpy_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_tx_cpy_delete(struct tx_cpy_nthw *p)
{
	if (p) {
		free(p->m_writers);
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_tx_cpy_init(struct tx_cpy_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_TX_CPY, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: TxCpy %d: no such instance", p_adapter_id_str,
			n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_tx_cpy = nthw_fpga_query_module(p_fpga, MOD_TX_CPY, n_instance);

	const int writers_cnt = nthw_fpga_get_product_param(p->mp_fpga, NT_TX_CPY_WRITERS, 0);

	if (writers_cnt < 1)
		return -1;

	p->m_writers_cnt = (unsigned int)writers_cnt;
	p->m_writers = calloc(p->m_writers_cnt, sizeof(struct tx_cpy_writers_s));

	if (p->m_writers == NULL)
		return -1;

	const int variant = nthw_fpga_get_product_param(p->mp_fpga, NT_TX_CPY_VARIANT, 0);

	switch (p->m_writers_cnt) {
	default:
	case 6:
		p->m_writers[5].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER5_CTRL);
		p->m_writers[5].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[5].mp_writer_ctrl,
				CPY_WRITER5_CTRL_ADR);
		p->m_writers[5].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[5].mp_writer_ctrl,
				CPY_WRITER5_CTRL_CNT);
		p->m_writers[5].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER5_DATA);
		p->m_writers[5].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[5].mp_writer_data,
				CPY_WRITER5_DATA_READER_SELECT);
		p->m_writers[5].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[5].mp_writer_data,
				CPY_WRITER5_DATA_DYN);
		p->m_writers[5].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[5].mp_writer_data,
				CPY_WRITER5_DATA_OFS);
		p->m_writers[5].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[5].mp_writer_data,
				CPY_WRITER5_DATA_LEN);

		if (variant != 0) {
			p->m_writers[5].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[5].mp_writer_data,
					CPY_WRITER5_DATA_MASK_POINTER);
			p->m_writers[5].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER5_MASK_CTRL);
			p->m_writers[5].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[5].mp_writer_mask_ctrl,
					CPY_WRITER5_MASK_CTRL_ADR);
			p->m_writers[5].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[5].mp_writer_mask_ctrl,
					CPY_WRITER5_MASK_CTRL_CNT);
			p->m_writers[5].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER5_MASK_DATA);
			p->m_writers[5].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[5].mp_writer_mask_data,
					CPY_WRITER5_MASK_DATA_BYTE_MASK);
		}

	/* Fallthrough */
	case 5:
		p->m_writers[4].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER4_CTRL);
		p->m_writers[4].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[4].mp_writer_ctrl,
				CPY_WRITER4_CTRL_ADR);
		p->m_writers[4].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[4].mp_writer_ctrl,
				CPY_WRITER4_CTRL_CNT);
		p->m_writers[4].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER4_DATA);
		p->m_writers[4].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[4].mp_writer_data,
				CPY_WRITER4_DATA_READER_SELECT);
		p->m_writers[4].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[4].mp_writer_data,
				CPY_WRITER4_DATA_DYN);
		p->m_writers[4].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[4].mp_writer_data,
				CPY_WRITER4_DATA_OFS);
		p->m_writers[4].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[4].mp_writer_data,
				CPY_WRITER4_DATA_LEN);

		if (variant != 0) {
			p->m_writers[4].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[4].mp_writer_data,
					CPY_WRITER4_DATA_MASK_POINTER);
			p->m_writers[4].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER4_MASK_CTRL);
			p->m_writers[4].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[4].mp_writer_mask_ctrl,
					CPY_WRITER4_MASK_CTRL_ADR);
			p->m_writers[4].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[4].mp_writer_mask_ctrl,
					CPY_WRITER4_MASK_CTRL_CNT);
			p->m_writers[4].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER4_MASK_DATA);
			p->m_writers[4].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[4].mp_writer_mask_data,
					CPY_WRITER4_MASK_DATA_BYTE_MASK);
		}

	/* Fallthrough */
	case 4:
		p->m_writers[3].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER3_CTRL);
		p->m_writers[3].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[3].mp_writer_ctrl,
				CPY_WRITER3_CTRL_ADR);
		p->m_writers[3].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[3].mp_writer_ctrl,
				CPY_WRITER3_CTRL_CNT);
		p->m_writers[3].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER3_DATA);
		p->m_writers[3].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[3].mp_writer_data,
				CPY_WRITER3_DATA_READER_SELECT);
		p->m_writers[3].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[3].mp_writer_data,
				CPY_WRITER3_DATA_DYN);
		p->m_writers[3].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[3].mp_writer_data,
				CPY_WRITER3_DATA_OFS);
		p->m_writers[3].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[3].mp_writer_data,
				CPY_WRITER3_DATA_LEN);

		if (variant != 0) {
			p->m_writers[3].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[3].mp_writer_data,
					CPY_WRITER3_DATA_MASK_POINTER);
			p->m_writers[3].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER3_MASK_CTRL);
			p->m_writers[3].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[3].mp_writer_mask_ctrl,
					CPY_WRITER3_MASK_CTRL_ADR);
			p->m_writers[3].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[3].mp_writer_mask_ctrl,
					CPY_WRITER3_MASK_CTRL_CNT);
			p->m_writers[3].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER3_MASK_DATA);
			p->m_writers[3].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[3].mp_writer_mask_data,
					CPY_WRITER3_MASK_DATA_BYTE_MASK);
		}

	/* Fallthrough */
	case 3:
		p->m_writers[2].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER2_CTRL);
		p->m_writers[2].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[2].mp_writer_ctrl,
				CPY_WRITER2_CTRL_ADR);
		p->m_writers[2].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[2].mp_writer_ctrl,
				CPY_WRITER2_CTRL_CNT);
		p->m_writers[2].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER2_DATA);
		p->m_writers[2].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[2].mp_writer_data,
				CPY_WRITER2_DATA_READER_SELECT);
		p->m_writers[2].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[2].mp_writer_data,
				CPY_WRITER2_DATA_DYN);
		p->m_writers[2].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[2].mp_writer_data,
				CPY_WRITER2_DATA_OFS);
		p->m_writers[2].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[2].mp_writer_data,
				CPY_WRITER2_DATA_LEN);

		if (variant != 0) {
			p->m_writers[2].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[2].mp_writer_data,
					CPY_WRITER2_DATA_MASK_POINTER);
			p->m_writers[2].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER2_MASK_CTRL);
			p->m_writers[2].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[2].mp_writer_mask_ctrl,
					CPY_WRITER2_MASK_CTRL_ADR);
			p->m_writers[2].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[2].mp_writer_mask_ctrl,
					CPY_WRITER2_MASK_CTRL_CNT);
			p->m_writers[2].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER2_MASK_DATA);
			p->m_writers[2].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[2].mp_writer_mask_data,
					CPY_WRITER2_MASK_DATA_BYTE_MASK);
		}

	/* Fallthrough */
	case 2:
		p->m_writers[1].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER1_CTRL);
		p->m_writers[1].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[1].mp_writer_ctrl,
				CPY_WRITER1_CTRL_ADR);
		p->m_writers[1].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[1].mp_writer_ctrl,
				CPY_WRITER1_CTRL_CNT);
		p->m_writers[1].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER1_DATA);
		p->m_writers[1].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[1].mp_writer_data,
				CPY_WRITER1_DATA_READER_SELECT);
		p->m_writers[1].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[1].mp_writer_data,
				CPY_WRITER1_DATA_DYN);
		p->m_writers[1].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[1].mp_writer_data,
				CPY_WRITER1_DATA_OFS);
		p->m_writers[1].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[1].mp_writer_data,
				CPY_WRITER1_DATA_LEN);

		if (variant != 0) {
			p->m_writers[1].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[1].mp_writer_data,
					CPY_WRITER1_DATA_MASK_POINTER);
			p->m_writers[1].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER1_MASK_CTRL);
			p->m_writers[1].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[1].mp_writer_mask_ctrl,
					CPY_WRITER1_MASK_CTRL_ADR);
			p->m_writers[1].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[1].mp_writer_mask_ctrl,
					CPY_WRITER1_MASK_CTRL_CNT);
			p->m_writers[1].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER1_MASK_DATA);
			p->m_writers[1].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[1].mp_writer_mask_data,
					CPY_WRITER1_MASK_DATA_BYTE_MASK);
		}

	/* Fallthrough */
	case 1:
		p->m_writers[0].mp_writer_ctrl =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER0_CTRL);
		p->m_writers[0].mp_writer_ctrl_addr =
			nthw_register_get_field(p->m_writers[0].mp_writer_ctrl,
				CPY_WRITER0_CTRL_ADR);
		p->m_writers[0].mp_writer_ctrl_cnt =
			nthw_register_get_field(p->m_writers[0].mp_writer_ctrl,
				CPY_WRITER0_CTRL_CNT);
		p->m_writers[0].mp_writer_data =
			nthw_module_get_register(p->m_tx_cpy, CPY_WRITER0_DATA);
		p->m_writers[0].mp_writer_data_reader_select =
			nthw_register_get_field(p->m_writers[0].mp_writer_data,
				CPY_WRITER0_DATA_READER_SELECT);
		p->m_writers[0].mp_writer_data_dyn =
			nthw_register_get_field(p->m_writers[0].mp_writer_data,
				CPY_WRITER0_DATA_DYN);
		p->m_writers[0].mp_writer_data_ofs =
			nthw_register_get_field(p->m_writers[0].mp_writer_data,
				CPY_WRITER0_DATA_OFS);
		p->m_writers[0].mp_writer_data_len =
			nthw_register_get_field(p->m_writers[0].mp_writer_data,
				CPY_WRITER0_DATA_LEN);

		if (variant != 0) {
			p->m_writers[0].mp_writer_data_mask_pointer =
				nthw_register_get_field(p->m_writers[0].mp_writer_data,
					CPY_WRITER0_DATA_MASK_POINTER);
			p->m_writers[0].mp_writer_mask_ctrl =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER0_MASK_CTRL);
			p->m_writers[0].mp_writer_mask_ctrl_addr =
				nthw_register_get_field(p->m_writers[0].mp_writer_mask_ctrl,
					CPY_WRITER0_MASK_CTRL_ADR);
			p->m_writers[0].mp_writer_mask_ctrl_cnt =
				nthw_register_get_field(p->m_writers[0].mp_writer_mask_ctrl,
					CPY_WRITER0_MASK_CTRL_CNT);
			p->m_writers[0].mp_writer_mask_data =
				nthw_module_get_register(p->m_tx_cpy, CPY_WRITER0_MASK_DATA);
			p->m_writers[0].mp_writer_mask_data_byte_mask =
				nthw_register_get_field(p->m_writers[0].mp_writer_mask_data,
					CPY_WRITER0_MASK_DATA_BYTE_MASK);
		}

		break;

	case 0:
		return -1;
	}

	return 0;
}

void nthw_tx_cpy_writer_select(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_ctrl_addr, val);
}

void nthw_tx_cpy_writer_cnt(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_ctrl_cnt, val);
}

void nthw_tx_cpy_writer_reader_select(const struct tx_cpy_nthw *p, unsigned int index,
	uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_data_reader_select, val);
}

void nthw_tx_cpy_writer_dyn(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_data_dyn, val);
}

void nthw_tx_cpy_writer_ofs(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_data_ofs, val);
}

void nthw_tx_cpy_writer_len(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_field_set_val32(p->m_writers[index].mp_writer_data_len, val);
}

void nthw_tx_cpy_writer_flush(const struct tx_cpy_nthw *p, unsigned int index)
{
	RTE_ASSERT(index < p->m_writers_cnt);
	nthw_register_flush(p->m_writers[index].mp_writer_ctrl, 1);
	nthw_register_flush(p->m_writers[index].mp_writer_data, 1);
}
