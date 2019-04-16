/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

int port_get_prop(struct ifpga_port_hw *port, struct feature_prop *prop)
{
	struct ifpga_feature *feature;

	if (!port)
		return -ENOENT;

	feature = get_port_feature_by_id(port, prop->feature_id);

	if (feature && feature->ops && feature->ops->get_prop)
		return feature->ops->get_prop(feature, prop);

	return -ENOENT;
}

int port_set_prop(struct ifpga_port_hw *port, struct feature_prop *prop)
{
	struct ifpga_feature *feature;

	if (!port)
		return -ENOENT;

	feature = get_port_feature_by_id(port, prop->feature_id);

	if (feature && feature->ops && feature->ops->set_prop)
		return feature->ops->set_prop(feature, prop);

	return -ENOENT;
}

int port_set_irq(struct ifpga_port_hw *port, u32 feature_id, void *irq_set)
{
	struct ifpga_feature *feature;

	if (!port)
		return -ENOENT;

	feature = get_port_feature_by_id(port, feature_id);

	if (feature && feature->ops && feature->ops->set_irq)
		return feature->ops->set_irq(feature, irq_set);

	return -ENOENT;
}

static int port_get_revision(struct ifpga_port_hw *port, u64 *revision)
{
	struct feature_port_header *port_hdr
		= get_port_feature_ioaddr_by_index(port,
						   PORT_FEATURE_ID_HEADER);
	struct feature_header header;

	header.csr = readq(&port_hdr->header);

	*revision = header.revision;

	return 0;
}

static int port_get_portidx(struct ifpga_port_hw *port, u64 *idx)
{
	struct feature_port_header *port_hdr;
	struct feature_port_capability capability;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	capability.csr = readq(&port_hdr->capability);
	*idx = capability.port_number;

	return 0;
}

static int port_get_latency_tolerance(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_control control;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	control.csr = readq(&port_hdr->control);
	*val = control.latency_tolerance;

	return 0;
}

static int port_get_ap1_event(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_status status;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	status.csr = readq(&port_hdr->status);
	spinlock_unlock(&port->lock);

	*val = status.ap1_event;

	return 0;
}

