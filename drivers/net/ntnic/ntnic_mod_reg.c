/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntnic_mod_reg.h"

static struct sg_ops_s *sg_ops;

void register_sg_ops(struct sg_ops_s *ops)
{
	sg_ops = ops;
}

const struct sg_ops_s *get_sg_ops(void)
{
	if (sg_ops == NULL)
		sg_init();
	return sg_ops;
}

static struct link_ops_s *link_100g_ops;

void register_100g_link_ops(struct link_ops_s *ops)
{
	link_100g_ops = ops;
}

const struct link_ops_s *get_100g_link_ops(void)
{
	if (link_100g_ops == NULL)
		link_100g_init();
	return link_100g_ops;
}

static const struct port_ops *port_ops;

void register_port_ops(const struct port_ops *ops)
{
	port_ops = ops;
}

const struct port_ops *get_port_ops(void)
{
	if (port_ops == NULL)
		port_init();
	return port_ops;
}

static const struct adapter_ops *adapter_ops;

void register_adapter_ops(const struct adapter_ops *ops)
{
	adapter_ops = ops;
}

const struct adapter_ops *get_adapter_ops(void)
{
	if (adapter_ops == NULL)
		adapter_init();
	return adapter_ops;
}

static struct clk9563_ops *clk9563_ops;

void register_clk9563_ops(struct clk9563_ops *ops)
{
	clk9563_ops = ops;
}

struct clk9563_ops *get_clk9563_ops(void)
{
	if (clk9563_ops == NULL)
		clk9563_ops_init();
	return clk9563_ops;
}

static struct rst_nt200a0x_ops *rst_nt200a0x_ops;

void register_rst_nt200a0x_ops(struct rst_nt200a0x_ops *ops)
{
	rst_nt200a0x_ops = ops;
}

struct rst_nt200a0x_ops *get_rst_nt200a0x_ops(void)
{
	if (rst_nt200a0x_ops == NULL)
		rst_nt200a0x_ops_init();
	return rst_nt200a0x_ops;
}

static struct rst9563_ops *rst9563_ops;

void register_rst9563_ops(struct rst9563_ops *ops)
{
	rst9563_ops = ops;
}

struct rst9563_ops *get_rst9563_ops(void)
{
	if (rst9563_ops == NULL)
		rst9563_ops_init();
	return rst9563_ops;
}

static const struct flow_backend_ops *flow_backend_ops;

void register_flow_backend_ops(const struct flow_backend_ops *ops)
{
	flow_backend_ops = ops;
}

const struct flow_backend_ops *get_flow_backend_ops(void)
{
	if (flow_backend_ops == NULL)
		flow_backend_init();

	return flow_backend_ops;
}

static const struct flow_filter_ops *flow_filter_ops;

void register_flow_filter_ops(const struct flow_filter_ops *ops)
{
	flow_filter_ops = ops;
}

const struct flow_filter_ops *get_flow_filter_ops(void)
{
	if (flow_filter_ops == NULL)
		init_flow_filter();

	return flow_filter_ops;
}
