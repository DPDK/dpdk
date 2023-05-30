/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef RTE_PDCP_H
#define RTE_PDCP_H

/**
 * @file rte_pdcp.h
 *
 * RTE PDCP support.
 *
 * A framework for PDCP protocol processing.
 */

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_pdcp_hdr.h>
#include <rte_security.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations. */
struct rte_pdcp_entity;

/* PDCP pre-process function based on entity configuration. */
typedef uint16_t (*rte_pdcp_pre_p_t)(const struct rte_pdcp_entity *entity,
				     struct rte_mbuf *mb[],
				     struct rte_crypto_op *cop[],
				     uint16_t num, uint16_t *nb_err);

/* PDCP post-process function based on entity configuration. */
typedef uint16_t (*rte_pdcp_post_p_t)(const struct rte_pdcp_entity *entity,
				      struct rte_mbuf *in_mb[],
				      struct rte_mbuf *out_mb[],
				      uint16_t num, uint16_t *nb_err);

/**
 * PDCP entity.
 *
 * 4.2.2 PDCP entities
 *
 * The PDCP entities are located in the PDCP sublayer.
 * Several PDCP entities may be defined for a UE.
 * Each PDCP entity is carrying the data of one radio bearer.
 * A PDCP entity is associated either to the control plane or the user plane
 * depending on which radio bearer it is carrying data for.
 */
struct rte_pdcp_entity {
	/** Entity specific pre-process handle. */
	rte_pdcp_pre_p_t pre_process;
	/** Entity specific post-process handle. */
	rte_pdcp_post_p_t post_process;
	/**
	 * PDCP entities may hold packets for purposes of in-order delivery
	 * (in case of receiving PDCP entity) and re-transmission
	 * (in case of transmitting PDCP entity).
	 *
	 * The field 'max_pkt_cache' would be used to indicate the maximum
	 * number of packets that may be cached in an entity at any point of time.
	 * When application provides buffers to receive packets from PDCP entity,
	 * the size of the buffer should be such that it can
	 * hold additionally 'max_pkt_cache' number of packets.
	 */
	uint32_t max_pkt_cache;
} __rte_cache_aligned;

/**
 * Callback function type for t-Reordering timer start, set during PDCP entity establish.
 * This callback is invoked by PDCP library, during t-Reordering timer start event.
 * Only one t-Reordering per receiving PDCP entity would be running at a given time.
 *
 * @see struct rte_pdcp_timer
 * @see rte_pdcp_entity_establish()
 *
 * @param timer
 *   Pointer to timer.
 * @param args
 *   Pointer to timer arguments.
 */
typedef void (*rte_pdcp_t_reordering_start_cb_t)(void *timer, void *args);

/**
 * Callback function type for t-Reordering timer stop, set during PDCP entity establish.
 * This callback will be invoked by PDCP library, during t-Reordering timer stop event.
 *
 * @see struct rte_pdcp_timer
 * @see rte_pdcp_entity_establish()
 *
 * @param timer
 *   Pointer to timer.
 * @param args
 *   Pointer to timer arguments.
 */
typedef void (*rte_pdcp_t_reordering_stop_cb_t)(void *timer, void *args);

/**
 * PDCP t-Reordering timer interface
 *
 * Configuration provided by user, that PDCP library will invoke according to timer behaviour.
 */
/* Structure rte_pdcp_t_reordering 8< */
struct rte_pdcp_t_reordering {
	/** Timer pointer, to be used in callback functions. */
	void *timer;
	/** Timer arguments, to be used in callback functions. */
	void *args;
	/** Timer start callback handle. */
	rte_pdcp_t_reordering_start_cb_t start;
	/** Timer stop callback handle. */
	rte_pdcp_t_reordering_stop_cb_t stop;
};
/* >8 End of structure rte_pdcp_t_reordering. */

/**
 * PDCP entity configuration to be used for establishing an entity.
 */
