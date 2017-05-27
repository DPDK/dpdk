/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2017 Chelsio Communications.
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

#ifndef __CHELSIO_COMMON_H
#define __CHELSIO_COMMON_H

#include "cxgbe_compat.h"
#include "t4_hw.h"
#include "t4_chip_type.h"
#include "t4fw_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CXGBE_PAGE_SIZE RTE_PGSIZE_4K

enum {
	MAX_NPORTS     = 4,     /* max # of ports */
};

enum {
	T5_REGMAP_SIZE = (332 * 1024),
};

enum {
	MEMWIN0_APERTURE = 2048,
	MEMWIN0_BASE     = 0x1b800,
};

enum dev_master { MASTER_CANT, MASTER_MAY, MASTER_MUST };

enum dev_state { DEV_STATE_UNINIT, DEV_STATE_INIT, DEV_STATE_ERR };

enum {
	PAUSE_RX      = 1 << 0,
	PAUSE_TX      = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};

enum {
	FEC_RS        = 1 << 0,
	FEC_BASER_RS  = 1 << 1,
	FEC_RESERVED  = 1 << 2,
};

struct port_stats {
	u64 tx_octets;            /* total # of octets in good frames */
	u64 tx_frames;            /* all good frames */
	u64 tx_bcast_frames;      /* all broadcast frames */
	u64 tx_mcast_frames;      /* all multicast frames */
	u64 tx_ucast_frames;      /* all unicast frames */
	u64 tx_error_frames;      /* all error frames */

	u64 tx_frames_64;         /* # of Tx frames in a particular range */
	u64 tx_frames_65_127;
	u64 tx_frames_128_255;
	u64 tx_frames_256_511;
	u64 tx_frames_512_1023;
	u64 tx_frames_1024_1518;
	u64 tx_frames_1519_max;

	u64 tx_drop;              /* # of dropped Tx frames */
	u64 tx_pause;             /* # of transmitted pause frames */
	u64 tx_ppp0;              /* # of transmitted PPP prio 0 frames */
	u64 tx_ppp1;              /* # of transmitted PPP prio 1 frames */
	u64 tx_ppp2;              /* # of transmitted PPP prio 2 frames */
	u64 tx_ppp3;              /* # of transmitted PPP prio 3 frames */
	u64 tx_ppp4;              /* # of transmitted PPP prio 4 frames */
	u64 tx_ppp5;              /* # of transmitted PPP prio 5 frames */
	u64 tx_ppp6;              /* # of transmitted PPP prio 6 frames */
	u64 tx_ppp7;              /* # of transmitted PPP prio 7 frames */

	u64 rx_octets;            /* total # of octets in good frames */
	u64 rx_frames;            /* all good frames */
	u64 rx_bcast_frames;      /* all broadcast frames */
	u64 rx_mcast_frames;      /* all multicast frames */
	u64 rx_ucast_frames;      /* all unicast frames */
	u64 rx_too_long;          /* # of frames exceeding MTU */
	u64 rx_jabber;            /* # of jabber frames */
	u64 rx_fcs_err;           /* # of received frames with bad FCS */
	u64 rx_len_err;           /* # of received frames with length error */
	u64 rx_symbol_err;        /* symbol errors */
	u64 rx_runt;              /* # of short frames */

	u64 rx_frames_64;         /* # of Rx frames in a particular range */
	u64 rx_frames_65_127;
	u64 rx_frames_128_255;
	u64 rx_frames_256_511;
	u64 rx_frames_512_1023;
	u64 rx_frames_1024_1518;
	u64 rx_frames_1519_max;

	u64 rx_pause;             /* # of received pause frames */
	u64 rx_ppp0;              /* # of received PPP prio 0 frames */
	u64 rx_ppp1;              /* # of received PPP prio 1 frames */
	u64 rx_ppp2;              /* # of received PPP prio 2 frames */
	u64 rx_ppp3;              /* # of received PPP prio 3 frames */
	u64 rx_ppp4;              /* # of received PPP prio 4 frames */
	u64 rx_ppp5;              /* # of received PPP prio 5 frames */
	u64 rx_ppp6;              /* # of received PPP prio 6 frames */
	u64 rx_ppp7;              /* # of received PPP prio 7 frames */

