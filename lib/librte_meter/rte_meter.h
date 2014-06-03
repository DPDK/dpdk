/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *
 ***/

#include <stdint.h>

/*
 * Application Programmer's Interface (API)
 *
 ***/

/** Packet Color Set */
enum rte_meter_color {
	e_RTE_METER_GREEN = 0, /**< Green */
	e_RTE_METER_YELLOW,    /**< Yellow */
	e_RTE_METER_RED,       /**< Red */
	e_RTE_METER_COLORS     /**< Number of available colors */
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
	uint64_t cbs; /**< Committed Burst Size (CBS). Measured in byes. */
	uint64_t pbs; /**< Peak Burst Size (PBS). Measured in bytes. */
};

/** Internal data structure storing the srTCM run-time context per metered traffic flow. */
struct rte_meter_srtcm;

/** Internal data structure storing the trTCM run-time context per metered traffic flow. */
struct rte_meter_trtcm;

/**
 * srTCM configuration per metered traffic flow
 *
 * @param m
 *    Pointer to pre-allocated srTCM data structure
 * @param params
 *    User parameters per srTCM metered traffic flow
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_srtcm_config(struct rte_meter_srtcm *m,
	struct rte_meter_srtcm_params *params);

/**
 * trTCM configuration per metered traffic flow
 *
 * @param m
 *    Pointer to pre-allocated trTCM data structure
 * @param params
 *    User parameters per trTCM metered traffic flow
 * @return
 *    0 upon success, error code otherwise
 */
int
rte_meter_trtcm_config(struct rte_meter_trtcm *m,
	struct rte_meter_trtcm_params *params);

/**
 * srTCM color blind traffic metering
 *
 * @param m
 *    Handle to srTCM instance
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_length
 *    Length of the current IP packet (measured in bytes)
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_meter_color
rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	uint64_t time,
	uint32_t pkt_len);

/**
 * srTCM color aware traffic metering
 *
 * @param m
 *    Handle to srTCM instance
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_length
 *    Length of the current IP packet (measured in bytes)
 * @param pkt_color
 *    Input color of the current IP packet
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_meter_color
rte_meter_srtcm_color_aware_check(struct rte_meter_srtcm *m,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_meter_color pkt_color);

/**
 * trTCM color blind traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_length
 *    Length of the current IP packet (measured in bytes)
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_meter_color
rte_meter_trtcm_color_blind_check(struct rte_meter_trtcm *m,
	uint64_t time,
	uint32_t pkt_len);

/**
 * trTCM color aware traffic metering
 *
 * @param m
 *    Handle to trTCM instance
 * @param time
 *    Current CPU time stamp (measured in CPU cycles)
 * @param pkt_length
 *    Length of the current IP packet (measured in bytes)
 * @param pkt_color
 *    Input color of the current IP packet
 * @return
 *    Color assigned to the current IP packet
 */
static inline enum rte_meter_color
rte_meter_trtcm_color_aware_check(struct rte_meter_trtcm *m,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_meter_color pkt_color);

/*
 * Inline implementation of run-time methods
 *
 ***/

/* Internal data structure storing the srTCM run-time context per metered traffic flow. */
struct rte_meter_srtcm {
	uint64_t time; /* Time of latest update of C and E token buckets */
	uint64_t tc;   /* Number of bytes currently available in the committed (C) token bucket */
	uint64_t te;   /* Number of bytes currently available in the excess (E) token bucket */
	uint64_t cbs;  /* Upper limit for C token bucket */
	uint64_t ebs;  /* Upper limit for E token bucket */
	uint64_t cir_period; /* Number of CPU cycles for one update of C and E token buckets */
	uint64_t cir_bytes_per_period; /* Number of bytes to add to C and E token buckets on each update */
};

/* Internal data structure storing the trTCM run-time context per metered traffic flow. */
struct rte_meter_trtcm {
	uint64_t time_tc; /* Time of latest update of C token bucket */
	uint64_t time_tp; /* Time of latest update of E token bucket */
	uint64_t tc;      /* Number of bytes currently available in the committed (C) token bucket */
	uint64_t tp;      /* Number of bytes currently available in the peak (P) token bucket */
	uint64_t cbs;     /* Upper limit for C token bucket */
	uint64_t pbs;     /* Upper limit for P token bucket */
	uint64_t cir_period; /* Number of CPU cycles for one update of C token bucket */
	uint64_t cir_bytes_per_period; /* Number of bytes to add to C token bucket on each update */
	uint64_t pir_period; /* Number of CPU cycles for one update of P token bucket */
	uint64_t pir_bytes_per_period; /* Number of bytes to add to P token bucket on each update */
};

