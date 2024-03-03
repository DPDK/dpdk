/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CNXK_REP_MSG_H__
#define __CNXK_REP_MSG_H__

#include <stdint.h>

#define CNXK_REP_MSG_MAX_BUFFER_SZ 1500

typedef enum CNXK_TYPE {
	CNXK_TYPE_HEADER = 0,
	CNXK_TYPE_MSG,
} cnxk_type_t;

typedef enum CNXK_REP_MSG {
	/* General sync messages */
	CNXK_REP_MSG_READY = 0,
	CNXK_REP_MSG_ACK,
	CNXK_REP_MSG_EXIT,
	/* Ethernet operation msgs */
	CNXK_REP_MSG_ETH_SET_MAC,
	CNXK_REP_MSG_ETH_STATS_GET,
	CNXK_REP_MSG_ETH_STATS_CLEAR,
	/* End of messaging sequence */
	CNXK_REP_MSG_END,
} cnxk_rep_msg_t;

typedef enum CNXK_NACK_CODE {
	CNXK_REP_CTRL_MSG_NACK_INV_RDY_DATA = 0x501,
	CNXK_REP_CTRL_MSG_NACK_INV_REP_CNT = 0x502,
	CNXK_REP_CTRL_MSG_NACK_REP_STAT_UP_FAIL = 0x503,
} cnxk_nack_code_t;

/* Types */
typedef struct cnxk_type_data {
	cnxk_type_t type;
	uint32_t length;
	uint64_t data[];
} __rte_packed cnxk_type_data_t;

/* Header */
typedef struct cnxk_header {
	uint64_t signature;
	uint16_t nb_hops;
} __rte_packed cnxk_header_t;

/* Message meta */
typedef struct cnxk_rep_msg_data {
	cnxk_rep_msg_t type;
	uint32_t length;
	uint64_t data[];
} __rte_packed cnxk_rep_msg_data_t;

/* Ack msg */
typedef struct cnxk_rep_msg_ack_data {
	cnxk_rep_msg_t type;
	uint32_t size;
	union {
		void *data;
		uint64_t val;
		int64_t sval;
	} u;
} __rte_packed cnxk_rep_msg_ack_data_t;

/* Ack msg */
typedef struct cnxk_rep_msg_ack_data1 {
	cnxk_rep_msg_t type;
	uint32_t size;
	uint64_t data[];
} __rte_packed cnxk_rep_msg_ack_data1_t;

/* Ready msg */
typedef struct cnxk_rep_msg_ready_data {
	uint8_t val;
	uint16_t nb_ports;
	uint16_t data[];
} __rte_packed cnxk_rep_msg_ready_data_t;

/* Exit msg */
typedef struct cnxk_rep_msg_exit_data {
	uint8_t val;
	uint16_t nb_ports;
	uint16_t data[];
} __rte_packed cnxk_rep_msg_exit_data_t;

/* Ethernet op - set mac */
typedef struct cnxk_rep_msg_eth_mac_set_meta {
	uint16_t portid;
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
} __rte_packed cnxk_rep_msg_eth_set_mac_meta_t;

/* Ethernet op - get/clear stats */
typedef struct cnxk_rep_msg_eth_stats_meta {
	uint16_t portid;
} __rte_packed cnxk_rep_msg_eth_stats_meta_t;

void cnxk_rep_msg_populate_command(void *buffer, uint32_t *length, cnxk_rep_msg_t type,
				   uint32_t size);
void cnxk_rep_msg_populate_command_meta(void *buffer, uint32_t *length, void *msg_meta, uint32_t sz,
					cnxk_rep_msg_t msg);
void cnxk_rep_msg_populate_msg_end(void *buffer, uint32_t *length);
void cnxk_rep_msg_populate_type(void *buffer, uint32_t *length, cnxk_type_t type, uint32_t sz);
void cnxk_rep_msg_populate_header(void *buffer, uint32_t *length);
int cnxk_rep_msg_send_process(struct cnxk_rep_dev *rep_dev, void *buffer, uint32_t len,
			      cnxk_rep_msg_ack_data_t *adata);
int cnxk_rep_msg_control_thread_launch(struct cnxk_eswitch_dev *eswitch_dev);

#endif /* __CNXK_REP_MSG_H__ */
