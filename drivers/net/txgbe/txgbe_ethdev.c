/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rte_common.h>
#include <ethdev_pci.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_kvargs.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"
#include "txgbe_regs_group.h"

static const struct reg_info txgbe_regs_general[] = {
	{TXGBE_RST, 1, 1, "TXGBE_RST"},
	{TXGBE_STAT, 1, 1, "TXGBE_STAT"},
	{TXGBE_PORTCTL, 1, 1, "TXGBE_PORTCTL"},
	{TXGBE_SDP, 1, 1, "TXGBE_SDP"},
	{TXGBE_SDPCTL, 1, 1, "TXGBE_SDPCTL"},
	{TXGBE_LEDCTL, 1, 1, "TXGBE_LEDCTL"},
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_nvm[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_interrupt[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_fctl_others[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_rxdma[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_rx[] = {
	{0, 0, 0, ""}
};

static struct reg_info txgbe_regs_tx[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_wakeup[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_dcb[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_mac[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbe_regs_diagnostic[] = {
	{0, 0, 0, ""},
};

/* PF registers */
static const struct reg_info *txgbe_regs_others[] = {
				txgbe_regs_general,
				txgbe_regs_nvm,
				txgbe_regs_interrupt,
				txgbe_regs_fctl_others,
				txgbe_regs_rxdma,
				txgbe_regs_rx,
				txgbe_regs_tx,
				txgbe_regs_wakeup,
				txgbe_regs_dcb,
				txgbe_regs_mac,
				txgbe_regs_diagnostic,
				NULL};

static int txgbe_fdir_filter_init(struct rte_eth_dev *eth_dev);
static int txgbe_fdir_filter_uninit(struct rte_eth_dev *eth_dev);
static int txgbe_l2_tn_filter_init(struct rte_eth_dev *eth_dev);
static int txgbe_l2_tn_filter_uninit(struct rte_eth_dev *eth_dev);
static int  txgbe_dev_set_link_up(struct rte_eth_dev *dev);
static int  txgbe_dev_set_link_down(struct rte_eth_dev *dev);
static int txgbe_dev_close(struct rte_eth_dev *dev);
static int txgbe_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static int txgbe_dev_stats_reset(struct rte_eth_dev *dev);
static void txgbe_vlan_hw_strip_enable(struct rte_eth_dev *dev, uint16_t queue);
static void txgbe_vlan_hw_strip_disable(struct rte_eth_dev *dev,
					uint16_t queue);

static void txgbe_dev_link_status_print(struct rte_eth_dev *dev);
static int txgbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on);
static int txgbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev);
static int txgbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev);
static int txgbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev);
static int txgbe_dev_interrupt_get_status(struct rte_eth_dev *dev,
				      struct rte_intr_handle *handle);
static int txgbe_dev_interrupt_action(struct rte_eth_dev *dev,
				      struct rte_intr_handle *handle);
static void txgbe_dev_interrupt_handler(void *param);
static void txgbe_dev_interrupt_delayed_handler(void *param);
static void txgbe_configure_msix(struct rte_eth_dev *dev);

static int txgbe_filter_restore(struct rte_eth_dev *dev);
static void txgbe_l2_tunnel_conf(struct rte_eth_dev *dev);

#define TXGBE_SET_HWSTRIP(h, q) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(h)->bitmap[idx] |= 1 << bit;\
	} while (0)

#define TXGBE_CLEAR_HWSTRIP(h, q) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(h)->bitmap[idx] &= ~(1 << bit);\
	} while (0)

#define TXGBE_GET_HWSTRIP(h, q, r) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(r) = (h)->bitmap[idx] >> bit & 1;\
	} while (0)

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_txgbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_SP1000) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_WX1820) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_TXD_ALIGN,
	.nb_seg_max = TXGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = TXGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops txgbe_eth_dev_ops;

#define HW_XSTAT(m) {#m, offsetof(struct txgbe_hw_stats, m)}
#define HW_XSTAT_NAME(m, n) {n, offsetof(struct txgbe_hw_stats, m)}
static const struct rte_txgbe_xstats_name_off rte_txgbe_stats_strings[] = {
	/* MNG RxTx */
	HW_XSTAT(mng_bmc2host_packets),
	HW_XSTAT(mng_host2bmc_packets),
	/* Basic RxTx */
	HW_XSTAT(rx_packets),
	HW_XSTAT(tx_packets),
	HW_XSTAT(rx_bytes),
	HW_XSTAT(tx_bytes),
	HW_XSTAT(rx_total_bytes),
	HW_XSTAT(rx_total_packets),
	HW_XSTAT(tx_total_packets),
	HW_XSTAT(rx_total_missed_packets),
	HW_XSTAT(rx_broadcast_packets),
	HW_XSTAT(rx_multicast_packets),
	HW_XSTAT(rx_management_packets),
	HW_XSTAT(tx_management_packets),
	HW_XSTAT(rx_management_dropped),

	/* Basic Error */
	HW_XSTAT(rx_crc_errors),
	HW_XSTAT(rx_illegal_byte_errors),
	HW_XSTAT(rx_error_bytes),
	HW_XSTAT(rx_mac_short_packet_dropped),
	HW_XSTAT(rx_length_errors),
	HW_XSTAT(rx_undersize_errors),
	HW_XSTAT(rx_fragment_errors),
	HW_XSTAT(rx_oversize_errors),
	HW_XSTAT(rx_jabber_errors),
	HW_XSTAT(rx_l3_l4_xsum_error),
	HW_XSTAT(mac_local_errors),
	HW_XSTAT(mac_remote_errors),

	/* Flow Director */
	HW_XSTAT(flow_director_added_filters),
	HW_XSTAT(flow_director_removed_filters),
	HW_XSTAT(flow_director_filter_add_errors),
	HW_XSTAT(flow_director_filter_remove_errors),
	HW_XSTAT(flow_director_matched_filters),
	HW_XSTAT(flow_director_missed_filters),

	/* FCoE */
	HW_XSTAT(rx_fcoe_crc_errors),
	HW_XSTAT(rx_fcoe_mbuf_allocation_errors),
	HW_XSTAT(rx_fcoe_dropped),
	HW_XSTAT(rx_fcoe_packets),
	HW_XSTAT(tx_fcoe_packets),
	HW_XSTAT(rx_fcoe_bytes),
	HW_XSTAT(tx_fcoe_bytes),
	HW_XSTAT(rx_fcoe_no_ddp),
	HW_XSTAT(rx_fcoe_no_ddp_ext_buff),

	/* MACSEC */
	HW_XSTAT(tx_macsec_pkts_untagged),
	HW_XSTAT(tx_macsec_pkts_encrypted),
	HW_XSTAT(tx_macsec_pkts_protected),
	HW_XSTAT(tx_macsec_octets_encrypted),
	HW_XSTAT(tx_macsec_octets_protected),
	HW_XSTAT(rx_macsec_pkts_untagged),
	HW_XSTAT(rx_macsec_pkts_badtag),
	HW_XSTAT(rx_macsec_pkts_nosci),
	HW_XSTAT(rx_macsec_pkts_unknownsci),
	HW_XSTAT(rx_macsec_octets_decrypted),
	HW_XSTAT(rx_macsec_octets_validated),
	HW_XSTAT(rx_macsec_sc_pkts_unchecked),
	HW_XSTAT(rx_macsec_sc_pkts_delayed),
	HW_XSTAT(rx_macsec_sc_pkts_late),
	HW_XSTAT(rx_macsec_sa_pkts_ok),
	HW_XSTAT(rx_macsec_sa_pkts_invalid),
	HW_XSTAT(rx_macsec_sa_pkts_notvalid),
	HW_XSTAT(rx_macsec_sa_pkts_unusedsa),
	HW_XSTAT(rx_macsec_sa_pkts_notusingsa),

	/* MAC RxTx */
	HW_XSTAT(rx_size_64_packets),
	HW_XSTAT(rx_size_65_to_127_packets),
	HW_XSTAT(rx_size_128_to_255_packets),
	HW_XSTAT(rx_size_256_to_511_packets),
	HW_XSTAT(rx_size_512_to_1023_packets),
	HW_XSTAT(rx_size_1024_to_max_packets),
	HW_XSTAT(tx_size_64_packets),
	HW_XSTAT(tx_size_65_to_127_packets),
	HW_XSTAT(tx_size_128_to_255_packets),
	HW_XSTAT(tx_size_256_to_511_packets),
	HW_XSTAT(tx_size_512_to_1023_packets),
	HW_XSTAT(tx_size_1024_to_max_packets),

	/* Flow Control */
	HW_XSTAT(tx_xon_packets),
	HW_XSTAT(rx_xon_packets),
	HW_XSTAT(tx_xoff_packets),
	HW_XSTAT(rx_xoff_packets),

	HW_XSTAT_NAME(tx_xon_packets, "tx_flow_control_xon_packets"),
	HW_XSTAT_NAME(rx_xon_packets, "rx_flow_control_xon_packets"),
	HW_XSTAT_NAME(tx_xoff_packets, "tx_flow_control_xoff_packets"),
	HW_XSTAT_NAME(rx_xoff_packets, "rx_flow_control_xoff_packets"),
};

#define TXGBE_NB_HW_STATS (sizeof(rte_txgbe_stats_strings) / \
			   sizeof(rte_txgbe_stats_strings[0]))

/* Per-priority statistics */
#define UP_XSTAT(m) {#m, offsetof(struct txgbe_hw_stats, up[0].m)}
static const struct rte_txgbe_xstats_name_off rte_txgbe_up_strings[] = {
	UP_XSTAT(rx_up_packets),
	UP_XSTAT(tx_up_packets),
	UP_XSTAT(rx_up_bytes),
	UP_XSTAT(tx_up_bytes),
	UP_XSTAT(rx_up_drop_packets),

	UP_XSTAT(tx_up_xon_packets),
	UP_XSTAT(rx_up_xon_packets),
	UP_XSTAT(tx_up_xoff_packets),
	UP_XSTAT(rx_up_xoff_packets),
	UP_XSTAT(rx_up_dropped),
	UP_XSTAT(rx_up_mbuf_alloc_errors),
	UP_XSTAT(tx_up_xon2off_packets),
};

#define TXGBE_NB_UP_STATS (sizeof(rte_txgbe_up_strings) / \
			   sizeof(rte_txgbe_up_strings[0]))

/* Per-queue statistics */
#define QP_XSTAT(m) {#m, offsetof(struct txgbe_hw_stats, qp[0].m)}
static const struct rte_txgbe_xstats_name_off rte_txgbe_qp_strings[] = {
	QP_XSTAT(rx_qp_packets),
	QP_XSTAT(tx_qp_packets),
	QP_XSTAT(rx_qp_bytes),
	QP_XSTAT(tx_qp_bytes),
	QP_XSTAT(rx_qp_mc_packets),
};

#define TXGBE_NB_QP_STATS (sizeof(rte_txgbe_qp_strings) / \
			   sizeof(rte_txgbe_qp_strings[0]))

static inline int
txgbe_is_sfp(struct txgbe_hw *hw)
{
	switch (hw->phy.type) {
	case txgbe_phy_sfp_avago:
	case txgbe_phy_sfp_ftl:
	case txgbe_phy_sfp_intel:
	case txgbe_phy_sfp_unknown:
	case txgbe_phy_sfp_tyco_passive:
	case txgbe_phy_sfp_unknown_passive:
		return 1;
	default:
		return 0;
	}
}

static inline int32_t
txgbe_pf_reset_hw(struct txgbe_hw *hw)
{
	uint32_t ctrl_ext;
	int32_t status;

	status = hw->mac.reset_hw(hw);

	ctrl_ext = rd32(hw, TXGBE_PORTCTL);
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= TXGBE_PORTCTL_RSTDONE;
	wr32(hw, TXGBE_PORTCTL, ctrl_ext);
	txgbe_flush(hw);

	if (status == TXGBE_ERR_SFP_NOT_PRESENT)
		status = 0;
	return status;
}

static inline void
txgbe_enable_intr(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	wr32(hw, TXGBE_IENMISC, intr->mask_misc);
	wr32(hw, TXGBE_IMC(0), TXGBE_IMC_MASK);
	wr32(hw, TXGBE_IMC(1), TXGBE_IMC_MASK);
	txgbe_flush(hw);
}

static void
txgbe_disable_intr(struct txgbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32(hw, TXGBE_IENMISC, ~BIT_MASK32);
	wr32(hw, TXGBE_IMS(0), TXGBE_IMC_MASK);
	wr32(hw, TXGBE_IMS(1), TXGBE_IMC_MASK);
	txgbe_flush(hw);
}

static int
txgbe_dev_queue_stats_mapping_set(struct rte_eth_dev *eth_dev,
				  uint16_t queue_id,
				  uint8_t stat_idx,
				  uint8_t is_rx)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_stat_mappings *stat_mappings =
		TXGBE_DEV_STAT_MAPPINGS(eth_dev);
	uint32_t qsmr_mask = 0;
	uint32_t clearing_mask = QMAP_FIELD_RESERVED_BITS_MASK;
	uint32_t q_map;
	uint8_t n, offset;

	if (hw->mac.type != txgbe_mac_raptor)
		return -ENOSYS;

	if (stat_idx & ~QMAP_FIELD_RESERVED_BITS_MASK)
		return -EIO;

	PMD_INIT_LOG(DEBUG, "Setting port %d, %s queue_id %d to stat index %d",
		     (int)(eth_dev->data->port_id), is_rx ? "RX" : "TX",
		     queue_id, stat_idx);

	n = (uint8_t)(queue_id / NB_QMAP_FIELDS_PER_QSM_REG);
	if (n >= TXGBE_NB_STAT_MAPPING) {
		PMD_INIT_LOG(ERR, "Nb of stat mapping registers exceeded");
		return -EIO;
	}
	offset = (uint8_t)(queue_id % NB_QMAP_FIELDS_PER_QSM_REG);

	/* Now clear any previous stat_idx set */
	clearing_mask <<= (QSM_REG_NB_BITS_PER_QMAP_FIELD * offset);
	if (!is_rx)
		stat_mappings->tqsm[n] &= ~clearing_mask;
	else
		stat_mappings->rqsm[n] &= ~clearing_mask;

	q_map = (uint32_t)stat_idx;
	q_map &= QMAP_FIELD_RESERVED_BITS_MASK;
	qsmr_mask = q_map << (QSM_REG_NB_BITS_PER_QMAP_FIELD * offset);
	if (!is_rx)
		stat_mappings->tqsm[n] |= qsmr_mask;
	else
		stat_mappings->rqsm[n] |= qsmr_mask;

	PMD_INIT_LOG(DEBUG, "Set port %d, %s queue_id %d to stat index %d",
		     (int)(eth_dev->data->port_id), is_rx ? "RX" : "TX",
		     queue_id, stat_idx);
	PMD_INIT_LOG(DEBUG, "%s[%d] = 0x%08x", is_rx ? "RQSMR" : "TQSM", n,
		     is_rx ? stat_mappings->rqsm[n] : stat_mappings->tqsm[n]);
	return 0;
}

static void
txgbe_dcb_init(struct txgbe_hw *hw, struct txgbe_dcb_config *dcb_config)
{
	int i;
	u8 bwgp;
	struct txgbe_dcb_tc_config *tc;

	UNREFERENCED_PARAMETER(hw);

	dcb_config->num_tcs.pg_tcs = TXGBE_DCB_TC_MAX;
	dcb_config->num_tcs.pfc_tcs = TXGBE_DCB_TC_MAX;
	bwgp = (u8)(100 / TXGBE_DCB_TC_MAX);
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		tc = &dcb_config->tc_config[i];
		tc->path[TXGBE_DCB_TX_CONFIG].bwg_id = i;
		tc->path[TXGBE_DCB_TX_CONFIG].bwg_percent = bwgp + (i & 1);
		tc->path[TXGBE_DCB_RX_CONFIG].bwg_id = i;
		tc->path[TXGBE_DCB_RX_CONFIG].bwg_percent = bwgp + (i & 1);
		tc->pfc = txgbe_dcb_pfc_disabled;
	}

	/* Initialize default user to priority mapping, UPx->TC0 */
	tc = &dcb_config->tc_config[0];
	tc->path[TXGBE_DCB_TX_CONFIG].up_to_tc_bitmap = 0xFF;
	tc->path[TXGBE_DCB_RX_CONFIG].up_to_tc_bitmap = 0xFF;
	for (i = 0; i < TXGBE_DCB_BWG_MAX; i++) {
		dcb_config->bw_percentage[i][TXGBE_DCB_TX_CONFIG] = 100;
		dcb_config->bw_percentage[i][TXGBE_DCB_RX_CONFIG] = 100;
	}
	dcb_config->rx_pba_cfg = txgbe_dcb_pba_equal;
	dcb_config->pfc_mode_enable = false;
	dcb_config->vt_mode = true;
	dcb_config->round_robin_enable = false;
	/* support all DCB capabilities */
	dcb_config->support.capabilities = 0xFF;
}

/*
 * Ensure that all locks are released before first NVM or PHY access
 */
static void
txgbe_swfw_lock_reset(struct txgbe_hw *hw)
{
	uint16_t mask;

	/*
	 * These ones are more tricky since they are common to all ports; but
	 * swfw_sync retries last long enough (1s) to be almost sure that if
	 * lock can not be taken it is due to an improper lock of the
	 * semaphore.
	 */
	mask = TXGBE_MNGSEM_SWPHY |
	       TXGBE_MNGSEM_SWMBX |
	       TXGBE_MNGSEM_SWFLASH;
	if (hw->mac.acquire_swfw_sync(hw, mask) < 0)
		PMD_DRV_LOG(DEBUG, "SWFW common locks released");

	hw->mac.release_swfw_sync(hw, mask);
}

static int
txgbe_handle_devarg(__rte_unused const char *key, const char *value,
		  void *extra_args)
{
	uint16_t *n = extra_args;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	*n = (uint16_t)strtoul(value, NULL, 10);
	if (*n == USHRT_MAX && errno == ERANGE)
		return -1;

	return 0;
}

static void
txgbe_parse_devargs(struct txgbe_hw *hw, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	u16 auto_neg = 1;
	u16 poll = 0;
	u16 present = 0;
	u16 sgmii = 0;
	u16 ffe_set = 0;
	u16 ffe_main = 27;
	u16 ffe_pre = 8;
	u16 ffe_post = 44;

	if (devargs == NULL)
		goto null;

	kvlist = rte_kvargs_parse(devargs->args, txgbe_valid_arguments);
	if (kvlist == NULL)
		goto null;

	rte_kvargs_process(kvlist, TXGBE_DEVARG_BP_AUTO,
			   &txgbe_handle_devarg, &auto_neg);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_KR_POLL,
			   &txgbe_handle_devarg, &poll);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_KR_PRESENT,
			   &txgbe_handle_devarg, &present);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_KX_SGMII,
			   &txgbe_handle_devarg, &sgmii);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_FFE_SET,
			   &txgbe_handle_devarg, &ffe_set);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_FFE_MAIN,
			   &txgbe_handle_devarg, &ffe_main);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_FFE_PRE,
			   &txgbe_handle_devarg, &ffe_pre);
	rte_kvargs_process(kvlist, TXGBE_DEVARG_FFE_POST,
			   &txgbe_handle_devarg, &ffe_post);
	rte_kvargs_free(kvlist);

null:
	hw->devarg.auto_neg = auto_neg;
	hw->devarg.poll = poll;
	hw->devarg.present = present;
	hw->devarg.sgmii = sgmii;
	hw->phy.ffe_set = ffe_set;
	hw->phy.ffe_main = ffe_main;
	hw->phy.ffe_pre = ffe_pre;
	hw->phy.ffe_post = ffe_post;
}

