/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 Intel Corporation
 *   Copyright 2017 NXP
 *   Copyright 2017 Cavium
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INCLUDE_RTE_MTR_H__
#define __INCLUDE_RTE_MTR_H__

/**
 * @file
 * RTE Generic Traffic Metering and Policing API
 *
 * This interface provides the ability to configure the traffic metering and
 * policing (MTR) in a generic way.
 *
 * The processing done for each input packet hitting a MTR object is:
 *    A) Traffic metering: The packet is assigned a color (the meter output
 *       color), based on the previous history of the flow reflected in the
 *       current state of the MTR object, according to the specific traffic
 *       metering algorithm. The traffic metering algorithm can typically work
 *       in color aware mode, in which case the input packet already has an
 *       initial color (the input color), or in color blind mode, which is
 *       equivalent to considering all input packets initially colored as green.
 *    B) Policing: There is a separate policer action configured for each meter
 *       output color, which can:
 *          a) Drop the packet.
 *          b) Keep the same packet color: the policer output color matches the
 *             meter output color (essentially a no-op action).
 *          c) Recolor the packet: the policer output color is different than
 *             the meter output color.
 *       The policer output color is the output color of the packet, which is
 *       set in the packet meta-data (i.e. struct rte_mbuf::sched::color).
 *    C) Statistics: The set of counters maintained for each MTR object is
 *       configurable and subject to the implementation support. This set
 *       includes the number of packets and bytes dropped or passed for each
 *       output color.
 *
 * Once successfully created, an MTR object is linked to one or several flows
 * through the meter action of the flow API.
 *    A) Whether an MTR object is private to a flow or potentially shared by
 *       several flows has to be specified at creation time.
 *    B) Several meter actions can be potentially registered for the same flow.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */
#include <stdint.h>
#include <rte_compat.h>
#include <rte_common.h>
#include <rte_meter.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Statistics counter type
 */
enum rte_mtr_stats_type {
	/** Number of packets passed as green by the policer. */
	RTE_MTR_STATS_N_PKTS_GREEN = 1 << 0,

	/** Number of packets passed as yellow by the policer. */
	RTE_MTR_STATS_N_PKTS_YELLOW = 1 << 1,

	/** Number of packets passed as red by the policer. */
	RTE_MTR_STATS_N_PKTS_RED = 1 << 2,

	/** Number of packets dropped by the policer. */
	RTE_MTR_STATS_N_PKTS_DROPPED = 1 << 3,

	/** Number of bytes passed as green by the policer. */
	RTE_MTR_STATS_N_BYTES_GREEN = 1 << 4,

	/** Number of bytes passed as yellow by the policer. */
	RTE_MTR_STATS_N_BYTES_YELLOW = 1 << 5,

	/** Number of bytes passed as red by the policer. */
	RTE_MTR_STATS_N_BYTES_RED = 1 << 6,

	/** Number of bytes dropped by the policer. */
	RTE_MTR_STATS_N_BYTES_DROPPED = 1 << 7,
};

/**
 * Statistics counters
 */
struct rte_mtr_stats {
	/** Number of packets passed by the policer (per color). */
	uint64_t n_pkts[RTE_COLORS];

	/** Number of bytes passed by the policer (per color). */
	uint64_t n_bytes[RTE_COLORS];

	/** Number of packets dropped by the policer. */
	uint64_t n_pkts_dropped;

	/** Number of bytes passed by the policer. */
	uint64_t n_bytes_dropped;
};

/**
 * Traffic metering algorithms
 */
enum rte_mtr_algorithm {
	/** No traffic metering performed, the output color is the same as the
	 * input color for every input packet. The meter of the MTR object is
	 * working in pass-through mode, having same effect as meter disable.
	 * @see rte_mtr_meter_disable()
	 */
	RTE_MTR_NONE = 0,

	/** Single Rate Three Color Marker (srTCM) - IETF RFC 2697. */
	RTE_MTR_SRTCM_RFC2697,

	/** Two Rate Three Color Marker (trTCM) - IETF RFC 2698. */
	RTE_MTR_TRTCM_RFC2698,

	/** Two Rate Three Color Marker (trTCM) - IETF RFC 4115. */
	RTE_MTR_TRTCM_RFC4115,
};

/**
 * Meter profile
 */
struct rte_mtr_meter_profile {
	/** Traffic metering algorithm. */
	enum rte_mtr_algorithm alg;

	RTE_STD_C11
	union {
		/** Items only valid when *alg* is set to srTCM - RFC 2697. */
		struct {
			/** Committed Information Rate (CIR) (bytes/second). */
			uint64_t cir;

			/** Committed Burst Size (CBS) (bytes). */
			uint64_t cbs;

			/** Excess Burst Size (EBS) (bytes). */
			uint64_t ebs;
		} srtcm_rfc2697;

