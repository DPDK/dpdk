/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

int
softnic_link_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->link_list);

	return 0;
}

void
softnic_link_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_link *link;

		link = TAILQ_FIRST(&p->link_list);
		if (link == NULL)
			break;

		TAILQ_REMOVE(&p->link_list, link, node);
		free(link);
	}
}

struct softnic_link *
softnic_link_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_link *link;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(link, &p->link_list, node)
		if (strcmp(link->name, name) == 0)
			return link;

	return NULL;
}

struct softnic_link *
softnic_link_create(struct pmd_internals *p,
	const char *name,
	struct softnic_link_params *params)
{
	struct rte_eth_dev_info port_info;
	struct softnic_link *link;
	uint16_t port_id;
	int ret;

	/* Check input params */
	if (name == NULL ||
		softnic_link_find(p, name) ||
		params == NULL)
		return NULL;

	port_id = params->port_id;
	if (params->dev_name) {
		int status;

		status = rte_eth_dev_get_port_by_name(params->dev_name,
			&port_id);

		if (status)
			return NULL;
	} else {
		if (!rte_eth_dev_is_valid_port(port_id))
			return NULL;
	}

	ret = rte_eth_dev_info_get(port_id, &port_info);
	if (ret != 0)
		return NULL;

	/* Node allocation */
	link = calloc(1, sizeof(struct softnic_link));
	if (link == NULL)
		return NULL;

	/* Node fill in */
	strlcpy(link->name, name, sizeof(link->name));
	link->port_id = port_id;
	link->n_rxq = port_info.nb_rx_queues;
	link->n_txq = port_info.nb_tx_queues;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->link_list, link, node);

	return link;
}