	u64 rx_ovflow0;           /* drops due to buffer-group 0 overflows */
	u64 rx_ovflow1;           /* drops due to buffer-group 1 overflows */
	u64 rx_ovflow2;           /* drops due to buffer-group 2 overflows */
	u64 rx_ovflow3;           /* drops due to buffer-group 3 overflows */
	u64 rx_trunc0;            /* buffer-group 0 truncated packets */
	u64 rx_trunc1;            /* buffer-group 1 truncated packets */
	u64 rx_trunc2;            /* buffer-group 2 truncated packets */
	u64 rx_trunc3;            /* buffer-group 3 truncated packets */
};

struct sge_params {
	u32 hps;                        /* host page size for our PF/VF */
	u32 eq_qpp;                     /* egress queues/page for our PF/VF */
	u32 iq_qpp;                     /* egress queues/page for our PF/VF */
};

struct tp_params {
	unsigned int ntxchan;        /* # of Tx channels */
	unsigned int tre;            /* log2 of core clocks per TP tick */
	unsigned int dack_re;        /* DACK timer resolution */
	unsigned int la_mask;        /* what events are recorded by TP LA */
	unsigned short tx_modq[NCHAN];  /* channel to modulation queue map */

	u32 vlan_pri_map;               /* cached TP_VLAN_PRI_MAP */
	u32 ingress_config;             /* cached TP_INGRESS_CONFIG */

	/* cached TP_OUT_CONFIG compressed error vector
	 * and passing outer header info for encapsulated packets.
	 */
	int rx_pkt_encap;

	/*
	 * TP_VLAN_PRI_MAP Compressed Filter Tuple field offsets.  This is a
	 * subset of the set of fields which may be present in the Compressed
	 * Filter Tuple portion of filters and TCP TCB connections.  The
	 * fields which are present are controlled by the TP_VLAN_PRI_MAP.
	 * Since a variable number of fields may or may not be present, their
	 * shifted field positions within the Compressed Filter Tuple may
	 * vary, or not even be present if the field isn't selected in
	 * TP_VLAN_PRI_MAP.  Since some of these fields are needed in various
	 * places we store their offsets here, or a -1 if the field isn't
	 * present.
	 */
	int vlan_shift;
	int vnic_shift;
	int port_shift;
	int protocol_shift;
};

struct vpd_params {
	unsigned int cclk;
};

struct pci_params {
	uint16_t        vendor_id;
	uint16_t        device_id;
	uint32_t        vpd_cap_addr;
	uint16_t        speed;
	uint8_t         width;
};

/*
 * Firmware device log.
 */
struct devlog_params {
	u32 memtype;                    /* which memory (EDC0, EDC1, MC) */
	u32 start;                      /* start of log in firmware memory */
	u32 size;                       /* size of log */
};

struct arch_specific_params {
	u8 nchan;
	u16 mps_rplc_size;
	u16 vfcount;
	u32 sge_fl_db;
	u16 mps_tcam_size;
};

struct adapter_params {
	struct sge_params sge;
	struct tp_params  tp;
	struct vpd_params vpd;
	struct pci_params pci;
	struct devlog_params devlog;
	enum pcie_memwin drv_memwin;

	unsigned int sf_size;             /* serial flash size in bytes */
	unsigned int sf_nsec;             /* # of flash sectors */

	unsigned int fw_vers;
	unsigned int bs_vers;
	unsigned int tp_vers;
	unsigned int er_vers;

	unsigned short mtus[NMTUS];
	unsigned short a_wnd[NCCTRL_WIN];
	unsigned short b_wnd[NCCTRL_WIN];

	unsigned int mc_size;             /* MC memory size */
	unsigned int cim_la_size;

