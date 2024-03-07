/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2018-2025 NXP
 *
 */

#ifndef __FSL_DPMAC_H
#define __FSL_DPMAC_H

/** @addtogroup dpmac Data Path MAC API
 * Contains initialization APIs and runtime control APIs for DPMAC
 * @{
 */

struct fsl_mc_io;

int dpmac_open(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       int dpmac_id,
	       uint16_t *token);

int dpmac_close(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

/**
 * enum dpmac_link_type -  DPMAC link type
 * @DPMAC_LINK_TYPE_NONE: No link
 * @DPMAC_LINK_TYPE_FIXED: Link is fixed type
 * @DPMAC_LINK_TYPE_PHY: Link by PHY ID
 * @DPMAC_LINK_TYPE_BACKPLANE: Backplane link type
 */
enum dpmac_link_type {
	DPMAC_LINK_TYPE_NONE,
	DPMAC_LINK_TYPE_FIXED,
	DPMAC_LINK_TYPE_PHY,
	DPMAC_LINK_TYPE_BACKPLANE
};

/**
 * enum dpmac_eth_if - DPMAC Ethrnet interface
 * @DPMAC_ETH_IF_MII: MII interface
 * @DPMAC_ETH_IF_RMII: RMII interface
 * @DPMAC_ETH_IF_SMII: SMII interface
 * @DPMAC_ETH_IF_GMII: GMII interface
 * @DPMAC_ETH_IF_RGMII: RGMII interface
 * @DPMAC_ETH_IF_SGMII: SGMII interface
 * @DPMAC_ETH_IF_QSGMII: QSGMII interface
 * @DPMAC_ETH_IF_XAUI: XAUI interface
 * @DPMAC_ETH_IF_XFI: XFI interface
 * @DPMAC_ETH_IF_CAUI: CAUI interface
 * @DPMAC_ETH_IF_1000BASEX: 1000BASEX interface
 * @DPMAC_ETH_IF_USXGMII: USXGMII interface
 */
enum dpmac_eth_if {
	DPMAC_ETH_IF_MII,
	DPMAC_ETH_IF_RMII,
	DPMAC_ETH_IF_SMII,
	DPMAC_ETH_IF_GMII,
	DPMAC_ETH_IF_RGMII,
	DPMAC_ETH_IF_SGMII,
	DPMAC_ETH_IF_QSGMII,
	DPMAC_ETH_IF_XAUI,
	DPMAC_ETH_IF_XFI,
	DPMAC_ETH_IF_CAUI,
	DPMAC_ETH_IF_1000BASEX,
	DPMAC_ETH_IF_USXGMII,
};

enum dpmac_link_speed {
	DPMAC_LINK_SPEED_10M = 0,
	DPMAC_LINK_SPEED_100M,
	DPMAC_LINK_SPEED_1G,
	DPMAC_LINK_SPEED_2_5G,
	DPMAC_LINK_SPEED_5G,
	DPMAC_LINK_SPEED_10G,
	DPMAC_LINK_SPEED_25G,
	DPMAC_LINK_SPEED_40G,
	DPMAC_LINK_SPEED_50G,
	DPMAC_LINK_SPEED_100G,
	DPMAC_LINK_SPEED_MAX,
};

/*
 * @DPMAC_FEC_NONE: RS-FEC (enabled by default) is disabled
 * @DPMAC_FEC_RS: RS-FEC (Clause 91) mode configured
 * @DPMAC_FEC_FC: FC-FEC (Clause 74) mode configured (not yet supported)
 */
enum dpmac_fec_mode {
	DPMAC_FEC_NONE,
	DPMAC_FEC_RS,
	DPMAC_FEC_FC,
};

/* serdes sfi/custom settings feature internals
 * @SERDES_CFG_DEFAULT: the default configuration.
 * @SERDES_CFG_SFI: default operating mode for XFI interfaces
 * @SERDES_CFG_CUSTOM: It allows the user to manually configure the type of equalization,
 *	amplitude, preq and post1q settings. Can be used with all interfaces except RGMII.
 */
enum serdes_eq_cfg_mode {
	SERDES_CFG_DEFAULT = 0,
	SERDES_CFG_SFI,
	SERDES_CFG_CUSTOM,
};

/**
 * struct dpmac_cfg - Structure representing DPMAC configuration
 * @mac_id:	Represents the Hardware MAC ID; in case of multiple WRIOP,
 *		the MAC IDs are continuous.
 *		For example:  2 WRIOPs, 16 MACs in each:
 *				MAC IDs for the 1st WRIOP: 1-16,
 *				MAC IDs for the 2nd WRIOP: 17-32.
 */
struct dpmac_cfg {
	uint16_t mac_id;
};

int dpmac_create(struct fsl_mc_io *mc_io,
		 uint16_t dprc_token,
		 uint32_t cmd_flags,
		 const struct dpmac_cfg *cfg,
		 uint32_t *obj_id);

int dpmac_destroy(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  uint32_t object_id);

/**
 * DPMAC IRQ Index and Events
 */

/**
 * IRQ index
 */
#define DPMAC_IRQ_INDEX				0
/**
 * IRQ event - indicates a change in link state
 */
#define DPMAC_IRQ_EVENT_LINK_CFG_REQ		0x00000001
/**
 * IRQ event - Indicates that the link state changed
 */
#define DPMAC_IRQ_EVENT_LINK_CHANGED		0x00000002
/**
 * IRQ event - The object requests link up
 */
#define DPMAC_IRQ_EVENT_LINK_UP_REQ			0x00000004
/**
 * IRQ event - The object requests link down
 */
#define DPMAC_IRQ_EVENT_LINK_DOWN_REQ		0x00000008
/**
 * IRQ event - indicates a change in endpoint
 */
#define DPMAC_IRQ_EVENT_ENDPOINT_CHANGED	0x00000010

int dpmac_set_irq_enable(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 uint8_t irq_index,
			 uint8_t en);

int dpmac_get_irq_enable(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 uint8_t irq_index,
			 uint8_t *en);

int dpmac_set_irq_mask(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       uint8_t irq_index,
		       uint32_t mask);

int dpmac_get_irq_mask(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       uint8_t irq_index,
		       uint32_t *mask);

int dpmac_get_irq_status(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 uint8_t irq_index,
			 uint32_t *status);

int dpmac_clear_irq_status(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint8_t irq_index,
			   uint32_t status);

/**
 * @brief	Inter-Frame Gap mode
 *
 * LAN/WAN uses different Inter-Frame Gap mode
 */
enum dpmac_ifg_mode {
	DPMAC_IFG_MODE_FIXED,
	/*!< IFG length represents number of octets in steps of 4 */
	DPMAC_IFG_MODE_STRETCHED
	/*!< IFG length represents the stretch factor */
};

/**
 * @brief Structure representing Inter-Frame Gap mode configuration
 */
struct dpmac_ifg_cfg {
	enum dpmac_ifg_mode ipg_mode; /*!< WAN/LAN mode */
	uint8_t ipg_length; /*!< IPG Length, default value is 0xC */
};

/**
 * @brief Structure used to read through MDIO
 */
struct dpmac_mdio_read {
	uint8_t cl45; /*!< Clause 45 */
	uint8_t phy_addr; /*!< MDIO PHY address */
	uint16_t reg; /*!< PHY register */
};

/**
 * @brief Structure used to write through MDIO
 */
struct dpmac_mdio_write {
	uint8_t cl45; /*!< Clause 45 */
	uint8_t phy_addr; /*!< MDIO PHY address */
	uint16_t reg; /*!< PHY register */
	uint16_t data; /*!< Data to be written */
};

#define DPMAC_SET_PARAMS_IFG 0x1

/**
 * struct serdes_eq_settings - Structure  SerDes equalization settings
 * cfg: serdes sfi/custom/default settings feature internals
 * @eq_type: Number of levels of TX equalization
 * @sgn_preq: Precursor sign indicating direction of eye closure
 * @eq_preq: Drive strength of TX full swing transition bit to precursor
 * @sgn_post1q: First post-cursor sign indicating direction of eye closure
 * @eq_post1q: Drive strength of full swing transition bit to first post-cursor
 * @eq_amp_red: Overall transmit amplitude reduction
 */
struct serdes_eq_settings {
	enum serdes_eq_cfg_mode cfg;
	int eq_type;
	int sgn_preq;
	int eq_preq;
	int sgn_post1q;
	int eq_post1q;
	int eq_amp_red;
};

/**
 * struct dpmac_attr - Structure representing DPMAC attributes
 * @id:		DPMAC object ID
 * @max_rate:	Maximum supported rate - in Mbps
 * @eth_if:	Ethernet interface
 * @link_type:	link type
 * @fec_mode:	FEC mode - Configurable only for 25G interfaces
 * serdes_cfg:	SerDes equalization settings
 */
struct dpmac_attr {
	uint16_t id;
	uint32_t max_rate;
	enum dpmac_eth_if eth_if;
	enum dpmac_link_type link_type;
	enum dpmac_fec_mode fec_mode;
	struct serdes_eq_settings serdes_cfg;
	struct dpmac_ifg_cfg ifg_cfg;
};

int dpmac_get_attributes(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 struct dpmac_attr *attr);

int dpmac_set_params(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
			   uint32_t flags,
			   struct dpmac_ifg_cfg ifg_cfg);

int dpmac_get_mac_addr(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint8_t mac_addr[6]);

/**
 * DPMAC link configuration/state options
 */

/**
 * Enable auto-negotiation
 */
#define DPMAC_LINK_OPT_AUTONEG		0x0000000000000001ULL
/**
 * Enable half-duplex mode
 */
#define DPMAC_LINK_OPT_HALF_DUPLEX	0x0000000000000002ULL
/**
 * Enable pause frames
 */
#define DPMAC_LINK_OPT_PAUSE		0x0000000000000004ULL
/**
 * Enable a-symmetric pause frames
 */
#define DPMAC_LINK_OPT_ASYM_PAUSE	0x0000000000000008ULL
/**
 * Advertise 10MB full duplex
 */
#define DPMAC_ADVERTISED_10BASET_FULL           0x0000000000000001ULL
/**
 * Advertise 100MB full duplex
 */
#define DPMAC_ADVERTISED_100BASET_FULL          0x0000000000000002ULL
/**
 * Advertise 1GB full duplex
 */
#define DPMAC_ADVERTISED_1000BASET_FULL         0x0000000000000004ULL
/**
 * Advertise auto-negotiation enable
 */
#define DPMAC_ADVERTISED_AUTONEG                0x0000000000000008ULL
/**
 * Advertise 10GB full duplex
 */
#define DPMAC_ADVERTISED_10000BASET_FULL        0x0000000000000010ULL
/**
 * Advertise 2.5GB full duplex
 */
#define DPMAC_ADVERTISED_2500BASEX_FULL         0x0000000000000020ULL
/**
 * Advertise 5GB full duplex
 */
#define DPMAC_ADVERTISED_5000BASET_FULL         0x0000000000000040ULL

/**
 * struct dpmac_link_cfg - Structure representing DPMAC link configuration
 * @rate: Link's rate - in Mbps
 * @options: Enable/Disable DPMAC link cfg features (bitmap)
 * @advertising: Speeds that are advertised for autoneg (bitmap)
 */
struct dpmac_link_cfg {
	uint32_t rate;
	uint64_t options;
	uint64_t advertising;
};

int dpmac_get_link_cfg(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t token,
		       struct dpmac_link_cfg *cfg);

/**
 * struct dpmac_link_state - DPMAC link configuration request
 * @rate: Rate in Mbps
 * @options: Enable/Disable DPMAC link cfg features (bitmap)
 * @up: Link state
 * @state_valid: Ignore/Update the state of the link
 * @supported: Speeds capability of the phy (bitmap)
 * @advertising: Speeds that are advertised for autoneg (bitmap)
 */
struct dpmac_link_state {
	uint32_t rate;
	uint64_t options;
	int up;
	int state_valid;
	uint64_t supported;
	uint64_t advertising;
};

int dpmac_set_link_state(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 struct dpmac_link_state *link_state);

/**
 * enum dpmac_counter - DPMAC counter types
 * @DPMAC_CNT_ING_FRAME_64: counts 64-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_127: counts 65- to 127-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_255: counts 128- to 255-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_511: counts 256- to 511-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_1023: counts 512- to 1023-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_1518: counts 1024- to 1518-bytes frames, good or bad.
 * @DPMAC_CNT_ING_FRAME_1519_MAX: counts 1519-bytes frames and larger
 *				  (up to max frame length specified),
 *				  good or bad.
 * @DPMAC_CNT_ING_FRAG: counts frames which are shorter than 64 bytes received
 *			with a wrong CRC
 * @DPMAC_CNT_ING_JABBER: counts frames longer than the maximum frame length
 *			  specified, with a bad frame check sequence.
 * @DPMAC_CNT_ING_FRAME_DISCARD: counts dropped frames due to internal errors.
 *				 Occurs when a receive FIFO overflows.
 *				 Includes also frames truncated as a result of
 *				 the receive FIFO overflow.
 * @DPMAC_CNT_ING_ALIGN_ERR: counts frames with an alignment error
 *			     (optional used for wrong SFD).
 * @DPMAC_CNT_EGR_UNDERSIZED: counts frames transmitted that was less than 64
 *			      bytes long with a good CRC.
 * @DPMAC_CNT_ING_OVERSIZED: counts frames longer than the maximum frame length
 *			     specified, with a good frame check sequence.
 * @DPMAC_CNT_ING_VALID_PAUSE_FRAME: counts valid pause frames (regular and PFC)
 * @DPMAC_CNT_EGR_VALID_PAUSE_FRAME: counts valid pause frames transmitted
 *				     (regular and PFC).
 * @DPMAC_CNT_ING_BYTE: counts bytes received except preamble for all valid
 *			frames and valid pause frames.
 * @DPMAC_CNT_ING_MCAST_FRAME: counts received multicast frames.
 * @DPMAC_CNT_ING_BCAST_FRAME: counts received broadcast frames.
 * @DPMAC_CNT_ING_ALL_FRAME: counts each good or bad frames received.
 * @DPMAC_CNT_ING_UCAST_FRAME: counts received unicast frames.
 * @DPMAC_CNT_ING_ERR_FRAME: counts frames received with an error
 *			     (except for undersized/fragment frame).
 * @DPMAC_CNT_EGR_BYTE: counts bytes transmitted except preamble for all valid
 *			frames and valid pause frames transmitted.
 * @DPMAC_CNT_EGR_MCAST_FRAME: counts transmitted multicast frames.
 * @DPMAC_CNT_EGR_BCAST_FRAME: counts transmitted broadcast frames.
 * @DPMAC_CNT_EGR_UCAST_FRAME: counts transmitted unicast frames.
 * @DPMAC_CNT_EGR_ERR_FRAME: counts frames transmitted with an error.
 * @DPMAC_CNT_ING_GOOD_FRAME: counts frames received without error, including
 *			      pause frames.
 * @DPMAC_CNT_EGR_GOOD_FRAME: counts frames transmitted without error, including
 *			      pause frames.
 * @DPMAC_CNT_EGR_FRAME_64: counts transmitted 64-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_127: counts transmitted 65 to 127-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_255: counts transmitted 128 to 255-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_511: counts transmitted 256 to 511-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_1023: counts transmitted 512 to 1023-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_1518: counts transmitted 1024 to 1518-bytes frames, good or bad.
 * @DPMAC_CNT_EGR_FRAME_1519_MAX: counts transmitted 1519-bytes frames and
 * larger (up to max frame length specified), good or bad.
 * @DPMAC_CNT_ING_ALL_BYTE: counts bytes received in both good and bad packets
 * @DPMAC_CNT_ING_FCS_ERR: counts frames received with a CRC-32 error but the
 * frame is otherwise of correct length
 * @DPMAC_CNT_ING_VLAN_FRAME: counts the received VLAN tagged frames which are valid.
 * @DPMAC_CNT_ING_UNDERSIZED: counts received frames which were less than 64
 * bytes long and with a good CRC.
 * @DPMAC_CNT_ING_CONTROL_FRAME: counts received control frames (type 0x8808)
 * but not pause frames.
 * @DPMAC_CNT_ING_FRAME_DISCARD_NOT_TRUNC: counts the fully dropped frames (not
 * truncated) due to internal errors of the MAC client. Occurs when a received
 * FIFO overflows.
 * @DPMAC_CNT_EGR_ALL_BYTE: counts transmitted bytes in both good and bad
 * packets.
 * @DPMAC_CNT_EGR_FCS_ERR: counts transmitted frames with a CRC-32 error except
 * for underflows.
 * @DPMAC_CNT_EGR_VLAN_FRAME: counts the transmitted VLAN tagged frames which
 * are valid.
 * @DPMAC_CNT_EGR_ALL_FRAME: counts all transmitted frames, good or bad.
 * @DPMAC_CNT_EGR_CONTROL_FRAME: counts transmitted control frames (type
 * 0x8808) but not pause frames.
 */
enum dpmac_counter {
	DPMAC_CNT_ING_FRAME_64,
	DPMAC_CNT_ING_FRAME_127,
	DPMAC_CNT_ING_FRAME_255,
	DPMAC_CNT_ING_FRAME_511,
	DPMAC_CNT_ING_FRAME_1023,
	DPMAC_CNT_ING_FRAME_1518,
	DPMAC_CNT_ING_FRAME_1519_MAX,
	DPMAC_CNT_ING_FRAG,
	DPMAC_CNT_ING_JABBER,
	DPMAC_CNT_ING_FRAME_DISCARD,
	DPMAC_CNT_ING_ALIGN_ERR,
	DPMAC_CNT_EGR_UNDERSIZED,
	DPMAC_CNT_ING_OVERSIZED,
	DPMAC_CNT_ING_VALID_PAUSE_FRAME,
	DPMAC_CNT_EGR_VALID_PAUSE_FRAME,
	DPMAC_CNT_ING_BYTE,
	DPMAC_CNT_ING_MCAST_FRAME,
	DPMAC_CNT_ING_BCAST_FRAME,
	DPMAC_CNT_ING_ALL_FRAME,
	DPMAC_CNT_ING_UCAST_FRAME,
	DPMAC_CNT_ING_ERR_FRAME,
	DPMAC_CNT_EGR_BYTE,
	DPMAC_CNT_EGR_MCAST_FRAME,
	DPMAC_CNT_EGR_BCAST_FRAME,
	DPMAC_CNT_EGR_UCAST_FRAME,
	DPMAC_CNT_EGR_ERR_FRAME,
	DPMAC_CNT_ING_GOOD_FRAME,
	DPMAC_CNT_EGR_GOOD_FRAME,
	DPMAC_CNT_EGR_FRAME_64,
	DPMAC_CNT_EGR_FRAME_127,
	DPMAC_CNT_EGR_FRAME_255,
	DPMAC_CNT_EGR_FRAME_511,
	DPMAC_CNT_EGR_FRAME_1023,
	DPMAC_CNT_EGR_FRAME_1518,
	DPMAC_CNT_EGR_FRAME_1519_MAX,
	DPMAC_CNT_ING_ALL_BYTE,
	DPMAC_CNT_ING_FCS_ERR,
	DPMAC_CNT_ING_VLAN_FRAME,
	DPMAC_CNT_ING_UNDERSIZED,
	DPMAC_CNT_ING_CONTROL_FRAME,
	DPMAC_CNT_ING_FRAME_DISCARD_NOT_TRUNC,
	DPMAC_CNT_EGR_ALL_BYTE,
	DPMAC_CNT_EGR_FCS_ERR,
	DPMAC_CNT_EGR_VLAN_FRAME,
	DPMAC_CNT_EGR_ALL_FRAME,
	DPMAC_CNT_EGR_CONTROL_FRAME
};

int dpmac_get_counter(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpmac_counter  type,
		      uint64_t *counter);

int dpmac_get_api_version(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t *major_ver,
			  uint16_t *minor_ver);

int dpmac_reset(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token);

int dpmac_set_protocol(struct fsl_mc_io *mc_io, uint32_t cmd_flags,
		       uint16_t token, enum dpmac_eth_if protocol);

int dpmac_get_statistics(struct fsl_mc_io *mc_io, uint32_t cmd_flags, uint16_t token,
			 uint64_t iova_cnt, uint64_t iova_values, uint32_t num_cnt);

#endif /* __FSL_DPMAC_H */
