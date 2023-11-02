/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <nfp_platform.h>

#include "nfp_logs.h"
#include "nfp_nsp.h"

#define NSP_ETH_NBI_PORT_COUNT          24
#define NSP_ETH_MAX_COUNT               (2 * NSP_ETH_NBI_PORT_COUNT)
#define NSP_ETH_TABLE_SIZE              (NSP_ETH_MAX_COUNT * sizeof(union eth_table_entry))

#define NSP_ETH_PORT_LANES              GENMASK_ULL(3, 0)
#define NSP_ETH_PORT_INDEX              GENMASK_ULL(15, 8)
#define NSP_ETH_PORT_LABEL              GENMASK_ULL(53, 48)
#define NSP_ETH_PORT_PHYLABEL           GENMASK_ULL(59, 54)
#define NSP_ETH_PORT_FEC_SUPP_BASER     RTE_BIT64(60)
#define NSP_ETH_PORT_FEC_SUPP_RS        RTE_BIT64(61)
#define NSP_ETH_PORT_SUPP_ANEG          RTE_BIT64(63)

#define NSP_ETH_PORT_LANES_MASK         rte_cpu_to_le_64(NSP_ETH_PORT_LANES)

#define NSP_ETH_STATE_CONFIGURED        RTE_BIT64(0)
#define NSP_ETH_STATE_ENABLED           RTE_BIT64(1)
#define NSP_ETH_STATE_TX_ENABLED        RTE_BIT64(2)
#define NSP_ETH_STATE_RX_ENABLED        RTE_BIT64(3)
#define NSP_ETH_STATE_RATE              GENMASK_ULL(11, 8)
#define NSP_ETH_STATE_INTERFACE         GENMASK_ULL(19, 12)
#define NSP_ETH_STATE_MEDIA             GENMASK_ULL(21, 20)
#define NSP_ETH_STATE_OVRD_CHNG         RTE_BIT64(22)
#define NSP_ETH_STATE_ANEG              GENMASK_ULL(25, 23)
#define NSP_ETH_STATE_FEC               GENMASK_ULL(27, 26)
#define NSP_ETH_STATE_ACT_FEC           GENMASK_ULL(29, 28)
#define NSP_ETH_STATE_TX_PAUSE          RTE_BIT64(31)
#define NSP_ETH_STATE_RX_PAUSE          RTE_BIT64(32)

#define NSP_ETH_CTRL_CONFIGURED         RTE_BIT64(0)
#define NSP_ETH_CTRL_ENABLED            RTE_BIT64(1)
#define NSP_ETH_CTRL_TX_ENABLED         RTE_BIT64(2)
#define NSP_ETH_CTRL_RX_ENABLED         RTE_BIT64(3)
#define NSP_ETH_CTRL_SET_RATE           RTE_BIT64(4)
#define NSP_ETH_CTRL_SET_LANES          RTE_BIT64(5)
#define NSP_ETH_CTRL_SET_ANEG           RTE_BIT64(6)
#define NSP_ETH_CTRL_SET_FEC            RTE_BIT64(7)
#define NSP_ETH_CTRL_SET_TX_PAUSE       RTE_BIT64(10)
#define NSP_ETH_CTRL_SET_RX_PAUSE       RTE_BIT64(11)

/* Which connector port. */
#define PORT_TP                 0x00
#define PORT_AUI                0x01
#define PORT_MII                0x02
#define PORT_FIBRE              0x03
#define PORT_BNC                0x04
#define PORT_DA                 0x05
#define PORT_NONE               0xef
#define PORT_OTHER              0xff

enum nfp_eth_raw {
	NSP_ETH_RAW_PORT = 0,
	NSP_ETH_RAW_STATE,
	NSP_ETH_RAW_MAC,
	NSP_ETH_RAW_CONTROL,
	NSP_ETH_NUM_RAW,
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
		uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
		uint8_t resv[2];
		uint64_t control;
	};
	uint64_t raw[NSP_ETH_NUM_RAW];
};

