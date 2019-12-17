
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_METER_H__
#define __INCLUDE_RTE_METER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Traffic Metering
 *
 * Traffic metering algorithms:
 *    1. Single Rate Three Color Marker (srTCM): defined by IETF RFC 2697
 *    2. Two Rate Three Color Marker (trTCM): defined by IETF RFC 2698
 *    3. Two Rate Three Color Marker (trTCM): defined by IETF RFC 4115
 *
 ***/

#include <stdint.h>

#include "rte_compat.h"

/*
 * Application Programmer's Interface (API)
 *
 ***/

/**
 * Color
 */
enum rte_color {
	RTE_COLOR_GREEN = 0, /**< Green */
	RTE_COLOR_YELLOW, /**< Yellow */
	RTE_COLOR_RED, /**< Red */
	RTE_COLORS /**< Number of colors */
};

/** srTCM parameters per metered traffic flow. The CIR, CBS and EBS parameters only
count bytes of IP packets and do not include link specific headers. At least one of
the CBS or EBS parameters has to be greater than zero. */
struct rte_meter_srtcm_params {
	uint64_t cir; /**< Committed Information Rate (CIR). Measured in bytes per second. */
	uint64_t cbs; /**< Committed Burst Size (CBS).  Measured in bytes. */
	uint64_t ebs; /**< Excess Burst Size (EBS).  Measured in bytes. */
};

/** trTCM parameters per metered traffic flow. The CIR, PIR, CBS and PBS parameters
only count bytes of IP packets and do not include link specific headers. PIR has to
be greater than or equal to CIR. Both CBS or EBS have to be greater than zero. */
struct rte_meter_trtcm_params {
	uint64_t cir; /**< Committed Information Rate (CIR). Measured in bytes per second. */
	uint64_t pir; /**< Peak Information Rate (PIR). Measured in bytes per second. */
	uint64_t cbs; /**< Committed Burst Size (CBS). Measured in bytes. */
	uint64_t pbs; /**< Peak Burst Size (PBS). Measured in bytes. */
};

/** trTCM parameters per metered traffic flow. The CIR, EIR, CBS and EBS
parameters only count bytes of IP packets and do not include link specific
headers. The CBS and EBS need to be greater than zero if CIR and EIR are
none-zero respectively.*/
struct rte_meter_trtcm_rfc4115_params {
	uint64_t cir; /**< Committed Information Rate (CIR). Measured in bytes per second. */
	uint64_t eir; /**< Excess Information Rate (EIR). Measured in bytes per second. */
	uint64_t cbs; /**< Committed Burst Size (CBS). Measured in bytes. */
	uint64_t ebs; /**< Excess Burst Size (EBS). Measured in bytes. */
};

/**
 * Internal data structure storing the srTCM configuration profile. Typically
 * shared by multiple srTCM objects.
 */
struct rte_meter_srtcm_profile;

/**
 * Internal data structure storing the trTCM configuration profile. Typically
 * shared by multiple trTCM objects.
 */
struct rte_meter_trtcm_profile;

/**
 * Internal data structure storing the trTCM RFC4115 configuration profile.
 * Typically shared by multiple trTCM objects.
 */
struct rte_meter_trtcm_rfc4115_profile;

/** Internal data structure storing the srTCM run-time context per metered traffic flow. */
struct rte_meter_srtcm;

/** Internal data structure storing the trTCM run-time context per metered traffic flow. */
struct rte_meter_trtcm;

/**
 * Internal data structure storing the trTCM RFC4115 run-time context per
 * metered traffic flow.
 */
struct rte_meter_trtcm_rfc4115;