		/** Items only valid when *alg* is set to trTCM - RFC 2698. */
		struct {
			/** Committed Information Rate (CIR) (bytes/second). */
			uint64_t cir;

			/** Peak Information Rate (PIR) (bytes/second). */
			uint64_t pir;

			/** Committed Burst Size (CBS) (byes). */
			uint64_t cbs;

			/** Peak Burst Size (PBS) (bytes). */
			uint64_t pbs;
		} trtcm_rfc2698;

		/** Items only valid when *alg* is set to trTCM - RFC 4115. */
		struct {
			/** Committed Information Rate (CIR) (bytes/second). */
			uint64_t cir;

			/** Excess Information Rate (EIR) (bytes/second). */
			uint64_t eir;

			/** Committed Burst Size (CBS) (byes). */
			uint64_t cbs;

			/** Excess Burst Size (EBS) (bytes). */
			uint64_t ebs;
		} trtcm_rfc4115;
	};
};

/**
 * Policer actions
 */
enum rte_mtr_policer_action {
	/** Recolor the packet as green. */
	MTR_POLICER_ACTION_COLOR_GREEN = 0,

	/** Recolor the packet as yellow. */
	MTR_POLICER_ACTION_COLOR_YELLOW,

	/** Recolor the packet as red. */
	MTR_POLICER_ACTION_COLOR_RED,

	/** Drop the packet. */
	MTR_POLICER_ACTION_DROP,
};

/**
 * Parameters for each traffic metering & policing object
 *
 * @see enum rte_mtr_stats_type
 */
struct rte_mtr_params {
	/** Meter profile ID. */
	uint32_t meter_profile_id;

	/** Meter input color in case of MTR object chaining. When non-zero: if
	 * a previous MTR object is enabled in the same flow, then the color
	 * determined by the latest MTR object in the same flow is used as the
	 * input color by the current MTR object, otherwise the current MTR
	 * object uses the *dscp_table* to determine the input color. When zero:
	 * the color determined by any previous MTR object in same flow is
	 * ignored by the current MTR object, which uses the *dscp_table* to
	 * determine the input color.
	 */
	int use_prev_mtr_color;

	/** Meter input color. When non-NULL: it points to a pre-allocated and
	 * pre-populated table with exactly 64 elements providing the input
	 * color for each value of the IPv4/IPv6 Differentiated Services Code
	 * Point (DSCP) input packet field. When NULL: it is equivalent to
	 * setting this parameter to an all-green populated table (i.e. table
	 * with all the 64 elements set to green color). The color blind mode
	 * is configured by setting *use_prev_mtr_color* to 0 and *dscp_table*
	 * to either NULL or to an all-green populated table. When
	 * *use_prev_mtr_color* is non-zero value or when *dscp_table* contains
	 * at least one yellow or red color element, then the color aware mode
	 * is configured.
	 */
	enum rte_color *dscp_table;

	/** Non-zero to enable the meter, zero to disable the meter at the time
	 * of MTR object creation. Ignored when the meter profile indicated by
	 * *meter_profile_id* is set to NONE.
	 * @see rte_mtr_meter_disable()
	 */
	int meter_enable;

	/** Policer actions (per meter output color). */
	enum rte_mtr_policer_action action[RTE_COLORS];

	/** Set of stats counters to be enabled.
	 * @see enum rte_mtr_stats_type
	 */
	uint64_t stats_mask;
};

/**
 * MTR capabilities
 */
struct rte_mtr_capabilities {
	/** Maximum number of MTR objects. */
	uint32_t n_max;

	/** Maximum number of MTR objects that can be shared by multiple flows.
	 * The value of zero indicates that shared MTR objects are not
	 * supported. The maximum value is *n_max*.
	 */
	uint32_t n_shared_max;

	/** When non-zero, this flag indicates that all the MTR objects that
	 * cannot be shared by multiple flows have identical capability set.
	 */
	int identical;

	/** When non-zero, this flag indicates that all the MTR objects that
	 * can be shared by multiple flows have identical capability set.
	 */
	int shared_identical;

	/** Maximum number of flows that can share the same MTR object. The
	 * value of zero is invalid. The value of 1 means that shared MTR
	 * objects not supported.
	 */
	uint32_t shared_n_flows_per_mtr_max;

	/** Maximum number of MTR objects that can be part of the same flow. The
	 * value of zero is invalid. The value of 1 indicates that MTR object
	 * chaining is not supported. The maximum value is *n_max*.
	 */
	uint32_t chaining_n_mtrs_per_flow_max;

