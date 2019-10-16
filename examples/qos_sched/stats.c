/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <unistd.h>
#include <string.h>

#include "main.h"

int
qavg_q(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id, uint8_t tc,
		uint8_t q)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint32_t count, i, queue_id = 0;
	uint32_t average;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc ||
		subport_id >= port_params.n_subports_per_port ||
		pipe_id >= subport_params[subport_id].n_pipes_per_subport_enabled  ||
		tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE ||
		q >= RTE_SCHED_BE_QUEUES_PER_PIPE ||
		(tc < RTE_SCHED_TRAFFIC_CLASS_BE && q > 0))
		return -1;

	port = qos_conf[i].sched_port;
	for (i = 0; i < subport_id; i++)
		queue_id += subport_params[i].n_pipes_per_subport_enabled *
				RTE_SCHED_QUEUES_PER_PIPE;
	if (tc < RTE_SCHED_TRAFFIC_CLASS_BE)
		queue_id += pipe_id * RTE_SCHED_QUEUES_PER_PIPE + tc;
	else
		queue_id += pipe_id * RTE_SCHED_QUEUES_PER_PIPE + tc + q;

	average = 0;
	for (count = 0; count < qavg_ntimes; count++) {
		rte_sched_queue_read_stats(port, queue_id, &stats, &qlen);
		average += qlen;
		usleep(qavg_period);
	}

	average /= qavg_ntimes;

	printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

	return 0;
}

int
qavg_tcpipe(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id,
		uint8_t tc)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint32_t count, i, queue_id = 0;
	uint32_t average, part_average;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc || subport_id >= port_params.n_subports_per_port ||
		pipe_id >= subport_params[subport_id].n_pipes_per_subport_enabled ||
		tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
		return -1;

	port = qos_conf[i].sched_port;

	for (i = 0; i < subport_id; i++)
		queue_id +=
			subport_params[i].n_pipes_per_subport_enabled *
			RTE_SCHED_QUEUES_PER_PIPE;

	queue_id += pipe_id * RTE_SCHED_QUEUES_PER_PIPE + tc;

	average = 0;

	for (count = 0; count < qavg_ntimes; count++) {
		part_average = 0;

		if (tc < RTE_SCHED_TRAFFIC_CLASS_BE) {
			rte_sched_queue_read_stats(port, queue_id,
				&stats, &qlen);
			part_average += qlen;
		} else {
			for (i = 0; i < RTE_SCHED_BE_QUEUES_PER_PIPE; i++) {
				rte_sched_queue_read_stats(port, queue_id + i,
					&stats, &qlen);
				part_average += qlen;
			}
			average += part_average / RTE_SCHED_BE_QUEUES_PER_PIPE;
		}
		usleep(qavg_period);
	}

	average /= qavg_ntimes;

	printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

	return 0;
}

int
qavg_pipe(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint32_t count, i, queue_id = 0;
	uint32_t average, part_average;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc ||
		subport_id >= port_params.n_subports_per_port ||
		pipe_id >= subport_params[subport_id].n_pipes_per_subport_enabled)
		return -1;

	port = qos_conf[i].sched_port;

	for (i = 0; i < subport_id; i++)
		queue_id += subport_params[i].n_pipes_per_subport_enabled *
				RTE_SCHED_QUEUES_PER_PIPE;

	queue_id += pipe_id * RTE_SCHED_QUEUES_PER_PIPE;

	average = 0;

	for (count = 0; count < qavg_ntimes; count++) {
		part_average = 0;
		for (i = 0; i < RTE_SCHED_QUEUES_PER_PIPE; i++) {
			rte_sched_queue_read_stats(port, queue_id + i,
				&stats, &qlen);
			part_average += qlen;
		}
		average += part_average / RTE_SCHED_QUEUES_PER_PIPE;
		usleep(qavg_period);
	}

	average /= qavg_ntimes;

	printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

	return 0;
}

int
qavg_tcsubport(uint16_t port_id, uint32_t subport_id, uint8_t tc)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint32_t queue_id, count, i, j, subport_queue_id = 0;
	uint32_t average, part_average;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc ||
		subport_id >= port_params.n_subports_per_port ||
		tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
		return -1;

	port = qos_conf[i].sched_port;

	for (i = 0; i < subport_id; i++)
		subport_queue_id +=
			subport_params[i].n_pipes_per_subport_enabled *
			RTE_SCHED_QUEUES_PER_PIPE;

	average = 0;

	for (count = 0; count < qavg_ntimes; count++) {
		uint32_t n_pipes_per_subport =
			subport_params[subport_id].n_pipes_per_subport_enabled;

		part_average = 0;
		for (i = 0; i < n_pipes_per_subport; i++) {
			if (tc < RTE_SCHED_TRAFFIC_CLASS_BE) {
				queue_id = subport_queue_id +
					i * RTE_SCHED_QUEUES_PER_PIPE + tc;
				rte_sched_queue_read_stats(port, queue_id,
					&stats, &qlen);
				part_average += qlen;
			} else {
				for (j = 0; j < RTE_SCHED_BE_QUEUES_PER_PIPE; j++) {
					queue_id = subport_queue_id +
							i * RTE_SCHED_QUEUES_PER_PIPE +
							tc + j;
					rte_sched_queue_read_stats(port, queue_id,
						&stats, &qlen);
					part_average += qlen;
				}
			}
		}

		if (tc < RTE_SCHED_TRAFFIC_CLASS_BE)
			average += part_average /
				(subport_params[subport_id].n_pipes_per_subport_enabled);
		else
			average += part_average /
				(subport_params[subport_id].n_pipes_per_subport_enabled) *
				RTE_SCHED_BE_QUEUES_PER_PIPE;

		usleep(qavg_period);
	}

	average /= qavg_ntimes;

	printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

	return 0;
}