static int
eth_txgbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(eth_dev);
	struct txgbe_hwstrip *hwstrip = TXGBE_DEV_HWSTRIP(eth_dev);
	struct txgbe_dcb_config *dcb_config = TXGBE_DEV_DCB_CONFIG(eth_dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(eth_dev);
	struct txgbe_bw_conf *bw_conf = TXGBE_DEV_BW_CONF(eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	const struct rte_memzone *mz;
	uint32_t ctrl_ext;
	uint16_t csum;
	int err, i, ret;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &txgbe_eth_dev_ops;
	eth_dev->rx_queue_count       = txgbe_dev_rx_queue_count;
	eth_dev->rx_descriptor_status = txgbe_dev_rx_descriptor_status;
	eth_dev->tx_descriptor_status = txgbe_dev_tx_descriptor_status;
	eth_dev->rx_pkt_burst = &txgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &txgbe_xmit_pkts;
	eth_dev->tx_pkt_prepare = &txgbe_prep_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX and TX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		struct txgbe_tx_queue *txq;
		/* TX queue function in primary, set by last queue initialized
		 * Tx queue may not initialized by primary process
		 */
		if (eth_dev->data->tx_queues) {
			uint16_t nb_tx_queues = eth_dev->data->nb_tx_queues;
			txq = eth_dev->data->tx_queues[nb_tx_queues - 1];
			txgbe_set_tx_function(eth_dev, txq);
		} else {
			/* Use default TX function if we get here */
			PMD_INIT_LOG(NOTICE, "No TX queues configured yet. "
				     "Using default TX function.");
		}

		txgbe_set_rx_function(eth_dev);

		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->allow_unsupported_sfp = 1;

	/* Reserve memory for interrupt status block */
	mz = rte_eth_dma_zone_reserve(eth_dev, "txgbe_driver", -1,
		16, TXGBE_ALIGN, SOCKET_ID_ANY);
	if (mz == NULL)
		return -ENOMEM;

	hw->isb_dma = TMZ_PADDR(mz);
	hw->isb_mem = TMZ_VADDR(mz);

	txgbe_parse_devargs(hw, pci_dev->device.devargs);
	/* Initialize the shared code (base driver) */
	err = txgbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", err);
		return -EIO;
	}

	/* Unlock any pending hardware semaphore */
	txgbe_swfw_lock_reset(hw);

#ifdef RTE_LIB_SECURITY
	/* Initialize security_ctx only for primary process*/
	if (txgbe_ipsec_ctx_create(eth_dev))
		return -ENOMEM;
#endif

	/* Initialize DCB configuration*/
	memset(dcb_config, 0, sizeof(struct txgbe_dcb_config));
	txgbe_dcb_init(hw, dcb_config);

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = txgbe_fc_full;
	hw->fc.current_mode = txgbe_fc_full;
	hw->fc.pause_time = TXGBE_FC_PAUSE_TIME;
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		hw->fc.low_water[i] = TXGBE_FC_XON_LOTH;
		hw->fc.high_water[i] = TXGBE_FC_XOFF_HITH;
	}
	hw->fc.send_xon = 1;

	err = hw->rom.init_params(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM init failed: %d", err);
		return -EIO;
	}

	/* Make sure we have a good EEPROM before we read from it */
	err = hw->rom.validate_checksum(hw, &csum);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM checksum is not valid: %d", err);
		return -EIO;
	}

	err = hw->mac.init_hw(hw);

	/*
	 * Devices with copper phys will fail to initialise if txgbe_init_hw()
	 * is called too soon after the kernel driver unbinding/binding occurs.
	 * The failure occurs in txgbe_identify_phy() for all devices,
	 * but for non-copper devies, txgbe_identify_sfp_module() is
	 * also called. See txgbe_identify_phy(). The reason for the
	 * failure is not known, and only occuts when virtualisation features
	 * are disabled in the bios. A delay of 200ms  was found to be enough by
	 * trial-and-error, and is doubled to be safe.
	 */
	if (err && hw->phy.media_type == txgbe_media_type_copper) {
		rte_delay_ms(200);
		err = hw->mac.init_hw(hw);
	}

	if (err == TXGBE_ERR_SFP_NOT_PRESENT)
		err = 0;

	if (err == TXGBE_ERR_EEPROM_VERSION) {
		PMD_INIT_LOG(ERR, "This device is a pre-production adapter/"
			     "LOM.  Please be aware there may be issues associated "
			     "with your hardware.");
		PMD_INIT_LOG(ERR, "If you are experiencing problems "
			     "please contact your hardware representative "
			     "who provided you with this hardware.");
	} else if (err == TXGBE_ERR_SFP_NOT_SUPPORTED) {
		PMD_INIT_LOG(ERR, "Unsupported SFP+ Module");
	}
	if (err) {
		PMD_INIT_LOG(ERR, "Hardware Initialization Failure: %d", err);
		return -EIO;
	}

	/* Reset the hw statistics */
	txgbe_dev_stats_reset(eth_dev);

	/* disable interrupt */
	txgbe_disable_intr(hw);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("txgbe", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store "
			     "MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* Allocate memory for storing hash filter MAC addresses */
	eth_dev->data->hash_mac_addrs = rte_zmalloc("txgbe",
			RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC, 0);
	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC);
		return -ENOMEM;
	}

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	/* initialize the hw strip bitmap*/
	memset(hwstrip, 0, sizeof(*hwstrip));

	/* initialize PF if max_vfs not zero */
	ret = txgbe_pf_host_init(eth_dev);
	if (ret) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		rte_free(eth_dev->data->hash_mac_addrs);
		eth_dev->data->hash_mac_addrs = NULL;
		return ret;
	}

	ctrl_ext = rd32(hw, TXGBE_PORTCTL);
	/* let hardware know driver is loaded */
	ctrl_ext |= TXGBE_PORTCTL_DRVLOAD;
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= TXGBE_PORTCTL_RSTDONE;
	wr32(hw, TXGBE_PORTCTL, ctrl_ext);
	txgbe_flush(hw);

	if (txgbe_is_sfp(hw) && hw->phy.sfp_type != txgbe_sfp_type_not_present)
		PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d, SFP+: %d",
			     (int)hw->mac.type, (int)hw->phy.type,
			     (int)hw->phy.sfp_type);
	else
		PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d",
			     (int)hw->mac.type, (int)hw->phy.type);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	rte_intr_callback_register(intr_handle,
				   txgbe_dev_interrupt_handler, eth_dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* enable support intr */
	txgbe_enable_intr(eth_dev);

	/* initialize filter info */
	memset(filter_info, 0,
	       sizeof(struct txgbe_filter_info));

	/* initialize 5tuple filter list */
	TAILQ_INIT(&filter_info->fivetuple_list);

	/* initialize flow director filter list & hash */
	txgbe_fdir_filter_init(eth_dev);

	/* initialize l2 tunnel filter list & hash */
	txgbe_l2_tn_filter_init(eth_dev);

	/* initialize flow filter lists */
	txgbe_filterlist_init();

	/* initialize bandwidth configuration info */
	memset(bw_conf, 0, sizeof(struct txgbe_bw_conf));

	/* initialize Traffic Manager configuration */
	txgbe_tm_conf_init(eth_dev);

	return 0;
}

static int
eth_txgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	txgbe_dev_close(eth_dev);

	return 0;
}

static int txgbe_ntuple_filter_uninit(struct rte_eth_dev *eth_dev)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(eth_dev);
	struct txgbe_5tuple_filter *p_5tuple;

	while ((p_5tuple = TAILQ_FIRST(&filter_info->fivetuple_list))) {
		TAILQ_REMOVE(&filter_info->fivetuple_list,
			     p_5tuple,
			     entries);
		rte_free(p_5tuple);
	}
	memset(filter_info->fivetuple_mask, 0,
	       sizeof(uint32_t) * TXGBE_5TUPLE_ARRAY_SIZE);

	return 0;
}

static int txgbe_fdir_filter_uninit(struct rte_eth_dev *eth_dev)
{
	struct txgbe_hw_fdir_info *fdir_info = TXGBE_DEV_FDIR(eth_dev);
	struct txgbe_fdir_filter *fdir_filter;

	rte_free(fdir_info->hash_map);
	rte_hash_free(fdir_info->hash_handle);

	while ((fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list))) {
		TAILQ_REMOVE(&fdir_info->fdir_list,
			     fdir_filter,
			     entries);
		rte_free(fdir_filter);
	}

	return 0;
}

static int txgbe_l2_tn_filter_uninit(struct rte_eth_dev *eth_dev)
{
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(eth_dev);
	struct txgbe_l2_tn_filter *l2_tn_filter;

	rte_free(l2_tn_info->hash_map);
	rte_hash_free(l2_tn_info->hash_handle);

	while ((l2_tn_filter = TAILQ_FIRST(&l2_tn_info->l2_tn_list))) {
		TAILQ_REMOVE(&l2_tn_info->l2_tn_list,
			     l2_tn_filter,
			     entries);
		rte_free(l2_tn_filter);
	}

	return 0;
}

static int txgbe_fdir_filter_init(struct rte_eth_dev *eth_dev)
{
	struct txgbe_hw_fdir_info *fdir_info = TXGBE_DEV_FDIR(eth_dev);
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = TXGBE_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct txgbe_atr_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	TAILQ_INIT(&fdir_info->fdir_list);
	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE,
		 "fdir_%s", TDEV_NAME(eth_dev));
	fdir_info->hash_handle = rte_hash_create(&fdir_hash_params);
	if (!fdir_info->hash_handle) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table!");
		return -EINVAL;
	}
	fdir_info->hash_map = rte_zmalloc("txgbe",
					  sizeof(struct txgbe_fdir_filter *) *
					  TXGBE_MAX_FDIR_FILTER_NUM,
					  0);
	if (!fdir_info->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir hash map!");
		return -ENOMEM;
	}
	fdir_info->mask_added = FALSE;

	return 0;
}

static int txgbe_l2_tn_filter_init(struct rte_eth_dev *eth_dev)
{
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(eth_dev);
	char l2_tn_hash_name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters l2_tn_hash_params = {
		.name = l2_tn_hash_name,
		.entries = TXGBE_MAX_L2_TN_FILTER_NUM,
		.key_len = sizeof(struct txgbe_l2_tn_key),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	TAILQ_INIT(&l2_tn_info->l2_tn_list);
	snprintf(l2_tn_hash_name, RTE_HASH_NAMESIZE,
		 "l2_tn_%s", TDEV_NAME(eth_dev));
	l2_tn_info->hash_handle = rte_hash_create(&l2_tn_hash_params);
	if (!l2_tn_info->hash_handle) {
		PMD_INIT_LOG(ERR, "Failed to create L2 TN hash table!");
		return -EINVAL;
	}
	l2_tn_info->hash_map = rte_zmalloc("txgbe",
				   sizeof(struct txgbe_l2_tn_filter *) *
				   TXGBE_MAX_L2_TN_FILTER_NUM,
				   0);
	if (!l2_tn_info->hash_map) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate memory for L2 TN hash map!");
		return -ENOMEM;
	}
	l2_tn_info->e_tag_en = FALSE;
	l2_tn_info->e_tag_fwd_en = FALSE;
	l2_tn_info->e_tag_ether_type = RTE_ETHER_TYPE_ETAG;

	return 0;
}

static int
eth_txgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
			sizeof(struct txgbe_adapter),
			eth_dev_pci_specific_init, pci_dev,
			eth_txgbe_dev_init, NULL);
}

static int eth_txgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!ethdev)
		return 0;

	return rte_eth_dev_destroy(ethdev, eth_txgbe_dev_uninit);
}

static struct rte_pci_driver rte_txgbe_pmd = {
	.id_table = pci_id_txgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = eth_txgbe_pci_probe,
	.remove = eth_txgbe_pci_remove,
};

static int
txgbe_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(dev);
	uint32_t vfta;
	uint32_t vid_idx;
	uint32_t vid_bit;

	vid_idx = (uint32_t)((vlan_id >> 5) & 0x7F);
	vid_bit = (uint32_t)(1 << (vlan_id & 0x1F));
	vfta = rd32(hw, TXGBE_VLANTBL(vid_idx));
	if (on)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	wr32(hw, TXGBE_VLANTBL(vid_idx), vfta);

	/* update local VFTA copy */
	shadow_vfta->vfta[vid_idx] = vfta;

	return 0;
}

static void
txgbe_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_rx_queue *rxq;
	bool restart;
	uint32_t rxcfg, rxbal, rxbah;

	if (on)
		txgbe_vlan_hw_strip_enable(dev, queue);
	else
		txgbe_vlan_hw_strip_disable(dev, queue);

	rxq = dev->data->rx_queues[queue];
	rxbal = rd32(hw, TXGBE_RXBAL(rxq->reg_idx));
	rxbah = rd32(hw, TXGBE_RXBAH(rxq->reg_idx));
	rxcfg = rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) {
		restart = (rxcfg & TXGBE_RXCFG_ENA) &&
			!(rxcfg & TXGBE_RXCFG_VLAN);
		rxcfg |= TXGBE_RXCFG_VLAN;
	} else {
		restart = (rxcfg & TXGBE_RXCFG_ENA) &&
			(rxcfg & TXGBE_RXCFG_VLAN);
		rxcfg &= ~TXGBE_RXCFG_VLAN;
	}
	rxcfg &= ~TXGBE_RXCFG_ENA;

	if (restart) {
		/* set vlan strip for ring */
		txgbe_dev_rx_queue_stop(dev, queue);
		wr32(hw, TXGBE_RXBAL(rxq->reg_idx), rxbal);
		wr32(hw, TXGBE_RXBAH(rxq->reg_idx), rxbah);
		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), rxcfg);
		txgbe_dev_rx_queue_start(dev, queue);
	}
}

static int
txgbe_vlan_tpid_set(struct rte_eth_dev *dev,
		    enum rte_vlan_type vlan_type,
		    uint16_t tpid)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret = 0;
	uint32_t portctrl, vlan_ext, qinq;

	portctrl = rd32(hw, TXGBE_PORTCTL);

	vlan_ext = (portctrl & TXGBE_PORTCTL_VLANEXT);
	qinq = vlan_ext && (portctrl & TXGBE_PORTCTL_QINQ);
	switch (vlan_type) {
	case RTE_ETH_VLAN_TYPE_INNER:
		if (vlan_ext) {
			wr32m(hw, TXGBE_VLANCTL,
				TXGBE_VLANCTL_TPID_MASK,
				TXGBE_VLANCTL_TPID(tpid));
			wr32m(hw, TXGBE_DMATXCTRL,
				TXGBE_DMATXCTRL_TPID_MASK,
				TXGBE_DMATXCTRL_TPID(tpid));
		} else {
			ret = -ENOTSUP;
			PMD_DRV_LOG(ERR, "Inner type is not supported"
				    " by single VLAN");
		}

		if (qinq) {
			wr32m(hw, TXGBE_TAGTPID(0),
				TXGBE_TAGTPID_LSB_MASK,
				TXGBE_TAGTPID_LSB(tpid));
		}
		break;
	case RTE_ETH_VLAN_TYPE_OUTER:
		if (vlan_ext) {
			/* Only the high 16-bits is valid */
			wr32m(hw, TXGBE_EXTAG,
				TXGBE_EXTAG_VLAN_MASK,
				TXGBE_EXTAG_VLAN(tpid));
		} else {
			wr32m(hw, TXGBE_VLANCTL,
				TXGBE_VLANCTL_TPID_MASK,
				TXGBE_VLANCTL_TPID(tpid));
			wr32m(hw, TXGBE_DMATXCTRL,
				TXGBE_DMATXCTRL_TPID_MASK,
				TXGBE_DMATXCTRL_TPID(tpid));
		}

		if (qinq) {
			wr32m(hw, TXGBE_TAGTPID(0),
				TXGBE_TAGTPID_MSB_MASK,
				TXGBE_TAGTPID_MSB(tpid));
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported VLAN type %d", vlan_type);
		return -EINVAL;
	}

	return ret;
}

void
txgbe_vlan_hw_filter_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t vlnctrl;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Disable */
	vlnctrl = rd32(hw, TXGBE_VLANCTL);
	vlnctrl &= ~TXGBE_VLANCTL_VFE;
	wr32(hw, TXGBE_VLANCTL, vlnctrl);
}

void
txgbe_vlan_hw_filter_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(dev);
	uint32_t vlnctrl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Enable */
	vlnctrl = rd32(hw, TXGBE_VLANCTL);
	vlnctrl &= ~TXGBE_VLANCTL_CFIENA;
	vlnctrl |= TXGBE_VLANCTL_VFE;
	wr32(hw, TXGBE_VLANCTL, vlnctrl);

	/* write whatever is in local vfta copy */
	for (i = 0; i < TXGBE_VFTA_SIZE; i++)
		wr32(hw, TXGBE_VLANTBL(i), shadow_vfta->vfta[i]);
}

void
txgbe_vlan_hw_strip_bitmap_set(struct rte_eth_dev *dev, uint16_t queue, bool on)
{
	struct txgbe_hwstrip *hwstrip = TXGBE_DEV_HWSTRIP(dev);
	struct txgbe_rx_queue *rxq;

	if (queue >= TXGBE_MAX_RX_QUEUE_NUM)
		return;

	if (on)
		TXGBE_SET_HWSTRIP(hwstrip, queue);
	else
		TXGBE_CLEAR_HWSTRIP(hwstrip, queue);

	if (queue >= dev->data->nb_rx_queues)
		return;

	rxq = dev->data->rx_queues[queue];

	if (on) {
		rxq->vlan_flags = RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		rxq->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	} else {
		rxq->vlan_flags = RTE_MBUF_F_RX_VLAN;
		rxq->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}
}

static void
txgbe_vlan_hw_strip_disable(struct rte_eth_dev *dev, uint16_t queue)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, TXGBE_RXCFG(queue));
	ctrl &= ~TXGBE_RXCFG_VLAN;
	wr32(hw, TXGBE_RXCFG(queue), ctrl);

	/* record those setting for HW strip per queue */
	txgbe_vlan_hw_strip_bitmap_set(dev, queue, 0);
}

static void
txgbe_vlan_hw_strip_enable(struct rte_eth_dev *dev, uint16_t queue)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, TXGBE_RXCFG(queue));
	ctrl |= TXGBE_RXCFG_VLAN;
	wr32(hw, TXGBE_RXCFG(queue), ctrl);

	/* record those setting for HW strip per queue */
	txgbe_vlan_hw_strip_bitmap_set(dev, queue, 1);
}

static void
txgbe_vlan_hw_extend_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, TXGBE_PORTCTL);
	ctrl &= ~TXGBE_PORTCTL_VLANEXT;
	wr32(hw, TXGBE_PORTCTL, ctrl);
}

static void
txgbe_vlan_hw_extend_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl  = rd32(hw, TXGBE_PORTCTL);
	ctrl |= TXGBE_PORTCTL_VLANEXT;
	wr32(hw, TXGBE_PORTCTL, ctrl);
}

static void
txgbe_qinq_hw_strip_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, TXGBE_PORTCTL);
	ctrl &= ~TXGBE_PORTCTL_QINQ;
	wr32(hw, TXGBE_PORTCTL, ctrl);
}

static void
txgbe_qinq_hw_strip_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl  = rd32(hw, TXGBE_PORTCTL);
	ctrl |= TXGBE_PORTCTL_QINQ | TXGBE_PORTCTL_VLANEXT;
	wr32(hw, TXGBE_PORTCTL, ctrl);
}

void
txgbe_vlan_hw_strip_config(struct rte_eth_dev *dev)
{
	struct txgbe_rx_queue *rxq;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			txgbe_vlan_strip_queue_set(dev, i, 1);
		else
			txgbe_vlan_strip_queue_set(dev, i, 0);
	}
}

void
txgbe_config_vlan_strip_on_all_queues(struct rte_eth_dev *dev, int mask)
{
	uint16_t i;
	struct rte_eth_rxmode *rxmode;
	struct txgbe_rx_queue *rxq;

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		rxmode = &dev->data->dev_conf.rxmode;
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				rxq->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
			}
		else
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				rxq->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
			}
	}
}

static int
txgbe_vlan_offload_config(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode;
	rxmode = &dev->data->dev_conf.rxmode;

	if (mask & RTE_ETH_VLAN_STRIP_MASK)
		txgbe_vlan_hw_strip_config(dev);

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			txgbe_vlan_hw_filter_enable(dev);
		else
			txgbe_vlan_hw_filter_disable(dev);
	}

	if (mask & RTE_ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
			txgbe_vlan_hw_extend_enable(dev);
		else
			txgbe_vlan_hw_extend_disable(dev);
	}

	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP)
			txgbe_qinq_hw_strip_enable(dev);
		else
			txgbe_qinq_hw_strip_disable(dev);
	}

	return 0;
}

