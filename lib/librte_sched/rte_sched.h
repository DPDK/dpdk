/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_SCHED_H__
#define __INCLUDE_RTE_SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Hierarchical Scheduler
 *
 * The hierarchical scheduler prioritizes the transmission of packets
 * from different users and traffic classes according to the Service
 * Level Agreements (SLAs) defined for the current network node.
 *
 * The scheduler supports thousands of packet queues grouped under a
 * 5-level hierarchy:
 *     1. Port:
 *           - Typical usage: output Ethernet port;
 *           - Multiple ports are scheduled in round robin order with
 *	    equal priority;
 *     2. Subport:
 *           - Typical usage: group of users;
 *           - Traffic shaping using the token bucket algorithm
 *	    (one bucket per subport);
 *           - Upper limit enforced per traffic class at subport level;
 *           - Lower priority traffic classes able to reuse subport
 *	    bandwidth currently unused by higher priority traffic
 *	    classes of the same subport;
 *           - When any subport traffic class is oversubscribed
 *	    (configuration time event), the usage of subport member
 *	    pipes with high demand for thattraffic class pipes is
 *	    truncated to a dynamically adjusted value with no
 *             impact to low demand pipes;
 *     3. Pipe:
 *           - Typical usage: individual user/subscriber;
 *           - Traffic shaping using the token bucket algorithm
 *	    (one bucket per pipe);
 *     4. Traffic class:
 *           - Traffic classes of the same pipe handled in strict
 *	    priority order;
 *           - Upper limit enforced per traffic class at the pipe level;
 *           - Lower priority traffic classes able to reuse pipe
 *	    bandwidth currently unused by higher priority traffic
 *	    classes of the same pipe;
 *     5. Queue:
 *           - Typical usage: queue hosting packets from one or
 *	    multiple connections of same traffic class belonging to
 *	    the same user;
 *           - Weighted Round Robin (WRR) is used to service the
 *	    queues within same pipe traffic class.
 *
 */

#include <sys/types.h>
#include <rte_compat.h>
#include <rte_mbuf.h>
#include <rte_meter.h>

/** Random Early Detection (RED) */
#ifdef RTE_SCHED_RED
#include "rte_red.h"
#endif

/** Number of traffic classes per pipe (as well as subport).
 * Cannot be changed.
 */
#define RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE    4

/** Number of queues per pipe traffic class. Cannot be changed. */
#define RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS    4

/** Number of queues per pipe. */
#define RTE_SCHED_QUEUES_PER_PIPE             \
	(RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE *     \
	RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS)

/** Maximum number of pipe profiles that can be defined per port.
 * Compile-time configurable.
 */
#ifndef RTE_SCHED_PIPE_PROFILES_PER_PORT
#define RTE_SCHED_PIPE_PROFILES_PER_PORT      256
#endif

/*
 * Ethernet framing overhead. Overhead fields per Ethernet frame:
 * 1. Preamble:                             7 bytes;
 * 2. Start of Frame Delimiter (SFD):       1 byte;
 * 3. Frame Check Sequence (FCS):           4 bytes;
 * 4. Inter Frame Gap (IFG):               12 bytes.
 *
 * The FCS is considered overhead only if not included in the packet
 * length (field pkt_len of struct rte_mbuf).
 */
#ifndef RTE_SCHED_FRAME_OVERHEAD_DEFAULT
#define RTE_SCHED_FRAME_OVERHEAD_DEFAULT      24
#endif

/*
 * Subport configuration parameters. The period and credits_per_period
 * parameters are measured in bytes, with one byte meaning the time
 * duration associated with the transmission of one byte on the
 * physical medium of the output port, with pipe or pipe traffic class
 * rate (measured as percentage of output port rate) determined as
 * credits_per_period divided by period. One credit represents one
 * byte.
 */
struct rte_sched_subport_params {
	/* Subport token bucket */
	uint32_t tb_rate;                /**< Rate (measured in bytes per second) */
	uint32_t tb_size;                /**< Size (measured in credits) */

	/* Subport traffic classes */
	uint32_t tc_rate[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Traffic class rates (measured in bytes per second) */
	uint32_t tc_period;
	/**< Enforcement period for rates (measured in milliseconds) */
};

/** Subport statistics */
struct rte_sched_subport_stats {
	/* Packets */
	uint32_t n_pkts_tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Number of packets successfully written */
	uint32_t n_pkts_tc_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Number of packets dropped */

