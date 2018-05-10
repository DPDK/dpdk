/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "cxgbe.h"

/*
 * Figure out how many Ports and Queue Sets we can support.  This depends on
 * knowing our Virtual Function Resources and may be called a second time if
 * we fall back from MSI-X to MSI Interrupt Mode.
 */
static void size_nports_qsets(struct adapter *adapter)
{
	struct vf_resources *vfres = &adapter->params.vfres;
	unsigned int ethqsets, pmask_nports;

	/*
	 * The number of "ports" which we support is equal to the number of
	 * Virtual Interfaces with which we've been provisioned.
	 */
	adapter->params.nports = vfres->nvi;
	if (adapter->params.nports > MAX_NPORTS) {
		dev_warn(adapter->pdev_dev, "only using %d of %d maximum"
			 " allowed virtual interfaces\n", MAX_NPORTS,
			 adapter->params.nports);
		adapter->params.nports = MAX_NPORTS;
	}

	/*
	 * We may have been provisioned with more VIs than the number of
	 * ports we're allowed to access (our Port Access Rights Mask).
	 * This is obviously a configuration conflict but we don't want to
	 * do anything silly just because of that.
	 */
	pmask_nports = hweight32(adapter->params.vfres.pmask);
	if (pmask_nports < adapter->params.nports) {
		dev_warn(adapter->pdev_dev, "only using %d of %d provissioned"
			 " virtual interfaces; limited by Port Access Rights"
			 " mask %#x\n", pmask_nports, adapter->params.nports,
			 adapter->params.vfres.pmask);
		adapter->params.nports = pmask_nports;
	}

	/*
	 * We need to reserve an Ingress Queue for the Asynchronous Firmware
	 * Event Queue.
	 *
	 * For each Queue Set, we'll need the ability to allocate two Egress
	 * Contexts -- one for the Ingress Queue Free List and one for the TX
	 * Ethernet Queue.
	 */
	ethqsets = vfres->niqflint - 1;
	if (vfres->nethctrl != ethqsets)
		ethqsets = min(vfres->nethctrl, ethqsets);
	if (vfres->neq < ethqsets * 2)
		ethqsets = vfres->neq / 2;
	if (ethqsets > MAX_ETH_QSETS)
		ethqsets = MAX_ETH_QSETS;
	adapter->sge.max_ethqsets = ethqsets;

	if (adapter->sge.max_ethqsets < adapter->params.nports) {
		dev_warn(adapter->pdev_dev, "only using %d of %d available"
			 " virtual interfaces (too few Queue Sets)\n",
			 adapter->sge.max_ethqsets, adapter->params.nports);
		adapter->params.nports = adapter->sge.max_ethqsets;
	}
}

void cxgbevf_stats_get(struct port_info *pi, struct port_stats *stats)
{
	t4vf_get_port_stats(pi->adapter, pi->pidx, stats);
}

static int adap_init0vf(struct adapter *adapter)
{
	u32 param, val = 0;
	int err;

	err = t4vf_fw_reset(adapter);
	if (err < 0) {
		dev_err(adapter->pdev_dev, "FW reset failed: err=%d\n", err);
		return err;
	}

	/*
	 * Grab basic operational parameters.  These will predominantly have
	 * been set up by the Physical Function Driver or will be hard coded
	 * into the adapter.  We just have to live with them ...  Note that
	 * we _must_ get our VPD parameters before our SGE parameters because
	 * we need to know the adapter's core clock from the VPD in order to
	 * properly decode the SGE Timer Values.
	 */
	err = t4vf_get_dev_params(adapter);
	if (err) {
		dev_err(adapter->pdev_dev, "unable to retrieve adapter"
			" device parameters: err=%d\n", err);
		return err;
	}

	err = t4vf_get_vpd_params(adapter);
	if (err) {
		dev_err(adapter->pdev_dev, "unable to retrieve adapter"
			" VPD parameters: err=%d\n", err);
		return err;
	}

	adapter->pf = t4vf_get_pf_from_vf(adapter);
	err = t4vf_sge_init(adapter);
	if (err) {
		dev_err(adapter->pdev_dev, "error in sge init\n");
		return err;
	}

	err = t4vf_get_rss_glb_config(adapter);
	if (err) {
		dev_err(adapter->pdev_dev, "unable to retrieve adapter"
			" RSS parameters: err=%d\n", err);
		return err;
	}
	if (adapter->params.rss.mode !=
	    FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL) {
		dev_err(adapter->pdev_dev, "unable to operate with global RSS"
			" mode %d\n", adapter->params.rss.mode);
		return -EINVAL;
	}

	/* If we're running on newer firmware, let it know that we're
	 * prepared to deal with encapsulated CPL messages.  Older
	 * firmware won't understand this and we'll just get
	 * unencapsulated messages ...
	 */
	param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) |
		V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_CPLFW4MSG_ENCAP);
	val = 1;
	t4vf_set_params(adapter, 1, &param, &val);

	/*
	 * Grab our Virtual Interface resource allocation, extract the
	 * features that we're interested in and do a bit of sanity testing on
	 * what we discover.
	 */
	err = t4vf_get_vfres(adapter);
	if (err) {
		dev_err(adapter->pdev_dev, "unable to get virtual interface"
			" resources: err=%d\n", err);
		return err;
	}

	/*
	 * Check for various parameter sanity issues.
	 */
	if (adapter->params.vfres.pmask == 0) {
		dev_err(adapter->pdev_dev, "no port access configured\n"
			"usable!\n");
		return -EINVAL;
	}
	if (adapter->params.vfres.nvi == 0) {
		dev_err(adapter->pdev_dev, "no virtual interfaces configured/"
			"usable!\n");
		return -EINVAL;
	}

	/*
	 * Initialize nports and max_ethqsets now that we have our Virtual
	 * Function Resources.
	 */
	size_nports_qsets(adapter);
	adapter->flags |= FW_OK;
	return 0;
}

