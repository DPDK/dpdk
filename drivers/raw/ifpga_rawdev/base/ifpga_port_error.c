/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

static int port_err_get_revision(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_error *port_err;
	struct feature_header header;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);
	header.csr = readq(&port_err->header);
	*val = header.revision;

	return 0;
}

static int port_err_get_errors(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_error *port_err;
	struct feature_port_err_key error;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);
	error.csr = readq(&port_err->port_error);
	*val = error.csr;

	return 0;
}

static int port_err_get_first_error(struct ifpga_port_hw *port, u64 *val)
{
	struct feature_port_error *port_err;
	struct feature_port_first_err_key first_error;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);
	first_error.csr = readq(&port_err->port_first_error);
	*val = first_error.csr;

	return 0;
}

static int port_err_get_first_malformed_req_lsb(struct ifpga_port_hw *port,
						u64 *val)
{
	struct feature_port_error *port_err;
	struct feature_port_malformed_req0 malreq0;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);

	malreq0.header_lsb = readq(&port_err->malreq0);
	*val = malreq0.header_lsb;

	return 0;
}

static int port_err_get_first_malformed_req_msb(struct ifpga_port_hw *port,
						u64 *val)
{
	struct feature_port_error *port_err;
	struct feature_port_malformed_req1 malreq1;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);

	malreq1.header_msb = readq(&port_err->malreq1);
	*val = malreq1.header_msb;

	return 0;
}

static int port_err_set_clear(struct ifpga_port_hw *port, u64 val)
{
	int ret;

	spinlock_lock(&port->lock);
	ret = port_err_clear(port, val);
	spinlock_unlock(&port->lock);

	return ret;
}

static int port_error_init(struct feature *feature)
{
	struct ifpga_port_hw *port = feature->parent;

	dev_info(NULL, "port error Init.\n");

	spinlock_lock(&port->lock);
	port_err_mask(port, false);
	if (feature->ctx_num)
		port->capability |= FPGA_PORT_CAP_ERR_IRQ;
	spinlock_unlock(&port->lock);

	return 0;
}

static void port_error_uinit(struct feature *feature)
{
	UNUSED(feature);
}

static int port_error_get_prop(struct feature *feature,
			       struct feature_prop *prop)
{
	struct ifpga_port_hw *port = feature->parent;

	switch (prop->prop_id) {
	case PORT_ERR_PROP_REVISION:
		return port_err_get_revision(port, &prop->data);
	case PORT_ERR_PROP_ERRORS:
		return port_err_get_errors(port, &prop->data);
	case PORT_ERR_PROP_FIRST_ERROR:
		return port_err_get_first_error(port, &prop->data);
	case PORT_ERR_PROP_FIRST_MALFORMED_REQ_LSB:
		return port_err_get_first_malformed_req_lsb(port, &prop->data);
	case PORT_ERR_PROP_FIRST_MALFORMED_REQ_MSB:
		return port_err_get_first_malformed_req_msb(port, &prop->data);
	}

	return -ENOENT;
}

static int port_error_set_prop(struct feature *feature,
			       struct feature_prop *prop)
{
	struct ifpga_port_hw *port = feature->parent;

	if (prop->prop_id == PORT_ERR_PROP_CLEAR)
		return port_err_set_clear(port, prop->data);

	return -ENOENT;
}

struct feature_ops ifpga_rawdev_port_error_ops = {
	.init = port_error_init,
	.uinit = port_error_uinit,
	.get_prop = port_error_get_prop,
	.set_prop = port_error_set_prop,
};