static const struct {
	enum nfp_eth_rate rate;
	uint32_t speed;
} nsp_eth_rate_tbl[] = {
	{ RATE_INVALID, RTE_ETH_SPEED_NUM_NONE, },
	{ RATE_10M,     RTE_ETH_SPEED_NUM_10M, },
	{ RATE_100M,    RTE_ETH_SPEED_NUM_100M, },
	{ RATE_1G,      RTE_ETH_SPEED_NUM_1G, },
	{ RATE_10G,     RTE_ETH_SPEED_NUM_10G, },
	{ RATE_25G,     RTE_ETH_SPEED_NUM_25G, },
};

static uint32_t
nfp_eth_rate2speed(enum nfp_eth_rate rate)
{
	uint32_t i;

	for (i = 0; i < RTE_DIM(nsp_eth_rate_tbl); i++)
		if (nsp_eth_rate_tbl[i].rate == rate)
			return nsp_eth_rate_tbl[i].speed;

	return 0;
}

static enum nfp_eth_rate
nfp_eth_speed2rate(uint32_t speed)
{
	uint32_t i;

	for (i = 0; i < RTE_DIM(nsp_eth_rate_tbl); i++)
		if (nsp_eth_rate_tbl[i].speed == speed)
			return nsp_eth_rate_tbl[i].rate;

	return RATE_INVALID;
}

static void
nfp_eth_copy_mac_reverse(uint8_t *dst,
		const uint8_t *src)
{
	uint32_t i;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		dst[RTE_ETHER_ADDR_LEN - i - 1] = src[i];
}

static void
nfp_eth_port_translate(struct nfp_nsp *nsp,
		const union eth_table_entry *src,
		uint32_t index,
		struct nfp_eth_table_port *dst)
{
	uint32_t fec;
	uint64_t port;
	uint32_t rate;
	uint64_t state;

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

	nfp_eth_copy_mac_reverse(&dst->mac_addr.addr_bytes[0], src->mac_addr);

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
	if (dst->fec_modes_supported != 0)
		dst->fec_modes_supported |= NFP_FEC_AUTO | NFP_FEC_DISABLED;

	dst->fec = FIELD_GET(NSP_ETH_STATE_FEC, state);
	dst->act_fec = dst->fec;

	if (nfp_nsp_get_abi_ver_minor(nsp) < 33)
		return;

	dst->act_fec = FIELD_GET(NSP_ETH_STATE_ACT_FEC, state);
	dst->supp_aneg = FIELD_GET(NSP_ETH_PORT_SUPP_ANEG, port);

	if (nfp_nsp_get_abi_ver_minor(nsp) < 37) {
		dst->tx_pause_enabled = true;
		dst->rx_pause_enabled = true;
		return;
	}

	dst->tx_pause_enabled = FIELD_GET(NSP_ETH_STATE_TX_PAUSE, state);
	dst->rx_pause_enabled = FIELD_GET(NSP_ETH_STATE_RX_PAUSE, state);
}

static void
nfp_eth_calc_port_geometry(struct nfp_eth_table *table)
{
	uint32_t i;
	uint32_t j;

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

			table->ports[i].is_split = true;
		}
	}
}

static void
nfp_eth_calc_port_type(struct nfp_eth_table_port *entry)
{
	if (entry->interface == NFP_INTERFACE_NONE) {
		entry->port_type = PORT_NONE;
		return;
	} else if (entry->interface == NFP_INTERFACE_RJ45) {
		entry->port_type = PORT_TP;
		return;
	}

	if (entry->media == NFP_MEDIA_FIBRE)
		entry->port_type = PORT_FIBRE;
	else
		entry->port_type = PORT_DA;
}

