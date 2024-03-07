/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 *
 * Copyright 2010-2012 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright 2019-2024 NXP
 *
 */

#ifndef __FMAN_H
#define __FMAN_H

#include <stdbool.h>
#include <net/if.h>

#include <ethdev_driver.h>
#include <rte_ether.h>

#include <compat.h>
#include <dpaa_list.h>

#ifndef FMAN_DEVICE_PATH
#define FMAN_DEVICE_PATH "/dev/mem"
#endif

#define MEMAC_NUM_OF_PADDRS 7 /* Num of additional exact match MAC adr regs */

/* Control and Configuration Register (COMMAND_CONFIG) for MEMAC */
#define CMD_CFG_LOOPBACK_EN	0x00000400
/**< 21 XGMII/GMII loopback enable */
#define CMD_CFG_PROMIS_EN	0x00000010
/**< 27 Promiscuous operation enable */
#define CMD_CFG_PAUSE_IGNORE	0x00000100
/**< 23 Ignore Pause frame quanta */

/* Statistics Configuration Register (STATN_CONFIG) */
#define STATS_CFG_CLR           0x00000004
/**< 29 Reset all counters */
#define STATS_CFG_CLR_ON_RD     0x00000002
/**< 30 Clear on read */
#define STATS_CFG_SATURATE      0x00000001
/**< 31 Saturate at the maximum val */

/**< Max receive frame length mask */
#define MAXFRM_SIZE_MEMAC	0x00007fe0
#define MAXFRM_RX_MASK		0x0000ffff

/**< Interface Mode Register Register for MEMAC */
#define IF_MODE_RLP 0x00000820

/**< Pool Limits */
#define FMAN_PORT_MAX_EXT_POOLS_NUM	8
#define FMAN_PORT_OBS_EXT_POOLS_NUM	2

#define FMAN_PORT_CG_MAP_NUM		8
#define FMAN_PORT_PRS_RESULT_WORDS_NUM	8
#define FMAN_PORT_BMI_FIFO_UNITS	0x100
#define FMAN_PORT_IC_OFFSET_UNITS	0x10

#define FMAN_BMI_COUNTERS_EN 0x80000000

#define FMAN_ENABLE_BPOOL_DEPLETION	0xF00000F0

#define HASH_CTRL_MCAST_EN	0x00000100
#define GROUP_ADDRESS		0x0000010000000000LL
#define HASH_CTRL_ADDR_MASK	0x0000003F

#define FMAN_RTC_MAX_NUM_OF_ALARMS		3
#define FMAN_RTC_MAX_NUM_OF_PERIODIC_PULSES	4
#define FMAN_RTC_MAX_NUM_OF_EXT_TRIGGERS	3

/* Represents the different flavour of network interface */
enum fman_mac_type {
	fman_offline_internal = 0,
	fman_mac_1g,
	fman_mac_10g,
	fman_mac_2_5g,
	fman_onic,
};

struct mac_addr {
	uint32_t   mac_addr_l;	/**< Lower 32 bits of 48-bit MAC address */
	uint32_t   mac_addr_u;	/**< Upper 16 bits of 48-bit MAC address */
};

