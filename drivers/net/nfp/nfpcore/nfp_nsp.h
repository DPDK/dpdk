/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NSP_NSP_H__
#define __NSP_NSP_H__

#include "nfp_cpp.h"
#include "nfp_nsp.h"

/* Offsets relative to the CSR base */
#define NSP_STATUS              0x00
#define   NSP_STATUS_MAGIC      GENMASK_ULL(63, 48)
#define   NSP_STATUS_MAJOR      GENMASK_ULL(47, 44)
#define   NSP_STATUS_MINOR      GENMASK_ULL(43, 32)
#define   NSP_STATUS_CODE       GENMASK_ULL(31, 16)
#define   NSP_STATUS_RESULT     GENMASK_ULL(15, 8)
#define   NSP_STATUS_BUSY       RTE_BIT64(0)

#define NSP_COMMAND             0x08
#define   NSP_COMMAND_OPTION    GENMASK_ULL(63, 32)
#define   NSP_COMMAND_CODE      GENMASK_ULL(31, 16)
#define   NSP_COMMAND_START     RTE_BIT64(0)

/* CPP address to retrieve the data from */
#define NSP_BUFFER              0x10
#define   NSP_BUFFER_CPP        GENMASK_ULL(63, 40)
#define   NSP_BUFFER_PCIE       GENMASK_ULL(39, 38)
#define   NSP_BUFFER_ADDRESS    GENMASK_ULL(37, 0)

#define NSP_DFLT_BUFFER         0x18

#define NSP_DFLT_BUFFER_CONFIG 0x20
#define   NSP_DFLT_BUFFER_SIZE_MB    GENMASK_ULL(7, 0)

#define NSP_MAGIC               0xab10
#define NSP_MAJOR               0
#define NSP_MINOR               8

#define NSP_CODE_MAJOR          GENMASK(15, 12)
#define NSP_CODE_MINOR          GENMASK(11, 0)

enum nfp_nsp_cmd {
	SPCODE_NOOP             = 0, /* No operation */
	SPCODE_SOFT_RESET       = 1, /* Soft reset the NFP */
	SPCODE_FW_DEFAULT       = 2, /* Load default (UNDI) FW */
	SPCODE_PHY_INIT         = 3, /* Initialize the PHY */
	SPCODE_MAC_INIT         = 4, /* Initialize the MAC */
	SPCODE_PHY_RXADAPT      = 5, /* Re-run PHY RX Adaptation */
	SPCODE_FW_LOAD          = 6, /* Load fw from buffer, len in option */
	SPCODE_ETH_RESCAN       = 7, /* Rescan ETHs, write ETH_TABLE to buf */
	SPCODE_ETH_CONTROL      = 8, /* Update media config from buffer */
	SPCODE_NSP_SENSORS      = 12, /* Read NSP sensor(s) */
	SPCODE_NSP_IDENTIFY     = 13, /* Read NSP version */
};

static const struct {
	int code;
	const char *msg;
} nsp_errors[] = {
	{ 6010, "could not map to phy for port" },
	{ 6011, "not an allowed rate/lanes for port" },
	{ 6012, "not an allowed rate/lanes for port" },
	{ 6013, "high/low error, change other port first" },
	{ 6014, "config not found in flash" },
};

struct nfp_nsp {
	struct nfp_cpp *cpp;
	struct nfp_resource *res;
	struct {
		uint16_t major;
		uint16_t minor;
	} ver;

	/* Eth table config state */
	int modified;
	unsigned int idx;
	void *entries;
};

