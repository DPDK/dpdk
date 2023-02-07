/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PIPELINE_H__
#define __INCLUDE_RTE_SWX_PIPELINE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Pipeline
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_compat.h>

#include "rte_swx_port.h"
#include "rte_swx_table.h"
#include "rte_swx_extern.h"

/** Name size. */
#ifndef RTE_SWX_NAME_SIZE
#define RTE_SWX_NAME_SIZE 64
#endif

/** Instruction size. */
#ifndef RTE_SWX_INSTRUCTION_SIZE
#define RTE_SWX_INSTRUCTION_SIZE 256
#endif

/** Instruction tokens. */
#ifndef RTE_SWX_INSTRUCTION_TOKENS_MAX
#define RTE_SWX_INSTRUCTION_TOKENS_MAX 16
#endif

/*
 * Pipeline setup and operation
 */

/** Pipeline opaque data structure. */
struct rte_swx_pipeline;

/**
 * Pipeline find
 *
 * @param[in] name
 *   Pipeline name.
 * @return
 *   Valid pipeline handle if found or NULL otherwise.
 */
__rte_experimental
struct rte_swx_pipeline *
rte_swx_pipeline_find(const char *name);

/**
 * Pipeline configure
 *
 * @param[out] p
 *   Pipeline handle. Must point to valid memory. Contains valid pipeline handle
 *   when the function returns successfully.
 * @param[in] name
 *   Pipeline unique name.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Pipeline with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_config(struct rte_swx_pipeline **p,
			const char *name,
			int numa_node);

/*
 * Pipeline input ports
 */

/**
 * Pipeline input port type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Input port type name.
 * @param[in] ops
 *   Input port type operations.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Input port type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_port_in_type_register(struct rte_swx_pipeline *p,
				       const char *name,
				       struct rte_swx_port_in_ops *ops);

/**
 * Pipeline input port configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Input port ID.
 * @param[in] port_type_name
 *   Existing input port type name.
 * @param[in] args
 *   Input port creation arguments.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -ENODEV: Input port object creation error.
 */
__rte_experimental
int
rte_swx_pipeline_port_in_config(struct rte_swx_pipeline *p,
				uint32_t port_id,
				const char *port_type_name,
				void *args);

/*
 * Pipeline output ports
 */

/**
 * Pipeline output port type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Output port type name.
 * @param[in] ops
 *   Output port type operations.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Output port type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_port_out_type_register(struct rte_swx_pipeline *p,
					const char *name,
					struct rte_swx_port_out_ops *ops);

/**
 * Pipeline output port configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Output port ID.
 * @param[in] port_type_name
 *   Existing output port type name.
 * @param[in] args
 *   Output port creation arguments.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -ENODEV: Output port object creation error.
 */
__rte_experimental
int
rte_swx_pipeline_port_out_config(struct rte_swx_pipeline *p,
				 uint32_t port_id,
				 const char *port_type_name,
				 void *args);
/*
 * Packet mirroring
 */

/** Default number of packet mirroring slots. */
#ifndef RTE_SWX_PACKET_MIRRORING_SLOTS_DEFAULT
#define RTE_SWX_PACKET_MIRRORING_SLOTS_DEFAULT 4
#endif

/** Default maximum number of packet mirroring sessions. */
#ifndef RTE_SWX_PACKET_MIRRORING_SESSIONS_DEFAULT
#define RTE_SWX_PACKET_MIRRORING_SESSIONS_DEFAULT 64
#endif

/** Packet mirroring parameters. */
struct rte_swx_pipeline_mirroring_params {
	/** Number of packet mirroring slots. */
	uint32_t n_slots;

	/** Maximum number of packet mirroring sessions. */
	uint32_t n_sessions;
};

/**
 * Packet mirroring configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] params
 *   Packet mirroring parameters.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough memory;
 *   -EEXIST: Pipeline was already built successfully.
 */
__rte_experimental
int
rte_swx_pipeline_mirroring_config(struct rte_swx_pipeline *p,
				  struct rte_swx_pipeline_mirroring_params *params);

/*
 * Extern objects and functions
 */

