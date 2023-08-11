/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PIPELINE_H__
#define __INCLUDE_RTE_PIPELINE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Pipeline
 *
 * This tool is part of the DPDK Packet Framework tool suite and provides
 * a standard methodology (logically similar to OpenFlow) for rapid development
 * of complex packet processing pipelines out of ports, tables and actions.
 *
 * <B>Basic operation.</B> A pipeline is constructed by connecting its input
 * ports to its output ports through a chain of lookup tables. As result of
 * lookup operation into the current table, one of the table entries (or the
 * default table entry, in case of lookup miss) is identified to provide the
 * actions to be executed on the current packet and the associated action
 * meta-data. The behavior of user actions is defined through the configurable
 * table action handler, while the reserved actions define the next hop for the
 * current packet (either another table, an output port or packet drop) and are
 * handled transparently by the framework.
 *
 * <B>Initialization and run-time flows.</B> Once all the pipeline elements
 * (input ports, tables, output ports) have been created, input ports connected
 * to tables, table action handlers configured, tables populated with the
 * initial set of entries (actions and action meta-data) and input ports
 * enabled, the pipeline runs automatically, pushing packets from input ports
 * to tables and output ports. At each table, the identified user actions are
 * being executed, resulting in action meta-data (stored in the table entry)
 * and packet meta-data (stored with the packet descriptor) being updated. The
 * pipeline tables can have further updates and input ports can be disabled or
 * enabled later on as required.
 *
 * <B>Multi-core scaling.</B> Typically, each CPU core will run its own
 * pipeline instance. Complex application-level pipelines can be implemented by
 * interconnecting multiple CPU core-level pipelines in tree-like topologies,
 * as the same port devices (e.g. SW rings) can serve as output ports for the
 * pipeline running on CPU core A, as well as input ports for the pipeline
 * running on CPU core B. This approach enables the application development
 * using the pipeline (CPU cores connected serially), cluster/run-to-completion
 * (CPU cores connected in parallel) or mixed (pipeline of CPU core clusters)
 * programming models.
 *
 * <B>Thread safety.</B> It is possible to have multiple pipelines running on
 * the same CPU core, but it is not allowed (for thread safety reasons) to have
 * multiple CPU cores running the same pipeline instance.
 */

#include <stdint.h>

#include <rte_port.h>
#include <rte_table.h>
#include <rte_common.h>

struct rte_mbuf;

/*
 * Pipeline
 */
/** Opaque data type for pipeline */
struct rte_pipeline;

/** Parameters for pipeline creation  */
struct rte_pipeline_params {
	/** Pipeline name */
	const char *name;

	/** CPU socket ID where memory for the pipeline and its elements (ports
	and tables) should be allocated */
	int socket_id;

	/** Offset within packet meta-data to port_id to be used by action
	"Send packet to output port read from packet meta-data". Has to be
	4-byte aligned. */
	uint32_t offset_port_id;
};

/** Pipeline port in stats. */
struct rte_pipeline_port_in_stats {
	/** Port in stats. */
	struct rte_port_in_stats stats;

	/** Number of packets dropped by action handler. */
	uint64_t n_pkts_dropped_by_ah;

};

/** Pipeline port out stats. */
struct rte_pipeline_port_out_stats {
	/** Port out stats. */
	struct rte_port_out_stats stats;

	/** Number of packets dropped by action handler. */
	uint64_t n_pkts_dropped_by_ah;
};

/** Pipeline table stats. */
struct rte_pipeline_table_stats {
	/** Table stats. */
	struct rte_table_stats stats;

	/** Number of packets dropped by lookup hit action handler. */
	uint64_t n_pkts_dropped_by_lkp_hit_ah;

	/** Number of packets dropped by lookup miss action handler. */
	uint64_t n_pkts_dropped_by_lkp_miss_ah;

	/** Number of packets dropped by pipeline in behalf of this
	 * table based on action specified in table entry. */
	uint64_t n_pkts_dropped_lkp_hit;

