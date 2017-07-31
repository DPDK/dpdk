/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_byteorder.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_vnic.h"
#include "rte_pmd_bnxt.h"
#include "hsi_struct_def_dpdk.h"

int bnxt_rcv_msg_from_vf(struct bnxt *bp, uint16_t vf_id, void *msg)
{
	struct rte_pmd_bnxt_mb_event_param ret_param;

	ret_param.retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
	ret_param.vf_id = vf_id;
	ret_param.msg = msg;

	_rte_eth_dev_callback_process(bp->eth_dev, RTE_ETH_EVENT_VF_MBOX,
				      NULL, &ret_param);

	/* Default to approve */
	if (ret_param.retval == RTE_PMD_BNXT_MB_EVENT_PROCEED)
		ret_param.retval = RTE_PMD_BNXT_MB_EVENT_NOOP_ACK;

	return ret_param.retval == RTE_PMD_BNXT_MB_EVENT_NOOP_ACK ?
		true : false;
}

int rte_pmd_bnxt_set_tx_loopback(uint8_t port, uint8_t on)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1)
		return -EINVAL;

	eth_dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(eth_dev))
		return -ENOTSUP;

	bp = (struct bnxt *)eth_dev->data->dev_private;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set Tx loopback on non-PF port %d!\n",
			port);
		return -ENOTSUP;
	}

	if (on)
		bp->pf.evb_mode = BNXT_EVB_MODE_VEB;
	else
		bp->pf.evb_mode = BNXT_EVB_MODE_VEPA;

	rc = bnxt_hwrm_pf_evb_mode(bp);

	return rc;
}

static void
rte_pmd_bnxt_set_all_queues_drop_en_cb(struct bnxt_vnic_info *vnic, void *onptr)
{
	uint8_t *on = onptr;
	vnic->bd_stall = !(*on);
}

int rte_pmd_bnxt_set_all_queues_drop_en(uint8_t port, uint8_t on)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;
	uint32_t i;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1)
		return -EINVAL;

	eth_dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(eth_dev))
		return -ENOTSUP;

	bp = (struct bnxt *)eth_dev->data->dev_private;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set all queues drop on non-PF port!\n");
		return -ENOTSUP;
	}

	if (bp->vnic_info == NULL)
		return -ENODEV;

	/* Stall PF */
	for (i = 0; i < bp->nr_vnics; i++) {
		bp->vnic_info[i].bd_stall = !on;
		rc = bnxt_hwrm_vnic_cfg(bp, &bp->vnic_info[i]);
		if (rc) {
			RTE_LOG(ERR, PMD, "Failed to update PF VNIC %d.\n", i);
			return rc;
		}
	}

	/* Stall all active VFs */
	for (i = 0; i < bp->pf.active_vfs; i++) {
		rc = bnxt_hwrm_func_vf_vnic_query_and_config(bp, i,
				rte_pmd_bnxt_set_all_queues_drop_en_cb, &on,
				bnxt_hwrm_vnic_cfg);
		if (rc) {
			RTE_LOG(ERR, PMD, "Failed to update VF VNIC %d.\n", i);
			break;
		}
	}

	return rc;
}

int rte_pmd_bnxt_set_vf_mac_addr(uint8_t port, uint16_t vf,
				struct ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf >= dev_info.max_vfs || mac_addr == NULL)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set VF %d mac address on non-PF port %d!\n",
			vf, port);
		return -ENOTSUP;
	}

	rc = bnxt_hwrm_func_vf_mac(bp, vf, (uint8_t *)mac_addr);

	return rc;
}

