/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __FSL_DPSECI_H
#define __FSL_DPSECI_H

/* Data Path SEC Interface API
 * Contains initialization APIs and runtime control APIs for DPSECI
 */

struct fsl_mc_io;

/**
 * General DPSECI macros
 */

/**
 * Maximum number of Tx/Rx priorities per DPSECI object
 */
#define DPSECI_PRIO_NUM		8

/**
 * All queues considered; see dpseci_set_rx_queue()
 */
#define DPSECI_ALL_QUEUES	(uint8_t)(-1)

/**
 * dpseci_open() - Open a control session for the specified object
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpseci_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	dpseci_id	DPSECI unique ID
 * @param	token		Returned token; use in subsequent API calls
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_open(struct fsl_mc_io *mc_io,
	    uint32_t cmd_flags,
	    int dpseci_id,
	    uint16_t *token);

/**
 * dpseci_close() - Close the control session of the object
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_close(struct fsl_mc_io *mc_io,
	     uint32_t cmd_flags,
	     uint16_t token);

/**
 * struct dpseci_cfg - Structure representing DPSECI configuration
 */
struct dpseci_cfg {
	uint8_t num_tx_queues;	/* num of queues towards the SEC */
	uint8_t num_rx_queues;	/* num of queues back from the SEC */
	uint8_t priorities[DPSECI_PRIO_NUM];
	/**< Priorities for the SEC hardware processing;
	 * each place in the array is the priority of the tx queue
	 * towards the SEC,
	 * valid priorities are configured with values 1-8;
	 */
};

/**
 * dpseci_create() - Create the DPSECI object
 * Create the DPSECI object, allocate required resources and
 * perform required initialization.
 *
 * The object can be created either by declaring it in the
 * DPL file, or by calling this function.
 *
 * The function accepts an authentication token of a parent
 * container that this object should be assigned to. The token
 * can be '0' so the object will be assigned to the default container.
 * The newly created object can be opened with the returned
 * object id and using the container's associated tokens and MC portals.
 *
 * @param	mc_io	      Pointer to MC portal's I/O object
 * @param	dprc_token    Parent container token; '0' for default container
 * @param	cmd_flags     Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	cfg	      Configuration structure
 * @param	obj_id	      returned object id
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_create(struct fsl_mc_io *mc_io,
	      uint16_t dprc_token,
	      uint32_t cmd_flags,
	      const struct dpseci_cfg *cfg,
	      uint32_t *obj_id);

/**
 * dpseci_destroy() - Destroy the DPSECI object and release all its resources.
 * The function accepts the authentication token of the parent container that
 * created the object (not the one that currently owns the object). The object
 * is searched within parent using the provided 'object_id'.
 * All tokens to the object must be closed before calling destroy.
 *
 * @param	mc_io	      Pointer to MC portal's I/O object
 * @param	dprc_token    Parent container token; '0' for default container
 * @param	cmd_flags     Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	object_id     The object id; it must be a valid id within the
 *			      container that created this object;
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_destroy(struct fsl_mc_io	*mc_io,
	       uint16_t	dprc_token,
	       uint32_t	cmd_flags,
	       uint32_t	object_id);

/**
 * dpseci_enable() - Enable the DPSECI, allow sending and receiving frames.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_enable(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token);

/**
 * dpseci_disable() - Disable the DPSECI, stop sending and receiving frames.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_disable(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token);

/**
 * dpseci_is_enabled() - Check if the DPSECI is enabled.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	en		Returns '1' if object is enabled; '0' otherwise
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_is_enabled(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  int *en);

/**
 * dpseci_reset() - Reset the DPSECI, returns the object to initial state.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_reset(struct fsl_mc_io *mc_io,
	     uint32_t cmd_flags,
	     uint16_t token);

/**
 * struct dpseci_irq_cfg - IRQ configuration
 */