	/** Number of packets dropped by pipeline in behalf of this
	 *  table based on action specified in table entry. */
	uint64_t n_pkts_dropped_lkp_miss;
};

/**
 * Pipeline create
 *
 * @param params
 *   Parameters for pipeline creation
 * @return
 *   Handle to pipeline instance on success or NULL otherwise
 */
struct rte_pipeline *rte_pipeline_create(struct rte_pipeline_params *params);

/**
 * Pipeline free
 *
 * @param p
 *   Handle to pipeline instance
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_free(struct rte_pipeline *p);

/**
 * Pipeline consistency check
 *
 * @param p
 *   Handle to pipeline instance
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_check(struct rte_pipeline *p);

/**
 * Pipeline run
 *
 * @param p
 *   Handle to pipeline instance
 * @return
 *   Number of packets read and processed
 */
int rte_pipeline_run(struct rte_pipeline *p);

/**
 * Pipeline flush
 *
 * @param p
 *   Handle to pipeline instance
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_flush(struct rte_pipeline *p);

/*
 * Actions
 */
/** Reserved actions */
enum rte_pipeline_action {
	/** Drop the packet */
	RTE_PIPELINE_ACTION_DROP = 0,

	/** Send packet to output port */
	RTE_PIPELINE_ACTION_PORT,

	/** Send packet to output port read from packet meta-data */
	RTE_PIPELINE_ACTION_PORT_META,

	/** Send packet to table */
	RTE_PIPELINE_ACTION_TABLE,

	/** Number of reserved actions */
	RTE_PIPELINE_ACTIONS
};

/*
 * Table
 */
/** Maximum number of tables allowed for any given pipeline instance. The
	value of this parameter cannot be changed. */
#define RTE_PIPELINE_TABLE_MAX                                     64

/**
 * Head format for the table entry of any pipeline table. For any given
 * pipeline table, all table entries should have the same size and format. For
 * any given pipeline table, the table entry has to start with a head of this
 * structure, which contains the reserved actions and their associated
 * meta-data, and then optionally continues with user actions and their
 * associated meta-data. As all the currently defined reserved actions are
 * mutually exclusive, only one reserved action can be set per table entry.
 */
struct rte_pipeline_table_entry {
	/** Reserved action */
	enum rte_pipeline_action action;

	union {
		/** Output port ID (meta-data for "Send packet to output port"
		action) */
		uint32_t port_id;
		/** Table ID (meta-data for "Send packet to table" action) */
		uint32_t table_id;
	};
	/** Start of table entry area for user defined actions and meta-data */
	__extension__ uint8_t action_data[0];
};

/**
 * Pipeline table action handler on lookup hit
 *
 * The action handler can decide to drop packets by resetting the associated
 * packet bit in the pkts_mask parameter. In this case, the action handler is
 * required not to free the packet buffer, which will be freed eventually by
 * the pipeline.
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param pkts_mask
 *   64-bit bitmask specifying which packets in the input burst are valid. When
 *   pkts_mask bit n is set, then element n of pkts array is pointing to a
 *   valid packet and element n of entries array is pointing to a valid table
 *   entry associated with the packet, with the association typically done by
 *   the table lookup operation. Otherwise, element n of pkts array and element
 *   n of entries array will not be accessed.
 * @param entries
 *   Set of table entries specified as array of up to 64 pointers to struct
 *   rte_pipeline_table_entry
 * @param arg
 *   Opaque parameter registered by the user at the pipeline table creation
 *   time
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_pipeline_table_action_handler_hit)(
	struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	struct rte_pipeline_table_entry **entries,
	void *arg);

/**
 * Pipeline table action handler on lookup miss
 *
 * The action handler can decide to drop packets by resetting the associated
 * packet bit in the pkts_mask parameter. In this case, the action handler is
 * required not to free the packet buffer, which will be freed eventually by
 * the pipeline.
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param pkts_mask
 *   64-bit bitmask specifying which packets in the input burst are valid. When
 *   pkts_mask bit n is set, then element n of pkts array is pointing to a
 *   valid packet. Otherwise, element n of pkts array will not be accessed.
 * @param entry
 *   Single table entry associated with all the valid packets from the input
 *   burst, specified as pointer to struct rte_pipeline_table_entry.
 *   This entry is the pipeline table default entry that is associated by the
 *   table lookup operation with the input packets that have resulted in lookup
 *   miss.
 * @param arg
 *   Opaque parameter registered by the user at the pipeline table creation
 *   time
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_pipeline_table_action_handler_miss)(
	struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	struct rte_pipeline_table_entry *entry,
	void *arg);

/** Parameters for pipeline table creation. Action handlers have to be either
    both enabled or both disabled (they can be disabled by setting them to
    NULL). */