static struct nfp_eth_table *
nfp_eth_read_ports_real(struct nfp_nsp *nsp)
{
	int ret;
	uint32_t i;
	uint32_t j;
	int cnt = 0;
	uint32_t table_sz;
	struct nfp_eth_table *table;
	union eth_table_entry *entries;

	entries = malloc(NSP_ETH_TABLE_SIZE);
	if (entries == NULL)
		return NULL;

	memset(entries, 0, NSP_ETH_TABLE_SIZE);
	ret = nfp_nsp_read_eth_table(nsp, entries, NSP_ETH_TABLE_SIZE);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Reading port table failed %d", ret);
		goto err;
	}

	for (i = 0; i < NSP_ETH_MAX_COUNT; i++)
		if ((entries[i].port & NSP_ETH_PORT_LANES_MASK) != 0)
			cnt++;

	/*
	 * Some versions of flash will give us 0 instead of port count. For
	 * those that give a port count, verify it against the value calculated
	 * above.
	 */
	if (ret != 0 && ret != cnt) {
		PMD_DRV_LOG(ERR, "Table entry count (%d) unmatch entries present (%d)",
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
		if ((entries[i].port & NSP_ETH_PORT_LANES_MASK) != 0)
			nfp_eth_port_translate(nsp, &entries[i], i, &table->ports[j++]);
	}

	nfp_eth_calc_port_geometry(table);
	for (i = 0; i < table->count; i++)
		nfp_eth_calc_port_type(&table->ports[i]);

	free(entries);

	return table;

err:
	free(entries);
	return NULL;
}

/**
 * Read the port information from the device.
 *
 * Returned structure should be freed once no longer needed.
 *
 * @param cpp
 *   NFP CPP handle
 *
 * @return
 *   Populated ETH table or NULL on error.
 */
struct nfp_eth_table *
nfp_eth_read_ports(struct nfp_cpp *cpp)
{
	struct nfp_nsp *nsp;
	struct nfp_eth_table *ret;

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL)
		return NULL;

	ret = nfp_eth_read_ports_real(nsp);
	nfp_nsp_close(nsp);

	return ret;
}