struct nfp_nsp *nfp_nsp_open(struct nfp_cpp *cpp);
void nfp_nsp_close(struct nfp_nsp *state);
uint16_t nfp_nsp_get_abi_ver_major(struct nfp_nsp *state);
uint16_t nfp_nsp_get_abi_ver_minor(struct nfp_nsp *state);
int nfp_nsp_wait(struct nfp_nsp *state);
int nfp_nsp_device_soft_reset(struct nfp_nsp *state);
int nfp_nsp_load_fw(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_mac_reinit(struct nfp_nsp *state);
int nfp_nsp_read_identify(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_read_sensors(struct nfp_nsp *state, uint32_t sensor_mask,
		void *buf, size_t size);

static inline int
nfp_nsp_has_mac_reinit(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 20;
}

enum nfp_eth_interface {
	NFP_INTERFACE_NONE      = 0,
	NFP_INTERFACE_SFP       = 1,
	NFP_INTERFACE_SFPP      = 10,
	NFP_INTERFACE_SFP28     = 28,
	NFP_INTERFACE_QSFP      = 40,
	NFP_INTERFACE_CXP       = 100,
	NFP_INTERFACE_QSFP28    = 112,
};

enum nfp_eth_media {
	NFP_MEDIA_DAC_PASSIVE = 0,
	NFP_MEDIA_DAC_ACTIVE,
	NFP_MEDIA_FIBRE,
};

enum nfp_eth_aneg {
	NFP_ANEG_AUTO = 0,
	NFP_ANEG_SEARCH,
	NFP_ANEG_25G_CONSORTIUM,
	NFP_ANEG_25G_IEEE,
	NFP_ANEG_DISABLED,
};

enum nfp_eth_fec {
	NFP_FEC_AUTO_BIT = 0,
	NFP_FEC_BASER_BIT,
	NFP_FEC_REED_SOLOMON_BIT,
	NFP_FEC_DISABLED_BIT,
};

#define NFP_FEC_AUTO            RTE_BIT32(NFP_FEC_AUTO_BIT)
#define NFP_FEC_BASER           RTE_BIT32(NFP_FEC_BASER_BIT)
#define NFP_FEC_REED_SOLOMON    RTE_BIT32(NFP_FEC_REED_SOLOMON_BIT)
#define NFP_FEC_DISABLED        RTE_BIT32(NFP_FEC_DISABLED_BIT)

/* ETH table information */
struct nfp_eth_table {
	uint32_t count;     /**< Number of table entries */
	uint32_t max_index; /**< Max of @index fields of all @ports */
	struct nfp_eth_table_port {
		/** Port index according to legacy ethX numbering */
		uint32_t eth_index;
		uint32_t index;  /**< Chip-wide first channel index */
		uint32_t nbi;    /**< NBI index */
		uint32_t base;   /**< First channel index (within NBI) */
		uint32_t lanes;  /**< Number of channels */
		uint32_t speed;  /**< Interface speed (in Mbps) */

		uint32_t interface;  /**< Interface (module) plugged in */
		enum nfp_eth_media media; /**< Media type of the @interface */

		enum nfp_eth_fec fec;     /**< Forward Error Correction mode */
		enum nfp_eth_aneg aneg;   /**< Auto negotiation mode */

		struct rte_ether_addr mac_addr;  /**< Interface MAC address */

		uint8_t label_port;    /**< Port id */
		/** Id of interface within port (for split ports) */
		uint8_t label_subport;

		int enabled;     /**< Enable port */
		int tx_enabled;  /**< Enable TX */
		int rx_enabled;  /**< Enable RX */

		int override_changed;  /**< Media reconfig pending */

		uint8_t port_type;    /**< One of %PORT_* */
		/** Sum of lanes of all subports of this port */
		uint32_t port_lanes;

		int is_split;   /**< Split port */

		uint32_t fec_modes_supported;  /**< Bitmap of FEC modes supported */
	} ports[]; /**< Table of ports */
};

struct nfp_eth_table *nfp_eth_read_ports(struct nfp_cpp *cpp);

int nfp_eth_set_mod_enable(struct nfp_cpp *cpp, uint32_t idx, int enable);
int nfp_eth_set_configured(struct nfp_cpp *cpp, uint32_t idx, int configed);
int nfp_eth_set_fec(struct nfp_cpp *cpp, uint32_t idx, enum nfp_eth_fec mode);

int nfp_nsp_read_eth_table(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_write_eth_table(struct nfp_nsp *state, const void *buf,
		size_t size);
void nfp_nsp_config_set_state(struct nfp_nsp *state, void *entries,
		uint32_t idx);
void nfp_nsp_config_clear_state(struct nfp_nsp *state);
void nfp_nsp_config_set_modified(struct nfp_nsp *state, int modified);
void *nfp_nsp_config_entries(struct nfp_nsp *state);
int nfp_nsp_config_modified(struct nfp_nsp *state);
uint32_t nfp_nsp_config_idx(struct nfp_nsp *state);

static inline int
nfp_eth_can_support_fec(struct nfp_eth_table_port *eth_port)
{
	return eth_port->fec_modes_supported != 0;
}

static inline uint32_t
nfp_eth_supported_fec_modes(struct nfp_eth_table_port *eth_port)
{
	return eth_port->fec_modes_supported;
}

struct nfp_nsp *nfp_eth_config_start(struct nfp_cpp *cpp, uint32_t idx);
int nfp_eth_config_commit_end(struct nfp_nsp *nsp);
void nfp_eth_config_cleanup_end(struct nfp_nsp *nsp);

int nfp_eth_set_aneg(struct nfp_nsp *nsp, enum nfp_eth_aneg mode);
int nfp_eth_set_speed(struct nfp_nsp *nsp, uint32_t speed);
int nfp_eth_set_split(struct nfp_nsp *nsp, uint32_t lanes);

/* NSP static information */
struct nfp_nsp_identify {
	char version[40];      /**< Opaque version string */
	uint8_t flags;         /**< Version flags */
	uint8_t br_primary;    /**< Branch id of primary bootloader */
	uint8_t br_secondary;  /**< Branch id of secondary bootloader */
	uint8_t br_nsp;        /**< Branch id of NSP */
	uint16_t primary;      /**< Version of primary bootloader */
	uint16_t secondary;    /**< Version id of secondary bootloader */
	uint16_t nsp;          /**< Version id of NSP */
	uint64_t sensor_mask;  /**< Mask of present sensors available on NIC */
};

struct nfp_nsp_identify *nfp_nsp_identify(struct nfp_nsp *nsp);

enum nfp_nsp_sensor_id {
	NFP_SENSOR_CHIP_TEMPERATURE,
	NFP_SENSOR_ASSEMBLY_POWER,
	NFP_SENSOR_ASSEMBLY_12V_POWER,
	NFP_SENSOR_ASSEMBLY_3V3_POWER,
};

int nfp_hwmon_read_sensor(struct nfp_cpp *cpp, enum nfp_nsp_sensor_id id,
		uint32_t *val);

#endif /* __NSP_NSP_H__ */
