/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_eswitch.h>

#define PF_SHIFT 10
static inline int
get_hw_func(uint16_t pf, uint16_t vf)
{
	return (pf << PF_SHIFT) | vf;
}

static int
populate_repr_hw_info(struct cnxk_eswitch_dev *eswitch_dev, struct rte_eth_devargs *eth_da,
		      uint16_t idx)
{
	struct cnxk_eswitch_devargs *esw_da = &eswitch_dev->esw_da[idx];
	uint16_t nb_repr_ports, hw_func;
	int rc, i, j;

	if (eth_da->type == RTE_ETH_REPRESENTOR_NONE) {
		plt_err("No representor type found");
		return -EINVAL;
	}

	if (eth_da->type != RTE_ETH_REPRESENTOR_VF && eth_da->type != RTE_ETH_REPRESENTOR_PF &&
	    eth_da->type != RTE_ETH_REPRESENTOR_SF) {
		plt_err("unsupported representor type %d", eth_da->type);
		return -ENOTSUP;
	}

	nb_repr_ports = (eth_da->type == RTE_ETH_REPRESENTOR_PF) ? eth_da->nb_ports :
								   eth_da->nb_representor_ports;
	esw_da->nb_repr_ports = nb_repr_ports;
	/* If plain list is provided as representor pattern */
	if (eth_da->nb_ports == 0)
		return 0;

	esw_da->repr_hw_info = plt_zmalloc(nb_repr_ports * sizeof(struct cnxk_esw_repr_hw_info), 0);
	if (!esw_da->repr_hw_info) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto fail;
	}

	plt_esw_dbg("Representor param %d has %d pfvf", idx, nb_repr_ports);
	/* Check if representor can be created for PFVF and populating HW func list */
	for (i = 0; i < nb_repr_ports; i++) {
		if (eth_da->type == RTE_ETH_REPRESENTOR_PF)
			hw_func = get_hw_func(eth_da->ports[i], 0);
		else
			hw_func = get_hw_func(eth_da->ports[0], eth_da->representor_ports[i] + 1);

		for (j = 0; j < eswitch_dev->repr_cnt.max_repr; j++) {
			if (eswitch_dev->nix.rep_pfvf_map[j] == hw_func)
				break;
		}

		/* HW func which doesn not match the map table received from AF, no
		 * representor port is assigned.
		 */
		if (j == eswitch_dev->repr_cnt.max_repr) {
			plt_err("Representor port can't be created for PF%dVF%d", eth_da->ports[0],
				eth_da->representor_ports[i]);
			rc = -EINVAL;
			goto fail;
		}

		esw_da->repr_hw_info[i].hw_func = hw_func;
		esw_da->repr_hw_info[i].rep_id = j;
		esw_da->repr_hw_info[i].pfvf = (eth_da->type == RTE_ETH_REPRESENTOR_PF) ?
						       eth_da->ports[0] :
						       eth_da->representor_ports[i];
		TAILQ_INIT(&esw_da->repr_hw_info[i].repr_flow_list);
		plt_esw_dbg("	HW func %x index %d type %d", hw_func, j, eth_da->type);
	}

	esw_da->type = CNXK_ESW_DA_TYPE_PFVF;

	return 0;
fail:
	return rc;
}

int
cnxk_eswitch_repr_devargs(struct rte_pci_device *pci_dev, struct cnxk_eswitch_dev *eswitch_dev)
{
	struct rte_devargs *devargs = pci_dev->device.devargs;
	struct rte_eth_devargs eth_da[RTE_MAX_ETHPORTS];
	int rc, i, j, count;

	if (devargs == NULL) {
		plt_err("No devargs passed");
		rc = -EINVAL;
		goto fail;
	}

	/* Parse devargs passed to ESW device */
	rc = rte_eth_devargs_parse(devargs->args, eth_da, RTE_MAX_ETHPORTS);
	if (rc < 0) {
		plt_err("Failed to parse devargs, err %d", rc);
		goto fail;
	}

	count = rc;
	j = eswitch_dev->nb_esw_da;
	for (i = 0; i < count; i++) {
		rc = populate_repr_hw_info(eswitch_dev, &eth_da[i], j);
		if (rc) {
			plt_err("Failed to populate representer hw funcs, err %d", rc);
			goto fail;
		}

		rte_memcpy(&eswitch_dev->esw_da[j].da, &eth_da[i], sizeof(struct rte_eth_devargs));
		/* No of representor ports to be created */
		eswitch_dev->repr_cnt.nb_repr_created += eswitch_dev->esw_da[j].nb_repr_ports;
		j++;
	}
	eswitch_dev->nb_esw_da += count;

	return 0;
fail:
	return rc;
}