int
qavg_subport(uint16_t port_id, uint32_t subport_id)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint32_t queue_id, count, i, j, subport_queue_id = 0;
	uint32_t average, part_average;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc ||
		subport_id >= port_params.n_subports_per_port)
		return -1;

	port = qos_conf[i].sched_port;

	for (i = 0; i < subport_id; i++)
		subport_queue_id += subport_params[i].n_pipes_per_subport_enabled *
			RTE_SCHED_QUEUES_PER_PIPE;

	average = 0;

	for (count = 0; count < qavg_ntimes; count++) {
		uint32_t n_pipes_per_subport =
			subport_params[subport_id].n_pipes_per_subport_enabled;

		part_average = 0;
		for (i = 0; i < n_pipes_per_subport; i++) {
			queue_id = subport_queue_id + i * RTE_SCHED_QUEUES_PER_PIPE;

			for (j = 0; j < RTE_SCHED_QUEUES_PER_PIPE; j++) {
				rte_sched_queue_read_stats(port, queue_id + j,
					&stats, &qlen);
				part_average += qlen;
			}
		}

		average += part_average /
			(subport_params[subport_id].n_pipes_per_subport_enabled *
			RTE_SCHED_QUEUES_PER_PIPE);
		usleep(qavg_period);
	}

	average /= qavg_ntimes;

	printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

	return 0;
}

int
subport_stat(uint16_t port_id, uint32_t subport_id)
{
	struct rte_sched_subport_stats stats;
	struct rte_sched_port *port;
	uint32_t tc_ov[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	uint8_t i;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc || subport_id >= port_params.n_subports_per_port)
		return -1;

	port = qos_conf[i].sched_port;
	memset(tc_ov, 0, sizeof(tc_ov));

	rte_sched_subport_read_stats(port, subport_id, &stats, tc_ov);

	printf("\n");
	printf("+----+-------------+-------------+-------------+-------------+-------------+\n");
	printf("| TC |   Pkts OK   |Pkts Dropped |  Bytes OK   |Bytes Dropped|  OV Status  |\n");
	printf("+----+-------------+-------------+-------------+-------------+-------------+\n");

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
		printf("|  %d | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu32 " |\n",
			i, stats.n_pkts_tc[i], stats.n_pkts_tc_dropped[i],
		stats.n_bytes_tc[i], stats.n_bytes_tc_dropped[i], tc_ov[i]);
		printf("+----+-------------+-------------+-------------+-------------+-------------+\n");
	}
	printf("\n");

	return 0;
}

int
pipe_stat(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id)
{
	struct rte_sched_queue_stats stats;
	struct rte_sched_port *port;
	uint16_t qlen;
	uint8_t i, j;
	uint32_t queue_id = 0;

	for (i = 0; i < nb_pfc; i++) {
		if (qos_conf[i].tx_port == port_id)
			break;
	}

	if (i == nb_pfc ||
		subport_id >= port_params.n_subports_per_port ||
		pipe_id >= subport_params[subport_id].n_pipes_per_subport_enabled)
		return -1;

	port = qos_conf[i].sched_port;
	for (i = 0; i < subport_id; i++)
		queue_id += subport_params[i].n_pipes_per_subport_enabled *
			RTE_SCHED_QUEUES_PER_PIPE;

	queue_id += pipe_id * RTE_SCHED_QUEUES_PER_PIPE;

	printf("\n");
	printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
	printf("| TC | Queue |   Pkts OK   |Pkts Dropped |  Bytes OK   |Bytes Dropped|    Length   |\n");
	printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
		if (i < RTE_SCHED_TRAFFIC_CLASS_BE) {
			rte_sched_queue_read_stats(port, queue_id + i, &stats, &qlen);
			printf("|  %d |   %d   | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11i |\n",
				i, 0, stats.n_pkts, stats.n_pkts_dropped, stats.n_bytes,
				stats.n_bytes_dropped, qlen);
			printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
		} else {
			for (j = 0; j < RTE_SCHED_BE_QUEUES_PER_PIPE; j++) {
				rte_sched_queue_read_stats(port, queue_id + i + j,
					&stats, &qlen);
				printf("|  %d |   %d   | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11" PRIu64 " | %11i |\n",
					i, j, stats.n_pkts, stats.n_pkts_dropped, stats.n_bytes,
					stats.n_bytes_dropped, qlen);
				printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
			}
		}
	}
	printf("\n");

	return 0;
}