struct dpseci_irq_cfg {
	uint64_t addr;
	/* Address that must be written to signal a message-based interrupt */
	uint32_t val;
	/* Value to write into irq_addr address */
	int irq_num;
	/* A user defined number associated with this IRQ */
};

/**
 * dpseci_set_irq() - Set IRQ information for the DPSECI to trigger an interrupt
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	Identifies the interrupt index to configure
 * @param	irq_cfg		IRQ configuration
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_set_irq(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token,
	       uint8_t irq_index,
	       struct dpseci_irq_cfg *irq_cfg);

/**
 * dpseci_get_irq() - Get IRQ information from the DPSECI
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	type		Interrupt type: 0 represents message interrupt
 *				type (both irq_addr and irq_val are valid)
 * @param	irq_cfg		IRQ attributes
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_irq(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token,
	       uint8_t irq_index,
	       int *type,
	       struct dpseci_irq_cfg *irq_cfg);

/**
 * dpseci_set_irq_enable() - Set overall interrupt state.
 * Allows GPP software to control when interrupts are generated.
 * Each interrupt can have up to 32 causes.  The enable/disable control's the
 * overall interrupt state. if the interrupt is disabled no causes will cause
 * an interrupt
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	en		Interrupt state - enable = 1, disable = 0
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_set_irq_enable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint8_t en);

/**
 * dpseci_get_irq_enable() - Get overall interrupt state
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	en		Returned Interrupt state - enable = 1,
 *				disable = 0
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_irq_enable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint8_t *en);

/**
 * dpseci_set_irq_mask() - Set interrupt mask.
 * Every interrupt can have up to 32 causes and the interrupt model supports
 * masking/unmasking each cause independently
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	mask		event mask to trigger interrupt;
 *				each bit:
 *					0 = ignore event
 *					1 = consider event for asserting IRQ
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_set_irq_mask(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t irq_index,
		    uint32_t mask);

/**
 * dpseci_get_irq_mask() - Get interrupt mask.
 * Every interrupt can have up to 32 causes and the interrupt model supports
 * masking/unmasking each cause independently
 *
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	mask		Returned event mask to trigger interrupt
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_irq_mask(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t irq_index,
		    uint32_t *mask);

/**
 * dpseci_get_irq_status() - Get the current status of any pending interrupts
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	status		Returned interrupts status - one bit per cause:
 *					0 = no interrupt pending
 *					1 = interrupt pending
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_irq_status(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t *status);

/**
 * dpseci_clear_irq_status() - Clear a pending interrupt's status
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	irq_index	The interrupt index to configure
 * @param	status		bits to clear (W1C) - one bit per cause:
 *					0 = don't change
 *					1 = clear status bit
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_clear_irq_status(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint32_t status);

/**
 * struct dpseci_attr - Structure representing DPSECI attributes
 * @param	id: DPSECI object ID
 * @param	num_tx_queues: number of queues towards the SEC
 * @param	num_rx_queues: number of queues back from the SEC
 */
struct dpseci_attr {
	int id;			/* DPSECI object ID */
	uint8_t num_tx_queues;	/* number of queues towards the SEC */
	uint8_t num_rx_queues;	/* number of queues back from the SEC */
};

/**
 * dpseci_get_attributes() - Retrieve DPSECI attributes.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	attr		Returned object's attributes
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_attributes(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      struct dpseci_attr *attr);

/**
 * enum dpseci_dest - DPSECI destination types
 * @DPSECI_DEST_NONE: Unassigned destination; The queue is set in parked mode
 *		and does not generate FQDAN notifications; user is expected to
 *		dequeue from the queue based on polling or other user-defined
 *		method
 * @DPSECI_DEST_DPIO: The queue is set in schedule mode and generates FQDAN
 *		notifications to the specified DPIO; user is expected to dequeue
 *		from the queue only after notification is received
 * @DPSECI_DEST_DPCON: The queue is set in schedule mode and does not generate
 *		FQDAN notifications, but is connected to the specified DPCON
 *		object; user is expected to dequeue from the DPCON channel
 */
