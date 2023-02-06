/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <rte_common.h>
#include <rte_byteorder.h>
#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp_nsp.h"
#include "nfp6000/nfp6000.h"

#define GENMASK_ULL(h, l) \
	(((~0ULL) - (1ULL << (l)) + 1) & \
	 (~0ULL >> (64 - 1 - (h))))

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define FIELD_GET(_mask, _reg)						\
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		(typeof(_x))(((_reg) & (_x)) >> __bf_shf(_x));	\
	}))

#define FIELD_FIT(_mask, _val)						\
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		!((((typeof(_x))_val) << __bf_shf(_x)) & ~(_x)); \
	}))

#define FIELD_PREP(_mask, _val)						\
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		((typeof(_x))(_val) << __bf_shf(_x)) & (_x);	\
	}))

#define NSP_ETH_NBI_PORT_COUNT		24
#define NSP_ETH_MAX_COUNT		(2 * NSP_ETH_NBI_PORT_COUNT)
#define NSP_ETH_TABLE_SIZE		(NSP_ETH_MAX_COUNT *		\
					 sizeof(union eth_table_entry))

#define NSP_ETH_PORT_LANES		GENMASK_ULL(3, 0)
#define NSP_ETH_PORT_INDEX		GENMASK_ULL(15, 8)
#define NSP_ETH_PORT_LABEL		GENMASK_ULL(53, 48)
#define NSP_ETH_PORT_PHYLABEL		GENMASK_ULL(59, 54)
#define NSP_ETH_PORT_FEC_SUPP_BASER	BIT_ULL(60)
#define NSP_ETH_PORT_FEC_SUPP_RS	BIT_ULL(61)

#define NSP_ETH_PORT_LANES_MASK		rte_cpu_to_le_64(NSP_ETH_PORT_LANES)

#define NSP_ETH_STATE_CONFIGURED	BIT_ULL(0)
#define NSP_ETH_STATE_ENABLED		BIT_ULL(1)
#define NSP_ETH_STATE_TX_ENABLED	BIT_ULL(2)
#define NSP_ETH_STATE_RX_ENABLED	BIT_ULL(3)
#define NSP_ETH_STATE_RATE		GENMASK_ULL(11, 8)
#define NSP_ETH_STATE_INTERFACE		GENMASK_ULL(19, 12)
#define NSP_ETH_STATE_MEDIA		GENMASK_ULL(21, 20)
#define NSP_ETH_STATE_OVRD_CHNG		BIT_ULL(22)
#define NSP_ETH_STATE_ANEG		GENMASK_ULL(25, 23)
#define NSP_ETH_STATE_FEC		GENMASK_ULL(27, 26)

#define NSP_ETH_CTRL_CONFIGURED		BIT_ULL(0)
#define NSP_ETH_CTRL_ENABLED		BIT_ULL(1)
#define NSP_ETH_CTRL_TX_ENABLED		BIT_ULL(2)
#define NSP_ETH_CTRL_RX_ENABLED		BIT_ULL(3)
#define NSP_ETH_CTRL_SET_RATE		BIT_ULL(4)
#define NSP_ETH_CTRL_SET_LANES		BIT_ULL(5)
#define NSP_ETH_CTRL_SET_ANEG		BIT_ULL(6)
#define NSP_ETH_CTRL_SET_FEC		BIT_ULL(7)

/* Which connector port. */
#define PORT_TP			0x00
#define PORT_AUI		0x01
#define PORT_MII		0x02
#define PORT_FIBRE		0x03
#define PORT_BNC		0x04
#define PORT_DA			0x05
#define PORT_NONE		0xef
#define PORT_OTHER		0xff

#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_2500		2500
#define SPEED_5000		5000
#define SPEED_10000		10000
#define SPEED_14000		14000
#define SPEED_20000		20000
#define SPEED_25000		25000
#define SPEED_40000		40000
#define SPEED_50000		50000
#define SPEED_56000		56000
#define SPEED_100000		100000

enum nfp_eth_raw {
	NSP_ETH_RAW_PORT = 0,
	NSP_ETH_RAW_STATE,
	NSP_ETH_RAW_MAC,
	NSP_ETH_RAW_CONTROL,

	NSP_ETH_NUM_RAW
};

enum nfp_eth_rate {
	RATE_INVALID = 0,
	RATE_10M,
	RATE_100M,
	RATE_1G,
	RATE_10G,
	RATE_25G,
};

union eth_table_entry {
	struct {
		uint64_t port;
		uint64_t state;
		uint8_t mac_addr[6];
		uint8_t resv[2];
		uint64_t control;
	};
	uint64_t raw[NSP_ETH_NUM_RAW];
};

