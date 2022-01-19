/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include <ethdev_driver.h>
#include <rte_ether.h>

#include "common.h"
#include "t4_regs.h"

/**
 * t4vf_wait_dev_ready - wait till to reads of registers work
 *
 * Wait for the device to become ready (signified by our "who am I" register
 * returning a value other than all 1's).  Return an error if it doesn't
 * become ready ...
 */
static int t4vf_wait_dev_ready(struct adapter *adapter)
{
	const u32 whoami = T4VF_PL_BASE_ADDR + A_PL_VF_WHOAMI;
	const u32 notready1 = 0xffffffff;
	const u32 notready2 = 0xeeeeeeee;
	u32 val;

	val = t4_read_reg(adapter, whoami);
	if (val != notready1 && val != notready2)
		return 0;

	msleep(500);
	val = t4_read_reg(adapter, whoami);
	if (val != notready1 && val != notready2)
		return 0;

	dev_err(adapter, "Device didn't become ready for access, whoami = %#x\n",
		val);
	return -EIO;
}

/*
 * Get the reply to a mailbox command and store it in @rpl in big-endian order.
 */
static void get_mbox_rpl(struct adapter *adap, __be64 *rpl, int nflit,
			 u32 mbox_addr)
{
	for ( ; nflit; nflit--, mbox_addr += 8)
		*rpl++ = cpu_to_be64(t4_read_reg64(adap, mbox_addr));
}

/**
 * t4vf_wr_mbox_core - send a command to FW through the mailbox
 * @adapter: the adapter
 * @cmd: the command to write
 * @size: command length in bytes
 * @rpl: where to optionally store the reply
 * @sleep_ok: if true we may sleep while awaiting command completion
 *
 * Sends the given command to FW through the mailbox and waits for the
 * FW to execute the command.  If @rpl is not %NULL it is used to store
 * the FW's reply to the command.  The command and its optional reply
 * are of the same length.  FW can take up to 500 ms to respond.
 * @sleep_ok determines whether we may sleep while awaiting the response.
 * If sleeping is allowed we use progressive backoff otherwise we spin.
 *
 * The return value is 0 on success or a negative errno on failure.  A
 * failure can happen either because we are not able to execute the
 * command or FW executes it but signals an error.  In the latter case
 * the return value is the error code indicated by FW (negated).
 */
