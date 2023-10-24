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
 *	    pipes with high demand for that traffic class pipes is
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
 *	    queues within same pipe lowest priority traffic class (best-effort).
 */

#include <rte_mbuf.h>
#include <rte_meter.h>

/** Congestion Management */
#include "rte_red.h"
#include "rte_pie.h"

/** Maximum number of queues per pipe.
 * Note that the multiple queues (power of 2) can only be assigned to
 * lowest priority (best-effort) traffic class. Other higher priority traffic
 * classes can only have one queue.
 * Can not change.
 *
 * @see struct rte_sched_port_params
 */
#define RTE_SCHED_QUEUES_PER_PIPE    16

/** Number of WRR queues for best-effort traffic class per pipe.
 *
 * @see struct rte_sched_pipe_params
 */
#define RTE_SCHED_BE_QUEUES_PER_PIPE    4

/** Number of traffic classes per pipe (as well as subport).
 * @see struct rte_sched_subport_params
 * @see struct rte_sched_pipe_params
 */
#define RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE    \
(RTE_SCHED_QUEUES_PER_PIPE - RTE_SCHED_BE_QUEUES_PER_PIPE + 1)

/** Best-effort traffic class ID
 * Can not change.
 */
#define RTE_SCHED_TRAFFIC_CLASS_BE    (RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE - 1)

/*
 * Ethernet framing overhead. Overhead fields per Ethernet frame:
 * 1. Preamble:                             7 bytes;
 * 2. Start of Frame Delimiter (SFD):       1 byte;
 * 3. Frame Check Sequence (FCS):           4 bytes;
 * 4. Inter Frame Gap (IFG):               12 bytes.
 *
 * The FCS is considered overhead only if not included in the packet
 * length (field pkt_len of struct rte_mbuf).
 *
 * @see struct rte_sched_port_params
 */
#ifndef RTE_SCHED_FRAME_OVERHEAD_DEFAULT
#define RTE_SCHED_FRAME_OVERHEAD_DEFAULT      24
#endif

/**
 * Congestion Management (CMAN) mode
 *
 * This is used for controlling the admission of packets into a packet queue or
 * group of packet queues on congestion.
 *
 * The *Random Early Detection (RED)* algorithm works by proactively dropping
 * more and more input packets as the queue occupancy builds up. When the queue
 * is full or almost full, RED effectively works as *tail drop*. The *Weighted
 * RED* algorithm uses a separate set of RED thresholds for each packet color.
 *
 * Similar to RED, Proportional Integral Controller Enhanced (PIE) randomly
 * drops a packet at the onset of the congestion and tries to control the
 * latency around the target value. The congestion detection, however, is based
 * on the queueing latency instead of the queue length like RED. For more
 * information, refer RFC8033.
 */
enum rte_sched_cman_mode {
	RTE_SCHED_CMAN_RED, /**< Random Early Detection (RED) */
	RTE_SCHED_CMAN_PIE, /**< Proportional Integral Controller Enhanced (PIE) */
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
	/** Token bucket rate (measured in bytes per second) */
	uint64_t tb_rate;

	/** Token bucket size (measured in credits) */
	uint64_t tb_size;

	/** Traffic class rates (measured in bytes per second) */
	uint64_t tc_rate[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Enforcement period (measured in milliseconds) */
	uint64_t tc_period;

	/** Best-effort traffic class oversubscription weight */
	uint8_t tc_ov_weight;

	/** WRR weights of best-effort traffic class queues */
	uint8_t wrr_weights[RTE_SCHED_BE_QUEUES_PER_PIPE];
};

/*
 * Congestion Management configuration parameters.
 */
struct rte_sched_cman_params {
	/** Congestion Management mode */
	enum rte_sched_cman_mode cman_mode;

	union {
		/** RED parameters */
		struct rte_red_params red_params[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE][RTE_COLORS];