struct rte_pipeline_table_params {
	/** Table operations (specific to each table type) */
	struct rte_table_ops *ops;
	/** Opaque param to be passed to the table create operation when
	invoked */
	void *arg_create;
	/** Callback function to execute the user actions on input packets in
	case of lookup hit */
	rte_pipeline_table_action_handler_hit f_action_hit;
	/** Callback function to execute the user actions on input packets in
	case of lookup miss */
	rte_pipeline_table_action_handler_miss f_action_miss;

	/** Opaque parameter to be passed to lookup hit and/or lookup miss
	action handlers when invoked */
	void *arg_ah;
	/** Memory size to be reserved per table entry for storing the user
	actions and their meta-data */
	uint32_t action_data_size;
};

/**
 * Pipeline table create
 *
 * @param p
 *   Handle to pipeline instance
 * @param params
 *   Parameters for pipeline table creation
 * @param table_id
 *   Table ID. Valid only within the scope of table IDs of the current
 *   pipeline. Only returned after a successful invocation.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_create(struct rte_pipeline *p,
	struct rte_pipeline_table_params *params,
	uint32_t *table_id);

/**
 * Pipeline table default entry add
 *
 * The contents of the table default entry is updated with the provided actions
 * and meta-data. When the default entry is not configured (by using this
 * function), the built-in default entry has the action "Drop" and meta-data
 * set to all-zeros.
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param default_entry
 *   New contents for the table default entry
 * @param default_entry_ptr
 *   On successful invocation, pointer to the default table entry which can be
 *   used for further read-write accesses to this table entry. This pointer
 *   is valid until the default entry is deleted or re-added.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_default_entry_add(struct rte_pipeline *p,
	uint32_t table_id,
	struct rte_pipeline_table_entry *default_entry,
	struct rte_pipeline_table_entry **default_entry_ptr);

/**
 * Pipeline table default entry delete
 *
 * The new contents of the table default entry is set to reserved action "Drop
 * the packet" with meta-data cleared (i.e. set to all-zeros).
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param entry
 *   On successful invocation, when entry points to a valid buffer, the
 *   previous contents of the table default entry (as it was just before the
 *   delete operation) is copied to this buffer
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_default_entry_delete(struct rte_pipeline *p,
	uint32_t table_id,
	struct rte_pipeline_table_entry *entry);

/**
 * Pipeline table entry add
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param key
 *   Table entry key
 * @param entry
 *   New contents for the table entry identified by key
 * @param key_found
 *   On successful invocation, set to TRUE (value different than 0) if key was
 *   already present in the table before the add operation and to FALSE (value
 *   0) if not
 * @param entry_ptr
 *   On successful invocation, pointer to the table entry associated with key.
 *   This can be used for further read-write accesses to this table entry and
 *   is valid until the key is deleted from the table or re-added (usually for
 *   associating different actions and/or action meta-data to the current key)
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_entry_add(struct rte_pipeline *p,
	uint32_t table_id,
	void *key,
	struct rte_pipeline_table_entry *entry,
	int *key_found,
	struct rte_pipeline_table_entry **entry_ptr);

/**
 * Pipeline table entry delete
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param key
 *   Table entry key
 * @param key_found
 *   On successful invocation, set to TRUE (value different than 0) if key was
 *   found in the table before the delete operation and to FALSE (value 0) if
 *   not
 * @param entry
 *   On successful invocation, when key is found in the table and entry points
 *   to a valid buffer, the table entry contents (as it was before the delete
 *   was performed) is copied to this buffer
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_entry_delete(struct rte_pipeline *p,
	uint32_t table_id,
	void *key,
	int *key_found,
	struct rte_pipeline_table_entry *entry);

/**
 * Pipeline table entry add bulk
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param keys
 *   Array containing table entry keys
 * @param entries
 *   Array containing new contents for every table entry identified by key
 * @param n_keys
 *   Number of keys to add
 * @param key_found
 *   On successful invocation, key_found for every item in the array is set to
 *   TRUE (value different than 0) if key was already present in the table
 *   before the add operation and to FALSE (value 0) if not
 * @param entries_ptr
 *   On successful invocation, array *entries_ptr stores pointer to every table
 *   entry associated with key. This can be used for further read-write accesses
 *   to this table entry and is valid until the key is deleted from the table or
 *   re-added (usually for associating different actions and/or action meta-data
 *   to the current key)
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_entry_add_bulk(struct rte_pipeline *p,
	uint32_t table_id,
	void **keys,
	struct rte_pipeline_table_entry **entries,
	uint32_t n_keys,
	int *key_found,
	struct rte_pipeline_table_entry **entries_ptr);

/**
 * Pipeline table entry delete bulk
 *
 * @param p
 *   Handle to pipeline instance
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @param keys
 *   Array containing table entry keys
 * @param n_keys
 *   Number of keys to delete
 * @param key_found
 *   On successful invocation, key_found for every item in the array is set to
 *   TRUE (value different than 0) if key was found in the table before the
 *   delete operation and to FALSE (value 0) if not
 * @param entries
 *   If entries pointer is NULL, this pointer is ignored for every entry found.
 *   Else, after successful invocation, if specific key is found in the table
 *   and entry points to a valid buffer, the table entry contents (as it was
 *   before the delete was performed) is copied to this buffer.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_entry_delete_bulk(struct rte_pipeline *p,
	uint32_t table_id,
	void **keys,
	uint32_t n_keys,
	int *key_found,
	struct rte_pipeline_table_entry **entries);

/**
 * Read pipeline table stats.
 *
 * This function reads table statistics identified by *table_id* of given
 * pipeline *p*.
 *
 * @param p
 *   Handle to pipeline instance.
 * @param table_id
 *   Port ID what stats will be returned.
 * @param stats
 *   Statistics buffer.
 * @param clear
 *   If not 0 clear stats after reading.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_table_stats_read(struct rte_pipeline *p, uint32_t table_id,
	struct rte_pipeline_table_stats *stats, int clear);

/*
 * Port IN
 */