int t4vf_wr_mbox_core(struct adapter *adapter,
		      const void __attribute__((__may_alias__)) *cmd,
		      int size, void *rpl, bool sleep_ok)
{
	/*
	 * We delay in small increments at first in an effort to maintain
	 * responsiveness for simple, fast executing commands but then back
	 * off to larger delays to a maximum retry delay.
	 */
	static const int delay[] = {
		1, 1, 3, 5, 10, 10, 20, 50, 100
	};


	u32 mbox_ctl = T4VF_CIM_BASE_ADDR + A_CIM_VF_EXT_MAILBOX_CTRL;
	__be64 cmd_rpl[MBOX_LEN / 8];
	struct mbox_entry *entry;
	unsigned int delay_idx;
	u32 v, mbox_data;
	const __be64 *p;
	int i, ret;
	int ms;

	/* In T6, mailbox size is changed to 128 bytes to avoid
	 * invalidating the entire prefetch buffer.
	 */
	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5)
		mbox_data = T4VF_MBDATA_BASE_ADDR;
	else
		mbox_data = T6VF_MBDATA_BASE_ADDR;

	/*
	 * Commands must be multiples of 16 bytes in length and may not be
	 * larger than the size of the Mailbox Data register array.
	 */
	if ((size % 16) != 0 ||
			size > NUM_CIM_VF_MAILBOX_DATA_INSTANCES * 4)
		return -EINVAL;

	entry = t4_os_alloc(sizeof(*entry));
	if (entry == NULL)
		return -ENOMEM;

	/*
	 * Queue ourselves onto the mailbox access list.  When our entry is at
	 * the front of the list, we have rights to access the mailbox.  So we
	 * wait [for a while] till we're at the front [or bail out with an
	 * EBUSY] ...
	 */
	t4_os_atomic_add_tail(entry, &adapter->mbox_list, &adapter->mbox_lock);

	delay_idx = 0;
	ms = delay[0];

	for (i = 0; ; i += ms) {
		/*
		 * If we've waited too long, return a busy indication.  This
		 * really ought to be based on our initial position in the
		 * mailbox access list but this is a start.  We very rarely
		 * contend on access to the mailbox ...
		 */
		if (i > (2 * FW_CMD_MAX_TIMEOUT)) {
			t4_os_atomic_list_del(entry, &adapter->mbox_list,
					      &adapter->mbox_lock);
			ret = -EBUSY;
			goto out_free;
		}

		/*
		 * If we're at the head, break out and start the mailbox
		 * protocol.
		 */
		if (t4_os_list_first_entry(&adapter->mbox_list) == entry)
			break;

		/*
		 * Delay for a bit before checking again ...
		 */
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			rte_delay_ms(ms);
		}
	}

	/*
	 * Loop trying to get ownership of the mailbox.  Return an error
	 * if we can't gain ownership.
	 */
	v = G_MBOWNER(t4_read_reg(adapter, mbox_ctl));
	for (i = 0; v == X_MBOWNER_NONE && i < 3; i++)
		v = G_MBOWNER(t4_read_reg(adapter, mbox_ctl));

	if (v != X_MBOWNER_PL) {
		t4_os_atomic_list_del(entry, &adapter->mbox_list,
				      &adapter->mbox_lock);
		ret = (v == X_MBOWNER_FW) ? -EBUSY : -ETIMEDOUT;
		goto out_free;
	}

	/*
	 * Write the command array into the Mailbox Data register array and
	 * transfer ownership of the mailbox to the firmware.
	 */
	for (i = 0, p = cmd; i < size; i += 8)
		t4_write_reg64(adapter, mbox_data + i, be64_to_cpu(*p++));

	t4_read_reg(adapter, mbox_data);          /* flush write */
	t4_write_reg(adapter, mbox_ctl,
			F_MBMSGVALID | V_MBOWNER(X_MBOWNER_FW));
	t4_read_reg(adapter, mbox_ctl);          /* flush write */
	delay_idx = 0;
	ms = delay[0];

	/*
	 * Spin waiting for firmware to acknowledge processing our command.
	 */
	for (i = 0; i < FW_CMD_MAX_TIMEOUT; i++) {
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			rte_delay_ms(ms);
		}

		/*
		 * If we're the owner, see if this is the reply we wanted.
		 */
		v = t4_read_reg(adapter, mbox_ctl);
		if (G_MBOWNER(v) == X_MBOWNER_PL) {
			/*
			 * If the Message Valid bit isn't on, revoke ownership
			 * of the mailbox and continue waiting for our reply.
			 */
			if ((v & F_MBMSGVALID) == 0) {
				t4_write_reg(adapter, mbox_ctl,
					     V_MBOWNER(X_MBOWNER_NONE));
				continue;
			}

			/*
			 * We now have our reply.  Extract the command return
			 * value, copy the reply back to our caller's buffer
			 * (if specified) and revoke ownership of the mailbox.
			 * We return the (negated) firmware command return
			 * code (this depends on FW_SUCCESS == 0).  (Again we
			 * avoid clogging the log with FW_VI_STATS_CMD
			 * reply results.)
			 */

			/*
			 * Retrieve the command reply and release the mailbox.
			 */
			get_mbox_rpl(adapter, cmd_rpl, size / 8, mbox_data);
			t4_write_reg(adapter, mbox_ctl,
				     V_MBOWNER(X_MBOWNER_NONE));
			t4_os_atomic_list_del(entry, &adapter->mbox_list,
					      &adapter->mbox_lock);

			/* return value in high-order host-endian word */
			v = be64_to_cpu(cmd_rpl[0]);

			if (rpl) {
				/* request bit in high-order BE word */
				WARN_ON((be32_to_cpu(*(const u32 *)cmd)
					 & F_FW_CMD_REQUEST) == 0);
				memcpy(rpl, cmd_rpl, size);
			}
			ret = -((int)G_FW_CMD_RETVAL(v));
			goto out_free;
		}
	}

	/*
	 * We timed out.  Return the error ...
	 */
	dev_err(adapter, "command %#x timed out\n",
		*(const u8 *)cmd);
	dev_err(adapter, "    Control = %#x\n", t4_read_reg(adapter, mbox_ctl));
	t4_os_atomic_list_del(entry, &adapter->mbox_list, &adapter->mbox_lock);
	ret = -ETIMEDOUT;