static int
txgbe_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	txgbe_config_vlan_strip_on_all_queues(dev, mask);

	txgbe_vlan_offload_config(dev, mask);

	return 0;
}

static void
txgbe_vmdq_vlan_hw_filter_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	/* VLNCTL: enable vlan filtering and allow all vlan tags through */
	uint32_t vlanctrl = rd32(hw, TXGBE_VLANCTL);

	vlanctrl |= TXGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, TXGBE_VLANCTL, vlanctrl);
}

static int
txgbe_check_vf_rss_rxq_num(struct rte_eth_dev *dev, uint16_t nb_rx_q)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	switch (nb_rx_q) {
	case 1:
	case 2:
		RTE_ETH_DEV_SRIOV(dev).active = RTE_ETH_64_POOLS;
		break;
	case 4:
		RTE_ETH_DEV_SRIOV(dev).active = RTE_ETH_32_POOLS;
		break;
	default:
		return -EINVAL;
	}

	RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool =
		TXGBE_MAX_RX_QUEUE_NUM / RTE_ETH_DEV_SRIOV(dev).active;
	RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx =
		pci_dev->max_vfs * RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;
	return 0;
}

static int
txgbe_check_mq_mode(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;

	if (RTE_ETH_DEV_SRIOV(dev).active != 0) {
		/* check multi-queue mode */
		switch (dev_conf->rxmode.mq_mode) {
		case RTE_ETH_MQ_RX_VMDQ_DCB:
			PMD_INIT_LOG(INFO, "RTE_ETH_MQ_RX_VMDQ_DCB mode supported in SRIOV");
			break;
		case RTE_ETH_MQ_RX_VMDQ_DCB_RSS:
			/* DCB/RSS VMDQ in SRIOV mode, not implement yet */
			PMD_INIT_LOG(ERR, "SRIOV active,"
					" unsupported mq_mode rx %d.",
					dev_conf->rxmode.mq_mode);
			return -EINVAL;
		case RTE_ETH_MQ_RX_RSS:
		case RTE_ETH_MQ_RX_VMDQ_RSS:
			dev->data->dev_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_VMDQ_RSS;
			if (nb_rx_q <= RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)
				if (txgbe_check_vf_rss_rxq_num(dev, nb_rx_q)) {
					PMD_INIT_LOG(ERR, "SRIOV is active,"
						" invalid queue number"
						" for VMDQ RSS, allowed"
						" value are 1, 2 or 4.");
					return -EINVAL;
				}
			break;
		case RTE_ETH_MQ_RX_VMDQ_ONLY:
		case RTE_ETH_MQ_RX_NONE:
			/* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.rxmode.mq_mode =
				RTE_ETH_MQ_RX_VMDQ_ONLY;
			break;
		default: /* RTE_ETH_MQ_RX_DCB, RTE_ETH_MQ_RX_DCB_RSS or RTE_ETH_MQ_TX_DCB*/
			/* SRIOV only works in VMDq enable mode */
			PMD_INIT_LOG(ERR, "SRIOV is active,"
					" wrong mq_mode rx %d.",
					dev_conf->rxmode.mq_mode);
			return -EINVAL;
		}

		switch (dev_conf->txmode.mq_mode) {
		case RTE_ETH_MQ_TX_VMDQ_DCB:
			PMD_INIT_LOG(INFO, "RTE_ETH_MQ_TX_VMDQ_DCB mode supported in SRIOV");
			dev->data->dev_conf.txmode.mq_mode = RTE_ETH_MQ_TX_VMDQ_DCB;
			break;
		default: /* RTE_ETH_MQ_TX_VMDQ_ONLY or RTE_ETH_MQ_TX_NONE */
			dev->data->dev_conf.txmode.mq_mode =
				RTE_ETH_MQ_TX_VMDQ_ONLY;
			break;
		}

		/* check valid queue number */
		if ((nb_rx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool) ||
		    (nb_tx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)) {
			PMD_INIT_LOG(ERR, "SRIOV is active,"
					" nb_rx_q=%d nb_tx_q=%d queue number"
					" must be less than or equal to %d.",
					nb_rx_q, nb_tx_q,
					RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool);
			return -EINVAL;
		}
	} else {
		if (dev_conf->rxmode.mq_mode == RTE_ETH_MQ_RX_VMDQ_DCB_RSS) {
			PMD_INIT_LOG(ERR, "VMDQ+DCB+RSS mq_mode is"
					  " not supported.");
			return -EINVAL;
		}
		/* check configuration for vmdb+dcb mode */
		if (dev_conf->rxmode.mq_mode == RTE_ETH_MQ_RX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_conf *conf;

			if (nb_rx_q != TXGBE_VMDQ_DCB_NB_QUEUES) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB, nb_rx_q != %d.",
						TXGBE_VMDQ_DCB_NB_QUEUES);
				return -EINVAL;
			}
			conf = &dev_conf->rx_adv_conf.vmdq_dcb_conf;
			if (!(conf->nb_queue_pools == RTE_ETH_16_POOLS ||
			       conf->nb_queue_pools == RTE_ETH_32_POOLS)) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB selected,"
						" nb_queue_pools must be %d or %d.",
						RTE_ETH_16_POOLS, RTE_ETH_32_POOLS);
				return -EINVAL;
			}
		}
		if (dev_conf->txmode.mq_mode == RTE_ETH_MQ_TX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_tx_conf *conf;

			if (nb_tx_q != TXGBE_VMDQ_DCB_NB_QUEUES) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB, nb_tx_q != %d",
						 TXGBE_VMDQ_DCB_NB_QUEUES);
				return -EINVAL;
			}
			conf = &dev_conf->tx_adv_conf.vmdq_dcb_tx_conf;
			if (!(conf->nb_queue_pools == RTE_ETH_16_POOLS ||
			       conf->nb_queue_pools == RTE_ETH_32_POOLS)) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB selected,"
						" nb_queue_pools != %d and"
						" nb_queue_pools != %d.",
						RTE_ETH_16_POOLS, RTE_ETH_32_POOLS);
				return -EINVAL;
			}
		}

		/* For DCB mode check our configuration before we go further */
		if (dev_conf->rxmode.mq_mode == RTE_ETH_MQ_RX_DCB) {
			const struct rte_eth_dcb_rx_conf *conf;

			conf = &dev_conf->rx_adv_conf.dcb_rx_conf;
			if (!(conf->nb_tcs == RTE_ETH_4_TCS ||
			       conf->nb_tcs == RTE_ETH_8_TCS)) {
				PMD_INIT_LOG(ERR, "DCB selected, nb_tcs != %d"
						" and nb_tcs != %d.",
						RTE_ETH_4_TCS, RTE_ETH_8_TCS);
				return -EINVAL;
			}
		}

		if (dev_conf->txmode.mq_mode == RTE_ETH_MQ_TX_DCB) {
			const struct rte_eth_dcb_tx_conf *conf;

			conf = &dev_conf->tx_adv_conf.dcb_tx_conf;
			if (!(conf->nb_tcs == RTE_ETH_4_TCS ||
			       conf->nb_tcs == RTE_ETH_8_TCS)) {
				PMD_INIT_LOG(ERR, "DCB selected, nb_tcs != %d"
						" and nb_tcs != %d.",
						RTE_ETH_4_TCS, RTE_ETH_8_TCS);
				return -EINVAL;
			}
		}
	}
	return 0;
}

static int
txgbe_dev_configure(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* multiple queue mode checking */
	ret  = txgbe_check_mq_mode(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "txgbe_check_mq_mode fails with %d.",
			    ret);
		return ret;
	}

	/* set flag to update link status after init */
	intr->flags |= TXGBE_FLAG_NEED_LINK_UPDATE;

	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation Rx preconditions we will reset it.
	 */
	adapter->rx_bulk_alloc_allowed = true;

	return 0;
}

static void
txgbe_dev_phy_intr_setup(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	uint32_t gpie;

	gpie = rd32(hw, TXGBE_GPIOINTEN);
	gpie |= TXGBE_GPIOBIT_6;
	wr32(hw, TXGBE_GPIOINTEN, gpie);
	intr->mask_misc |= TXGBE_ICRMISC_GPIO;
	intr->mask_misc |= TXGBE_ICRMISC_ANDONE;
}

int
txgbe_set_vf_rate_limit(struct rte_eth_dev *dev, uint16_t vf,
			uint16_t tx_rate, uint64_t q_msk)
{
	struct txgbe_hw *hw;
	struct txgbe_vf_info *vfinfo;
	struct rte_eth_link link;
	uint8_t  nb_q_per_pool;
	uint32_t queue_stride;
	uint32_t queue_idx, idx = 0, vf_idx;
	uint32_t queue_end;
	uint16_t total_rate = 0;
	struct rte_pci_device *pci_dev;
	int ret;

	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	ret = rte_eth_link_get_nowait(dev->data->port_id, &link);
	if (ret < 0)
		return ret;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (tx_rate > link.link_speed)
		return -EINVAL;

	if (q_msk == 0)
		return 0;

	hw = TXGBE_DEV_HW(dev);
	vfinfo = *(TXGBE_DEV_VFDATA(dev));
	nb_q_per_pool = RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;
	queue_stride = TXGBE_MAX_RX_QUEUE_NUM / RTE_ETH_DEV_SRIOV(dev).active;
	queue_idx = vf * queue_stride;
	queue_end = queue_idx + nb_q_per_pool - 1;
	if (queue_end >= hw->mac.max_tx_queues)
		return -EINVAL;

	if (vfinfo) {
		for (vf_idx = 0; vf_idx < pci_dev->max_vfs; vf_idx++) {
			if (vf_idx == vf)
				continue;
			for (idx = 0; idx < RTE_DIM(vfinfo[vf_idx].tx_rate);
				idx++)
				total_rate += vfinfo[vf_idx].tx_rate[idx];
		}
	} else {
		return -EINVAL;
	}

	/* Store tx_rate for this vf. */
	for (idx = 0; idx < nb_q_per_pool; idx++) {
		if (((uint64_t)0x1 << idx) & q_msk) {
			if (vfinfo[vf].tx_rate[idx] != tx_rate)
				vfinfo[vf].tx_rate[idx] = tx_rate;
			total_rate += tx_rate;
		}
	}

	if (total_rate > dev->data->dev_link.link_speed) {
		/* Reset stored TX rate of the VF if it causes exceed
		 * link speed.
		 */
		memset(vfinfo[vf].tx_rate, 0, sizeof(vfinfo[vf].tx_rate));
		return -EINVAL;
	}

	/* Set ARBTXRATE of each queue/pool for vf X  */
	for (; queue_idx <= queue_end; queue_idx++) {
		if (0x1 & q_msk)
			txgbe_set_queue_rate_limit(dev, queue_idx, tx_rate);
		q_msk = q_msk >> 1;
	}

	return 0;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
txgbe_dev_start(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	int err;
	bool link_up = false, negotiate = 0;
	uint32_t speed = 0;
	uint32_t allowed_speeds = 0;
	int mask = 0;
	int status;
	uint16_t vf, idx;
	uint32_t *link_speeds;
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);

	PMD_INIT_FUNC_TRACE();

	/* Stop the link setup handler before resetting the HW. */
	rte_eal_alarm_cancel(txgbe_dev_setup_link_alarm_handler, dev);

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* stop adapter */
	hw->adapter_stopped = 0;
	txgbe_stop_hw(hw);

	/* reinitialize adapter
	 * this calls reset and start
	 */
	hw->nb_rx_queues = dev->data->nb_rx_queues;
	hw->nb_tx_queues = dev->data->nb_tx_queues;
	status = txgbe_pf_reset_hw(hw);
	if (status != 0)
		return -1;
	hw->mac.start_hw(hw);
	hw->mac.get_link_status = true;
	hw->dev_start = true;

	/* configure PF module if SRIOV enabled */
	txgbe_pf_host_configure(dev);

	txgbe_dev_phy_intr_setup(dev);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						   dev->data->nb_rx_queues)) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
				     " intr_vec", dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}
	/* configure msix for sleep until rx interrupt */
	txgbe_configure_msix(dev);

	/* initialize transmission unit */
	txgbe_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = txgbe_dev_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		goto error;
	}

	mask = RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK |
		RTE_ETH_VLAN_EXTEND_MASK;
	err = txgbe_vlan_offload_config(dev, mask);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to set VLAN offload");
		goto error;
	}

	if (dev->data->dev_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_VMDQ_ONLY) {
		/* Enable vlan filtering for VMDq */
		txgbe_vmdq_vlan_hw_filter_enable(dev);
	}

	/* Configure DCB hw */
	txgbe_configure_pb(dev);
	txgbe_configure_port(dev);
	txgbe_configure_dcb(dev);

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		err = txgbe_fdir_configure(dev);
		if (err)
			goto error;
	}

	/* Restore vf rate limit */
	if (vfinfo != NULL) {
		for (vf = 0; vf < pci_dev->max_vfs; vf++)
			for (idx = 0; idx < TXGBE_MAX_QUEUE_NUM_PER_VF; idx++)
				if (vfinfo[vf].tx_rate[idx] != 0)
					txgbe_set_vf_rate_limit(dev, vf,
						vfinfo[vf].tx_rate[idx],
						1 << idx);
	}

	err = txgbe_dev_rxtx_start(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Unable to start rxtx queues");
		goto error;
	}

	/* Skip link setup if loopback mode is enabled. */
	if (hw->mac.type == txgbe_mac_raptor &&
	    dev->data->dev_conf.lpbk_mode)
		goto skip_link_setup;

	if (txgbe_is_sfp(hw) && hw->phy.multispeed_fiber) {
		err = hw->mac.setup_sfp(hw);
		if (err)
			goto error;
	}

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn on the copper */
		hw->phy.set_phy_power(hw, true);
	} else {
		/* Turn on the laser */
		hw->mac.enable_tx_laser(hw);
	}

	if ((hw->subsystem_device_id & 0xFF) != TXGBE_DEV_ID_KR_KX_KX4)
		err = hw->mac.check_link(hw, &speed, &link_up, 0);
	if (err)
		goto error;
	dev->data->dev_link.link_status = link_up;

	err = hw->mac.get_link_capabilities(hw, &speed, &negotiate);
	if (err)
		goto error;

	allowed_speeds = RTE_ETH_LINK_SPEED_100M | RTE_ETH_LINK_SPEED_1G |
			RTE_ETH_LINK_SPEED_10G;

	link_speeds = &dev->data->dev_conf.link_speeds;
	if (((*link_speeds) >> 1) & ~(allowed_speeds >> 1)) {
		PMD_INIT_LOG(ERR, "Invalid link setting");
		goto error;
	}

	speed = 0x0;
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
		speed = (TXGBE_LINK_SPEED_100M_FULL |
			 TXGBE_LINK_SPEED_1GB_FULL |
			 TXGBE_LINK_SPEED_10GB_FULL);
	} else {
		if (*link_speeds & RTE_ETH_LINK_SPEED_10G)
			speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_5G)
			speed |= TXGBE_LINK_SPEED_5GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_2_5G)
			speed |= TXGBE_LINK_SPEED_2_5GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_1G)
			speed |= TXGBE_LINK_SPEED_1GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_100M)
			speed |= TXGBE_LINK_SPEED_100M_FULL;
	}

	err = hw->mac.setup_link(hw, speed, link_up);
	if (err)
		goto error;

skip_link_setup:

	if (rte_intr_allow_others(intr_handle)) {
		txgbe_dev_misc_interrupt_setup(dev);
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			txgbe_dev_lsc_interrupt_setup(dev, TRUE);
		else
			txgbe_dev_lsc_interrupt_setup(dev, FALSE);
		txgbe_dev_macsec_interrupt_setup(dev);
		txgbe_set_ivar_map(hw, -1, 1, TXGBE_MISC_VEC_ID);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     txgbe_dev_interrupt_handler, dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO, "lsc won't enable because of"
				     " no intr multiplex");
	}

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0 &&
	    rte_intr_dp_is_en(intr_handle))
		txgbe_dev_rxq_interrupt_setup(dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since hw reset */
	txgbe_enable_intr(dev);
	txgbe_l2_tunnel_conf(dev);
	txgbe_filter_restore(dev);

	if (tm_conf->root && !tm_conf->committed)
		PMD_DRV_LOG(WARNING,
			    "please call hierarchy_commit() "
			    "before starting the port");

	/*
	 * Update link status right before return, because it may
	 * start link configuration process in a separate thread.
	 */
	txgbe_dev_link_update(dev, 0);

	wr32m(hw, TXGBE_LEDCTL, 0xFFFFFFFF, TXGBE_LEDCTL_ORD_MASK);

	txgbe_read_stats_registers(hw, hw_stats);
	hw->offset_loaded = 1;

	return 0;

error:
	PMD_INIT_LOG(ERR, "failure in dev start: %d", err);
	txgbe_dev_clear_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
txgbe_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int vf;
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);

	if (hw->adapter_stopped)
		return 0;

	PMD_INIT_FUNC_TRACE();

	rte_eal_alarm_cancel(txgbe_dev_setup_link_alarm_handler, dev);

	/* disable interrupts */
	txgbe_disable_intr(hw);

	/* reset the NIC */
	txgbe_pf_reset_hw(hw);
	hw->adapter_stopped = 0;

	/* stop adapter */
	txgbe_stop_hw(hw);

	for (vf = 0; vfinfo != NULL && vf < pci_dev->max_vfs; vf++)
		vfinfo[vf].clear_to_send = false;

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn off the copper */
		hw->phy.set_phy_power(hw, false);
	} else {
		/* Turn off the laser */
		hw->mac.disable_tx_laser(hw);
	}

	txgbe_dev_clear_queues(dev);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;
	dev->data->lro = 0;

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   txgbe_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	/* reset hierarchy commit */
	tm_conf->committed = false;

	adapter->rss_reta_updated = 0;
	wr32m(hw, TXGBE_LEDCTL, 0xFFFFFFFF, TXGBE_LEDCTL_SEL_MASK);

	hw->adapter_stopped = true;
	dev->data->dev_started = 0;
	hw->dev_start = false;

	return 0;
}

/*
 * Set device link up: enable tx.
 */
static int
txgbe_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn on the copper */
		hw->phy.set_phy_power(hw, true);
	} else {
		/* Turn on the laser */
		hw->mac.enable_tx_laser(hw);
		hw->dev_start = true;
		txgbe_dev_link_update(dev, 0);
	}

	return 0;
}

/*
 * Set device link down: disable tx.
 */
static int
txgbe_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn off the copper */
		hw->phy.set_phy_power(hw, false);
	} else {
		/* Turn off the laser */
		hw->mac.disable_tx_laser(hw);
		hw->dev_start = false;
		txgbe_dev_link_update(dev, 0);
	}

	return 0;
}

/*
 * Reset and stop device.
 */
static int
txgbe_dev_close(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int retries = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	txgbe_pf_reset_hw(hw);

	ret = txgbe_dev_stop(dev);

	txgbe_dev_free_queues(dev);

	/* reprogram the RAR[0] in case user changed it. */
	txgbe_set_rar(hw, 0, hw->mac.addr, 0, true);

	/* Unlock any pending hardware semaphore */
	txgbe_swfw_lock_reset(hw);

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	do {
		ret = rte_intr_callback_unregister(intr_handle,
				txgbe_dev_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR,
				"intr callback unregister failed: %d",
				ret);
		}
		rte_delay_ms(100);
	} while (retries++ < (10 + TXGBE_LINK_UP_TIME));

	/* cancel the delay handler before remove dev */
	rte_eal_alarm_cancel(txgbe_dev_interrupt_delayed_handler, dev);

	/* uninitialize PF if max_vfs not zero */
	txgbe_pf_host_uninit(dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	rte_free(dev->data->hash_mac_addrs);
	dev->data->hash_mac_addrs = NULL;

	/* remove all the fdir filters & hash */
	txgbe_fdir_filter_uninit(dev);

	/* remove all the L2 tunnel filters & hash */
	txgbe_l2_tn_filter_uninit(dev);

	/* Remove all ntuple filters of the device */
	txgbe_ntuple_filter_uninit(dev);

	/* clear all the filters list */
	txgbe_filterlist_flush();

	/* Remove all Traffic Manager configuration */
	txgbe_tm_conf_uninit(dev);

#ifdef RTE_LIB_SECURITY
	rte_free(dev->security_ctx);
	dev->security_ctx = NULL;
#endif

	return ret;
}

