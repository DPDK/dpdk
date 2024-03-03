/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#include <cnxk_rep.h>
#include <cnxk_rep_msg.h>

#define PF_SHIFT 10
#define PF_MASK	 0x3F

static uint16_t
get_pf(uint16_t hw_func)
{
	return (hw_func >> PF_SHIFT) & PF_MASK;
}

static uint16_t
switch_domain_id_allocate(struct cnxk_eswitch_dev *eswitch_dev, uint16_t pf)
{
	int i = 0;

	for (i = 0; i < eswitch_dev->nb_switch_domain; i++) {
		if (eswitch_dev->sw_dom[i].pf == pf)
			return eswitch_dev->sw_dom[i].switch_domain_id;
	}

	return RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
}

int
cnxk_rep_state_update(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func, uint16_t *rep_id)
{
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	int i, rc = 0;

	/* Delete the individual PFVF flows as common eswitch VF rule will be used. */
	rc = cnxk_eswitch_flow_rules_delete(eswitch_dev, hw_func);
	if (rc) {
		if (rc != -ENOENT) {
			plt_err("Failed to delete %x flow rules", hw_func);
			goto fail;
		}
	}
	/* Rep ID for respective HW func */
	rc = cnxk_eswitch_representor_id(eswitch_dev, hw_func, rep_id);
	if (rc) {
		if (rc != -ENOENT) {
			plt_err("Failed to get rep info for %x", hw_func);
			goto fail;
		}
	}
	/* Update the state - representee is standalone or part of companian app */
	for (i = 0; i < eswitch_dev->repr_cnt.nb_repr_probed; i++) {
		rep_eth_dev = eswitch_dev->rep_info[i].rep_eth_dev;
		if (!rep_eth_dev) {
			plt_err("Failed to get rep ethdev handle");
			rc = -EINVAL;
			goto fail;
		}

		rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
		if (rep_dev->hw_func == hw_func && rep_dev->is_vf_active)
			rep_dev->native_repte = false;
	}

	return 0;
fail:
	return rc;
}

int
cnxk_rep_dev_uninit(struct rte_eth_dev *ethdev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	plt_rep_dbg("Representor port:%d uninit", ethdev->data->port_id);
	rte_free(ethdev->data->mac_addrs);
	ethdev->data->mac_addrs = NULL;

	return 0;
}

int
cnxk_rep_dev_remove(struct cnxk_eswitch_dev *eswitch_dev)
{
	int i, rc = 0;

	for (i = 0; i < eswitch_dev->nb_switch_domain; i++) {
		rc = rte_eth_switch_domain_free(eswitch_dev->sw_dom[i].switch_domain_id);
		if (rc)
			plt_err("Failed to alloc switch domain: %d", rc);
	}

	return rc;
}

static int
cnxk_rep_parent_setup(struct cnxk_eswitch_dev *eswitch_dev)
{
	uint16_t pf, prev_pf = 0, switch_domain_id;
	int rc, i, j = 0;

	if (eswitch_dev->rep_info)
		return 0;

	eswitch_dev->rep_info =
		plt_zmalloc(sizeof(eswitch_dev->rep_info[0]) * eswitch_dev->repr_cnt.max_repr, 0);
	if (!eswitch_dev->rep_info) {
		plt_err("Failed to alloc memory for rep info");
		rc = -ENOMEM;
		goto fail;
	}

	/* Allocate switch domain for all PFs (VFs will be under same domain as PF) */
	for (i = 0; i < eswitch_dev->repr_cnt.max_repr; i++) {
		pf = get_pf(eswitch_dev->nix.rep_pfvf_map[i]);
		if (pf == prev_pf)
			continue;

		rc = rte_eth_switch_domain_alloc(&switch_domain_id);
		if (rc) {
			plt_err("Failed to alloc switch domain: %d", rc);
			goto fail;
		}
		plt_rep_dbg("Allocated switch domain id %d for pf %d\n", switch_domain_id, pf);
		eswitch_dev->sw_dom[j].switch_domain_id = switch_domain_id;
		eswitch_dev->sw_dom[j].pf = pf;
		prev_pf = pf;
		j++;
	}
	eswitch_dev->nb_switch_domain = j;

	return 0;
fail:
	return rc;
}

static uint16_t
cnxk_rep_tx_burst(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	PLT_SET_USED(tx_queue);
	PLT_SET_USED(tx_pkts);
	PLT_SET_USED(nb_pkts);

	return 0;
}

static uint16_t
cnxk_rep_rx_burst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	PLT_SET_USED(rx_queue);
	PLT_SET_USED(rx_pkts);
	PLT_SET_USED(nb_pkts);

	return 0;
}