enum dpseci_dest {
	DPSECI_DEST_NONE = 0,
	DPSECI_DEST_DPIO = 1,
	DPSECI_DEST_DPCON = 2
};

/**
 * struct dpseci_dest_cfg - Structure representing DPSECI destination parameters
 */
struct dpseci_dest_cfg {
	enum dpseci_dest dest_type; /* Destination type */
	int dest_id;
	/* Either DPIO ID or DPCON ID, depending on the destination type */
	uint8_t priority;
	/* Priority selection within the DPIO or DPCON channel; valid values
	 * are 0-1 or 0-7, depending on the number of priorities in that
	 * channel; not relevant for 'DPSECI_DEST_NONE' option
	 */
};

/**
 * DPSECI queue modification options
 */

/**
 * Select to modify the user's context associated with the queue
 */
#define DPSECI_QUEUE_OPT_USER_CTX		0x00000001

/**
 * Select to modify the queue's destination
 */
#define DPSECI_QUEUE_OPT_DEST			0x00000002

/**
 * Select to modify the queue's order preservation
 */
#define DPSECI_QUEUE_OPT_ORDER_PRESERVATION	0x00000004

/**
 * struct dpseci_rx_queue_cfg - DPSECI RX queue configuration
 */
struct dpseci_rx_queue_cfg {
	uint32_t options;
	/* Flags representing the suggested modifications to the queue;
	 * Use any combination of 'DPSECI_QUEUE_OPT_<X>' flags
	 */
	int order_preservation_en;
	/* order preservation configuration for the rx queue
	 * valid only if 'DPSECI_QUEUE_OPT_ORDER_PRESERVATION' is contained in
	 * 'options'
	 */
	uint64_t user_ctx;
	/* User context value provided in the frame descriptor of each
	 * dequeued frame;
	 * valid only if 'DPSECI_QUEUE_OPT_USER_CTX' is contained in 'options'
	 */
	struct dpseci_dest_cfg dest_cfg;
	/* Queue destination parameters;
	 * valid only if 'DPSECI_QUEUE_OPT_DEST' is contained in 'options'
	 */
};

/**
 * dpseci_set_rx_queue() - Set Rx queue configuration
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	queue		Select the queue relative to number of
 *				priorities configured at DPSECI creation; use
 *				DPSECI_ALL_QUEUES to configure all Rx queues
 *				identically.
 * @param	cfg		Rx queue configuration
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_set_rx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    const struct dpseci_rx_queue_cfg *cfg);

/**
 * struct dpseci_rx_queue_attr - Structure representing attributes of Rx queues
 */
struct dpseci_rx_queue_attr {
	uint64_t user_ctx;
	/* User context value provided in the frame descriptor of
	 * each dequeued frame
	 */
	int order_preservation_en;
	/* Status of the order preservation configuration on the queue */
	struct dpseci_dest_cfg	dest_cfg;
	/* Queue destination configuration */
	uint32_t fqid;
	/* Virtual FQID value to be used for dequeue operations */
};

/**
 * dpseci_get_rx_queue() - Retrieve Rx queue attributes.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	queue		Select the queue relative to number of
 *				priorities configured at DPSECI creation
 * @param	attr		Returned Rx queue attributes
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_rx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    struct dpseci_rx_queue_attr *attr);

/**
 * struct dpseci_tx_queue_attr - Structure representing attributes of Tx queues
 */
struct dpseci_tx_queue_attr {
	uint32_t fqid;
	/* Virtual FQID to be used for sending frames to SEC hardware */
	uint8_t priority;
	/* SEC hardware processing priority for the queue */
};

/**
 * dpseci_get_tx_queue() - Retrieve Tx queue attributes.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	queue		Select the queue relative to number of
 *				priorities configured at DPSECI creation
 * @param	attr		Returned Tx queue attributes
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_tx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    struct dpseci_tx_queue_attr *attr);

/**
 * struct dpseci_sec_attr - Structure representing attributes of the SEC
 *			hardware accelerator
 */