static const struct {
	enum nfp_eth_rate rate;
	unsigned int speed;
} nsp_eth_rate_tbl[] = {
	{ RATE_INVALID,	0, },
	{ RATE_10M,	SPEED_10, },
	{ RATE_100M,	SPEED_100, },
	{ RATE_1G,	SPEED_1000, },
	{ RATE_10G,	SPEED_10000, },
	{ RATE_25G,	SPEED_25000, },
};

static unsigned int
nfp_eth_rate2speed(enum nfp_eth_rate rate)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(nsp_eth_rate_tbl); i++)
		if (nsp_eth_rate_tbl[i].rate == rate)
			return nsp_eth_rate_tbl[i].speed;

	return 0;
}

static unsigned int
nfp_eth_speed2rate(unsigned int speed)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(nsp_eth_rate_tbl); i++)
		if (nsp_eth_rate_tbl[i].speed == speed)
			return nsp_eth_rate_tbl[i].rate;

	return RATE_INVALID;
}

static void
nfp_eth_copy_mac_reverse(uint8_t *dst, const uint8_t *src)
{
	int i;

	for (i = 0; i < (int)ETH_ALEN; i++)
		dst[ETH_ALEN - i - 1] = src[i];
}

static void
nfp_eth_port_translate(struct nfp_nsp *nsp, const union eth_table_entry *src,
		       unsigned int index, struct nfp_eth_table_port *dst)
{
	unsigned int rate;
	unsigned int fec;
	uint64_t port, state;

	port = rte_le_to_cpu_64(src->port);
	state = rte_le_to_cpu_64(src->state);

	dst->eth_index = FIELD_GET(NSP_ETH_PORT_INDEX, port);
	dst->index = index;
	dst->nbi = index / NSP_ETH_NBI_PORT_COUNT;
	dst->base = index % NSP_ETH_NBI_PORT_COUNT;
	dst->lanes = FIELD_GET(NSP_ETH_PORT_LANES, port);

	dst->enabled = FIELD_GET(NSP_ETH_STATE_ENABLED, state);
	dst->tx_enabled = FIELD_GET(NSP_ETH_STATE_TX_ENABLED, state);
	dst->rx_enabled = FIELD_GET(NSP_ETH_STATE_RX_ENABLED, state);

	rate = nfp_eth_rate2speed(FIELD_GET(NSP_ETH_STATE_RATE, state));
	dst->speed = dst->lanes * rate;

	dst->interface = FIELD_GET(NSP_ETH_STATE_INTERFACE, state);
	dst->media = FIELD_GET(NSP_ETH_STATE_MEDIA, state);

	nfp_eth_copy_mac_reverse(dst->mac_addr, src->mac_addr);

	dst->label_port = FIELD_GET(NSP_ETH_PORT_PHYLABEL, port);
	dst->label_subport = FIELD_GET(NSP_ETH_PORT_LABEL, port);

	if (nfp_nsp_get_abi_ver_minor(nsp) < 17)
		return;

	dst->override_changed = FIELD_GET(NSP_ETH_STATE_OVRD_CHNG, state);
	dst->aneg = FIELD_GET(NSP_ETH_STATE_ANEG, state);

	if (nfp_nsp_get_abi_ver_minor(nsp) < 22)
		return;

	fec = FIELD_GET(NSP_ETH_PORT_FEC_SUPP_BASER, port);
	dst->fec_modes_supported |= fec << NFP_FEC_BASER_BIT;
	fec = FIELD_GET(NSP_ETH_PORT_FEC_SUPP_RS, port);
	dst->fec_modes_supported |= fec << NFP_FEC_REED_SOLOMON_BIT;
	if (dst->fec_modes_supported)
		dst->fec_modes_supported |= NFP_FEC_AUTO | NFP_FEC_DISABLED;

	dst->fec = 1 << FIELD_GET(NSP_ETH_STATE_FEC, state);
}

static void
nfp_eth_calc_port_geometry(struct nfp_eth_table *table)
{
	unsigned int i, j;

	for (i = 0; i < table->count; i++) {
		table->max_index = RTE_MAX(table->max_index,
					   table->ports[i].index);

		for (j = 0; j < table->count; j++) {
			if (table->ports[i].label_port !=
			    table->ports[j].label_port)
				continue;
			table->ports[i].port_lanes += table->ports[j].lanes;

			if (i == j)
				continue;
			if (table->ports[i].label_subport ==
			    table->ports[j].label_subport)
				PMD_DRV_LOG(DEBUG, "Port %d subport %d is a duplicate",
					 table->ports[i].label_port,
					 table->ports[i].label_subport);

			table->ports[i].is_split = 1;
		}
	}
}