/**
 * Pipeline extern type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Extern type name.
 * @param[in] mailbox_struct_type_name
 *   Name of existing struct type used to define the mailbox size and layout for
 *   the extern objects that are instances of this type. Each extern object gets
 *   its own mailbox, which is used to pass the input arguments to the member
 *   functions and retrieve the output results.
 * @param[in] constructor
 *   Function used to create the extern objects that are instances of this type.
 * @param[in] destructor
 *   Function used to free the extern objects that are instances of  this type.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Extern type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_extern_type_register(struct rte_swx_pipeline *p,
	const char *name,
	const char *mailbox_struct_type_name,
	rte_swx_extern_type_constructor_t constructor,
	rte_swx_extern_type_destructor_t destructor);

/**
 * Pipeline extern type member function register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] extern_type_name
 *   Existing extern type name.
 * @param[in] name
 *   Name for the new member function to be added to the extern type.
 * @param[in] member_func
 *   The new member function.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Member function with this name already exists for this type;
 *   -ENOSPC: Maximum number of member functions reached for this type.
 */
__rte_experimental
int
rte_swx_pipeline_extern_type_member_func_register(struct rte_swx_pipeline *p,
	const char *extern_type_name,
	const char *name,
	rte_swx_extern_type_member_func_t member_func);

/**
 * Pipeline extern object configure
 *
 * Instantiate a given extern type to create new extern object.
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] extern_type_name
 *   Existing extern type name.
 * @param[in] name
 *   Name for the new object instantiating the extern type.
 * @param[in] args
 *   Extern object constructor arguments.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Extern object with this name already exists;
 *   -ENODEV: Extern object constructor error.
 */
__rte_experimental
int
rte_swx_pipeline_extern_object_config(struct rte_swx_pipeline *p,
				      const char *extern_type_name,
				      const char *name,
				      const char *args);

/**
 * Pipeline extern function register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Extern function name.
 * @param[in] mailbox_struct_type_name
 *   Name of existing struct type used to define the mailbox size and layout for
 *   this extern function. The mailbox is used to pass the input arguments to
 *   the extern function and retrieve the output results.
 * @param[in] func
 *   The extern function.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Extern function with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_extern_func_register(struct rte_swx_pipeline *p,
				      const char *name,
				      const char *mailbox_struct_type_name,
				      rte_swx_extern_func_t func);
/*
 * Hash function.
 */

/**
 * Pipeline hash function register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Hash function name.
 * @param[in] func
 *   Hash function.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Hash function with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_hash_func_register(struct rte_swx_pipeline *p,
				    const char *name,
				    rte_swx_hash_func_t func);

/*
 * RSS.
 */

/**
 * Pipeline Receive Side Scaling (RSS) object configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Name for the new RSS object.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: RSS object with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_rss_config(struct rte_swx_pipeline *p,
			    const char *name);

/*
 * Packet headers and meta-data
 */

/** Structure (struct) field. */
struct rte_swx_field_params {
	/** Struct field name. */
	const char *name;

	/** Struct field size (in bits).
	 * Restriction: All struct fields must be a multiple of 8 bits.
	 * Restriction: All struct fields must be no greater than 64 bits.
	 */
	uint32_t n_bits;
};

/**
 * Pipeline struct type register
 *
 * Structs are used extensively in many part of the pipeline to define the size
 * and layout of a specific memory piece such as: headers, meta-data, action
 * data stored in a table entry, mailboxes for extern objects and functions.
 * Similar to C language structs, they are a well defined sequence of fields,
 * with each field having a unique name and a constant size.
 *
 * In order to use structs to express variable size packet headers such as IPv4
 * with options, it is allowed for the last field of the struct type to have a
 * variable size between 0 and *n_bits* bits, with the actual size of this field
 * determined at run-time for each packet. This struct feature is restricted to
 * just a few selected instructions that deal with packet headers, so a typical
 * struct generally has a constant size that is fully known when its struct type
 * is registered.
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Struct type name.
 * @param[in] fields
 *   The sequence of struct fields.
 * @param[in] n_fields
 *   The number of struct fields.
 * @param[in] last_field_has_variable_size
 *   If non-zero (true), then the last field has a variable size between 0 and
 *   *n_bits* bits, with its actual size determined at run-time for each packet.
 *   If zero (false), then the last field has a constant size of *n_bits* bits.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Struct type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_struct_type_register(struct rte_swx_pipeline *p,
				      const char *name,
				      struct rte_swx_field_params *fields,
				      uint32_t n_fields,
				      int last_field_has_variable_size);

/**
 * Pipeline packet header register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Header name.
 * @param[in] struct_type_name
 *   The struct type instantiated by this packet header.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Header with this name already exists;
 *   -ENOSPC: Maximum number of headers reached for the pipeline.
 */