int rte_pmd_bnxt_set_vf_rate_limit(uint8_t port, uint16_t vf,
				uint16_t tx_rate, uint64_t q_msk)
{
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;
	uint16_t tot_rate = 0;
	uint64_t idx;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	eth_dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(eth_dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)eth_dev->data->dev_private;

	if (!bp->pf.active_vfs)
		return -EINVAL;

	if (vf >= bp->pf.max_vfs)
		return -EINVAL;

	/* Add up the per queue BW and configure MAX BW of the VF */
	for (idx = 0; idx < 64; idx++) {
		if ((1ULL << idx) & q_msk)
			tot_rate += tx_rate;
	}

	/* Requested BW can't be greater than link speed */
	if (tot_rate > eth_dev->data->dev_link.link_speed) {
		RTE_LOG(ERR, PMD, "Rate > Link speed. Set to %d\n", tot_rate);
		return -EINVAL;
	}

	/* Requested BW already configured */
	if (tot_rate == bp->pf.vf_info[vf].max_tx_rate)
		return 0;

	rc = bnxt_hwrm_func_bw_cfg(bp, vf, tot_rate,
				HWRM_FUNC_CFG_INPUT_ENABLES_MAX_BW);

	if (!rc)
		bp->pf.vf_info[vf].max_tx_rate = tot_rate;

	return rc;
}

int rte_pmd_bnxt_set_vf_mac_anti_spoof(uint8_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;
	uint32_t func_flags;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1)
		return -EINVAL;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set mac spoof on non-PF port %d!\n", port);
		return -EINVAL;
	}

	if (vf >= dev_info.max_vfs)
		return -EINVAL;

	/* Prev setting same as new setting. */
	if (on == bp->pf.vf_info[vf].mac_spoof_en)
		return 0;

	func_flags = bp->pf.vf_info[vf].func_cfg_flags;
	func_flags &= ~(HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_ENABLE |
	    HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_DISABLE);

	if (on)
		func_flags |=
			HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_ENABLE;
	else
		func_flags |=
			HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_DISABLE;

	rc = bnxt_hwrm_func_cfg_vf_set_flags(bp, vf, func_flags);
	if (!rc) {
		bp->pf.vf_info[vf].mac_spoof_en = on;
		bp->pf.vf_info[vf].func_cfg_flags = func_flags;
	}

	return rc;
}

int rte_pmd_bnxt_set_vf_vlan_anti_spoof(uint8_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1)
		return -EINVAL;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set VLAN spoof on non-PF port %d!\n", port);
		return -EINVAL;
	}

	if (vf >= dev_info.max_vfs)
		return -EINVAL;

	if (on == bp->pf.vf_info[vf].vlan_spoof_en)
		return 0;

	rc = bnxt_hwrm_func_cfg_vf_set_vlan_anti_spoof(bp, vf, on);
	if (!rc) {
		bp->pf.vf_info[vf].vlan_spoof_en = on;
		if (on) {
			if (bnxt_hwrm_cfa_vlan_antispoof_cfg(bp,
				bp->pf.first_vf_id + vf,
				bp->pf.vf_info[vf].vlan_count,
				bp->pf.vf_info[vf].vlan_as_table))
				rc = -1;
		}
	} else {
		RTE_LOG(ERR, PMD, "Failed to update VF VNIC %d.\n", vf);
	}

	return rc;
}

static void
rte_pmd_bnxt_set_vf_vlan_stripq_cb(struct bnxt_vnic_info *vnic, void *onptr)
{
	uint8_t *on = onptr;
	vnic->vlan_strip = *on;
}

int
rte_pmd_bnxt_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set VF %d stripq on non-PF port %d!\n",
			vf, port);
		return -ENOTSUP;
	}

	rc = bnxt_hwrm_func_vf_vnic_query_and_config(bp, vf,
				rte_pmd_bnxt_set_vf_vlan_stripq_cb, &on,
				bnxt_hwrm_vnic_cfg);
	if (rc)
		RTE_LOG(ERR, PMD, "Failed to update VF VNIC %d.\n", vf);

	return rc;
}