/* Structure rte_pdcp_entity_conf 8< */
struct rte_pdcp_entity_conf {
	/** PDCP transform for the entity. */
	struct rte_security_pdcp_xform pdcp_xfrm;
	/** Crypto transform applicable for the entity. */
	struct rte_crypto_sym_xform *crypto_xfrm;
	/** Mempool for crypto symmetric session. */
	struct rte_mempool *sess_mpool;
	/** Crypto op pool. */
	struct rte_mempool *cop_pool;
	/** Mbuf pool to be used for allocating control PDUs.*/
	struct rte_mempool *ctrl_pdu_pool;
	/**
	 * Sequence number value to be used.
	 * 32 bit count value to be used for the first packet
	 * would be derived based on HFN (`rte_security_pdcp_xform.hfn`) and SN.
	 */
	uint32_t sn;
	/** Indicate whether the PDCP entity belongs to Side Link Radio Bearer. */
	bool is_slrb;
	/** Enable security offload on the device specified. */
	bool en_sec_offload;
	/** Device on which security/crypto session need to be created. */
	uint8_t dev_id;
	/**
	 * Reverse direction during IV generation.
	 * Can be used to simulate UE crypto processing.
	 */
	bool reverse_iv_direction;
	/**
	 * Status report required (specified in TS 38.331).
	 *
	 * If PDCP entity is configured to send a PDCP status report,
	 * the upper layer application may request a receiving PDCP entity
	 * to generate a PDCP status report using ``rte_pdcp_control_pdu_create``.
	 * In addition, PDCP status reports may be generated during operations
	 * such as entity re-establishment.
	 */
	bool status_report_required;
	/** Enable out of order delivery. */
	bool out_of_order_delivery;
	/** t-Reordering timer configuration. */
	struct rte_pdcp_t_reordering t_reordering;
};
/* >8 End of structure rte_pdcp_entity_conf. */

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * 5.1.1 PDCP entity establishment
 *
 * Establish PDCP entity based on provided input configuration.
 *
 * @param conf
 *   Parameters to be used for initializing PDCP entity object.
 * @return
 *   - Valid handle if success
 *   - NULL in case of failure. rte_errno will be set to error code.
 */