	/**
	 * When non-zero, it indicates that the packet color identified by one
	 * MTR object can be used as the packet input color by any subsequent
	 * MTR object from the same flow. When zero, it indicates that the color
	 * determined by one MTR object is always ignored by any subsequent MTR
	 * object from the same flow. Only valid when MTR chaining is supported,
	 * i.e. *chaining_n_mtrs_per_flow_max* is greater than 1. When non-zero,
	 * it also means that the color aware mode is supported by at least one
	 * metering algorithm.
	 */
	int chaining_use_prev_mtr_color_supported;

	/**
	 * When non-zero, it indicates that the packet color identified by one
	 * MTR object is always used as the packet input color by any subsequent
	 * MTR object that is part of the same flow. When zero, it indicates
	 * that whether the color determined by one MTR object is either ignored
	 * or used as the packet input color by any subsequent MTR object from
	 * the same flow is individually configurable for each MTR object. Only
	 * valid when *chaining_use_prev_mtr_color_supported* is non-zero.
	 */
	int chaining_use_prev_mtr_color_enforced;

	/** Maximum number of MTR objects that can have their meter configured
	 * to run the srTCM RFC 2697 algorithm. The value of 0 indicates this
	 * metering algorithm is not supported. The maximum value is *n_max*.
	 */
	uint32_t meter_srtcm_rfc2697_n_max;

	/** Maximum number of MTR objects that can have their meter configured
	 * to run the trTCM RFC 2698 algorithm. The value of 0 indicates this
	 * metering algorithm is not supported. The maximum value is *n_max*.
	 */
	uint32_t meter_trtcm_rfc2698_n_max;

	/** Maximum number of MTR objects that can have their meter configured
	 * to run the trTCM RFC 4115 algorithm. The value of 0 indicates this
	 * metering algorithm is not supported. The maximum value is *n_max*.
	 */
	uint32_t meter_trtcm_rfc4115_n_max;

	/** Maximum traffic rate that can be metered by a single MTR object. For
	 * srTCM RFC 2697, this is the maximum CIR rate. For trTCM RFC 2698,
	 * this is the maximum PIR rate. For trTCM RFC 4115, this is the maximum
	 * value for the sum of PIR and EIR rates.
	 */
	uint64_t meter_rate_max;

	/**
	 * When non-zero, it indicates that color aware mode is supported for
	 * the srTCM RFC 2697 metering algorithm.
	 */
	int color_aware_srtcm_rfc2697_supported;

	/**
	 * When non-zero, it indicates that color aware mode is supported for
	 * the trTCM RFC 2698 metering algorithm.
	 */
	int color_aware_trtcm_rfc2698_supported;

	/**
	 * When non-zero, it indicates that color aware mode is supported for
	 * the trTCM RFC 4115 metering algorithm.
	 */
	int color_aware_trtcm_rfc4115_supported;

	/** When non-zero, it indicates that the policer packet recolor actions
	 * are supported.
	 * @see enum rte_mtr_policer_action
	 */
	int policer_action_recolor_supported;

	/** When non-zero, it indicates that the policer packet drop action is
	 * supported.
	 * @see enum rte_mtr_policer_action
	 */
	int policer_action_drop_supported;

	/** Set of supported statistics counter types.
	 * @see enum rte_mtr_stats_type
	 */
	uint64_t stats_mask;
};

/**
 * Verbose error types.
 *
 * Most of them provide the type of the object referenced by struct
 * rte_mtr_error::cause.
 */
enum rte_mtr_error_type {
	RTE_MTR_ERROR_TYPE_NONE, /**< No error. */
	RTE_MTR_ERROR_TYPE_UNSPECIFIED, /**< Cause unspecified. */
	RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
	RTE_MTR_ERROR_TYPE_METER_PROFILE,
	RTE_MTR_ERROR_TYPE_MTR_ID,
	RTE_MTR_ERROR_TYPE_MTR_PARAMS,
	RTE_MTR_ERROR_TYPE_POLICER_ACTION_GREEN,
	RTE_MTR_ERROR_TYPE_POLICER_ACTION_YELLOW,
	RTE_MTR_ERROR_TYPE_POLICER_ACTION_RED,
	RTE_MTR_ERROR_TYPE_STATS_MASK,
	RTE_MTR_ERROR_TYPE_STATS,
	RTE_MTR_ERROR_TYPE_SHARED,
};

/**
 * Verbose error structure definition.
 *
 * This object is normally allocated by applications and set by PMDs, the
 * message points to a constant string which does not need to be freed by
 * the application, however its pointer can be considered valid only as long
 * as its associated DPDK port remains configured. Closing the underlying
 * device or unloading the PMD invalidates it.
 *
 * Both cause and message may be NULL regardless of the error type.
 */
struct rte_mtr_error {
	enum rte_mtr_error_type type; /**< Cause field and error type. */
	const void *cause; /**< Object responsible for the error. */
	const char *message; /**< Human-readable error message. */
};

