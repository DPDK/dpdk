/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_byteorder.h>

#include "nitrox_hal.h"
#include "nitrox_csr.h"

#define MAX_VF_QUEUES	8
#define MAX_PF_QUEUES	64
#define NITROX_TIMER_THOLD	0x3FFFFF
#define NITROX_COUNT_THOLD      0xFFFFFFFF

void
nps_pkt_input_ring_disable(uint8_t *bar_addr, uint16_t ring)
{
	union nps_pkt_in_instr_ctl pkt_in_instr_ctl;
	uint64_t reg_addr;
	int max_retries = 5;

	reg_addr = NPS_PKT_IN_INSTR_CTLX(ring);
	pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	pkt_in_instr_ctl.s.enb = 0;
	nitrox_write_csr(bar_addr, reg_addr, pkt_in_instr_ctl.u64);
	rte_delay_us_block(100);

	/* wait for enable bit to be cleared */
	pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	while (pkt_in_instr_ctl.s.enb && max_retries--) {
		rte_delay_ms(10);
		pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	}
}

void
nps_pkt_solicited_port_disable(uint8_t *bar_addr, uint16_t port)
{
	union nps_pkt_slc_ctl pkt_slc_ctl;
	uint64_t reg_addr;
	int max_retries = 5;

	/* clear enable bit */
	reg_addr = NPS_PKT_SLC_CTLX(port);
	pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	pkt_slc_ctl.s.enb = 0;
	nitrox_write_csr(bar_addr, reg_addr, pkt_slc_ctl.u64);
	rte_delay_us_block(100);

	pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	while (pkt_slc_ctl.s.enb && max_retries--) {
		rte_delay_ms(10);
		pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	}
}

void
setup_nps_pkt_input_ring(uint8_t *bar_addr, uint16_t ring, uint32_t rsize,
			 phys_addr_t raddr)
{
	union nps_pkt_in_instr_ctl pkt_in_instr_ctl;
	union nps_pkt_in_instr_rsize pkt_in_instr_rsize;
	union nps_pkt_in_instr_baoff_dbell pkt_in_instr_baoff_dbell;
	union nps_pkt_in_done_cnts pkt_in_done_cnts;
	uint64_t base_addr, reg_addr;
	int max_retries = 5;

	nps_pkt_input_ring_disable(bar_addr, ring);

	/* write base address */
	reg_addr = NPS_PKT_IN_INSTR_BADDRX(ring);
	base_addr = raddr;
	nitrox_write_csr(bar_addr, reg_addr, base_addr);
	rte_delay_us_block(CSR_DELAY);

	/* write ring size */
	reg_addr = NPS_PKT_IN_INSTR_RSIZEX(ring);
	pkt_in_instr_rsize.u64 = 0;
	pkt_in_instr_rsize.s.rsize = rsize;
	nitrox_write_csr(bar_addr, reg_addr, pkt_in_instr_rsize.u64);
	rte_delay_us_block(CSR_DELAY);

	/* clear door bell */
	reg_addr = NPS_PKT_IN_INSTR_BAOFF_DBELLX(ring);
	pkt_in_instr_baoff_dbell.u64 = 0;
	pkt_in_instr_baoff_dbell.s.dbell = 0xFFFFFFFF;
	nitrox_write_csr(bar_addr, reg_addr, pkt_in_instr_baoff_dbell.u64);
	rte_delay_us_block(CSR_DELAY);

	/* clear done count */
	reg_addr = NPS_PKT_IN_DONE_CNTSX(ring);
	pkt_in_done_cnts.u64 = nitrox_read_csr(bar_addr, reg_addr);
	nitrox_write_csr(bar_addr, reg_addr, pkt_in_done_cnts.u64);
	rte_delay_us_block(CSR_DELAY);

	/* Setup PKT IN RING Interrupt Threshold */
	reg_addr = NPS_PKT_IN_INT_LEVELSX(ring);
	nitrox_write_csr(bar_addr, reg_addr, 0xFFFFFFFF);
	rte_delay_us_block(CSR_DELAY);

	/* enable ring */
	reg_addr = NPS_PKT_IN_INSTR_CTLX(ring);
	pkt_in_instr_ctl.u64 = 0;
	pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	pkt_in_instr_ctl.s.is64b = 1;
	pkt_in_instr_ctl.s.enb = 1;
	nitrox_write_csr(bar_addr, reg_addr, pkt_in_instr_ctl.u64);
	rte_delay_us_block(100);

	pkt_in_instr_ctl.u64 = 0;
	pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	/* wait for ring to be enabled */
	while (!pkt_in_instr_ctl.s.enb && max_retries--) {
		rte_delay_ms(10);
		pkt_in_instr_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	}
}