int rte_pmd_bnxt_set_vf_rxmode(uint8_t port, uint16_t vf,
				uint16_t rx_mask, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	uint16_t flag = 0;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (!bp->pf.vf_info)
		return -EINVAL;

	if (vf >= bp->pdev->max_vfs)
		return -EINVAL;

	if (rx_mask & (ETH_VMDQ_ACCEPT_UNTAG | ETH_VMDQ_ACCEPT_HASH_MC)) {
		RTE_LOG(ERR, PMD, "Currently cannot toggle this setting\n");
		return -ENOTSUP;
	}

	if (rx_mask & ETH_VMDQ_ACCEPT_HASH_UC && !on) {
		RTE_LOG(ERR, PMD, "Currently cannot disable UC Rx\n");
		return -ENOTSUP;
	}

	if (rx_mask & ETH_VMDQ_ACCEPT_BROADCAST)
		flag |= BNXT_VNIC_INFO_BCAST;
	if (rx_mask & ETH_VMDQ_ACCEPT_MULTICAST)
		flag |= BNXT_VNIC_INFO_ALLMULTI;

	if (on)
		bp->pf.vf_info[vf].l2_rx_mask |= flag;
	else
		bp->pf.vf_info[vf].l2_rx_mask &= ~flag;

	rc = bnxt_hwrm_func_vf_vnic_query_and_config(bp, vf,
					vf_vnic_set_rxmask_cb,
					&bp->pf.vf_info[vf].l2_rx_mask,
					bnxt_set_rx_mask_no_vlan);
	if (rc)
		RTE_LOG(ERR, PMD, "bnxt_hwrm_func_vf_vnic_set_rxmask failed\n");

	return rc;
}

static int bnxt_set_vf_table(struct bnxt *bp, uint16_t vf)
{
	int rc = 0;
	int dflt_vnic;
	struct bnxt_vnic_info vnic;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set VLAN table on non-PF port!\n");
		return -EINVAL;
	}

	if (vf >= bp->pdev->max_vfs)
		return -EINVAL;

	dflt_vnic = bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(bp, vf);
	if (dflt_vnic < 0) {
		/* This simply indicates there's no driver loaded.
		 * This is not an error.
		 */
		RTE_LOG(ERR, PMD, "Unable to get default VNIC for VF %d\n", vf);
	} else {
		memset(&vnic, 0, sizeof(vnic));
		vnic.fw_vnic_id = dflt_vnic;
		if (bnxt_hwrm_vnic_qcfg(bp, &vnic,
					bp->pf.first_vf_id + vf) == 0) {
			if (bnxt_hwrm_cfa_l2_set_rx_mask(bp, &vnic,
						bp->pf.vf_info[vf].vlan_count,
						bp->pf.vf_info[vf].vlan_table))
				rc = -1;
		} else {
			rc = -1;
		}
	}

	return rc;
}