#define MEMAC_RX_ENABLE ((uint32_t)0x2)
#define MEMAC_TX_ENABLE ((uint32_t)0x1)
struct memac_regs {
	/* General Control and Status */
	uint32_t res0000[2];
	uint32_t command_config;	/**< 0x008 Ctrl and cfg */
	struct mac_addr mac_addr0;	/**< 0x00C-0x010 MAC_ADDR_0...1 */
	uint32_t maxfrm;		/**< 0x014 Max frame length */
	uint32_t res0018[5];
	uint32_t hashtable_ctrl;	/**< 0x02C Hash table control */
	uint32_t res0030[4];
	uint32_t ievent;		/**< 0x040 Interrupt event */
	uint32_t tx_ipg_length;
	/**< 0x044 Transmitter inter-packet-gap */
	uint32_t res0048;
	uint32_t imask;			/**< 0x04C Interrupt mask */
	uint32_t res0050;
	uint32_t pause_quanta[4];	/**< 0x054 Pause quanta */
	uint32_t pause_thresh[4];	/**< 0x064 Pause quanta threshold */
	uint32_t rx_pause_status;	/**< 0x074 Receive pause status */
	uint32_t res0078[2];
	struct mac_addr mac_addr[MEMAC_NUM_OF_PADDRS];
	/**< 0x80-0x0B4 mac padr */
	uint32_t lpwake_timer;
	/**< 0x0B8 Low Power Wakeup Timer */
	uint32_t sleep_timer;
	/**< 0x0BC Transmit EEE Low Power Timer */
	uint32_t res00c0[8];
	uint32_t statn_config;
	/**< 0x0E0 Statistics configuration */
	uint32_t res00e4[7];
	/* Rx Statistics Counter */
	uint32_t reoct_l;		/**<Rx Eth Octets Counter */
	uint32_t reoct_u;
	uint32_t roct_l;		/**<Rx Octet Counters */
	uint32_t roct_u;
	uint32_t raln_l;		/**<Rx Alignment Error Counter */
	uint32_t raln_u;
	uint32_t rxpf_l;		/**<Rx valid Pause Frame */
	uint32_t rxpf_u;
	uint32_t rfrm_l;		/**<Rx Frame counter */
	uint32_t rfrm_u;
	uint32_t rfcs_l;		/**<Rx frame check seq error */
	uint32_t rfcs_u;
	uint32_t rvlan_l;		/**<Rx Vlan Frame Counter */
	uint32_t rvlan_u;
	uint32_t rerr_l;		/**<Rx Frame error */
	uint32_t rerr_u;
	uint32_t ruca_l;		/**<Rx Unicast */
	uint32_t ruca_u;
	uint32_t rmca_l;		/**<Rx Multicast */
	uint32_t rmca_u;
	uint32_t rbca_l;		/**<Rx Broadcast */
	uint32_t rbca_u;
	uint32_t rdrp_l;		/**<Rx Dropper Packet */
	uint32_t rdrp_u;
	uint32_t rpkt_l;		/**<Rx packet */
	uint32_t rpkt_u;
	uint32_t rund_l;		/**<Rx undersized packets */
	uint32_t rund_u;
	uint32_t r64_l;			/**<Rx 64 byte */
	uint32_t r64_u;
	uint32_t r127_l;
	uint32_t r127_u;
	uint32_t r255_l;
	uint32_t r255_u;
	uint32_t r511_l;
	uint32_t r511_u;
	uint32_t r1023_l;
	uint32_t r1023_u;
	uint32_t r1518_l;
	uint32_t r1518_u;
	uint32_t r1519x_l;
	uint32_t r1519x_u;
	uint32_t rovr_l;		/**<Rx oversized but good */
	uint32_t rovr_u;
	uint32_t rjbr_l;		/**<Rx oversized with bad csum */
	uint32_t rjbr_u;
	uint32_t rfrg_l;		/**<Rx fragment Packet */
	uint32_t rfrg_u;
	uint32_t rcnp_l;		/**<Rx control packets (0x8808 */
	uint32_t rcnp_u;
	uint32_t rdrntp_l;		/**<Rx dropped due to FIFO overflow */
	uint32_t rdrntp_u;
	uint32_t res01d0[12];
	/* Tx Statistics Counter */
	uint32_t teoct_l;		/**<Tx eth octets */
	uint32_t teoct_u;
	uint32_t toct_l;		/**<Tx Octets */
	uint32_t toct_u;
	uint32_t res0210[2];
	uint32_t txpf_l;		/**<Tx valid pause frame */
	uint32_t txpf_u;
	uint32_t tfrm_l;		/**<Tx frame counter */
	uint32_t tfrm_u;
	uint32_t tfcs_l;		/**<Tx FCS error */
	uint32_t tfcs_u;
	uint32_t tvlan_l;		/**<Tx Vlan Frame */
	uint32_t tvlan_u;
	uint32_t terr_l;		/**<Tx frame error */
	uint32_t terr_u;
	uint32_t tuca_l;		/**<Tx Unicast */
	uint32_t tuca_u;
	uint32_t tmca_l;		/**<Tx Multicast */
	uint32_t tmca_u;
	uint32_t tbca_l;		/**<Tx Broadcast */
	uint32_t tbca_u;
	uint32_t res0258[2];
	uint32_t tpkt_l;		/**<Tx Packet */
	uint32_t tpkt_u;
	uint32_t tund_l;		/**<Tx Undersized */
	uint32_t tund_u;
	uint32_t t64_l;
	uint32_t t64_u;
	uint32_t t127_l;
	uint32_t t127_u;
	uint32_t t255_l;
	uint32_t t255_u;
	uint32_t t511_l;
	uint32_t t511_u;
	uint32_t t1023_l;
	uint32_t t1023_u;
	uint32_t t1518_l;
	uint32_t t1518_u;
	uint32_t t1519x_l;
	uint32_t t1519x_u;
	uint32_t res02a8[6];
	uint32_t tcnp_l;		/**<Tx Control Packet type - 0x8808 */
	uint32_t tcnp_u;
	uint32_t res02c8[14];
	/* Line Interface Control */
	uint32_t if_mode;		/**< 0x300 Interface Mode Control */
	uint32_t if_status;		/**< 0x304 Interface Status */
	uint32_t res0308[14];
	/* HiGig/2 */
	uint32_t hg_config;		/**< 0x340 Control and cfg */
	uint32_t res0344[3];
	uint32_t hg_pause_quanta;	/**< 0x350 Pause quanta */
	uint32_t res0354[3];
	uint32_t hg_pause_thresh;	/**< 0x360 Pause quanta threshold */
	uint32_t res0364[3];
	uint32_t hgrx_pause_status;	/**< 0x370 Receive pause status */
	uint32_t hg_fifos_status;	/**< 0x374 fifos status */
	uint32_t rhm;			/**< 0x378 rx messages counter */
	uint32_t thm;			/**< 0x37C tx messages counter */
};