		/** PIE parameters */
		struct rte_pie_params pie_params[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	};
};

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
	/** Number of subport pipes.
	 * The subport can enable/allocate fewer pipes than the maximum
	 * number set through struct port_params::n_max_pipes_per_subport,
	 * as needed, to avoid memory allocation for the queues of the
	 * pipes that are not really needed.
	 */
	uint32_t n_pipes_per_subport_enabled;

	/** Packet queue size for each traffic class.
	 * All the pipes within the same subport share the similar
	 * configuration for the queues.
	 */
	uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Pipe profile table.
	 * Every pipe is configured using one of the profiles from this table.
	 */
	struct rte_sched_pipe_params *pipe_profiles;

	/** Profiles in the pipe profile table */
	uint32_t n_pipe_profiles;

	/** Max allowed profiles in the pipe profile table */
	uint32_t n_max_pipe_profiles;

	/** Congestion Management parameters
	 * If NULL the congestion management is disabled for the subport,
	 * otherwise proper parameters need to be provided.
	 */
	struct rte_sched_cman_params *cman_params;
};

struct rte_sched_subport_profile_params {
	/** Token bucket rate (measured in bytes per second) */
	uint64_t tb_rate;

	/** Token bucket size (measured in credits) */
	uint64_t tb_size;

	/** Traffic class rates (measured in bytes per second) */
	uint64_t tc_rate[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Enforcement period for rates (measured in milliseconds) */
	uint64_t tc_period;
};

/** Subport statistics */
struct rte_sched_subport_stats {
	/** Number of packets successfully written */
	uint64_t n_pkts_tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Number of packets dropped */
	uint64_t n_pkts_tc_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Number of bytes successfully written for each traffic class */
	uint64_t n_bytes_tc[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Number of bytes dropped for each traffic class */
	uint64_t n_bytes_tc_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];

	/** Number of packets dropped by congestion management scheme */
	uint64_t n_pkts_cman_dropped[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
};

/** Queue statistics */
struct rte_sched_queue_stats {
	/** Packets successfully written */
	uint64_t n_pkts;

	/** Packets dropped */
	uint64_t n_pkts_dropped;

	/** Packets dropped by congestion management scheme */
	uint64_t n_pkts_cman_dropped;

	/** Bytes successfully written */
	uint64_t n_bytes;

	/** Bytes dropped */
	uint64_t n_bytes_dropped;
};

/** Port configuration parameters. */
struct rte_sched_port_params {
	/** Name of the port to be associated */
	const char *name;

	/** CPU socket ID */
	int socket;

	/** Output port rate (measured in bytes per second) */
	uint64_t rate;

	/** Maximum Ethernet frame size (measured in bytes).
	 * Should not include the framing overhead.
	 */
	uint32_t mtu;

	/** Framing overhead per packet (measured in bytes) */
	uint32_t frame_overhead;

	/** Number of subports */
	uint32_t n_subports_per_port;

	/** subport profile table.
	 * Every pipe is configured using one of the profiles from this table.
	 */
	struct rte_sched_subport_profile_params *subport_profiles;

	/** Profiles in the pipe profile table */
	uint32_t n_subport_profiles;

	/** Max allowed profiles in the pipe profile table */
	uint32_t n_max_subport_profiles;