/**
 * srTCM profile configuration
 *
 * @param p
 *    Pointer to pre-allocated srTCM profile data structure
 * @param params
 *    srTCM profile parameters
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_srtcm_profile_config(struct rte_meter_srtcm_profile *p,
	struct rte_meter_srtcm_params *params);

/**
 * trTCM profile configuration
 *
 * @param p
 *    Pointer to pre-allocated trTCM profile data structure
 * @param params
 *    trTCM profile parameters
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_trtcm_profile_config(struct rte_meter_trtcm_profile *p,
	struct rte_meter_trtcm_params *params);
/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * trTCM RFC 4115 profile configuration
 *
 * @param p
 *    Pointer to pre-allocated trTCM profile data structure
 * @param params
 *    trTCM profile parameters
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_trtcm_rfc4115_profile_config(
	struct rte_meter_trtcm_rfc4115_profile *p,
	struct rte_meter_trtcm_rfc4115_params *params);

/**
 * srTCM configuration per metered traffic flow
 *
 * @param m
 *    Pointer to pre-allocated srTCM data structure
 * @param p
 *    srTCM profile. Needs to be valid.
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_srtcm_config(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_profile *p);

/**
 * trTCM configuration per metered traffic flow
 *
 * @param m
 *    Pointer to pre-allocated trTCM data structure
 * @param p
 *    trTCM profile. Needs to be valid.
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_trtcm_config(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_profile *p);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * trTCM RFC 4115 configuration per metered traffic flow
 *
 * @param m
 *    Pointer to pre-allocated trTCM data structure
 * @param p
 *    trTCM profile. Needs to be valid.
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_trtcm_rfc4115_config(struct rte_meter_trtcm_rfc4115 *m,
	struct rte_meter_trtcm_rfc4115_profile *p);

/**
 * srTCM color blind traffic metering
 *
 * @param m
 *    Handle to srTCM instance
 * @param p
 *    srTCM profile specified at srTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len);

/**
 * srTCM color aware traffic metering
 *
 * @param m
 *    Handle to srTCM instance
 * @param p
 *    srTCM profile specified at srTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @param pkt_color
 *    Input color of the current IP packet
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_srtcm_color_aware_check(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color);

/**
 * trTCM color blind traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param p
 *    trTCM profile specified at trTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_trtcm_color_blind_check(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len);

/**
 * trTCM color aware traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param p
 *    trTCM profile specified at trTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @param pkt_color
 *    Input color of the current IP packet
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_trtcm_color_aware_check(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * trTCM RFC4115 color blind traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param p
 *    trTCM profile specified at trTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_trtcm_rfc4115_color_blind_check(
	struct rte_meter_trtcm_rfc4115 *m,
	struct rte_meter_trtcm_rfc4115_profile *p,
	uint64_t time,
	uint32_t pkt_len);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * trTCM RFC4115 color aware traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param p
 *    trTCM profile specified at trTCM object creation time
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_len
 *    Length of the current IP packet (measured in bytes)
 * @param pkt_color
 *    Input color of the current IP packet
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_color
rte_meter_trtcm_rfc4115_color_aware_check(
	struct rte_meter_trtcm_rfc4115 *m,
	struct rte_meter_trtcm_rfc4115_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color);

/*
 * Inline implementation of run-time methods
 *
 ***/

struct rte_meter_srtcm_profile {
	uint64_t cbs;
	/**< Upper limit for C token bucket */
	uint64_t ebs;
	/**< Upper limit for E token bucket */
	uint64_t cir_period;
	/**< Number of CPU cycles for each update of C and E token buckets */
	uint64_t cir_bytes_per_period;
	/**< Number of bytes to add to C and E token buckets on each update */
};

/* Internal data structure storing the srTCM run-time context per metered traffic flow. */
struct rte_meter_srtcm {
	uint64_t time; /* Time of latest update of C and E token buckets */
	uint64_t tc;   /* Number of bytes currently available in the committed (C) token bucket */
	uint64_t te;   /* Number of bytes currently available in the excess (E) token bucket */
};

struct rte_meter_trtcm_profile {
	uint64_t cbs;
	/**< Upper limit for C token bucket */
	uint64_t pbs;
	/**< Upper limit for P token bucket */
	uint64_t cir_period;
	/**< Number of CPU cycles for one update of C token bucket */
	uint64_t cir_bytes_per_period;
	/**< Number of bytes to add to C token bucket on each update */
	uint64_t pir_period;
	/**< Number of CPU cycles for one update of P token bucket */
	uint64_t pir_bytes_per_period;
	/**< Number of bytes to add to P token bucket on each update */
};

/**
 * Internal data structure storing the trTCM run-time context per metered
 * traffic flow.
 */
