/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_hwif.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_nic_event.h"
#include "hinic3_ethdev.h"

static const char *g_hw_to_char_fec[HILINK_FEC_MAX_TYPE] = {
	"not set", "rsfec", "basefec", "nofec", "llrsfec",
};
static const char *g_hw_to_speed_info[PORT_SPEED_UNKNOWN] = {
	"not set", "10MB", "100MB", "1GB",   "10GB",
	"25GB",	   "40GB", "50GB",  "100GB", "200GB",
};
static const char *g_hw_to_an_state_info[PORT_CFG_AN_OFF + 1] = {
	"not set", "on", "off",
};

struct port_type_table {
	uint32_t port_type;
	const char *port_type_name;
};

void
hinic3_get_link_port_info(struct hinic3_hwdev *hwdev, uint8_t link_state,
	      struct rte_eth_link *link)
{
	uint32_t port_speed[LINK_SPEED_LEVELS] = {
		RTE_ETH_SPEED_NUM_NONE, RTE_ETH_SPEED_NUM_10M,
		RTE_ETH_SPEED_NUM_100M, RTE_ETH_SPEED_NUM_1G,
		RTE_ETH_SPEED_NUM_10G,	RTE_ETH_SPEED_NUM_25G,
		RTE_ETH_SPEED_NUM_40G,	RTE_ETH_SPEED_NUM_50G,
		RTE_ETH_SPEED_NUM_100G, RTE_ETH_SPEED_NUM_200G,
	};
	struct nic_port_info port_info = {0};
	int err;

	if (!link_state) {
		link->link_status = RTE_ETH_LINK_DOWN;
		link->link_speed = RTE_ETH_SPEED_NUM_NONE;
		link->link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		link->link_autoneg = RTE_ETH_LINK_FIXED;
	} else {
		link->link_status = RTE_ETH_LINK_UP;

		err = hinic3_get_port_info(hwdev, &port_info);
		if (err) {
			link->link_speed = RTE_ETH_SPEED_NUM_NONE;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			link->link_autoneg = RTE_ETH_LINK_FIXED;
		} else {
			link->link_speed =
				port_speed[port_info.speed % LINK_SPEED_LEVELS];
			link->link_duplex = port_info.duplex;
			link->link_autoneg = port_info.autoneg_state;
		}
	}
}

static void
hinic3_link_event_stats(struct hinic3_hwdev *hwdev, uint8_t link)
{
	if (link)
		rte_atomic_fetch_add_explicit(&hwdev->hw_stats.link_event_stats.link_up_stats,
			1, rte_memory_order_seq_cst);
	else
		rte_atomic_fetch_add_explicit(&hwdev->hw_stats.link_event_stats.link_down_stats,
			1, rte_memory_order_seq_cst);
}

static void
hinic3_set_vport_state(struct hinic3_hwdev *hwdev,
		       struct hinic3_cmd_link_state *link_state)
{
	struct hinic3_nic_dev *nic_dev = NULL;
	int err = 0;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(hwdev->eth_dev);

	if (link_state->state) {
		if (hinic3_get_bit(HINIC3_DEV_START, &nic_dev->dev_status))
			err = hinic3_set_vport_enable(hwdev, true);
	} else {
		err = hinic3_set_vport_enable(hwdev, false);
	}

	if (err)
		PMD_DRV_LOG(ERR, "Set vport status failed");
}

