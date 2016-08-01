/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "LICENSE.GPL".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


#include <linux/netdevice.h>

#include "igb.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */

#define IGB_MAX_NIC 32

#define OPTION_UNSET   -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1
#define MAX_NUM_LIST_OPTS 15

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */

#define IGB_PARAM_INIT { [0 ... IGB_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when igb_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define IGB_PARAM(X, desc) \
	static const int X[IGB_MAX_NIC+1] = IGB_PARAM_INIT; \
	MODULE_PARM(X, "1-" __MODULE_STRING(IGB_MAX_NIC) "i"); \
	MODULE_PARM_DESC(X, desc);
#else
#define IGB_PARAM(X, desc) \
	static int X[IGB_MAX_NIC+1] = IGB_PARAM_INIT; \
	static unsigned int num_##X; \
	module_param_array_named(X, X, int, &num_##X, 0); \
	MODULE_PARM_DESC(X, desc);
#endif

/* Interrupt Throttle Rate (interrupts/sec)
 *
 * Valid Range: 100-100000 (0=off, 1=dynamic, 3=dynamic conservative)
 */
IGB_PARAM(InterruptThrottleRate,
	  "Maximum interrupts per second, per vector, (max 100000), default 3=adaptive");
#define DEFAULT_ITR                    3
#define MAX_ITR                   100000
/* #define MIN_ITR                      120 */
#define MIN_ITR                      0
/* IntMode (Interrupt Mode)
 *
 * Valid Range: 0 - 2
 *
 * Default Value: 2 (MSI-X)
 */
IGB_PARAM(IntMode, "Change Interrupt Mode (0=Legacy, 1=MSI, 2=MSI-X), default 2");
#define MAX_INTMODE                    IGB_INT_MODE_MSIX
#define MIN_INTMODE                    IGB_INT_MODE_LEGACY

IGB_PARAM(Node, "set the starting node to allocate memory on, default -1");

/* LLIPort (Low Latency Interrupt TCP Port)
 *
 * Valid Range: 0 - 65535
 *
 * Default Value: 0 (disabled)
 */
IGB_PARAM(LLIPort, "Low Latency Interrupt TCP Port (0-65535), default 0=off");

#define DEFAULT_LLIPORT                0
#define MAX_LLIPORT               0xFFFF
#define MIN_LLIPORT                    0

/* LLIPush (Low Latency Interrupt on TCP Push flag)
 *
 * Valid Range: 0, 1
 *
 * Default Value: 0 (disabled)
 */
IGB_PARAM(LLIPush, "Low Latency Interrupt on TCP Push flag (0,1), default 0=off");

#define DEFAULT_LLIPUSH                0
#define MAX_LLIPUSH                    1
#define MIN_LLIPUSH                    0

/* LLISize (Low Latency Interrupt on Packet Size)
 *
 * Valid Range: 0 - 1500
 *
 * Default Value: 0 (disabled)
 */
IGB_PARAM(LLISize, "Low Latency Interrupt on Packet Size (0-1500), default 0=off");

#define DEFAULT_LLISIZE                0
#define MAX_LLISIZE                 1500
#define MIN_LLISIZE                    0

/* RSS (Enable RSS multiqueue receive)
 *
 * Valid Range: 0 - 8
 *
 * Default Value:  1
 */
IGB_PARAM(RSS, "Number of Receive-Side Scaling Descriptor Queues (0-8), default 1, 0=number of cpus");

#define DEFAULT_RSS       1
#define MAX_RSS           8
#define MIN_RSS           0

/* VMDQ (Enable VMDq multiqueue receive)
 *
 * Valid Range: 0 - 8
 *
 * Default Value:  0
 */
IGB_PARAM(VMDQ, "Number of Virtual Machine Device Queues: 0-1 = disable, 2-8 enable, default 0");

#define DEFAULT_VMDQ      0
#define MAX_VMDQ          MAX_RSS
#define MIN_VMDQ          0

/* max_vfs (Enable SR-IOV VF devices)
 *
 * Valid Range: 0 - 7
 *
 * Default Value:  0
 */
IGB_PARAM(max_vfs, "Number of Virtual Functions: 0 = disable, 1-7 enable, default 0");

#define DEFAULT_SRIOV     0
#define MAX_SRIOV         7
#define MIN_SRIOV         0

/* MDD (Enable Malicious Driver Detection)
 *
 * Only available when SR-IOV is enabled - max_vfs is greater than 0
 *
 * Valid Range: 0, 1
 *
 * Default Value:  1
 */
IGB_PARAM(MDD, "Malicious Driver Detection (0/1), default 1 = enabled. "
	  "Only available when max_vfs is greater than 0");

#ifdef DEBUG

/* Disable Hardware Reset on Tx Hang
 *
 * Valid Range: 0, 1
 *
 * Default Value: 0 (disabled, i.e. h/w will reset)
 */
IGB_PARAM(DisableHwReset, "Disable reset of hardware on Tx hang");

/* Dump Transmit and Receive buffers
 *
 * Valid Range: 0, 1
 *
 * Default Value: 0
 */
IGB_PARAM(DumpBuffers, "Dump Tx/Rx buffers on Tx hang or by request");

#endif /* DEBUG */

/* QueuePairs (Enable TX/RX queue pairs for interrupt handling)
 *
 * Valid Range: 0 - 1
 *
 * Default Value:  1
 */
IGB_PARAM(QueuePairs, "Enable Tx/Rx queue pairs for interrupt handling (0,1), default 1=on");

#define DEFAULT_QUEUE_PAIRS           1
#define MAX_QUEUE_PAIRS               1
#define MIN_QUEUE_PAIRS               0

/* Enable/disable EEE (a.k.a. IEEE802.3az)
 *
 * Valid Range: 0, 1
 *
 * Default Value: 1
 */
 IGB_PARAM(EEE, "Enable/disable on parts that support the feature");

/* Enable/disable DMA Coalescing
 *
 * Valid Values: 0(off), 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000,
 * 9000, 10000(msec), 250(usec), 500(usec)
 *
 * Default Value: 0
 */
 IGB_PARAM(DMAC, "Disable or set latency for DMA Coalescing ((0=off, 1000-10000(msec), 250, 500 (usec))");

#ifndef IGB_NO_LRO
/* Enable/disable Large Receive Offload
 *
 * Valid Values: 0(off), 1(on)
 *
 * Default Value: 0
 */
 IGB_PARAM(LRO, "Large Receive Offload (0,1), default 0=off");

#endif
struct igb_opt_list {
	int i;
	char *str;
};
struct igb_option {
	enum { enable_option, range_option, list_option } type;
	const char *name;
	const char *err;
	int def;
	union {
		struct { /* range_option info */
			int min;
			int max;
		} r;
		struct { /* list_option info */
			int nr;
			struct igb_opt_list *p;
		} l;
	} arg;
};

static int igb_validate_option(unsigned int *value,
			       struct igb_option *opt,
			       struct igb_adapter *adapter)
{
	if (*value == OPTION_UNSET) {
		*value = opt->def;
		return 0;
	}

	switch (opt->type) {
	case enable_option:
		switch (*value) {
		case OPTION_ENABLED:
			DPRINTK(PROBE, INFO, "%s Enabled\n", opt->name);
			return 0;
		case OPTION_DISABLED:
			DPRINTK(PROBE, INFO, "%s Disabled\n", opt->name);
			return 0;
		}
		break;
	case range_option:
		if (*value >= opt->arg.r.min && *value <= opt->arg.r.max) {
			DPRINTK(PROBE, INFO,
					"%s set to %d\n", opt->name, *value);
			return 0;
		}
		break;
	case list_option: {
		int i;
		struct igb_opt_list *ent;

		for (i = 0; i < opt->arg.l.nr; i++) {
			ent = &opt->arg.l.p[i];
			if (*value == ent->i) {
				if (ent->str[0] != '\0')
					DPRINTK(PROBE, INFO, "%s\n", ent->str);
				return 0;
			}
		}
	}
		break;
	default:
		BUG();
	}

	DPRINTK(PROBE, INFO, "Invalid %s value specified (%d) %s\n",
	       opt->name, *value, opt->err);
	*value = opt->def;
	return -1;
}

/**
 * igb_check_options - Range Checking for Command Line Parameters
 * @adapter: board private structure
 *
 * This routine checks all command line parameters for valid user
 * input.  If an invalid value is given, or if no user specified
 * value exists, a default value is used.  The final value is stored
 * in a variable in the adapter structure.
 **/

void igb_check_options(struct igb_adapter *adapter)
{
	int bd = adapter->bd_number;
	struct e1000_hw *hw = &adapter->hw;

	if (bd >= IGB_MAX_NIC) {
		DPRINTK(PROBE, NOTICE,
		       "Warning: no configuration for board #%d\n", bd);
		DPRINTK(PROBE, NOTICE, "Using defaults for all values\n");
#ifndef module_param_array
		bd = IGB_MAX_NIC;
#endif
	}

	{ /* Interrupt Throttling Rate */
		struct igb_option opt = {
			.type = range_option,
			.name = "Interrupt Throttling Rate (ints/sec)",
			.err  = "using default of " __MODULE_STRING(DEFAULT_ITR),
			.def  = DEFAULT_ITR,
			.arg  = { .r = { .min = MIN_ITR,
					 .max = MAX_ITR } }
		};

#ifdef module_param_array
		if (num_InterruptThrottleRate > bd) {
#endif
			unsigned int itr = InterruptThrottleRate[bd];

			switch (itr) {
			case 0:
				DPRINTK(PROBE, INFO, "%s turned off\n",
				        opt.name);
				if (hw->mac.type >= e1000_i350)
					adapter->dmac = IGB_DMAC_DISABLE;
				adapter->rx_itr_setting = itr;
				break;
			case 1:
				DPRINTK(PROBE, INFO, "%s set to dynamic mode\n",
					opt.name);
				adapter->rx_itr_setting = itr;
				break;
			case 3:
				DPRINTK(PROBE, INFO,
				        "%s set to dynamic conservative mode\n",
					opt.name);
				adapter->rx_itr_setting = itr;
				break;
			default:
				igb_validate_option(&itr, &opt, adapter);
				/* Save the setting, because the dynamic bits
				 * change itr.  In case of invalid user value,
				 * default to conservative mode, else need to
				 * clear the lower two bits because they are
				 * used as control */
				if (itr == 3) {
					adapter->rx_itr_setting = itr;
				} else {
					adapter->rx_itr_setting = 1000000000 /
					                          (itr * 256);
					adapter->rx_itr_setting &= ~3;
				}
				break;
			}
#ifdef module_param_array
		} else {
			adapter->rx_itr_setting = opt.def;
		}
#endif
		adapter->tx_itr_setting = adapter->rx_itr_setting;
	}
	{ /* Interrupt Mode */
		struct igb_option opt = {
			.type = range_option,
			.name = "Interrupt Mode",
			.err  = "defaulting to 2 (MSI-X)",
			.def  = IGB_INT_MODE_MSIX,
			.arg  = { .r = { .min = MIN_INTMODE,
					 .max = MAX_INTMODE } }
		};

#ifdef module_param_array
		if (num_IntMode > bd) {
#endif
			unsigned int int_mode = IntMode[bd];
			igb_validate_option(&int_mode, &opt, adapter);
			adapter->int_mode = int_mode;
#ifdef module_param_array
		} else {
			adapter->int_mode = opt.def;
		}
#endif
	}
	{ /* Low Latency Interrupt TCP Port */
		struct igb_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt TCP Port",
			.err  = "using default of " __MODULE_STRING(DEFAULT_LLIPORT),
			.def  = DEFAULT_LLIPORT,
			.arg  = { .r = { .min = MIN_LLIPORT,
					 .max = MAX_LLIPORT } }
		};

#ifdef module_param_array
		if (num_LLIPort > bd) {
#endif
			adapter->lli_port = LLIPort[bd];
			if (adapter->lli_port) {
				igb_validate_option(&adapter->lli_port, &opt,
				        adapter);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_port = opt.def;
		}
#endif
	}
	{ /* Low Latency Interrupt on Packet Size */
		struct igb_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on Packet Size",
			.err  = "using default of " __MODULE_STRING(DEFAULT_LLISIZE),
			.def  = DEFAULT_LLISIZE,
			.arg  = { .r = { .min = MIN_LLISIZE,
					 .max = MAX_LLISIZE } }
		};

#ifdef module_param_array
		if (num_LLISize > bd) {
#endif
			adapter->lli_size = LLISize[bd];
			if (adapter->lli_size) {
				igb_validate_option(&adapter->lli_size, &opt,
				        adapter);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_size = opt.def;
		}
#endif
	}
	{ /* Low Latency Interrupt on TCP Push flag */
		struct igb_option opt = {
			.type = enable_option,
			.name = "Low Latency Interrupt on TCP Push flag",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED
		};

#ifdef module_param_array
		if (num_LLIPush > bd) {
#endif
			unsigned int lli_push = LLIPush[bd];
			igb_validate_option(&lli_push, &opt, adapter);
			adapter->flags |= lli_push ? IGB_FLAG_LLI_PUSH : 0;
#ifdef module_param_array
		} else {
			adapter->flags |= opt.def ? IGB_FLAG_LLI_PUSH : 0;
		}
#endif
	}
	{ /* SRIOV - Enable SR-IOV VF devices */
		struct igb_option opt = {
			.type = range_option,
			.name = "max_vfs - SR-IOV VF devices",
			.err  = "using default of " __MODULE_STRING(DEFAULT_SRIOV),
			.def  = DEFAULT_SRIOV,
			.arg  = { .r = { .min = MIN_SRIOV,
					 .max = MAX_SRIOV } }
		};

#ifdef module_param_array
		if (num_max_vfs > bd) {
#endif
			adapter->vfs_allocated_count = max_vfs[bd];
			igb_validate_option(&adapter->vfs_allocated_count, &opt, adapter);

#ifdef module_param_array
		} else {
			adapter->vfs_allocated_count = opt.def;
		}
#endif
		if (adapter->vfs_allocated_count) {
			switch (hw->mac.type) {
			case e1000_82575:
			case e1000_82580:
			case e1000_i210:
			case e1000_i211:
			case e1000_i354:
				adapter->vfs_allocated_count = 0;
				DPRINTK(PROBE, INFO, "SR-IOV option max_vfs not supported.\n");
			default:
				break;
			}
		}
	}
	{ /* VMDQ - Enable VMDq multiqueue receive */
		struct igb_option opt = {
			.type = range_option,
			.name = "VMDQ - VMDq multiqueue queue count",
			.err  = "using default of " __MODULE_STRING(DEFAULT_VMDQ),
			.def  = DEFAULT_VMDQ,
			.arg  = { .r = { .min = MIN_VMDQ,
					 .max = (MAX_VMDQ - adapter->vfs_allocated_count) } }
		};
		if ((hw->mac.type != e1000_i210) ||
		    (hw->mac.type != e1000_i211)) {
#ifdef module_param_array
		if (num_VMDQ > bd) {
#endif
			adapter->vmdq_pools = (VMDQ[bd] == 1 ? 0 : VMDQ[bd]);
			if (adapter->vfs_allocated_count && !adapter->vmdq_pools) {
				DPRINTK(PROBE, INFO, "Enabling SR-IOV requires VMDq be set to at least 1\n");
				adapter->vmdq_pools = 1;
			}
			igb_validate_option(&adapter->vmdq_pools, &opt, adapter);

#ifdef module_param_array
		} else {
			if (!adapter->vfs_allocated_count)
				adapter->vmdq_pools = (opt.def == 1 ? 0 : opt.def);
			else
				adapter->vmdq_pools = 1;
		}
#endif
#ifdef CONFIG_IGB_VMDQ_NETDEV
		if (hw->mac.type == e1000_82575 && adapter->vmdq_pools) {
			DPRINTK(PROBE, INFO, "VMDq not supported on this part.\n");
			adapter->vmdq_pools = 0;
		}
#endif

	} else {
		DPRINTK(PROBE, INFO, "VMDq option is not supported.\n");
		adapter->vmdq_pools = opt.def;
	}
	}
	{ /* RSS - Enable RSS multiqueue receives */
		struct igb_option opt = {
			.type = range_option,
			.name = "RSS - RSS multiqueue receive count",
			.err  = "using default of " __MODULE_STRING(DEFAULT_RSS),
			.def  = DEFAULT_RSS,
			.arg  = { .r = { .min = MIN_RSS,
					 .max = MAX_RSS } }
		};

		switch (hw->mac.type) {
		case e1000_82575:
#ifndef CONFIG_IGB_VMDQ_NETDEV
			if (!!adapter->vmdq_pools) {
				if (adapter->vmdq_pools <= 2) {
					if (adapter->vmdq_pools == 2)
						opt.arg.r.max = 3;
				} else {
					opt.arg.r.max = 1;
				}
			} else {
				opt.arg.r.max = 4;
			}
#else
			opt.arg.r.max = !!adapter->vmdq_pools ? 1 : 4;
#endif /* CONFIG_IGB_VMDQ_NETDEV */
			break;
		case e1000_i210:
			opt.arg.r.max = 4;
			break;
		case e1000_i211:
			opt.arg.r.max = 2;
			break;
		case e1000_82576:
#ifndef CONFIG_IGB_VMDQ_NETDEV
			if (!!adapter->vmdq_pools)
				opt.arg.r.max = 2;
			break;
#endif /* CONFIG_IGB_VMDQ_NETDEV */
		case e1000_82580:
		case e1000_i350:
		case e1000_i354:
		default:
			if (!!adapter->vmdq_pools)
				opt.arg.r.max = 1;
			break;
		}

		if (adapter->int_mode != IGB_INT_MODE_MSIX) {
			DPRINTK(PROBE, INFO, "RSS is not supported when in MSI/Legacy Interrupt mode, %s\n",
				opt.err);
			opt.arg.r.max = 1;
		}

#ifdef module_param_array
		if (num_RSS > bd) {
#endif
			adapter->rss_queues = RSS[bd];
			switch (adapter->rss_queues) {
			case 1:
				break;
			default:
				igb_validate_option(&adapter->rss_queues, &opt, adapter);
				if (adapter->rss_queues)
					break;
			case 0:
				adapter->rss_queues = min_t(u32, opt.arg.r.max, num_online_cpus());
				break;
			}
#ifdef module_param_array
		} else {
			adapter->rss_queues = opt.def;
		}
#endif
	}
	{ /* QueuePairs - Enable Tx/Rx queue pairs for interrupt handling */
		struct igb_option opt = {
			.type = enable_option,
			.name = "QueuePairs - Tx/Rx queue pairs for interrupt handling",
			.err  = "defaulting to Enabled",
			.def  = OPTION_ENABLED
		};
#ifdef module_param_array
		if (num_QueuePairs > bd) {
#endif
			unsigned int qp = QueuePairs[bd];
			/*
			 * We must enable queue pairs if the number of queues
			 * exceeds the number of available interrupts. We are
			 * limited to 10, or 3 per unallocated vf. On I210 and
			 * I211 devices, we are limited to 5 interrupts.
			 * However, since I211 only supports 2 queues, we do not
			 * need to check and override the user option.
			 */
			if (qp == OPTION_DISABLED) {
				if (adapter->rss_queues > 4)
					qp = OPTION_ENABLED;

				if (adapter->vmdq_pools > 4)
					qp = OPTION_ENABLED;

				if (adapter->rss_queues > 1 &&
				    (adapter->vmdq_pools > 3 ||
				     adapter->vfs_allocated_count > 6))
					qp = OPTION_ENABLED;

				if (hw->mac.type == e1000_i210 &&
				    adapter->rss_queues > 2)
					qp = OPTION_ENABLED;

				if (qp == OPTION_ENABLED)
					DPRINTK(PROBE, INFO, "Number of queues exceeds available interrupts, %s\n",
						opt.err);
			}
			igb_validate_option(&qp, &opt, adapter);
			adapter->flags |= qp ? IGB_FLAG_QUEUE_PAIRS : 0;
#ifdef module_param_array
		} else {
			adapter->flags |= opt.def ? IGB_FLAG_QUEUE_PAIRS : 0;
		}
#endif
	}
	{ /* EEE -  Enable EEE for capable adapters */

		if (hw->mac.type >= e1000_i350) {
			struct igb_option opt = {
				.type = enable_option,
				.name = "EEE Support",
				.err  = "defaulting to Enabled",
				.def  = OPTION_ENABLED
			};
#ifdef module_param_array
			if (num_EEE > bd) {
#endif
				unsigned int eee = EEE[bd];
				igb_validate_option(&eee, &opt, adapter);
				adapter->flags |= eee ? IGB_FLAG_EEE : 0;
				if (eee)
					hw->dev_spec._82575.eee_disable = false;
				else
					hw->dev_spec._82575.eee_disable = true;

#ifdef module_param_array
			} else {
				adapter->flags |= opt.def ? IGB_FLAG_EEE : 0;
				if (adapter->flags & IGB_FLAG_EEE)
					hw->dev_spec._82575.eee_disable = false;
				else
					hw->dev_spec._82575.eee_disable = true;
			}
#endif
		}
	}
	{ /* DMAC -  Enable DMA Coalescing for capable adapters */

		if (hw->mac.type >= e1000_i350) {
			struct igb_opt_list list [] = {
				{ IGB_DMAC_DISABLE, "DMAC Disable"},
				{ IGB_DMAC_MIN, "DMAC 250 usec"},
				{ IGB_DMAC_500, "DMAC 500 usec"},
				{ IGB_DMAC_EN_DEFAULT, "DMAC 1000 usec"},
				{ IGB_DMAC_2000, "DMAC 2000 usec"},
				{ IGB_DMAC_3000, "DMAC 3000 usec"},
				{ IGB_DMAC_4000, "DMAC 4000 usec"},
				{ IGB_DMAC_5000, "DMAC 5000 usec"},
				{ IGB_DMAC_6000, "DMAC 6000 usec"},
				{ IGB_DMAC_7000, "DMAC 7000 usec"},
				{ IGB_DMAC_8000, "DMAC 8000 usec"},
				{ IGB_DMAC_9000, "DMAC 9000 usec"},
				{ IGB_DMAC_MAX, "DMAC 10000 usec"}
			};
			struct igb_option opt = {
				.type = list_option,
				.name = "DMA Coalescing",
				.err  = "using default of "__MODULE_STRING(IGB_DMAC_DISABLE),
				.def  = IGB_DMAC_DISABLE,
				.arg = { .l = { .nr = 13,
					 	.p = list
					}
				}
			};
#ifdef module_param_array
			if (num_DMAC > bd) {
#endif
				unsigned int dmac = DMAC[bd];
				if (adapter->rx_itr_setting == IGB_DMAC_DISABLE)
					dmac = IGB_DMAC_DISABLE;
				igb_validate_option(&dmac, &opt, adapter);
				switch (dmac) {
				case IGB_DMAC_DISABLE:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_MIN:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_500:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_EN_DEFAULT:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_2000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_3000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_4000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_5000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_6000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_7000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_8000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_9000:
					adapter->dmac = dmac;
					break;
				case IGB_DMAC_MAX:
					adapter->dmac = dmac;
					break;
				default:
					adapter->dmac = opt.def;
					DPRINTK(PROBE, INFO,
					"Invalid DMAC setting, "
					"resetting DMAC to %d\n", opt.def);
				}
#ifdef module_param_array
			} else
				adapter->dmac = opt.def;
#endif
		}
	}
#ifndef IGB_NO_LRO
	{ /* LRO - Enable Large Receive Offload */
		struct igb_option opt = {
			.type = enable_option,
			.name = "LRO - Large Receive Offload",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED
		};
		struct net_device *netdev = adapter->netdev;
#ifdef module_param_array
		if (num_LRO > bd) {
#endif
			unsigned int lro = LRO[bd];
			igb_validate_option(&lro, &opt, adapter);
			netdev->features |= lro ? NETIF_F_LRO : 0;
#ifdef module_param_array
		} else if (opt.def == OPTION_ENABLED) {
			netdev->features |= NETIF_F_LRO;
		}
#endif
	}
#endif /* IGB_NO_LRO */
	{ /* MDD - Enable Malicious Driver Detection. Only available when
	     SR-IOV is enabled. */
		struct igb_option opt = {
			.type = enable_option,
			.name = "Malicious Driver Detection",
			.err  = "defaulting to 1",
			.def  = OPTION_ENABLED,
			.arg  = { .r = { .min = OPTION_DISABLED,
					 .max = OPTION_ENABLED } }
		};

#ifdef module_param_array
		if (num_MDD > bd) {
#endif
			adapter->mdd = MDD[bd];
			igb_validate_option((uint *)&adapter->mdd, &opt,
					    adapter);
#ifdef module_param_array
		} else {
			adapter->mdd = opt.def;
		}
#endif
	}
}