__rte_experimental
int
rte_swx_pipeline_packet_header_register(struct rte_swx_pipeline *p,
					const char *name,
					const char *struct_type_name);

/**
 * Pipeline packet meta-data register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] struct_type_name
 *   The struct type instantiated by the packet meta-data.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_pipeline_packet_metadata_register(struct rte_swx_pipeline *p,
					  const char *struct_type_name);

/*
 * Instructions
 */

/**
 * Instruction operands:
 *
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>|     | Description               | Format           | DST | SRC |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| hdr | Header                    | h.header         |     |     |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| act | Action                    | ACTION           |     |     |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| tbl | Table                     | TABLE            |     |     |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| H   | Header field              | h.header.field   | YES | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| M   | Meta-data field           | m.field          | YES | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| E   | Extern obj mailbox field  | e.ext_obj.field  | YES | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| F   | Extern func mailbox field | f.ext_func.field | YES | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| T   | Table action data field   | t.header.field   | NO  | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *<pre>| I   | Immediate value (64-bit)  | h.header.field   | NO  | YES |</pre>
 *<pre>+-----+---------------------------+------------------+-----+-----+</pre>
 *
 * Instruction set:
 *
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| Instr.     | Instruction          | Instruction       | 1st  | 2nd    |</pre>
 *<pre>| Name       | Description          | Format            | opnd.| opnd.  |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| rx         | Receive one pkt      | rx m.port_in      | M    |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| tx         | Transmit one pkt     | tx m.port_out     | M    |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| extract    | Extract one hdr      | extract h.hdr     | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| emit       | Emit one hdr         | emit h.hdr        | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| validate   | Validate one hdr     | validate h.hdr    | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| invalidate | Invalidate one hdr   | invalidate h.hdr  | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| mov        | dst = src            | mov dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| add        | dst += src           | add dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| sub        | dst -= src           | add dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| ckadd      | Checksum add: dst =  | add dst src       | HMEF | HMEFTI |</pre>
 *<pre>|            | dst '+ src[0:1] '+   |                   |      | or hdr |</pre>
 *<pre>|            | src[2:3] '+ ...      |                   |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| cksub      | Checksum subtract:   | add dst src       | HMEF | HMEFTI |</pre>
 *<pre>|            | dst = dst '- src     |                   |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| and        | dst &= src           | and dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| or         | dst |= src           | or  dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| xor        | dst ^= src           | xor dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| shl        | dst <<= src          | shl dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| shr        | dst >>= src          | shr dst src       | HMEF | HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| table      | Table lookup         | table TABLE       | tbl  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| extern     | Ext obj member func  | extern e.obj.mfunc| ext  |        |</pre>
 *<pre>|            | call or ext func call| extern f.func     |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmp        | Unconditional jump   | jmp LABEL         |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpv       | Jump if hdr is valid | jmpv LABEL h.hdr  | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpnv      | Jump if hdr is inval | jmpnv LABEL h.hdr | hdr  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmph       | Jump if tbl lkp hit  | jmph LABEL        |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpnh      | Jump if tbl lkp miss | jmpnh LABEL       |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpa       | Jump if action run   | jmpa LABEL ACTION | act  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpna      | Jump if act not run  | jmpna LABEL ACTION| act  |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpeq      | Jump if (a == b)     | jmpeq LABEL a b   | HMEFT| HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpneq     | Jump if (a != b)     | jmpneq LABEL a b  | HMEFT| HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmplt      | Jump if (a < b)      | jmplt LABEL a b   | HMEFT| HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| jmpgt      | Jump if (a > b)      | jmpgt LABEL a b   | HMEFT| HMEFTI |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *<pre>| return     | Return from action   | return            |      |        |</pre>
 *<pre>+------------+----------------------+-------------------+------+--------+</pre>
 *
 * At initialization time, the pipeline and action instructions (including the
 * symbolic name operands) are translated to internal data structures that are
 * used at run-time.
 */

