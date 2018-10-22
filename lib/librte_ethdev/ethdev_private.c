/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include "rte_ethdev.h"
#include "rte_ethdev_driver.h"
#include "ethdev_private.h"

uint16_t
eth_dev_to_id(const struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return RTE_MAX_ETHPORTS;
	return dev - rte_eth_devices;
}

struct rte_eth_dev *
eth_find_device(const struct rte_eth_dev *start, rte_eth_cmp_t cmp,
		const void *data)
{
	struct rte_eth_dev *edev;
	ptrdiff_t idx;

	/* Avoid Undefined Behaviour */
	if (start != NULL &&
	    (start < &rte_eth_devices[0] ||
	     start > &rte_eth_devices[RTE_MAX_ETHPORTS]))
		return NULL;
	if (start != NULL)
		idx = eth_dev_to_id(start) + 1;
	else
		idx = 0;
	for (; idx < RTE_MAX_ETHPORTS; idx++) {
		edev = &rte_eth_devices[idx];
		if (cmp(edev, data) == 0)
			return edev;
	}
	return NULL;
}

int
rte_eth_devargs_parse_list(char *str, rte_eth_devargs_callback_t callback,
	void *data)
{
	char *str_start;
	int state;
	int result;

	if (*str != '[')
		/* Single element, not a list */
		return callback(str, data);

	/* Sanity check, then strip the brackets */
	str_start = &str[strlen(str) - 1];
	if (*str_start != ']') {
		RTE_LOG(ERR, EAL, "(%s): List does not end with ']'\n", str);
		return -EINVAL;
	}
	str++;
	*str_start = '\0';

	/* Process list elements */
	state = 0;
	while (1) {
		if (state == 0) {
			if (*str == '\0')
				break;
			if (*str != ',') {
				str_start = str;
				state = 1;
			}
		} else if (state == 1) {
			if (*str == ',' || *str == '\0') {
				if (str > str_start) {
					/* Non-empty string fragment */
					*str = '\0';
					result = callback(str_start, data);
					if (result < 0)
						return result;
				}
				state = 0;
			}
		}
		str++;
	}
	return 0;
}

static int
rte_eth_devargs_process_range(char *str, uint16_t *list, uint16_t *len_list,
	const uint16_t max_list)
{
	uint16_t lo, hi, val;
	int result;

	result = sscanf(str, "%hu-%hu", &lo, &hi);
	if (result == 1) {
		if (*len_list >= max_list)
			return -ENOMEM;
		list[(*len_list)++] = lo;
	} else if (result == 2) {
		if (lo >= hi || lo > RTE_MAX_ETHPORTS || hi > RTE_MAX_ETHPORTS)
			return -EINVAL;
		for (val = lo; val <= hi; val++) {
			if (*len_list >= max_list)
				return -ENOMEM;
			list[(*len_list)++] = val;
		}
	} else
		return -EINVAL;
	return 0;
}

int
rte_eth_devargs_parse_representor_ports(char *str, void *data)
{
	struct rte_eth_devargs *eth_da = data;

	return rte_eth_devargs_process_range(str, eth_da->representor_ports,
		&eth_da->nb_representor_ports, RTE_MAX_ETHPORTS);
}
