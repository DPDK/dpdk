/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
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
 *     * Neither the name of Chelsio Communications nor the names of its
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
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "cxgbe.h"

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

static void print_port_info(struct adapter *adap)
{
	int i;
	char buf[80];
	struct rte_pci_addr *loc = &adap->pdev->addr;

	for_each_port(adap, i) {
		const struct port_info *pi = &adap->port[i];
		char *bufp = buf;

		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_100M)
			bufp += sprintf(bufp, "100/");
		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_1G)
			bufp += sprintf(bufp, "1000/");
		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_10G)
			bufp += sprintf(bufp, "10G/");
		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_40G)
			bufp += sprintf(bufp, "40G/");
		if (bufp != buf)
			--bufp;
		sprintf(bufp, "BASE-%s",
			t4_get_port_type_description(pi->port_type));

		dev_info(adap,
			 " " PCI_PRI_FMT " Chelsio rev %d %s %s\n",
			 loc->domain, loc->bus, loc->devid, loc->function,
			 CHELSIO_CHIP_RELEASE(adap->params.chip), buf,
			 (adap->flags & USING_MSIX) ? " MSI-X" :
			 (adap->flags & USING_MSI) ? " MSI" : "");
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
	t4_fixup_host_params_compat(adapter, PAGE_SIZE, L1_CACHE_BYTES,
				    T5_LAST_REV);

	/*
	 * Keep the chip default offset to deliver Ingress packets into our
	 * DMA buffers to zero
	 */
	rx_dma_offset = 0;
	t4_set_reg_field(adapter, A_SGE_CONTROL, V_PKTSHIFT(M_PKTSHIFT),
			 V_PKTSHIFT(rx_dma_offset));

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
	 * initialization constants burried in the driver.
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
		state &= ~DEV_STATE_INIT;
	}

	t4_get_fw_version(adap, &adap->params.fw_vers);
	t4_get_tp_version(adap, &adap->params.tp_vers);

	dev_info(adap, "fw: %u.%u.%u.%u, TP: %u.%u.%u.%u\n",
		 G_FW_HDR_FW_VER_MAJOR(adap->params.fw_vers),
		 G_FW_HDR_FW_VER_MINOR(adap->params.fw_vers),
		 G_FW_HDR_FW_VER_MICRO(adap->params.fw_vers),
		 G_FW_HDR_FW_VER_BUILD(adap->params.fw_vers),
		 G_FW_HDR_FW_VER_MAJOR(adap->params.tp_vers),
		 G_FW_HDR_FW_VER_MINOR(adap->params.tp_vers),
		 G_FW_HDR_FW_VER_MICRO(adap->params.tp_vers),
		 G_FW_HDR_FW_VER_BUILD(adap->params.tp_vers));

	ret = t4_get_core_clock(adap, &adap->params.vpd);
	if (ret < 0) {
		dev_err(adap, "%s: could not get core clock, error %d\n",
			__func__, -ret);
		goto bye;
	}

	/*
	 * Find out what ports are available to us.  Note that we need to do
	 * this before calling adap_init0_no_config() since it needs nports
	 * and portvec ...
	 */
	v = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_PORTVEC);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1, &v, &port_vec);
	if (ret < 0) {
		dev_err(adap, "%s: failure in t4_queury_params; error = %d\n",
			__func__, ret);
		goto bye;
	}

	adap->params.nports = hweight32(port_vec);
	adap->params.portvec = port_vec;

	dev_debug(adap, "%s: adap->params.nports = %u\n", __func__,
		  adap->params.nports);

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

	const struct port_info *pi = &adap->port[port_id];

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		dev_info(adap, "Port%d: port module unplugged\n", pi->port_id);
	else if (pi->mod_type < ARRAY_SIZE(mod_str))
		dev_info(adap, "Port%d: %s port module inserted\n", pi->port_id,
			 mod_str[pi->mod_type]);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_NOTSUPPORTED)
		dev_info(adap, "Port%d: unsupported optical port module inserted\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_UNKNOWN)
		dev_info(adap, "Port%d: unknown port module inserted, forcing TWINAX\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_ERROR)
		dev_info(adap, "Port%d: transceiver module error\n",
			 pi->port_id);
	else
		dev_info(adap, "Port%d: unknown module type %d inserted\n",
			 pi->port_id, pi->mod_type);
}

int cxgbe_probe(struct adapter *adapter)
{
	struct port_info *pi;
	int func, i;
	int err = 0;

	func = G_SOURCEPF(t4_read_reg(adapter, A_PL_WHOAMI));
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
		num_seg = PAGE_SIZE / UDBS_SEG_SIZE;
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
		char name[RTE_ETH_NAME_MAX_LEN];
		struct rte_eth_dev_data *data = NULL;
		const unsigned int numa_node = rte_socket_id();

		pi = &adapter->port[i];
		pi->adapter = adapter;
		pi->xact_addr_filt = -1;
		pi->port_id = i;

		snprintf(name, sizeof(name), "cxgbe%d",
			 adapter->eth_dev->data->port_id + i);

		if (i == 0) {
			/* First port is already allocated by DPDK */
			pi->eth_dev = adapter->eth_dev;
			goto allocate_mac;
		}

		/*
		 * now do all data allocation - for eth_dev structure,
		 * and internal (private) data for the remaining ports
		 */

		/* reserve an ethdev entry */
		pi->eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_PCI);
		if (!pi->eth_dev)
			goto out_free;

		data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
		if (!data)
			goto out_free;

		data->port_id = adapter->eth_dev->data->port_id + i;

		pi->eth_dev->data = data;

allocate_mac:
		pi->eth_dev->pci_dev = adapter->pdev;
		pi->eth_dev->data->dev_private = pi;
		pi->eth_dev->driver = adapter->eth_dev->driver;
		pi->eth_dev->dev_ops = adapter->eth_dev->dev_ops;
		TAILQ_INIT(&pi->eth_dev->link_intr_cbs);

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

	print_port_info(adapter);

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
		if (pi->eth_dev->data)
			rte_free(pi->eth_dev->data);
	}

	if (adapter->flags & FW_OK)
		t4_fw_bye(adapter, adapter->mbox);
	return -err;
}