/**
 * MTR capabilities get
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[out] cap
 *   MTR capabilities. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_capabilities_get(uint16_t port_id,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error);

/**
 * Meter profile add
 *
 * Create a new meter profile with ID set to *meter_profile_id*. The new profile
 * is used to create one or several MTR objects.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] meter_profile_id
 *   ID for the new meter profile. Needs to be unused by any of the existing
 *   meter profiles added for the current port.
 * @param[in] profile
 *   Meter profile parameters. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_profile_add(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error);

/**
 * Meter profile delete
 *
 * Delete an existing meter profile. This operation fails when there is
 * currently at least one user (i.e. MTR object) of this profile.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] meter_profile_id
 *   Meter profile ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_profile_delete(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error);

/**
 * MTR object create
 *
 * Create a new MTR object for the current port. This object is run as part of
 * associated flow action for traffic metering and policing.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be unused by any of the existing MTR objects.
 *   created for the current port.
 * @param[in] params
 *   MTR object params. Needs to be pre-allocated and valid.
 * @param[in] shared
 *   Non-zero when this MTR object can be shared by multiple flows, zero when
 *   this MTR object can be used by a single flow.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see enum rte_flow_action_type::RTE_FLOW_ACTION_TYPE_METER
 */
int __rte_experimental
rte_mtr_create(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error);

/**
 * MTR object destroy
 *
 * Delete an existing MTR object. This operation fails when there is currently
 * at least one user (i.e. flow) of this MTR object.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 *   created for the current port.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_destroy(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error);

/**
 * MTR object meter disable
 *
 * Disable the meter of an existing MTR object. In disabled state, the meter of
 * the current MTR object works in pass-through mode, meaning that for each
 * input packet the meter output color is always the same as the input color. In
 * particular, when the meter of the current MTR object is configured in color
 * blind mode, the input color is always green, so the meter output color is
 * also always green. Note that the policer and the statistics of the current
 * MTR object are working as usual while the meter is disabled. No action is
 * taken and this function returns successfully when the meter of the current
 * MTR object is already disabled.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_disable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error);

/**
 * MTR object meter enable
 *
 * Enable the meter of an existing MTR object. If the MTR object has its meter
 * already enabled, then no action is taken and this function returns
 * successfully.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_enable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error);

/**
 * MTR object meter profile update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 * @param[in] meter_profile_id
 *   Meter profile ID for the current MTR object. Needs to be valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_profile_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error);

/**
 * MTR object DSCP table update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 * @param[in] dscp_table
 *   When non-NULL: it points to a pre-allocated and pre-populated table with
 *   exactly 64 elements providing the input color for each value of the
 *   IPv4/IPv6 Differentiated Services Code Point (DSCP) input packet field.
 *   When NULL: it is equivalent to setting this parameter to an “all-green”
 *   populated table (i.e. table with all the 64 elements set to green color).
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_meter_dscp_table_update(uint16_t port_id,
	uint32_t mtr_id,
	enum rte_color *dscp_table,
	struct rte_mtr_error *error);

/**
 * MTR object policer actions update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 * @param[in] action_mask
 *   Bit mask indicating which policer actions need to be updated. One or more
 *   policer actions can be updated in a single function invocation. To update
 *   the policer action associated with color C, bit (1 << C) needs to be set in
 *   *action_mask* and element at position C in the *actions* array needs to be
 *   valid.
 * @param[in] actions
 *   Pre-allocated and pre-populated array of policer actions.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_mtr_policer_actions_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t action_mask,
	enum rte_mtr_policer_action *actions,
	struct rte_mtr_error *error);

/**
 * MTR object enabled statistics counters update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 * @param[in] stats_mask
 *   Mask of statistics counter types to be enabled for the current MTR object.
 *   Any statistics counter type not included in this set is to be disabled for
 *   the current MTR object.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see enum rte_mtr_stats_type
 */
int __rte_experimental
rte_mtr_stats_update(uint16_t port_id,
	uint32_t mtr_id,
	uint64_t stats_mask,
	struct rte_mtr_error *error);

/**
 * MTR object statistics counters read
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mtr_id
 *   MTR object ID. Needs to be valid.
 * @param[out] stats
 *   When non-NULL, it contains the current value for the statistics counters
 *   enabled for the current MTR object.
 * @param[out] stats_mask
 *   When non-NULL, it contains the mask of statistics counter types that are
 *   currently enabled for this MTR object, indicating which of the counters
 *   retrieved with the *stats* structure are valid.
 * @param[in] clear
 *   When this parameter has a non-zero value, the statistics counters are
 *   cleared (i.e. set to zero) immediately after they have been read,
 *   otherwise the statistics counters are left untouched.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see enum rte_mtr_stats_type
 */
int __rte_experimental
rte_mtr_stats_read(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_mtr_error *error);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_MTR_H__ */
