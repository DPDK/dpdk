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

#include <stddef.h>
#include <stdint.h>

#include <rte_compat.h>

#include "rte_swx_port.h"
#include "rte_swx_extern.h"

/** Name size. */
#ifndef RTE_SWX_NAME_SIZE
#define RTE_SWX_NAME_SIZE 64
#endif
/*
 * Pipeline setup and operation
 */

/** Pipeline opaque data structure. */
struct rte_swx_pipeline;

/**
 * Pipeline configure
 *
 * @param[out] p
 *   Pipeline handle. Must point to valid memory. Contains valid pipeline handle
 *   when the function returns successfully.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory.
 */
__rte_experimental
int
rte_swx_pipeline_config(struct rte_swx_pipeline **p,
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
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Struct type name.
 * @param[in] fields
 *   The sequence of struct fields.
 * @param[in] n_fields
 *   The number of struct fields.
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
				      uint32_t n_fields);

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