	/* Bytes */
	uint32_t n_bytes_tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Number of bytes successfully written for each traffic class */
	uint32_t n_bytes_tc_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Number of bytes dropped for each traffic class */

#ifdef RTE_SCHED_RED
	uint32_t n_pkts_red_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Number of packets dropped by red */
#endif
};

/*
 * Pipe configuration parameters. The period and credits_per_period
 * parameters are measured in bytes, with one byte meaning the time
 * duration associated with the transmission of one byte on the
 * physical medium of the output port, with pipe or pipe traffic class
 * rate (measured as percentage of output port rate) determined as
 * credits_per_period divided by period. One credit represents one
 * byte.
 */
struct rte_sched_pipe_params {
	/* Pipe token bucket */
	uint32_t tb_rate;                /**< Rate (measured in bytes per second) */
	uint32_t tb_size;                /**< Size (measured in credits) */

	/* Pipe traffic classes */
	uint32_t tc_rate[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Traffic class rates (measured in bytes per second) */
	uint32_t tc_period;
	/**< Enforcement period (measured in milliseconds) */
#ifdef RTE_SCHED_SUBPORT_TC_OV
	uint8_t tc_ov_weight;		 /**< Weight Traffic class 3 oversubscription */
#endif

	/* Pipe queues */
	uint8_t  wrr_weights[RTE_SCHED_QUEUES_PER_PIPE]; /**< WRR weights */
};

/** Queue statistics */
struct rte_sched_queue_stats {
	/* Packets */
	uint32_t n_pkts;                 /**< Packets successfully written */
	uint32_t n_pkts_dropped;         /**< Packets dropped */
#ifdef RTE_SCHED_RED
	uint32_t n_pkts_red_dropped;	 /**< Packets dropped by RED */
#endif