static inline enum rte_meter_color
rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	uint64_t time,
	uint32_t pkt_len)
{
	uint64_t time_diff, n_periods, tc, te;

	/* Bucket update */
	time_diff = time - m->time;
	n_periods = time_diff / m->cir_period;
	m->time += n_periods * m->cir_period;

	tc = m->tc + n_periods * m->cir_bytes_per_period;
	if (tc > m->cbs)
		tc = m->cbs;

	te = m->te + n_periods * m->cir_bytes_per_period;
	if (te > m->ebs)
		te = m->ebs;

	/* Color logic */
	if (tc >= pkt_len) {
		m->tc = tc - pkt_len;
		m->te = te;
		return e_RTE_METER_GREEN;
	}

	if (te >= pkt_len) {
		m->tc = tc;
		m->te = te - pkt_len;
		return e_RTE_METER_YELLOW;
	}

	m->tc = tc;
	m->te = te;
	return e_RTE_METER_RED;
}

static inline enum rte_meter_color
rte_meter_srtcm_color_aware_check(struct rte_meter_srtcm *m,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_meter_color pkt_color)
{
	uint64_t time_diff, n_periods, tc, te;

	/* Bucket update */
	time_diff = time - m->time;
	n_periods = time_diff / m->cir_period;
	m->time += n_periods * m->cir_period;

	tc = m->tc + n_periods * m->cir_bytes_per_period;
	if (tc > m->cbs)
		tc = m->cbs;

	te = m->te + n_periods * m->cir_bytes_per_period;
	if (te > m->ebs)
		te = m->ebs;

	/* Color logic */
	if ((pkt_color == e_RTE_METER_GREEN) && (tc >= pkt_len)) {
		m->tc = tc - pkt_len;
		m->te = te;
		return e_RTE_METER_GREEN;
	}

	if ((pkt_color != e_RTE_METER_RED) && (te >= pkt_len)) {
		m->tc = tc;
		m->te = te - pkt_len;
		return e_RTE_METER_YELLOW;
	}

	m->tc = tc;
	m->te = te;
	return e_RTE_METER_RED;
}

static inline enum rte_meter_color
rte_meter_trtcm_color_blind_check(struct rte_meter_trtcm *m,
	uint64_t time,
	uint32_t pkt_len)
{
	uint64_t time_diff_tc, time_diff_tp, n_periods_tc, n_periods_tp, tc, tp;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_tp = time - m->time_tp;
	n_periods_tc = time_diff_tc / m->cir_period;
	n_periods_tp = time_diff_tp / m->pir_period;
	m->time_tc += n_periods_tc * m->cir_period;
	m->time_tp += n_periods_tp * m->pir_period;

	tc = m->tc + n_periods_tc * m->cir_bytes_per_period;
	if (tc > m->cbs)
		tc = m->cbs;

	tp = m->tp + n_periods_tp * m->pir_bytes_per_period;
	if (tp > m->pbs)
		tp = m->pbs;

	/* Color logic */
	if (tp < pkt_len) {
		m->tc = tc;
		m->tp = tp;
		return e_RTE_METER_RED;
	}

	if (tc < pkt_len) {
		m->tc = tc;
		m->tp = tp - pkt_len;
		return e_RTE_METER_YELLOW;
	}

	m->tc = tc - pkt_len;
	m->tp = tp - pkt_len;
	return e_RTE_METER_GREEN;
}

static inline enum rte_meter_color
rte_meter_trtcm_color_aware_check(struct rte_meter_trtcm *m,
	uint64_t time,
	uint32_t pkt_len,
	enum rte_meter_color pkt_color)
{
	uint64_t time_diff_tc, time_diff_tp, n_periods_tc, n_periods_tp, tc, tp;

	/* Bucket update */
	time_diff_tc = time - m->time_tc;
	time_diff_tp = time - m->time_tp;
	n_periods_tc = time_diff_tc / m->cir_period;
	n_periods_tp = time_diff_tp / m->pir_period;
	m->time_tc += n_periods_tc * m->cir_period;
	m->time_tp += n_periods_tp * m->pir_period;

	tc = m->tc + n_periods_tc * m->cir_bytes_per_period;
	if (tc > m->cbs)
		tc = m->cbs;

	tp = m->tp + n_periods_tp * m->pir_bytes_per_period;
	if (tp > m->pbs)
		tp = m->pbs;

	/* Color logic */
	if ((pkt_color == e_RTE_METER_RED) || (tp < pkt_len)) {
		m->tc = tc;
		m->tp = tp;
		return e_RTE_METER_RED;
	}

	if ((pkt_color == e_RTE_METER_YELLOW) || (tc < pkt_len)) {
		m->tc = tc;
		m->tp = tp - pkt_len;
		return e_RTE_METER_YELLOW;
	}

	m->tc = tc - pkt_len;
	m->tp = tp - pkt_len;
	return e_RTE_METER_GREEN;
}

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_METER_H__ */
