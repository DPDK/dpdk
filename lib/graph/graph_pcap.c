/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_pcapng.h>

#include "rte_graph_worker.h"

#include "graph_pcap_private.h"

#define GRAPH_PCAP_BUF_SZ	128
#define GRAPH_PCAP_NUM_PACKETS	1024
#define GRAPH_PCAP_PKT_POOL	"graph_pcap_pkt_pool"
#define GRAPH_PCAP_FILE_NAME	"dpdk_graph_pcap_capture_XXXXXX.pcapng"

/* For multi-process, packets are captured in separate files. */
static rte_pcapng_t *pcapng_fd;
static bool pcap_enable;
struct rte_mempool *pkt_mp;

void
graph_pcap_enable(bool val)
{
	pcap_enable = val;
}

int
graph_pcap_is_enable(void)
{
	return pcap_enable;
}

void
graph_pcap_exit(struct rte_graph *graph)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_mempool_free(pkt_mp);

	if (pcapng_fd) {
		rte_pcapng_close(pcapng_fd);
		pcapng_fd = NULL;
	}

	/* Disable pcap. */
	graph->pcap_enable = 0;
	graph_pcap_enable(0);
}

static int
graph_pcap_default_path_get(char **dir_path)
{
	struct passwd *pwd;
	char *home_dir;

	/* First check for shell environment variable */
	home_dir = getenv("HOME");
	if (home_dir == NULL) {
		graph_warn("Home env not preset.");
		/* Fallback to password file entry */
		pwd = getpwuid(getuid());
		if (pwd == NULL)
			return -EINVAL;

		home_dir = pwd->pw_dir;
	}

	/* Append default pcap file to directory */
	if (asprintf(dir_path, "%s/%s", home_dir, GRAPH_PCAP_FILE_NAME) == -1)
		return -ENOMEM;

	return 0;
}

int
graph_pcap_file_open(const char *filename)
{
	int fd, ret;
	uint16_t portid;
	char file_name[RTE_GRAPH_PCAP_FILE_SZ];
	char *pcap_dir;

	if (pcapng_fd)
		goto done;

	if (!filename || filename[0] == '\0') {
		if (graph_pcap_default_path_get(&pcap_dir) < 0)
			return -1;
		snprintf(file_name, RTE_GRAPH_PCAP_FILE_SZ, "%s", pcap_dir);
		free(pcap_dir);
	} else {
		snprintf(file_name, RTE_GRAPH_PCAP_FILE_SZ, "%s_XXXXXX.pcapng",
			 filename);
	}

	fd = mkstemps(file_name, strlen(".pcapng"));
	if (fd < 0) {
		graph_err("mkstemps() failure");
		return -1;
	}

	graph_info("pcap filename: %s", file_name);

	/* Open a capture file */
	pcapng_fd = rte_pcapng_fdopen(fd, NULL, NULL, "Graph pcap tracer",
				      NULL);
	if (pcapng_fd == NULL) {
		graph_err("Graph rte_pcapng_fdopen failed.");
		goto error;
	}

	/* Add the configured interfaces as possible capture ports */
	RTE_ETH_FOREACH_DEV(portid) {
		ret = rte_pcapng_add_interface(pcapng_fd, portid,
					       NULL, NULL, NULL);
		if (ret < 0) {
			graph_err("Graph rte_pcapng_add_interface port %u failed: %d",
				  portid, ret);
			goto error;
		}
	}

done:
	return 0;
error:
	if (pcapng_fd != NULL) {
		rte_pcapng_close(pcapng_fd);
		pcapng_fd = NULL;
	}
	close(fd);
	return -1;
}

int
graph_pcap_mp_init(void)
{
	pkt_mp = rte_mempool_lookup(GRAPH_PCAP_PKT_POOL);
	if (pkt_mp)
		goto done;

	/* Make a pool for cloned packets */
	pkt_mp = rte_pktmbuf_pool_create_by_ops(GRAPH_PCAP_PKT_POOL,
			IOV_MAX + RTE_GRAPH_BURST_SIZE,	0, 0,
			rte_pcapng_mbuf_size(RTE_MBUF_DEFAULT_BUF_SIZE),
			SOCKET_ID_ANY, "ring_mp_mc");
	if (pkt_mp == NULL) {
		graph_err("Cannot create mempool for graph pcap capture.");
		return -1;
	}

done:
	return 0;
}

int
graph_pcap_init(struct graph *graph)
{
	struct rte_graph *graph_data = graph->graph;

	if (graph_pcap_file_open(graph->pcap_filename) < 0)
		goto error;

	if (graph_pcap_mp_init() < 0)
		goto error;

	/* User configured number of packets to capture. */
	if (graph->num_pkt_to_capture)
		graph_data->nb_pkt_to_capture = graph->num_pkt_to_capture;
	else
		graph_data->nb_pkt_to_capture = GRAPH_PCAP_NUM_PACKETS;

	/* All good. Now populate data for secondary process. */
	rte_strscpy(graph_data->pcap_filename, graph->pcap_filename, RTE_GRAPH_PCAP_FILE_SZ);
	graph_data->pcap_enable = 1;

	return 0;

error:
	graph_pcap_exit(graph_data);
	graph_pcap_enable(0);
	graph_err("Graph pcap initialization failed. Disabling pcap trace.");
	return -1;
}

uint16_t
graph_pcap_dispatch(struct rte_graph *graph,
			      struct rte_node *node, void **objs,
			      uint16_t nb_objs)
{
	struct rte_mbuf *mbuf_clones[RTE_GRAPH_BURST_SIZE];
	char buffer[GRAPH_PCAP_BUF_SZ];
	uint64_t i, num_packets;
	struct rte_mbuf *mbuf;
	ssize_t len;

	if (!nb_objs || (graph->nb_pkt_captured >= graph->nb_pkt_to_capture))
		goto done;

	num_packets = graph->nb_pkt_to_capture - graph->nb_pkt_captured;
	/* nb_objs will never be greater than RTE_GRAPH_BURST_SIZE */
	if (num_packets > nb_objs)
		num_packets = nb_objs;

	snprintf(buffer, GRAPH_PCAP_BUF_SZ, "%s: %s", graph->name, node->name);

	for (i = 0; i < num_packets; i++) {
		struct rte_mbuf *mc;
		mbuf = (struct rte_mbuf *)objs[i];

		mc = rte_pcapng_copy(mbuf->port, 0, mbuf, pkt_mp, mbuf->pkt_len,
				     0, buffer);
		if (mc == NULL)
			break;

		mbuf_clones[i] = mc;
	}

	/* write it to capture file */
	len = rte_pcapng_write_packets(pcapng_fd, mbuf_clones, i);
	rte_pktmbuf_free_bulk(mbuf_clones, i);
	if (len <= 0)
		goto done;

	graph->nb_pkt_captured += i;

done:
	return node->original_process(graph, node, objs, nb_objs);
}