static void
nfp_eth_calc_port_type(struct nfp_eth_table_port *entry)
{
	if (entry->interface == NFP_INTERFACE_NONE) {
		entry->port_type = PORT_NONE;
		return;
	}

	if (entry->media == NFP_MEDIA_FIBRE)
		entry->port_type = PORT_FIBRE;
	else
		entry->port_type = PORT_DA;
}

static struct nfp_eth_table *
__nfp_eth_read_ports(struct nfp_nsp *nsp)
{
	union eth_table_entry *entries;
	struct nfp_eth_table *table;
	uint32_t table_sz;
	int i, j, ret, cnt = 0;
	const struct rte_ether_addr *mac;

	entries = malloc(NSP_ETH_TABLE_SIZE);
	if (entries == NULL)
		return NULL;

	memset(entries, 0, NSP_ETH_TABLE_SIZE);
	ret = nfp_nsp_read_eth_table(nsp, entries, NSP_ETH_TABLE_SIZE);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "reading port table failed %d", ret);
		goto err;
	}

	/* The NFP3800 NIC support 8 ports, but only 2 ports are valid,
	 * the rest 6 ports mac are all 0, ensure we don't use these port
	 */
	for (i = 0; i < NSP_ETH_MAX_COUNT; i++) {
		mac = (const struct rte_ether_addr *)entries[i].mac_addr;
		if ((entries[i].port & NSP_ETH_PORT_LANES_MASK) &&
				!rte_is_zero_ether_addr(mac))
			cnt++;
	}

	/* Some versions of flash will give us 0 instead of port count. For
	 * those that give a port count, verify it against the value calculated
	 * above.
	 */
	if (ret && ret != cnt) {
		PMD_DRV_LOG(ERR, "table entry count (%d) unmatch entries present (%d)",
		       ret, cnt);
		goto err;
	}

	table_sz = sizeof(*table) + sizeof(struct nfp_eth_table_port) * cnt;
	table = malloc(table_sz);
	if (table == NULL)
		goto err;

	memset(table, 0, table_sz);
	table->count = cnt;
	for (i = 0, j = 0; i < NSP_ETH_MAX_COUNT; i++) {
		mac = (const struct rte_ether_addr *)entries[i].mac_addr;
		if ((entries[i].port & NSP_ETH_PORT_LANES_MASK) &&
				!rte_is_zero_ether_addr(mac))
			nfp_eth_port_translate(nsp, &entries[i], i,
					&table->ports[j++]);
	}

	nfp_eth_calc_port_geometry(table);
	for (i = 0; i < (int)table->count; i++)
		nfp_eth_calc_port_type(&table->ports[i]);

	free(entries);

	return table;

err:
	free(entries);
	return NULL;
}

/*
 * nfp_eth_read_ports() - retrieve port information
 * @cpp:	NFP CPP handle
 *
 * Read the port information from the device.  Returned structure should
 * be freed with kfree() once no longer needed.
 *
 * Return: populated ETH table or NULL on error.
 */
struct nfp_eth_table *
nfp_eth_read_ports(struct nfp_cpp *cpp)
{
	struct nfp_eth_table *ret;
	struct nfp_nsp *nsp;

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL)
		return NULL;

	ret = __nfp_eth_read_ports(nsp);
	nfp_nsp_close(nsp);

	return ret;
}

struct nfp_nsp *
nfp_eth_config_start(struct nfp_cpp *cpp, unsigned int idx)
{
	union eth_table_entry *entries;
	struct nfp_nsp *nsp;
	int ret;

	entries = malloc(NSP_ETH_TABLE_SIZE);
	if (entries == NULL)
		return NULL;

	memset(entries, 0, NSP_ETH_TABLE_SIZE);
	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL) {
		free(entries);
		return nsp;
	}

	ret = nfp_nsp_read_eth_table(nsp, entries, NSP_ETH_TABLE_SIZE);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "reading port table failed %d", ret);
		goto err;
	}

	if ((entries[idx].port & NSP_ETH_PORT_LANES_MASK) == 0) {
		PMD_DRV_LOG(ERR, "trying to set port state on disabled port %d", idx);
		goto err;
	}

	nfp_nsp_config_set_state(nsp, entries, idx);
	return nsp;

err:
	nfp_nsp_close(nsp);
	free(entries);
	return NULL;
}

void
nfp_eth_config_cleanup_end(struct nfp_nsp *nsp)
{
	union eth_table_entry *entries = nfp_nsp_config_entries(nsp);

	nfp_nsp_config_set_modified(nsp, 0);
	nfp_nsp_config_clear_state(nsp);
	nfp_nsp_close(nsp);
	free(entries);
}