/*
 * Pipeline action
 */

/**
 * Pipeline action configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Action name.
 * @param[in] args_struct_type_name
 *   The struct type instantiated by the action data. The action data represent
 *   the action arguments that are stored in the table entry together with the
 *   action ID. Set to NULL when the action does not have any arguments.
 * @param[in] instructions
 *   Action instructions.
 * @param[in] n_instructions
 *   Number of action instructions.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Action with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_action_config(struct rte_swx_pipeline *p,
			       const char *name,
			       const char *args_struct_type_name,
			       const char **instructions,
			       uint32_t n_instructions);

/*
 * Pipeline table
 */

/**
 * Pipeline table type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Table type name.
 * @param[in] match_type
 *   Match type implemented by the new table type.
 * @param[in] ops
 *   Table type operations.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Table type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_table_type_register(struct rte_swx_pipeline *p,
				     const char *name,
				     enum rte_swx_table_match_type match_type,
				     struct rte_swx_table_ops *ops);

/** Match field parameters. */
struct rte_swx_match_field_params {
	/** Match field name. Must be either a field of one of the registered
	 * packet headers ("h.header.field") or a field of the registered
	 * meta-data ("m.field").
	 */
	const char *name;

	/** Match type of the field. */
	enum rte_swx_table_match_type match_type;
};

/** Pipeline table parameters. */
struct rte_swx_pipeline_table_params {
	/** The set of match fields for the current table.
	 * Restriction: All the match fields of the current table need to be
	 * part of the same struct, i.e. either all the match fields are part of
	 * the same header or all the match fields are part of the meta-data.
	 */
	struct rte_swx_match_field_params *fields;

	/** The number of match fields for the current table. If set to zero, no
	 * "regular" entries (i.e. entries other than the default entry) can be
	 * added to the current table and the match process always results in
	 * lookup miss.
	 */
	uint32_t n_fields;

	/** The set of actions for the current table. */
	const char **action_names;

	/**  Array of *n_actions* flags. For each action, the associated flag
	 * indicates whether the action can be assigned to regular table entries
	 * (when non-zero, i.e. true) or not (when zero, i.e. false). When set
	 * to NULL, it defaults to true for all actions.
	 */
	int *action_is_for_table_entries;

	/**  Array of *n_actions* flags. For each action, the associated flag
	 * indicates whether the action can be assigned to the default table
	 * entry (when non-zero, i.e. true) or not (when zero, i.e. false).
	 * When set to NULL, it defaults to true for all actions.
	 */
	int *action_is_for_default_entry;

	/** The number of actions for the current table. Must be at least one.
	 */
	uint32_t n_actions;

	/** The default table action that gets executed on lookup miss. Must be
	 * one of the table actions included in the *action_names*.
	 */
	const char *default_action_name;

	/** Default action arguments. Specified as a string with the format
	 * "ARG0_NAME ARG0_VALUE ...". The number of arguments in this string
	 * must match exactly the number of arguments of the default action.
	 * Must be NULL if the default action does not have any arguments.
	 */
	const char *default_action_args;

	/** If non-zero (true), then the default action of the current table
	 * cannot be changed. If zero (false), then the default action can be
	 * changed in the future with another action from the *action_names*
	 * list.
	 */
	int default_action_is_const;

	/** Hash function name. When not set to NULL, it must point to one of
	 * the hash functions that were registered for the current pipeline.
	 * Ignored by the table implementation when not needed. When needed but
	 * NULL, the table implementation will select the hash function to use.
	 */
	const char *hash_func_name;
};

/**
 * Pipeline table configure
 *
 * @param[out] p
 *   Pipeline handle.
 * @param[in] name
 *   Table name.
 * @param[in] params
 *   Table parameters.
 * @param[in] recommended_table_type_name
 *   Recommended table type. Typically set to NULL. Useful as guidance when
 *   there are multiple table types registered for the match type of the table,
 *   as determined from the table match fields specification. Silently ignored
 *   if the recommended table type does not exist or it serves a different match
 *   type.
 * @param[in] args
 *   Table creation arguments.
 * @param[in] size
 *   Guideline on maximum number of table entries.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Table with this name already exists;
 *   -ENODEV: Table creation error.
 */