/** Maximum number of input ports allowed for any given pipeline instance. The
	value of this parameter cannot be changed. */
#define RTE_PIPELINE_PORT_IN_MAX                                    64

/**
 * Pipeline input port action handler
 *
 * The action handler can decide to drop packets by resetting the associated
 * packet bit in the pkts_mask parameter. In this case, the action handler is
 * required not to free the packet buffer, which will be freed eventually by
 * the pipeline.
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param n
 *   Number of packets in the input burst. This parameter specifies that
 *   elements 0 to (n-1) of pkts array are valid.
 * @param arg
 *   Opaque parameter registered by the user at the pipeline table creation
 *   time
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_pipeline_port_in_action_handler)(
	struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint32_t n,
	void *arg);

/** Parameters for pipeline input port creation */
struct rte_pipeline_port_in_params {
	/** Input port operations (specific to each table type) */
	struct rte_port_in_ops *ops;
	/** Opaque parameter to be passed to create operation when invoked */
	void *arg_create;

	/** Callback function to execute the user actions on input packets.
		Disabled if set to NULL. */
	rte_pipeline_port_in_action_handler f_action;
	/** Opaque parameter to be passed to the action handler when invoked */
	void *arg_ah;

	/** Recommended burst size for the RX operation(in number of pkts) */
	uint32_t burst_size;
};