struct nfp_nsp *
nfp_eth_config_start(struct nfp_cpp *cpp,
		uint32_t idx)
{
	int ret;
	struct nfp_nsp *nsp;
	union eth_table_entry *entries;

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
		PMD_DRV_LOG(ERR, "Reading port table failed %d", ret);
		goto err;
	}

	if ((entries[idx].port & NSP_ETH_PORT_LANES_MASK) == 0) {
		PMD_DRV_LOG(ERR, "Trying to set port state on disabled port %d", idx);
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

/**
 * Perform the configuration which was requested with __nfp_eth_set_*()
 * helpers and recorded in @nsp state. If device was already configured
 * as requested or no __nfp_eth_set_*() operations were made, no NSP command
 * will be performed.
 *
 * @param nsp
 *   NFP NSP handle returned from nfp_eth_config_start()
 *
 * @return
 *   - (0) Configuration successful
 *   - (1) No changes were needed
 *   - (-ERRNO) Configuration failed
 */
int
nfp_eth_config_commit_end(struct nfp_nsp *nsp)
{
	int ret = 1;
	union eth_table_entry *entries = nfp_nsp_config_entries(nsp);

	if (nfp_nsp_config_modified(nsp)) {
		ret = nfp_nsp_write_eth_table(nsp, entries, NSP_ETH_TABLE_SIZE);
		ret = ret < 0 ? ret : 0;
	}

	nfp_eth_config_cleanup_end(nsp);

	return ret;
}

/**
 * Enable or disable PHY module (this usually means setting the TX lanes
 * disable bits).
 *
 * @param cpp
 *   NFP CPP handle
 * @param idx
 *   NFP chip-wide port index
 * @param enable
 *   Desired state
 *
 * @return
 *   - (0) Configuration successful
 *   - (1) No changes were needed
 *   - (-ERRNO) Configuration failed
 */
int
nfp_eth_set_mod_enable(struct nfp_cpp *cpp,
		uint32_t idx,
		bool enable)
{
	uint64_t reg;
	struct nfp_nsp *nsp;
	union eth_table_entry *entries;

	nsp = nfp_eth_config_start(cpp, idx);
	if (nsp == NULL)
		return -EIO;

	entries = nfp_nsp_config_entries(nsp);

	/* Check if we are already in requested state */
	reg = rte_le_to_cpu_64(entries[idx].state);
	if (enable != (int)FIELD_GET(NSP_ETH_CTRL_ENABLED, reg)) {
		reg = rte_le_to_cpu_64(entries[idx].control);
		reg &= ~NSP_ETH_CTRL_ENABLED;
		reg |= FIELD_PREP(NSP_ETH_CTRL_ENABLED, enable);
		entries[idx].control = rte_cpu_to_le_64(reg);

		nfp_nsp_config_set_modified(nsp, true);
	}

	return nfp_eth_config_commit_end(nsp);
}

/**
 * Set the ifup/ifdown state on the PHY.
 *
 * @param cpp
 *   NFP CPP handle
 * @param idx
 *   NFP chip-wide port index
 * @param configured
 *   Desired state
 *
 * @return
 *   - (0) Configuration successful
 *   - (1) No changes were needed
 *   - (-ERRNO) Configuration failed
 */
int
nfp_eth_set_configured(struct nfp_cpp *cpp,
		uint32_t idx,
		bool configured)
{
	uint64_t reg;
	struct nfp_nsp *nsp;
	union eth_table_entry *entries;

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
	if (configured != (int)FIELD_GET(NSP_ETH_STATE_CONFIGURED, reg)) {
		reg = rte_le_to_cpu_64(entries[idx].control);
		reg &= ~NSP_ETH_CTRL_CONFIGURED;
		reg |= FIELD_PREP(NSP_ETH_CTRL_CONFIGURED, configured);
		entries[idx].control = rte_cpu_to_le_64(reg);

		nfp_nsp_config_set_modified(nsp, true);
	}

	return nfp_eth_config_commit_end(nsp);
}

static int
nfp_eth_set_bit_config(struct nfp_nsp *nsp,
		uint32_t raw_idx,
		const uint64_t mask,
		const uint32_t shift,
		uint64_t val,
		const uint64_t ctrl_bit)
{
	uint64_t reg;
	uint32_t idx = nfp_nsp_config_idx(nsp);
	union eth_table_entry *entries = nfp_nsp_config_entries(nsp);

	/*
	 * Note: set features were added in ABI 0.14 but the error
	 * codes were initially not populated correctly.
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

	nfp_nsp_config_set_modified(nsp, true);

	return 0;
}

#define NFP_ETH_SET_BIT_CONFIG(nsp, raw_idx, mask, val, ctrl_bit)      \
	(__extension__ ({                                              \
		typeof(mask) _x = (mask);                              \
		nfp_eth_set_bit_config(nsp, raw_idx, _x, __bf_shf(_x), \
				val, ctrl_bit);                        \
	}))

/**
 * Allow/disallow PHY module to advertise/perform autonegotiation.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * @param nsp
 *   NFP NSP handle returned from nfp_eth_config_start()
 * @param mode
 *   Desired autonegotiation mode
 *
 * @return
 *   0 or -ERRNO
 */
int
nfp_eth_set_aneg(struct nfp_nsp *nsp,
		enum nfp_eth_aneg mode)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
			NSP_ETH_STATE_ANEG, mode, NSP_ETH_CTRL_SET_ANEG);
}

/**
 * Set the PHY module forward error correction mode.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * @param nsp
 *   NFP NSP handle returned from nfp_eth_config_start()
 * @param mode
 *   Desired fec mode
 *
 * @return
 *   0 or -ERRNO
 */
static int
nfp_eth_set_fec_real(struct nfp_nsp *nsp,
		enum nfp_eth_fec mode)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
			NSP_ETH_STATE_FEC, mode, NSP_ETH_CTRL_SET_FEC);
}