/*
 * nfp_eth_config_commit_end() - perform recorded configuration changes
 * @nsp:	NFP NSP handle returned from nfp_eth_config_start()
 *
 * Perform the configuration which was requested with __nfp_eth_set_*()
 * helpers and recorded in @nsp state.  If device was already configured
 * as requested or no __nfp_eth_set_*() operations were made no NSP command
 * will be performed.
 *
 * Return:
 * 0 - configuration successful;
 * 1 - no changes were needed;
 * -ERRNO - configuration failed.
 */
int
nfp_eth_config_commit_end(struct nfp_nsp *nsp)
{
	union eth_table_entry *entries = nfp_nsp_config_entries(nsp);
	int ret = 1;

	if (nfp_nsp_config_modified(nsp)) {
		ret = nfp_nsp_write_eth_table(nsp, entries, NSP_ETH_TABLE_SIZE);
		ret = ret < 0 ? ret : 0;
	}

	nfp_eth_config_cleanup_end(nsp);

	return ret;
}

/*
 * nfp_eth_set_mod_enable() - set PHY module enable control bit
 * @cpp:	NFP CPP handle
 * @idx:	NFP chip-wide port index
 * @enable:	Desired state
 *
 * Enable or disable PHY module (this usually means setting the TX lanes
 * disable bits).
 *
 * Return:
 * 0 - configuration successful;
 * 1 - no changes were needed;
 * -ERRNO - configuration failed.
 */
int
nfp_eth_set_mod_enable(struct nfp_cpp *cpp, unsigned int idx, int enable)
{
	union eth_table_entry *entries;
	struct nfp_nsp *nsp;
	uint64_t reg;

	nsp = nfp_eth_config_start(cpp, idx);
	if (nsp == NULL)
		return -1;

	entries = nfp_nsp_config_entries(nsp);

	/* Check if we are already in requested state */
	reg = rte_le_to_cpu_64(entries[idx].state);
	if (enable != (int)FIELD_GET(NSP_ETH_CTRL_ENABLED, reg)) {
		reg = rte_le_to_cpu_64(entries[idx].control);
		reg &= ~NSP_ETH_CTRL_ENABLED;
		reg |= FIELD_PREP(NSP_ETH_CTRL_ENABLED, enable);
		entries[idx].control = rte_cpu_to_le_64(reg);

		nfp_nsp_config_set_modified(nsp, 1);
	}

	return nfp_eth_config_commit_end(nsp);
}

/*
 * nfp_eth_set_configured() - set PHY module configured control bit
 * @cpp:	NFP CPP handle
 * @idx:	NFP chip-wide port index
 * @configed:	Desired state
 *
 * Set the ifup/ifdown state on the PHY.
 *
 * Return:
 * 0 - configuration successful;
 * 1 - no changes were needed;
 * -ERRNO - configuration failed.
 */
int
nfp_eth_set_configured(struct nfp_cpp *cpp, unsigned int idx, int configed)
{
	union eth_table_entry *entries;
	struct nfp_nsp *nsp;
	uint64_t reg;

	nsp = nfp_eth_config_start(cpp, idx);
	if (nsp == NULL)
		return -EIO;

	/*
	 * Older ABI versions did support this feature, however this has only
	 * been reliable since ABI 20.
	 */
	if (nfp_nsp_get_abi_ver_minor(nsp) < 20) {
		nfp_eth_config_cleanup_end(nsp);
		return -EOPNOTSUPP;
	}

	entries = nfp_nsp_config_entries(nsp);

	/* Check if we are already in requested state */
	reg = rte_le_to_cpu_64(entries[idx].state);
	if (configed != (int)FIELD_GET(NSP_ETH_STATE_CONFIGURED, reg)) {
		reg = rte_le_to_cpu_64(entries[idx].control);
		reg &= ~NSP_ETH_CTRL_CONFIGURED;
		reg |= FIELD_PREP(NSP_ETH_CTRL_CONFIGURED, configed);
		entries[idx].control = rte_cpu_to_le_64(reg);

		nfp_nsp_config_set_modified(nsp, 1);
	}

	return nfp_eth_config_commit_end(nsp);
}

