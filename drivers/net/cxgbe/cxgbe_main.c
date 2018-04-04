/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>
#include <rte_kvargs.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "cxgbe.h"

#define CXGBE_DEVARG_KEEP_OVLAN "keep_ovlan"

/*
 * Response queue handler for the FW event queue.
 */
static int fwevtq_handler(struct sge_rspq *q, const __be64 *rsp,
			  __rte_unused const struct pkt_gl *gl)
{
	u8 opcode = ((const struct rss_header *)rsp)->opcode;

	rsp++;                                          /* skip RSS header */

	/*
	 * FW can send EGR_UPDATEs encapsulated in a CPL_FW4_MSG.
	 */
	if (unlikely(opcode == CPL_FW4_MSG &&
		     ((const struct cpl_fw4_msg *)rsp)->type ==
		      FW_TYPE_RSSCPL)) {
		rsp++;
		opcode = ((const struct rss_header *)rsp)->opcode;
		rsp++;
		if (opcode != CPL_SGE_EGR_UPDATE) {
			dev_err(q->adapter, "unexpected FW4/CPL %#x on FW event queue\n",
				opcode);
			goto out;
		}
	}

	if (likely(opcode == CPL_SGE_EGR_UPDATE)) {
		/* do nothing */
	} else if (opcode == CPL_FW6_MSG || opcode == CPL_FW4_MSG) {
		const struct cpl_fw6_msg *msg = (const void *)rsp;

		t4_handle_fw_rpl(q->adapter, msg->data);
	} else {
		dev_err(adapter, "unexpected CPL %#x on FW event queue\n",
			opcode);
	}
out:
	return 0;
}

int setup_sge_fwevtq(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;
	int err = 0;
	int msi_idx = 0;

	err = t4_sge_alloc_rxq(adapter, &s->fw_evtq, true, adapter->eth_dev,
			       msi_idx, NULL, fwevtq_handler, -1, NULL, 0,
			       rte_socket_id());
	return err;
}

static int closest_timer(const struct sge *s, int time)
{
	unsigned int i, match = 0;
	int delta, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->timer_val); i++) {
		delta = time - s->timer_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

static int closest_thres(const struct sge *s, int thres)
{
	unsigned int i, match = 0;
	int delta, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->counter_val); i++) {
		delta = thres - s->counter_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

/**
 * cxgb4_set_rspq_intr_params - set a queue's interrupt holdoff parameters
 * @q: the Rx queue
 * @us: the hold-off time in us, or 0 to disable timer
 * @cnt: the hold-off packet count, or 0 to disable counter
 *
 * Sets an Rx queue's interrupt hold-off time and packet count.  At least
 * one of the two needs to be enabled for the queue to generate interrupts.
 */
int cxgb4_set_rspq_intr_params(struct sge_rspq *q, unsigned int us,
			       unsigned int cnt)
{
	struct adapter *adap = q->adapter;
	unsigned int timer_val;

	if (cnt) {
		int err;
		u32 v, new_idx;

		new_idx = closest_thres(&adap->sge, cnt);
		if (q->desc && q->pktcnt_idx != new_idx) {
			/* the queue has already been created, update it */
			v = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
			    V_FW_PARAMS_PARAM_X(
			    FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH) |
			    V_FW_PARAMS_PARAM_YZ(q->cntxt_id);
			err = t4_set_params(adap, adap->mbox, adap->pf, 0, 1,
					    &v, &new_idx);
			if (err)
				return err;
		}
		q->pktcnt_idx = new_idx;
	}

	timer_val = (us == 0) ? X_TIMERREG_RESTART_COUNTER :
				closest_timer(&adap->sge, us);

	if ((us | cnt) == 0)
		q->intr_params = V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX);
	else
		q->intr_params = V_QINTR_TIMER_IDX(timer_val) |
				 V_QINTR_CNT_EN(cnt > 0);
	return 0;
}

static inline bool is_x_1g_port(const struct link_config *lc)
{
	return (lc->pcaps & FW_PORT_CAP32_SPEED_1G) != 0;
}

static inline bool is_x_10g_port(const struct link_config *lc)
{
	unsigned int speeds, high_speeds;

	speeds = V_FW_PORT_CAP32_SPEED(G_FW_PORT_CAP32_SPEED(lc->pcaps));
	high_speeds = speeds &
		      ~(FW_PORT_CAP32_SPEED_100M | FW_PORT_CAP32_SPEED_1G);

	return high_speeds != 0;
}

inline void init_rspq(struct adapter *adap, struct sge_rspq *q,
		      unsigned int us, unsigned int cnt,
		      unsigned int size, unsigned int iqe_size)
{
	q->adapter = adap;
	cxgb4_set_rspq_intr_params(q, us, cnt);
	q->iqe_len = iqe_size;
	q->size = size;
}

int cfg_queue_count(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int max_queues = s->max_ethqsets / adap->params.nports;

	if ((eth_dev->data->nb_rx_queues < 1) ||
	    (eth_dev->data->nb_tx_queues < 1))
		return -EINVAL;

	if ((eth_dev->data->nb_rx_queues > max_queues) ||
	    (eth_dev->data->nb_tx_queues > max_queues))
		return -EINVAL;

	if (eth_dev->data->nb_rx_queues > pi->rss_size)
		return -EINVAL;

	/* We must configure RSS, since config has changed*/
	pi->flags &= ~PORT_RSS_DONE;

	pi->n_rx_qsets = eth_dev->data->nb_rx_queues;
	pi->n_tx_qsets = eth_dev->data->nb_tx_queues;

	return 0;
}

