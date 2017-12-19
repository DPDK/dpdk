/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2017 NXP
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <fman.h>
/* This header declares things about Fman hardware itself (the format of status
 * words and an inline implementation of CRC64). We include it only in order to
 * instantiate the one global variable it depends on.
 */
#include <fsl_fman.h>
#include <fsl_fman_crc64.h>
#include <fsl_bman.h>

/* Instantiate the global variable that the inline CRC64 implementation (in
 * <fsl_fman.h>) depends on.
 */
DECLARE_FMAN_CRC64_TABLE();

#define ETH_ADDR_TO_UINT64(eth_addr)                  \
	(uint64_t)(((uint64_t)(eth_addr)[0] << 40) |   \
	((uint64_t)(eth_addr)[1] << 32) |   \
	((uint64_t)(eth_addr)[2] << 24) |   \
	((uint64_t)(eth_addr)[3] << 16) |   \
	((uint64_t)(eth_addr)[4] << 8) |    \
	((uint64_t)(eth_addr)[5]))

void
fman_if_set_mcast_filter_table(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	void *hashtable_ctrl;
	uint32_t i;

	hashtable_ctrl = &((struct memac_regs *)__if->ccsr_map)->hashtable_ctrl;
	for (i = 0; i < 64; i++)
		out_be32(hashtable_ctrl, i|HASH_CTRL_MCAST_EN);
}

void
fman_if_reset_mcast_filter_table(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	void *hashtable_ctrl;
	uint32_t i;

	hashtable_ctrl = &((struct memac_regs *)__if->ccsr_map)->hashtable_ctrl;
	for (i = 0; i < 64; i++)
		out_be32(hashtable_ctrl, i & ~HASH_CTRL_MCAST_EN);
}

static
uint32_t get_mac_hash_code(uint64_t eth_addr)
{
	uint64_t	mask1, mask2;
	uint32_t	xorVal = 0;
	uint8_t		i, j;

	for (i = 0; i < 6; i++) {
		mask1 = eth_addr & (uint64_t)0x01;
		eth_addr >>= 1;

		for (j = 0; j < 7; j++) {
			mask2 = eth_addr & (uint64_t)0x01;
			mask1 ^= mask2;
			eth_addr >>= 1;
		}

		xorVal |= (mask1 << (5 - i));
	}

	return xorVal;
}

int
fman_if_add_hash_mac_addr(struct fman_if *p, uint8_t *eth)
{
	uint64_t eth_addr;
	void *hashtable_ctrl;
	uint32_t hash;

	struct __fman_if *__if = container_of(p, struct __fman_if, __if);

	eth_addr = ETH_ADDR_TO_UINT64(eth);

	if (!(eth_addr & GROUP_ADDRESS))
		return -1;

	hash = get_mac_hash_code(eth_addr) & HASH_CTRL_ADDR_MASK;
	hash = hash | HASH_CTRL_MCAST_EN;

	hashtable_ctrl = &((struct memac_regs *)__if->ccsr_map)->hashtable_ctrl;
	out_be32(hashtable_ctrl, hash);

	return 0;
}

int
fman_if_get_primary_mac_addr(struct fman_if *p, uint8_t *eth)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	void *mac_reg =
		&((struct memac_regs *)__if->ccsr_map)->mac_addr0.mac_addr_l;
	u32 val = in_be32(mac_reg);

	eth[0] = (val & 0x000000ff) >> 0;
	eth[1] = (val & 0x0000ff00) >> 8;
	eth[2] = (val & 0x00ff0000) >> 16;
	eth[3] = (val & 0xff000000) >> 24;

	mac_reg =  &((struct memac_regs *)__if->ccsr_map)->mac_addr0.mac_addr_u;
	val = in_be32(mac_reg);

	eth[4] = (val & 0x000000ff) >> 0;
	eth[5] = (val & 0x0000ff00) >> 8;

	return 0;
}

void
fman_if_clear_mac_addr(struct fman_if *p, uint8_t addr_num)
{
	struct __fman_if *m = container_of(p, struct __fman_if, __if);
	void *reg;

	if (addr_num) {
		reg = &((struct memac_regs *)m->ccsr_map)->
				mac_addr[addr_num-1].mac_addr_l;
		out_be32(reg, 0x0);
		reg = &((struct memac_regs *)m->ccsr_map)->
					mac_addr[addr_num-1].mac_addr_u;
		out_be32(reg, 0x0);
	} else {
		reg = &((struct memac_regs *)m->ccsr_map)->mac_addr0.mac_addr_l;
		out_be32(reg, 0x0);
		reg = &((struct memac_regs *)m->ccsr_map)->mac_addr0.mac_addr_u;
		out_be32(reg, 0x0);
	}
}

