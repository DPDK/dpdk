/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "hw_mod_backend.h"

int flow_api_backend_init(struct flow_api_backend_s *dev,
	const struct flow_api_backend_ops *iface,
	void *be_dev)
{
	assert(dev);
	dev->iface = iface;
	dev->be_dev = be_dev;
	dev->num_phy_ports = iface->get_nb_phy_port(be_dev);
	dev->num_rx_ports = iface->get_nb_rx_port(be_dev);
	dev->max_categories = iface->get_nb_categories(be_dev);
	dev->max_queues = iface->get_nb_queues(be_dev);

	return 0;
}

int flow_api_backend_done(struct flow_api_backend_s *dev)
{
	(void)dev;

	return 0;
}