struct rte_meter_trtcm {
	uint64_t time_tc;
	/**< Time of latest update of C token bucket */
	uint64_t time_tp;
	/**< Time of latest update of P token bucket */
	uint64_t tc;
	/**< Number of bytes currently available in committed(C) token bucket */
	uint64_t tp;
	/**< Number of bytes currently available in the peak(P) token bucket */
};

struct rte_meter_trtcm_rfc4115_profile {
	uint64_t cbs;
	/**< Upper limit for C token bucket */
	uint64_t ebs;
	/**< Upper limit for E token bucket */
	uint64_t cir_period;
	/**< Number of CPU cycles for one update of C token bucket */
	uint64_t cir_bytes_per_period;
	/**< Number of bytes to add to C token bucket on each update */
	uint64_t eir_period;
	/**< Number of CPU cycles for one update of E token bucket */
	uint64_t eir_bytes_per_period;
	/**< Number of bytes to add to E token bucket on each update */
};

/**
 * Internal data structure storing the trTCM RFC4115 run-time context per
 * metered traffic flow.
 */
struct rte_meter_trtcm_rfc4115 {
	uint64_t time_tc;
	/**< Time of latest update of C token bucket */
	uint64_t time_te;
	/**< Time of latest update of E token bucket */
	uint64_t tc;
	/**< Number of bytes currently available in committed(C) token bucket */
	uint64_t te;
	/**< Number of bytes currently available in the excess(E) token bucket */
};