int rte_pmd_bnxt_set_vf_vlan_filter(uint8_t port, uint16_t vlan,
				    uint64_t vf_mask, uint8_t vlan_on)
{
	struct bnxt_vlan_table_entry *ve;
	struct bnxt_vlan_antispoof_table_entry *vase;
	struct rte_eth_dev *dev;
	struct bnxt *bp;
	uint16_t cnt;
	int rc = 0;
	int i, j;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	bp = (struct bnxt *)dev->data->dev_private;
	if (!bp->pf.vf_info)
		return -EINVAL;

	for (i = 0; vf_mask; i++, vf_mask >>= 1) {
		cnt = bp->pf.vf_info[i].vlan_count;
		if ((vf_mask & 1)  == 0)
			continue;

		if (bp->pf.vf_info[i].vlan_table == NULL) {
			rc = -1;
			continue;
		}
		if (bp->pf.vf_info[i].vlan_as_table == NULL) {
			rc = -1;
			continue;
		}
		if (vlan_on) {
			/* First, search for a duplicate... */
			for (j = 0; j < cnt; j++) {
				if (rte_be_to_cpu_16(
				   bp->pf.vf_info[i].vlan_table[j].vid) == vlan)
					break;
			}
			if (j == cnt) {
				/* Now check that there's space */
				if (cnt == getpagesize() / sizeof(struct
				    bnxt_vlan_antispoof_table_entry)) {
					RTE_LOG(ERR, PMD,
					     "VLAN anti-spoof table is full\n");
					RTE_LOG(ERR, PMD,
						"VF %d cannot add VLAN %u\n",
						i, vlan);
					rc = -1;
					continue;
				}

				/* cnt is one less than vlan_count */
				cnt = bp->pf.vf_info[i].vlan_count++;
				/*
				 * And finally, add to the
				 * end of the table
				 */
				vase = &bp->pf.vf_info[i].vlan_as_table[cnt];
				// TODO: Hardcoded TPID
				vase->tpid = rte_cpu_to_be_16(0x8100);
				vase->vid = rte_cpu_to_be_16(vlan);
				vase->mask = rte_cpu_to_be_16(0xfff);
				ve = &bp->pf.vf_info[i].vlan_table[cnt];
				/* TODO: Hardcoded TPID */
				ve->tpid = rte_cpu_to_be_16(0x8100);
				ve->vid = rte_cpu_to_be_16(vlan);
			}
		} else {
			for (j = 0; j < cnt; j++) {
				if (rte_be_to_cpu_16(
				   bp->pf.vf_info[i].vlan_table[j].vid) != vlan)
					continue;
				memmove(&bp->pf.vf_info[i].vlan_table[j],
					&bp->pf.vf_info[i].vlan_table[j + 1],
					getpagesize() - ((j + 1) *
					sizeof(struct bnxt_vlan_table_entry)));
				memmove(&bp->pf.vf_info[i].vlan_as_table[j],
					&bp->pf.vf_info[i].vlan_as_table[j + 1],
					getpagesize() - ((j + 1) * sizeof(struct
					bnxt_vlan_antispoof_table_entry)));
				j--;
				cnt = --bp->pf.vf_info[i].vlan_count;
			}
		}
		bnxt_set_vf_table(bp, i);
	}

	return rc;
}

int rte_pmd_bnxt_get_vf_stats(uint8_t port,
			      uint16_t vf_id,
			      struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf_id >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to get VF %d stats on non-PF port %d!\n",
			vf_id, port);
		return -ENOTSUP;
	}

	return bnxt_hwrm_func_qstats(bp, bp->pf.first_vf_id + vf_id, stats);
}

int rte_pmd_bnxt_reset_vf_stats(uint8_t port,
				uint16_t vf_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf_id >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to reset VF %d stats on non-PF port %d!\n",
			vf_id, port);
		return -ENOTSUP;
	}

	return bnxt_hwrm_func_clr_stats(bp, bp->pf.first_vf_id + vf_id);
}

int rte_pmd_bnxt_get_vf_rx_status(uint8_t port, uint16_t vf_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf_id >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to query VF %d RX stats on non-PF port %d!\n",
			vf_id, port);
		return -ENOTSUP;
	}

	return bnxt_vf_vnic_count(bp, vf_id);
}

int rte_pmd_bnxt_get_vf_tx_drop_count(uint8_t port, uint16_t vf_id,
				      uint64_t *count)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf_id >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to query VF %d TX drops on non-PF port %d!\n",
			vf_id, port);
		return -ENOTSUP;
	}

	return bnxt_hwrm_func_qstats_tx_drop(bp, bp->pf.first_vf_id + vf_id,
					     count);
}