static int
nfp_eth_set_bit_config(struct nfp_nsp *nsp, unsigned int raw_idx,
		       const uint64_t mask, const unsigned int shift,
		       unsigned int val, const uint64_t ctrl_bit)
{
	union eth_table_entry *entries = nfp_nsp_config_entries(nsp);
	unsigned int idx = nfp_nsp_config_idx(nsp);
	uint64_t reg;

	/*
	 * Note: set features were added in ABI 0.14 but the error
	 *	 codes were initially not populated correctly.
	 */
	if (nfp_nsp_get_abi_ver_minor(nsp) < 17) {
		PMD_DRV_LOG(ERR, "set operations not supported, please update flash");
		return -EOPNOTSUPP;
	}

	/* Check if we are already in requested state */
	reg = rte_le_to_cpu_64(entries[idx].raw[raw_idx]);
	if (val == (reg & mask) >> shift)
		return 0;

	reg &= ~mask;
	reg |= (val << shift) & mask;
	entries[idx].raw[raw_idx] = rte_cpu_to_le_64(reg);

	entries[idx].control |= rte_cpu_to_le_64(ctrl_bit);

	nfp_nsp_config_set_modified(nsp, 1);

	return 0;
}

#define NFP_ETH_SET_BIT_CONFIG(nsp, raw_idx, mask, val, ctrl_bit)	\
	(__extension__ ({ \
		typeof(mask) _x = (mask); \
		nfp_eth_set_bit_config(nsp, raw_idx, _x, __bf_shf(_x), \
				       val, ctrl_bit);			\
	}))

/*
 * __nfp_eth_set_aneg() - set PHY autonegotiation control bit
 * @nsp:	NFP NSP handle returned from nfp_eth_config_start()
 * @mode:	Desired autonegotiation mode
 *
 * Allow/disallow PHY module to advertise/perform autonegotiation.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * Return: 0 or -ERRNO.
 */
int
__nfp_eth_set_aneg(struct nfp_nsp *nsp, enum nfp_eth_aneg mode)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
				      NSP_ETH_STATE_ANEG, mode,
				      NSP_ETH_CTRL_SET_ANEG);
}

/*
 * __nfp_eth_set_fec() - set PHY forward error correction control bit
 * @nsp:	NFP NSP handle returned from nfp_eth_config_start()
 * @mode:	Desired fec mode
 *
 * Set the PHY module forward error correction mode.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * Return: 0 or -ERRNO.
 */
static int
__nfp_eth_set_fec(struct nfp_nsp *nsp, enum nfp_eth_fec mode)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
				      NSP_ETH_STATE_FEC, mode,
				      NSP_ETH_CTRL_SET_FEC);
}

/*
 * nfp_eth_set_fec() - set PHY forward error correction control mode
 * @cpp:	NFP CPP handle
 * @idx:	NFP chip-wide port index
 * @mode:	Desired fec mode
 *
 * Return:
 * 0 - configuration successful;
 * 1 - no changes were needed;
 * -ERRNO - configuration failed.
 */
int
nfp_eth_set_fec(struct nfp_cpp *cpp, unsigned int idx, enum nfp_eth_fec mode)
{
	struct nfp_nsp *nsp;
	int err;

	nsp = nfp_eth_config_start(cpp, idx);
	if (nsp == NULL)
		return -EIO;

	err = __nfp_eth_set_fec(nsp, mode);
	if (err) {
		nfp_eth_config_cleanup_end(nsp);
		return err;
	}

	return nfp_eth_config_commit_end(nsp);
}

/*
 * __nfp_eth_set_speed() - set interface speed/rate
 * @nsp:	NFP NSP handle returned from nfp_eth_config_start()
 * @speed:	Desired speed (per lane)
 *
 * Set lane speed.  Provided @speed value should be subport speed divided
 * by number of lanes this subport is spanning (i.e. 10000 for 40G, 25000 for
 * 50G, etc.)
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * Return: 0 or -ERRNO.
 */
int
__nfp_eth_set_speed(struct nfp_nsp *nsp, unsigned int speed)
{
	enum nfp_eth_rate rate;

	rate = nfp_eth_speed2rate(speed);
	if (rate == RATE_INVALID) {
		PMD_DRV_LOG(ERR, "could not find matching lane rate for speed %u", speed);
		return -EINVAL;
	}

	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
				      NSP_ETH_STATE_RATE, rate,
				      NSP_ETH_CTRL_SET_RATE);
}

/*
 * __nfp_eth_set_split() - set interface lane split
 * @nsp:	NFP NSP handle returned from nfp_eth_config_start()
 * @lanes:	Desired lanes per port
 *
 * Set number of lanes in the port.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * Return: 0 or -ERRNO.
 */
int
__nfp_eth_set_split(struct nfp_nsp *nsp, unsigned int lanes)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_PORT, NSP_ETH_PORT_LANES,
				      lanes, NSP_ETH_CTRL_SET_LANES);
}
