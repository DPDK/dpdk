/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#include <cnxk_rep.h>
#include <cnxk_rep_msg.h>

#define REPTE_MSG_PROC_THRD_NAME_MAX_LEN 30

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
cnxk_rep_state_update(struct cnxk_eswitch_dev *eswitch_dev, uint32_t state, uint16_t *rep_id)
{
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	uint16_t hw_func, nb_rxq;
	int i, rc = 0;

	nb_rxq = state & 0xFFFF;
	hw_func = (state >> 16) & 0xFFFF;
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
		if (rep_dev->hw_func == hw_func && rep_dev->is_vf_active) {
			rep_dev->native_repte = false;
			rep_dev->nb_rxq = nb_rxq;
		}
	}

	return 0;
fail:
	return rc;
}

int
cnxk_rep_dev_uninit(struct rte_eth_dev *ethdev)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	plt_rep_dbg("Representor port:%d uninit", ethdev->data->port_id);
	rte_free(ethdev->data->mac_addrs);
	ethdev->data->mac_addrs = NULL;

	rep_dev->parent_dev->repr_cnt.nb_repr_probed--;

	return 0;
}

int
cnxk_rep_dev_remove(struct cnxk_eswitch_dev *eswitch_dev)
{
	int i, rc = 0;

	roc_eswitch_nix_process_repte_notify_cb_unregister(&eswitch_dev->nix);
	for (i = 0; i < eswitch_dev->nb_switch_domain; i++) {
		rc = rte_eth_switch_domain_free(eswitch_dev->sw_dom[i].switch_domain_id);
		if (rc)
			plt_err("Failed to alloc switch domain: %d", rc);
	}

	return rc;
}

static int
cnxk_representee_release(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func)
{
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	int i, rc = 0;

	for (i = 0; i < eswitch_dev->repr_cnt.nb_repr_probed; i++) {
		rep_eth_dev = eswitch_dev->rep_info[i].rep_eth_dev;
		if (!rep_eth_dev) {
			plt_err("Failed to get rep ethdev handle");
			rc = -EINVAL;
			goto done;
		}

		rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
		if (rep_dev->hw_func == hw_func &&
		    (!rep_dev->native_repte || rep_dev->is_vf_active)) {
			rep_dev->is_vf_active = false;
			rc = cnxk_rep_dev_stop(rep_eth_dev);
			if (rc) {
				plt_err("Failed to stop repr port %d, rep id %d", rep_dev->port_id,
					rep_dev->rep_id);
				goto done;
			}

			cnxk_rep_rx_queue_release(rep_eth_dev, 0);
			cnxk_rep_tx_queue_release(rep_eth_dev, 0);
			plt_rep_dbg("Released representor ID %d representing %x", rep_dev->rep_id,
				    hw_func);
			break;
		}
	}
done:
	return rc;
}

static int
cnxk_representee_setup(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func, uint16_t rep_id)
{
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	int i, rc = 0;

	for (i = 0; i < eswitch_dev->repr_cnt.nb_repr_probed; i++) {
		rep_eth_dev = eswitch_dev->rep_info[i].rep_eth_dev;
		if (!rep_eth_dev) {
			plt_err("Failed to get rep ethdev handle");
			rc = -EINVAL;
			goto done;
		}

		rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
		if (rep_dev->hw_func == hw_func && !rep_dev->is_vf_active) {
			rep_dev->is_vf_active = true;
			rep_dev->native_repte = true;
			if (rep_dev->rep_id != rep_id) {
				plt_err("Rep ID assigned during init %d does not match %d",
					rep_dev->rep_id, rep_id);
				rc = -EINVAL;
				goto done;
			}

			rc = cnxk_rep_rx_queue_setup(rep_eth_dev, rep_dev->rxq->qid,
						     rep_dev->rxq->nb_desc, 0,
						     rep_dev->rxq->rx_conf, rep_dev->rxq->mpool);
			if (rc) {
				plt_err("Failed to setup rxq repr port %d, rep id %d",
					rep_dev->port_id, rep_dev->rep_id);
				goto done;
			}

			rc = cnxk_rep_tx_queue_setup(rep_eth_dev, rep_dev->txq->qid,
						     rep_dev->txq->nb_desc, 0,
						     rep_dev->txq->tx_conf);
			if (rc) {
				plt_err("Failed to setup txq repr port %d, rep id %d",
					rep_dev->port_id, rep_dev->rep_id);
				goto done;
			}

			rc = cnxk_rep_dev_start(rep_eth_dev);
			if (rc) {
				plt_err("Failed to start repr port %d, rep id %d", rep_dev->port_id,
					rep_dev->rep_id);
				goto done;
			}
			break;
		}
	}
done:
	return rc;
}