/**
 * Pipeline input port create
 *
 * @param p
 *   Handle to pipeline instance
 * @param params
 *   Parameters for pipeline input port creation
 * @param port_id
 *   Input port ID. Valid only within the scope of input port IDs of the
 *   current pipeline. Only returned after a successful invocation.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_in_create(struct rte_pipeline *p,
	struct rte_pipeline_port_in_params *params,
	uint32_t *port_id);

/**
 * Pipeline input port connect to table
 *
 * @param p
 *   Handle to pipeline instance
 * @param port_id
 *   Port ID (returned by previous invocation of pipeline input port create)
 * @param table_id
 *   Table ID (returned by previous invocation of pipeline table create)
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_in_connect_to_table(struct rte_pipeline *p,
	uint32_t port_id,
	uint32_t table_id);

/**
 * Pipeline input port enable
 *
 * @param p
 *   Handle to pipeline instance
 * @param port_id
 *   Port ID (returned by previous invocation of pipeline input port create)
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_in_enable(struct rte_pipeline *p,
	uint32_t port_id);

/**
 * Pipeline input port disable
 *
 * @param p
 *   Handle to pipeline instance
 * @param port_id
 *   Port ID (returned by previous invocation of pipeline input port create)
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_in_disable(struct rte_pipeline *p,
	uint32_t port_id);

/**
 * Read pipeline port in stats.
 *
 * This function reads port in statistics identified by *port_id* of given
 * pipeline *p*.
 *
 * @param p
 *   Handle to pipeline instance.
 * @param port_id
 *   Port ID what stats will be returned.
 * @param stats
 *   Statistics buffer.
 * @param clear
 *   If not 0 clear stats after reading.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_in_stats_read(struct rte_pipeline *p, uint32_t port_id,
	struct rte_pipeline_port_in_stats *stats, int clear);

/*
 * Port OUT
 */
/** Maximum number of output ports allowed for any given pipeline instance. The
	value of this parameter cannot be changed. */
#define RTE_PIPELINE_PORT_OUT_MAX                                   64

/**
 * Pipeline output port action handler
 *
 * The action handler can decide to drop packets by resetting the associated
 * packet bit in the pkts_mask parameter. In this case, the action handler is
 * required not to free the packet buffer, which will be freed eventually by
 * the pipeline.
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param pkts_mask
 *   64-bit bitmask specifying which packets in the input burst are valid. When
 *   pkts_mask bit n is set, then element n of pkts array is pointing to a
 *   valid packet. Otherwise, element n of pkts array will not be accessed.
 * @param arg
 *   Opaque parameter registered by the user at the pipeline table creation
 *   time
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_pipeline_port_out_action_handler)(
	struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	void *arg);

/** Parameters for pipeline output port creation. The action handlers have to
be either both enabled or both disabled (by setting them to NULL). When
enabled, the pipeline selects between them at different moments, based on the
number of packets that have to be sent to the same output port. */
struct rte_pipeline_port_out_params {
	/** Output port operations (specific to each table type) */
	struct rte_port_out_ops *ops;
	/** Opaque parameter to be passed to create operation when invoked */
	void *arg_create;

	/** Callback function executing the user actions on bust of input
	packets */
	rte_pipeline_port_out_action_handler f_action;
	/** Opaque parameter to be passed to the action handler when invoked */
	void *arg_ah;
};