out_free:
	t4_os_free(entry);
	return ret;
}

/**
 * t4vf_fw_reset - issue a reset to FW
 * @adapter: the adapter
 *
 * Issues a reset command to FW.  For a Physical Function this would
 * result in the Firmware resetting all of its state.  For a Virtual
 * Function this just resets the state associated with the VF.
 */
int t4vf_fw_reset(struct adapter *adapter)
{
	struct fw_reset_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_RESET_CMD) |
				      F_FW_CMD_WRITE);
	cmd.retval_len16 = cpu_to_be32(V_FW_CMD_LEN16(FW_LEN16(cmd)));
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

/**
 * t4vf_prep_adapter - prepare SW and HW for operation
 * @adapter: the adapter
 *
 * Initialize adapter SW state for the various HW modules, set initial
 * values for some adapter tunables, take PHYs out of reset, and
 * initialize the MDIO interface.
 */
int t4vf_prep_adapter(struct adapter *adapter)
{
	u32 pl_vf_rev;
	int ret, ver;

	ret = t4vf_wait_dev_ready(adapter);
	if (ret < 0)
		return ret;

	/*
	 * Default port and clock for debugging in case we can't reach
	 * firmware.
	 */
	adapter->params.nports = 1;
	adapter->params.vfres.pmask = 1;
	adapter->params.vpd.cclk = 50000;

	pl_vf_rev = G_REV(t4_read_reg(adapter, A_PL_VF_REV));
	adapter->params.pci.device_id = adapter->pdev->id.device_id;
	adapter->params.pci.vendor_id = adapter->pdev->id.vendor_id;

	/*
	 * WE DON'T NEED adapter->params.chip CODE ONCE PL_REV CONTAINS
	 * ADAPTER (VERSION << 4 | REVISION)
	 */
	ver = CHELSIO_PCI_ID_VER(adapter->params.pci.device_id);
	adapter->params.chip = 0;
	switch (ver) {
	case CHELSIO_T5:
		adapter->params.chip |= CHELSIO_CHIP_CODE(CHELSIO_T5,
							  pl_vf_rev);
		adapter->params.arch.sge_fl_db = F_DBPRIO | F_DBTYPE;
		adapter->params.arch.mps_tcam_size =
			NUM_MPS_T5_CLS_SRAM_L_INSTANCES;
		break;
	case CHELSIO_T6:
		adapter->params.chip |= CHELSIO_CHIP_CODE(CHELSIO_T6,
							  pl_vf_rev);
		adapter->params.arch.sge_fl_db = 0;
		adapter->params.arch.mps_tcam_size =
			NUM_MPS_T5_CLS_SRAM_L_INSTANCES;
		break;
	default:
		dev_err(adapter, "%s: Device %d is not supported\n",
			__func__, adapter->params.pci.device_id);
		return -EINVAL;
	}
	return 0;
}

