/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_MCP_API_H__
#define __ECORE_MCP_API_H__

#include "ecore_status.h"

struct ecore_mcp_link_speed_params {
	bool autoneg;
	u32 advertised_speeds; /* bitmask of DRV_SPEED_CAPABILITY */
	u32 forced_speed; /* In Mb/s */
};

struct ecore_mcp_link_pause_params {
	bool autoneg;
	bool forced_rx;
	bool forced_tx;
};

struct ecore_mcp_link_params {
	struct ecore_mcp_link_speed_params speed;
	struct ecore_mcp_link_pause_params pause;
	u32 loopback_mode; /* in PMM_LOOPBACK values */
};

struct ecore_mcp_link_capabilities {
	u32 speed_capabilities;
	bool default_speed_autoneg; /* In Mb/s */
	u32 default_speed; /* In Mb/s */
};

struct ecore_mcp_link_state {
	bool link_up;

	u32 line_speed; /* In Mb/s */
	u32 min_pf_rate; /* In Mb/s */
	u32 speed; /* In Mb/s */
	bool full_duplex;

	bool an;
	bool an_complete;
	bool parallel_detection;
	bool pfc_enabled;

#define ECORE_LINK_PARTNER_SPEED_1G_HD	(1 << 0)
#define ECORE_LINK_PARTNER_SPEED_1G_FD	(1 << 1)
#define ECORE_LINK_PARTNER_SPEED_10G	(1 << 2)
#define ECORE_LINK_PARTNER_SPEED_20G	(1 << 3)
#define ECORE_LINK_PARTNER_SPEED_25G	(1 << 4)
#define ECORE_LINK_PARTNER_SPEED_40G	(1 << 5)
#define ECORE_LINK_PARTNER_SPEED_50G	(1 << 6)
#define ECORE_LINK_PARTNER_SPEED_100G	(1 << 7)
	u32 partner_adv_speed;

	bool partner_tx_flow_ctrl_en;
	bool partner_rx_flow_ctrl_en;

#define ECORE_LINK_PARTNER_SYMMETRIC_PAUSE (1)
#define ECORE_LINK_PARTNER_ASYMMETRIC_PAUSE (2)
#define ECORE_LINK_PARTNER_BOTH_PAUSE (3)
	u8 partner_adv_pause;

	bool sfp_tx_fault;
};

struct ecore_mcp_function_info {
	u8 pause_on_host;

	enum ecore_pci_personality protocol;

	u8 bandwidth_min;
	u8 bandwidth_max;

	u8 mac[ETH_ALEN];

	u64 wwn_port;
	u64 wwn_node;

#define ECORE_MCP_VLAN_UNSET		(0xffff)
	u16 ovlan;
};

struct ecore_mcp_nvm_common {
	u32 offset;
	u32 param;
	u32 resp;
	u32 cmd;
};

struct ecore_mcp_nvm_rd {
	u32 *buf_size;
	u32 *buf;
};

struct ecore_mcp_nvm_wr {
	u32 buf_size;
	u32 *buf;
};

struct ecore_mcp_nvm_params {
#define ECORE_MCP_CMD		(1 << 0)
#define ECORE_MCP_NVM_RD	(1 << 1)
#define ECORE_MCP_NVM_WR	(1 << 2)
	u8 type;

	struct ecore_mcp_nvm_common nvm_common;

	union {
		struct ecore_mcp_nvm_rd nvm_rd;
		struct ecore_mcp_nvm_wr nvm_wr;
	};
};

#ifndef __EXTRACT__LINUX__
enum ecore_nvm_images {
	ECORE_NVM_IMAGE_ISCSI_CFG,
	ECORE_NVM_IMAGE_FCOE_CFG,
};
#endif

struct ecore_mcp_drv_version {
	u32 version;
	u8 name[MCP_DRV_VER_STR_SIZE - 4];
};

struct ecore_mcp_lan_stats {
	u64 ucast_rx_pkts;
	u64 ucast_tx_pkts;
	u32 fcs_err;
};

#ifndef ECORE_PROTO_STATS
#define ECORE_PROTO_STATS
struct ecore_mcp_fcoe_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u32 fcs_err;
	u32 login_failure;
};

struct ecore_mcp_iscsi_stats {
	u64 rx_pdus;
	u64 tx_pdus;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct ecore_mcp_rdma_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u64 rx_bytes;
	u64 tx_byts;
};

enum ecore_mcp_protocol_type {
	ECORE_MCP_LAN_STATS,
	ECORE_MCP_FCOE_STATS,
	ECORE_MCP_ISCSI_STATS,
	ECORE_MCP_RDMA_STATS
};

