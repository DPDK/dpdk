/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include "opdl_evdev.h"
#include "opdl_log.h"

static const char * const port_xstat_str[] = {

	"claim_pkts_requested",
	"claim_pkts_granted",
	"claim_non_empty",
	"claim_empty",
	"total_cycles",
};


void
opdl_xstats_init(struct rte_eventdev *dev)
{
	uint32_t i, j;

	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return;

	for (i = 0; i < device->max_port_nb; i++) {
		struct opdl_port *port = &device->ports[i];

		for (j = 0; j < max_num_port_xstat; j++) {
			uint32_t index = (i * max_num_port_xstat) + j;

			/* Name */
			snprintf(device->port_xstat[index].stat.name,
				sizeof(device->port_xstat[index].stat.name),
				"port_%02u_%s", i, port_xstat_str[j]);

			/* ID */
			device->port_xstat[index].id = index;

			/* Stats ptr */
			device->port_xstat[index].value = &port->port_stat[j];
		}
	}
}

int
opdl_xstats_uninit(struct rte_eventdev *dev)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return 0;

	memset(device->port_xstat,
	       0,
	       sizeof(device->port_xstat));

	return 0;
}

int
opdl_xstats_get_names(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		unsigned int *ids, unsigned int size)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return -ENOTSUP;

	if (mode == RTE_EVENT_DEV_XSTATS_DEVICE ||
			mode == RTE_EVENT_DEV_XSTATS_QUEUE)
		return -EINVAL;

	if (queue_port_id >= device->max_port_nb)
		return -EINVAL;

	if (size < max_num_port_xstat)
		return max_num_port_xstat;

	uint32_t port_idx = queue_port_id * max_num_port_xstat;

	uint32_t j;
	for (j = 0; j < max_num_port_xstat; j++) {

		strcpy(xstats_names[j].name,
				device->port_xstat[j + port_idx].stat.name);
		ids[j] = device->port_xstat[j + port_idx].id;
	}

	return max_num_port_xstat;
}

int
opdl_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id,
		const unsigned int ids[],
		uint64_t values[], unsigned int n)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return -ENOTSUP;

	if (mode == RTE_EVENT_DEV_XSTATS_DEVICE ||
			mode == RTE_EVENT_DEV_XSTATS_QUEUE)
		return -EINVAL;

	if (queue_port_id >= device->max_port_nb)
		return -EINVAL;

	if (n > max_num_port_xstat)
		return -EINVAL;

	uint32_t p_start = queue_port_id * max_num_port_xstat;
	uint32_t p_finish = p_start + max_num_port_xstat;

	uint32_t i;
	for (i = 0; i < n; i++) {
		if (ids[i] < p_start || ids[i] >= p_finish)
			return -EINVAL;

		values[i] = *(device->port_xstat[ids[i]].value);
	}

	return n;
}

uint64_t
opdl_xstats_get_by_name(const struct rte_eventdev *dev,
		const char *name, unsigned int *id)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return -ENOTSUP;

	uint32_t max_index = device->max_port_nb * max_num_port_xstat;

	uint32_t i;
	for (i = 0; i < max_index; i++) {

		if (strncmp(name,
			   device->port_xstat[i].stat.name,
			   RTE_EVENT_DEV_XSTATS_NAME_SIZE) == 0) {
			if (id != NULL)
				*id = i;
			if (device->port_xstat[i].value)
				return *(device->port_xstat[i].value);
			break;
		}
	}
	return -EINVAL;
}

int
opdl_xstats_reset(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id, const uint32_t ids[],
		uint32_t nb_ids)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return -ENOTSUP;

	RTE_SET_USED(dev);
	RTE_SET_USED(mode);
	RTE_SET_USED(queue_port_id);
	RTE_SET_USED(ids);
	RTE_SET_USED(nb_ids);

	return -ENOTSUP;
}