static int port_set_ap1_event(struct ifpga_port_hw *port, u64 val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_status status;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	status.csr = readq(&port_hdr->status);
	status.ap1_event = val;
	writeq(status.csr, &port_hdr->status);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_get_ap2_event(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_status status;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	status.csr = readq(&port_hdr->status);
	spinlock_unlock(&port->lock);

	*val = status.ap2_event;

	return 0;
}

static int port_set_ap2_event(struct ifpga_port_hw *port, u64 val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_status status;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	status.csr = readq(&port_hdr->status);
	status.ap2_event = val;
	writeq(status.csr, &port_hdr->status);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_get_power_state(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;
	struct feature_port_status status;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	status.csr = readq(&port_hdr->status);
	spinlock_unlock(&port->lock);

	*val = status.power_state;

	return 0;
}

static int port_get_userclk_freqcmd(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	*val = readq(&port_hdr->user_clk_freq_cmd0);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_set_userclk_freqcmd(struct ifpga_port_hw *port, u64 val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	writeq(val, &port_hdr->user_clk_freq_cmd0);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_get_userclk_freqcntrcmd(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	*val = readq(&port_hdr->user_clk_freq_cmd1);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_set_userclk_freqcntrcmd(struct ifpga_port_hw *port, u64 val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	writeq(val, &port_hdr->user_clk_freq_cmd1);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_get_userclk_freqsts(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	*val = readq(&port_hdr->user_clk_freq_sts0);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_get_userclk_freqcntrsts(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_header *port_hdr;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	spinlock_lock(&port->lock);
	*val = readq(&port_hdr->user_clk_freq_sts1);
	spinlock_unlock(&port->lock);

	return 0;
}

static int port_hdr_init(struct ifpga_feature *feature)
{
	struct ifpga_port_hw *port = feature->parent;

	dev_info(NULL, "port hdr Init.\n");

	fpga_port_reset(port);

	return 0;
}

static void port_hdr_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "port hdr uinit.\n");
}

static int port_hdr_get_prop(struct ifpga_feature *feature,
		struct feature_prop *prop)
{
	struct ifpga_port_hw *port = feature->parent;

	switch (prop->prop_id) {
	case PORT_HDR_PROP_REVISION:
		return port_get_revision(port, &prop->data);
	case PORT_HDR_PROP_PORTIDX:
		return port_get_portidx(port, &prop->data);
	case PORT_HDR_PROP_LATENCY_TOLERANCE:
		return port_get_latency_tolerance(port, &prop->data);
	case PORT_HDR_PROP_AP1_EVENT:
		return port_get_ap1_event(port, &prop->data);
	case PORT_HDR_PROP_AP2_EVENT:
		return port_get_ap2_event(port, &prop->data);
	case PORT_HDR_PROP_POWER_STATE:
		return port_get_power_state(port, &prop->data);
	case PORT_HDR_PROP_USERCLK_FREQCMD:
		return port_get_userclk_freqcmd(port, &prop->data);
	case PORT_HDR_PROP_USERCLK_FREQCNTRCMD:
		return port_get_userclk_freqcntrcmd(port, &prop->data);
	case PORT_HDR_PROP_USERCLK_FREQSTS:
		return port_get_userclk_freqsts(port, &prop->data);
	case PORT_HDR_PROP_USERCLK_CNTRSTS:
		return port_get_userclk_freqcntrsts(port, &prop->data);
	}

	return -ENOENT;
}

static int port_hdr_set_prop(struct ifpga_feature *feature,
		struct feature_prop *prop)
{
	struct ifpga_port_hw *port = feature->parent;

	switch (prop->prop_id) {
	case PORT_HDR_PROP_AP1_EVENT:
		return port_set_ap1_event(port, prop->data);
	case PORT_HDR_PROP_AP2_EVENT:
		return port_set_ap2_event(port, prop->data);
	case PORT_HDR_PROP_USERCLK_FREQCMD:
		return port_set_userclk_freqcmd(port, prop->data);
	case PORT_HDR_PROP_USERCLK_FREQCNTRCMD:
		return port_set_userclk_freqcntrcmd(port, prop->data);
	}

	return -ENOENT;
}

struct ifpga_feature_ops ifpga_rawdev_port_hdr_ops = {
	.init = port_hdr_init,
	.uinit = port_hdr_uinit,
	.get_prop = port_hdr_get_prop,
	.set_prop = port_hdr_set_prop,
};

static int port_stp_init(struct ifpga_feature *feature)
{
	struct ifpga_port_hw *port = feature->parent;

	dev_info(NULL, "port stp Init.\n");

	spinlock_lock(&port->lock);
	port->stp_addr = feature->addr;
	port->stp_size = feature->size;
	spinlock_unlock(&port->lock);

	return 0;
}

static void port_stp_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "port stp uinit.\n");
}

struct ifpga_feature_ops ifpga_rawdev_port_stp_ops = {
	.init = port_stp_init,
	.uinit = port_stp_uinit,
};

static int port_uint_init(struct ifpga_feature *feature)
{
	struct ifpga_port_hw *port = feature->parent;

	dev_info(NULL, "PORT UINT Init.\n");

	spinlock_lock(&port->lock);
	if (feature->ctx_num) {
		port->capability |= FPGA_PORT_CAP_UAFU_IRQ;
		port->num_uafu_irqs = feature->ctx_num;
	}
	spinlock_unlock(&port->lock);

	return 0;
}

static void port_uint_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "PORT UINT UInit.\n");
}

struct ifpga_feature_ops ifpga_rawdev_port_uint_ops = {
	.init = port_uint_init,
	.uinit = port_uint_uinit,
};

static int port_afu_init(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "PORT AFU Init.\n");

	return 0;
}

static void port_afu_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "PORT AFU UInit.\n");
}

struct ifpga_feature_ops ifpga_rawdev_port_afu_ops = {
	.init = port_afu_init,
	.uinit = port_afu_uinit,
};