	unsigned char nports;             /* # of ethernet ports */
	unsigned char portvec;

	enum chip_type chip;              /* chip code */
	struct arch_specific_params arch; /* chip specific params */

	bool ulptx_memwrite_dsgl;          /* use of T5 DSGL allowed */
};

struct link_config {
	unsigned short supported;        /* link capabilities */
	unsigned short advertising;      /* advertised capabilities */
	unsigned int   requested_speed;  /* speed user has requested */
	unsigned int   speed;            /* actual link speed */
	unsigned char  requested_fc;     /* flow control user has requested */
	unsigned char  fc;               /* actual link flow control */
	unsigned char  requested_fec;    /* Forward Error Correction user */
	unsigned char  fec;              /* has requested and actual FEC */
	unsigned char  autoneg;          /* autonegotiating? */
	unsigned char  link_ok;          /* link up? */
};

#include "adapter.h"

void t4_set_reg_field(struct adapter *adap, unsigned int addr, u32 mask,
		      u32 val);
int t4_wait_op_done_val(struct adapter *adapter, int reg, u32 mask,
			int polarity,
			int attempts, int delay, u32 *valp);

static inline int t4_wait_op_done(struct adapter *adapter, int reg, u32 mask,
				  int polarity, int attempts, int delay)
{
	return t4_wait_op_done_val(adapter, reg, mask, polarity, attempts,
				   delay, NULL);
}

#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; ++iter)

void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log);
void t4_tp_wr_bits_indirect(struct adapter *adap, unsigned int addr,
			    unsigned int mask, unsigned int val);
void t4_intr_enable(struct adapter *adapter);
void t4_intr_disable(struct adapter *adapter);
int t4_link_l1cfg(struct adapter *adap, unsigned int mbox, unsigned int port,
		  struct link_config *lc);
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta);
int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state);
int t4_fw_bye(struct adapter *adap, unsigned int mbox);
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset);
int t4_fw_halt(struct adapter *adap, unsigned int mbox, int reset);
int t4_fw_restart(struct adapter *adap, unsigned int mbox, int reset);
int t4_fl_pkt_align(struct adapter *adap);
int t4_fixup_host_params_compat(struct adapter *adap, unsigned int page_size,
				unsigned int cache_line_size,
				enum chip_type chip_compat);
int t4_fixup_host_params(struct adapter *adap, unsigned int page_size,
			 unsigned int cache_line_size);
int t4_fw_initialize(struct adapter *adap, unsigned int mbox);
int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val);
int t4_set_params_timeout(struct adapter *adap, unsigned int mbox,
			  unsigned int pf, unsigned int vf,
			  unsigned int nparams, const u32 *params,
			  const u32 *val, int timeout);
int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val);
int t4_alloc_vi_func(struct adapter *adap, unsigned int mbox,
		     unsigned int port, unsigned int pf, unsigned int vf,
		     unsigned int nmac, u8 *mac, unsigned int *rss_size,
		     unsigned int portfunc, unsigned int idstype);
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		unsigned int *rss_size);
int t4_free_vi(struct adapter *adap, unsigned int mbox,
	       unsigned int pf, unsigned int vf,
	       unsigned int viid);
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int mtu, int promisc, int all_multi, int bcast, int vlanex,
		  bool sleep_ok);
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, bool add_smt);
int t4_enable_vi_params(struct adapter *adap, unsigned int mbox,
			unsigned int viid, bool rx_en, bool tx_en, bool dcb_en);
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en);
int t4_iq_start_stop(struct adapter *adap, unsigned int mbox, bool start,
		     unsigned int pf, unsigned int vf, unsigned int iqid,
		     unsigned int fl0id, unsigned int fl1id);
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id);
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid);

static inline unsigned int core_ticks_per_usec(const struct adapter *adap)
{
	return adap->params.vpd.cclk / 1000;
}

static inline unsigned int us_to_core_ticks(const struct adapter *adap,
					    unsigned int us)
{
	return (us * adap->params.vpd.cclk) / 1000;
}