/**
 * t4vf_query_params - query FW or device parameters
 * @adapter: the adapter
 * @nparams: the number of parameters
 * @params: the parameter names
 * @vals: the parameter values
 *
 * Reads the values of firmware or device parameters.  Up to 7 parameters
 * can be queried at once.
 */
int t4vf_query_params(struct adapter *adapter, unsigned int nparams,
		      const u32 *params, u32 *vals)
{
	struct fw_params_cmd cmd, rpl;
	struct fw_params_param *p;
	unsigned int i;
	size_t len16;
	int ret;

	if (nparams > 7)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_READ);
	len16 = DIV_ROUND_UP(offsetof(struct fw_params_cmd,
			     param[nparams]), 16);
	cmd.retval_len16 = cpu_to_be32(V_FW_CMD_LEN16(len16));
	for (i = 0, p = &cmd.param[0]; i < nparams; i++, p++)
		p->mnem = cpu_to_be32(*params++);
	ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (ret == 0)
		for (i = 0, p = &rpl.param[0]; i < nparams; i++, p++)
			*vals++ = be32_to_cpu(p->val);
	return ret;
}

/**
 * t4vf_get_vpd_params - retrieve device VPD paremeters
 * @adapter: the adapter
 *
 * Retrives various device Vital Product Data parameters.  The parameters
 * are stored in @adapter->params.vpd.
 */
int t4vf_get_vpd_params(struct adapter *adapter)
{
	struct vpd_params *vpd_params = &adapter->params.vpd;
	u32 params[7], vals[7];
	int v;

	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_CCLK));
	v = t4vf_query_params(adapter, 1, params, vals);
	if (v != FW_SUCCESS)
		return v;
	vpd_params->cclk = vals[0];
	dev_debug(adapter, "%s: vpd_params->cclk = %u\n",
		  __func__, vpd_params->cclk);
	return 0;
}

/**
 * t4vf_get_dev_params - retrieve device paremeters
 * @adapter: the adapter
 *
 * Retrives fw and tp version.
 */
int t4vf_get_dev_params(struct adapter *adapter)
{
	u32 params[7], vals[7];
	int v;

	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_FWREV));
	params[1] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_TPREV));
	v = t4vf_query_params(adapter, 2, params, vals);
	if (v != FW_SUCCESS)
		return v;
	adapter->params.fw_vers = vals[0];
	adapter->params.tp_vers = vals[1];

	dev_info(adapter, "Firmware version: %u.%u.%u.%u\n",
		 G_FW_HDR_FW_VER_MAJOR(adapter->params.fw_vers),
		 G_FW_HDR_FW_VER_MINOR(adapter->params.fw_vers),
		 G_FW_HDR_FW_VER_MICRO(adapter->params.fw_vers),
		 G_FW_HDR_FW_VER_BUILD(adapter->params.fw_vers));

	dev_info(adapter, "TP Microcode version: %u.%u.%u.%u\n",
		 G_FW_HDR_FW_VER_MAJOR(adapter->params.tp_vers),
		 G_FW_HDR_FW_VER_MINOR(adapter->params.tp_vers),
		 G_FW_HDR_FW_VER_MICRO(adapter->params.tp_vers),
		 G_FW_HDR_FW_VER_BUILD(adapter->params.tp_vers));
	return 0;
}

/**
 * t4vf_set_params - sets FW or device parameters
 * @adapter: the adapter
 * @nparams: the number of parameters
 * @params: the parameter names
 * @vals: the parameter values
 *
 * Sets the values of firmware or device parameters.  Up to 7 parameters
 * can be specified at once.
 */
int t4vf_set_params(struct adapter *adapter, unsigned int nparams,
		    const u32 *params, const u32 *vals)
{
	struct fw_params_param *p;
	struct fw_params_cmd cmd;
	unsigned int i;
	size_t len16;

	if (nparams > 7)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_WRITE);
	len16 = DIV_ROUND_UP(offsetof(struct fw_params_cmd,
			     param[nparams]), 16);
	cmd.retval_len16 = cpu_to_be32(V_FW_CMD_LEN16(len16));
	for (i = 0, p = &cmd.param[0]; i < nparams; i++, p++) {
		p->mnem = cpu_to_be32(*params++);
		p->val = cpu_to_be32(*vals++);
	}
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