/*
 * Reset PF device.
 */
static int
txgbe_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	/* When a DPDK PMD PF begin to reset PF port, it should notify all
	 * its VF to make them align with it. The detailed notification
	 * mechanism is PMD specific. As to txgbe PF, it is rather complex.
	 * To avoid unexpected behavior in VF, currently reset of PF with
	 * SR-IOV activation is not supported. It might be supported later.
	 */
	if (dev->data->sriov.active)
		return -ENOTSUP;

	ret = eth_txgbe_dev_uninit(dev);
	if (ret)
		return ret;

	ret = eth_txgbe_dev_init(dev, NULL);

	return ret;
}

#define UPDATE_QP_COUNTER_32bit(reg, last_counter, counter)     \
	{                                                       \
		uint32_t current_counter = rd32(hw, reg);       \
		if (current_counter < last_counter)             \
			current_counter += 0x100000000LL;       \
		if (!hw->offset_loaded)                         \
			last_counter = current_counter;         \
		counter = current_counter - last_counter;       \
		counter &= 0xFFFFFFFFLL;                        \
	}

#define UPDATE_QP_COUNTER_36bit(reg_lsb, reg_msb, last_counter, counter) \
	{                                                                \
		uint64_t current_counter_lsb = rd32(hw, reg_lsb);        \
		uint64_t current_counter_msb = rd32(hw, reg_msb);        \
		uint64_t current_counter = (current_counter_msb << 32) | \
			current_counter_lsb;                             \
		if (current_counter < last_counter)                      \
			current_counter += 0x1000000000LL;               \
		if (!hw->offset_loaded)                                  \
			last_counter = current_counter;                  \
		counter = current_counter - last_counter;                \
		counter &= 0xFFFFFFFFFLL;                                \
	}

void
txgbe_read_stats_registers(struct txgbe_hw *hw,
			   struct txgbe_hw_stats *hw_stats)
{
	unsigned int i;

	/* QP Stats */
	for (i = 0; i < hw->nb_rx_queues; i++) {
		UPDATE_QP_COUNTER_32bit(TXGBE_QPRXPKT(i),
			hw->qp_last[i].rx_qp_packets,
			hw_stats->qp[i].rx_qp_packets);
		UPDATE_QP_COUNTER_36bit(TXGBE_QPRXOCTL(i), TXGBE_QPRXOCTH(i),
			hw->qp_last[i].rx_qp_bytes,
			hw_stats->qp[i].rx_qp_bytes);
		UPDATE_QP_COUNTER_32bit(TXGBE_QPRXMPKT(i),
			hw->qp_last[i].rx_qp_mc_packets,
			hw_stats->qp[i].rx_qp_mc_packets);
	}

	for (i = 0; i < hw->nb_tx_queues; i++) {
		UPDATE_QP_COUNTER_32bit(TXGBE_QPTXPKT(i),
			hw->qp_last[i].tx_qp_packets,
			hw_stats->qp[i].tx_qp_packets);
		UPDATE_QP_COUNTER_36bit(TXGBE_QPTXOCTL(i), TXGBE_QPTXOCTH(i),
			hw->qp_last[i].tx_qp_bytes,
			hw_stats->qp[i].tx_qp_bytes);
	}
	/* PB Stats */
	for (i = 0; i < TXGBE_MAX_UP; i++) {
		hw_stats->up[i].rx_up_xon_packets +=
				rd32(hw, TXGBE_PBRXUPXON(i));
		hw_stats->up[i].rx_up_xoff_packets +=
				rd32(hw, TXGBE_PBRXUPXOFF(i));
		hw_stats->up[i].tx_up_xon_packets +=
				rd32(hw, TXGBE_PBTXUPXON(i));
		hw_stats->up[i].tx_up_xoff_packets +=
				rd32(hw, TXGBE_PBTXUPXOFF(i));
		hw_stats->up[i].tx_up_xon2off_packets +=
				rd32(hw, TXGBE_PBTXUPOFF(i));
		hw_stats->up[i].rx_up_dropped +=
				rd32(hw, TXGBE_PBRXMISS(i));
	}
	hw_stats->rx_xon_packets += rd32(hw, TXGBE_PBRXLNKXON);
	hw_stats->rx_xoff_packets += rd32(hw, TXGBE_PBRXLNKXOFF);
	hw_stats->tx_xon_packets += rd32(hw, TXGBE_PBTXLNKXON);
	hw_stats->tx_xoff_packets += rd32(hw, TXGBE_PBTXLNKXOFF);

	/* DMA Stats */
	hw_stats->rx_packets += rd32(hw, TXGBE_DMARXPKT);
	hw_stats->tx_packets += rd32(hw, TXGBE_DMATXPKT);

	hw_stats->rx_bytes += rd64(hw, TXGBE_DMARXOCTL);
	hw_stats->tx_bytes += rd64(hw, TXGBE_DMATXOCTL);
	hw_stats->rx_dma_drop += rd32(hw, TXGBE_DMARXDROP);
	hw_stats->rx_drop_packets += rd32(hw, TXGBE_PBRXDROP);

	/* MAC Stats */
	hw_stats->rx_crc_errors += rd64(hw, TXGBE_MACRXERRCRCL);
	hw_stats->rx_multicast_packets += rd64(hw, TXGBE_MACRXMPKTL);
	hw_stats->tx_multicast_packets += rd64(hw, TXGBE_MACTXMPKTL);

	hw_stats->rx_total_packets += rd64(hw, TXGBE_MACRXPKTL);
	hw_stats->tx_total_packets += rd64(hw, TXGBE_MACTXPKTL);
	hw_stats->rx_total_bytes += rd64(hw, TXGBE_MACRXGBOCTL);

	hw_stats->rx_broadcast_packets += rd64(hw, TXGBE_MACRXOCTL);
	hw_stats->tx_broadcast_packets += rd32(hw, TXGBE_MACTXOCTL);

	hw_stats->rx_size_64_packets += rd64(hw, TXGBE_MACRX1TO64L);
	hw_stats->rx_size_65_to_127_packets += rd64(hw, TXGBE_MACRX65TO127L);
	hw_stats->rx_size_128_to_255_packets += rd64(hw, TXGBE_MACRX128TO255L);
	hw_stats->rx_size_256_to_511_packets += rd64(hw, TXGBE_MACRX256TO511L);
	hw_stats->rx_size_512_to_1023_packets +=
			rd64(hw, TXGBE_MACRX512TO1023L);
	hw_stats->rx_size_1024_to_max_packets +=
			rd64(hw, TXGBE_MACRX1024TOMAXL);
	hw_stats->tx_size_64_packets += rd64(hw, TXGBE_MACTX1TO64L);
	hw_stats->tx_size_65_to_127_packets += rd64(hw, TXGBE_MACTX65TO127L);
	hw_stats->tx_size_128_to_255_packets += rd64(hw, TXGBE_MACTX128TO255L);
	hw_stats->tx_size_256_to_511_packets += rd64(hw, TXGBE_MACTX256TO511L);
	hw_stats->tx_size_512_to_1023_packets +=
			rd64(hw, TXGBE_MACTX512TO1023L);
	hw_stats->tx_size_1024_to_max_packets +=
			rd64(hw, TXGBE_MACTX1024TOMAXL);

	hw_stats->rx_undersize_errors += rd64(hw, TXGBE_MACRXERRLENL);
	hw_stats->rx_oversize_errors += rd32(hw, TXGBE_MACRXOVERSIZE);
	hw_stats->rx_jabber_errors += rd32(hw, TXGBE_MACRXJABBER);

	/* MNG Stats */
	hw_stats->mng_bmc2host_packets = rd32(hw, TXGBE_MNGBMC2OS);
	hw_stats->mng_host2bmc_packets = rd32(hw, TXGBE_MNGOS2BMC);
	hw_stats->rx_management_packets = rd32(hw, TXGBE_DMARXMNG);
	hw_stats->tx_management_packets = rd32(hw, TXGBE_DMATXMNG);

	/* FCoE Stats */
	hw_stats->rx_fcoe_crc_errors += rd32(hw, TXGBE_FCOECRC);
	hw_stats->rx_fcoe_mbuf_allocation_errors += rd32(hw, TXGBE_FCOELAST);
	hw_stats->rx_fcoe_dropped += rd32(hw, TXGBE_FCOERPDC);
	hw_stats->rx_fcoe_packets += rd32(hw, TXGBE_FCOEPRC);
	hw_stats->tx_fcoe_packets += rd32(hw, TXGBE_FCOEPTC);
	hw_stats->rx_fcoe_bytes += rd32(hw, TXGBE_FCOEDWRC);
	hw_stats->tx_fcoe_bytes += rd32(hw, TXGBE_FCOEDWTC);

	/* Flow Director Stats */
	hw_stats->flow_director_matched_filters += rd32(hw, TXGBE_FDIRMATCH);
	hw_stats->flow_director_missed_filters += rd32(hw, TXGBE_FDIRMISS);
	hw_stats->flow_director_added_filters +=
		TXGBE_FDIRUSED_ADD(rd32(hw, TXGBE_FDIRUSED));
	hw_stats->flow_director_removed_filters +=
		TXGBE_FDIRUSED_REM(rd32(hw, TXGBE_FDIRUSED));
	hw_stats->flow_director_filter_add_errors +=
		TXGBE_FDIRFAIL_ADD(rd32(hw, TXGBE_FDIRFAIL));
	hw_stats->flow_director_filter_remove_errors +=
		TXGBE_FDIRFAIL_REM(rd32(hw, TXGBE_FDIRFAIL));

	/* MACsec Stats */
	hw_stats->tx_macsec_pkts_untagged += rd32(hw, TXGBE_LSECTX_UTPKT);
	hw_stats->tx_macsec_pkts_encrypted +=
			rd32(hw, TXGBE_LSECTX_ENCPKT);
	hw_stats->tx_macsec_pkts_protected +=
			rd32(hw, TXGBE_LSECTX_PROTPKT);
	hw_stats->tx_macsec_octets_encrypted +=
			rd32(hw, TXGBE_LSECTX_ENCOCT);
	hw_stats->tx_macsec_octets_protected +=
			rd32(hw, TXGBE_LSECTX_PROTOCT);
	hw_stats->rx_macsec_pkts_untagged += rd32(hw, TXGBE_LSECRX_UTPKT);
	hw_stats->rx_macsec_pkts_badtag += rd32(hw, TXGBE_LSECRX_BTPKT);
	hw_stats->rx_macsec_pkts_nosci += rd32(hw, TXGBE_LSECRX_NOSCIPKT);
	hw_stats->rx_macsec_pkts_unknownsci += rd32(hw, TXGBE_LSECRX_UNSCIPKT);
	hw_stats->rx_macsec_octets_decrypted += rd32(hw, TXGBE_LSECRX_DECOCT);
	hw_stats->rx_macsec_octets_validated += rd32(hw, TXGBE_LSECRX_VLDOCT);
	hw_stats->rx_macsec_sc_pkts_unchecked +=
			rd32(hw, TXGBE_LSECRX_UNCHKPKT);
	hw_stats->rx_macsec_sc_pkts_delayed += rd32(hw, TXGBE_LSECRX_DLYPKT);
	hw_stats->rx_macsec_sc_pkts_late += rd32(hw, TXGBE_LSECRX_LATEPKT);
	for (i = 0; i < 2; i++) {
		hw_stats->rx_macsec_sa_pkts_ok +=
			rd32(hw, TXGBE_LSECRX_OKPKT(i));
		hw_stats->rx_macsec_sa_pkts_invalid +=
			rd32(hw, TXGBE_LSECRX_INVPKT(i));
		hw_stats->rx_macsec_sa_pkts_notvalid +=
			rd32(hw, TXGBE_LSECRX_BADPKT(i));
	}
	hw_stats->rx_macsec_sa_pkts_unusedsa +=
			rd32(hw, TXGBE_LSECRX_INVSAPKT);
	hw_stats->rx_macsec_sa_pkts_notusingsa +=
			rd32(hw, TXGBE_LSECRX_BADSAPKT);

	hw_stats->rx_total_missed_packets = 0;
	for (i = 0; i < TXGBE_MAX_UP; i++) {
		hw_stats->rx_total_missed_packets +=
			hw_stats->up[i].rx_up_dropped;
	}
}

static int
txgbe_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);
	struct txgbe_stat_mappings *stat_mappings =
			TXGBE_DEV_STAT_MAPPINGS(dev);
	uint32_t i, j;

	txgbe_read_stats_registers(hw, hw_stats);

	if (stats == NULL)
		return -EINVAL;

	/* Fill out the rte_eth_stats statistics structure */
	stats->ipackets = hw_stats->rx_packets;
	stats->ibytes = hw_stats->rx_bytes;
	stats->opackets = hw_stats->tx_packets;
	stats->obytes = hw_stats->tx_bytes;

	memset(&stats->q_ipackets, 0, sizeof(stats->q_ipackets));
	memset(&stats->q_opackets, 0, sizeof(stats->q_opackets));
	memset(&stats->q_ibytes, 0, sizeof(stats->q_ibytes));
	memset(&stats->q_obytes, 0, sizeof(stats->q_obytes));
	memset(&stats->q_errors, 0, sizeof(stats->q_errors));
	for (i = 0; i < TXGBE_MAX_QP; i++) {
		uint32_t n = i / NB_QMAP_FIELDS_PER_QSM_REG;
		uint32_t offset = (i % NB_QMAP_FIELDS_PER_QSM_REG) * 8;
		uint32_t q_map;

		q_map = (stat_mappings->rqsm[n] >> offset)
				& QMAP_FIELD_RESERVED_BITS_MASK;
		j = (q_map < RTE_ETHDEV_QUEUE_STAT_CNTRS
		     ? q_map : q_map % RTE_ETHDEV_QUEUE_STAT_CNTRS);
		stats->q_ipackets[j] += hw_stats->qp[i].rx_qp_packets;
		stats->q_ibytes[j] += hw_stats->qp[i].rx_qp_bytes;

		q_map = (stat_mappings->tqsm[n] >> offset)
				& QMAP_FIELD_RESERVED_BITS_MASK;
		j = (q_map < RTE_ETHDEV_QUEUE_STAT_CNTRS
		     ? q_map : q_map % RTE_ETHDEV_QUEUE_STAT_CNTRS);
		stats->q_opackets[j] += hw_stats->qp[i].tx_qp_packets;
		stats->q_obytes[j] += hw_stats->qp[i].tx_qp_bytes;
	}

	/* Rx Errors */
	stats->imissed  = hw_stats->rx_total_missed_packets +
			  hw_stats->rx_dma_drop;
	stats->ierrors  = hw_stats->rx_crc_errors +
			  hw_stats->rx_mac_short_packet_dropped +
			  hw_stats->rx_length_errors +
			  hw_stats->rx_undersize_errors +
			  hw_stats->rx_oversize_errors +
			  hw_stats->rx_drop_packets +
			  hw_stats->rx_illegal_byte_errors +
			  hw_stats->rx_error_bytes +
			  hw_stats->rx_fragment_errors +
			  hw_stats->rx_fcoe_crc_errors +
			  hw_stats->rx_fcoe_mbuf_allocation_errors;

	/* Tx Errors */
	stats->oerrors  = 0;
	return 0;
}

static int
txgbe_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);

	/* HW registers are cleared on read */
	hw->offset_loaded = 0;
	txgbe_dev_stats_get(dev, NULL);
	hw->offset_loaded = 1;

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));

	return 0;
}

/* This function calculates the number of xstats based on the current config */
static unsigned
txgbe_xstats_calc_num(struct rte_eth_dev *dev)
{
	int nb_queues = max(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	return TXGBE_NB_HW_STATS +
	       TXGBE_NB_UP_STATS * TXGBE_MAX_UP +
	       TXGBE_NB_QP_STATS * nb_queues;
}

static inline int
txgbe_get_name_by_id(uint32_t id, char *name, uint32_t size)
{
	int nb, st;

	/* Extended stats from txgbe_hw_stats */
	if (id < TXGBE_NB_HW_STATS) {
		snprintf(name, size, "[hw]%s",
			rte_txgbe_stats_strings[id].name);
		return 0;
	}
	id -= TXGBE_NB_HW_STATS;

	/* Priority Stats */
	if (id < TXGBE_NB_UP_STATS * TXGBE_MAX_UP) {
		nb = id / TXGBE_NB_UP_STATS;
		st = id % TXGBE_NB_UP_STATS;
		snprintf(name, size, "[p%u]%s", nb,
			rte_txgbe_up_strings[st].name);
		return 0;
	}
	id -= TXGBE_NB_UP_STATS * TXGBE_MAX_UP;

	/* Queue Stats */
	if (id < TXGBE_NB_QP_STATS * TXGBE_MAX_QP) {
		nb = id / TXGBE_NB_QP_STATS;
		st = id % TXGBE_NB_QP_STATS;
		snprintf(name, size, "[q%u]%s", nb,
			rte_txgbe_qp_strings[st].name);
		return 0;
	}
	id -= TXGBE_NB_QP_STATS * TXGBE_MAX_QP;

	return -(int)(id + 1);
}

static inline int
txgbe_get_offset_by_id(uint32_t id, uint32_t *offset)
{
	int nb, st;

	/* Extended stats from txgbe_hw_stats */
	if (id < TXGBE_NB_HW_STATS) {
		*offset = rte_txgbe_stats_strings[id].offset;
		return 0;
	}
	id -= TXGBE_NB_HW_STATS;

	/* Priority Stats */
	if (id < TXGBE_NB_UP_STATS * TXGBE_MAX_UP) {
		nb = id / TXGBE_NB_UP_STATS;
		st = id % TXGBE_NB_UP_STATS;
		*offset = rte_txgbe_up_strings[st].offset +
			nb * (TXGBE_NB_UP_STATS * sizeof(uint64_t));
		return 0;
	}
	id -= TXGBE_NB_UP_STATS * TXGBE_MAX_UP;

	/* Queue Stats */
	if (id < TXGBE_NB_QP_STATS * TXGBE_MAX_QP) {
		nb = id / TXGBE_NB_QP_STATS;
		st = id % TXGBE_NB_QP_STATS;
		*offset = rte_txgbe_qp_strings[st].offset +
			nb * (TXGBE_NB_QP_STATS * sizeof(uint64_t));
		return 0;
	}

	return -1;
}

static int txgbe_dev_xstats_get_names(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int limit)
{
	unsigned int i, count;

	count = txgbe_xstats_calc_num(dev);
	if (xstats_names == NULL)
		return count;

	/* Note: limit >= cnt_stats checked upstream
	 * in rte_eth_xstats_names()
	 */
	limit = min(limit, count);

	/* Extended stats from txgbe_hw_stats */
	for (i = 0; i < limit; i++) {
		if (txgbe_get_name_by_id(i, xstats_names[i].name,
			sizeof(xstats_names[i].name))) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
	}

	return i;
}

static int txgbe_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
	const uint64_t *ids,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int limit)
{
	unsigned int i;

	if (ids == NULL)
		return txgbe_dev_xstats_get_names(dev, xstats_names, limit);

	for (i = 0; i < limit; i++) {
		if (txgbe_get_name_by_id(ids[i], xstats_names[i].name,
				sizeof(xstats_names[i].name))) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			return -1;
		}
	}

	return i;
}

