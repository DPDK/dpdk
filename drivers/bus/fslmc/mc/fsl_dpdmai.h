/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __FSL_DPDMAI_H
#define __FSL_DPDMAI_H

struct fsl_mc_io;

/* Data Path DMA Interface API
 * Contains initialization APIs and runtime control APIs for DPDMAI
 */

/* General DPDMAI macros */

/**
 * Maximum number of Tx/Rx priorities per DPDMAI object
 */
#define DPDMAI_PRIO_NUM		2

/**
 * All queues considered; see dpdmai_set_rx_queue()
 */
#define DPDMAI_ALL_QUEUES	(uint8_t)(-1)

int dpdmai_open(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		int dpdmai_id,
		uint16_t *token);

int dpdmai_close(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

/**
 * struct dpdmai_cfg - Structure representing DPDMAI configuration
 * @priorities: Priorities for the DMA hardware processing; valid priorities are
 *	configured with values 1-8; the entry following last valid entry
 *	should be configured with 0
 */
struct dpdmai_cfg {
	uint8_t num_queues;
	uint8_t priorities[DPDMAI_PRIO_NUM];
};

int dpdmai_create(struct fsl_mc_io *mc_io,
		  uint16_t dprc_token,
		  uint32_t cmd_flags,
		  const struct dpdmai_cfg *cfg,
		  uint32_t *obj_id);

int dpdmai_destroy(struct fsl_mc_io *mc_io,
		   uint16_t dprc_token,
		   uint32_t cmd_flags,
		   uint32_t object_id);

int dpdmai_enable(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token);

int dpdmai_disable(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token);

int dpdmai_is_enabled(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      int *en);

int dpdmai_reset(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token);

/**
 * struct dpdmai_attr - Structure representing DPDMAI attributes
 * @id: DPDMAI object ID
 * @num_of_priorities: number of priorities
 */
struct dpdmai_attr {
	int id;
	uint8_t num_of_priorities;
	uint8_t num_of_queues;
};

int dpdmai_get_attributes(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
			  uint16_t token,
			  struct dpdmai_attr *attr);

/**
 * enum dpdmai_dest - DPDMAI destination types
 * @DPDMAI_DEST_NONE: Unassigned destination; The queue is set in parked mode
 *	and does not generate FQDAN notifications; user is expected to dequeue
 *	from the queue based on polling or other user-defined method
 * @DPDMAI_DEST_DPIO: The queue is set in schedule mode and generates FQDAN
 *	notifications to the specified DPIO; user is expected to dequeue
 *	from the queue only after notification is received
 * @DPDMAI_DEST_DPCON: The queue is set in schedule mode and does not generate
 *	FQDAN notifications, but is connected to the specified DPCON object;
 *	user is expected to dequeue from the DPCON channel
 */
enum dpdmai_dest {
	DPDMAI_DEST_NONE = 0,
	DPDMAI_DEST_DPIO = 1,
	DPDMAI_DEST_DPCON = 2
};

/**
 * struct dpdmai_dest_cfg - Structure representing DPDMAI destination parameters
 * @dest_type: Destination type
 * @dest_id: Either DPIO ID or DPCON ID, depending on the destination type
 * @priority: Priority selection within the DPIO or DPCON channel; valid values
 *	are 0-1 or 0-7, depending on the number of priorities in that
 *	channel; not relevant for 'DPDMAI_DEST_NONE' option
 */
struct dpdmai_dest_cfg {
	enum dpdmai_dest dest_type;
	int dest_id;
	uint8_t priority;
};

/* DPDMAI queue modification options */

/**
 * Select to modify the user's context associated with the queue
 */
#define DPDMAI_QUEUE_OPT_USER_CTX	0x00000001

/**
 * Select to modify the queue's destination
 */
#define DPDMAI_QUEUE_OPT_DEST		0x00000002

/**
 * struct dpdmai_rx_queue_cfg - DPDMAI RX queue configuration
 * @options: Flags representing the suggested modifications to the queue;
 *	Use any combination of 'DPDMAI_QUEUE_OPT_<X>' flags
 * @user_ctx: User context value provided in the frame descriptor of each
 *	dequeued frame;
 *	valid only if 'DPDMAI_QUEUE_OPT_USER_CTX' is contained in 'options'
 * @dest_cfg: Queue destination parameters;
 *	valid only if 'DPDMAI_QUEUE_OPT_DEST' is contained in 'options'
 */
struct dpdmai_rx_queue_cfg {
	uint32_t options;
	uint64_t user_ctx;
	struct dpdmai_dest_cfg dest_cfg;

};

int dpdmai_set_rx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue_idx,
			uint8_t priority,
			const struct dpdmai_rx_queue_cfg *cfg);

/**
 * struct dpdmai_rx_queue_attr - Structure representing attributes of Rx queues
 * @user_ctx:  User context value provided in the frame descriptor of each
 *	 dequeued frame
 * @dest_cfg: Queue destination configuration
 * @fqid: Virtual FQID value to be used for dequeue operations
 */
struct dpdmai_rx_queue_attr {
	uint64_t user_ctx;
	struct dpdmai_dest_cfg dest_cfg;
	uint32_t fqid;
};

int dpdmai_get_rx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue_idx,
			uint8_t priority,
			struct dpdmai_rx_queue_attr *attr);

/**
 * struct dpdmai_tx_queue_attr - Structure representing attributes of Tx queues
 * @fqid: Virtual FQID to be used for sending frames to DMA hardware
 */

struct dpdmai_tx_queue_attr {
	uint32_t fqid;
};

int dpdmai_get_tx_queue(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t queue_idx,
			uint8_t priority,
			struct dpdmai_tx_queue_attr *attr);

#endif /* __FSL_DPDMAI_H */