/**
 * t4vf_fl_pkt_align - return the fl packet alignment
 * @adapter: the adapter
 *
 * T4 has a single field to specify the packing and padding boundary.
 * T5 onwards has separate fields for this and hence the alignment for
 * next packet offset is maximum of these two.
 */
int t4vf_fl_pkt_align(struct adapter *adapter, u32 sge_control,
		      u32 sge_control2)
{
	unsigned int ingpadboundary, ingpackboundary, fl_align, ingpad_shift;

	/* T4 uses a single control field to specify both the PCIe Padding and
	 * Packing Boundary.  T5 introduced the ability to specify these
	 * separately.  The actual Ingress Packet Data alignment boundary
	 * within Packed Buffer Mode is the maximum of these two
	 * specifications.
	 */
	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5)
		ingpad_shift = X_INGPADBOUNDARY_SHIFT;
	else
		ingpad_shift = X_T6_INGPADBOUNDARY_SHIFT;

	ingpadboundary = 1 << (G_INGPADBOUNDARY(sge_control) + ingpad_shift);

	fl_align = ingpadboundary;
	if (!is_t4(adapter->params.chip)) {
		ingpackboundary = G_INGPACKBOUNDARY(sge_control2);
		if (ingpackboundary == X_INGPACKBOUNDARY_16B)
			ingpackboundary = 16;
		else
			ingpackboundary = 1 << (ingpackboundary +
					X_INGPACKBOUNDARY_SHIFT);

		fl_align = max(ingpadboundary, ingpackboundary);
	}
	return fl_align;
}

unsigned int t4vf_get_pf_from_vf(struct adapter *adapter)
{
	u32 whoami;

	whoami = t4_read_reg(adapter, T4VF_PL_BASE_ADDR + A_PL_VF_WHOAMI);
	return (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5 ?
			G_SOURCEPF(whoami) : G_T6_SOURCEPF(whoami));
}

/**
 * t4vf_get_rss_glb_config - retrieve adapter RSS Global Configuration
 * @adapter: the adapter
 *
 * Retrieves global RSS mode and parameters with which we have to live
 * and stores them in the @adapter's RSS parameters.
 */
int t4vf_get_rss_glb_config(struct adapter *adapter)
{
	struct rss_params *rss = &adapter->params.rss;
	struct fw_rss_glb_config_cmd cmd, rpl;
	int v;

	/*
	 * Execute an RSS Global Configuration read command to retrieve
	 * our RSS configuration.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_RSS_GLB_CONFIG_CMD) |
				      F_FW_CMD_REQUEST |
				      F_FW_CMD_READ);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v != FW_SUCCESS)
		return v;

	/*
	 * Translate the big-endian RSS Global Configuration into our
	 * cpu-endian format based on the RSS mode.  We also do first level
	 * filtering at this point to weed out modes which don't support
	 * VF Drivers ...
	 */
	rss->mode = G_FW_RSS_GLB_CONFIG_CMD_MODE
			(be32_to_cpu(rpl.u.manual.mode_pkd));
	switch (rss->mode) {
	case FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL: {
		u32 word = be32_to_cpu
				(rpl.u.basicvirtual.synmapen_to_hashtoeplitz);

		rss->u.basicvirtual.synmapen =
			((word & F_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN) != 0);
		rss->u.basicvirtual.syn4tupenipv6 =
			((word & F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6) != 0);
		rss->u.basicvirtual.syn2tupenipv6 =
			((word & F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6) != 0);
		rss->u.basicvirtual.syn4tupenipv4 =
			((word & F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4) != 0);
		rss->u.basicvirtual.syn2tupenipv4 =
			((word & F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4) != 0);
		rss->u.basicvirtual.ofdmapen =
			((word & F_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN) != 0);
		rss->u.basicvirtual.tnlmapen =
			((word & F_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN) != 0);
		rss->u.basicvirtual.tnlalllookup =
			((word  & F_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP) != 0);
		rss->u.basicvirtual.hashtoeplitz =
			((word & F_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ) != 0);

		/* we need at least Tunnel Map Enable to be set */
		if (!rss->u.basicvirtual.tnlmapen)
			return -EINVAL;
		break;
	}

	default:
		/* all unknown/unsupported RSS modes result in an error */
		return -EINVAL;
	}
	return 0;
}