struct dpseci_sec_attr {
	uint16_t ip_id;		/* ID for SEC */
	uint8_t major_rev;	/* Major revision number for SEC */
	uint8_t minor_rev;	/* Minor revision number for SEC */
	uint8_t era;		/* SEC Era */
	uint8_t deco_num;
	/* The number of copies of the DECO that are implemented in
	 * this version of SEC
	 */
	uint8_t zuc_auth_acc_num;
	/* The number of copies of ZUCA that are implemented in this
	 * version of SEC
	 */
	uint8_t zuc_enc_acc_num;
	/* The number of copies of ZUCE that are implemented in this
	 * version of SEC
	 */
	uint8_t snow_f8_acc_num;
	/* The number of copies of the SNOW-f8 module that are
	 * implemented in this version of SEC
	 */
	uint8_t snow_f9_acc_num;
	/* The number of copies of the SNOW-f9 module that are
	 * implemented in this version of SEC
	 */
	uint8_t crc_acc_num;
	/* The number of copies of the CRC module that are implemented
	 * in this version of SEC
	 */
	uint8_t pk_acc_num;
	/* The number of copies of the Public Key module that are
	 * implemented in this version of SEC
	 */
	uint8_t kasumi_acc_num;
	/* The number of copies of the Kasumi module that are
	 * implemented in this version of SEC
	 */
	uint8_t rng_acc_num;
	/* The number of copies of the Random Number Generator that are
	 * implemented in this version of SEC
	 */
	uint8_t md_acc_num;
	/* The number of copies of the MDHA (Hashing module) that are
	 * implemented in this version of SEC
	 */
	uint8_t arc4_acc_num;
	/* The number of copies of the ARC4 module that are implemented
	 * in this version of SEC
	 */
	uint8_t des_acc_num;
	/* The number of copies of the DES module that are implemented
	 * in this version of SEC
	 */
	uint8_t aes_acc_num;
	/* The number of copies of the AES module that are implemented
	 * in this version of SEC
	 */
};

/**
 * dpseci_get_sec_attr() - Retrieve SEC accelerator attributes.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	attr		Returned SEC attributes
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_sec_attr(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    struct dpseci_sec_attr *attr);

/**
 * struct dpseci_sec_counters - Structure representing global SEC counters and
 *				not per dpseci counters
 */
struct dpseci_sec_counters {
	uint64_t dequeued_requests; /* Number of Requests Dequeued */
	uint64_t ob_enc_requests;   /* Number of Outbound Encrypt Requests */
	uint64_t ib_dec_requests;   /* Number of Inbound Decrypt Requests */
	uint64_t ob_enc_bytes;      /* Number of Outbound Bytes Encrypted */
	uint64_t ob_prot_bytes;     /* Number of Outbound Bytes Protected */
	uint64_t ib_dec_bytes;      /* Number of Inbound Bytes Decrypted */
	uint64_t ib_valid_bytes;    /* Number of Inbound Bytes Validated */
};

/**
 * dpseci_get_sec_counters() - Retrieve SEC accelerator counters.
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	token		Token of DPSECI object
 * @param	counters	Returned SEC counters
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_sec_counters(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpseci_sec_counters *counters);

/**
 * dpseci_get_api_version() - Get Data Path SEC Interface API version
 * @param	mc_io		Pointer to MC portal's I/O object
 * @param	cmd_flags	Command flags; one or more of 'MC_CMD_FLAG_'
 * @param	major_ver	Major version of data path sec API
 * @param	minor_ver	Minor version of data path sec API
 *
 * @return:
 *   - Return '0' on Success.
 *   - Return Error code otherwise.
 */
int
dpseci_get_api_version(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t *major_ver,
		       uint16_t *minor_ver);

#endif /* __FSL_DPSECI_H */