#define BMI_PORT_CFG_FDOVR 0x02000000

struct rx_bmi_regs {
	uint32_t fmbm_rcfg;		/**< Rx Configuration */
	uint32_t fmbm_rst;		/**< Rx Status */
	uint32_t fmbm_rda;		/**< Rx DMA attributes*/
	uint32_t fmbm_rfp;		/**< Rx FIFO Parameters*/
	uint32_t fmbm_rfed;		/**< Rx Frame End Data*/
	uint32_t fmbm_ricp;		/**< Rx Internal Context Parameters*/
	uint32_t fmbm_rim;		/**< Rx Internal Buffer Margins*/
	uint32_t fmbm_rebm;		/**< Rx External Buffer Margins*/
	uint32_t fmbm_rfne;		/**< Rx Frame Next Engine*/
	uint32_t fmbm_rfca;		/**< Rx Frame Command Attributes.*/
	uint32_t fmbm_rfpne;		/**< Rx Frame Parser Next Engine*/
	uint32_t fmbm_rpso;		/**< Rx Parse Start Offset*/
	uint32_t fmbm_rpp;		/**< Rx Policer Profile  */
	uint32_t fmbm_rccb;		/**< Rx Coarse Classification Base */
	uint32_t fmbm_reth;		/**< Rx Excessive Threshold */
	uint32_t reserved003c[1];	/**< (0x03C 0x03F) */
	uint32_t fmbm_rprai[FMAN_PORT_PRS_RESULT_WORDS_NUM];
					/**< Rx Parse Results Array Init*/
	uint32_t fmbm_rfqid;		/**< Rx Frame Queue ID*/
	uint32_t fmbm_refqid;		/**< Rx Error Frame Queue ID*/
	uint32_t fmbm_rfsdm;		/**< Rx Frame Status Discard Mask*/
	uint32_t fmbm_rfsem;		/**< Rx Frame Status Error Mask*/
	uint32_t fmbm_rfene;		/**< Rx Frame Enqueue Next Engine */
	uint32_t reserved0074[0x2];	/**< (0x074-0x07C)  */
	uint32_t fmbm_rcmne;
	/**< Rx Frame Continuous Mode Next Engine */
	uint32_t reserved0080[0x20];/**< (0x080 0x0FF)  */
	uint32_t fmbm_ebmpi[FMAN_PORT_MAX_EXT_POOLS_NUM];
					/**< Buffer Manager pool Information-*/
	uint32_t fmbm_acnt[FMAN_PORT_MAX_EXT_POOLS_NUM];
					/**< Allocate Counter-*/
	uint32_t reserved0120[16];
					/**< 0x130/0x140 - 0x15F reserved -*/
	uint32_t fmbm_rcgm[FMAN_PORT_CG_MAP_NUM];
					/**< Congestion Group Map*/
	uint32_t fmbm_mpd;		/**< BM Pool Depletion  */
	uint32_t reserved0184[0x1F];	/**< (0x184 0x1FF) */
	uint32_t fmbm_rstc;		/**< Rx Statistics Counters*/
	uint32_t fmbm_rfrc;		/**< Rx Frame Counter*/
	uint32_t fmbm_rfbc;		/**< Rx Bad Frames Counter*/
	uint32_t fmbm_rlfc;		/**< Rx Large Frames Counter*/
	uint32_t fmbm_rffc;		/**< Rx Filter Frames Counter*/
	uint32_t fmbm_rfdc;		/**< Rx Frame Discard Counter*/
	uint32_t fmbm_rfldec;		/**< Rx Frames List DMA Error Counter*/
	uint32_t fmbm_rodc;		/**< Rx Out of Buffers Discard nntr*/
	uint32_t fmbm_rbdc;		/**< Rx Buffers Deallocate Counter*/
	uint32_t reserved0224[0x17];	/**< (0x224 0x27F) */
	uint32_t fmbm_rpc;		/**< Rx Performance Counters*/
	uint32_t fmbm_rpcp;		/**< Rx Performance Count Parameters*/
	uint32_t fmbm_rccn;		/**< Rx Cycle Counter*/
	uint32_t fmbm_rtuc;		/**< Rx Tasks Utilization Counter*/
	uint32_t fmbm_rrquc;
	/**< Rx Receive Queue Utilization cntr*/
	uint32_t fmbm_rduc;		/**< Rx DMA Utilization Counter*/
	uint32_t fmbm_rfuc;		/**< Rx FIFO Utilization Counter*/
	uint32_t fmbm_rpac;		/**< Rx Pause Activation Counter*/
	uint32_t reserved02a0[0x18];	/**< (0x2A0 0x2FF) */
	uint32_t fmbm_rdbg;		/**< Rx Debug-*/
};