static int
txgbe_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
					 unsigned int limit)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);
	unsigned int i, count;

	txgbe_read_stats_registers(hw, hw_stats);

	/* If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	count = txgbe_xstats_calc_num(dev);
	if (xstats == NULL)
		return count;

	limit = min(limit, txgbe_xstats_calc_num(dev));

	/* Extended stats from txgbe_hw_stats */
	for (i = 0; i < limit; i++) {
		uint32_t offset = 0;

		if (txgbe_get_offset_by_id(i, &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		xstats[i].value = *(uint64_t *)(((char *)hw_stats) + offset);
		xstats[i].id = i;
	}

	return i;
}

static int
txgbe_dev_xstats_get_(struct rte_eth_dev *dev, uint64_t *values,
					 unsigned int limit)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);
	unsigned int i, count;

	txgbe_read_stats_registers(hw, hw_stats);

	/* If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	count = txgbe_xstats_calc_num(dev);
	if (values == NULL)
		return count;

	limit = min(limit, txgbe_xstats_calc_num(dev));

	/* Extended stats from txgbe_hw_stats */
	for (i = 0; i < limit; i++) {
		uint32_t offset;

		if (txgbe_get_offset_by_id(i, &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		values[i] = *(uint64_t *)(((char *)hw_stats) + offset);
	}

	return i;
}

static int
txgbe_dev_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		uint64_t *values, unsigned int limit)
{
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);
	unsigned int i;

	if (ids == NULL)
		return txgbe_dev_xstats_get_(dev, values, limit);

	for (i = 0; i < limit; i++) {
		uint32_t offset;

		if (txgbe_get_offset_by_id(ids[i], &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		values[i] = *(uint64_t *)(((char *)hw_stats) + offset);
	}

	return i;
}

static int
txgbe_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_stats *hw_stats = TXGBE_DEV_STATS(dev);

	/* HW registers are cleared on read */
	hw->offset_loaded = 0;
	txgbe_read_stats_registers(hw, hw_stats);
	hw->offset_loaded = 1;

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));

	return 0;
}

static int
txgbe_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	u32 etrack_id;
	int ret;

	hw->phy.get_fw_version(hw, &etrack_id);

	ret = snprintf(fw_version, fw_size, "0x%08x", etrack_id);
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static int
txgbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = 15872;
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	dev_info->max_hash_mac_addrs = TXGBE_VMDQ_NUM_UC_MAC;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->max_vmdq_pools = RTE_ETH_64_POOLS;
	dev_info->vmdq_queue_num = dev_info->max_rx_queues;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;
	dev_info->rx_queue_offload_capa = txgbe_get_rx_queue_offloads(dev);
	dev_info->rx_offload_capa = (txgbe_get_rx_port_offloads(dev) |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_queue_offload_capa = txgbe_get_tx_queue_offloads(dev);
	dev_info->tx_offload_capa = txgbe_get_tx_port_offloads(dev);

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = TXGBE_DEFAULT_RX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_RX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = TXGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = TXGBE_DEFAULT_TX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_TX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = TXGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->hash_key_size = TXGBE_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = TXGBE_RSS_OFFLOAD_ALL;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_10G;
	dev_info->speed_capa |= RTE_ETH_LINK_SPEED_100M;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;

	return 0;
}

const uint32_t *
txgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	if (dev->rx_pkt_burst == txgbe_recv_pkts ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_lro_single_alloc ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_lro_bulk_alloc ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_bulk_alloc)
		return txgbe_get_supported_ptypes();

	return NULL;
}

void
txgbe_dev_setup_link_alarm_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	u32 speed;
	bool autoneg = false;

	speed = hw->phy.autoneg_advertised;
	if (!speed)
		hw->mac.get_link_capabilities(hw, &speed, &autoneg);

	hw->mac.setup_link(hw, speed, true);

	intr->flags &= ~TXGBE_FLAG_NEED_LINK_CONFIG;
}

/* return 0 means link status changed, -1 means not changed */
int
txgbe_dev_link_update_share(struct rte_eth_dev *dev,
			    int wait_to_complete)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_link link;
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	bool link_up;
	int err;
	int wait = 1;

	memset(&link, 0, sizeof(link));
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			RTE_ETH_LINK_SPEED_FIXED);

	hw->mac.get_link_status = true;

	if (intr->flags & TXGBE_FLAG_NEED_LINK_CONFIG)
		return rte_eth_linkstatus_set(dev, &link);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		wait = 0;

	err = hw->mac.check_link(hw, &link_speed, &link_up, wait);

	if (err != 0) {
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		return rte_eth_linkstatus_set(dev, &link);
	}

	if (link_up == 0) {
		if ((hw->subsystem_device_id & 0xFF) ==
				TXGBE_DEV_ID_KR_KX_KX4) {
			hw->mac.bp_down_event(hw);
		} else if (hw->phy.media_type == txgbe_media_type_fiber) {
			intr->flags |= TXGBE_FLAG_NEED_LINK_CONFIG;
			rte_eal_alarm_set(10,
				txgbe_dev_setup_link_alarm_handler, dev);
		}
		return rte_eth_linkstatus_set(dev, &link);
	} else if (!hw->dev_start) {
		return rte_eth_linkstatus_set(dev, &link);
	}

	intr->flags &= ~TXGBE_FLAG_NEED_LINK_CONFIG;
	link.link_status = RTE_ETH_LINK_UP;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case TXGBE_LINK_SPEED_UNKNOWN:
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;

	case TXGBE_LINK_SPEED_100M_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;

	case TXGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;

	case TXGBE_LINK_SPEED_2_5GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_2_5G;
		break;

	case TXGBE_LINK_SPEED_5GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_5G;
		break;

	case TXGBE_LINK_SPEED_10GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
txgbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	return txgbe_dev_link_update_share(dev, wait_to_complete);
}

static int
txgbe_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl |= (TXGBE_PSRCTL_UCP | TXGBE_PSRCTL_MCP);
	wr32(hw, TXGBE_PSRCTL, fctrl);

	return 0;
}

static int
txgbe_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl &= (~TXGBE_PSRCTL_UCP);
	if (dev->data->all_multicast == 1)
		fctrl |= TXGBE_PSRCTL_MCP;
	else
		fctrl &= (~TXGBE_PSRCTL_MCP);
	wr32(hw, TXGBE_PSRCTL, fctrl);

	return 0;
}

static int
txgbe_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl |= TXGBE_PSRCTL_MCP;
	wr32(hw, TXGBE_PSRCTL, fctrl);

	return 0;
}

static int
txgbe_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t fctrl;

	if (dev->data->promiscuous == 1)
		return 0; /* must remain in all_multicast mode */

	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl &= (~TXGBE_PSRCTL_MCP);
	wr32(hw, TXGBE_PSRCTL, fctrl);

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param on
 *  Enable or Disable.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	txgbe_dev_link_status_print(dev);
	if (on)
		intr->mask_misc |= TXGBE_ICRMISC_LSC;
	else
		intr->mask_misc &= ~TXGBE_ICRMISC_LSC;

	return 0;
}

static int
txgbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	u64 mask;

	mask = TXGBE_ICR_MASK;
	mask &= (1ULL << TXGBE_MISC_VEC_ID);
	intr->mask |= mask;
	intr->mask_misc |= TXGBE_ICRMISC_GPIO;
	intr->mask_misc |= TXGBE_ICRMISC_ANDONE;
	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	u64 mask;

	mask = TXGBE_ICR_MASK;
	mask &= ~((1ULL << TXGBE_RX_VEC_START) - 1);
	intr->mask |= mask;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	intr->mask_misc |= TXGBE_ICRMISC_LNKSEC;

	return 0;
}

/*
 * It reads ICR and sets flag (TXGBE_ICRMISC_LSC) for the link_update.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_interrupt_get_status(struct rte_eth_dev *dev,
				struct rte_intr_handle *intr_handle)
{
	uint32_t eicr;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	if (rte_intr_type_get(intr_handle) != RTE_INTR_HANDLE_UIO &&
		rte_intr_type_get(intr_handle) != RTE_INTR_HANDLE_VFIO_MSIX)
		wr32(hw, TXGBE_PX_INTA, 1);

	/* clear all cause mask */
	txgbe_disable_intr(hw);

	/* read-on-clear nic registers here */
	eicr = ((u32 *)hw->isb_mem)[TXGBE_ISB_MISC];
	PMD_DRV_LOG(DEBUG, "eicr %x", eicr);

	intr->flags = 0;

	/* set flag for async link update */
	if (eicr & TXGBE_ICRMISC_LSC)
		intr->flags |= TXGBE_FLAG_NEED_LINK_UPDATE;

	if (eicr & TXGBE_ICRMISC_ANDONE)
		intr->flags |= TXGBE_FLAG_NEED_AN_CONFIG;

	if (eicr & TXGBE_ICRMISC_VFMBX)
		intr->flags |= TXGBE_FLAG_MAILBOX;

	if (eicr & TXGBE_ICRMISC_LNKSEC)
		intr->flags |= TXGBE_FLAG_MACSEC;

	if (eicr & TXGBE_ICRMISC_GPIO)
		intr->flags |= TXGBE_FLAG_PHY_INTERRUPT;

	return 0;
}

/**
 * It gets and then prints the link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static void
txgbe_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);

	if (link.link_status) {
		PMD_INIT_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned int)link.link_speed,
			link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
					"full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down",
				(int)(dev->data->port_id));
	}
	PMD_INIT_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
}

/*
 * It executes link_update after knowing an interrupt occurred.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_interrupt_action(struct rte_eth_dev *dev,
			   struct rte_intr_handle *intr_handle)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	int64_t timeout;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_DRV_LOG(DEBUG, "intr action type %d", intr->flags);

	if (intr->flags & TXGBE_FLAG_MAILBOX) {
		txgbe_pf_mbx_process(dev);
		intr->flags &= ~TXGBE_FLAG_MAILBOX;
	}

	if (intr->flags & TXGBE_FLAG_PHY_INTERRUPT) {
		hw->phy.handle_lasi(hw);
		intr->flags &= ~TXGBE_FLAG_PHY_INTERRUPT;
	}

	if (intr->flags & TXGBE_FLAG_NEED_AN_CONFIG) {
		if (hw->devarg.auto_neg == 1 && hw->devarg.poll == 0) {
			hw->mac.kr_handle(hw);
			intr->flags &= ~TXGBE_FLAG_NEED_AN_CONFIG;
		}
	}

	if (intr->flags & TXGBE_FLAG_NEED_LINK_UPDATE) {
		struct rte_eth_link link;

		/*get the link status before link update, for predicting later*/
		rte_eth_linkstatus_get(dev, &link);

		txgbe_dev_link_update(dev, 0);

		/* likely to up */
		if (!link.link_status)
			/* handle it 1 sec later, wait it being stable */
			timeout = TXGBE_LINK_UP_CHECK_TIMEOUT;
		/* likely to down */
		else if ((hw->subsystem_device_id & 0xFF) ==
				TXGBE_DEV_ID_KR_KX_KX4 &&
				hw->devarg.auto_neg == 1)
			/* handle it 2 sec later for backplane AN73 */
			timeout = 2000;
		else
			/* handle it 4 sec later, wait it being stable */
			timeout = TXGBE_LINK_DOWN_CHECK_TIMEOUT;

		txgbe_dev_link_status_print(dev);
		if (rte_eal_alarm_set(timeout * 1000,
				      txgbe_dev_interrupt_delayed_handler,
				      (void *)dev) < 0) {
			PMD_DRV_LOG(ERR, "Error setting alarm");
		} else {
			/* only disable lsc interrupt */
			intr->mask_misc &= ~TXGBE_ICRMISC_LSC;

			intr->mask_orig = intr->mask;
			/* only disable all misc interrupts */
			intr->mask &= ~(1ULL << TXGBE_MISC_VEC_ID);
		}
	}

	PMD_DRV_LOG(DEBUG, "enable intr immediately");
	txgbe_enable_intr(dev);
	rte_intr_enable(intr_handle);

	return 0;
}

/**
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the
 * NIC interrupt state is not stable for txgbe after link is just down,
 * it needs to wait 4 seconds to get the stable status.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
txgbe_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t eicr;

	txgbe_disable_intr(hw);

	eicr = ((u32 *)hw->isb_mem)[TXGBE_ISB_MISC];
	if (eicr & TXGBE_ICRMISC_VFMBX)
		txgbe_pf_mbx_process(dev);

	if (intr->flags & TXGBE_FLAG_PHY_INTERRUPT) {
		hw->phy.handle_lasi(hw);
		intr->flags &= ~TXGBE_FLAG_PHY_INTERRUPT;
	}

	if (intr->flags & TXGBE_FLAG_NEED_LINK_UPDATE) {
		txgbe_dev_link_update(dev, 0);
		intr->flags &= ~TXGBE_FLAG_NEED_LINK_UPDATE;
		txgbe_dev_link_status_print(dev);
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC,
					      NULL);
	}

	if (intr->flags & TXGBE_FLAG_MACSEC) {
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_MACSEC,
					      NULL);
		intr->flags &= ~TXGBE_FLAG_MACSEC;
	}

	/* restore original mask */
	intr->mask_misc |= TXGBE_ICRMISC_LSC;

	intr->mask = intr->mask_orig;
	intr->mask_orig = 0;

	PMD_DRV_LOG(DEBUG, "enable intr in delayed handler S[%08x]", eicr);
	txgbe_enable_intr(dev);
	rte_intr_enable(intr_handle);
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
txgbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	txgbe_dev_interrupt_get_status(dev, dev->intr_handle);
	txgbe_dev_interrupt_action(dev, dev->intr_handle);
}

static int
txgbe_dev_led_on(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;

	hw = TXGBE_DEV_HW(dev);
	return txgbe_led_on(hw, TXGBE_LEDCTL_ACTIVE) == 0 ? 0 : -ENOTSUP;
}

static int
txgbe_dev_led_off(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;

	hw = TXGBE_DEV_HW(dev);
	return txgbe_led_off(hw, TXGBE_LEDCTL_ACTIVE) == 0 ? 0 : -ENOTSUP;
}

static int
txgbe_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct txgbe_hw *hw;
	uint32_t mflcn_reg;
	uint32_t fccfg_reg;
	int rx_pause;
	int tx_pause;

	hw = TXGBE_DEV_HW(dev);

	fc_conf->pause_time = hw->fc.pause_time;
	fc_conf->high_water = hw->fc.high_water[0];
	fc_conf->low_water = hw->fc.low_water[0];
	fc_conf->send_xon = hw->fc.send_xon;
	fc_conf->autoneg = !hw->fc.disable_fc_autoneg;

	/*
	 * Return rx_pause status according to actual setting of
	 * RXFCCFG register.
	 */
	mflcn_reg = rd32(hw, TXGBE_RXFCCFG);
	if (mflcn_reg & (TXGBE_RXFCCFG_FC | TXGBE_RXFCCFG_PFC))
		rx_pause = 1;
	else
		rx_pause = 0;

	/*
	 * Return tx_pause status according to actual setting of
	 * TXFCCFG register.
	 */
	fccfg_reg = rd32(hw, TXGBE_TXFCCFG);
	if (fccfg_reg & (TXGBE_TXFCCFG_FC | TXGBE_TXFCCFG_PFC))
		tx_pause = 1;
	else
		tx_pause = 0;

	if (rx_pause && tx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else if (tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	return 0;
}

static int
txgbe_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct txgbe_hw *hw;
	int err;
	uint32_t rx_buf_size;
	uint32_t max_high_water;
	enum txgbe_fc_mode rte_fcmode_2_txgbe_fcmode[] = {
		txgbe_fc_none,
		txgbe_fc_rx_pause,
		txgbe_fc_tx_pause,
		txgbe_fc_full
	};

	PMD_INIT_FUNC_TRACE();

	hw = TXGBE_DEV_HW(dev);
	rx_buf_size = rd32(hw, TXGBE_PBRXSIZE(0));
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x", rx_buf_size);

	/*
	 * At least reserve one Ethernet frame for watermark
	 * high_water/low_water in kilo bytes for txgbe
	 */
	max_high_water = (rx_buf_size - RTE_ETHER_MAX_LEN) >> 10;
	if (fc_conf->high_water > max_high_water ||
	    fc_conf->high_water < fc_conf->low_water) {
		PMD_INIT_LOG(ERR, "Invalid high/low water setup value in KB");
		PMD_INIT_LOG(ERR, "High_water must <= 0x%x", max_high_water);
		return -EINVAL;
	}

	hw->fc.requested_mode = rte_fcmode_2_txgbe_fcmode[fc_conf->mode];
	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water[0]  = fc_conf->high_water;
	hw->fc.low_water[0]   = fc_conf->low_water;
	hw->fc.send_xon       = fc_conf->send_xon;
	hw->fc.disable_fc_autoneg = !fc_conf->autoneg;

	err = txgbe_fc_enable(hw);

	/* Not negotiated is not an error case */
	if (err == 0 || err == TXGBE_ERR_FC_NOT_NEGOTIATED) {
		wr32m(hw, TXGBE_MACRXFLT, TXGBE_MACRXFLT_CTL_MASK,
		      (fc_conf->mac_ctrl_frame_fwd
		       ? TXGBE_MACRXFLT_CTL_NOPS : TXGBE_MACRXFLT_CTL_DROP));
		txgbe_flush(hw);

		return 0;
	}

	PMD_INIT_LOG(ERR, "txgbe_fc_enable = 0x%x", err);
	return -EIO;
}

static int
txgbe_priority_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_pfc_conf *pfc_conf)
{
	int err;
	uint32_t rx_buf_size;
	uint32_t max_high_water;
	uint8_t tc_num;
	uint8_t  map[TXGBE_DCB_UP_MAX] = { 0 };
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_dcb_config *dcb_config = TXGBE_DEV_DCB_CONFIG(dev);

	enum txgbe_fc_mode rte_fcmode_2_txgbe_fcmode[] = {
		txgbe_fc_none,
		txgbe_fc_rx_pause,
		txgbe_fc_tx_pause,
		txgbe_fc_full
	};

	PMD_INIT_FUNC_TRACE();

	txgbe_dcb_unpack_map_cee(dcb_config, TXGBE_DCB_RX_CONFIG, map);
	tc_num = map[pfc_conf->priority];
	rx_buf_size = rd32(hw, TXGBE_PBRXSIZE(tc_num));
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x", rx_buf_size);
	/*
	 * At least reserve one Ethernet frame for watermark
	 * high_water/low_water in kilo bytes for txgbe
	 */
	max_high_water = (rx_buf_size - RTE_ETHER_MAX_LEN) >> 10;
	if (pfc_conf->fc.high_water > max_high_water ||
	    pfc_conf->fc.high_water <= pfc_conf->fc.low_water) {
		PMD_INIT_LOG(ERR, "Invalid high/low water setup value in KB");
		PMD_INIT_LOG(ERR, "High_water must <= 0x%x", max_high_water);
		return -EINVAL;
	}

	hw->fc.requested_mode = rte_fcmode_2_txgbe_fcmode[pfc_conf->fc.mode];
	hw->fc.pause_time = pfc_conf->fc.pause_time;
	hw->fc.send_xon = pfc_conf->fc.send_xon;
	hw->fc.low_water[tc_num] =  pfc_conf->fc.low_water;
	hw->fc.high_water[tc_num] = pfc_conf->fc.high_water;

	err = txgbe_dcb_pfc_enable(hw, tc_num);

	/* Not negotiated is not an error case */
	if (err == 0 || err == TXGBE_ERR_FC_NOT_NEGOTIATED)
		return 0;

	PMD_INIT_LOG(ERR, "txgbe_dcb_pfc_enable = 0x%x", err);
	return -EIO;
}

int
txgbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_reta_entry64 *reta_conf,
			  uint16_t reta_size)
{
	uint8_t i, j, mask;
	uint32_t reta;
	uint16_t idx, shift;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	if (!txgbe_rss_update_sp(hw->mac.type)) {
		PMD_DRV_LOG(ERR, "RSS reta update is not supported on this "
			"NIC.");
		return -ENOTSUP;
	}

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += 4) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)RS64(reta_conf[idx].mask, shift, 0xF);
		if (!mask)
			continue;

		reta = rd32at(hw, TXGBE_REG_RSSTBL, i >> 2);
		for (j = 0; j < 4; j++) {
			if (RS8(mask, j, 0x1)) {
				reta  &= ~(MS32(8 * j, 0xFF));
				reta |= LS32(reta_conf[idx].reta[shift + j],
						8 * j, 0xFF);
			}
		}
		wr32at(hw, TXGBE_REG_RSSTBL, i >> 2, reta);
	}
	adapter->rss_reta_updated = 1;

	return 0;
}