	/* Bytes */
	uint32_t n_bytes;                /**< Bytes successfully written */
	uint32_t n_bytes_dropped;        /**< Bytes dropped */
};

/** Port configuration parameters. */
struct rte_sched_port_params {
	const char *name;                /**< String to be associated */
	int socket;                      /**< CPU socket ID */
	uint32_t rate;                   /**< Output port rate
					  * (measured in bytes per second) */
	uint32_t mtu;                    /**< Maximum Ethernet frame size
					  * (measured in bytes).
					  * Should not include the framing overhead. */
	uint32_t frame_overhead;         /**< Framing overhead per packet
					  * (measured in bytes) */
	uint32_t n_subports_per_port;    /**< Number of subports */
	uint32_t n_pipes_per_subport;    /**< Number of pipes per subport */
	uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	/**< Packet queue size for each traffic class.
	 * All queues within the same pipe traffic class have the same
	 * size. Queues from different pipes serving the same traffic
	 * class have the same size. */
	struct rte_sched_pipe_params *pipe_profiles;
	/**< Pipe profile table.
	 * Every pipe is configured using one of the profiles from this table. */
	uint32_t n_pipe_profiles;        /**< Profiles in the pipe profile table */
#ifdef RTE_SCHED_RED
	struct rte_red_params red_params[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE][e_RTE_METER_COLORS]; /**< RED parameters */
#endif
};

/*
 * Configuration
 *
 ***/

/**
 * Hierarchical scheduler port configuration
 *
 * @param params
 *   Port scheduler configuration parameter structure
 * @return
 *   Handle to port scheduler instance upon success or NULL otherwise.
 */
struct rte_sched_port *
rte_sched_port_config(struct rte_sched_port_params *params);

/**
 * Hierarchical scheduler port free
 *
 * @param port
 *   Handle to port scheduler instance
 */
void
rte_sched_port_free(struct rte_sched_port *port);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Hierarchical scheduler pipe profile add
 *
 * @param port
 *   Handle to port scheduler instance
 * @param params
 *   Pipe profile parameters
 * @param pipe_profile_id
 *   Set to valid profile id when profile is added successfully.
 * @return
 *   0 upon success, error code otherwise
 */
int __rte_experimental
rte_sched_port_pipe_profile_add(struct rte_sched_port *port,
	struct rte_sched_pipe_params *params,
	uint32_t *pipe_profile_id);

/**
 * Hierarchical scheduler subport configuration
 *
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param params
 *   Subport configuration parameters
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_subport_config(struct rte_sched_port *port,
	uint32_t subport_id,
	struct rte_sched_subport_params *params);

/**
 * Hierarchical scheduler pipe configuration
 *
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param pipe_id
 *   Pipe ID within subport
 * @param pipe_profile
 *   ID of port-level pre-configured pipe profile
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_pipe_config(struct rte_sched_port *port,
	uint32_t subport_id,
	uint32_t pipe_id,
	int32_t pipe_profile);

/**
 * Hierarchical scheduler memory footprint size per port
 *
 * @param params
 *   Port scheduler configuration parameter structure
 * @return
 *   Memory footprint size in bytes upon success, 0 otherwise
 */
uint32_t
rte_sched_port_get_memory_footprint(struct rte_sched_port_params *params);

/*
 * Statistics
 *
 ***/

/**
 * Hierarchical scheduler subport statistics read
 *
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param stats
 *   Pointer to pre-allocated subport statistics structure where the statistics
 *   counters should be stored
 * @param tc_ov
 *   Pointer to pre-allocated 4-entry array where the oversubscription status for
 *   each of the 4 subport traffic classes should be stored.
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_subport_read_stats(struct rte_sched_port *port,
	uint32_t subport_id,
	struct rte_sched_subport_stats *stats,
	uint32_t *tc_ov);

/**
 * Hierarchical scheduler queue statistics read
 *
 * @param port
 *   Handle to port scheduler instance
 * @param queue_id
 *   Queue ID within port scheduler
 * @param stats
 *   Pointer to pre-allocated subport statistics structure where the statistics
 *   counters should be stored
 * @param qlen
 *   Pointer to pre-allocated variable where the current queue length
 *   should be stored.
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_queue_read_stats(struct rte_sched_port *port,
	uint32_t queue_id,
	struct rte_sched_queue_stats *stats,
	uint16_t *qlen);

/**
 * Scheduler hierarchy path write to packet descriptor. Typically
 * called by the packet classification stage.
 *
 * @param pkt
 *   Packet descriptor handle
 * @param subport
 *   Subport ID
 * @param pipe
 *   Pipe ID within subport
 * @param traffic_class
 *   Traffic class ID within pipe (0 .. 3)
 * @param queue
 *   Queue ID within pipe traffic class (0 .. 3)
 * @param color
 *   Packet color set
 */
void
rte_sched_port_pkt_write(struct rte_mbuf *pkt,
			 uint32_t subport, uint32_t pipe, uint32_t traffic_class,
			 uint32_t queue, enum rte_meter_color color);

/**
 * Scheduler hierarchy path read from packet descriptor (struct
 * rte_mbuf). Typically called as part of the hierarchical scheduler
 * enqueue operation. The subport, pipe, traffic class and queue
 * parameters need to be pre-allocated by the caller.
 *
 * @param pkt
 *   Packet descriptor handle
 * @param subport
 *   Subport ID
 * @param pipe
 *   Pipe ID within subport
 * @param traffic_class
 *   Traffic class ID within pipe (0 .. 3)
 * @param queue
 *   Queue ID within pipe traffic class (0 .. 3)
 *
 */
void
rte_sched_port_pkt_read_tree_path(const struct rte_mbuf *pkt,
				  uint32_t *subport, uint32_t *pipe,
				  uint32_t *traffic_class, uint32_t *queue);

enum rte_meter_color
rte_sched_port_pkt_read_color(const struct rte_mbuf *pkt);

/**
 * Hierarchical scheduler port enqueue. Writes up to n_pkts to port
 * scheduler and returns the number of packets actually written. For
 * each packet, the port scheduler queue to write the packet to is
 * identified by reading the hierarchy path from the packet
 * descriptor; if the queue is full or congested and the packet is not
 * written to the queue, then the packet is automatically dropped
 * without any action required from the caller.
 *
 * @param port
 *   Handle to port scheduler instance
 * @param pkts
 *   Array storing the packet descriptor handles
 * @param n_pkts
 *   Number of packets to enqueue from the pkts array into the port scheduler
 * @return
 *   Number of packets successfully enqueued
 */
int
rte_sched_port_enqueue(struct rte_sched_port *port, struct rte_mbuf **pkts, uint32_t n_pkts);

/**
 * Hierarchical scheduler port dequeue. Reads up to n_pkts from the
 * port scheduler and stores them in the pkts array and returns the
 * number of packets actually read.  The pkts array needs to be
 * pre-allocated by the caller with at least n_pkts entries.
 *
 * @param port
 *   Handle to port scheduler instance
 * @param pkts
 *   Pre-allocated packet descriptor array where the packets dequeued
 *   from the port
 *   scheduler should be stored
 * @param n_pkts
 *   Number of packets to dequeue from the port scheduler
 * @return
 *   Number of packets successfully dequeued and placed in the pkts array
 */
int
rte_sched_port_dequeue(struct rte_sched_port *port, struct rte_mbuf **pkts, uint32_t n_pkts);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SCHED_H__ */