struct tx_bmi_regs {
	uint32_t fmbm_tcfg;		/**< Tx Configuration*/
	uint32_t fmbm_tst;		/**< Tx Status*/
	uint32_t fmbm_tda;		/**< Tx DMA attributes*/
	uint32_t fmbm_tfp;		/**< Tx FIFO Parameters*/
	uint32_t fmbm_tfed;		/**< Tx Frame End Data*/
	uint32_t fmbm_ticp;		/**< Tx Internal Context Parameters*/
	uint32_t fmbm_tfdne;		/**< Tx Frame Dequeue Next Engine*/
	uint32_t fmbm_tfca;		/**< Tx Frame Attributes*/
	uint32_t fmbm_tcfqid;		/**< Tx Confirmation Frame Queue ID*/
	uint32_t fmbm_tefqid;		/**< Tx Error Frame Queue ID*/
	uint32_t fmbm_tfene;		/**< Tx Frame Enqueue Next Engine*/
	uint32_t fmbm_trlmts;		/**< Tx Rate Limiter Scale*/
	uint32_t fmbm_trlmt;		/**< Tx Rate Limiter*/
};

/* Description FM RTC timer alarm */
struct t_tmr_alarm {
	uint32_t tmr_alarm_h;
	uint32_t tmr_alarm_l;
};

/* Description FM RTC timer Ex trigger */
struct t_tmr_ext_trigger {
	uint32_t tmr_etts_h;
	uint32_t tmr_etts_l;
};

