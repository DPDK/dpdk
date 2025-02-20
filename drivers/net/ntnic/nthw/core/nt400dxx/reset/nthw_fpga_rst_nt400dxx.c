/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"
#include "nthw_hif.h"
#include "ntnic_mod_reg.h"

static int nthw_fpga_rst_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	int res = -1;
	nthw_fpga_t *p_fpga = NULL;

	p_fpga = p_fpga_info->mp_fpga;

	nthw_hif_t *p_nthw_hif = nthw_hif_new();
	res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

	if (res == 0)
		NT_LOG(DBG, NTHW, "%s: Hif module found", p_fpga_info->mp_adapter_id_str);

	/* (A) Test HIF clock is running by performing simple write/read test of HIF registers */
	const uint32_t test_pattern[2] = { 0x11223344, 0x55667788 };

	for (uint8_t i = 0; i < 2; ++i) {
		uint32_t test_data = 0;
		nthw_hif_write_test_reg(p_nthw_hif, i, test_pattern[i]);
		nthw_hif_read_test_reg(p_nthw_hif, i, &test_data);

		if (test_data != test_pattern[i]) {
			NT_LOG(ERR,
				NTHW,
				"%s: %s: Test sys 250 clock failed",
				p_fpga_info->mp_adapter_id_str,
				__func__);
			return -1;
		}
	}

	nthw_hif_delete(p_nthw_hif);

	/* (b) Init RAB0 */
	nthw_rac_rab_init(p_fpga_info->mp_nthw_rac, 0x7);
	nthw_rac_rab_init(p_fpga_info->mp_nthw_rac, 0x6);

	/* Create PRM */
	p_fpga_info->mp_nthw_agx.p_prm = nthw_prm_nt400dxx_new();
	res = nthw_prm_nt400dxx_init(p_fpga_info->mp_nthw_agx.p_prm, p_fpga, 0);

	if (res != 0)
		return res;

	/* (b1) Reset platform. It is released later */
	nthw_prm_nt400dxx_platform_rst(p_fpga_info->mp_nthw_agx.p_prm, 1);
	nt_os_wait_usec(10000);

	/* (C) Reset peripherals and release the reset */
	nthw_prm_nt400dxx_periph_rst(p_fpga_info->mp_nthw_agx.p_prm, 1);
	nt_os_wait_usec(10000);
	nthw_prm_nt400dxx_periph_rst(p_fpga_info->mp_nthw_agx.p_prm, 0);
	nt_os_wait_usec(10000);

	res = nthw_fpga_avr_probe(p_fpga, 0);

	if (res != 0)
		return res;

	/* Create I2C */
	p_fpga_info->mp_nthw_agx.p_i2cm = nthw_i2cm_new();
	res = nthw_i2cm_init(p_fpga_info->mp_nthw_agx.p_i2cm, p_fpga, 0);

	if (res != 0)
		return res;

	/* Create pca9849 i2c mux */
	p_fpga_info->mp_nthw_agx.p_pca9849 = nthw_pca9849_new();
	res = nthw_pca9849_init(p_fpga_info->mp_nthw_agx.p_pca9849,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x71);

	if (res != 0)
		return res;

	/* Create LED driver */
	p_fpga_info->mp_nthw_agx.p_pca9532_led = nthw_pca9532_new();
	res = nthw_pca9532_init(p_fpga_info->mp_nthw_agx.p_pca9532_led,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x60,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			3);

	if (res != 0)
		return res;

	for (uint8_t i = 0; i < 16; i++)
		nthw_pca9532_set_led_on(p_fpga_info->mp_nthw_agx.p_pca9532_led, i, false);

	/* Create PCM */
	p_fpga_info->mp_nthw_agx.p_pcm = nthw_pcm_nt400dxx_new();
	res = nthw_pcm_nt400dxx_init(p_fpga_info->mp_nthw_agx.p_pcm, p_fpga, 0);

	if (res != 0)
		return res;

	return 0;
}

static int nthw_fpga_rst_nt400dxx_reset(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	return 0;
}

static struct rst_nt400dxx_ops rst_nt400dxx_ops = {
	.nthw_fpga_rst_nt400dxx_init = nthw_fpga_rst_nt400dxx_init,
	.nthw_fpga_rst_nt400dxx_reset = nthw_fpga_rst_nt400dxx_reset
};

void rst_nt400dxx_ops_init(void)
{
	register_rst_nt400dxx_ops(&rst_nt400dxx_ops);
}
