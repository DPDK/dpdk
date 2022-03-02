/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_GLOBAL_H_
#define _ARK_GLOBAL_H_

#include <time.h>
#include <assert.h>

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_version.h>

#include "ark_pktdir.h"
#include "ark_pktgen.h"
#include "ark_pktchkr.h"

#define ETH_ARK_ARG_MAXLEN	64
#define ARK_SYSCTRL_BASE  0x0
#define ARK_PKTGEN_BASE   0x10000
#define ARK_MPU_RX_BASE   0x20000
#define ARK_UDM_BASE      0x30000
#define ARK_MPU_TX_BASE   0x40000
#define ARK_DDM_BASE      0x60000
#define ARK_CMAC_BASE     0x80000
#define ARK_PKTDIR_BASE   0xa0000
#define ARK_PKTCHKR_BASE  0x90000
#define ARK_RCPACING_BASE 0xb0000
#define ARK_EXTERNAL_BASE 0x100000
#define ARK_MPU_QOFFSET   0x00100
#define ARK_MAX_PORTS     RTE_MAX_ETHPORTS

#define offset8(n)     n
#define offset16(n)   ((n) / 2)
#define offset32(n)   ((n) / 4)
#define offset64(n)   ((n) / 8)

/* Maximum length of arg list in bytes */
#define ARK_MAX_ARG_LEN 256

/*
 * Structure to store private data for each PF/VF instance.
 */
#define def_ptr(type, name) \
	union type {		   \
		uint64_t *t64;	   \
		uint32_t *t32;	   \
		uint16_t *t16;	   \
		uint8_t  *t8;	   \
		void     *v;	   \
	} name


/* Extension hooks for extraction and placement of user meta data
 * during RX an TX operations. These functions are the bridge
 * between the mbuf struct and the tuser fields on the AXIS
 * interfaces in the FPGA
 */
/* RX hook populates mbuf fields from user defined *meta up to 20 bytes */
typedef void (*rx_user_meta_hook_fn)(struct rte_mbuf *mbuf,
				     const uint32_t *meta,
				     void *ext_user_data);
/* TX hook populate *meta, with up to 20 bytes.  meta_cnt
 * returns the number of uint32_t words populated, 0 to 5
 */
typedef void (*tx_user_meta_hook_fn)(const struct rte_mbuf *mbuf,
				     uint32_t *meta, uint8_t *meta_cnt,
				     void *ext_user_data);

struct ark_user_ext {
	void *(*dev_init)(struct rte_eth_dev *, void *abar, int port_id);
	void (*dev_uninit)(struct rte_eth_dev *, void *);
	int (*dev_get_port_count)(struct rte_eth_dev *, void *);
	int (*dev_configure)(struct rte_eth_dev *, void *);
	int (*dev_start)(struct rte_eth_dev *, void *);
	void (*dev_stop)(struct rte_eth_dev *, void *);
	void (*dev_close)(struct rte_eth_dev *, void *);
	int (*link_update)(struct rte_eth_dev *, int wait_to_complete, void *);
	int (*dev_set_link_up)(struct rte_eth_dev *, void *);
	int (*dev_set_link_down)(struct rte_eth_dev *, void *);
	int (*stats_get)(struct rte_eth_dev *, struct rte_eth_stats *, void *);
	void (*stats_reset)(struct rte_eth_dev *, void *);
	void (*mac_addr_add)(struct rte_eth_dev *,
						  struct rte_ether_addr *,
						 uint32_t,
						 uint32_t,
						 void *);
	void (*mac_addr_remove)(struct rte_eth_dev *, uint32_t, void *);
	void (*mac_addr_set)(struct rte_eth_dev *, struct rte_ether_addr *,
			void *);
	int (*set_mtu)(struct rte_eth_dev *, uint16_t, void *);
	/* user meta, hook functions  */
	rx_user_meta_hook_fn rx_user_meta_hook;
	tx_user_meta_hook_fn tx_user_meta_hook;
};

struct ark_adapter {
	/* User extension private data */
	void *user_data[ARK_MAX_PORTS];

	/* Pointers to packet generator and checker */
	int start_pg;
	uint16_t pg_running;
	ark_pkt_gen_t pg;
	ark_pkt_chkr_t pc;
	ark_pkt_dir_t pd;

	int num_ports;

	/* Packet generator/checker args */
	char pkt_gen_args[ARK_MAX_ARG_LEN];
	char pkt_chkr_args[ARK_MAX_ARG_LEN];
	uint32_t pkt_dir_v;

	/* eth device */
	struct rte_eth_dev *eth_dev;

	void *d_handle;
	struct ark_user_ext user_ext;

	/* Our Bar 0 */
	uint8_t *bar0;

	/* Application Bar */
	uint8_t *a_bar;

	/* Arkville demo block offsets */
	def_ptr(sys_ctrl, sysctrl);
	def_ptr(pkt_gen, pktgen);
	def_ptr(mpu_rx, mpurx);
	def_ptr(UDM, udm);
	def_ptr(mpu_tx, mputx);
	def_ptr(DDM, ddm);
	def_ptr(CMAC, cmac);
	def_ptr(external, external);
	def_ptr(pkt_dir, pktdir);
	def_ptr(pkt_chkr, pktchkr);

	int started;
	uint16_t rx_queues;
	uint16_t tx_queues;

	struct ark_rqpace_t *rqpacing;
};

typedef uint32_t *ark_t;

#endif