struct rtc_regs {
	uint32_t tmr_id;	/* 0x000 Module ID register */
	uint32_t tmr_id2;	/* 0x004 Controller ID register */
	uint32_t reserved0008[30];
	uint32_t tmr_ctrl;	/* 0x0080 timer control register */
	uint32_t tmr_tevent;	/* 0x0084 timer event register */
	uint32_t tmr_temask;	/* 0x0088 timer event mask register */
	uint32_t reserved008c[3];
	uint32_t tmr_cnt_h;	/* 0x0098 timer counter high register */
	uint32_t tmr_cnt_l;	/* 0x009c timer counter low register */
	uint32_t tmr_add;	/* 0x00a0 timer drift compensation addend register */
	uint32_t tmr_acc;	/* 0x00a4 timer accumulator register */
	uint32_t tmr_prsc;	/* 0x00a8 timer prescale */
	uint32_t reserved00ac;
	uint32_t tmr_off_h;	/* 0x00b0 timer offset high */
	uint32_t tmr_off_l;	/* 0x00b4 timer offset low  */
	struct t_tmr_alarm tmr_alarm[FMAN_RTC_MAX_NUM_OF_ALARMS];
				/* 0x00b8 timer alarm */
	uint32_t tmr_fiper[FMAN_RTC_MAX_NUM_OF_PERIODIC_PULSES];
				/* 0x00d0 timer fixed period interval */
	struct t_tmr_ext_trigger tmr_etts[FMAN_RTC_MAX_NUM_OF_EXT_TRIGGERS];
				/* 0x00e0 time stamp general purpose external */
	uint32_t reserved00f0[4];
};

struct fman_port_qmi_regs {
	uint32_t fmqm_pnc;		/**< PortID n Configuration Register */
	uint32_t fmqm_pns;		/**< PortID n Status Register */
	uint32_t fmqm_pnts;		/**< PortID n Task Status Register */
	uint32_t reserved00c[4];	/**< 0xn00C - 0xn01B */
	uint32_t fmqm_pnen;		/**< PortID n Enqueue NIA Register */
	uint32_t fmqm_pnetfc;		/**< PortID n Enq Total Frame Counter */
	uint32_t reserved024[2];	/**< 0xn024 - 0x02B */
	uint32_t fmqm_pndn;		/**< PortID n Dequeue NIA Register */
	uint32_t fmqm_pndc;		/**< PortID n Dequeue Config Register */
	uint32_t fmqm_pndtfc;		/**< PortID n Dequeue tot Frame cntr */
	uint32_t fmqm_pndfdc;		/**< PortID n Dequeue FQID Dflt Cntr */
	uint32_t fmqm_pndcc;		/**< PortID n Dequeue Confirm Counter */
};

struct onic_port_cfg {
	char macless_name[IF_NAME_MAX_LEN];
	uint32_t rx_start;
	uint32_t rx_count;
	uint32_t tx_start;
	uint32_t tx_count;
	struct rte_ether_addr src_mac;
	struct rte_ether_addr peer_mac;
};

/* This struct exports parameters about an Fman network interface, determined
 * from the device-tree.
 */
struct fman_if {
	/* Which Fman this interface belongs to */
	struct __fman *fman;
	/* The type/speed of the interface */
	enum fman_mac_type mac_type;
	/* Boolean, set when mac type is memac */
	uint8_t is_memac;
	/* Boolean, set when PHY is RGMII */
	uint8_t is_rgmii;
	/* The index of this MAC (within the Fman it belongs to) */
	uint8_t mac_idx;
	/* The MAC address */
	struct rte_ether_addr mac_addr;
	/* The Qman channel to schedule Tx FQs to */
	u16 tx_channel_id;

	uint8_t base_profile_id;
	uint8_t num_profiles;

	uint8_t is_shared_mac;
	/* The hard-coded FQIDs for this interface. Note: this doesn't cover
	 * the PCD nor the "Rx default" FQIDs, which are configured via FMC
	 * and its XML-based configuration. These values are being parsed from
	 * kernel device tree.
	 */
	uint32_t fqid_rx_pcd;
	uint32_t fqid_rx_pcd_count;
	uint32_t fqid_rx_def;
	uint32_t fqid_rx_err;
	uint32_t fqid_tx_err;
	uint32_t fqid_tx_confirm;

	/* oNIC port info */
	struct onic_port_cfg onic_info;

	struct list_head bpool_list;
	/* The node for linking this interface into "fman_if_list" */
	struct list_head node;
};

/* This struct exposes parameters for buffer pools, extracted from the network
 * interface settings in the device tree.
 */