static int
cnxk_representee_state_msg_process(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func,
				   bool enable)
{
	struct cnxk_eswitch_devargs *esw_da;
	uint16_t rep_id = UINT16_MAX;
	int rc = 0, i, j;

	/* Traversing the initialized represented list */
	for (i = 0; i < eswitch_dev->nb_esw_da; i++) {
		esw_da = &eswitch_dev->esw_da[i];
		for (j = 0; j < esw_da->nb_repr_ports; j++) {
			if (esw_da->repr_hw_info[j].hw_func == hw_func) {
				rep_id = esw_da->repr_hw_info[j].rep_id;
				break;
			}
		}
		if (rep_id != UINT16_MAX)
			break;
	}
	/* No action on PF func for which representor has not been created */
	if (rep_id == UINT16_MAX)
		goto done;

	if (enable) {
		rc = cnxk_representee_setup(eswitch_dev, hw_func, rep_id);
		if (rc) {
			plt_err("Failed to setup representee, err %d", rc);
			goto fail;
		}
		plt_rep_dbg("		Representor ID %d representing %x", rep_id, hw_func);
		rc = cnxk_eswitch_flow_rules_install(eswitch_dev, hw_func);
		if (rc) {
			plt_err("Failed to install rxtx flow rules for %x", hw_func);
			goto fail;
		}
	} else {
		rc = cnxk_eswitch_flow_rules_delete(eswitch_dev, hw_func);
		if (rc) {
			plt_err("Failed to delete flow rules for %x", hw_func);
			goto fail;
		}
		rc = cnxk_representee_release(eswitch_dev, hw_func);
		if (rc) {
			plt_err("Failed to release representee, err %d", rc);
			goto fail;
		}
	}

done:
	return 0;
fail:
	return rc;
}

static int
cnxk_representee_mtu_msg_process(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func,
				 uint16_t mtu)
{
	struct cnxk_eswitch_devargs *esw_da;
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	uint16_t rep_id = UINT16_MAX;
	int rc = 0;
	int i, j;

	/* Traversing the initialized represented list */
	for (i = 0; i < eswitch_dev->nb_esw_da; i++) {
		esw_da = &eswitch_dev->esw_da[i];
		for (j = 0; j < esw_da->nb_repr_ports; j++) {
			if (esw_da->repr_hw_info[j].hw_func == hw_func) {
				rep_id = esw_da->repr_hw_info[j].rep_id;
				break;
			}
		}
		if (rep_id != UINT16_MAX)
			break;
	}
	/* No action on PF func for which representor has not been created */
	if (rep_id == UINT16_MAX)
		goto done;

	for (i = 0; i < eswitch_dev->repr_cnt.nb_repr_probed; i++) {
		rep_eth_dev = eswitch_dev->rep_info[i].rep_eth_dev;
		if (!rep_eth_dev) {
			plt_err("Failed to get rep ethdev handle");
			rc = -EINVAL;
			goto done;
		}

		rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
		if (rep_dev->rep_id == rep_id) {
			plt_rep_dbg("Setting MTU as %d for hw_func %x rep_id %d", mtu, hw_func,
				    rep_id);
			rep_dev->repte_mtu = mtu;
			break;
		}
	}

done:
	return rc;
}

static int
cnxk_representee_msg_process(struct cnxk_eswitch_dev *eswitch_dev,
			     struct roc_eswitch_repte_notify_msg *notify_msg)
{
	int rc = 0;

	switch (notify_msg->type) {
	case ROC_ESWITCH_REPTE_STATE:
		plt_rep_dbg("	  REPTE STATE: hw_func %x action %s", notify_msg->state.hw_func,
			    notify_msg->state.enable ? "enable" : "disable");
		rc = cnxk_representee_state_msg_process(eswitch_dev, notify_msg->state.hw_func,
							notify_msg->state.enable);
		break;
	case ROC_ESWITCH_LINK_STATE:
		plt_rep_dbg("	  LINK STATE: hw_func %x action %s", notify_msg->link.hw_func,
			    notify_msg->link.enable ? "enable" : "disable");
		break;
	case ROC_ESWITCH_REPTE_MTU:
		plt_rep_dbg("	   REPTE MTU: hw_func %x rep_id %d mtu %d", notify_msg->mtu.hw_func,
			    notify_msg->mtu.rep_id, notify_msg->mtu.mtu);
		rc = cnxk_representee_mtu_msg_process(eswitch_dev, notify_msg->mtu.hw_func,
						      notify_msg->mtu.mtu);
		break;
	default:
		plt_err("Invalid notification msg received %d", notify_msg->type);
		break;
	};

	return rc;
}