int
txgbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint8_t i, j, mask;
	uint32_t reta;
	uint16_t idx, shift;

	PMD_INIT_FUNC_TRACE();

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += 4) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)RS64(reta_conf[idx].mask, shift, 0xF);
		if (!mask)
			continue;

		reta = rd32at(hw, TXGBE_REG_RSSTBL, i >> 2);
		for (j = 0; j < 4; j++) {
			if (RS8(mask, j, 0x1))
				reta_conf[idx].reta[shift + j] =
					(uint16_t)RS32(reta, 8 * j, 0xFF);
		}
	}

	return 0;
}

static int
txgbe_add_rar(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
				uint32_t index, uint32_t pool)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t enable_addr = 1;

	return txgbe_set_rar(hw, index, mac_addr->addr_bytes,
			     pool, enable_addr);
}

static void
txgbe_remove_rar(struct rte_eth_dev *dev, uint32_t index)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	txgbe_clear_rar(hw, index);
}

static int
txgbe_set_default_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	txgbe_remove_rar(dev, 0);
	txgbe_add_rar(dev, addr, 0, pci_dev->max_vfs);

	return 0;
}

static int
txgbe_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t frame_size = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	struct rte_eth_dev_data *dev_data = dev->data;

	/* If device is started, refuse mtu that requires the support of
	 * scattered packets when this feature has not been enabled before.
	 */
	if (dev_data->dev_started && !dev_data->scattered_rx &&
	    (frame_size + 2 * RTE_VLAN_HLEN >
	     dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)) {
		PMD_INIT_LOG(ERR, "Stop port first.");
		return -EINVAL;
	}

	if (hw->mode)
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRAME_SIZE_MAX);
	else
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(frame_size));

	return 0;
}

static uint32_t
txgbe_uta_vector(struct txgbe_hw *hw, struct rte_ether_addr *uc_addr)
{
	uint32_t vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 4) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 3) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 2) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((uc_addr->addr_bytes[4]) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

static int
txgbe_uc_hash_table_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *mac_addr, uint8_t on)
{
	uint32_t vector;
	uint32_t uta_idx;
	uint32_t reg_val;
	uint32_t uta_mask;
	uint32_t psrctl;

	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(dev);

	/* The UTA table only exists on pf hardware */
	if (hw->mac.type < txgbe_mac_raptor)
		return -ENOTSUP;

	vector = txgbe_uta_vector(hw, mac_addr);
	uta_idx = (vector >> 5) & 0x7F;
	uta_mask = 0x1UL << (vector & 0x1F);

	if (!!on == !!(uta_info->uta_shadow[uta_idx] & uta_mask))
		return 0;

	reg_val = rd32(hw, TXGBE_UCADDRTBL(uta_idx));
	if (on) {
		uta_info->uta_in_use++;
		reg_val |= uta_mask;
		uta_info->uta_shadow[uta_idx] |= uta_mask;
	} else {
		uta_info->uta_in_use--;
		reg_val &= ~uta_mask;
		uta_info->uta_shadow[uta_idx] &= ~uta_mask;
	}

	wr32(hw, TXGBE_UCADDRTBL(uta_idx), reg_val);

	psrctl = rd32(hw, TXGBE_PSRCTL);
	if (uta_info->uta_in_use > 0)
		psrctl |= TXGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~TXGBE_PSRCTL_UCHFENA;

	psrctl &= ~TXGBE_PSRCTL_ADHF12_MASK;
	psrctl |= TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, TXGBE_PSRCTL, psrctl);

	return 0;
}

static int
txgbe_uc_all_hash_table_set(struct rte_eth_dev *dev, uint8_t on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(dev);
	uint32_t psrctl;
	int i;

	/* The UTA table only exists on pf hardware */
	if (hw->mac.type < txgbe_mac_raptor)
		return -ENOTSUP;

	if (on) {
		for (i = 0; i < RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = ~0;
			wr32(hw, TXGBE_UCADDRTBL(i), ~0);
		}
	} else {
		for (i = 0; i < RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = 0;
			wr32(hw, TXGBE_UCADDRTBL(i), 0);
		}
	}

	psrctl = rd32(hw, TXGBE_PSRCTL);
	if (on)
		psrctl |= TXGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~TXGBE_PSRCTL_UCHFENA;

	psrctl &= ~TXGBE_PSRCTL_ADHF12_MASK;
	psrctl |= TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, TXGBE_PSRCTL, psrctl);

	return 0;
}

uint32_t
txgbe_convert_vm_rx_mask_to_val(uint16_t rx_mask, uint32_t orig_val)
{
	uint32_t new_val = orig_val;

	if (rx_mask & RTE_ETH_VMDQ_ACCEPT_UNTAG)
		new_val |= TXGBE_POOLETHCTL_UTA;
	if (rx_mask & RTE_ETH_VMDQ_ACCEPT_HASH_MC)
		new_val |= TXGBE_POOLETHCTL_MCHA;
	if (rx_mask & RTE_ETH_VMDQ_ACCEPT_HASH_UC)
		new_val |= TXGBE_POOLETHCTL_UCHA;
	if (rx_mask & RTE_ETH_VMDQ_ACCEPT_BROADCAST)
		new_val |= TXGBE_POOLETHCTL_BCA;
	if (rx_mask & RTE_ETH_VMDQ_ACCEPT_MULTICAST)
		new_val |= TXGBE_POOLETHCTL_MCP;

	return new_val;
}

static int
txgbe_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t mask;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (queue_id < 32) {
		mask = rd32(hw, TXGBE_IMS(0));
		mask &= (1 << queue_id);
		wr32(hw, TXGBE_IMS(0), mask);
	} else if (queue_id < 64) {
		mask = rd32(hw, TXGBE_IMS(1));
		mask &= (1 << (queue_id - 32));
		wr32(hw, TXGBE_IMS(1), mask);
	}
	rte_intr_enable(intr_handle);

	return 0;
}

static int
txgbe_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	uint32_t mask;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (queue_id < 32) {
		mask = rd32(hw, TXGBE_IMS(0));
		mask &= ~(1 << queue_id);
		wr32(hw, TXGBE_IMS(0), mask);
	} else if (queue_id < 64) {
		mask = rd32(hw, TXGBE_IMS(1));
		mask &= ~(1 << (queue_id - 32));
		wr32(hw, TXGBE_IMS(1), mask);
	}

	return 0;
}

/**
 * set the IVAR registers, mapping interrupt causes to vectors
 * @param hw
 *  pointer to txgbe_hw struct
 * @direction
 *  0 for Rx, 1 for Tx, -1 for other causes
 * @queue
 *  queue to map the corresponding interrupt to
 * @msix_vector
 *  the vector to map to the corresponding queue
 */
void
txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
		   uint8_t queue, uint8_t msix_vector)
{
	uint32_t tmp, idx;

	if (direction == -1) {
		/* other causes */
		msix_vector |= TXGBE_IVARMISC_VLD;
		idx = 0;
		tmp = rd32(hw, TXGBE_IVARMISC);
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, TXGBE_IVARMISC, tmp);
	} else {
		/* rx or tx causes */
		/* Workaround for ICR lost */
		idx = ((16 * (queue & 1)) + (8 * direction));
		tmp = rd32(hw, TXGBE_IVAR(queue >> 1));
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, TXGBE_IVAR(queue >> 1), tmp);
	}
}

/**
 * Sets up the hardware to properly generate MSI-X interrupts
 * @hw
 *  board private structure
 */
static void
txgbe_configure_msix(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t queue_id, base = TXGBE_MISC_VEC_ID;
	uint32_t vec = TXGBE_MISC_VEC_ID;
	uint32_t gpie;

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd
	 * but if misx has been enabled already, need to configure
	 * auto clean, auto mask and throttling.
	 */
	gpie = rd32(hw, TXGBE_GPIE);
	if (!rte_intr_dp_is_en(intr_handle) &&
	    !(gpie & TXGBE_GPIE_MSIX))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = TXGBE_RX_VEC_START;
		vec = base;
	}

	/* setup GPIE for MSI-x mode */
	gpie = rd32(hw, TXGBE_GPIE);
	gpie |= TXGBE_GPIE_MSIX;
	wr32(hw, TXGBE_GPIE, gpie);

	/* Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	if (rte_intr_dp_is_en(intr_handle)) {
		for (queue_id = 0; queue_id < dev->data->nb_rx_queues;
			queue_id++) {
			/* by default, 1:1 mapping */
			txgbe_set_ivar_map(hw, 0, queue_id, vec);
			rte_intr_vec_list_index_set(intr_handle,
							   queue_id, vec);
			if (vec < base + rte_intr_nb_efd_get(intr_handle)
			    - 1)
				vec++;
		}

		txgbe_set_ivar_map(hw, -1, 1, TXGBE_MISC_VEC_ID);
	}
	wr32(hw, TXGBE_ITR(TXGBE_MISC_VEC_ID),
			TXGBE_ITR_IVAL_10G(TXGBE_QUEUE_ITR_INTERVAL_DEFAULT)
			| TXGBE_ITR_WRDSA);
}

int
txgbe_set_queue_rate_limit(struct rte_eth_dev *dev,
			   uint16_t queue_idx, uint16_t tx_rate)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t bcnrc_val;

	if (queue_idx >= hw->mac.max_tx_queues)
		return -EINVAL;

	if (tx_rate != 0) {
		bcnrc_val = TXGBE_ARBTXRATE_MAX(tx_rate);
		bcnrc_val |= TXGBE_ARBTXRATE_MIN(tx_rate / 2);
	} else {
		bcnrc_val = 0;
	}

	/*
	 * Set global transmit compensation time to the MMW_SIZE in ARBTXMMW
	 * register. MMW_SIZE=0x014 if 9728-byte jumbo is supported.
	 */
	wr32(hw, TXGBE_ARBTXMMW, 0x14);

	/* Set ARBTXRATE of queue X */
	wr32(hw, TXGBE_ARBPOOLIDX, queue_idx);
	wr32(hw, TXGBE_ARBTXRATE, bcnrc_val);
	txgbe_flush(hw);

	return 0;
}

int
txgbe_syn_filter_set(struct rte_eth_dev *dev,
			struct rte_eth_syn_filter *filter,
			bool add)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	uint32_t syn_info;
	uint32_t synqf;

	if (filter->queue >= TXGBE_MAX_RX_QUEUE_NUM)
		return -EINVAL;

	syn_info = filter_info->syn_info;

	if (add) {
		if (syn_info & TXGBE_SYNCLS_ENA)
			return -EINVAL;
		synqf = (uint32_t)TXGBE_SYNCLS_QPID(filter->queue);
		synqf |= TXGBE_SYNCLS_ENA;

		if (filter->hig_pri)
			synqf |= TXGBE_SYNCLS_HIPRIO;
		else
			synqf &= ~TXGBE_SYNCLS_HIPRIO;
	} else {
		synqf = rd32(hw, TXGBE_SYNCLS);
		if (!(syn_info & TXGBE_SYNCLS_ENA))
			return -ENOENT;
		synqf &= ~(TXGBE_SYNCLS_QPID_MASK | TXGBE_SYNCLS_ENA);
	}

	filter_info->syn_info = synqf;
	wr32(hw, TXGBE_SYNCLS, synqf);
	txgbe_flush(hw);
	return 0;
}

static inline enum txgbe_5tuple_protocol
convert_protocol_type(uint8_t protocol_value)
{
	if (protocol_value == IPPROTO_TCP)
		return TXGBE_5TF_PROT_TCP;
	else if (protocol_value == IPPROTO_UDP)
		return TXGBE_5TF_PROT_UDP;
	else if (protocol_value == IPPROTO_SCTP)
		return TXGBE_5TF_PROT_SCTP;
	else
		return TXGBE_5TF_PROT_NONE;
}

/* inject a 5-tuple filter to HW */
static inline void
txgbe_inject_5tuple_filter(struct rte_eth_dev *dev,
			   struct txgbe_5tuple_filter *filter)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int i;
	uint32_t ftqf, sdpqf;
	uint32_t l34timir = 0;
	uint32_t mask = TXGBE_5TFCTL0_MASK;

	i = filter->index;
	sdpqf = TXGBE_5TFPORT_DST(be_to_le16(filter->filter_info.dst_port));
	sdpqf |= TXGBE_5TFPORT_SRC(be_to_le16(filter->filter_info.src_port));

	ftqf = TXGBE_5TFCTL0_PROTO(filter->filter_info.proto);
	ftqf |= TXGBE_5TFCTL0_PRI(filter->filter_info.priority);
	if (filter->filter_info.src_ip_mask == 0) /* 0 means compare. */
		mask &= ~TXGBE_5TFCTL0_MSADDR;
	if (filter->filter_info.dst_ip_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MDADDR;
	if (filter->filter_info.src_port_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MSPORT;
	if (filter->filter_info.dst_port_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MDPORT;
	if (filter->filter_info.proto_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MPROTO;
	ftqf |= mask;
	ftqf |= TXGBE_5TFCTL0_MPOOL;
	ftqf |= TXGBE_5TFCTL0_ENA;

	wr32(hw, TXGBE_5TFDADDR(i), be_to_le32(filter->filter_info.dst_ip));
	wr32(hw, TXGBE_5TFSADDR(i), be_to_le32(filter->filter_info.src_ip));
	wr32(hw, TXGBE_5TFPORT(i), sdpqf);
	wr32(hw, TXGBE_5TFCTL0(i), ftqf);

	l34timir |= TXGBE_5TFCTL1_QP(filter->queue);
	wr32(hw, TXGBE_5TFCTL1(i), l34timir);
}

/*
 * add a 5tuple filter
 *
 * @param
 * dev: Pointer to struct rte_eth_dev.
 * index: the index the filter allocates.
 * filter: pointer to the filter that will be added.
 * rx_queue: the queue id the filter assigned to.
 *
 * @return
 *    - On success, zero.
 *    - On failure, a negative value.
 */
static int
txgbe_add_5tuple_filter(struct rte_eth_dev *dev,
			struct txgbe_5tuple_filter *filter)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	int i, idx, shift;

	/*
	 * look for an unused 5tuple filter index,
	 * and insert the filter to list.
	 */
	for (i = 0; i < TXGBE_MAX_FTQF_FILTERS; i++) {
		idx = i / (sizeof(uint32_t) * NBBY);
		shift = i % (sizeof(uint32_t) * NBBY);
		if (!(filter_info->fivetuple_mask[idx] & (1 << shift))) {
			filter_info->fivetuple_mask[idx] |= 1 << shift;
			filter->index = i;
			TAILQ_INSERT_TAIL(&filter_info->fivetuple_list,
					  filter,
					  entries);
			break;
		}
	}
	if (i >= TXGBE_MAX_FTQF_FILTERS) {
		PMD_DRV_LOG(ERR, "5tuple filters are full.");
		return -ENOSYS;
	}

	txgbe_inject_5tuple_filter(dev, filter);

	return 0;
}

/*
 * remove a 5tuple filter
 *
 * @param
 * dev: Pointer to struct rte_eth_dev.
 * filter: the pointer of the filter will be removed.
 */
static void
txgbe_remove_5tuple_filter(struct rte_eth_dev *dev,
			struct txgbe_5tuple_filter *filter)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	uint16_t index = filter->index;

	filter_info->fivetuple_mask[index / (sizeof(uint32_t) * NBBY)] &=
				~(1 << (index % (sizeof(uint32_t) * NBBY)));
	TAILQ_REMOVE(&filter_info->fivetuple_list, filter, entries);
	rte_free(filter);

	wr32(hw, TXGBE_5TFDADDR(index), 0);
	wr32(hw, TXGBE_5TFSADDR(index), 0);
	wr32(hw, TXGBE_5TFPORT(index), 0);
	wr32(hw, TXGBE_5TFCTL0(index), 0);
	wr32(hw, TXGBE_5TFCTL1(index), 0);
}

static inline struct txgbe_5tuple_filter *
txgbe_5tuple_filter_lookup(struct txgbe_5tuple_filter_list *filter_list,
			struct txgbe_5tuple_filter_info *key)
{
	struct txgbe_5tuple_filter *it;

	TAILQ_FOREACH(it, filter_list, entries) {
		if (memcmp(key, &it->filter_info,
			sizeof(struct txgbe_5tuple_filter_info)) == 0) {
			return it;
		}
	}
	return NULL;
}

/* translate elements in struct rte_eth_ntuple_filter
 * to struct txgbe_5tuple_filter_info
 */
static inline int
ntuple_filter_to_5tuple(struct rte_eth_ntuple_filter *filter,
			struct txgbe_5tuple_filter_info *filter_info)
{
	if (filter->queue >= TXGBE_MAX_RX_QUEUE_NUM ||
		filter->priority > TXGBE_5TUPLE_MAX_PRI ||
		filter->priority < TXGBE_5TUPLE_MIN_PRI)
		return -EINVAL;

	switch (filter->dst_ip_mask) {
	case UINT32_MAX:
		filter_info->dst_ip_mask = 0;
		filter_info->dst_ip = filter->dst_ip;
		break;
	case 0:
		filter_info->dst_ip_mask = 1;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid dst_ip mask.");
		return -EINVAL;
	}

	switch (filter->src_ip_mask) {
	case UINT32_MAX:
		filter_info->src_ip_mask = 0;
		filter_info->src_ip = filter->src_ip;
		break;
	case 0:
		filter_info->src_ip_mask = 1;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid src_ip mask.");
		return -EINVAL;
	}

	switch (filter->dst_port_mask) {
	case UINT16_MAX:
		filter_info->dst_port_mask = 0;
		filter_info->dst_port = filter->dst_port;
		break;
	case 0:
		filter_info->dst_port_mask = 1;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid dst_port mask.");
		return -EINVAL;
	}

	switch (filter->src_port_mask) {
	case UINT16_MAX:
		filter_info->src_port_mask = 0;
		filter_info->src_port = filter->src_port;
		break;
	case 0:
		filter_info->src_port_mask = 1;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid src_port mask.");
		return -EINVAL;
	}

	switch (filter->proto_mask) {
	case UINT8_MAX:
		filter_info->proto_mask = 0;
		filter_info->proto =
			convert_protocol_type(filter->proto);
		break;
	case 0:
		filter_info->proto_mask = 1;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid protocol mask.");
		return -EINVAL;
	}

	filter_info->priority = (uint8_t)filter->priority;
	return 0;
}

/*
 * add or delete a ntuple filter
 *
 * @param
 * dev: Pointer to struct rte_eth_dev.
 * ntuple_filter: Pointer to struct rte_eth_ntuple_filter
 * add: if true, add filter, if false, remove filter
 *
 * @return
 *    - On success, zero.
 *    - On failure, a negative value.
 */
int
txgbe_add_del_ntuple_filter(struct rte_eth_dev *dev,
			struct rte_eth_ntuple_filter *ntuple_filter,
			bool add)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	struct txgbe_5tuple_filter_info filter_5tuple;
	struct txgbe_5tuple_filter *filter;
	int ret;

	if (ntuple_filter->flags != RTE_5TUPLE_FLAGS) {
		PMD_DRV_LOG(ERR, "only 5tuple is supported.");
		return -EINVAL;
	}

	memset(&filter_5tuple, 0, sizeof(struct txgbe_5tuple_filter_info));
	ret = ntuple_filter_to_5tuple(ntuple_filter, &filter_5tuple);
	if (ret < 0)
		return ret;

	filter = txgbe_5tuple_filter_lookup(&filter_info->fivetuple_list,
					 &filter_5tuple);
	if (filter != NULL && add) {
		PMD_DRV_LOG(ERR, "filter exists.");
		return -EEXIST;
	}
	if (filter == NULL && !add) {
		PMD_DRV_LOG(ERR, "filter doesn't exist.");
		return -ENOENT;
	}

	if (add) {
		filter = rte_zmalloc("txgbe_5tuple_filter",
				sizeof(struct txgbe_5tuple_filter), 0);
		if (filter == NULL)
			return -ENOMEM;
		rte_memcpy(&filter->filter_info,
				 &filter_5tuple,
				 sizeof(struct txgbe_5tuple_filter_info));
		filter->queue = ntuple_filter->queue;
		ret = txgbe_add_5tuple_filter(dev, filter);
		if (ret < 0) {
			rte_free(filter);
			return ret;
		}
	} else {
		txgbe_remove_5tuple_filter(dev, filter);
	}

	return 0;
}