static inline enum rte_color
rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len)
{
	uint64_t time_diff, n_periods, tc, te;

	/* Bucket update */
	time_diff = time - m->time;
	n_periods = time_diff / p->cir_period;
	m->time += n_periods * p->cir_period;

	/* Put the tokens overflowing from tc into te bucket */
	tc = m->tc + n_periods * p->cir_bytes_per_period;
	te = m->te;
	if (tc > p->cbs) {
		te += (tc - p->cbs);
		if (te > p->ebs)
			te = p->ebs;
		tc = p->cbs;
	}

	/* Color logic */
	if (tc >= pkt_len) {
		m->tc = tc - pkt_len;
		m->te = te;
		return RTE_COLOR_GREEN;
	}

	if (te >= pkt_len) {
		m->tc = tc;
		m->te = te - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	m->tc = tc;
	m->te = te;
	return RTE_COLOR_RED;
}

static inline enum rte_color
rte_meter_srtcm_color_aware_check(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color)
{
	uint64_t time_diff, n_periods, tc, te;

	/* Bucket update */
	time_diff = time - m->time;
	n_periods = time_diff / p->cir_period;
	m->time += n_periods * p->cir_period;

	/* Put the tokens overflowing from tc into te bucket */
	tc = m->tc + n_periods * p->cir_bytes_per_period;
	te = m->te;
	if (tc > p->cbs) {
		te += (tc - p->cbs);
		if (te > p->ebs)
			te = p->ebs;
		tc = p->cbs;
	}

	/* Color logic */
	if ((pkt_color == RTE_COLOR_GREEN) && (tc >= pkt_len)) {
		m->tc = tc - pkt_len;
		m->te = te;
		return RTE_COLOR_GREEN;
	}

	if ((pkt_color != RTE_COLOR_RED) && (te >= pkt_len)) {
		m->tc = tc;
		m->te = te - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	m->tc = tc;
	m->te = te;
	return RTE_COLOR_RED;
}

static inline enum rte_color
rte_meter_trtcm_color_blind_check(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len)
{
	uint64_t time_diff_tc, time_diff_tp, n_periods_tc, n_periods_tp, tc, tp;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_tp = time - m->time_tp;
	n_periods_tc = time_diff_tc / p->cir_period;
	n_periods_tp = time_diff_tp / p->pir_period;
	m->time_tc += n_periods_tc * p->cir_period;
	m->time_tp += n_periods_tp * p->pir_period;

	tc = m->tc + n_periods_tc * p->cir_bytes_per_period;
	if (tc > p->cbs)
		tc = p->cbs;

	tp = m->tp + n_periods_tp * p->pir_bytes_per_period;
	if (tp > p->pbs)
		tp = p->pbs;

	/* Color logic */
	if (tp < pkt_len) {
		m->tc = tc;
		m->tp = tp;
		return RTE_COLOR_RED;
	}

	if (tc < pkt_len) {
		m->tc = tc;
		m->tp = tp - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	m->tc = tc - pkt_len;
	m->tp = tp - pkt_len;
	return RTE_COLOR_GREEN;
}

static inline enum rte_color
rte_meter_trtcm_color_aware_check(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color)
{
	uint64_t time_diff_tc, time_diff_tp, n_periods_tc, n_periods_tp, tc, tp;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_tp = time - m->time_tp;
	n_periods_tc = time_diff_tc / p->cir_period;
	n_periods_tp = time_diff_tp / p->pir_period;
	m->time_tc += n_periods_tc * p->cir_period;
	m->time_tp += n_periods_tp * p->pir_period;

	tc = m->tc + n_periods_tc * p->cir_bytes_per_period;
	if (tc > p->cbs)
		tc = p->cbs;

	tp = m->tp + n_periods_tp * p->pir_bytes_per_period;
	if (tp > p->pbs)
		tp = p->pbs;

	/* Color logic */
	if ((pkt_color == RTE_COLOR_RED) || (tp < pkt_len)) {
		m->tc = tc;
		m->tp = tp;
		return RTE_COLOR_RED;
	}

	if ((pkt_color == RTE_COLOR_YELLOW) || (tc < pkt_len)) {
		m->tc = tc;
		m->tp = tp - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	m->tc = tc - pkt_len;
	m->tp = tp - pkt_len;
	return RTE_COLOR_GREEN;
}

static inline enum rte_color
rte_meter_trtcm_rfc4115_color_blind_check(
	struct rte_meter_trtcm_rfc4115 *m,
	struct rte_meter_trtcm_rfc4115_profile *p,
	uint64_t time,
	uint32_t pkt_len)
{
	uint64_t time_diff_tc, time_diff_te, n_periods_tc, n_periods_te, tc, te;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_te = time - m->time_te;
	n_periods_tc = time_diff_tc / p->cir_period;
	n_periods_te = time_diff_te / p->eir_period;
	m->time_tc += n_periods_tc * p->cir_period;
	m->time_te += n_periods_te * p->eir_period;

	tc = m->tc + n_periods_tc * p->cir_bytes_per_period;
	if (tc > p->cbs)
		tc = p->cbs;

	te = m->te + n_periods_te * p->eir_bytes_per_period;
	if (te > p->ebs)
		te = p->ebs;

	/* Color logic */
	if (tc >= pkt_len) {
		m->tc = tc - pkt_len;
		m->te = te;
		return RTE_COLOR_GREEN;
	}
	if (te >= pkt_len) {
		m->tc = tc;
		m->te = te - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	/* If we end up here the color is RED */
	m->tc = tc;
	m->te = te;
	return RTE_COLOR_RED;
}

static inline enum rte_color
rte_meter_trtcm_rfc4115_color_aware_check(
	struct rte_meter_trtcm_rfc4115 *m,
	struct rte_meter_trtcm_rfc4115_profile *p,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_color pkt_color)
{
	uint64_t time_diff_tc, time_diff_te, n_periods_tc, n_periods_te, tc, te;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_te = time - m->time_te;
	n_periods_tc = time_diff_tc / p->cir_period;
	n_periods_te = time_diff_te / p->eir_period;
	m->time_tc += n_periods_tc * p->cir_period;
	m->time_te += n_periods_te * p->eir_period;

	tc = m->tc + n_periods_tc * p->cir_bytes_per_period;
	if (tc > p->cbs)
		tc = p->cbs;

	te = m->te + n_periods_te * p->eir_bytes_per_period;
	if (te > p->ebs)
		te = p->ebs;

	/* Color logic */
	if ((pkt_color == RTE_COLOR_GREEN) && (tc >= pkt_len)) {
		m->tc = tc - pkt_len;
		m->te = te;
		return RTE_COLOR_GREEN;
	}

	if ((pkt_color != RTE_COLOR_RED) && (te >= pkt_len)) {
		m->tc = tc;
		m->te = te - pkt_len;
		return RTE_COLOR_YELLOW;
	}

	/* If we end up here the color is RED */
	m->tc = tc;
	m->te = te;
	return RTE_COLOR_RED;
}


#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_METER_H__ */