int
fman_if_add_mac_addr(struct fman_if *p, uint8_t *eth, uint8_t addr_num)
{
	struct __fman_if *m = container_of(p, struct __fman_if, __if);

	void *reg;
	u32 val;

	memcpy(&m->__if.mac_addr, eth, ETHER_ADDR_LEN);

	if (addr_num)
		reg = &((struct memac_regs *)m->ccsr_map)->
					mac_addr[addr_num-1].mac_addr_l;
	else
		reg = &((struct memac_regs *)m->ccsr_map)->mac_addr0.mac_addr_l;

	val = (m->__if.mac_addr.addr_bytes[0] |
	       (m->__if.mac_addr.addr_bytes[1] << 8) |
	       (m->__if.mac_addr.addr_bytes[2] << 16) |
	       (m->__if.mac_addr.addr_bytes[3] << 24));
	out_be32(reg, val);

	if (addr_num)
		reg = &((struct memac_regs *)m->ccsr_map)->
					mac_addr[addr_num-1].mac_addr_u;
	else
		reg = &((struct memac_regs *)m->ccsr_map)->mac_addr0.mac_addr_u;

	val = ((m->__if.mac_addr.addr_bytes[4] << 0) |
	       (m->__if.mac_addr.addr_bytes[5] << 8));
	out_be32(reg, val);

	return 0;
}

void
fman_if_set_rx_ignore_pause_frames(struct fman_if *p, bool enable)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	u32 value = 0;
	void *cmdcfg;

	assert(fman_ccsr_map_fd != -1);

	/* Set Rx Ignore Pause Frames */
	cmdcfg = &((struct memac_regs *)__if->ccsr_map)->command_config;
	if (enable)
		value = in_be32(cmdcfg) | CMD_CFG_PAUSE_IGNORE;
	else
		value = in_be32(cmdcfg) & ~CMD_CFG_PAUSE_IGNORE;

	out_be32(cmdcfg, value);
}

void
fman_if_conf_max_frame_len(struct fman_if *p, unsigned int max_frame_len)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	unsigned int *maxfrm;

	assert(fman_ccsr_map_fd != -1);

	/* Set Max frame length */
	maxfrm = &((struct memac_regs *)__if->ccsr_map)->maxfrm;
	out_be32(maxfrm, (MAXFRM_RX_MASK & max_frame_len));
}

void
fman_if_stats_get(struct fman_if *p, struct rte_eth_stats *stats)
{
	struct __fman_if *m = container_of(p, struct __fman_if, __if);
	struct memac_regs *regs = m->ccsr_map;

	/* read recved packet count */
	stats->ipackets = ((u64)in_be32(&regs->rfrm_u)) << 32 |
			in_be32(&regs->rfrm_l);
	stats->ibytes = ((u64)in_be32(&regs->roct_u)) << 32 |
			in_be32(&regs->roct_l);
	stats->ierrors = ((u64)in_be32(&regs->rerr_u)) << 32 |
			in_be32(&regs->rerr_l);

	/* read xmited packet count */
	stats->opackets = ((u64)in_be32(&regs->tfrm_u)) << 32 |
			in_be32(&regs->tfrm_l);
	stats->obytes = ((u64)in_be32(&regs->toct_u)) << 32 |
			in_be32(&regs->toct_l);
	stats->oerrors = ((u64)in_be32(&regs->terr_u)) << 32 |
			in_be32(&regs->terr_l);
}

void
fman_if_stats_get_all(struct fman_if *p, uint64_t *value, int n)
{
	struct __fman_if *m = container_of(p, struct __fman_if, __if);
	struct memac_regs *regs = m->ccsr_map;
	int i;
	uint64_t base_offset = offsetof(struct memac_regs, reoct_l);

	for (i = 0; i < n; i++)
		value[i] = ((u64)in_be32((char *)regs
				+ base_offset + 8 * i + 4)) << 32 |
				((u64)in_be32((char *)regs
				+ base_offset + 8 * i));
}