struct fman_if_bpool {
	uint32_t bpid;
	uint64_t count;
	uint64_t size;
	uint64_t addr;
	/* The node for linking this bpool into fman_if::bpool_list */
	struct list_head node;
};

/* Internal Context transfer params - FMBM_RICP*/
struct fman_if_ic_params {
	/*IC offset in the packet buffer */
	uint16_t iceof;
	/*IC internal offset */
	uint16_t iciof;
	/*IC size to copy */
	uint16_t icsz;
};

#define FMAN_ADDRESS_NUM 2
struct __fman {
	const struct device_node *fman_node;
	uint64_t ccsr_phy;
	uint64_t ccsr_size;
	void *ccsr_vir;

	uint64_t time_phy;
	uint64_t time_size;
	void *time_vir;

	uint8_t idx;
	uint32_t ip_rev;
	uint32_t dealloc_bufs_mask_hi;
	uint32_t dealloc_bufs_mask_lo;
	struct list_head node;
};

/* The exported "struct fman_if" type contains the subset of fields we want
 * exposed. This struct is embedded in a larger "struct __fman_if" which
 * contains the extra bits we *don't* want exposed.
 */
struct __fman_if {
	struct fman_if __if;
	char node_name[IF_NAME_MAX_LEN];
	char node_path[PATH_MAX];
	uint64_t regs_size;
	void *ccsr_map;
	void *bmi_map;
	void *tx_bmi_map;
	void *qmi_map;
};

/* And this is the base list node that the interfaces are added to. (See
 * fman_if_enable_all_rx() below for an example of its use.)
 */
extern const struct list_head *fman_if_list;

extern int fman_ccsr_map_fd;

/* To iterate the "bpool_list" for an interface. Eg;
 *        struct fman_if *p = get_ptr_to_some_interface();
 *        struct fman_if_bpool *bp;
 *        printf("Interface uses following BPIDs;\n");
 *        fman_if_for_each_bpool(bp, p) {
 *            printf("    %d\n", bp->bpid);
 *            [...]
 *        }
 */
#define fman_if_for_each_bpool(bp, __if) \
	list_for_each_entry(bp, &(__if)->bpool_list, node)

#define FMAN_ERR(rc, fmt, ...) \
	do { \
		_errno = (rc); \
		RTE_LOG_LINE(ERR, DPAA_BUS, fmt "(%d)", ##__VA_ARGS__, errno); \
	} while (0)

#define FMAN_IP_REV_1	0xC30C4
#define FMAN_IP_REV_1_MAJOR_MASK 0xff
#define FMAN_IP_REV_1_MAJOR_SHIFT 8
#define FMAN_V3	0x06

#define DPAA_FQD_CTX_A_SHIFT_BITS       24
#define DPAA_FQD_CTX_B_SHIFT_BITS       24

/* Following flags are used to set in context A hi field of FQD */
#define DPAA_FQD_CTX_A_OVERRIDE_FQ	(0x80 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_IGNORE_CMD	(0x40 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_A1_FIELD_VALID	(0x20 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_A2_FIELD_VALID	(0x10 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_A0_FIELD_VALID	(0x08 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_B0_FIELD_VALID	(0x04 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_OVERRIDE_OMB	(0x02 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A_RESERVED		(0x01 << DPAA_FQD_CTX_A_SHIFT_BITS)

/* Following flags are used to set in context A lo field of FQD */
#define DPAA_FQD_CTX_A2_EBD_BIT		(0x80 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_EBAD_BIT	(0x40 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_FWD_BIT		(0x20 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_NL_BIT		(0x10 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_CWD_BIT		(0x08 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_NENQ_BIT	(0x04 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_RESERVED_BIT	(0x02 << DPAA_FQD_CTX_A_SHIFT_BITS)
#define DPAA_FQD_CTX_A2_VSPE_BIT	(0x01 << DPAA_FQD_CTX_A_SHIFT_BITS)

/**
 * Initialize the FMAN driver
 *
 * @args void
 * @return
 *	0 for success; error OTHERWISE
 */
int fman_init(void);

/**
 * Teardown the FMAN driver
 *
 * @args void
 * @return void
 */
void fman_finish(void);

#endif	/* __FMAN_H */