/**
 * t4vf_get_vfres - retrieve VF resource limits
 * @adapter: the adapter
 *
 * Retrieves configured resource limits and capabilities for a virtual
 * function.  The results are stored in @adapter->vfres.
 */
int t4vf_get_vfres(struct adapter *adapter)
{
	struct vf_resources *vfres = &adapter->params.vfres;
	struct fw_pfvf_cmd cmd, rpl;
	u32 word;
	int v;

	/*
	 * Execute PFVF Read command to get VF resource limits; bail out early
	 * with error on command failure.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PFVF_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_READ);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v != FW_SUCCESS)
		return v;

	/*
	 * Extract VF resource limits and return success.
	 */
	word = be32_to_cpu(rpl.niqflint_niq);
	vfres->niqflint = G_FW_PFVF_CMD_NIQFLINT(word);
	vfres->niq = G_FW_PFVF_CMD_NIQ(word);

	word = be32_to_cpu(rpl.type_to_neq);
	vfres->neq = G_FW_PFVF_CMD_NEQ(word);
	vfres->pmask = G_FW_PFVF_CMD_PMASK(word);

	word = be32_to_cpu(rpl.tc_to_nexactf);
	vfres->tc = G_FW_PFVF_CMD_TC(word);
	vfres->nvi = G_FW_PFVF_CMD_NVI(word);
	vfres->nexactf = G_FW_PFVF_CMD_NEXACTF(word);

	word = be32_to_cpu(rpl.r_caps_to_nethctrl);
	vfres->r_caps = G_FW_PFVF_CMD_R_CAPS(word);
	vfres->wx_caps = G_FW_PFVF_CMD_WX_CAPS(word);
	vfres->nethctrl = G_FW_PFVF_CMD_NETHCTRL(word);
	return 0;
}

/**
 * t4vf_get_port_stats_fw - collect "port" statistics via Firmware
 * @adapter: the adapter
 * @pidx: the port index
 * @s: the stats structure to fill
 *
 * Collect statistics for the "port"'s Virtual Interface via Firmware
 * commands.
 */
static int t4vf_get_port_stats_fw(struct adapter *adapter, int pidx,
				  struct port_stats *p)
{
	struct port_info *pi = adap2pinfo(adapter, pidx);
	unsigned int rem = VI_VF_NUM_STATS;
	struct fw_vi_stats_vf fwstats;
	__be64 *fwsp = (__be64 *)&fwstats;