void
fman_if_stats_reset(struct fman_if *p)
{
	struct __fman_if *m = container_of(p, struct __fman_if, __if);
	struct memac_regs *regs = m->ccsr_map;
	uint32_t tmp;

	tmp = in_be32(&regs->statn_config);

	tmp |= STATS_CFG_CLR;

	out_be32(&regs->statn_config, tmp);

	while (in_be32(&regs->statn_config) & STATS_CFG_CLR)
		;
}

void
fman_if_promiscuous_enable(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	void *cmdcfg;

	assert(fman_ccsr_map_fd != -1);

	/* Enable Rx promiscuous mode */
	cmdcfg = &((struct memac_regs *)__if->ccsr_map)->command_config;
	out_be32(cmdcfg, in_be32(cmdcfg) | CMD_CFG_PROMIS_EN);
}

void
fman_if_promiscuous_disable(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);
	void *cmdcfg;

	assert(fman_ccsr_map_fd != -1);

	/* Disable Rx promiscuous mode */
	cmdcfg = &((struct memac_regs *)__if->ccsr_map)->command_config;
	out_be32(cmdcfg, in_be32(cmdcfg) & (~CMD_CFG_PROMIS_EN));
}

void
fman_if_enable_rx(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	/* enable Rx and Tx */
	out_be32(__if->ccsr_map + 8, in_be32(__if->ccsr_map + 8) | 3);
}

void
fman_if_disable_rx(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	/* only disable Rx, not Tx */
	out_be32(__if->ccsr_map + 8, in_be32(__if->ccsr_map + 8) & ~(u32)2);
}

void
fman_if_loopback_enable(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	/* Enable loopback mode */
	if ((__if->__if.is_memac) && (__if->__if.is_rgmii)) {
		unsigned int *ifmode =
			&((struct memac_regs *)__if->ccsr_map)->if_mode;
		out_be32(ifmode, in_be32(ifmode) | IF_MODE_RLP);
	} else{
		unsigned int *cmdcfg =
			&((struct memac_regs *)__if->ccsr_map)->command_config;
		out_be32(cmdcfg, in_be32(cmdcfg) | CMD_CFG_LOOPBACK_EN);
	}
}

void
fman_if_loopback_disable(struct fman_if *p)
{
	struct __fman_if *__if = container_of(p, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);
	/* Disable loopback mode */
	if ((__if->__if.is_memac) && (__if->__if.is_rgmii)) {
		unsigned int *ifmode =
			&((struct memac_regs *)__if->ccsr_map)->if_mode;
		out_be32(ifmode, in_be32(ifmode) & ~IF_MODE_RLP);
	} else {
		unsigned int *cmdcfg =
			&((struct memac_regs *)__if->ccsr_map)->command_config;
		out_be32(cmdcfg, in_be32(cmdcfg) & ~CMD_CFG_LOOPBACK_EN);
	}
}

void
fman_if_set_bp(struct fman_if *fm_if, unsigned num __always_unused,
		    int bpid, size_t bufsize)
{
	u32 fmbm_ebmpi;
	u32 ebmpi_val_ace = 0xc0000000;
	u32 ebmpi_mask = 0xffc00000;

	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	fmbm_ebmpi =
	       in_be32(&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_ebmpi[0]);
	fmbm_ebmpi = ebmpi_val_ace | (fmbm_ebmpi & ebmpi_mask) | (bpid << 16) |
		     (bufsize);

	out_be32(&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_ebmpi[0],
		 fmbm_ebmpi);
}

int
fman_if_get_fc_threshold(struct fman_if *fm_if)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *fmbm_mpd;

	assert(fman_ccsr_map_fd != -1);

	fmbm_mpd = &((struct rx_bmi_regs *)__if->bmi_map)->fmbm_mpd;
	return in_be32(fmbm_mpd);
}

int
fman_if_set_fc_threshold(struct fman_if *fm_if, u32 high_water,
			 u32 low_water, u32 bpid)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *fmbm_mpd;

	assert(fman_ccsr_map_fd != -1);

	fmbm_mpd = &((struct rx_bmi_regs *)__if->bmi_map)->fmbm_mpd;
	out_be32(fmbm_mpd, FMAN_ENABLE_BPOOL_DEPLETION);
	return bm_pool_set_hw_threshold(bpid, low_water, high_water);

}

int
fman_if_get_fc_quanta(struct fman_if *fm_if)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	return in_be32(&((struct memac_regs *)__if->ccsr_map)->pause_quanta[0]);
}

int
fman_if_set_fc_quanta(struct fman_if *fm_if, u16 pause_quanta)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	out_be32(&((struct memac_regs *)__if->ccsr_map)->pause_quanta[0],
		 pause_quanta);
	return 0;
}