static uint32_t
cnxk_representee_msg_thread_main(void *arg)
{
	struct cnxk_eswitch_dev *eswitch_dev = (struct cnxk_eswitch_dev *)arg;
	struct cnxk_esw_repte_msg_proc *repte_msg_proc;
	struct cnxk_esw_repte_msg *msg, *next_msg;
	int count, rc;

	repte_msg_proc = &eswitch_dev->repte_msg_proc;
	pthread_mutex_lock(&eswitch_dev->repte_msg_proc.mutex);
	while (eswitch_dev->repte_msg_proc.start_thread) {
		do {
			rc = pthread_cond_wait(&eswitch_dev->repte_msg_proc.repte_msg_cond,
					       &eswitch_dev->repte_msg_proc.mutex);
		} while (rc != 0);

		/* Go through list pushed from interrupt context and process each message */
		next_msg = TAILQ_FIRST(&repte_msg_proc->msg_list);
		count = 0;
		while (next_msg) {
			msg = next_msg;
			count++;
			plt_rep_dbg("	Processing msg %d: ", count);
			/* Unlocking for interrupt thread to grab lock
			 * while thread process the message.
			 */
			pthread_mutex_unlock(&eswitch_dev->repte_msg_proc.mutex);
			/* Processing the message */
			cnxk_representee_msg_process(eswitch_dev, msg->notify_msg);
			/* Locking as cond wait will unlock before wait */
			pthread_mutex_lock(&eswitch_dev->repte_msg_proc.mutex);
			next_msg = TAILQ_NEXT(msg, next);
			TAILQ_REMOVE(&repte_msg_proc->msg_list, msg, next);
			rte_free(msg->notify_msg);
			rte_free(msg);
		}
	}

	pthread_mutex_unlock(&eswitch_dev->repte_msg_proc.mutex);

	return 0;
}

static int
cnxk_representee_notification(void *roc_nix, struct roc_eswitch_repte_notify_msg *notify_msg)
{
	struct cnxk_esw_repte_msg_proc *repte_msg_proc;
	struct cnxk_eswitch_dev *eswitch_dev;
	struct cnxk_esw_repte_msg *msg;
	int rc = 0;

	RTE_SET_USED(roc_nix);
	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		plt_err("Failed to get PF ethdev handle");
		rc = -EINVAL;
		goto done;
	}

	repte_msg_proc = &eswitch_dev->repte_msg_proc;
	msg = rte_zmalloc("msg", sizeof(struct cnxk_esw_repte_msg), 0);
	if (!msg) {
		plt_err("Failed to allocate memory for repte msg");
		rc = -ENOMEM;
		goto done;
	}

	msg->notify_msg = plt_zmalloc(sizeof(struct roc_eswitch_repte_notify_msg), 0);
	if (!msg->notify_msg) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto done;
	}

	rte_memcpy(msg->notify_msg, notify_msg, sizeof(struct roc_eswitch_repte_notify_msg));
	plt_rep_dbg("Pushing new notification : msg type %d", msg->notify_msg->type);
	pthread_mutex_lock(&eswitch_dev->repte_msg_proc.mutex);
	TAILQ_INSERT_TAIL(&repte_msg_proc->msg_list, msg, next);
	/* Signal vf message handler thread */
	pthread_cond_signal(&eswitch_dev->repte_msg_proc.repte_msg_cond);
	pthread_mutex_unlock(&eswitch_dev->repte_msg_proc.mutex);

done:
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
		plt_rep_dbg("Allocated switch domain id %d for pf %d", switch_domain_id, pf);
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
	eth_dev->rx_pkt_burst = cnxk_rep_rx_burst_dummy;
	eth_dev->tx_pkt_burst = cnxk_rep_tx_burst_dummy;

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
	char name[REPTE_MSG_PROC_THRD_NAME_MAX_LEN];
	struct cnxk_eswitch_devargs *esw_da;
	uint16_t num_rep;
	int i, j, rc;

	if (eswitch_dev->repr_cnt.nb_repr_created > RTE_MAX_ETHPORTS) {
		plt_err("nb_representor_ports %d > %d MAX ETHPORTS",
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

	if (!eswitch_dev->repte_msg_proc.start_thread) {
		/* Register callback for representee notification */
		if (roc_eswitch_nix_process_repte_notify_cb_register(&eswitch_dev->nix,
							     cnxk_representee_notification)) {
			plt_err("Failed to register callback for representee notification");
			rc = -EINVAL;
			goto fail;
		}

		/* Create a thread for handling msgs from VFs */
		TAILQ_INIT(&eswitch_dev->repte_msg_proc.msg_list);
		pthread_cond_init(&eswitch_dev->repte_msg_proc.repte_msg_cond, NULL);
		pthread_mutex_init(&eswitch_dev->repte_msg_proc.mutex, NULL);

		rte_strscpy(name, "repte_msg_proc_thrd", REPTE_MSG_PROC_THRD_NAME_MAX_LEN);
		eswitch_dev->repte_msg_proc.start_thread = true;
		rc =
		rte_thread_create_internal_control(&eswitch_dev->repte_msg_proc.repte_msg_thread,
						   name, cnxk_representee_msg_thread_main,
						   eswitch_dev);
		if (rc != 0) {
			plt_err("Failed to create thread for VF mbox handling");
			goto thread_fail;
		}
	}

	return 0;
thread_fail:
	pthread_mutex_destroy(&eswitch_dev->repte_msg_proc.mutex);
	pthread_cond_destroy(&eswitch_dev->repte_msg_proc.repte_msg_cond);
fail:
	return rc;
}