__rte_experimental
struct rte_pdcp_entity *
rte_pdcp_entity_establish(const struct rte_pdcp_entity_conf *conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * 5.1.3 PDCP entity release
 *
 * Release PDCP entity.
 *
 * For UL/transmitting PDCP entity, all stored PDCP SDUs would be dropped.
 * For DL/receiving PDCP entity, the stored PDCP SDUs would be returned in
 * *out_mb* buffer. The buffer should be large enough to hold all cached
 * packets in the entity.
 *
 * Entity release would result in freeing all memory associated with the PDCP
 * entity as well as any crypto/security sessions created.
 *
 * @param pdcp_entity
 *   Pointer to the PDCP entity to be released.
 * @param[out] out_mb
 *   The address of an array that can hold up to *rte_pdcp_entity.max_pkt_cache*
 *   pointers to *rte_mbuf* structures.
 * @return
 *   -  0: Success and no cached packets to return
 *   - >0: Success and the number of packets returned in out_mb
 *   - <0: Error code in case of failures
 */
__rte_experimental
int
rte_pdcp_entity_release(struct rte_pdcp_entity *pdcp_entity,
			struct rte_mbuf *out_mb[]);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * 5.1.4 PDCP entity suspend
 *
 * Suspend PDCP entity.
 *
 * For DL/receiving PDCP entity, the stored PDCP SDUs would be returned in
 * *out_mb* buffer. The buffer should be large enough to hold all cached
 * packets in the entity.
 *
 * For UL/transmitting PDCP entity, *out_mb* buffer would be unused.
 *
 * @param pdcp_entity
 *   Pointer to the PDCP entity to be suspended.
 * @param[out] out_mb
 *   The address of an array that can hold up to *rte_pdcp_entity.max_pkt_cache*
 *   pointers to *rte_mbuf* structures.
 * @return
 *   -  0: Success and no cached packets to return.
 *   - >0: Success and the number of packets returned in out_mb.
 *   - <0: Error code in case of failures.
 */
__rte_experimental
int
rte_pdcp_entity_suspend(struct rte_pdcp_entity *pdcp_entity,
			struct rte_mbuf *out_mb[]);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create control PDU packet of the `type` specified. The control PDU packet
 * would be allocated from *rte_pdcp_entity_conf.ctrl_pdu_pool* by lib PDCP.
 *
 * @param pdcp_entity
 *   Pointer to the PDCP entity for which the control PDU need to be generated.
 * @param type
 *   Type of control PDU to be generated.
 * @return
 *   - Control PDU generated, in case of success.
 *   - NULL in case of failure. rte_errno will be set to error code.
 */
__rte_experimental
struct rte_mbuf *
rte_pdcp_control_pdu_create(struct rte_pdcp_entity *pdcp_entity,
			    enum rte_pdcp_ctrl_pdu_type type);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * For input mbufs and given PDCP entity pre-process the mbufs and prepare
 * crypto ops that can be enqueued to the cryptodev associated with given
 * session. Only error packets would be moved returned in the input buffer,
 * *mb*, and it is the responsibility of the application to free the same.
 *
 * @param entity
 *   Pointer to the *rte_pdcp_entity* object the packets belong to.
 * @param[in, out] mb
 *   The address of an array of *num* pointers to *rte_mbuf* structures
 *   which contain the input packets.
 *   Any error packets would be returned in the same buffer.
 * @param[out] cop
 *   The address of an array that can hold up to *num* pointers to
 *   *rte_crypto_op* structures. Crypto ops would be allocated by
 *   ``rte_pdcp_pkt_pre_process`` API.
 * @param num
 *   The maximum number of packets to process.
 * @param[out] nb_err
 *   Pointer to return the number of error packets returned in *mb*.
 * @return
 *   Count of crypto_ops prepared.
 */
__rte_experimental
static inline uint16_t
rte_pdcp_pkt_pre_process(const struct rte_pdcp_entity *entity,
			 struct rte_mbuf *mb[], struct rte_crypto_op *cop[],
			 uint16_t num, uint16_t *nb_err)
{
	return entity->pre_process(entity, mb, cop, num, nb_err);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * For input mbufs and given PDCP entity, perform PDCP post-processing of the mbufs.
 *
 * Input mbufs are the ones retrieved from rte_crypto_ops dequeued from cryptodev
 * and grouped by *rte_pdcp_pkt_crypto_group()*.
 *
 * The post-processed packets would be returned in the *out_mb* buffer.
 * The resultant mbufs would be grouped into success packets and error packets.
 * Error packets would be grouped in the end of the array and
 * it is the responsibility of the application to handle the same.
 *
 * When in-order delivery is enabled, PDCP entity may buffer packets and would
 * deliver packets only when all prior packets have been post-processed.
 * That would result in returning more/less packets than enqueued.
 *
 * @param entity
 *   Pointer to the *rte_pdcp_entity* object the packets belong to.
 * @param in_mb
 *   The address of an array of *num* pointers to *rte_mbuf* structures.
 * @param[out] out_mb
 *   The address of an array that can hold up to *rte_pdcp_entity.max_pkt_cache*
 *   pointers to *rte_mbuf* structures to output packets after PDCP post-processing.
 * @param num
 *   The maximum number of packets to process.
 * @param[out] nb_err
 *   The number of error packets returned in *out_mb* buffer.
 * @return
 *   Count of packets returned in *out_mb* buffer.
 */
__rte_experimental
static inline uint16_t
rte_pdcp_pkt_post_process(const struct rte_pdcp_entity *entity,
			  struct rte_mbuf *in_mb[],
			  struct rte_mbuf *out_mb[],
			  uint16_t num, uint16_t *nb_err)
{
	return entity->post_process(entity, in_mb, out_mb, num, nb_err);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * 5.2.2.2 Actions when a t-Reordering expires
 *
 * When t-Reordering timer expires, PDCP is required to slide the reception
 * window by updating state variables such as RX_REORD & RX_DELIV.
 * PDCP would need to deliver some of the buffered packets
 * based on the state variables and conditions described.
 *
 * The expiry handle need to be invoked by the application when t-Reordering
 * timer expires. In addition to returning buffered packets, it may also restart
 * timer based on the state variables.
 *
 * @param entity
 *   Pointer to the *rte_pdcp_entity* for which the timer expired.
 * @param[out] out_mb
 *   The address of an array that can hold up to *rte_pdcp_entity.max_pkt_cache*
 *   pointers to *rte_mbuf* structures. Used to return buffered packets that are expired.
 * @return
 *   Number of packets returned in *out_mb* buffer.
 */
__rte_experimental
uint16_t
rte_pdcp_t_reordering_expiry_handle(const struct rte_pdcp_entity *entity,
				    struct rte_mbuf *out_mb[]);

/**
 * The header 'rte_pdcp_group.h' depends on defines in 'rte_pdcp.h'.
 * So include in the end.
 */
#include <rte_pdcp_group.h>

#ifdef __cplusplus
}
#endif

#endif /* RTE_PDCP_H */
