/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _EVENT_HELPER_H_
#define _EVENT_HELPER_H_

#include <rte_log.h>

#define RTE_LOGTYPE_EH  RTE_LOGTYPE_USER4

#define EH_LOG_ERR(...) \
	RTE_LOG(ERR, EH, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__ ,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__ ,)))

#define EH_LOG_INFO(...) \
	RTE_LOG(INFO, EH, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__ ,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__ ,)))

/* Max event devices supported */
#define EVENT_MODE_MAX_EVENT_DEVS RTE_EVENT_MAX_DEVS

/* Max Rx adapters supported */
#define EVENT_MODE_MAX_RX_ADAPTERS RTE_EVENT_MAX_DEVS

/* Max Tx adapters supported */
#define EVENT_MODE_MAX_TX_ADAPTERS RTE_EVENT_MAX_DEVS

/* Max Rx adapter connections */
#define EVENT_MODE_MAX_CONNECTIONS_PER_ADAPTER 16

/* Max Tx adapter connections */
#define EVENT_MODE_MAX_CONNECTIONS_PER_TX_ADAPTER 16

/* Max event queues supported per event device */
#define EVENT_MODE_MAX_EVENT_QUEUES_PER_DEV RTE_EVENT_MAX_QUEUES_PER_DEV

/* Max event-lcore links */
#define EVENT_MODE_MAX_LCORE_LINKS \
	(EVENT_MODE_MAX_EVENT_DEVS * EVENT_MODE_MAX_EVENT_QUEUES_PER_DEV)

/* Max adapters that one Rx core can handle */
#define EVENT_MODE_MAX_ADAPTERS_PER_RX_CORE EVENT_MODE_MAX_RX_ADAPTERS

/* Max adapters that one Tx core can handle */
#define EVENT_MODE_MAX_ADAPTERS_PER_TX_CORE EVENT_MODE_MAX_TX_ADAPTERS

/* Used to indicate that queue schedule type is not set */
#define SCHED_TYPE_NOT_SET	3

/**
 * Packet transfer mode of the application
 */
enum eh_pkt_transfer_mode {
	EH_PKT_TRANSFER_MODE_POLL = 0,
	EH_PKT_TRANSFER_MODE_EVENT,
};

/**
 * Event mode packet rx types
 */
enum eh_rx_types {
	EH_RX_TYPE_NON_BURST = 0,
	EH_RX_TYPE_BURST
};

/**
 * Event mode packet tx types
 */
enum eh_tx_types {
	EH_TX_TYPE_INTERNAL_PORT = 0,
	EH_TX_TYPE_NO_INTERNAL_PORT
};

/**
 * Event mode ipsec mode types
 */
enum eh_ipsec_mode_types {
	EH_IPSEC_MODE_TYPE_APP = 0,
	EH_IPSEC_MODE_TYPE_DRIVER
};

/* Event dev params */
struct eventdev_params {
	uint8_t eventdev_id;
	uint8_t nb_eventqueue;
	uint8_t nb_eventport;
	uint8_t ev_queue_mode;
	uint8_t all_internal_ports;
};

/**
 * Event-lcore link configuration
 */
struct eh_event_link_info {
	uint8_t eventdev_id;
		/**< Event device ID */
	uint8_t event_port_id;
		/**< Event port ID */
	uint8_t eventq_id;
		/**< Event queue to be linked to the port */
	uint8_t lcore_id;
		/**< Lcore to be polling on this port */
};

/* Rx adapter connection info */
struct rx_adapter_connection_info {
	uint8_t ethdev_id;
	uint8_t eventq_id;
	int32_t ethdev_rx_qid;
};

/* Rx adapter conf */
struct rx_adapter_conf {
	int32_t eventdev_id;
	int32_t adapter_id;
	uint32_t rx_core_id;
	uint8_t nb_connections;
	struct rx_adapter_connection_info
			conn[EVENT_MODE_MAX_CONNECTIONS_PER_ADAPTER];
};

/* Tx adapter connection info */
struct tx_adapter_connection_info {
	uint8_t ethdev_id;
	int32_t ethdev_tx_qid;
};

/* Tx adapter conf */
struct tx_adapter_conf {
	int32_t eventdev_id;
	int32_t adapter_id;
	uint32_t tx_core_id;
	uint8_t nb_connections;
	struct tx_adapter_connection_info
			conn[EVENT_MODE_MAX_CONNECTIONS_PER_TX_ADAPTER];
	uint8_t tx_ev_queue;
};