__rte_experimental
int
rte_swx_pipeline_table_config(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_swx_pipeline_table_params *params,
			      const char *recommended_table_type_name,
			      const char *args,
			      uint32_t size);

/** Pipeline selector table parameters. */
struct rte_swx_pipeline_selector_params {
	/** The group ID field. Input into the selection operation.
	 * Restriction: This field must be a meta-data field.
	 */
	const char *group_id_field_name;

	/** The set of fields used to select (through a hashing scheme) the
	 * member within the current group. Inputs into the selection operation.
	 * Restriction: All the selector fields must be part of the same struct,
	 * i.e. part of the same header or part of the meta-data structure.
	 */
	const char **selector_field_names;

	/** The number of selector fields. Must be non-zero. */
	uint32_t n_selector_fields;

	/** The member ID field. Output from the selection operation.
	 * Restriction: This field must be a meta-data field.
	 */
	const char *member_id_field_name;

	/** Maximum number of groups. Must be non-zero. */
	uint32_t n_groups_max;

	/** Maximum number of members per group. Must be non-zero. */
	uint32_t n_members_per_group_max;
};

/**
 * Pipeline selector table configure
 *
 * @param[out] p
 *   Pipeline handle.
 * @param[in] name
 *   Selector table name.
 * @param[in] params
 *   Selector table parameters.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Selector table with this name already exists;
 *   -ENODEV: Selector table creation error.
 */
__rte_experimental
int
rte_swx_pipeline_selector_config(struct rte_swx_pipeline *p,
				 const char *name,
				 struct rte_swx_pipeline_selector_params *params);

/** Pipeline learner table parameters. */
struct rte_swx_pipeline_learner_params {
	/** The set of match fields for the current table.
	 * Restriction: All the match fields of the current table need to be
	 * part of the same struct, i.e. either all the match fields are part of
	 * the same header or all the match fields are part of the meta-data.
	 */
	const char **field_names;

	/** The number of match fields for the current table. Must be non-zero.
	 */
	uint32_t n_fields;

	/** The set of actions for the current table. */
	const char **action_names;

	/**  Array of *n_actions* flags. For each action, the associated flag
	 * indicates whether the action can be assigned to regular table entries
	 * (when non-zero, i.e. true) or not (when zero, i.e. false). When set
	 * to NULL, it defaults to true for all actions.
	 */
	int *action_is_for_table_entries;

	/**  Array of *n_actions* flags. For each action, the associated flag
	 * indicates whether the action can be assigned to the default table
	 * entry (when non-zero, i.e. true) or not (when zero, i.e. false).
	 * When set to NULL, it defaults to true for all actions.
	 */
	int *action_is_for_default_entry;

	/** The number of actions for the current table. Must be at least one.
	 */
	uint32_t n_actions;

	/** The default table action that gets executed on lookup miss. Must be
	 * one of the table actions included in the *action_names*.
	 */
	const char *default_action_name;

	/** Default action arguments. Specified as a string with the format
	 * "ARG0_NAME ARG0_VALUE ...". The number of arguments in this string
	 * must match exactly the number of arguments of the default action.
	 * Must be NULL if the default action does not have any arguments.
	 */
	const char *default_action_args;

	/** If non-zero (true), then the default action of the current table
	 * cannot be changed. If zero (false), then the default action can be
	 * changed in the future with another action from the *action_names*
	 * list.
	 */
	int default_action_is_const;

	/** Hash function name. When not set to NULL, it must point to one of
	 * the hash functions that were registered for the current pipeline.
	 * When NULL, the default hash function will be used.
	 */
	const char *hash_func_name;
};

/**
 * Pipeline learner table configure
 *
 * @param[out] p
 *   Pipeline handle.
 * @param[in] name
 *   Learner table name.
 * @param[in] params
 *   Learner table parameters.
 * @param[in] size
 *   The maximum number of table entries. Must be non-zero.
 * @param[in] timeout
 *   Array of possible table entry timeouts in seconds. Must be non-NULL.
 * @param[in] n_timeouts
 *   Number of elements in the *timeout* array.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Learner table with this name already exists;
 *   -ENODEV: Learner table creation error.
 */
