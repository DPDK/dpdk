/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NSP_NSP_H__
#define __NSP_NSP_H__

#include "nfp_cpp.h"

/* EEPROM byte offsets */
#define SFP_SFF8472_COMPLIANCE          0x5e
#define SFP_SFF_REV_COMPLIANCE          1
#define NSP_SFF_EEPROM_BLOCK_LEN        8

/* Defines the valid values of the 'abi_drv_reset' hwinfo key */
#define NFP_NSP_DRV_RESET_DISK                  0
#define NFP_NSP_DRV_RESET_ALWAYS                1
#define NFP_NSP_DRV_RESET_NEVER                 2
#define NFP_NSP_DRV_RESET_DEFAULT               "0"

/* Defines the valid values of the 'app_fw_from_flash' hwinfo key */
#define NFP_NSP_APP_FW_LOAD_DISK                0
#define NFP_NSP_APP_FW_LOAD_FLASH               1
#define NFP_NSP_APP_FW_LOAD_PREF                2
#define NFP_NSP_APP_FW_LOAD_DEFAULT             "2"

struct nfp_nsp;

struct nfp_nsp *nfp_nsp_open(struct nfp_cpp *cpp);
void nfp_nsp_close(struct nfp_nsp *state);
uint16_t nfp_nsp_get_abi_ver_major(struct nfp_nsp *state);
uint16_t nfp_nsp_get_abi_ver_minor(struct nfp_nsp *state);
int nfp_nsp_wait(struct nfp_nsp *state);
int nfp_nsp_device_soft_reset(struct nfp_nsp *state);
int nfp_nsp_device_activate(struct nfp_nsp *state);
int nfp_nsp_load_fw(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_mac_reinit(struct nfp_nsp *state);
int nfp_nsp_read_identify(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_read_sensors(struct nfp_nsp *state, uint32_t sensor_mask,
		void *buf, size_t size);

static inline bool
nfp_nsp_has_mac_reinit(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 20;
}

static inline bool
nfp_nsp_has_stored_fw_load(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 23;
}

static inline bool
nfp_nsp_has_hwinfo_lookup(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 24;
}

static inline bool
nfp_nsp_has_hwinfo_set(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 25;
}

static inline bool
nfp_nsp_has_fw_loaded(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 25;
}

static inline bool
nfp_nsp_has_versions(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 27;
}

static inline bool
nfp_nsp_has_read_module_eeprom(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 28;
}

static inline bool
nfp_nsp_has_read_media(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 33;
}

enum nfp_eth_interface {
	NFP_INTERFACE_NONE      = 0,
	NFP_INTERFACE_SFP       = 1,
	NFP_INTERFACE_SFPP      = 10,
	NFP_INTERFACE_SFP28     = 28,
	NFP_INTERFACE_QSFP      = 40,
	NFP_INTERFACE_RJ45      = 45,
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
	NFP_FEC_INVALID_BIT,
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
		enum nfp_eth_fec act_fec; /**< Active Forward Error Correction mode */
		enum nfp_eth_aneg aneg;   /**< Auto negotiation mode */

		struct rte_ether_addr mac_addr;  /**< Interface MAC address */

		uint8_t label_port;    /**< Port id */
		/** Id of interface within port (for split ports) */
		uint8_t label_subport;

		bool enabled;     /**< Enable port */
		bool tx_enabled;  /**< Enable TX */
		bool rx_enabled;  /**< Enable RX */
		bool supp_aneg;   /**< Support auto negotiation */

		bool override_changed;  /**< Media reconfig pending */
		bool rx_pause_enabled;  /**< Switch of RX pause frame */
		bool tx_pause_enabled;  /**< Switch of TX pause frame */

		uint8_t port_type;    /**< One of %PORT_* */
		/** Sum of lanes of all subports of this port */
		uint32_t port_lanes;

		bool is_split;   /**< Split port */

		uint32_t fec_modes_supported;  /**< Bitmap of FEC modes supported */
	} ports[]; /**< Table of ports */
};

struct nfp_eth_table *nfp_eth_read_ports(struct nfp_cpp *cpp);

int nfp_eth_set_mod_enable(struct nfp_cpp *cpp, uint32_t idx, bool enable);
int nfp_eth_set_configured(struct nfp_cpp *cpp, uint32_t idx, bool configured);
int nfp_eth_set_fec(struct nfp_cpp *cpp, uint32_t idx, enum nfp_eth_fec mode);

int nfp_nsp_read_eth_table(struct nfp_nsp *state, void *buf, size_t size);
int nfp_nsp_write_eth_table(struct nfp_nsp *state, const void *buf,
		size_t size);
void nfp_nsp_config_set_state(struct nfp_nsp *state, void *entries,
		uint32_t idx);
void nfp_nsp_config_clear_state(struct nfp_nsp *state);
void nfp_nsp_config_set_modified(struct nfp_nsp *state, bool modified);
void *nfp_nsp_config_entries(struct nfp_nsp *state);
struct nfp_cpp *nfp_nsp_cpp(struct nfp_nsp *state);
bool nfp_nsp_config_modified(struct nfp_nsp *state);
uint32_t nfp_nsp_config_idx(struct nfp_nsp *state);
int nfp_nsp_hwinfo_set(struct nfp_nsp *state,
		const void *buf,
		size_t size);
int nfp_nsp_read_media(struct nfp_nsp *state,
		void *buf,
		size_t size);

static inline bool
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
int nfp_eth_set_tx_pause(struct nfp_nsp *nsp, bool tx_pause);
int nfp_eth_set_rx_pause(struct nfp_nsp *nsp, bool rx_pause);
int nfp_eth_set_idmode(struct nfp_cpp *cpp, uint32_t idx, bool is_on);

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
bool nfp_nsp_fw_loaded(struct nfp_nsp *state);
int nfp_nsp_load_stored_fw(struct nfp_nsp *state);
int nfp_nsp_hwinfo_lookup(struct nfp_nsp *state, void *buf, uint32_t size);
int nfp_nsp_hwinfo_lookup_optional(struct nfp_nsp *state,
		void *buf, size_t size, const char *default_val);
int nfp_nsp_read_module_eeprom(struct nfp_nsp *state, int eth_index,
		uint32_t offset, void *data,
		uint32_t len, uint32_t *read_len);

/* The buf used to receive bitmap of link modes */
struct nfp_eth_media_buf {
	uint8_t eth_index;
	uint8_t reserved[7];
	uint64_t supported_modes[2];
	uint64_t reserved_2[2];
};

/* Link modes about RJ45 haven't been used, so there's no mapping to them */
enum nfp_link_mode_list {
	NFP_MEDIA_W0_RJ45_10M,
	NFP_MEDIA_W0_RJ45_10M_HD,
	NFP_MEDIA_W0_RJ45_100M,
	NFP_MEDIA_W0_RJ45_100M_HD,
	NFP_MEDIA_W0_RJ45_1G,
	NFP_MEDIA_W0_RJ45_2P5G,
	NFP_MEDIA_W0_RJ45_5G,
	NFP_MEDIA_W0_RJ45_10G,
	NFP_MEDIA_1000BASE_CX,
	NFP_MEDIA_1000BASE_KX,
	NFP_MEDIA_10GBASE_KX4,
	NFP_MEDIA_10GBASE_KR,
	NFP_MEDIA_10GBASE_CX4,
	NFP_MEDIA_10GBASE_CR,
	NFP_MEDIA_10GBASE_SR,
	NFP_MEDIA_10GBASE_ER,
	NFP_MEDIA_25GBASE_KR,
	NFP_MEDIA_25GBASE_KR_S,
	NFP_MEDIA_25GBASE_CR,
	NFP_MEDIA_25GBASE_CR_S,
	NFP_MEDIA_25GBASE_SR,
	NFP_MEDIA_40GBASE_CR4,
	NFP_MEDIA_40GBASE_KR4,
	NFP_MEDIA_40GBASE_SR4,
	NFP_MEDIA_40GBASE_LR4,
	NFP_MEDIA_50GBASE_KR,
	NFP_MEDIA_50GBASE_SR,
	NFP_MEDIA_50GBASE_CR,
	NFP_MEDIA_50GBASE_LR,
	NFP_MEDIA_50GBASE_ER,
	NFP_MEDIA_50GBASE_FR,
	NFP_MEDIA_100GBASE_KR4,
	NFP_MEDIA_100GBASE_SR4,
	NFP_MEDIA_100GBASE_CR4,
	NFP_MEDIA_100GBASE_KP4,
	NFP_MEDIA_100GBASE_CR10,
	NFP_MEDIA_10GBASE_LR,
	NFP_MEDIA_25GBASE_LR,
	NFP_MEDIA_25GBASE_ER,
	NFP_MEDIA_LINK_MODES_NUMBER
};

#endif /* __NSP_NSP_H__ */