/**
 * Set PHY forward error correction control mode
 *
 * @param cpp
 *   NFP CPP handle
 * @param idx
 *   NFP chip-wide port index
 * @param mode
 *   Desired fec mode
 *
 * @return
 *   - (0) Configuration successful
 *   - (1) No changes were needed
 *   - (-ERRNO) Configuration failed
 */
int
nfp_eth_set_fec(struct nfp_cpp *cpp,
		uint32_t idx,
		enum nfp_eth_fec mode)
{
	int err;
	struct nfp_nsp *nsp;

	nsp = nfp_eth_config_start(cpp, idx);
	if (nsp == NULL)
		return -EIO;

	err = nfp_eth_set_fec_real(nsp, mode);
	if (err != 0) {
		nfp_eth_config_cleanup_end(nsp);
		return err;
	}

	return nfp_eth_config_commit_end(nsp);
}

/**
 * Set lane speed.
 * Provided @speed value should be subport speed divided by number of
 * lanes this subport is spanning (i.e. 10000 for 40G, 25000 for 50G, etc.)
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * @param nsp
 *   NFP NSP handle returned from nfp_eth_config_start()
 * @param speed
 *   Desired speed (per lane)
 *
 * @return
 *   0 or -ERRNO
 */
int
nfp_eth_set_speed(struct nfp_nsp *nsp,
		uint32_t speed)
{
	enum nfp_eth_rate rate;

	rate = nfp_eth_speed2rate(speed);
	if (rate == RATE_INVALID) {
		PMD_DRV_LOG(ERR, "Could not find matching lane rate for speed %u", speed);
		return -EINVAL;
	}

	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
			NSP_ETH_STATE_RATE, rate, NSP_ETH_CTRL_SET_RATE);
}

/**
 * Set number of lanes in the port.
 * Will write to hwinfo overrides in the flash (persistent config).
 *
 * @param nsp
 *   NFP NSP handle returned from nfp_eth_config_start()
 * @param lanes
 *   Desired lanes per port
 *
 * @return
 *   0 or -ERRNO
 */
int
nfp_eth_set_split(struct nfp_nsp *nsp,
		uint32_t lanes)
{
	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_PORT,
			NSP_ETH_PORT_LANES, lanes, NSP_ETH_CTRL_SET_LANES);
}

/**
 * Set TX pause switch.
 *
 * @param nsp
 *    NFP NSP handle returned from nfp_eth_config_start()
 * @param tx_pause
 *   TX pause switch
 *
 * @return
 *   0 or -ERRNO
 */
int
nfp_eth_set_tx_pause(struct nfp_nsp *nsp,
		bool tx_pause)
{
	if (nfp_nsp_get_abi_ver_minor(nsp) < 37) {
		PMD_DRV_LOG(ERR, "Set frame pause operation not supported, please update flash.");
		return -EOPNOTSUPP;
	}

	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
			NSP_ETH_STATE_TX_PAUSE, tx_pause, NSP_ETH_CTRL_SET_TX_PAUSE);
}

/**
 * Set RX pause switch.
 *
 * @param nsp
 *    NFP NSP handle returned from nfp_eth_config_start()
 * @param rx_pause
 *   RX pause switch
 *
 * @return
 *   0 or -ERRNO
 */
int
nfp_eth_set_rx_pause(struct nfp_nsp *nsp,
		bool rx_pause)
{
	if (nfp_nsp_get_abi_ver_minor(nsp) < 37) {
		PMD_DRV_LOG(ERR, "Set frame pause operation not supported, please update flash.");
		return -EOPNOTSUPP;
	}

	return NFP_ETH_SET_BIT_CONFIG(nsp, NSP_ETH_RAW_STATE,
			NSP_ETH_STATE_RX_PAUSE, rx_pause, NSP_ETH_CTRL_SET_RX_PAUSE);
}