	/** Maximum number of subport pipes.
	 * This parameter is used to reserve a fixed number of bits
	 * in struct rte_mbuf::sched.queue_id for the pipe_id for all
	 * the subports of the same port.
	 */
	uint32_t n_pipes_per_subport;
};

/*
 * Configuration
 */

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
 *   Handle to port scheduler instance.
 *   If port is NULL, no operation is performed.
 */
void
rte_sched_port_free(struct rte_sched_port *port);

/**
 * Hierarchical scheduler pipe profile add
 *
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param params
 *   Pipe profile parameters
 * @param pipe_profile_id
 *   Set to valid profile id when profile is added successfully.
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_subport_pipe_profile_add(struct rte_sched_port *port,
	uint32_t subport_id,
	struct rte_sched_pipe_params *params,
	uint32_t *pipe_profile_id);

/**
 * Hierarchical scheduler subport bandwidth profile add
 * Note that this function is safe to use in runtime for adding new
 * subport bandwidth profile as it doesn't have any impact on hierarchical
 * structure of the scheduler.
 * @param port
 *   Handle to port scheduler instance
 * @param profile
 *   Subport bandwidth profile
 * @param subport_profile_id
 *   Subport profile id
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_port_subport_profile_add(struct rte_sched_port *port,
	struct rte_sched_subport_profile_params *profile,
	uint32_t *subport_profile_id);

/**
 * Hierarchical scheduler subport configuration
 * Note that this function is safe to use at runtime
 * to configure subport bandwidth profile.
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param params
 *   Subport configuration parameters. Must be non-NULL
 *   for first invocation (i.e initialization) for a given
 *   subport. Ignored (recommended value is NULL) for all
 *   subsequent invocation on the same subport.
 * @param subport_profile_id
 *   ID of subport bandwidth profile
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_subport_config(struct rte_sched_port *port,
	uint32_t subport_id,
	struct rte_sched_subport_params *params,
	uint32_t subport_profile_id);

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
 *   ID of subport-level pre-configured pipe profile
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
 * @param port_params
 *   Port scheduler configuration parameter structure
 * @param subport_params
 *   Array of subport parameter structures
 * @return
 *   Memory footprint size in bytes upon success, 0 otherwise
 */
uint32_t
rte_sched_port_get_memory_footprint(struct rte_sched_port_params *port_params,
	struct rte_sched_subport_params **subport_params);

/*
 * Statistics
 */

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
 *   Pointer to pre-allocated RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE-entry array
 *   where the oversubscription status for each of the subport traffic classes
 *   should be stored.
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
 * @param port
 *   Handle to port scheduler instance
 * @param pkt
 *   Packet descriptor handle
 * @param subport
 *   Subport ID
 * @param pipe
 *   Pipe ID within subport
 * @param traffic_class
 *   Traffic class ID within pipe (0 .. RTE_SCHED_TRAFFIC_CLASS_BE)
 * @param queue
 *   Queue ID within pipe traffic class, 0 for high priority TCs, and
 *   0 .. (RTE_SCHED_BE_QUEUES_PER_PIPE - 1) for best-effort TC
 * @param color
 *   Packet color set
 */
void
rte_sched_port_pkt_write(struct rte_sched_port *port,
			 struct rte_mbuf *pkt,
			 uint32_t subport, uint32_t pipe, uint32_t traffic_class,
			 uint32_t queue, enum rte_color color);

/**
 * Scheduler hierarchy path read from packet descriptor (struct
 * rte_mbuf). Typically called as part of the hierarchical scheduler
 * enqueue operation. The subport, pipe, traffic class and queue
 * parameters need to be pre-allocated by the caller.
 *
 * @param port
 *   Handle to port scheduler instance
 * @param pkt
 *   Packet descriptor handle
 * @param subport
 *   Subport ID
 * @param pipe
 *   Pipe ID within subport
 * @param traffic_class
 *   Traffic class ID within pipe (0 .. RTE_SCHED_TRAFFIC_CLASS_BE)
 * @param queue
 *   Queue ID within pipe traffic class, 0 for high priority TCs, and
 *   0 .. (RTE_SCHED_BE_QUEUES_PER_PIPE - 1) for best-effort TC
 */
void
rte_sched_port_pkt_read_tree_path(struct rte_sched_port *port,
				  const struct rte_mbuf *pkt,
				  uint32_t *subport, uint32_t *pipe,
				  uint32_t *traffic_class, uint32_t *queue);

enum rte_color
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

/**
 * Hierarchical scheduler subport traffic class
 * oversubscription enable/disable.
 * This function should be called at the time of subport initialization.
 *
 * @param port
 *   Handle to port scheduler instance
 * @param subport_id
 *   Subport ID
 * @param tc_ov_enable
 *  Boolean flag to enable/disable TC OV
 * @return
 *   0 upon success, error code otherwise
 */
int
rte_sched_subport_tc_ov_config(struct rte_sched_port *port, uint32_t subport_id, bool tc_ov_enable);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SCHED_H__ */