union ecore_mcp_protocol_stats {
	struct ecore_mcp_lan_stats lan_stats;
	struct ecore_mcp_fcoe_stats fcoe_stats;
	struct ecore_mcp_iscsi_stats iscsi_stats;
	struct ecore_mcp_rdma_stats rdma_stats;
};
#endif

enum ecore_ov_config_method {
	ECORE_OV_CONFIG_MTU,
	ECORE_OV_CONFIG_MAC,
	ECORE_OV_CONFIG_WOL
};

enum ecore_ov_client {
	ECORE_OV_CLIENT_DRV,
	ECORE_OV_CLIENT_USER
};

enum ecore_ov_driver_state {
	ECORE_OV_DRIVER_STATE_NOT_LOADED,
	ECORE_OV_DRIVER_STATE_DISABLED,
	ECORE_OV_DRIVER_STATE_ACTIVE
};

#define ECORE_MAX_NPIV_ENTRIES 128
#define ECORE_WWN_SIZE 8
struct ecore_fc_npiv_tbl {
	u32 count;
	u8 wwpn[ECORE_MAX_NPIV_ENTRIES][ECORE_WWN_SIZE];
	u8 wwnn[ECORE_MAX_NPIV_ENTRIES][ECORE_WWN_SIZE];
};

#ifndef __EXTRACT__LINUX__
enum ecore_led_mode {
	ECORE_LED_MODE_OFF,
	ECORE_LED_MODE_ON,
	ECORE_LED_MODE_RESTORE
};
#endif

struct ecore_temperature_sensor {
	u8 sensor_location;
	u8 threshold_high;
	u8 critical;
	u8 current_temp;
};

#define ECORE_MAX_NUM_OF_SENSORS	7
struct ecore_temperature_info {
	u32 num_sensors;
	struct ecore_temperature_sensor sensors[ECORE_MAX_NUM_OF_SENSORS];
};

enum ecore_mba_img_idx {
	ECORE_MBA_LEGACY_IDX,
	ECORE_MBA_PCI3CLP_IDX,
	ECORE_MBA_PCI3_IDX,
	ECORE_MBA_FCODE_IDX,
	ECORE_EFI_X86_IDX,
	ECORE_EFI_IPF_IDX,
	ECORE_EFI_EBC_IDX,
	ECORE_EFI_X64_IDX,
	ECORE_MAX_NUM_OF_ROMIMG
};

struct ecore_mba_vers {
	u32 mba_vers[ECORE_MAX_NUM_OF_ROMIMG];
};

/**
 * @brief - returns the link params of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link params
 */
struct ecore_mcp_link_params *ecore_mcp_get_link_params(struct ecore_hwfn *);

/**
 * @brief - return the link state of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link state
 */
struct ecore_mcp_link_state *ecore_mcp_get_link_state(struct ecore_hwfn *);

/**
 * @brief - return the link capabilities of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to link capabilities
 */
struct ecore_mcp_link_capabilities
*ecore_mcp_get_link_capabilities(struct ecore_hwfn *p_hwfn);

/**
 * @brief Request the MFW to set the the link according to 'link_input'.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param b_up - raise link if `true'. Reset link if `false'.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_mcp_set_link(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					bool b_up);

/**
 * @brief Get the management firmware version value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_mfw_ver    - mfw version value
 * @param p_running_bundle_id	- image id in nvram; Optional.
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_get_mfw_ver(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *p_ptt,
					   u32 *p_mfw_ver,
					   u32 *p_running_bundle_id);

/**
 * @brief Get media type value of the port.
 *
 * @param p_dev      - ecore dev pointer
 * @param mfw_ver    - media type value
 *
 * @return enum _ecore_status_t -
 *      ECORE_SUCCESS - Operation was successful.
 *      ECORE_BUSY - Operation failed
 */
enum _ecore_status_t ecore_mcp_get_media_type(struct ecore_dev *p_dev,
					   u32 *media_type);

/**
 * @brief - Sends a command to the MCP mailbox.
 *
 * @param p_hwfn      - hw function
 * @param p_ptt       - PTT required for register access
 * @param cmd         - command to be sent to the MCP
 * @param param       - optional param
 * @param o_mcp_resp  - the MCP response code (exclude sequence)
 * @param o_mcp_param - optional parameter provided by the MCP response
 *
 * @return enum _ecore_status_t -
 *      ECORE_SUCCESS - operation was successful
 *      ECORE_BUSY    - operation failed
 */
enum _ecore_status_t ecore_mcp_cmd(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt, u32 cmd, u32 param,
				   u32 *o_mcp_resp, u32 *o_mcp_param);