int
fman_if_get_fdoff(struct fman_if *fm_if)
{
	u32 fmbm_ricp;
	int fdoff;
	int iceof_mask = 0x001f0000;
	int icsz_mask = 0x0000001f;

	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	fmbm_ricp =
		   in_be32(&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_ricp);
	/*iceof + icsz*/
	fdoff = ((fmbm_ricp & iceof_mask) >> 16) * 16 +
		(fmbm_ricp & icsz_mask) * 16;

	return fdoff;
}

void
fman_if_set_err_fqid(struct fman_if *fm_if, uint32_t err_fqid)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);

	assert(fman_ccsr_map_fd != -1);

	unsigned int *fmbm_refqid =
			&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_refqid;
	out_be32(fmbm_refqid, err_fqid);
}

int
fman_if_get_ic_params(struct fman_if *fm_if, struct fman_if_ic_params *icp)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	int val = 0;
	int iceof_mask = 0x001f0000;
	int icsz_mask = 0x0000001f;
	int iciof_mask = 0x00000f00;

	assert(fman_ccsr_map_fd != -1);

	unsigned int *fmbm_ricp =
		&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_ricp;
	val = in_be32(fmbm_ricp);

	icp->iceof = (val & iceof_mask) >> 12;
	icp->iciof = (val & iciof_mask) >> 4;
	icp->icsz = (val & icsz_mask) << 4;

	return 0;
}

int
fman_if_set_ic_params(struct fman_if *fm_if,
			  const struct fman_if_ic_params *icp)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	int val = 0;
	int iceof_mask = 0x001f0000;
	int icsz_mask = 0x0000001f;
	int iciof_mask = 0x00000f00;

	assert(fman_ccsr_map_fd != -1);

	val |= (icp->iceof << 12) & iceof_mask;
	val |= (icp->iciof << 4) & iciof_mask;
	val |= (icp->icsz >> 4) & icsz_mask;

	unsigned int *fmbm_ricp =
		&((struct rx_bmi_regs *)__if->bmi_map)->fmbm_ricp;
	out_be32(fmbm_ricp, val);

	return 0;
}

void
fman_if_set_fdoff(struct fman_if *fm_if, uint32_t fd_offset)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *fmbm_rebm;

	assert(fman_ccsr_map_fd != -1);

	fmbm_rebm = &((struct rx_bmi_regs *)__if->bmi_map)->fmbm_rebm;

	out_be32(fmbm_rebm, in_be32(fmbm_rebm) | (fd_offset << 16));
}

void
fman_if_set_maxfrm(struct fman_if *fm_if, uint16_t max_frm)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *reg_maxfrm;

	assert(fman_ccsr_map_fd != -1);

	reg_maxfrm = &((struct memac_regs *)__if->ccsr_map)->maxfrm;

	out_be32(reg_maxfrm, (in_be32(reg_maxfrm) & 0xFFFF0000) | max_frm);
}

uint16_t
fman_if_get_maxfrm(struct fman_if *fm_if)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *reg_maxfrm;

	assert(fman_ccsr_map_fd != -1);

	reg_maxfrm = &((struct memac_regs *)__if->ccsr_map)->maxfrm;

	return (in_be32(reg_maxfrm) | 0x0000FFFF);
}

void
fman_if_set_dnia(struct fman_if *fm_if, uint32_t nia)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *fmqm_pndn;

	assert(fman_ccsr_map_fd != -1);

	fmqm_pndn = &((struct fman_port_qmi_regs *)__if->qmi_map)->fmqm_pndn;

	out_be32(fmqm_pndn, nia);
}

void
fman_if_discard_rx_errors(struct fman_if *fm_if)
{
	struct __fman_if *__if = container_of(fm_if, struct __fman_if, __if);
	unsigned int *fmbm_rfsdm, *fmbm_rfsem;

	fmbm_rfsem = &((struct rx_bmi_regs *)__if->bmi_map)->fmbm_rfsem;
	out_be32(fmbm_rfsem, 0);

	/* Configure the discard mask to discard the error packets which have
	 * DMA errors, Frame size error, Header error etc. The mask 0x010CE3F0
	 * is to configured discard all the errors which come in the FD[STATUS]
	 */
	fmbm_rfsdm = &((struct rx_bmi_regs *)__if->bmi_map)->fmbm_rfsdm;
	out_be32(fmbm_rfsdm, 0x010CE3F0);
}