void cfg_queues(struct rte_eth_dev *eth_dev)
{
	struct rte_config *config = rte_eal_get_configuration();
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int i, nb_ports = 0, qidx = 0;
	unsigned int q_per_port = 0;

	if (!(adap->flags & CFG_QUEUES)) {
		for_each_port(adap, i) {
			struct port_info *tpi = adap2pinfo(adap, i);

			nb_ports += (is_x_10g_port(&tpi->link_cfg)) ||
				     is_x_1g_port(&tpi->link_cfg) ? 1 : 0;
		}

		/*
		 * We default up to # of cores queues per 1G/10G port.
		 */
		if (nb_ports)
			q_per_port = (MAX_ETH_QSETS -
				     (adap->params.nports - nb_ports)) /
				     nb_ports;

		if (q_per_port > config->lcore_count)
			q_per_port = config->lcore_count;

		for_each_port(adap, i) {
			struct port_info *pi = adap2pinfo(adap, i);

			pi->first_qset = qidx;

			/* Initially n_rx_qsets == n_tx_qsets */
			pi->n_rx_qsets = (is_x_10g_port(&pi->link_cfg) ||
					  is_x_1g_port(&pi->link_cfg)) ?
					  q_per_port : 1;
			pi->n_tx_qsets = pi->n_rx_qsets;

			if (pi->n_rx_qsets > pi->rss_size)
				pi->n_rx_qsets = pi->rss_size;

			qidx += pi->n_rx_qsets;
		}

		s->max_ethqsets = qidx;

		for (i = 0; i < ARRAY_SIZE(s->ethrxq); i++) {
			struct sge_eth_rxq *r = &s->ethrxq[i];

			init_rspq(adap, &r->rspq, 5, 32, 1024, 64);
			r->usembufs = 1;
			r->fl.size = (r->usembufs ? 1024 : 72);
		}

		for (i = 0; i < ARRAY_SIZE(s->ethtxq); i++)
			s->ethtxq[i].q.size = 1024;

		init_rspq(adap, &adap->sge.fw_evtq, 0, 0, 1024, 64);
		adap->flags |= CFG_QUEUES;
	}
}

void cxgbe_stats_get(struct port_info *pi, struct port_stats *stats)
{
	t4_get_port_stats_offset(pi->adapter, pi->tx_chan, stats,
				 &pi->stats_base);
}

void cxgbe_stats_reset(struct port_info *pi)
{
	t4_clr_port_stats(pi->adapter, pi->tx_chan);
}

static void setup_memwin(struct adapter *adap)
{
	u32 mem_win0_base;

	/* For T5, only relative offset inside the PCIe BAR is passed */
	mem_win0_base = MEMWIN0_BASE;

	/*
	 * Set up memory window for accessing adapter memory ranges.  (Read
	 * back MA register to ensure that changes propagate before we attempt
	 * to use the new values.)
	 */
	t4_write_reg(adap,
		     PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN,
					 MEMWIN_NIC),
		     mem_win0_base | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN0_APERTURE) - X_WINDOW_SHIFT));
	t4_read_reg(adap,
		    PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN,
					MEMWIN_NIC));
}

int init_rss(struct adapter *adap)
{
	unsigned int i;

	if (is_pf4(adap)) {
		int err;

		err = t4_init_rss_mode(adap, adap->mbox);
		if (err)
			return err;
	}

	for_each_port(adap, i) {
		struct port_info *pi = adap2pinfo(adap, i);

		pi->rss = rte_zmalloc(NULL, pi->rss_size * sizeof(u16), 0);
		if (!pi->rss)
			return -ENOMEM;

		pi->rss_hf = CXGBE_RSS_HF_ALL;
	}
	return 0;
}

/**
 * Dump basic information about the adapter.
 */
void print_adapter_info(struct adapter *adap)
{
	/**
	 * Hardware/Firmware/etc. Version/Revision IDs.
	 */
	t4_dump_version_info(adap);
}