static int
cnxk_rep_dev_init(struct rte_eth_dev *eth_dev, void *params)
{
	struct cnxk_rep_dev *rep_params = (struct cnxk_rep_dev *)params;
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);

	rep_dev->port_id = rep_params->port_id;
	rep_dev->switch_domain_id = rep_params->switch_domain_id;
	rep_dev->parent_dev = rep_params->parent_dev;
	rep_dev->hw_func = rep_params->hw_func;
	rep_dev->rep_id = rep_params->rep_id;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	eth_dev->data->representor_id = rep_params->port_id;
	eth_dev->data->backer_port_id = eth_dev->data->port_id;

	eth_dev->data->mac_addrs = plt_zmalloc(RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		plt_err("Failed to allocate memory for mac addr");
		return -ENOMEM;
	}

	rte_eth_random_addr(rep_dev->mac_addr);
	memcpy(eth_dev->data->mac_addrs, rep_dev->mac_addr, RTE_ETHER_ADDR_LEN);

	/* Set the device operations */
	eth_dev->dev_ops = &cnxk_rep_dev_ops;

	/* Rx/Tx functions stubs to avoid crashing */
	eth_dev->rx_pkt_burst = cnxk_rep_rx_burst;
	eth_dev->tx_pkt_burst = cnxk_rep_tx_burst;

	/* Only single queues for representor devices */
	eth_dev->data->nb_rx_queues = 1;
	eth_dev->data->nb_tx_queues = 1;

	eth_dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	eth_dev->data->dev_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_UP;
	eth_dev->data->dev_link.link_autoneg = RTE_ETH_LINK_FIXED;

	return 0;
}

static int
create_representor_ethdev(struct rte_pci_device *pci_dev, struct cnxk_eswitch_dev *eswitch_dev,
			  struct cnxk_eswitch_devargs *esw_da, int idx)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_dev *rep_eth_dev;
	uint16_t hw_func;
	int rc = 0;

	struct cnxk_rep_dev rep = {.port_id = eswitch_dev->repr_cnt.nb_repr_probed,
				   .parent_dev = eswitch_dev};

	if (esw_da->type == CNXK_ESW_DA_TYPE_PFVF) {
		hw_func = esw_da->repr_hw_info[idx].hw_func;
		rep.switch_domain_id = switch_domain_id_allocate(eswitch_dev, get_pf(hw_func));
		if (rep.switch_domain_id == RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
			plt_err("Failed to get a valid switch domain id");
			rc = -EINVAL;
			goto fail;
		}

		esw_da->repr_hw_info[idx].port_id = rep.port_id;
		/* Representor port net_bdf_port */
		snprintf(name, sizeof(name), "net_%s_hw_%x_representor_%d", pci_dev->device.name,
			 hw_func, rep.port_id);

		rep.hw_func = hw_func;
		rep.rep_id = esw_da->repr_hw_info[idx].rep_id;

	} else {
		snprintf(name, sizeof(name), "net_%s_representor_%d", pci_dev->device.name,
			 rep.port_id);
		rep.switch_domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	}

	rc = rte_eth_dev_create(&pci_dev->device, name, sizeof(struct cnxk_rep_dev), NULL, NULL,
				cnxk_rep_dev_init, &rep);
	if (rc) {
		plt_err("Failed to create cnxk vf representor %s", name);
		rc = -EINVAL;
		goto fail;
	}

	rep_eth_dev = rte_eth_dev_allocated(name);
	if (!rep_eth_dev) {
		plt_err("Failed to find the eth_dev for VF-Rep: %s.", name);
		rc = -ENODEV;
		goto fail;
	}

	plt_rep_dbg("Representor portid %d (%s) type %d probe done", rep_eth_dev->data->port_id,
		    name, esw_da->da.type);
	eswitch_dev->rep_info[rep.port_id].rep_eth_dev = rep_eth_dev;
	eswitch_dev->repr_cnt.nb_repr_probed++;

	return 0;
fail:
	return rc;
}

int
cnxk_rep_dev_probe(struct rte_pci_device *pci_dev, struct cnxk_eswitch_dev *eswitch_dev)
{
	struct cnxk_eswitch_devargs *esw_da;
	uint16_t num_rep;
	int i, j, rc;

	if (eswitch_dev->repr_cnt.nb_repr_created > RTE_MAX_ETHPORTS) {
		plt_err("nb_representor_ports %d > %d MAX ETHPORTS\n",
			eswitch_dev->repr_cnt.nb_repr_created, RTE_MAX_ETHPORTS);
		rc = -EINVAL;
		goto fail;
	}

	/* Initialize the internals of representor ports */
	rc = cnxk_rep_parent_setup(eswitch_dev);
	if (rc) {
		plt_err("Failed to setup the parent device, err %d", rc);
		goto fail;
	}

	for (i = eswitch_dev->last_probed; i < eswitch_dev->nb_esw_da; i++) {
		esw_da = &eswitch_dev->esw_da[i];
		/* Check the representor devargs */
		num_rep = esw_da->nb_repr_ports;
		for (j = 0; j < num_rep; j++) {
			rc = create_representor_ethdev(pci_dev, eswitch_dev, esw_da, j);
			if (rc)
				goto fail;
		}
	}
	eswitch_dev->last_probed = i;

	/* Launch a thread to handle control messages */
	if (!eswitch_dev->start_ctrl_msg_thrd) {
		rc = cnxk_rep_msg_control_thread_launch(eswitch_dev);
		if (rc) {
			plt_err("Failed to launch message ctrl thread");
			goto fail;
		}
	}

	return 0;
fail:
	return rc;
}