/* Eventmode conf data */
struct eventmode_conf {
	int nb_eventdev;
		/**< No of event devs */
	struct eventdev_params eventdev_config[EVENT_MODE_MAX_EVENT_DEVS];
		/**< Per event dev conf */
	uint8_t nb_rx_adapter;
		/**< No of Rx adapters */
	struct rx_adapter_conf rx_adapter[EVENT_MODE_MAX_RX_ADAPTERS];
		/**< Rx adapter conf */
	uint8_t nb_tx_adapter;
		/**< No of Tx adapters */
	struct tx_adapter_conf tx_adapter[EVENT_MODE_MAX_TX_ADAPTERS];
		/** Tx adapter conf */
	uint8_t nb_link;
		/**< No of links */
	struct eh_event_link_info
		link[EVENT_MODE_MAX_LCORE_LINKS];
		/**< Per link conf */
	struct rte_bitmap *eth_core_mask;
		/**< Core mask of cores to be used for software Rx and Tx */
	uint32_t eth_portmask;
		/**< Mask of the eth ports to be used */
	union {
		RTE_STD_C11
		struct {
			uint64_t sched_type			: 2;
		/**< Schedule type */
			uint64_t all_ev_queue_to_ev_port	: 1;
		/**<
		 * When enabled, all event queues need to be mapped to
		 * each event port
		 */
			uint64_t event_vector                   : 1;
		/**<
		 * Enable event vector, when enabled application can
		 * receive vector of events.
		 */
			uint64_t vector_size                    : 16;
		};
		uint64_t u64;
	} ext_params;
		/**< 64 bit field to specify extended params */
	uint64_t vector_tmo_ns;
		/**< Max vector timeout in nanoseconds */
};

/**
 * Event helper configuration
 */
struct eh_conf {
	enum eh_pkt_transfer_mode mode;
		/**< Packet transfer mode of the application */
	uint32_t eth_portmask;
		/**<
		 * Mask of the eth ports to be used. This portmask would be
		 * checked while initializing devices using helper routines.
		 */
	void *mode_params;
		/**< Mode specific parameters */

		/** Application specific params */
	enum eh_ipsec_mode_types ipsec_mode;
		/**< Mode of ipsec run */
};

/* Workers registered by the application */
struct eh_app_worker_params {
	union {
		RTE_STD_C11
		struct {
			uint64_t burst : 1;
			/**< Specify status of rx type burst */
			uint64_t tx_internal_port : 1;
			/**< Specify whether tx internal port is available */
			uint64_t ipsec_mode : 1;
			/**< Specify ipsec processing level */
		};
		uint64_t u64;
	} cap;
			/**< Capabilities of this worker */
	void (*worker_thread)(struct eh_event_link_info *links,
			uint8_t nb_links);
			/**< Worker thread */
};

/**
 * Allocate memory for event helper configuration and initialize
 * it with default values.
 *
 * @return
 * - pointer to event helper configuration structure on success.
 * - NULL on failure.
 */
struct eh_conf *
eh_conf_init(void);

/**
 * Uninitialize event helper configuration and release its memory
. *
 * @param conf
 *   Event helper configuration
 */
void
eh_conf_uninit(struct eh_conf *conf);

/**
 * Initialize event mode devices
 *
 * Application can call this function to get the event devices, eth devices
 * and eth rx & tx adapters initialized according to the default config or
 * config populated using the command line args.
 *
 * Application is expected to initialize the eth devices and then the event
 * mode helper subsystem will stop & start eth devices according to its
 * requirement. Call to this function should be done after the eth devices
 * are successfully initialized.
 *
 * @param conf
 *   Event helper configuration
 * @return
 *  - 0 on success.
 *  - (<0) on failure.
 */
int32_t
eh_devs_init(struct eh_conf *conf);

/**
 * Release event mode devices
 *
 * Application can call this function to release event devices,
 * eth rx & tx adapters according to the config.
 *
 * Call to this function should be done before application stops
 * and closes eth devices. This function will not close and stop
 * eth devices.
 *
 * @param conf
 *   Event helper configuration
 * @return
 *  - 0 on success.
 *  - (<0) on failure.
 */
int32_t
eh_devs_uninit(struct eh_conf *conf);

/**
 * Get eventdev tx queue
 *
 * If the application uses event device which does not support internal port
 * then it needs to submit the events to a Tx queue before final transmission.
 * This Tx queue will be created internally by the eventmode helper subsystem,
 * and application will need its queue ID when it runs the execution loop.
 *
 * @param mode_conf
 *   Event helper configuration
 * @param eventdev_id
 *   Event device ID
 * @return
 *   Tx queue ID
 */
uint8_t
eh_get_tx_queue(struct eh_conf *conf, uint8_t eventdev_id);

/**
 * Display event mode configuration
 *
 * @param conf
 *   Event helper configuration
 */
void
eh_display_conf(struct eh_conf *conf);


/**
 * Launch eventmode worker
 *
 * The application can request the eventmode helper subsystem to launch the
 * worker based on the capabilities of event device and the options selected
 * while initializing the eventmode.
 *
 * @param conf
 *   Event helper configuration
 * @param app_wrkr
 *   List of all the workers registered by application, along with its
 *   capabilities
 * @param nb_wrkr_param
 *   Number of workers passed by the application
 *
 */
void
eh_launch_worker(struct eh_conf *conf, struct eh_app_worker_params *app_wrkr,
		uint8_t nb_wrkr_param);

#endif /* _EVENT_HELPER_H_ */