void
setup_nps_pkt_solicit_output_port(uint8_t *bar_addr, uint16_t port)
{
	union nps_pkt_slc_ctl pkt_slc_ctl;
	union nps_pkt_slc_cnts pkt_slc_cnts;
	union nps_pkt_slc_int_levels pkt_slc_int_levels;
	uint64_t reg_addr;
	int max_retries = 5;

	nps_pkt_solicited_port_disable(bar_addr, port);

	/* clear pkt counts */
	reg_addr = NPS_PKT_SLC_CNTSX(port);
	pkt_slc_cnts.u64 = nitrox_read_csr(bar_addr, reg_addr);
	nitrox_write_csr(bar_addr, reg_addr, pkt_slc_cnts.u64);
	rte_delay_us_block(CSR_DELAY);

	/* slc interrupt levels */
	reg_addr = NPS_PKT_SLC_INT_LEVELSX(port);
	pkt_slc_int_levels.u64 = 0;
	pkt_slc_int_levels.s.bmode = 0;
	pkt_slc_int_levels.s.timet = NITROX_TIMER_THOLD;

	if (NITROX_COUNT_THOLD > 0)
		pkt_slc_int_levels.s.cnt = NITROX_COUNT_THOLD - 1;

	nitrox_write_csr(bar_addr, reg_addr, pkt_slc_int_levels.u64);
	rte_delay_us_block(CSR_DELAY);

	/* enable ring */
	reg_addr = NPS_PKT_SLC_CTLX(port);
	pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	pkt_slc_ctl.s.rh = 1;
	pkt_slc_ctl.s.z = 1;
	pkt_slc_ctl.s.enb = 1;
	nitrox_write_csr(bar_addr, reg_addr, pkt_slc_ctl.u64);
	rte_delay_us_block(100);

	pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	while (!pkt_slc_ctl.s.enb && max_retries--) {
		rte_delay_ms(10);
		pkt_slc_ctl.u64 = nitrox_read_csr(bar_addr, reg_addr);
	}
}

int
vf_get_vf_config_mode(uint8_t *bar_addr)
{
	union aqmq_qsz aqmq_qsz;
	uint64_t reg_addr;
	int q, vf_mode;

	aqmq_qsz.u64 = 0;
	aqmq_qsz.s.host_queue_size = 0xDEADBEEF;
	reg_addr = AQMQ_QSZX(0);
	nitrox_write_csr(bar_addr, reg_addr, aqmq_qsz.u64);
	rte_delay_us_block(CSR_DELAY);

	aqmq_qsz.u64 = 0;
	for (q = 1; q < MAX_VF_QUEUES; q++) {
		reg_addr = AQMQ_QSZX(q);
		aqmq_qsz.u64 = nitrox_read_csr(bar_addr, reg_addr);
		if (aqmq_qsz.s.host_queue_size == 0xDEADBEEF)
			break;
	}

	switch (q) {
	case 1:
		vf_mode = NITROX_MODE_VF128;
		break;
	case 2:
		vf_mode = NITROX_MODE_VF64;
		break;
	case 4:
		vf_mode = NITROX_MODE_VF32;
		break;
	case 8:
		vf_mode = NITROX_MODE_VF16;
		break;
	default:
		vf_mode = 0;
		break;
	}

	return vf_mode;
}

int
vf_config_mode_to_nr_queues(enum nitrox_vf_mode vf_mode)
{
	int nr_queues;

	switch (vf_mode) {
	case NITROX_MODE_PF:
		nr_queues = MAX_PF_QUEUES;
		break;
	case NITROX_MODE_VF16:
		nr_queues = 8;
		break;
	case NITROX_MODE_VF32:
		nr_queues = 4;
		break;
	case NITROX_MODE_VF64:
		nr_queues = 2;
		break;
	case NITROX_MODE_VF128:
		nr_queues = 1;
		break;
	default:
		nr_queues = 0;
		break;
	}

	return nr_queues;
}