static inline unsigned int core_ticks_to_us(const struct adapter *adapter,
					    unsigned int ticks)
{
	/* add Core Clock / 2 to round ticks to nearest uS */
	return ((ticks * 1000 + adapter->params.vpd.cclk / 2) /
		adapter->params.vpd.cclk);
}

int t4_wr_mbox_meat_timeout(struct adapter *adap, int mbox, const void *cmd,
			    int size, void *rpl, bool sleep_ok, int timeout);
int t4_wr_mbox_meat(struct adapter *adap, int mbox,
		    const void __attribute__((__may_alias__)) *cmd, int size,
		    void *rpl, bool sleep_ok);

static inline int t4_wr_mbox_timeout(struct adapter *adap, int mbox,
				     const void *cmd, int size, void *rpl,
				     int timeout)
{
	return t4_wr_mbox_meat_timeout(adap, mbox, cmd, size, rpl, true,
				       timeout);
}

int t4_get_core_clock(struct adapter *adapter, struct vpd_params *p);

static inline int t4_wr_mbox(struct adapter *adap, int mbox, const void *cmd,
			     int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, true);
}

static inline int t4_wr_mbox_ns(struct adapter *adap, int mbox, const void *cmd,
				int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, false);
}

void t4_read_indirect(struct adapter *adap, unsigned int addr_reg,
		      unsigned int data_reg, u32 *vals, unsigned int nregs,
		      unsigned int start_idx);
void t4_write_indirect(struct adapter *adap, unsigned int addr_reg,
		       unsigned int data_reg, const u32 *vals,
		       unsigned int nregs, unsigned int start_idx);

int t4_get_vpd_params(struct adapter *adapter, struct vpd_params *p);
int t4_read_flash(struct adapter *adapter, unsigned int addr,
		  unsigned int nwords, u32 *data, int byte_oriented);
int t4_flash_cfg_addr(struct adapter *adapter);
unsigned int t4_get_mps_bg_map(struct adapter *adapter, unsigned int pidx);
unsigned int t4_get_tp_ch_map(struct adapter *adapter, unsigned int pidx);
const char *t4_get_port_type_description(enum fw_port_type port_type);
void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p);
void t4_get_port_stats_offset(struct adapter *adap, int idx,
			      struct port_stats *stats,
			      struct port_stats *offset);
void t4_clr_port_stats(struct adapter *adap, int idx);
void t4_reset_link_config(struct adapter *adap, int idx);
int t4_get_version_info(struct adapter *adapter);
void t4_dump_version_info(struct adapter *adapter);
int t4_get_flash_params(struct adapter *adapter);
int t4_get_chip_type(struct adapter *adap, int ver);
int t4_prep_adapter(struct adapter *adapter);
int t4_port_init(struct adapter *adap, int mbox, int pf, int vf);
int t4_init_rss_mode(struct adapter *adap, int mbox);
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq);
int t4_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
		     unsigned int flags, unsigned int defq);

enum t4_bar2_qtype { T4_BAR2_QTYPE_EGRESS, T4_BAR2_QTYPE_INGRESS };
int t4_bar2_sge_qregs(struct adapter *adapter, unsigned int qid,
		      unsigned int qtype, u64 *pbar2_qoffset,
		      unsigned int *pbar2_qid);

int t4_init_sge_params(struct adapter *adapter);
int t4_init_tp_params(struct adapter *adap);
int t4_filter_field_shift(const struct adapter *adap, unsigned int filter_sel);
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl);
unsigned int t4_get_regs_len(struct adapter *adap);
void t4_get_regs(struct adapter *adap, void *buf, size_t buf_size);
int t4_seeprom_read(struct adapter *adapter, u32 addr, u32 *data);
int t4_seeprom_write(struct adapter *adapter, u32 addr, u32 data);
int t4_seeprom_wp(struct adapter *adapter, int enable);
#endif /* __CHELSIO_COMMON_H */
