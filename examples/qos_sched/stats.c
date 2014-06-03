/*-
 *  *   BSD LICENSE
 *  *
 *  *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *  *   All rights reserved.
 *  *
 *  *   Redistribution and use in source and binary forms, with or without
 *  *   modification, are permitted provided that the following conditions
 *  *   are met:
 *  *
 *  *     * Redistributions of source code must retain the above copyright
 *  *       notice, this list of conditions and the following disclaimer.
 *  *     * Redistributions in binary form must reproduce the above copyright
 *  *       notice, this list of conditions and the following disclaimer in
 *  *       the documentation and/or other materials provided with the
 *  *       distribution.
 *  *     * Neither the name of Intel Corporation nor the names of its
 *  *       contributors may be used to endorse or promote products derived
 *  *       from this software without specific prior written permission.
 *  *
 *  *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *   */

#include <unistd.h>
#include <string.h>

#include "main.h"

int
qavg_q(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id, uint8_t tc, uint8_t q)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint32_t queue_id, count, i;
        uint32_t average;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port || pipe_id >= port_params.n_pipes_per_subport
                        || tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE || q >= RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS)
                return -1;

        port = qos_conf[i].sched_port;

        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + pipe_id);
        queue_id = queue_id + (tc * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + q);

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
qavg_tcpipe(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id, uint8_t tc)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint32_t queue_id, count, i;
        uint32_t average, part_average;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port || pipe_id >= port_params.n_pipes_per_subport
                        || tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
                return -1;

        port = qos_conf[i].sched_port;

        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + pipe_id);

        average = 0;

        for (count = 0; count < qavg_ntimes; count++) {
                part_average = 0;
                for (i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
                        rte_sched_queue_read_stats(port, queue_id + (tc * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + i), &stats, &qlen);
                        part_average += qlen;
                }
                average += part_average / RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS;
                usleep(qavg_period);
        }

        average /= qavg_ntimes;

        printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

        return 0;
}

int
qavg_pipe(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint32_t queue_id, count, i;
        uint32_t average, part_average;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port || pipe_id >= port_params.n_pipes_per_subport)
                return -1;

        port = qos_conf[i].sched_port;

        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + pipe_id);

        average = 0;

        for (count = 0; count < qavg_ntimes; count++) {
                part_average = 0;
                for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
                        rte_sched_queue_read_stats(port, queue_id + i, &stats, &qlen);
                        part_average += qlen;
                }
                average += part_average / (RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS);
                usleep(qavg_period);
        }

        average /= qavg_ntimes;

        printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

        return 0;
}

int
qavg_tcsubport(uint8_t port_id, uint32_t subport_id, uint8_t tc)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint32_t queue_id, count, i, j;
        uint32_t average, part_average;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port || tc >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
                return -1;

        port = qos_conf[i].sched_port;

        average = 0;

        for (count = 0; count < qavg_ntimes; count++) {
                part_average = 0;
                for (i = 0; i < port_params.n_pipes_per_subport; i++) {
                        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + i);

                        for (j = 0; j < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; j++) {
                                rte_sched_queue_read_stats(port, queue_id + (tc * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + j), &stats, &qlen);
                                part_average += qlen;
                        }
                }

                average += part_average / (port_params.n_pipes_per_subport * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS);
                usleep(qavg_period);
        }

        average /= qavg_ntimes;

        printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

        return 0;
}

int
qavg_subport(uint8_t port_id, uint32_t subport_id)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint32_t queue_id, count, i, j;
        uint32_t average, part_average;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port)
                return -1;

        port = qos_conf[i].sched_port;

        average = 0;

        for (count = 0; count < qavg_ntimes; count++) {
                part_average = 0;
                for (i = 0; i < port_params.n_pipes_per_subport; i++) {
                        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + i);

                        for (j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; j++) {
                                rte_sched_queue_read_stats(port, queue_id + j, &stats, &qlen);
                                part_average += qlen;
                        }
                }

                average += part_average / (port_params.n_pipes_per_subport * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS);
                usleep(qavg_period);
        }

        average /= qavg_ntimes;

        printf("\nAverage queue size: %" PRIu32 " bytes.\n\n", average);

        return 0;
}

int
subport_stat(uint8_t port_id, uint32_t subport_id)
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
	memset (tc_ov, 0, sizeof(tc_ov));

        rte_sched_subport_read_stats(port, subport_id, &stats, tc_ov);

        printf("\n");
        printf("+----+-------------+-------------+-------------+-------------+-------------+\n");
        printf("| TC |   Pkts OK   |Pkts Dropped |  Bytes OK   |Bytes Dropped|  OV Status  |\n");
        printf("+----+-------------+-------------+-------------+-------------+-------------+\n");

        for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
                printf("|  %d | %11" PRIu32 " | %11" PRIu32 " | %11" PRIu32 " | %11" PRIu32 " | %11" PRIu32 " |\n", i,
                                stats.n_pkts_tc[i], stats.n_pkts_tc_dropped[i],
                                stats.n_bytes_tc[i], stats.n_bytes_tc_dropped[i], tc_ov[i]);
                printf("+----+-------------+-------------+-------------+-------------+-------------+\n");
        }
        printf("\n");

        return 0;
}

int
pipe_stat(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id)
{
        struct rte_sched_queue_stats stats;
        struct rte_sched_port *port;
        uint16_t qlen;
        uint8_t i, j;
        uint32_t queue_id;

        for (i = 0; i < nb_pfc; i++) {
                if (qos_conf[i].tx_port == port_id)
                        break;
        }
        if (i == nb_pfc || subport_id >= port_params.n_subports_per_port || pipe_id >= port_params.n_pipes_per_subport)
                return -1;

        port = qos_conf[i].sched_port;

        queue_id = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS * (subport_id * port_params.n_pipes_per_subport + pipe_id);

        printf("\n");
        printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
        printf("| TC | Queue |   Pkts OK   |Pkts Dropped |  Bytes OK   |Bytes Dropped|    Length   |\n");
        printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");

        for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
                for (j = 0; j < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; j++) {

                        rte_sched_queue_read_stats(port, queue_id + (i * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + j), &stats, &qlen);

                        printf("|  %d |   %d   | %11" PRIu32 " | %11" PRIu32 " | %11" PRIu32 " | %11" PRIu32 " | %11i |\n", i, j,
                                        stats.n_pkts, stats.n_pkts_dropped, stats.n_bytes, stats.n_bytes_dropped, qlen);
                        printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
                }
                if (i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE - 1)
                        printf("+----+-------+-------------+-------------+-------------+-------------+-------------+\n");
        }
        printf("\n");

        return 0;
}