/**
 * @brief - drains the nig, allowing completion to pass in case of pauses.
 *          (Should be called only from sleepable context)
 *
 * @param p_hwfn
 * @param p_ptt
 */
enum _ecore_status_t ecore_mcp_drain(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt);

#ifndef LINUX_REMOVE
/**
 * @brief - return the mcp function info of the hw function
 *
 * @param p_hwfn
 *
 * @returns pointer to mcp function info
 */
const struct ecore_mcp_function_info
*ecore_mcp_get_function_info(struct ecore_hwfn *p_hwfn);
#endif

/**
 * @brief - Function for reading/manipulating the nvram. Following are supported
 *          functionalities.
 *          1. Read: Read the specified nvram offset.
 *             input values:
 *               type   - ECORE_MCP_NVM_RD
 *               cmd    - command code (e.g. DRV_MSG_CODE_NVM_READ_NVRAM)
 *               offset - nvm offset
 *
 *             output values:
 *               buf      - buffer
 *               buf_size - buffer size
 *
 *          2. Write: Write the data at the specified nvram offset
 *             input values:
 *               type     - ECORE_MCP_NVM_WR
 *               cmd      - command code (e.g. DRV_MSG_CODE_NVM_WRITE_NVRAM)
 *               offset   - nvm offset
 *               buf      - buffer
 *               buf_size - buffer size
 *
 *          3. Command: Send the NVM command to MCP.
 *             input values:
 *               type   - ECORE_MCP_CMD
 *               cmd    - command code (e.g. DRV_MSG_CODE_NVM_DEL_FILE)
 *               offset - nvm offset
 *
 *
 * @param p_hwfn
 * @param p_ptt
 * @param params
 *
 * @return ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_command(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *p_ptt,
					   struct ecore_mcp_nvm_params *params);

#ifndef LINUX_REMOVE
/**
 * @brief - count number of function with a matching personality on engine.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param personalities - a bitmask of ecore_pci_personality values
 *
 * @returns the count of all devices on engine whose personality match one of
 *          the bitsmasks.
 */
int ecore_mcp_get_personality_cnt(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_ptt,
				  u32 personalities);
#endif

/**
 * @brief Get the flash size value
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_flash_size  - flash size in bytes to be filled.
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_get_flash_size(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u32 *p_flash_size);

/**
 * @brief Send driver version to MFW
 *
 * @param p_hwfn
 * @param p_ptt
 * @param version - Version value
 * @param name - Protocol driver name
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t
ecore_mcp_send_drv_version(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			   struct ecore_mcp_drv_version *p_ver);

/**
 * @brief Read the MFW process kill counter
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return u32
 */
u32 ecore_get_process_kill_counter(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt);