/**
 * Pipeline output port create
 *
 * @param p
 *   Handle to pipeline instance
 * @param params
 *   Parameters for pipeline output port creation
 * @param port_id
 *   Output port ID. Valid only within the scope of output port IDs of the
 *   current pipeline. Only returned after a successful invocation.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_out_create(struct rte_pipeline *p,
	struct rte_pipeline_port_out_params *params,
	uint32_t *port_id);

/**
 * Read pipeline port out stats.
 *
 * This function reads port out statistics identified by *port_id* of given
 * pipeline *p*.
 *
 * @param p
 *   Handle to pipeline instance.
 * @param port_id
 *   Port ID what stats will be returned.
 * @param stats
 *   Statistics buffer.
 * @param clear
 *   If not 0 clear stats after reading.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_out_stats_read(struct rte_pipeline *p, uint32_t port_id,
	struct rte_pipeline_port_out_stats *stats, int clear);

/*
 * Functions to be called as part of the port IN/OUT or table action handlers
 */
/**
 * Action handler packet insert to output port
 *
 * This function can be called by any input/output port or table action handler
 * to send a packet out through one of the pipeline output ports. This packet is
 * generated by the action handler, i.e. this packet is not part of the burst of
 * packets read from one of the pipeline input ports and currently processed by
 * the pipeline (this packet is not an element of the pkts array input parameter
 * of the action handler).
 *
 * @param p
 *   Handle to pipeline instance
 * @param port_id
 *   Output port ID (returned by previous invocation of pipeline output port
 *   create) to send the packet specified by pkt
 * @param pkt
 *   New packet generated by the action handler
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_port_out_packet_insert(struct rte_pipeline *p,
	uint32_t port_id,
	struct rte_mbuf *pkt);

#define rte_pipeline_ah_port_out_packet_insert \
	rte_pipeline_port_out_packet_insert

/**
 * Action handler packet hijack
 *
 * This function can be called by any input/output port or table action handler
 * to hijack selected packets from the burst of packets read from one of the
 * pipeline input ports and currently processed by the pipeline. The hijacked
 * packets are removed from any further pipeline processing, with the action
 * handler now having the full ownership for these packets.
 *
 * The action handler can further send the hijacked packets out through any
 * pipeline output port by calling the rte_pipeline_ah_port_out_packet_insert()
 * function. The action handler can also drop these packets by calling the
 * rte_pktmbuf_free() function, although a better alternative is provided by
 * the action handler using the rte_pipeline_ah_packet_drop() function.
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts_mask
 *   64-bit bitmask specifying which of the packets handed over for processing
 *   to the action handler is to be hijacked by the action handler. When
 *   pkts_mask bit n is set, then element n of the pkts array (input argument to
 *   the action handler) is hijacked.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_ah_packet_hijack(struct rte_pipeline *p,
	uint64_t pkts_mask);

/**
 * Action handler packet drop
 *
 * This function is called by the pipeline action handlers (port in/out, table)
 * to drop the packets selected using packet mask.
 *
 * This function can be called by any input/output port or table action handler
 * to drop selected packets from the burst of packets read from one of the
 * pipeline input ports and currently processed by the pipeline. The dropped
 * packets are removed from any further pipeline processing and the packet
 * buffers are eventually freed to their buffer pool.
 *
 * This function updates the drop statistics counters correctly, therefore the
 * recommended approach for dropping packets by the action handlers is to call
 * this function as opposed to the action handler hijacking the packets first
 * and then dropping them invisibly to the pipeline (by using the
 * rte_pktmbuf_free() function).
 *
 * @param p
 *   Handle to pipeline instance
 * @param pkts_mask
 *   64-bit bitmask specifying which of the packets handed over for processing
 *   to the action handler is to be dropped by the action handler. When
 *   pkts_mask bit n is set, then element n of the pkts array (input argument to
 *   the action handler) is dropped.
 * @return
 *   0 on success, error code otherwise
 */
int rte_pipeline_ah_packet_drop(struct rte_pipeline *p,
	uint64_t pkts_mask);

#ifdef __cplusplus
}
#endif

#endif