__rte_experimental
int
rte_swx_pipeline_learner_config(struct rte_swx_pipeline *p,
				const char *name,
				struct rte_swx_pipeline_learner_params *params,
				uint32_t size,
				uint32_t *timeout,
				uint32_t n_timeouts);

/**
 * Pipeline register array configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Register array name.
 * @param[in] size
 *   Number of registers in the array. Each register is 64-bit in size.
 * @param[in] init_val
 *   Initial value for every register in the array. The recommended value is 0.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Register array with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_regarray_config(struct rte_swx_pipeline *p,
				 const char *name,
				 uint32_t size,
				 uint64_t init_val);

/**
 * Pipeline meter array configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Meter array name.
 * @param[in] size
 *   Number of meters in the array. Each meter in the array implements the Two
 *   Rate Three Color Marker (trTCM) algorithm, as specified by RFC 2698.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Meter array with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_metarray_config(struct rte_swx_pipeline *p,
				 const char *name,
				 uint32_t size);

/**
 * Pipeline instructions configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] instructions
 *   Pipeline instructions.
 * @param[in] n_instructions
 *   Number of pipeline instructions.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory.
 */
__rte_experimental
int
rte_swx_pipeline_instructions_config(struct rte_swx_pipeline *p,
				     const char **instructions,
				     uint32_t n_instructions);

/**
 * Pipeline build
 *
 * Once called, the pipeline build operation marks the end of pipeline
 * configuration. At this point, all the internal data structures needed to run
 * the pipeline are built.
 *
 * @param[in] p
 *   Pipeline handle.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Pipeline was already built successfully.
 */
__rte_experimental
int
rte_swx_pipeline_build(struct rte_swx_pipeline *p);

/**
 * Pipeline C code generate based on input specification file
 *
 * @param[in] spec_file
 *   Pipeline specification file (.spec) provided as input.
 * @param[in] code_file
 *   Pipeline C language file (.c) to be generated.
 * @param[out] err_line
 *   In case of error and non-NULL, the line number within the *spec* file where
 *   the error occurred. The first line number in the file is 1.
 * @param[out] err_msg
 *   In case of error and non-NULL, the error message.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Resource with the same name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_codegen(FILE *spec_file,
			 FILE *code_file,
			 uint32_t *err_line,
			 const char **err_msg);

/**
 * Pipeline build from shared object library
 *
 * The shared object library must be built from the C language source code file
 * previously generated by the rte_swx_pipeline_codegen() API function.
 *
 * The pipeline I/O specification file defines the I/O ports of the pipeline.
 *
 * @param[out] p
 *   Pipeline handle. Must point to valid memory. Contains valid pipeline handle
 *   when the function returns successfully.
 * @param[in] name
 *   Pipeline unique name.
 * @param[in] lib_file_name
 *   Shared object library file name.
 * @param[in] iospec_file
 *   Pipeline I/O specification file.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Pipeline with this name already exists;
 *   -ENODEV: Extern object or table creation error.
 */
__rte_experimental
int
rte_swx_pipeline_build_from_lib(struct rte_swx_pipeline **p,
				const char *name,
				const char *lib_file_name,
				FILE *iospec_file,
				int numa_node);

/**
 * Pipeline run
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] n_instructions
 *   Number of instructions to execute.
 */
__rte_experimental
void
rte_swx_pipeline_run(struct rte_swx_pipeline *p,
		     uint32_t n_instructions);

/**
 * Pipeline flush
 *
 * Flush all output ports of the pipeline.
 *
 * @param[in] p
 *   Pipeline handle.
 *   If p is NULL, no operation is performed.
 */
__rte_experimental
void
rte_swx_pipeline_flush(struct rte_swx_pipeline *p);

/**
 * Pipeline free
 *
 * @param[in] p
 *   Pipeline handle.
 */
__rte_experimental
void
rte_swx_pipeline_free(struct rte_swx_pipeline *p);

#ifdef __cplusplus
}
#endif

#endif