int
txgbe_add_del_ethertype_filter(struct rte_eth_dev *dev,
			struct rte_eth_ethertype_filter *filter,
			bool add)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	uint32_t etqf = 0;
	uint32_t etqs = 0;
	int ret;
	struct txgbe_ethertype_filter ethertype_filter;

	if (filter->queue >= TXGBE_MAX_RX_QUEUE_NUM)
		return -EINVAL;

	if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
	    filter->ether_type == RTE_ETHER_TYPE_IPV6) {
		PMD_DRV_LOG(ERR, "unsupported ether_type(0x%04x) in"
			" ethertype filter.", filter->ether_type);
		return -EINVAL;
	}

	if (filter->flags & RTE_ETHTYPE_FLAGS_MAC) {
		PMD_DRV_LOG(ERR, "mac compare is unsupported.");
		return -EINVAL;
	}
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP) {
		PMD_DRV_LOG(ERR, "drop option is unsupported.");
		return -EINVAL;
	}

	ret = txgbe_ethertype_filter_lookup(filter_info, filter->ether_type);
	if (ret >= 0 && add) {
		PMD_DRV_LOG(ERR, "ethertype (0x%04x) filter exists.",
			    filter->ether_type);
		return -EEXIST;
	}
	if (ret < 0 && !add) {
		PMD_DRV_LOG(ERR, "ethertype (0x%04x) filter doesn't exist.",
			    filter->ether_type);
		return -ENOENT;
	}

	if (add) {
		etqf = TXGBE_ETFLT_ENA;
		etqf |= TXGBE_ETFLT_ETID(filter->ether_type);
		etqs |= TXGBE_ETCLS_QPID(filter->queue);
		etqs |= TXGBE_ETCLS_QENA;

		ethertype_filter.ethertype = filter->ether_type;
		ethertype_filter.etqf = etqf;
		ethertype_filter.etqs = etqs;
		ethertype_filter.conf = FALSE;
		ret = txgbe_ethertype_filter_insert(filter_info,
						    &ethertype_filter);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "ethertype filters are full.");
			return -ENOSPC;
		}
	} else {
		ret = txgbe_ethertype_filter_remove(filter_info, (uint8_t)ret);
		if (ret < 0)
			return -ENOSYS;
	}
	wr32(hw, TXGBE_ETFLT(ret), etqf);
	wr32(hw, TXGBE_ETCLS(ret), etqs);
	txgbe_flush(hw);

	return 0;
}

static int
txgbe_dev_flow_ops_get(__rte_unused struct rte_eth_dev *dev,
		       const struct rte_flow_ops **ops)
{
	*ops = &txgbe_flow_ops;
	return 0;
}

static u8 *
txgbe_dev_addr_list_itr(__rte_unused struct txgbe_hw *hw,
			u8 **mc_addr_ptr, u32 *vmdq)
{
	u8 *mc_addr;

	*vmdq = 0;
	mc_addr = *mc_addr_ptr;
	*mc_addr_ptr = (mc_addr + sizeof(struct rte_ether_addr));
	return mc_addr;
}

int
txgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct txgbe_hw *hw;
	u8 *mc_addr_list;

	hw = TXGBE_DEV_HW(dev);
	mc_addr_list = (u8 *)mc_addr_set;
	return hw->mac.update_mc_addr_list(hw, mc_addr_list, nb_mc_addr,
					 txgbe_dev_addr_list_itr, TRUE);
}

static uint64_t
txgbe_read_systime_cyclecounter(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint64_t systime_cycles;

	systime_cycles = (uint64_t)rd32(hw, TXGBE_TSTIMEL);
	systime_cycles |= (uint64_t)rd32(hw, TXGBE_TSTIMEH) << 32;

	return systime_cycles;
}

static uint64_t
txgbe_read_rx_tstamp_cyclecounter(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint64_t rx_tstamp_cycles;

	/* TSRXSTMPL stores ns and TSRXSTMPH stores seconds. */
	rx_tstamp_cycles = (uint64_t)rd32(hw, TXGBE_TSRXSTMPL);
	rx_tstamp_cycles |= (uint64_t)rd32(hw, TXGBE_TSRXSTMPH) << 32;

	return rx_tstamp_cycles;
}

static uint64_t
txgbe_read_tx_tstamp_cyclecounter(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint64_t tx_tstamp_cycles;

	/* TSTXSTMPL stores ns and TSTXSTMPH stores seconds. */
	tx_tstamp_cycles = (uint64_t)rd32(hw, TXGBE_TSTXSTMPL);
	tx_tstamp_cycles |= (uint64_t)rd32(hw, TXGBE_TSTXSTMPH) << 32;

	return tx_tstamp_cycles;
}

static void
txgbe_start_timecounters(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct rte_eth_link link;
	uint32_t incval = 0;
	uint32_t shift = 0;

	/* Get current link speed. */
	txgbe_dev_link_update(dev, 1);
	rte_eth_linkstatus_get(dev, &link);

	switch (link.link_speed) {
	case RTE_ETH_SPEED_NUM_100M:
		incval = TXGBE_INCVAL_100;
		shift = TXGBE_INCVAL_SHIFT_100;
		break;
	case RTE_ETH_SPEED_NUM_1G:
		incval = TXGBE_INCVAL_1GB;
		shift = TXGBE_INCVAL_SHIFT_1GB;
		break;
	case RTE_ETH_SPEED_NUM_10G:
	default:
		incval = TXGBE_INCVAL_10GB;
		shift = TXGBE_INCVAL_SHIFT_10GB;
		break;
	}

	wr32(hw, TXGBE_TSTIMEINC, TXGBE_TSTIMEINC_VP(incval, 2));

	memset(&adapter->systime_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	adapter->systime_tc.cc_mask = TXGBE_CYCLECOUNTER_MASK;
	adapter->systime_tc.cc_shift = shift;
	adapter->systime_tc.nsec_mask = (1ULL << shift) - 1;

	adapter->rx_tstamp_tc.cc_mask = TXGBE_CYCLECOUNTER_MASK;
	adapter->rx_tstamp_tc.cc_shift = shift;
	adapter->rx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;

	adapter->tx_tstamp_tc.cc_mask = TXGBE_CYCLECOUNTER_MASK;
	adapter->tx_tstamp_tc.cc_shift = shift;
	adapter->tx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;
}

static int
txgbe_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	adapter->systime_tc.nsec += delta;
	adapter->rx_tstamp_tc.nsec += delta;
	adapter->tx_tstamp_tc.nsec += delta;

	return 0;
}

static int
txgbe_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	uint64_t ns;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	ns = rte_timespec_to_ns(ts);
	/* Set the timecounters to a new value. */
	adapter->systime_tc.nsec = ns;
	adapter->rx_tstamp_tc.nsec = ns;
	adapter->tx_tstamp_tc.nsec = ns;

	return 0;
}

static int
txgbe_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
	uint64_t ns, systime_cycles;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	systime_cycles = txgbe_read_systime_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->systime_tc, systime_cycles);
	*ts = rte_ns_to_timespec(ns);

	return 0;
}

static int
txgbe_timesync_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t tsync_ctl;

	/* Stop the timesync system time. */
	wr32(hw, TXGBE_TSTIMEINC, 0x0);
	/* Reset the timesync system time value. */
	wr32(hw, TXGBE_TSTIMEL, 0x0);
	wr32(hw, TXGBE_TSTIMEH, 0x0);

	txgbe_start_timecounters(dev);

	/* Enable L2 filtering of IEEE1588/802.1AS Ethernet frame types. */
	wr32(hw, TXGBE_ETFLT(TXGBE_ETF_ID_1588),
		RTE_ETHER_TYPE_1588 | TXGBE_ETFLT_ENA | TXGBE_ETFLT_1588);

	/* Enable timestamping of received PTP packets. */
	tsync_ctl = rd32(hw, TXGBE_TSRXCTL);
	tsync_ctl |= TXGBE_TSRXCTL_ENA;
	wr32(hw, TXGBE_TSRXCTL, tsync_ctl);

	/* Enable timestamping of transmitted PTP packets. */
	tsync_ctl = rd32(hw, TXGBE_TSTXCTL);
	tsync_ctl |= TXGBE_TSTXCTL_ENA;
	wr32(hw, TXGBE_TSTXCTL, tsync_ctl);

	txgbe_flush(hw);

	return 0;
}

static int
txgbe_timesync_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t tsync_ctl;

	/* Disable timestamping of transmitted PTP packets. */
	tsync_ctl = rd32(hw, TXGBE_TSTXCTL);
	tsync_ctl &= ~TXGBE_TSTXCTL_ENA;
	wr32(hw, TXGBE_TSTXCTL, tsync_ctl);

	/* Disable timestamping of received PTP packets. */
	tsync_ctl = rd32(hw, TXGBE_TSRXCTL);
	tsync_ctl &= ~TXGBE_TSRXCTL_ENA;
	wr32(hw, TXGBE_TSRXCTL, tsync_ctl);

	/* Disable L2 filtering of IEEE1588/802.1AS Ethernet frame types. */
	wr32(hw, TXGBE_ETFLT(TXGBE_ETF_ID_1588), 0);

	/* Stop incrementing the System Time registers. */
	wr32(hw, TXGBE_TSTIMEINC, 0);

	return 0;
}

static int
txgbe_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp,
				 uint32_t flags __rte_unused)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	uint32_t tsync_rxctl;
	uint64_t rx_tstamp_cycles;
	uint64_t ns;

	tsync_rxctl = rd32(hw, TXGBE_TSRXCTL);
	if ((tsync_rxctl & TXGBE_TSRXCTL_VLD) == 0)
		return -EINVAL;

	rx_tstamp_cycles = txgbe_read_rx_tstamp_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->rx_tstamp_tc, rx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return  0;
}

static int
txgbe_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	uint32_t tsync_txctl;
	uint64_t tx_tstamp_cycles;
	uint64_t ns;

	tsync_txctl = rd32(hw, TXGBE_TSTXCTL);
	if ((tsync_txctl & TXGBE_TSTXCTL_VLD) == 0)
		return -EINVAL;

	tx_tstamp_cycles = txgbe_read_tx_tstamp_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->tx_tstamp_tc, tx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

static int
txgbe_get_reg_length(struct rte_eth_dev *dev __rte_unused)
{
	int count = 0;
	int g_ind = 0;
	const struct reg_info *reg_group;
	const struct reg_info **reg_set = txgbe_regs_others;

	while ((reg_group = reg_set[g_ind++]))
		count += txgbe_regs_group_count(reg_group);

	return count;
}

static int
txgbe_get_regs(struct rte_eth_dev *dev,
	      struct rte_dev_reg_info *regs)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t *data = regs->data;
	int g_ind = 0;
	int count = 0;
	const struct reg_info *reg_group;
	const struct reg_info **reg_set = txgbe_regs_others;

	if (data == NULL) {
		regs->length = txgbe_get_reg_length(dev);
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Support only full register dump */
	if (regs->length == 0 ||
	    regs->length == (uint32_t)txgbe_get_reg_length(dev)) {
		regs->version = hw->mac.type << 24 |
				hw->revision_id << 16 |
				hw->device_id;
		while ((reg_group = reg_set[g_ind++]))
			count += txgbe_read_regs_group(dev, &data[count],
						      reg_group);
		return 0;
	}

	return -ENOTSUP;
}

static int
txgbe_get_eeprom_length(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	/* Return unit is byte count */
	return hw->rom.word_size * 2;
}

static int
txgbe_get_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_rom_info *eeprom = &hw->rom;
	uint16_t *data = in_eeprom->data;
	int first, length;

	first = in_eeprom->offset >> 1;
	length = in_eeprom->length >> 1;
	if (first > hw->rom.word_size ||
	    ((first + length) > hw->rom.word_size))
		return -EINVAL;

	in_eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	return eeprom->readw_buffer(hw, first, length, data);
}

static int
txgbe_set_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_rom_info *eeprom = &hw->rom;
	uint16_t *data = in_eeprom->data;
	int first, length;

	first = in_eeprom->offset >> 1;
	length = in_eeprom->length >> 1;
	if (first > hw->rom.word_size ||
	    ((first + length) > hw->rom.word_size))
		return -EINVAL;

	in_eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	return eeprom->writew_buffer(hw,  first, length, data);
}

static int
txgbe_get_module_info(struct rte_eth_dev *dev,
		      struct rte_eth_dev_module_info *modinfo)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t status;
	uint8_t sff8472_rev, addr_mode;
	bool page_swap = false;

	/* Check whether we support SFF-8472 or not */
	status = hw->phy.read_i2c_eeprom(hw,
					     TXGBE_SFF_SFF_8472_COMP,
					     &sff8472_rev);
	if (status != 0)
		return -EIO;

	/* addressing mode is not supported */
	status = hw->phy.read_i2c_eeprom(hw,
					     TXGBE_SFF_SFF_8472_SWAP,
					     &addr_mode);
	if (status != 0)
		return -EIO;

	if (addr_mode & TXGBE_SFF_ADDRESSING_MODE) {
		PMD_DRV_LOG(ERR,
			    "Address change required to access page 0xA2, "
			    "but not supported. Please report the module "
			    "type to the driver maintainers.");
		page_swap = true;
	}

	if (sff8472_rev == TXGBE_SFF_SFF_8472_UNSUP || page_swap) {
		/* We have a SFP, but it does not support SFF-8472 */
		modinfo->type = RTE_ETH_MODULE_SFF_8079;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8079_LEN;
	} else {
		/* We have a SFP which supports a revision of SFF-8472. */
		modinfo->type = RTE_ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
	}

	return 0;
}

static int
txgbe_get_module_eeprom(struct rte_eth_dev *dev,
			struct rte_dev_eeprom_info *info)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t status = TXGBE_ERR_PHY_ADDR_INVALID;
	uint8_t databyte = 0xFF;
	uint8_t *data = info->data;
	uint32_t i = 0;

	if (info->length == 0)
		return -EINVAL;

	for (i = info->offset; i < info->offset + info->length; i++) {
		if (i < RTE_ETH_MODULE_SFF_8079_LEN)
			status = hw->phy.read_i2c_eeprom(hw, i, &databyte);
		else
			status = hw->phy.read_i2c_sff8472(hw, i, &databyte);

		if (status != 0)
			return -EIO;

		data[i - info->offset] = databyte;
	}

	return 0;
}

bool
txgbe_rss_update_sp(enum txgbe_mac_type mac_type)
{
	switch (mac_type) {
	case txgbe_mac_raptor:
	case txgbe_mac_raptor_vf:
		return 1;
	default:
		return 0;
	}
}

static int
txgbe_dev_get_dcb_info(struct rte_eth_dev *dev,
			struct rte_eth_dcb_info *dcb_info)
{
	struct txgbe_dcb_config *dcb_config = TXGBE_DEV_DCB_CONFIG(dev);
	struct txgbe_dcb_tc_config *tc;
	struct rte_eth_dcb_tc_queue_mapping *tc_queue;
	uint8_t nb_tcs;
	uint8_t i, j;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_DCB_FLAG)
		dcb_info->nb_tcs = dcb_config->num_tcs.pg_tcs;
	else
		dcb_info->nb_tcs = 1;

	tc_queue = &dcb_info->tc_queue;
	nb_tcs = dcb_info->nb_tcs;

	if (dcb_config->vt_mode) { /* vt is enabled */
		struct rte_eth_vmdq_dcb_conf *vmdq_rx_conf =
				&dev->data->dev_conf.rx_adv_conf.vmdq_dcb_conf;
		for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++)
			dcb_info->prio_tc[i] = vmdq_rx_conf->dcb_tc[i];
		if (RTE_ETH_DEV_SRIOV(dev).active > 0) {
			for (j = 0; j < nb_tcs; j++) {
				tc_queue->tc_rxq[0][j].base = j;
				tc_queue->tc_rxq[0][j].nb_queue = 1;
				tc_queue->tc_txq[0][j].base = j;
				tc_queue->tc_txq[0][j].nb_queue = 1;
			}
		} else {
			for (i = 0; i < vmdq_rx_conf->nb_queue_pools; i++) {
				for (j = 0; j < nb_tcs; j++) {
					tc_queue->tc_rxq[i][j].base =
						i * nb_tcs + j;
					tc_queue->tc_rxq[i][j].nb_queue = 1;
					tc_queue->tc_txq[i][j].base =
						i * nb_tcs + j;
					tc_queue->tc_txq[i][j].nb_queue = 1;
				}
			}
		}
	} else { /* vt is disabled */
		struct rte_eth_dcb_rx_conf *rx_conf =
				&dev->data->dev_conf.rx_adv_conf.dcb_rx_conf;
		for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++)
			dcb_info->prio_tc[i] = rx_conf->dcb_tc[i];
		if (dcb_info->nb_tcs == RTE_ETH_4_TCS) {
			for (i = 0; i < dcb_info->nb_tcs; i++) {
				dcb_info->tc_queue.tc_rxq[0][i].base = i * 32;
				dcb_info->tc_queue.tc_rxq[0][i].nb_queue = 16;
			}
			dcb_info->tc_queue.tc_txq[0][0].base = 0;
			dcb_info->tc_queue.tc_txq[0][1].base = 64;
			dcb_info->tc_queue.tc_txq[0][2].base = 96;
			dcb_info->tc_queue.tc_txq[0][3].base = 112;
			dcb_info->tc_queue.tc_txq[0][0].nb_queue = 64;
			dcb_info->tc_queue.tc_txq[0][1].nb_queue = 32;
			dcb_info->tc_queue.tc_txq[0][2].nb_queue = 16;
			dcb_info->tc_queue.tc_txq[0][3].nb_queue = 16;
		} else if (dcb_info->nb_tcs == RTE_ETH_8_TCS) {
			for (i = 0; i < dcb_info->nb_tcs; i++) {
				dcb_info->tc_queue.tc_rxq[0][i].base = i * 16;
				dcb_info->tc_queue.tc_rxq[0][i].nb_queue = 16;
			}
			dcb_info->tc_queue.tc_txq[0][0].base = 0;
			dcb_info->tc_queue.tc_txq[0][1].base = 32;
			dcb_info->tc_queue.tc_txq[0][2].base = 64;
			dcb_info->tc_queue.tc_txq[0][3].base = 80;
			dcb_info->tc_queue.tc_txq[0][4].base = 96;
			dcb_info->tc_queue.tc_txq[0][5].base = 104;
			dcb_info->tc_queue.tc_txq[0][6].base = 112;
			dcb_info->tc_queue.tc_txq[0][7].base = 120;
			dcb_info->tc_queue.tc_txq[0][0].nb_queue = 32;
			dcb_info->tc_queue.tc_txq[0][1].nb_queue = 32;
			dcb_info->tc_queue.tc_txq[0][2].nb_queue = 16;
			dcb_info->tc_queue.tc_txq[0][3].nb_queue = 16;
			dcb_info->tc_queue.tc_txq[0][4].nb_queue = 8;
			dcb_info->tc_queue.tc_txq[0][5].nb_queue = 8;
			dcb_info->tc_queue.tc_txq[0][6].nb_queue = 8;
			dcb_info->tc_queue.tc_txq[0][7].nb_queue = 8;
		}
	}
	for (i = 0; i < dcb_info->nb_tcs; i++) {
		tc = &dcb_config->tc_config[i];
		dcb_info->tc_bws[i] = tc->path[TXGBE_DCB_TX_CONFIG].bwg_percent;
	}
	return 0;
}

/* Update e-tag ether type */
static int
txgbe_update_e_tag_eth_type(struct txgbe_hw *hw,
			    uint16_t ether_type)
{
	uint32_t etag_etype;

	etag_etype = rd32(hw, TXGBE_EXTAG);
	etag_etype &= ~TXGBE_EXTAG_ETAG_MASK;
	etag_etype |= ether_type;
	wr32(hw, TXGBE_EXTAG, etag_etype);
	txgbe_flush(hw);

	return 0;
}

/* Enable e-tag tunnel */
static int
txgbe_e_tag_enable(struct txgbe_hw *hw)
{
	uint32_t etag_etype;

	etag_etype = rd32(hw, TXGBE_PORTCTL);
	etag_etype |= TXGBE_PORTCTL_ETAG;
	wr32(hw, TXGBE_PORTCTL, etag_etype);
	txgbe_flush(hw);

	return 0;
}