int rte_pmd_bnxt_mac_addr_add(uint8_t port, struct ether_addr *addr,
				uint32_t vf_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;
	struct bnxt_filter_info *filter;
	struct bnxt_vnic_info vnic;
	struct ether_addr dflt_mac;
	int rc;

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf_id >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to config VF %d MAC on non-PF port %d!\n",
			vf_id, port);
		return -ENOTSUP;
	}

	/* If the VF currently uses a random MAC, update default to this one */
	if (bp->pf.vf_info[vf_id].random_mac) {
		if (rte_pmd_bnxt_get_vf_rx_status(port, vf_id) <= 0)
			rc = bnxt_hwrm_func_vf_mac(bp, vf_id, (uint8_t *)addr);
	}

	/* query the default VNIC id used by the function */
	rc = bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(bp, vf_id);
	if (rc < 0)
		goto exit;

	memset(&vnic, 0, sizeof(struct bnxt_vnic_info));
	vnic.fw_vnic_id = rte_le_to_cpu_16(rc);
	rc = bnxt_hwrm_vnic_qcfg(bp, &vnic, bp->pf.first_vf_id + vf_id);
	if (rc < 0)
		goto exit;

	STAILQ_FOREACH(filter, &bp->pf.vf_info[vf_id].filter, next) {
		if (filter->flags ==
		    HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX &&
		    filter->enables ==
		    (HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
		     HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK) &&
		    memcmp(addr, filter->l2_addr, ETHER_ADDR_LEN) == 0) {
			bnxt_hwrm_clear_filter(bp, filter);
			break;
		}
	}

	if (filter == NULL)
		filter = bnxt_alloc_vf_filter(bp, vf_id);

	filter->fw_l2_filter_id = UINT64_MAX;
	filter->flags = HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
	filter->enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK;
	memcpy(filter->l2_addr, addr, ETHER_ADDR_LEN);
	memset(filter->l2_addr_mask, 0xff, ETHER_ADDR_LEN);

	/* Do not add a filter for the default MAC */
	if (bnxt_hwrm_func_qcfg_vf_default_mac(bp, vf_id, &dflt_mac) ||
	    memcmp(filter->l2_addr, dflt_mac.addr_bytes, ETHER_ADDR_LEN))
		rc = bnxt_hwrm_set_filter(bp, vnic.fw_vnic_id, filter);

exit:
	return rc;
}

int
rte_pmd_bnxt_set_vf_vlan_insert(uint8_t port, uint16_t vf,
		uint16_t vlan_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev))
		return -ENOTSUP;

	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (vf >= dev_info.max_vfs)
		return -EINVAL;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set VF %d vlan insert on non-PF port %d!\n",
			vf, port);
		return -ENOTSUP;
	}

	bp->pf.vf_info[vf].dflt_vlan = vlan_id;
	if (bnxt_hwrm_func_qcfg_current_vf_vlan(bp, vf) ==
	    bp->pf.vf_info[vf].dflt_vlan)
		return 0;

	rc = bnxt_hwrm_set_vf_vlan(bp, vf);

	return rc;
}

int rte_pmd_bnxt_set_vf_persist_stats(uint8_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;
	uint32_t func_flags;
	struct bnxt *bp;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1)
		return -EINVAL;

	dev = &rte_eth_devices[port];
	rte_eth_dev_info_get(port, &dev_info);
	bp = (struct bnxt *)dev->data->dev_private;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD,
			"Attempt to set persist stats on non-PF port %d!\n",
			port);
		return -EINVAL;
	}

	if (vf >= dev_info.max_vfs)
		return -EINVAL;

	/* Prev setting same as new setting. */
	if (on == bp->pf.vf_info[vf].persist_stats)
		return 0;

	func_flags = bp->pf.vf_info[vf].func_cfg_flags;

	if (on)
		func_flags |=
			HWRM_FUNC_CFG_INPUT_FLAGS_NO_AUTOCLEAR_STATISTIC;
	else
		func_flags &=
			~HWRM_FUNC_CFG_INPUT_FLAGS_NO_AUTOCLEAR_STATISTIC;

	rc = bnxt_hwrm_func_cfg_vf_set_flags(bp, vf, func_flags);
	if (!rc) {
		bp->pf.vf_info[vf].persist_stats = on;
		bp->pf.vf_info[vf].func_cfg_flags = func_flags;
	}

	return rc;
}