void print_port_info(struct adapter *adap)
{
	int i;
	char buf[80];
	struct rte_pci_addr *loc = &adap->pdev->addr;

	for_each_port(adap, i) {
		const struct port_info *pi = adap2pinfo(adap, i);
		char *bufp = buf;

		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_100M)
			bufp += sprintf(bufp, "100M/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_1G)
			bufp += sprintf(bufp, "1G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_10G)
			bufp += sprintf(bufp, "10G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_25G)
			bufp += sprintf(bufp, "25G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_40G)
			bufp += sprintf(bufp, "40G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_50G)
			bufp += sprintf(bufp, "50G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_100G)
			bufp += sprintf(bufp, "100G/");
		if (bufp != buf)
			--bufp;
		sprintf(bufp, "BASE-%s",
			t4_get_port_type_description(
					(enum fw_port_type)pi->port_type));

		dev_info(adap,
			 " " PCI_PRI_FMT " Chelsio rev %d %s %s\n",
			 loc->domain, loc->bus, loc->devid, loc->function,
			 CHELSIO_CHIP_RELEASE(adap->params.chip), buf,
			 (adap->flags & USING_MSIX) ? " MSI-X" :
			 (adap->flags & USING_MSI) ? " MSI" : "");
	}
}

static int
check_devargs_handler(__rte_unused const char *key, const char *value,
		      __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int cxgbe_get_devargs(struct rte_devargs *devargs, const char *key)
{
	struct rte_kvargs *kvlist;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	if (rte_kvargs_process(kvlist, key,
			       check_devargs_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static void configure_vlan_types(struct adapter *adapter)
{
	struct rte_pci_device *pdev = adapter->pdev;
	int i;

	for_each_port(adapter, i) {
		/* OVLAN Type 0x88a8 */
		t4_set_reg_field(adapter, MPS_PORT_RX_OVLAN_REG(i, A_RX_OVLAN0),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(M_OVLAN_ETYPE),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(0x88a8));
		/* OVLAN Type 0x9100 */
		t4_set_reg_field(adapter, MPS_PORT_RX_OVLAN_REG(i, A_RX_OVLAN1),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(M_OVLAN_ETYPE),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(0x9100));
		/* OVLAN Type 0x8100 */
		t4_set_reg_field(adapter, MPS_PORT_RX_OVLAN_REG(i, A_RX_OVLAN2),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(M_OVLAN_ETYPE),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(0x8100));

		/* IVLAN 0X8100 */
		t4_set_reg_field(adapter, MPS_PORT_RX_IVLAN(i),
				 V_IVLAN_ETYPE(M_IVLAN_ETYPE),
				 V_IVLAN_ETYPE(0x8100));

		t4_set_reg_field(adapter, MPS_PORT_RX_CTL(i),
				 F_OVLAN_EN0 | F_OVLAN_EN1 |
				 F_OVLAN_EN2 | F_IVLAN_EN,
				 F_OVLAN_EN0 | F_OVLAN_EN1 |
				 F_OVLAN_EN2 | F_IVLAN_EN);
	}

	if (cxgbe_get_devargs(pdev->device.devargs, CXGBE_DEVARG_KEEP_OVLAN))
		t4_tp_wr_bits_indirect(adapter, A_TP_INGRESS_CONFIG,
				       V_RM_OVLAN(1), V_RM_OVLAN(0));
}

static void configure_pcie_ext_tag(struct adapter *adapter)
{
	u16 v;
	int pos = t4_os_find_pci_capability(adapter, PCI_CAP_ID_EXP);

	if (!pos)
		return;

	if (pos > 0) {
		t4_os_pci_read_cfg2(adapter, pos + PCI_EXP_DEVCTL, &v);
		v |= PCI_EXP_DEVCTL_EXT_TAG;
		t4_os_pci_write_cfg2(adapter, pos + PCI_EXP_DEVCTL, v);
		if (is_t6(adapter->params.chip)) {
			t4_set_reg_field(adapter, A_PCIE_CFG2,
					 V_T6_TOTMAXTAG(M_T6_TOTMAXTAG),
					 V_T6_TOTMAXTAG(7));
			t4_set_reg_field(adapter, A_PCIE_CMD_CFG,
					 V_T6_MINTAG(M_T6_MINTAG),
					 V_T6_MINTAG(8));
		} else {
			t4_set_reg_field(adapter, A_PCIE_CFG2,
					 V_TOTMAXTAG(M_TOTMAXTAG),
					 V_TOTMAXTAG(3));
			t4_set_reg_field(adapter, A_PCIE_CMD_CFG,
					 V_MINTAG(M_MINTAG),
					 V_MINTAG(8));
		}
	}
}

/*
 * Tweak configuration based on system architecture, etc.  Most of these have
 * defaults assigned to them by Firmware Configuration Files (if we're using
 * them) but need to be explicitly set if we're using hard-coded
 * initialization. So these are essentially common tweaks/settings for
 * Configuration Files and hard-coded initialization ...
 */
static int adap_init0_tweaks(struct adapter *adapter)
{
	u8 rx_dma_offset;

	/*
	 * Fix up various Host-Dependent Parameters like Page Size, Cache
	 * Line Size, etc.  The firmware default is for a 4KB Page Size and
	 * 64B Cache Line Size ...
	 */
	t4_fixup_host_params_compat(adapter, CXGBE_PAGE_SIZE, L1_CACHE_BYTES,
				    T5_LAST_REV);

	/*
	 * Keep the chip default offset to deliver Ingress packets into our
	 * DMA buffers to zero
	 */
	rx_dma_offset = 0;
	t4_set_reg_field(adapter, A_SGE_CONTROL, V_PKTSHIFT(M_PKTSHIFT),
			 V_PKTSHIFT(rx_dma_offset));

	t4_set_reg_field(adapter, A_SGE_FLM_CFG,
			 V_CREDITCNT(M_CREDITCNT) | M_CREDITCNTPACKING,
			 V_CREDITCNT(3) | V_CREDITCNTPACKING(1));

	t4_set_reg_field(adapter, A_SGE_INGRESS_RX_THRESHOLD,
			 V_THRESHOLD_3(M_THRESHOLD_3), V_THRESHOLD_3(32U));

	t4_set_reg_field(adapter, A_SGE_CONTROL2, V_IDMAARBROUNDROBIN(1U),
			 V_IDMAARBROUNDROBIN(1U));

	/*
	 * Don't include the "IP Pseudo Header" in CPL_RX_PKT checksums: Linux
	 * adds the pseudo header itself.
	 */
	t4_tp_wr_bits_indirect(adapter, A_TP_INGRESS_CONFIG,
			       F_CSUM_HAS_PSEUDO_HDR, 0);

	return 0;
}

/*
 * Attempt to initialize the adapter via a Firmware Configuration File.
 */
static int adap_init0_config(struct adapter *adapter, int reset)
{
	struct fw_caps_config_cmd caps_cmd;
	unsigned long mtype = 0, maddr = 0;
	u32 finiver, finicsum, cfcsum;
	int ret;
	int config_issued = 0;
	int cfg_addr;
	char config_name[20];

	/*
	 * Reset device if necessary.
	 */
	if (reset) {
		ret = t4_fw_reset(adapter, adapter->mbox,
				  F_PIORSTMODE | F_PIORST);
		if (ret < 0) {
			dev_warn(adapter, "Firmware reset failed, error %d\n",
				 -ret);
			goto bye;
		}
	}

	cfg_addr = t4_flash_cfg_addr(adapter);
	if (cfg_addr < 0) {
		ret = cfg_addr;
		dev_warn(adapter, "Finding address for firmware config file in flash failed, error %d\n",
			 -ret);
		goto bye;
	}

	strcpy(config_name, "On Flash");
	mtype = FW_MEMTYPE_CF_FLASH;
	maddr = cfg_addr;

	/*
	 * Issue a Capability Configuration command to the firmware to get it
	 * to parse the Configuration File.  We don't use t4_fw_config_file()
	 * because we want the ability to modify various features after we've
	 * processed the configuration file ...
	 */
	memset(&caps_cmd, 0, sizeof(caps_cmd));
	caps_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
					   F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps_cmd.cfvalid_to_len16 =
		cpu_to_be32(F_FW_CAPS_CONFIG_CMD_CFVALID |
			    V_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(mtype) |
			    V_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(maddr >> 16) |
			    FW_LEN16(caps_cmd));
	ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd, sizeof(caps_cmd),
			 &caps_cmd);
	/*
	 * If the CAPS_CONFIG failed with an ENOENT (for a Firmware
	 * Configuration File in FLASH), our last gasp effort is to use the
	 * Firmware Configuration File which is embedded in the firmware.  A
	 * very few early versions of the firmware didn't have one embedded
	 * but we can ignore those.
	 */
	if (ret == -ENOENT) {
		dev_info(adapter, "%s: Going for embedded config in firmware..\n",
			 __func__);

		memset(&caps_cmd, 0, sizeof(caps_cmd));
		caps_cmd.op_to_write =
			cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_READ);
		caps_cmd.cfvalid_to_len16 = cpu_to_be32(FW_LEN16(caps_cmd));
		ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd,
				 sizeof(caps_cmd), &caps_cmd);
		strcpy(config_name, "Firmware Default");
	}

	config_issued = 1;
	if (ret < 0)
		goto bye;

	finiver = be32_to_cpu(caps_cmd.finiver);
	finicsum = be32_to_cpu(caps_cmd.finicsum);
	cfcsum = be32_to_cpu(caps_cmd.cfcsum);
	if (finicsum != cfcsum)
		dev_warn(adapter, "Configuration File checksum mismatch: [fini] csum=%#x, computed csum=%#x\n",
			 finicsum, cfcsum);

	/*
	 * If we're a pure NIC driver then disable all offloading facilities.
	 * This will allow the firmware to optimize aspects of the hardware
	 * configuration which will result in improved performance.
	 */
	caps_cmd.niccaps &= cpu_to_be16(~(FW_CAPS_CONFIG_NIC_HASHFILTER |
					  FW_CAPS_CONFIG_NIC_ETHOFLD));
	caps_cmd.toecaps = 0;
	caps_cmd.iscsicaps = 0;
	caps_cmd.rdmacaps = 0;
	caps_cmd.fcoecaps = 0;

	/*
	 * And now tell the firmware to use the configuration we just loaded.
	 */
	caps_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
					   F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	caps_cmd.cfvalid_to_len16 = htonl(FW_LEN16(caps_cmd));
	ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd, sizeof(caps_cmd),
			 NULL);
	if (ret < 0) {
		dev_warn(adapter, "Unable to finalize Firmware Capabilities %d\n",
			 -ret);
		goto bye;
	}

	/*
	 * Tweak configuration based on system architecture, etc.
	 */
	ret = adap_init0_tweaks(adapter);
	if (ret < 0) {
		dev_warn(adapter, "Unable to do init0-tweaks %d\n", -ret);
		goto bye;
	}

	/*
	 * And finally tell the firmware to initialize itself using the
	 * parameters from the Configuration File.
	 */
	ret = t4_fw_initialize(adapter, adapter->mbox);
	if (ret < 0) {
		dev_warn(adapter, "Initializing Firmware failed, error %d\n",
			 -ret);
		goto bye;
	}

	/*
	 * Return successfully and note that we're operating with parameters
	 * not supplied by the driver, rather than from hard-wired
	 * initialization constants buried in the driver.
	 */
	dev_info(adapter,
		 "Successfully configured using Firmware Configuration File \"%s\", version %#x, computed checksum %#x\n",
		 config_name, finiver, cfcsum);

	return 0;

	/*
	 * Something bad happened.  Return the error ...  (If the "error"
	 * is that there's no Configuration File on the adapter we don't
	 * want to issue a warning since this is fairly common.)
	 */
bye:
	if (config_issued && ret != -ENOENT)
		dev_warn(adapter, "\"%s\" configuration file error %d\n",
			 config_name, -ret);

	dev_debug(adapter, "%s: returning ret = %d ..\n", __func__, ret);
	return ret;
}

static int adap_init0(struct adapter *adap)
{
	int ret = 0;
	u32 v, port_vec;
	enum dev_state state;
	u32 params[7], val[7];
	int reset = 1;
	int mbox = adap->mbox;

	/*
	 * Contact FW, advertising Master capability.
	 */
	ret = t4_fw_hello(adap, adap->mbox, adap->mbox, MASTER_MAY, &state);
	if (ret < 0) {
		dev_err(adap, "%s: could not connect to FW, error %d\n",
			__func__, -ret);
		goto bye;
	}

	CXGBE_DEBUG_MBOX(adap, "%s: adap->mbox = %d; ret = %d\n", __func__,
			 adap->mbox, ret);

	if (ret == mbox)
		adap->flags |= MASTER_PF;

	if (state == DEV_STATE_INIT) {
		/*
		 * Force halt and reset FW because a previous instance may have
		 * exited abnormally without properly shutting down
		 */
		ret = t4_fw_halt(adap, adap->mbox, reset);
		if (ret < 0) {
			dev_err(adap, "Failed to halt. Exit.\n");
			goto bye;
		}

		ret = t4_fw_restart(adap, adap->mbox, reset);
		if (ret < 0) {
			dev_err(adap, "Failed to restart. Exit.\n");
			goto bye;
		}
		state = (enum dev_state)((unsigned)state & ~DEV_STATE_INIT);
	}

	t4_get_version_info(adap);

	ret = t4_get_core_clock(adap, &adap->params.vpd);
	if (ret < 0) {
		dev_err(adap, "%s: could not get core clock, error %d\n",
			__func__, -ret);
		goto bye;
	}

	/*
	 * If the firmware is initialized already (and we're not forcing a
	 * master initialization), note that we're living with existing
	 * adapter parameters.  Otherwise, it's time to try initializing the
	 * adapter ...
	 */
	if (state == DEV_STATE_INIT) {
		dev_info(adap, "Coming up as %s: Adapter already initialized\n",
			 adap->flags & MASTER_PF ? "MASTER" : "SLAVE");
	} else {
		dev_info(adap, "Coming up as MASTER: Initializing adapter\n");

		ret = adap_init0_config(adap, reset);
		if (ret == -ENOENT) {
			dev_err(adap,
				"No Configuration File present on adapter. Using hard-wired configuration parameters.\n");
			goto bye;
		}
	}
	if (ret < 0) {
		dev_err(adap, "could not initialize adapter, error %d\n", -ret);
		goto bye;
	}

	/* Find out what ports are available to us. */
	v = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_PORTVEC);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1, &v, &port_vec);
	if (ret < 0) {
		dev_err(adap, "%s: failure in t4_query_params; error = %d\n",
			__func__, ret);
		goto bye;
	}

	adap->params.nports = hweight32(port_vec);
	adap->params.portvec = port_vec;

	dev_debug(adap, "%s: adap->params.nports = %u\n", __func__,
		  adap->params.nports);

	/*
	 * Give the SGE code a chance to pull in anything that it needs ...
	 * Note that this must be called after we retrieve our VPD parameters
	 * in order to know how to convert core ticks to seconds, etc.
	 */
	ret = t4_sge_init(adap);
	if (ret < 0) {
		dev_err(adap, "t4_sge_init failed with error %d\n",
			-ret);
		goto bye;
	}

	/*
	 * Grab some of our basic fundamental operating parameters.
	 */
#define FW_PARAM_DEV(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))

#define FW_PARAM_PFVF(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param) |  \
	 V_FW_PARAMS_PARAM_Y(0) | \
	 V_FW_PARAMS_PARAM_Z(0))

	/* If we're running on newer firmware, let it know that we're
	 * prepared to deal with encapsulated CPL messages.  Older
	 * firmware won't understand this and we'll just get
	 * unencapsulated messages ...
	 */
	params[0] = FW_PARAM_PFVF(CPLFW4MSG_ENCAP);
	val[0] = 1;
	(void)t4_set_params(adap, adap->mbox, adap->pf, 0, 1, params, val);

	/*
	 * Find out whether we're allowed to use the T5+ ULPTX MEMWRITE DSGL
	 * capability.  Earlier versions of the firmware didn't have the
	 * ULPTX_MEMWRITE_DSGL so we'll interpret a query failure as no
	 * permission to use ULPTX MEMWRITE DSGL.
	 */
	if (is_t4(adap->params.chip)) {
		adap->params.ulptx_memwrite_dsgl = false;
	} else {
		params[0] = FW_PARAM_DEV(ULPTX_MEMWRITE_DSGL);
		ret = t4_query_params(adap, adap->mbox, adap->pf, 0,
				      1, params, val);
		adap->params.ulptx_memwrite_dsgl = (ret == 0 && val[0] != 0);
	}

	/*
	 * The MTU/MSS Table is initialized by now, so load their values.  If
	 * we're initializing the adapter, then we'll make any modifications
	 * we want to the MTU/MSS Table and also initialize the congestion
	 * parameters.
	 */
	t4_read_mtu_tbl(adap, adap->params.mtus, NULL);
	if (state != DEV_STATE_INIT) {
		int i;

		/*
		 * The default MTU Table contains values 1492 and 1500.
		 * However, for TCP, it's better to have two values which are
		 * a multiple of 8 +/- 4 bytes apart near this popular MTU.
		 * This allows us to have a TCP Data Payload which is a
		 * multiple of 8 regardless of what combination of TCP Options
		 * are in use (always a multiple of 4 bytes) which is
		 * important for performance reasons.  For instance, if no
		 * options are in use, then we have a 20-byte IP header and a
		 * 20-byte TCP header.  In this case, a 1500-byte MSS would
		 * result in a TCP Data Payload of 1500 - 40 == 1460 bytes
		 * which is not a multiple of 8.  So using an MSS of 1488 in
		 * this case results in a TCP Data Payload of 1448 bytes which
		 * is a multiple of 8.  On the other hand, if 12-byte TCP Time
		 * Stamps have been negotiated, then an MTU of 1500 bytes
		 * results in a TCP Data Payload of 1448 bytes which, as
		 * above, is a multiple of 8 bytes ...
		 */
		for (i = 0; i < NMTUS; i++)
			if (adap->params.mtus[i] == 1492) {
				adap->params.mtus[i] = 1488;
				break;
			}

		t4_load_mtus(adap, adap->params.mtus, adap->params.a_wnd,
			     adap->params.b_wnd);
	}
	t4_init_sge_params(adap);
	t4_init_tp_params(adap);
	configure_pcie_ext_tag(adap);
	configure_vlan_types(adap);

	adap->params.drv_memwin = MEMWIN_NIC;
	adap->flags |= FW_OK;
	dev_debug(adap, "%s: returning zero..\n", __func__);
	return 0;

	/*
	 * Something bad happened.  If a command timed out or failed with EIO
	 * FW does not operate within its spec or something catastrophic
	 * happened to HW/FW, stop issuing commands.
	 */
bye:
	if (ret != -ETIMEDOUT && ret != -EIO)
		t4_fw_bye(adap, adap->mbox);
	return ret;
}

/**
 * t4_os_portmod_changed - handle port module changes
 * @adap: the adapter associated with the module change
 * @port_id: the port index whose module status has changed
 *
 * This is the OS-dependent handler for port module changes.  It is
 * invoked when a port module is removed or inserted for any OS-specific
 * processing.
 */
void t4_os_portmod_changed(const struct adapter *adap, int port_id)
{
	static const char * const mod_str[] = {
		NULL, "LR", "SR", "ER", "passive DA", "active DA", "LRM"
	};

	const struct port_info *pi = adap2pinfo(adap, port_id);

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		dev_info(adap, "Port%d: port module unplugged\n", pi->port_id);
	else if (pi->mod_type < ARRAY_SIZE(mod_str))
		dev_info(adap, "Port%d: %s port module inserted\n", pi->port_id,
			 mod_str[pi->mod_type]);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_NOTSUPPORTED)
		dev_info(adap, "Port%d: unsupported port module inserted\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_UNKNOWN)
		dev_info(adap, "Port%d: unknown port module inserted\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_ERROR)
		dev_info(adap, "Port%d: transceiver module error\n",
			 pi->port_id);
	else
		dev_info(adap, "Port%d: unknown module type %d inserted\n",
			 pi->port_id, pi->mod_type);
}

/**
 * link_start - enable a port
 * @dev: the port to enable
 *
 * Performs the MAC and PHY actions needed to enable a port.
 */
int link_start(struct port_info *pi)
{
	struct adapter *adapter = pi->adapter;
	int ret;
	unsigned int mtu;

	mtu = pi->eth_dev->data->dev_conf.rxmode.max_rx_pkt_len -
	      (ETHER_HDR_LEN + ETHER_CRC_LEN);

	/*
	 * We do not set address filters and promiscuity here, the stack does
	 * that step explicitly.
	 */
	ret = t4_set_rxmode(adapter, adapter->mbox, pi->viid, mtu, -1, -1,
			    -1, 1, true);
	if (ret == 0) {
		ret = t4_change_mac(adapter, adapter->mbox, pi->viid,
				    pi->xact_addr_filt,
				    (u8 *)&pi->eth_dev->data->mac_addrs[0],
				    true, true);
		if (ret >= 0) {
			pi->xact_addr_filt = ret;
			ret = 0;
		}
	}
	if (ret == 0 && is_pf4(adapter))
		ret = t4_link_l1cfg(adapter, adapter->mbox, pi->tx_chan,
				    &pi->link_cfg);
	if (ret == 0) {
		/*
		 * Enabling a Virtual Interface can result in an interrupt
		 * during the processing of the VI Enable command and, in some
		 * paths, result in an attempt to issue another command in the
		 * interrupt context.  Thus, we disable interrupts during the
		 * course of the VI Enable command ...
		 */
		ret = t4_enable_vi_params(adapter, adapter->mbox, pi->viid,
					  true, true, false);
	}
	return ret;
}

/**
 * cxgbe_write_rss_conf - flash the RSS configuration for a given port
 * @pi: the port
 * @rss_hf: Hash configuration to apply
 */
int cxgbe_write_rss_conf(const struct port_info *pi, uint64_t rss_hf)
{
	struct adapter *adapter = pi->adapter;
	const struct sge_eth_rxq *rxq;
	u64 flags = 0;
	u16 rss;
	int err;

	/*  Should never be called before setting up sge eth rx queues */
	if (!(adapter->flags & FULL_INIT_DONE)) {
		dev_err(adap, "%s No RXQs available on port %d\n",
			__func__, pi->port_id);
		return -EINVAL;
	}

	/* Don't allow unsupported hash functions */
	if (rss_hf & ~CXGBE_RSS_HF_ALL)
		return -EINVAL;

	if (rss_hf & ETH_RSS_IPV4)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_UDPEN;

	if (rss_hf & ETH_RSS_IPV6)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_UDPEN;

	rxq = &adapter->sge.ethrxq[pi->first_qset];
	rss = rxq[0].rspq.abs_id;

	/* If Tunnel All Lookup isn't specified in the global RSS
	 * Configuration, then we need to specify a default Ingress
	 * Queue for any ingress packets which aren't hashed.  We'll
	 * use our first ingress queue ...
	 */
	err = t4_config_vi_rss(adapter, adapter->mbox, pi->viid,
			       flags, rss);
	return err;
}

/**
 * cxgbe_write_rss - write the RSS table for a given port
 * @pi: the port
 * @queues: array of queue indices for RSS
 *
 * Sets up the portion of the HW RSS table for the port's VI to distribute
 * packets to the Rx queues in @queues.
 */
int cxgbe_write_rss(const struct port_info *pi, const u16 *queues)
{
	u16 *rss;
	int i, err;
	struct adapter *adapter = pi->adapter;
	const struct sge_eth_rxq *rxq;

	/*  Should never be called before setting up sge eth rx queues */
	BUG_ON(!(adapter->flags & FULL_INIT_DONE));

	rxq = &adapter->sge.ethrxq[pi->first_qset];
	rss = rte_zmalloc(NULL, pi->rss_size * sizeof(u16), 0);
	if (!rss)
		return -ENOMEM;

	/* map the queue indices to queue ids */
	for (i = 0; i < pi->rss_size; i++, queues++)
		rss[i] = rxq[*queues].rspq.abs_id;

	err = t4_config_rss_range(adapter, adapter->pf, pi->viid, 0,
				  pi->rss_size, rss, pi->rss_size);
	rte_free(rss);
	return err;
}

/**
 * setup_rss - configure RSS
 * @adapter: the adapter
 *
 * Sets up RSS to distribute packets to multiple receive queues.  We
 * configure the RSS CPU lookup table to distribute to the number of HW
 * receive queues, and the response queue lookup table to narrow that
 * down to the response queues actually configured for each port.
 * We always configure the RSS mapping for all ports since the mapping
 * table has plenty of entries.
 */
int setup_rss(struct port_info *pi)
{
	int j, err;
	struct adapter *adapter = pi->adapter;

	dev_debug(adapter, "%s:  pi->rss_size = %u; pi->n_rx_qsets = %u\n",
		  __func__, pi->rss_size, pi->n_rx_qsets);

	if (!(pi->flags & PORT_RSS_DONE)) {
		if (adapter->flags & FULL_INIT_DONE) {
			/* Fill default values with equal distribution */
			for (j = 0; j < pi->rss_size; j++)
				pi->rss[j] = j % pi->n_rx_qsets;

			err = cxgbe_write_rss(pi, pi->rss);
			if (err)
				return err;

			err = cxgbe_write_rss_conf(pi, pi->rss_hf);
			if (err)
				return err;
			pi->flags |= PORT_RSS_DONE;
		}
	}
	return 0;
}

/*
 * Enable NAPI scheduling and interrupt generation for all Rx queues.
 */
static void enable_rx(struct adapter *adap, struct sge_rspq *q)
{
	/* 0-increment GTS to start the timer and enable interrupts */
	t4_write_reg(adap, is_pf4(adap) ? MYPF_REG(A_SGE_PF_GTS) :
					  T4VF_SGE_BASE_ADDR + A_SGE_VF_GTS,
		     V_SEINTARM(q->intr_params) |
		     V_INGRESSQID(q->cntxt_id));
}

void cxgbe_enable_rx_queues(struct port_info *pi)
{
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int i;

	for (i = 0; i < pi->n_rx_qsets; i++)
		enable_rx(adap, &s->ethrxq[pi->first_qset + i].rspq);
}

/**
 * fw_caps_to_speed_caps - translate Firmware Port Caps to Speed Caps.
 * @port_type: Firmware Port Type
 * @fw_caps: Firmware Port Capabilities
 * @speed_caps: Device Info Speed Capabilities
 *
 * Translate a Firmware Port Capabilities specification to Device Info
 * Speed Capabilities.
 */
static void fw_caps_to_speed_caps(enum fw_port_type port_type,
				  unsigned int fw_caps,
				  u32 *speed_caps)
{
#define SET_SPEED(__speed_name) \
	do { \
		*speed_caps |= ETH_LINK_ ## __speed_name; \
	} while (0)

#define FW_CAPS_TO_SPEED(__fw_name) \
	do { \
		if (fw_caps & FW_PORT_CAP32_ ## __fw_name) \
			SET_SPEED(__fw_name); \
	} while (0)

	switch (port_type) {
	case FW_PORT_TYPE_BT_SGMII:
	case FW_PORT_TYPE_BT_XFI:
	case FW_PORT_TYPE_BT_XAUI:
		FW_CAPS_TO_SPEED(SPEED_100M);
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_KX4:
	case FW_PORT_TYPE_KX:
	case FW_PORT_TYPE_FIBER_XFI:
	case FW_PORT_TYPE_FIBER_XAUI:
	case FW_PORT_TYPE_SFP:
	case FW_PORT_TYPE_QSFP_10G:
	case FW_PORT_TYPE_QSA:
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_KR:
		SET_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_BP_AP:
	case FW_PORT_TYPE_BP4_AP:
		SET_SPEED(SPEED_1G);
		SET_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_BP40_BA:
	case FW_PORT_TYPE_QSFP:
		SET_SPEED(SPEED_40G);
		break;

	case FW_PORT_TYPE_CR_QSFP:
	case FW_PORT_TYPE_SFP28:
	case FW_PORT_TYPE_KR_SFP28:
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		FW_CAPS_TO_SPEED(SPEED_25G);
		break;

	case FW_PORT_TYPE_CR2_QSFP:
		SET_SPEED(SPEED_50G);
		break;

	case FW_PORT_TYPE_KR4_100G:
	case FW_PORT_TYPE_CR4_QSFP:
		FW_CAPS_TO_SPEED(SPEED_25G);
		FW_CAPS_TO_SPEED(SPEED_40G);
		FW_CAPS_TO_SPEED(SPEED_50G);
		FW_CAPS_TO_SPEED(SPEED_100G);
		break;

	default:
		break;
	}

#undef FW_CAPS_TO_SPEED
#undef SET_SPEED
}

/**
 * cxgbe_get_speed_caps - Fetch supported speed capabilities
 * @pi: Underlying port's info
 * @speed_caps: Device Info speed capabilities
 *
 * Fetch supported speed capabilities of the underlying port.
 */
void cxgbe_get_speed_caps(struct port_info *pi, u32 *speed_caps)
{
	*speed_caps = 0;

	fw_caps_to_speed_caps(pi->port_type, pi->link_cfg.pcaps,
			      speed_caps);

	if (!(pi->link_cfg.pcaps & FW_PORT_CAP32_ANEG))
		*speed_caps |= ETH_LINK_SPEED_FIXED;
}

/**
 * cxgb_up - enable the adapter
 * @adap: adapter being enabled
 *
 * Called when the first port is enabled, this function performs the
 * actions necessary to make an adapter operational, such as completing
 * the initialization of HW modules, and enabling interrupts.
 */
int cxgbe_up(struct adapter *adap)
{
	enable_rx(adap, &adap->sge.fw_evtq);
	t4_sge_tx_monitor_start(adap);
	if (is_pf4(adap))
		t4_intr_enable(adap);
	adap->flags |= FULL_INIT_DONE;

	/* TODO: deadman watchdog ?? */
	return 0;
}

/*
 * Close the port
 */
int cxgbe_down(struct port_info *pi)
{
	struct adapter *adapter = pi->adapter;
	int err = 0;

	err = t4_enable_vi(adapter, adapter->mbox, pi->viid, false, false);
	if (err) {
		dev_err(adapter, "%s: disable_vi failed: %d\n", __func__, err);
		return err;
	}

	t4_reset_link_config(adapter, pi->pidx);
	return 0;
}

/*
 * Release resources when all the ports have been stopped.
 */
void cxgbe_close(struct adapter *adapter)
{
	struct port_info *pi;
	int i;

	if (adapter->flags & FULL_INIT_DONE) {
		if (is_pf4(adapter))
			t4_intr_disable(adapter);
		t4_sge_tx_monitor_stop(adapter);
		t4_free_sge_resources(adapter);
		for_each_port(adapter, i) {
			pi = adap2pinfo(adapter, i);
			if (pi->viid != 0)
				t4_free_vi(adapter, adapter->mbox,
					   adapter->pf, 0, pi->viid);
			rte_free(pi->eth_dev->data->mac_addrs);
			/* Skip first port since it'll be freed by DPDK stack */
			if (i) {
				rte_free(pi->eth_dev->data->dev_private);
				rte_eth_dev_release_port(pi->eth_dev);
			}
		}
		adapter->flags &= ~FULL_INIT_DONE;
	}

	if (is_pf4(adapter) && (adapter->flags & FW_OK))
		t4_fw_bye(adapter, adapter->mbox);
}

int cxgbe_probe(struct adapter *adapter)
{
	struct port_info *pi;
	int chip;
	int func, i;
	int err = 0;
	u32 whoami;

	whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	chip = t4_get_chip_type(adapter,
			CHELSIO_PCI_ID_VER(adapter->pdev->id.device_id));
	if (chip < 0)
		return chip;

	func = CHELSIO_CHIP_VERSION(chip) <= CHELSIO_T5 ?
	       G_SOURCEPF(whoami) : G_T6_SOURCEPF(whoami);

	adapter->mbox = func;
	adapter->pf = func;

	t4_os_lock_init(&adapter->mbox_lock);
	TAILQ_INIT(&adapter->mbox_list);

	err = t4_prep_adapter(adapter);
	if (err)
		return err;

	setup_memwin(adapter);
	err = adap_init0(adapter);
	if (err) {
		dev_err(adapter, "%s: Adapter initialization failed, error %d\n",
			__func__, err);
		goto out_free;
	}

	if (!is_t4(adapter->params.chip)) {
		/*
		 * The userspace doorbell BAR is split evenly into doorbell
		 * regions, each associated with an egress queue.  If this
		 * per-queue region is large enough (at least UDBS_SEG_SIZE)
		 * then it can be used to submit a tx work request with an
		 * implied doorbell.  Enable write combining on the BAR if
		 * there is room for such work requests.
		 */
		int s_qpp, qpp, num_seg;

		s_qpp = (S_QUEUESPERPAGEPF0 +
			(S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) *
			adapter->pf);
		qpp = 1 << ((t4_read_reg(adapter,
				A_SGE_EGRESS_QUEUES_PER_PAGE_PF) >> s_qpp)
				& M_QUEUESPERPAGEPF0);
		num_seg = CXGBE_PAGE_SIZE / UDBS_SEG_SIZE;
		if (qpp > num_seg)
			dev_warn(adapter, "Incorrect SGE EGRESS QUEUES_PER_PAGE configuration, continuing in debug mode\n");

		adapter->bar2 = (void *)adapter->pdev->mem_resource[2].addr;
		if (!adapter->bar2) {
			dev_err(adapter, "cannot map device bar2 region\n");
			err = -ENOMEM;
			goto out_free;
		}
		t4_write_reg(adapter, A_SGE_STAT_CFG, V_STATSOURCE_T5(7) |
			     V_STATMODE(0));
	}

	for_each_port(adapter, i) {
		const unsigned int numa_node = rte_socket_id();
		char name[RTE_ETH_NAME_MAX_LEN];
		struct rte_eth_dev *eth_dev;

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
		if (!eth_dev)
			goto out_free;

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
		pi->port_id = i;
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
			err = -1;
			goto out_free;
		}
	}

	if (adapter->flags & FW_OK) {
		err = t4_port_init(adapter, adapter->mbox, adapter->pf, 0);
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

	if (adapter->flags & FW_OK)
		t4_fw_bye(adapter, adapter->mbox);
	return -err;
}