static void
link_status_event_handler(struct hinic3_hwdev *hwdev, struct hinic3_handler_info *handler_info)
{
	struct hinic3_cmd_link_state *link_status = NULL;
	struct rte_eth_link link;
	int err;

	link_status = handler_info->buf_in;
	PMD_DRV_LOG(INFO,
		    "Link status report received, func_id: %d, status: %d(%s)",
		    hinic3_global_func_id(hwdev), link_status->state,
		    link_status->state ? "UP" : "DOWN");

	hinic3_link_event_stats(hwdev, link_status->state);

	hinic3_set_vport_state(hwdev, link_status);

	/* Link event reported only after set vport enable. */
	hinic3_get_link_port_info(hwdev, link_status->state, &link);
	err = rte_eth_linkstatus_set(hwdev->eth_dev, &link);
	if (!err && hwdev->eth_dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_callback_process(hwdev->eth_dev,
					    RTE_ETH_EVENT_INTR_LSC, NULL);
}

static const char *
get_port_type_name(uint32_t type)
{
	uint32_t i;
	const struct port_type_table port_optical_type_table_s[] = {
		{LINK_PORT_UNKNOWN, "UNKNOWN"},
		{LINK_PORT_OPTICAL_MM, "optical_sr"},
		{LINK_PORT_OPTICAL_SM, "optical_lr"},
		{LINK_PORT_PAS_COPPER, "copper"},
		{LINK_PORT_ACC, "ACC"},
		{LINK_PORT_BASET, "baset"},
		{LINK_PORT_AOC, "AOC"},
		{LINK_PORT_ELECTRIC, "electric"},
		{LINK_PORT_BACKBOARD_INTERFACE, "interface"},
	};

	for (i = 0; i < RTE_DIM(port_optical_type_table_s); i++) {
		if (type == port_optical_type_table_s[i].port_type)
			return port_optical_type_table_s[i].port_type_name;
	}
	return "UNKNOWN TYPE";
}

static void
get_port_type(struct mag_cmd_event_port_info *port_info, const char **port_type)
{
	if (port_info->port_type <= LINK_PORT_BACKBOARD_INTERFACE)
		*port_type = get_port_type_name(port_info->port_type);
	else
		PMD_DRV_LOG(INFO, "Unknown port type: %u", port_info->port_type);
}

static int
get_port_temperature_power(struct mag_cmd_event_port_info *info, char *str)
{
	char arr[CAP_INFO_MAX_LEN];

	snprintf(arr, CAP_INFO_MAX_LEN - 1, "%s, %s, Temperature: %u", str,
		 info->sfp_type ? "QSFP" : "SFP", info->cable_temp);
	if (info->sfp_type)
		snprintf(str, CAP_INFO_MAX_LEN - 1,
			 "%s, rx power: %uuw %uuW %uuW %uuW", arr,
			 info->power[0x0], info->power[0x1], info->power[0x2],
			 info->power[0x3]);
	else
		snprintf(str, CAP_INFO_MAX_LEN - 1,
			 "%s, rx power: %uuW, tx power: %uuW", arr,
			 info->power[0x0], info->power[0x1]);

	return 0;
}

static void
print_cable_info(struct mag_cmd_event_port_info *port_info)
{
	char str[CAP_INFO_MAX_LEN] = {0};
	char vendor[VENDOR_MAX_LEN] = {0};
	const char *port_type = "Unknown port type";
	int i;
	int err = 0;
	if (port_info->gpio_insert) {
		PMD_DRV_LOG(INFO, "Cable unpresent");
		return;
	}

	get_port_type(port_info, &port_type);

	for (i = (int)sizeof(port_info->vendor_name) - 1; i >= 0; i--) {
		if (port_info->vendor_name[i] == ' ')
			port_info->vendor_name[i] = '\0';
		else
			break;
	}

	memcpy(vendor, port_info->vendor_name,
	       sizeof(port_info->vendor_name));
	snprintf(str, CAP_INFO_MAX_LEN - 1,
		     "Vendor: %s, %s, length: %um, max_speed: %uGbps",
		     vendor, port_type, port_info->cable_length,
		     port_info->max_speed);

	if (port_info->port_type == LINK_PORT_OPTICAL_MM ||
	    port_info->port_type == LINK_PORT_AOC) {
		err = get_port_temperature_power(port_info, str);
		if (err)
			return;
	}

	PMD_DRV_LOG(INFO, "Cable information: %s", str);
}

static void
print_link_info(struct mag_cmd_event_port_info *port_info)
{
	const char *fec = "None";
	const char *speed = "None";
	const char *an_state = "None";

	if (port_info->fec < HILINK_FEC_MAX_TYPE)
		fec = g_hw_to_char_fec[port_info->fec];
	else
		PMD_DRV_LOG(INFO, "Unknown fec type: %u", port_info->fec);

	if (port_info->an_state > PORT_CFG_AN_OFF) {
		PMD_DRV_LOG(INFO, "an_state %u is invalid",
			    port_info->an_state);
		return;
	}

	an_state = g_hw_to_an_state_info[port_info->an_state];

	if (port_info->speed >= PORT_SPEED_UNKNOWN) {
		PMD_DRV_LOG(INFO, "speed %u is invalid", port_info->speed);
		return;
	}

	speed = g_hw_to_speed_info[port_info->speed];
	PMD_DRV_LOG(INFO, "Link information: speed %s, %s, autoneg %s",
				speed, fec, an_state);
}

static void
print_port_info(struct hinic3_hwdev *hwdev, struct mag_cmd_event_port_info *port_info, uint8_t type)
{
	print_cable_info(port_info);

	print_link_info(port_info);

	if (type == RTE_ETH_LINK_UP)
		return;

	PMD_DRV_LOG(INFO, "Function %d link down msg:",
		    hinic3_global_func_id(hwdev));

	PMD_DRV_LOG(DEBUG,
		    "PMA ctrl: %s, tx %s, rx %s, PMA fifo reg: 0x%x, PMA signal ok reg: 0x%x, RF/LF status reg: 0x%x",
		    port_info->pma_ctrl == 1 ? "off" : "on",
		    port_info->tx_enable ? "enable" : "disable",
		    port_info->rx_enable ? "enable" : "disable",
		    port_info->pma_fifo_reg, port_info->pma_signal_ok_reg,
		    port_info->rf_lf);
	PMD_DRV_LOG(DEBUG,
		    "alos: %u, rx_los: %u, PCS 64 66b reg: 0x%x, PCS link: 0x%x, MAC link: 0x%x, PCS_err_cnt: 0x%x",
		    port_info->alos, port_info->rx_los,
		    port_info->pcs_64_66b_reg, port_info->pcs_link,
		    port_info->pcs_mac_link, port_info->pcs_err_cnt);
	PMD_DRV_LOG(DEBUG,
		    "his_link_machine_state = 0x%08x, cur_link_machine_state = 0x%08x",
		    port_info->his_link_machine_state,
		    port_info->cur_link_machine_state);
}

static void
port_info_event_printf(struct hinic3_hwdev *hwdev, struct hinic3_handler_info *handler_info)
{
	struct mag_cmd_event_port_info *port_info = handler_info->buf_in;
	((struct mag_cmd_event_port_info *)(handler_info->buf_out))->head.status = 0;
	enum hinic3_nic_event_type type = port_info->event_type;
	if (type < RTE_ETH_LINK_DOWN || type > RTE_ETH_LINK_UP) {
		PMD_DRV_LOG(ERR, "Invalid hilink info report, type: %d", type);
		return;
	}

	print_port_info(hwdev, port_info, type);
}

struct nic_event_handler {
	uint16_t cmd;
	void (*handler)(struct hinic3_hwdev *hwdev, struct hinic3_handler_info *handler_info);
};

static const struct nic_event_handler nic_cmd_handler[] = {};

/**
 * Handle NIC event based on the provided command.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] handler_info
 * Pointer to handler info structure
 */
static void
nic_event_handler(struct hinic3_hwdev *hwdev, struct hinic3_handler_info *handler_info)
{
	uint32_t i, size = RTE_DIM(nic_cmd_handler);

	if (!hwdev)
		return;

	*handler_info->out_size = 0;

	for (i = 0; i < size; i++) {
		if (handler_info->cmd == nic_cmd_handler[i].cmd) {
			nic_cmd_handler[i].handler(hwdev, handler_info);
			break;
		}
	}

	if (i == size)
		PMD_DRV_LOG(ERR, "Unsupported nic event cmd(%d) to process", handler_info->cmd);
}

/*
 * VF handler mbox msg from ppf/pf.
 * VF link change event.
 */
int
hinic3_vf_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			struct hinic3_handler_info *handler_info)
{
	nic_event_handler(hwdev, handler_info);
	return 0;
}

