/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntnic_mod_reg.h"

static struct sg_ops_s *sg_ops;

void nthw_reg_sg_ops(struct sg_ops_s *ops)
{
	sg_ops = ops;
}

const struct sg_ops_s *nthw_get_sg_ops(void)
{
	if (sg_ops == NULL)
		nthw_sg_init();
	return sg_ops;
}

/*
 *
 */
static struct meter_ops_s *meter_ops;

void nthw_reg_meter_ops(struct meter_ops_s *ops)
{
	meter_ops = ops;
}

const struct meter_ops_s *nthw_get_meter_ops(void)
{
	if (meter_ops == NULL)
		nthw_meter_init();

	return meter_ops;
}

/*
 *
 */
static const struct ntnic_filter_ops *ntnic_filter_ops;

void nthw_reg_filter_ops(const struct ntnic_filter_ops *ops)
{
	ntnic_filter_ops = ops;
}

const struct ntnic_filter_ops *nthw_get_filter_ops(void)
{
	if (ntnic_filter_ops == NULL)
		nthw_filter_ops_init();

	return ntnic_filter_ops;
}

static struct link_ops_s *link_100g_ops;

void nthw_reg_100g_link_ops(struct link_ops_s *ops)
{
	link_100g_ops = ops;
}

const struct link_ops_s *nthw_get_100g_link_ops(void)
{
	if (link_100g_ops == NULL)
		nthw_link_100g_init();
	return link_100g_ops;
}

/*
 *
 */
static struct link_ops_s *link_agx_100g_ops;

void nthw_reg_agx_100g_link_ops(struct link_ops_s *ops)
{
	link_agx_100g_ops = ops;
}

const struct link_ops_s *nthw_get_agx_100g_link_ops(void)
{
	if (link_agx_100g_ops == NULL)
		nthw_link_agx_100g_ops_init();
	return link_agx_100g_ops;
}

static const struct port_ops *port_ops;

void nthw_reg_port_ops(const struct port_ops *ops)
{
	port_ops = ops;
}

const struct port_ops *nthw_get_port_ops(void)
{
	if (port_ops == NULL)
		nthw_port_init();
	return port_ops;
}

static const struct nt4ga_stat_ops *nt4ga_stat_ops;

void nthw_reg_nt4ga_stat_ops(const struct nt4ga_stat_ops *ops)
{
	nt4ga_stat_ops = ops;
}

const struct nt4ga_stat_ops *nthw_get_nt4ga_stat_ops(void)
{
	if (nt4ga_stat_ops == NULL)
		nthw_stat_ops_init();

	return nt4ga_stat_ops;
}

static const struct adapter_ops *adapter_ops;

void nthw_reg_adapter_ops(const struct adapter_ops *ops)
{
	adapter_ops = ops;
}

const struct adapter_ops *nthw_get_adapter_ops(void)
{
	if (adapter_ops == NULL)
		nthw_adapter_init();
	return adapter_ops;
}

static struct clk9563_ops *clk9563_ops;

void nthw_reg_clk9563_ops(struct clk9563_ops *ops)
{
	clk9563_ops = ops;
}

struct clk9563_ops *nthw_get_clk9563_ops(void)
{
	if (clk9563_ops == NULL)
		nthw_clk9563_ops_init();
	return clk9563_ops;
}

static struct rst_nt200a0x_ops *rst_nt200a0x_ops;

void nthw_reg_rst_nt200a0x_ops(struct rst_nt200a0x_ops *ops)
{
	rst_nt200a0x_ops = ops;
}

struct rst_nt200a0x_ops *nthw_get_rst_nt200a0x_ops(void)
{
	if (rst_nt200a0x_ops == NULL)
		nthw_rst_nt200a0x_ops_init();
	return rst_nt200a0x_ops;
}

static struct rst9563_ops *rst9563_ops;

void nthw_reg_rst9563_ops(struct rst9563_ops *ops)
{
	rst9563_ops = ops;
}

struct rst9563_ops *nthw_get_rst9563_ops(void)
{
	if (rst9563_ops == NULL)
		nthw_rst9563_ops_init();
	return rst9563_ops;
}

static const struct flow_backend_ops *flow_backend_ops;

void nthw_reg_flow_backend_ops(const struct flow_backend_ops *ops)
{
	flow_backend_ops = ops;
}

static struct rst9574_ops *rst9574_ops;

void nthw_reg_rst9574_ops(struct rst9574_ops *ops)
{
	rst9574_ops = ops;
}

struct rst9574_ops *nthw_get_rst9574_ops(void)
{
	if (rst9574_ops == NULL)
		nthw_rst9574_ops_init();

	return rst9574_ops;
}

static struct rst_nt400dxx_ops *rst_nt400dxx_ops;

void nthw_reg_rst_nt400dxx_ops(struct rst_nt400dxx_ops *ops)
{
	rst_nt400dxx_ops = ops;
}

struct rst_nt400dxx_ops *nthw_get_rst_nt400dxx_ops(void)
{
	if (rst_nt400dxx_ops == NULL)
		nthw_rst_nt400dxx_ops_init();

	return rst_nt400dxx_ops;
}

const struct flow_backend_ops *nthw_get_flow_backend_ops(void)
{
	if (flow_backend_ops == NULL)
		nthw_flow_backend_init();

	return flow_backend_ops;
}

static const struct profile_inline_ops *profile_inline_ops;

void nthw_reg_profile_inline_ops(const struct profile_inline_ops *ops)
{
	profile_inline_ops = ops;
}

const struct profile_inline_ops *nthw_get_profile_inline_ops(void)
{
	if (profile_inline_ops == NULL)
		nthw_profile_inline_init();

	return profile_inline_ops;
}

static const struct flow_filter_ops *flow_filter_ops;

void nthw_reg_flow_filter_ops(const struct flow_filter_ops *ops)
{
	flow_filter_ops = ops;
}

const struct flow_filter_ops *nthw_get_flow_filter_ops(void)
{
	if (flow_filter_ops == NULL)
		nthw_init_flow_filter();

	return flow_filter_ops;
}

static const struct rte_flow_fp_ops *dev_fp_flow_ops;

void nthw_reg_dev_fp_flow_ops(const struct rte_flow_fp_ops *ops)
{
	dev_fp_flow_ops = ops;
}

const struct rte_flow_fp_ops *nthw_get_dev_fp_flow_ops(void)
{
	if (dev_fp_flow_ops == NULL)
		nthw_dev_fp_flow_init();

	return dev_fp_flow_ops;
}

static const struct rte_flow_ops *dev_flow_ops;

void nthw_reg_dev_flow_ops(const struct rte_flow_ops *ops)
{
	dev_flow_ops = ops;
}

const struct rte_flow_ops *nthw_get_dev_flow_ops(void)
{
	if (dev_flow_ops == NULL)
		nthw_dev_flow_init();

	return dev_flow_ops;
}

static struct ntnic_xstats_ops *ntnic_xstats_ops;

void nthw_reg_xstats_ops(struct ntnic_xstats_ops *ops)
{
	ntnic_xstats_ops = ops;
}

struct ntnic_xstats_ops *nthw_get_xstats_ops(void)
{
	if (ntnic_xstats_ops == NULL)
		nthw_xstats_ops_init();

	return ntnic_xstats_ops;
}