	/*
	 * Grab the Virtual Interface statistics a chunk at a time via mailbox
	 * commands.  We could use a Work Request and get all of them at once
	 * but that's an asynchronous interface which is awkward to use.
	 */
	while (rem) {
		unsigned int ix = VI_VF_NUM_STATS - rem;
		unsigned int nstats = min(6U, rem);
		struct fw_vi_stats_cmd cmd, rpl;
		size_t len = (offsetof(struct fw_vi_stats_cmd, u) +
			      sizeof(struct fw_vi_stats_ctl));
		size_t len16 = DIV_ROUND_UP(len, 16);
		int ret;

		memset(&cmd, 0, sizeof(cmd));
		cmd.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_STATS_CMD) |
					     V_FW_VI_STATS_CMD_VIID(pi->viid) |
					     F_FW_CMD_REQUEST |
					     F_FW_CMD_READ);
		cmd.retval_len16 = cpu_to_be32(V_FW_CMD_LEN16(len16));
		cmd.u.ctl.nstats_ix =
			cpu_to_be16(V_FW_VI_STATS_CMD_IX(ix) |
				    V_FW_VI_STATS_CMD_NSTATS(nstats));
		ret = t4vf_wr_mbox_ns(adapter, &cmd, len, &rpl);
		if (ret != FW_SUCCESS)
			return ret;

		memcpy(fwsp, &rpl.u.ctl.stat0, sizeof(__be64) * nstats);

		rem -= nstats;
		fwsp += nstats;
	}

	/*
	 * Translate firmware statistics into host native statistics.
	 */
	p->tx_octets = be64_to_cpu(fwstats.tx_bcast_bytes) +
		       be64_to_cpu(fwstats.tx_mcast_bytes) +
		       be64_to_cpu(fwstats.tx_ucast_bytes);
	p->tx_bcast_frames = be64_to_cpu(fwstats.tx_bcast_frames);
	p->tx_mcast_frames = be64_to_cpu(fwstats.tx_mcast_frames);
	p->tx_ucast_frames = be64_to_cpu(fwstats.tx_ucast_frames);
	p->tx_drop = be64_to_cpu(fwstats.tx_drop_frames);

	p->rx_bcast_frames = be64_to_cpu(fwstats.rx_bcast_frames);
	p->rx_mcast_frames = be64_to_cpu(fwstats.rx_mcast_frames);
	p->rx_ucast_frames = be64_to_cpu(fwstats.rx_ucast_frames);
	p->rx_len_err = be64_to_cpu(fwstats.rx_err_frames);

	return 0;
}

/**
 *      t4vf_get_port_stats - collect "port" statistics
 *      @adapter: the adapter
 *      @pidx: the port index
 *      @s: the stats structure to fill
 *
 *      Collect statistics for the "port"'s Virtual Interface.
 */
void t4vf_get_port_stats(struct adapter *adapter, int pidx,
			 struct port_stats *p)
{
	/*
	 * If this is not the first Virtual Interface for our Virtual
	 * Function, we need to use Firmware commands to retrieve its
	 * MPS statistics.
	 */
	if (pidx != 0)
		t4vf_get_port_stats_fw(adapter, pidx, p);