static int
txgbe_e_tag_filter_del(struct rte_eth_dev *dev,
		       struct txgbe_l2_tunnel_conf  *l2_tunnel)
{
	int ret = 0;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t i, rar_entries;
	uint32_t rar_low, rar_high;

	rar_entries = hw->mac.num_rar_entries;

	for (i = 1; i < rar_entries; i++) {
		wr32(hw, TXGBE_ETHADDRIDX, i);
		rar_high = rd32(hw, TXGBE_ETHADDRH);
		rar_low  = rd32(hw, TXGBE_ETHADDRL);
		if ((rar_high & TXGBE_ETHADDRH_VLD) &&
		    (rar_high & TXGBE_ETHADDRH_ETAG) &&
		    (TXGBE_ETHADDRL_ETAG(rar_low) ==
		     l2_tunnel->tunnel_id)) {
			wr32(hw, TXGBE_ETHADDRL, 0);
			wr32(hw, TXGBE_ETHADDRH, 0);

			txgbe_clear_vmdq(hw, i, BIT_MASK32);

			return ret;
		}
	}

	return ret;
}

static int
txgbe_e_tag_filter_add(struct rte_eth_dev *dev,
		       struct txgbe_l2_tunnel_conf *l2_tunnel)
{
	int ret = 0;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t i, rar_entries;
	uint32_t rar_low, rar_high;

	/* One entry for one tunnel. Try to remove potential existing entry. */
	txgbe_e_tag_filter_del(dev, l2_tunnel);

	rar_entries = hw->mac.num_rar_entries;

	for (i = 1; i < rar_entries; i++) {
		wr32(hw, TXGBE_ETHADDRIDX, i);
		rar_high = rd32(hw, TXGBE_ETHADDRH);
		if (rar_high & TXGBE_ETHADDRH_VLD) {
			continue;
		} else {
			txgbe_set_vmdq(hw, i, l2_tunnel->pool);
			rar_high = TXGBE_ETHADDRH_VLD | TXGBE_ETHADDRH_ETAG;
			rar_low = l2_tunnel->tunnel_id;

			wr32(hw, TXGBE_ETHADDRL, rar_low);
			wr32(hw, TXGBE_ETHADDRH, rar_high);

			return ret;
		}
	}

	PMD_INIT_LOG(NOTICE, "The table of E-tag forwarding rule is full."
		     " Please remove a rule before adding a new one.");
	return -EINVAL;
}

static inline struct txgbe_l2_tn_filter *
txgbe_l2_tn_filter_lookup(struct txgbe_l2_tn_info *l2_tn_info,
			  struct txgbe_l2_tn_key *key)
{
	int ret;

	ret = rte_hash_lookup(l2_tn_info->hash_handle, (const void *)key);
	if (ret < 0)
		return NULL;

	return l2_tn_info->hash_map[ret];
}

static inline int
txgbe_insert_l2_tn_filter(struct txgbe_l2_tn_info *l2_tn_info,
			  struct txgbe_l2_tn_filter *l2_tn_filter)
{
	int ret;

	ret = rte_hash_add_key(l2_tn_info->hash_handle,
			       &l2_tn_filter->key);

	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert L2 tunnel filter"
			    " to hash table %d!",
			    ret);
		return ret;
	}

	l2_tn_info->hash_map[ret] = l2_tn_filter;

	TAILQ_INSERT_TAIL(&l2_tn_info->l2_tn_list, l2_tn_filter, entries);

	return 0;
}

static inline int
txgbe_remove_l2_tn_filter(struct txgbe_l2_tn_info *l2_tn_info,
			  struct txgbe_l2_tn_key *key)
{
	int ret;
	struct txgbe_l2_tn_filter *l2_tn_filter;

	ret = rte_hash_del_key(l2_tn_info->hash_handle, key);

	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "No such L2 tunnel filter to delete %d!",
			    ret);
		return ret;
	}

	l2_tn_filter = l2_tn_info->hash_map[ret];
	l2_tn_info->hash_map[ret] = NULL;

	TAILQ_REMOVE(&l2_tn_info->l2_tn_list, l2_tn_filter, entries);
	rte_free(l2_tn_filter);

	return 0;
}

/* Add l2 tunnel filter */
int
txgbe_dev_l2_tunnel_filter_add(struct rte_eth_dev *dev,
			       struct txgbe_l2_tunnel_conf *l2_tunnel,
			       bool restore)
{
	int ret;
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(dev);
	struct txgbe_l2_tn_key key;
	struct txgbe_l2_tn_filter *node;

	if (!restore) {
		key.l2_tn_type = l2_tunnel->l2_tunnel_type;
		key.tn_id = l2_tunnel->tunnel_id;

		node = txgbe_l2_tn_filter_lookup(l2_tn_info, &key);

		if (node) {
			PMD_DRV_LOG(ERR,
				    "The L2 tunnel filter already exists!");
			return -EINVAL;
		}

		node = rte_zmalloc("txgbe_l2_tn",
				   sizeof(struct txgbe_l2_tn_filter),
				   0);
		if (!node)
			return -ENOMEM;

		rte_memcpy(&node->key,
				 &key,
				 sizeof(struct txgbe_l2_tn_key));
		node->pool = l2_tunnel->pool;
		ret = txgbe_insert_l2_tn_filter(l2_tn_info, node);
		if (ret < 0) {
			rte_free(node);
			return ret;
		}
	}

	switch (l2_tunnel->l2_tunnel_type) {
	case RTE_ETH_L2_TUNNEL_TYPE_E_TAG:
		ret = txgbe_e_tag_filter_add(dev, l2_tunnel);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	if (!restore && ret < 0)
		(void)txgbe_remove_l2_tn_filter(l2_tn_info, &key);

	return ret;
}

/* Delete l2 tunnel filter */
int
txgbe_dev_l2_tunnel_filter_del(struct rte_eth_dev *dev,
			       struct txgbe_l2_tunnel_conf *l2_tunnel)
{
	int ret;
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(dev);
	struct txgbe_l2_tn_key key;

	key.l2_tn_type = l2_tunnel->l2_tunnel_type;
	key.tn_id = l2_tunnel->tunnel_id;
	ret = txgbe_remove_l2_tn_filter(l2_tn_info, &key);
	if (ret < 0)
		return ret;

	switch (l2_tunnel->l2_tunnel_type) {
	case RTE_ETH_L2_TUNNEL_TYPE_E_TAG:
		ret = txgbe_e_tag_filter_del(dev, l2_tunnel);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
txgbe_e_tag_forwarding_en_dis(struct rte_eth_dev *dev, bool en)
{
	int ret = 0;
	uint32_t ctrl;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	ctrl = rd32(hw, TXGBE_POOLCTL);
	ctrl &= ~TXGBE_POOLCTL_MODE_MASK;
	if (en)
		ctrl |= TXGBE_PSRPOOL_MODE_ETAG;
	wr32(hw, TXGBE_POOLCTL, ctrl);

	return ret;
}

/* Add UDP tunneling port */
static int
txgbe_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
			      struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret = 0;

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
		if (udp_tunnel->udp_port == 0) {
			PMD_DRV_LOG(ERR, "Add VxLAN port 0 is not allowed.");
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_VXLANPORT, udp_tunnel->udp_port);
		break;
	case RTE_ETH_TUNNEL_TYPE_GENEVE:
		if (udp_tunnel->udp_port == 0) {
			PMD_DRV_LOG(ERR, "Add Geneve port 0 is not allowed.");
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_GENEVEPORT, udp_tunnel->udp_port);
		break;
	case RTE_ETH_TUNNEL_TYPE_TEREDO:
		if (udp_tunnel->udp_port == 0) {
			PMD_DRV_LOG(ERR, "Add Teredo port 0 is not allowed.");
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_TEREDOPORT, udp_tunnel->udp_port);
		break;
	case RTE_ETH_TUNNEL_TYPE_VXLAN_GPE:
		if (udp_tunnel->udp_port == 0) {
			PMD_DRV_LOG(ERR, "Add VxLAN port 0 is not allowed.");
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_VXLANPORTGPE, udp_tunnel->udp_port);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	txgbe_flush(hw);

	return ret;
}

/* Remove UDP tunneling port */
static int
txgbe_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
			      struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret = 0;
	uint16_t cur_port;

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
		cur_port = (uint16_t)rd32(hw, TXGBE_VXLANPORT);
		if (cur_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Port %u does not exist.",
					udp_tunnel->udp_port);
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_VXLANPORT, 0);
		break;
	case RTE_ETH_TUNNEL_TYPE_GENEVE:
		cur_port = (uint16_t)rd32(hw, TXGBE_GENEVEPORT);
		if (cur_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Port %u does not exist.",
					udp_tunnel->udp_port);
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_GENEVEPORT, 0);
		break;
	case RTE_ETH_TUNNEL_TYPE_TEREDO:
		cur_port = (uint16_t)rd32(hw, TXGBE_TEREDOPORT);
		if (cur_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Port %u does not exist.",
					udp_tunnel->udp_port);
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_TEREDOPORT, 0);
		break;
	case RTE_ETH_TUNNEL_TYPE_VXLAN_GPE:
		cur_port = (uint16_t)rd32(hw, TXGBE_VXLANPORTGPE);
		if (cur_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Port %u does not exist.",
					udp_tunnel->udp_port);
			ret = -EINVAL;
			break;
		}
		wr32(hw, TXGBE_VXLANPORTGPE, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	txgbe_flush(hw);

	return ret;
}

/* restore n-tuple filter */
static inline void
txgbe_ntuple_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	struct txgbe_5tuple_filter *node;

	TAILQ_FOREACH(node, &filter_info->fivetuple_list, entries) {
		txgbe_inject_5tuple_filter(dev, node);
	}
}

/* restore ethernet type filter */
static inline void
txgbe_ethertype_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	int i;

	for (i = 0; i < TXGBE_ETF_ID_MAX; i++) {
		if (filter_info->ethertype_mask & (1 << i)) {
			wr32(hw, TXGBE_ETFLT(i),
					filter_info->ethertype_filters[i].etqf);
			wr32(hw, TXGBE_ETCLS(i),
					filter_info->ethertype_filters[i].etqs);
			txgbe_flush(hw);
		}
	}
}

/* restore SYN filter */
static inline void
txgbe_syn_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	uint32_t synqf;

	synqf = filter_info->syn_info;

	if (synqf & TXGBE_SYNCLS_ENA) {
		wr32(hw, TXGBE_SYNCLS, synqf);
		txgbe_flush(hw);
	}
}

/* restore L2 tunnel filter */
static inline void
txgbe_l2_tn_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(dev);
	struct txgbe_l2_tn_filter *node;
	struct txgbe_l2_tunnel_conf l2_tn_conf;

	TAILQ_FOREACH(node, &l2_tn_info->l2_tn_list, entries) {
		l2_tn_conf.l2_tunnel_type = node->key.l2_tn_type;
		l2_tn_conf.tunnel_id      = node->key.tn_id;
		l2_tn_conf.pool           = node->pool;
		(void)txgbe_dev_l2_tunnel_filter_add(dev, &l2_tn_conf, TRUE);
	}
}

/* restore rss filter */
static inline void
txgbe_rss_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);

	if (filter_info->rss_info.conf.queue_num)
		txgbe_config_rss_filter(dev,
			&filter_info->rss_info, TRUE);
}

static int
txgbe_filter_restore(struct rte_eth_dev *dev)
{
	txgbe_ntuple_filter_restore(dev);
	txgbe_ethertype_filter_restore(dev);
	txgbe_syn_filter_restore(dev);
	txgbe_fdir_filter_restore(dev);
	txgbe_l2_tn_filter_restore(dev);
	txgbe_rss_filter_restore(dev);

	return 0;
}

static void
txgbe_l2_tunnel_conf(struct rte_eth_dev *dev)
{
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (l2_tn_info->e_tag_en)
		(void)txgbe_e_tag_enable(hw);

	if (l2_tn_info->e_tag_fwd_en)
		(void)txgbe_e_tag_forwarding_en_dis(dev, 1);

	(void)txgbe_update_e_tag_eth_type(hw, l2_tn_info->e_tag_ether_type);
}

/* remove all the n-tuple filters */
void
txgbe_clear_all_ntuple_filter(struct rte_eth_dev *dev)
{
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	struct txgbe_5tuple_filter *p_5tuple;

	while ((p_5tuple = TAILQ_FIRST(&filter_info->fivetuple_list)))
		txgbe_remove_5tuple_filter(dev, p_5tuple);
}

/* remove all the ether type filters */
void
txgbe_clear_all_ethertype_filter(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);
	int i;

	for (i = 0; i < TXGBE_ETF_ID_MAX; i++) {
		if (filter_info->ethertype_mask & (1 << i) &&
		    !filter_info->ethertype_filters[i].conf) {
			(void)txgbe_ethertype_filter_remove(filter_info,
							    (uint8_t)i);
			wr32(hw, TXGBE_ETFLT(i), 0);
			wr32(hw, TXGBE_ETCLS(i), 0);
			txgbe_flush(hw);
		}
	}
}

/* remove the SYN filter */
void
txgbe_clear_syn_filter(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);

	if (filter_info->syn_info & TXGBE_SYNCLS_ENA) {
		filter_info->syn_info = 0;

		wr32(hw, TXGBE_SYNCLS, 0);
		txgbe_flush(hw);
	}
}

/* remove all the L2 tunnel filters */
int
txgbe_clear_all_l2_tn_filter(struct rte_eth_dev *dev)
{
	struct txgbe_l2_tn_info *l2_tn_info = TXGBE_DEV_L2_TN(dev);
	struct txgbe_l2_tn_filter *l2_tn_filter;
	struct txgbe_l2_tunnel_conf l2_tn_conf;
	int ret = 0;

	while ((l2_tn_filter = TAILQ_FIRST(&l2_tn_info->l2_tn_list))) {
		l2_tn_conf.l2_tunnel_type = l2_tn_filter->key.l2_tn_type;
		l2_tn_conf.tunnel_id      = l2_tn_filter->key.tn_id;
		l2_tn_conf.pool           = l2_tn_filter->pool;
		ret = txgbe_dev_l2_tunnel_filter_del(dev, &l2_tn_conf);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static const struct eth_dev_ops txgbe_eth_dev_ops = {
	.dev_configure              = txgbe_dev_configure,
	.dev_infos_get              = txgbe_dev_info_get,
	.dev_start                  = txgbe_dev_start,
	.dev_stop                   = txgbe_dev_stop,
	.dev_set_link_up            = txgbe_dev_set_link_up,
	.dev_set_link_down          = txgbe_dev_set_link_down,
	.dev_close                  = txgbe_dev_close,
	.dev_reset                  = txgbe_dev_reset,
	.promiscuous_enable         = txgbe_dev_promiscuous_enable,
	.promiscuous_disable        = txgbe_dev_promiscuous_disable,
	.allmulticast_enable        = txgbe_dev_allmulticast_enable,
	.allmulticast_disable       = txgbe_dev_allmulticast_disable,
	.link_update                = txgbe_dev_link_update,
	.stats_get                  = txgbe_dev_stats_get,
	.xstats_get                 = txgbe_dev_xstats_get,
	.xstats_get_by_id           = txgbe_dev_xstats_get_by_id,
	.stats_reset                = txgbe_dev_stats_reset,
	.xstats_reset               = txgbe_dev_xstats_reset,
	.xstats_get_names           = txgbe_dev_xstats_get_names,
	.xstats_get_names_by_id     = txgbe_dev_xstats_get_names_by_id,
	.queue_stats_mapping_set    = txgbe_dev_queue_stats_mapping_set,
	.fw_version_get             = txgbe_fw_version_get,
	.dev_supported_ptypes_get   = txgbe_dev_supported_ptypes_get,
	.mtu_set                    = txgbe_dev_mtu_set,
	.vlan_filter_set            = txgbe_vlan_filter_set,
	.vlan_tpid_set              = txgbe_vlan_tpid_set,
	.vlan_offload_set           = txgbe_vlan_offload_set,
	.vlan_strip_queue_set       = txgbe_vlan_strip_queue_set,
	.rx_queue_start	            = txgbe_dev_rx_queue_start,
	.rx_queue_stop              = txgbe_dev_rx_queue_stop,
	.tx_queue_start	            = txgbe_dev_tx_queue_start,
	.tx_queue_stop              = txgbe_dev_tx_queue_stop,
	.rx_queue_setup             = txgbe_dev_rx_queue_setup,
	.rx_queue_intr_enable       = txgbe_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable      = txgbe_dev_rx_queue_intr_disable,
	.rx_queue_release           = txgbe_dev_rx_queue_release,
	.tx_queue_setup             = txgbe_dev_tx_queue_setup,
	.tx_queue_release           = txgbe_dev_tx_queue_release,
	.dev_led_on                 = txgbe_dev_led_on,
	.dev_led_off                = txgbe_dev_led_off,
	.flow_ctrl_get              = txgbe_flow_ctrl_get,
	.flow_ctrl_set              = txgbe_flow_ctrl_set,
	.priority_flow_ctrl_set     = txgbe_priority_flow_ctrl_set,
	.mac_addr_add               = txgbe_add_rar,
	.mac_addr_remove            = txgbe_remove_rar,
	.mac_addr_set               = txgbe_set_default_mac_addr,
	.uc_hash_table_set          = txgbe_uc_hash_table_set,
	.uc_all_hash_table_set      = txgbe_uc_all_hash_table_set,
	.set_queue_rate_limit       = txgbe_set_queue_rate_limit,
	.reta_update                = txgbe_dev_rss_reta_update,
	.reta_query                 = txgbe_dev_rss_reta_query,
	.rss_hash_update            = txgbe_dev_rss_hash_update,
	.rss_hash_conf_get          = txgbe_dev_rss_hash_conf_get,
	.flow_ops_get               = txgbe_dev_flow_ops_get,
	.set_mc_addr_list           = txgbe_dev_set_mc_addr_list,
	.rxq_info_get               = txgbe_rxq_info_get,
	.txq_info_get               = txgbe_txq_info_get,
	.timesync_enable            = txgbe_timesync_enable,
	.timesync_disable           = txgbe_timesync_disable,
	.timesync_read_rx_timestamp = txgbe_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = txgbe_timesync_read_tx_timestamp,
	.get_reg                    = txgbe_get_regs,
	.get_eeprom_length          = txgbe_get_eeprom_length,
	.get_eeprom                 = txgbe_get_eeprom,
	.set_eeprom                 = txgbe_set_eeprom,
	.get_module_info            = txgbe_get_module_info,
	.get_module_eeprom          = txgbe_get_module_eeprom,
	.get_dcb_info               = txgbe_dev_get_dcb_info,
	.timesync_adjust_time       = txgbe_timesync_adjust_time,
	.timesync_read_time         = txgbe_timesync_read_time,
	.timesync_write_time        = txgbe_timesync_write_time,
	.udp_tunnel_port_add        = txgbe_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del        = txgbe_dev_udp_tunnel_port_del,
	.tm_ops_get                 = txgbe_tm_ops_get,
	.tx_done_cleanup            = txgbe_dev_tx_done_cleanup,
};

RTE_PMD_REGISTER_PCI(net_txgbe, rte_txgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_txgbe, pci_id_txgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_txgbe, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_txgbe,
			      TXGBE_DEVARG_BP_AUTO "=<0|1>"
			      TXGBE_DEVARG_KR_POLL "=<0|1>"
			      TXGBE_DEVARG_KR_PRESENT "=<0|1>"
			      TXGBE_DEVARG_KX_SGMII "=<0|1>"
			      TXGBE_DEVARG_FFE_SET "=<0-4>"
			      TXGBE_DEVARG_FFE_MAIN "=<uint16>"
			      TXGBE_DEVARG_FFE_PRE "=<uint16>"
			      TXGBE_DEVARG_FFE_POST "=<uint16>");

RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_driver, driver, NOTICE);
RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_bp, bp, NOTICE);

#ifdef RTE_LIBRTE_TXGBE_DEBUG_RX
	RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX
	RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_tx, tx, DEBUG);
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX_FREE
	RTE_LOG_REGISTER_SUFFIX(txgbe_logtype_tx_free, tx_free, DEBUG);
#endif