/**
 * @brief Trigger a recovery process
 *
 *  @param p_hwfn
 *  @param p_ptt
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_start_recovery_process(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt);

/**
 * @brief Notify MFW about the change in base device properties
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param config - Configuation that has been updated
 *  @param client - ecore client type
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t
ecore_mcp_ov_update_current_config(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   enum ecore_ov_config_method config,
				   enum ecore_ov_client client);

/**
 * @brief Notify MFW about the driver state
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param drv_state - Driver state
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t
ecore_mcp_ov_update_driver_state(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt,
				 enum ecore_ov_driver_state drv_state);

/**
 * @brief Read NPIV settings form the MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param p_table - Array to hold the FC NPIV data. Client need allocate the
 *                   required buffer. The field 'count' specifies number of NPIV
 *                   entries. A value of 0 means the table was not populated.
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t
ecore_mcp_ov_get_fc_npiv(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			 struct ecore_fc_npiv_tbl *p_table);

/**
 * @brief Send MTU size to MFW
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param mtu - MTU size
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_ov_update_mtu(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt, u16 mtu);

/**
 * @brief Set LED status
 *
 *  @param p_hwfn
 *  @param p_ptt
 *  @param mode - LED mode
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_set_led(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       enum ecore_led_mode mode);

/**
 * @brief Set secure mode
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_set_secure_mode(struct ecore_dev *p_dev,
						   u32 addr);

/**
 * @brief Write to phy
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_phy_write(struct ecore_dev *p_dev, u32 cmd,
					 u32 addr, u8 *p_buf, u32 len);

/**
 * @brief Write to nvm
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_write(struct ecore_dev *p_dev, u32 cmd,
					 u32 addr, u8 *p_buf, u32 len);

/**
 * @brief Put file begin
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_put_file_begin(struct ecore_dev *p_dev,
						  u32 addr);

/**
 * @brief Delete file
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_del_file(struct ecore_dev *p_dev,
					    u32 addr);

/**
 * @brief Check latest response
 *
 *  @param p_dev
 *  @param p_buf - nvm write buffer
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_resp(struct ecore_dev *p_dev, u8 *p_buf);

/**
 * @brief Read from phy
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *  @param cmd - nvm command
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_phy_read(struct ecore_dev *p_dev, u32 cmd,
					u32 addr, u8 *p_buf, u32 len);

/**
 * @brief Read from nvm
 *
 *  @param p_dev
 *  @param addr - nvm offset
 *  @param p_buf - nvm write buffer
 *  @param len - buffer len
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_nvm_read(struct ecore_dev *p_dev, u32 addr,
			   u8 *p_buf, u32 len);

/**
 * @brief Read from sfp
 *
 *  @param p_hwfn - hw function
 *  @param p_ptt  - PTT required for register access
 *  @param port   - transceiver port
 *  @param addr   - I2C address
 *  @param offset - offset in sfp
 *  @param len    - buffer length
 *  @param p_buf  - buffer to read into
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_phy_sfp_read(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u32 port, u32 addr, u32 offset,
					    u32 len, u8 *p_buf);

/**
 * @brief Write to sfp
 *
 *  @param p_hwfn - hw function
 *  @param p_ptt  - PTT required for register access
 *  @param port   - transceiver port
 *  @param addr   - I2C address
 *  @param offset - offset in sfp
 *  @param len    - buffer length
 *  @param p_buf  - buffer to write from
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_phy_sfp_write(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 port, u32 addr, u32 offset,
					     u32 len, u8 *p_buf);

/**
 * @brief Gpio read
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *  @param gpio      - gpio number
 *  @param gpio_val  - value read from gpio
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_gpio_read(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 gpio, u32 *gpio_val);

/**
 * @brief Gpio write
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *  @param gpio      - gpio number
 *  @param gpio_val  - value to write to gpio
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_gpio_write(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u16 gpio, u16 gpio_val);

/**
 * @brief Gpio get information
 *
 *  @param p_hwfn          - hw function
 *  @param p_ptt           - PTT required for register access
 *  @param gpio            - gpio number
 *  @param gpio_direction  - gpio is output (0) or input (1)
 *  @param gpio_ctrl       - gpio control is uninitialized (0),
 *                         path 0 (1), path 1 (2) or shared(3)
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_gpio_info(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 gpio, u32 *gpio_direction,
					 u32 *gpio_ctrl);

/**
 * @brief Bist register test
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_bist_register_test(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt);

/**
 * @brief Bist clock test
 *
 *  @param p_hwfn    - hw function
 *  @param p_ptt     - PTT required for register access
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_bist_clock_test(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt);

/**
 * @brief Bist nvm test - get number of images
 *
 *  @param p_hwfn       - hw function
 *  @param p_ptt        - PTT required for register access
 *  @param num_images   - number of images if operation was
 *			  successful. 0 if not.
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_bist_nvm_test_get_num_images(
						struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt,
						u32 *num_images);

/**
 * @brief Bist nvm test - get image attributes by index
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param p_image_att - Attributes of image
 *  @param image_index - Index of image to get information for
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_bist_nvm_test_get_image_att(
					struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct bist_nvm_image_att *p_image_att,
					u32 image_index);

/**
 * @brief ecore_mcp_get_temperature_info - get the status of the temperature
 *                                         sensors
 *
 *  @param p_hwfn        - hw function
 *  @param p_ptt         - PTT required for register access
 *  @param p_temp_status - A pointer to an ecore_temperature_info structure to
 *                         be filled with the temperature data
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t
ecore_mcp_get_temperature_info(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt,
			       struct ecore_temperature_info *p_temp_info);

/**
 * @brief Get MBA versions - get MBA sub images versions
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param p_mba_vers  - MBA versions array to fill
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_get_mba_versions(
	struct ecore_hwfn *p_hwfn,
	struct ecore_ptt *p_ptt,
	struct ecore_mba_vers *p_mba_vers);

/**
 * @brief Count memory ecc events
 *
 *  @param p_hwfn      - hw function
 *  @param p_ptt       - PTT required for register access
 *  @param num_events  - number of memory ecc events
 *
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation was successful.
 */
enum _ecore_status_t ecore_mcp_mem_ecc_events(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u64 *num_events);

/**
 * @brief Sets whether a critical error notification from the MFW is acked, or
 *        is it being ignored and thus allowing the MFW crash dump.
 *
 * @param p_dev
 * @param mdump_enable
 *
 */
void ecore_mcp_mdump_enable(struct ecore_dev *p_dev, bool mdump_enable);

#endif