int cxgbevf_probe(struct adapter *adapter)
{
	struct port_info *pi;
	unsigned int pmask;
	int err = 0;
	int i;

	t4_os_lock_init(&adapter->mbox_lock);
	TAILQ_INIT(&adapter->mbox_list);
	err = t4vf_prep_adapter(adapter);
	if (err)
		return err;

	if (!is_t4(adapter->params.chip)) {
		adapter->bar2 = (void *)adapter->pdev->mem_resource[2].addr;
		if (!adapter->bar2) {
			dev_err(adapter, "cannot map device bar2 region\n");
			err = -ENOMEM;
			return err;
		}
	}

	err = adap_init0vf(adapter);
	if (err) {
		dev_err(adapter, "%s: Adapter initialization failed, error %d\n",
				__func__, err);
		goto out_free;
	}

	pmask = adapter->params.vfres.pmask;
	for_each_port(adapter, i) {
		const unsigned int numa_node = rte_socket_id();
		char name[RTE_ETH_NAME_MAX_LEN];
		struct rte_eth_dev *eth_dev;
		int port_id;

		if (pmask == 0)
			break;
		port_id = ffs(pmask) - 1;
		pmask &= ~(1 << port_id);

		snprintf(name, sizeof(name), "%s_%d",
			 adapter->pdev->device.name, i);

		if (i == 0) {
			/* First port is already allocated by DPDK */
			eth_dev = adapter->eth_dev;
			goto allocate_mac;
		}

		/*
		 * now do all data allocation - for eth_dev structure,
		 * and internal (private) data for the remaining ports
		 */

		/* reserve an ethdev entry */
		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev) {
			err = -ENOMEM;
			goto out_free;
		}
		eth_dev->data->dev_private =
			rte_zmalloc_socket(name, sizeof(struct port_info),
					   RTE_CACHE_LINE_SIZE, numa_node);
		if (!eth_dev->data->dev_private)
			goto out_free;

allocate_mac:
		pi = (struct port_info *)eth_dev->data->dev_private;
		adapter->port[i] = pi;
		pi->eth_dev = eth_dev;
		pi->adapter = adapter;
		pi->xact_addr_filt = -1;
		pi->port_id = port_id;
		pi->pidx = i;

		pi->eth_dev->device = &adapter->pdev->device;
		pi->eth_dev->dev_ops = adapter->eth_dev->dev_ops;
		pi->eth_dev->tx_pkt_burst = adapter->eth_dev->tx_pkt_burst;
		pi->eth_dev->rx_pkt_burst = adapter->eth_dev->rx_pkt_burst;

		rte_eth_copy_pci_info(pi->eth_dev, adapter->pdev);
		pi->eth_dev->data->mac_addrs = rte_zmalloc(name,
							   ETHER_ADDR_LEN, 0);
		if (!pi->eth_dev->data->mac_addrs) {
			dev_err(adapter, "%s: Mem allocation failed for storing mac addr, aborting\n",
				__func__);
			err = -ENOMEM;
			goto out_free;
		}

		if (i > 0) {
			/* First port will be notified by upper layer */
			rte_eth_dev_probing_finish(eth_dev);
		}
	}

	if (adapter->flags & FW_OK) {
		err = t4vf_port_init(adapter);
		if (err) {
			dev_err(adapter, "%s: t4_port_init failed with err %d\n",
				__func__, err);
			goto out_free;
		}
	}

	cfg_queues(adapter->eth_dev);
	print_adapter_info(adapter);
	print_port_info(adapter);

	err = init_rss(adapter);
	if (err)
		goto out_free;
	return 0;

out_free:
	for_each_port(adapter, i) {
		pi = adap2pinfo(adapter, i);
		if (pi->viid != 0)
			t4_free_vi(adapter, adapter->mbox, adapter->pf,
				   0, pi->viid);
		/* Skip first port since it'll be de-allocated by DPDK */
		if (i == 0)
			continue;
		if (pi->eth_dev) {
			if (pi->eth_dev->data->dev_private)
				rte_free(pi->eth_dev->data->dev_private);
			rte_eth_dev_release_port(pi->eth_dev);
		}
	}
	return -err;
}