	/*
	 * But for the first VI, we can grab its statistics via the MPS
	 * register mapped into the VF register space.
	 */
#define GET_STAT(name) \
	t4_read_reg64(adapter, \
			T4VF_MPS_BASE_ADDR + A_MPS_VF_STAT_##name##_L)
	p->tx_octets = GET_STAT(TX_VF_BCAST_BYTES) +
		       GET_STAT(TX_VF_MCAST_BYTES) +
		       GET_STAT(TX_VF_UCAST_BYTES);
	p->tx_bcast_frames = GET_STAT(TX_VF_BCAST_FRAMES);
	p->tx_mcast_frames = GET_STAT(TX_VF_MCAST_FRAMES);
	p->tx_ucast_frames = GET_STAT(TX_VF_UCAST_FRAMES);
	p->tx_drop = GET_STAT(TX_VF_DROP_FRAMES);

	p->rx_bcast_frames = GET_STAT(RX_VF_BCAST_FRAMES);
	p->rx_mcast_frames = GET_STAT(RX_VF_MCAST_FRAMES);
	p->rx_ucast_frames = GET_STAT(RX_VF_UCAST_FRAMES);

	p->rx_len_err = GET_STAT(RX_VF_ERR_FRAMES);
#undef GET_STAT
}

static int t4vf_alloc_vi(struct adapter *adapter, int port_id)
{
	struct fw_vi_cmd cmd, rpl;
	int v;

	/*
	 * Execute a VI command to allocate Virtual Interface and return its
	 * VIID.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_WRITE |
				    F_FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_LEN16(cmd) |
					 F_FW_VI_CMD_ALLOC);
	cmd.portid_pkd = V_FW_VI_CMD_PORTID(port_id);
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v != FW_SUCCESS)
		return v;
	return G_FW_VI_CMD_VIID(be16_to_cpu(rpl.type_to_viid));
}

int t4vf_port_init(struct adapter *adapter)
{
	struct fw_port_cmd port_cmd, port_rpl, rpl;
	struct fw_vi_cmd vi_cmd, vi_rpl;
	u32 param, val, pcaps, acaps;
	enum fw_port_type port_type;
	int mdio_addr;
	int ret, i;

	param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) |
		 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_PORT_CAPS32));
	val = 1;
	ret = t4vf_set_params(adapter, 1, &param, &val);
	if (ret < 0)
		return ret;

	for_each_port(adapter, i) {
		struct port_info *p = adap2pinfo(adapter, i);
		u32 lstatus32;

		ret = t4vf_alloc_vi(adapter, p->port_id);
		if (ret < 0) {
			dev_err(&pdev->dev, "cannot allocate VI for port %d:"
				" err=%d\n", p->port_id, ret);
			return ret;
		}
		p->viid = ret;

		/*
		 * Execute a VI Read command to get our Virtual Interface
		 * information like MAC address, etc.
		 */
		memset(&vi_cmd, 0, sizeof(vi_cmd));
		vi_cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) |
					       F_FW_CMD_REQUEST |
					       F_FW_CMD_READ);
		vi_cmd.alloc_to_len16 = cpu_to_be32(FW_LEN16(vi_cmd));
		vi_cmd.type_to_viid = cpu_to_be16(V_FW_VI_CMD_VIID(p->viid));
		ret = t4vf_wr_mbox(adapter, &vi_cmd, sizeof(vi_cmd), &vi_rpl);
		if (ret != FW_SUCCESS)
			return ret;

		p->rss_size = G_FW_VI_CMD_RSSSIZE
				(be16_to_cpu(vi_rpl.norss_rsssize));
		t4_os_set_hw_addr(adapter, i, vi_rpl.mac);

		/*
		 * If we don't have read access to our port information, we're
		 * done now.  Else, execute a PORT Read command to get it ...
		 */
		if (!(adapter->params.vfres.r_caps & FW_CMD_CAP_PORT))
			return 0;

		memset(&port_cmd, 0, sizeof(port_cmd));
		port_cmd.op_to_portid =
			cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_READ |
				    V_FW_PORT_CMD_PORTID(p->port_id));
		val = FW_PORT_ACTION_GET_PORT_INFO32;
		port_cmd.action_to_len16 =
			cpu_to_be32(V_FW_PORT_CMD_ACTION(val) |
				    FW_LEN16(port_cmd));
		ret = t4vf_wr_mbox(adapter, &port_cmd, sizeof(port_cmd),
				   &port_rpl);
		if (ret != FW_SUCCESS)
			return ret;

		/*
		 * Extract the various fields from the Port Information message.
		 */
		rpl = port_rpl;
		lstatus32 = be32_to_cpu(rpl.u.info32.lstatus32_to_cbllen32);

		port_type = G_FW_PORT_CMD_PORTTYPE32(lstatus32);
		mdio_addr = (lstatus32 & F_FW_PORT_CMD_MDIOCAP32) ?
			    (int)G_FW_PORT_CMD_MDIOADDR32(lstatus32) : -1;
		pcaps = be32_to_cpu(port_rpl.u.info32.pcaps32);
		acaps = be32_to_cpu(port_rpl.u.info32.acaps32);

		t4_init_link_config(p, pcaps, acaps, mdio_addr, port_type,
				    FW_PORT_MOD_TYPE_NA);
	}
	return 0;
}