/* NIC event of PF/PPF handler reported by mgmt cpu. */
void
hinic3_pf_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			struct hinic3_handler_info *handler_info)
{
	nic_event_handler(hwdev, handler_info);
}

static const struct nic_event_handler mag_cmd_handler[] = {
	{
		.cmd = MAG_CMD_GET_LINK_STATUS,
		.handler = link_status_event_handler,
	},
	{
		.cmd = MAG_CMD_EVENT_PORT_INFO,
		.handler = port_info_event_printf,
	},
};

static int
hinic3_mag_event_handler(struct hinic3_hwdev *hwdev, struct hinic3_handler_info *handler_info)
{
	uint32_t size = RTE_DIM(mag_cmd_handler);
	uint32_t i;

	if (!hwdev)
		return -EINVAL;

	*handler_info->out_size = 0;
	for (i = 0; i < size; i++) {
		if (handler_info->cmd == mag_cmd_handler[i].cmd) {
			mag_cmd_handler[i].handler(hwdev, handler_info);
			break;
		}
	}

	/* Can't find this event cmd. */
	if (i == size)
		PMD_DRV_LOG(ERR, "Unsupported mag event, cmd: %u", handler_info->cmd);

	return 0;
}

int
hinic3_vf_mag_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			    struct hinic3_handler_info *handler_info)
{
	return hinic3_mag_event_handler(hwdev, handler_info);
}

/* pf/ppf handler mgmt cpu report hilink event. */
void
hinic3_pf_mag_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			    struct hinic3_handler_info *handler_info)
{
	hinic3_mag_event_handler(hwdev, handler_info);
}

uint8_t
hinic3_nic_sw_aeqe_handler(__rte_unused struct hinic3_hwdev *hwdev, uint8_t event, uint8_t *data)
{
	PMD_DRV_LOG(ERR,
		    "Received nic ucode aeq event type: 0x%x, data: %" PRIu64,
		    event, *((uint64_t *)data));

	return 0;
}
