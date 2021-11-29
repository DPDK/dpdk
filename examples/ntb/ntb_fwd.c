/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>

#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_pmd_ntb.h>
#include <rte_mbuf_pool_ops.h>

/* Per-port statistics struct */
struct ntb_port_statistics {
	uint64_t tx;
	uint64_t rx;
} __rte_cache_aligned;
/* Port 0: NTB dev, Port 1: ethdev when iofwd. */
struct ntb_port_statistics ntb_port_stats[2];

struct ntb_fwd_stream {
	uint16_t tx_port;
	uint16_t rx_port;
	uint16_t qp_id;
	uint8_t tx_ntb;  /* If ntb device is tx port. */
};

struct ntb_fwd_lcore_conf {
	uint16_t stream_id;
	uint16_t nb_stream;
	uint8_t stopped;
};

enum ntb_fwd_mode {
	FILE_TRANS = 0,
	RXONLY,
	TXONLY,
	IOFWD,
	MAX_FWD_MODE,
};
static const char *const fwd_mode_s[] = {
	"file-trans",
	"rxonly",
	"txonly",
	"iofwd",
	NULL,
};
static enum ntb_fwd_mode fwd_mode = MAX_FWD_MODE;

static struct ntb_fwd_lcore_conf fwd_lcore_conf[RTE_MAX_LCORE];
static struct ntb_fwd_stream *fwd_streams;

static struct rte_mempool *mbuf_pool;

#define NTB_DRV_NAME_LEN 7
#define MEMPOOL_CACHE_SIZE 256

static uint8_t in_test;
static uint8_t interactive = 1;
static uint16_t eth_port_id = RTE_MAX_ETHPORTS;
static uint16_t dev_id;

/* Number of queues, default set as 1 */
static uint16_t num_queues = 1;
static uint16_t ntb_buf_size = RTE_MBUF_DEFAULT_BUF_SIZE;

/* Configurable number of descriptors */
#define NTB_DEFAULT_NUM_DESCS 1024
static uint16_t nb_desc = NTB_DEFAULT_NUM_DESCS;

static uint16_t tx_free_thresh;

#define NTB_MAX_PKT_BURST 32
#define NTB_DFLT_PKT_BURST 32
static uint16_t pkt_burst = NTB_DFLT_PKT_BURST;

#define BURST_TX_RETRIES 64

static struct rte_eth_conf eth_port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

/* *** Help command with introduction. *** */
struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void
cmd_help_parsed(__rte_unused void *parsed_result,
		struct cmdline *cl,
		__rte_unused void *data)
{
	cmdline_printf(
		cl,
		"\n"
		"The following commands are currently available:\n\n"
		"Control:\n"
		"    quit                                      :"
		" Quit the application.\n"
		"\nTransmission:\n"
		"    send [path]                               :"
		" Send [path] file. Only take effect in file-trans mode\n"
		"    start                                     :"
		" Start transmissions.\n"
		"    stop                                      :"
		" Stop transmissions.\n"
		"    clear/show port stats                     :"
		" Clear/show port stats.\n"
		"    set fwd file-trans/rxonly/txonly/iofwd    :"
		" Set packet forwarding mode.\n"
	);

}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,
	.data = NULL,
	.help_str = "show help",
	.tokens = {
		(void *)&cmd_help_help,
		NULL,
	},
};

/* *** QUIT *** */
struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void
cmd_quit_parsed(__rte_unused void *parsed_result,
		struct cmdline *cl,
		__rte_unused void *data)
{
	struct ntb_fwd_lcore_conf *conf;
	uint32_t lcore_id;

	/* Stop transmission first. */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		conf = &fwd_lcore_conf[lcore_id];

		if (!conf->nb_stream)
			continue;

		if (conf->stopped)
			continue;

		conf->stopped = 1;
	}
	printf("\nWaiting for lcores to finish...\n");
	rte_eal_mp_wait_lcore();
	in_test = 0;

	/* Stop traffic and Close port. */
	rte_rawdev_stop(dev_id);
	rte_rawdev_close(dev_id);
	if (eth_port_id < RTE_MAX_ETHPORTS && fwd_mode == IOFWD) {
		rte_eth_dev_stop(eth_port_id);
		rte_eth_dev_close(eth_port_id);
	}

	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
		TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "exit application",
	.tokens = {
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* *** SEND FILE PARAMETERS *** */
struct cmd_sendfile_result {
	cmdline_fixed_string_t send_string;
	char filepath[];
};

static void
cmd_sendfile_parsed(void *parsed_result,
		    __rte_unused struct cmdline *cl,
		    __rte_unused void *data)
{
	struct cmd_sendfile_result *res = parsed_result;
	struct rte_rawdev_buf *pkts_send[NTB_MAX_PKT_BURST];
	struct rte_mbuf *mbuf_send[NTB_MAX_PKT_BURST];
	uint64_t size, count, i, j, nb_burst;
	uint16_t nb_tx, buf_size;
	unsigned int nb_pkt;
	size_t queue_id = 0;
	uint16_t retry = 0;
	uint32_t val;
	FILE *file;
	int ret;

	if (num_queues != 1) {
		printf("File transmission only supports 1 queue.\n");
		num_queues = 1;
	}

	file = fopen(res->filepath, "r");
	if (file == NULL) {
		printf("Fail to open the file.\n");
		return;
	}

	if (fseek(file, 0, SEEK_END) < 0) {
		printf("Fail to get file size.\n");
		fclose(file);
		return;
	}
	size = ftell(file);
	if (fseek(file, 0, SEEK_SET) < 0) {
		printf("Fail to get file size.\n");
		fclose(file);
		return;
	}

	/* Tell remote about the file size. */
	val = size >> 32;
	rte_rawdev_set_attr(dev_id, "spad_user_0", val);
	val = size;
	rte_rawdev_set_attr(dev_id, "spad_user_1", val);
	printf("Sending file, size is %"PRIu64"\n", size);

	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		pkts_send[i] = (struct rte_rawdev_buf *)
				malloc(sizeof(struct rte_rawdev_buf));

	buf_size = ntb_buf_size - RTE_PKTMBUF_HEADROOM;
	count = (size + buf_size - 1) / buf_size;
	nb_burst = (count + pkt_burst - 1) / pkt_burst;

	for (i = 0; i < nb_burst; i++) {
		val = RTE_MIN(count, pkt_burst);
		if (rte_mempool_get_bulk(mbuf_pool, (void **)mbuf_send,
					val) == 0) {
			for (nb_pkt = 0; nb_pkt < val; nb_pkt++) {
				mbuf_send[nb_pkt]->port = dev_id;
				mbuf_send[nb_pkt]->data_len =
				fread(rte_pktmbuf_mtod(mbuf_send[nb_pkt],
					void *), 1, buf_size, file);
				mbuf_send[nb_pkt]->pkt_len =
					mbuf_send[nb_pkt]->data_len;
				pkts_send[nb_pkt]->buf_addr = mbuf_send[nb_pkt];
			}
		} else {
			for (nb_pkt = 0; nb_pkt < val; nb_pkt++) {
				mbuf_send[nb_pkt] =
					rte_mbuf_raw_alloc(mbuf_pool);
				if (mbuf_send[nb_pkt] == NULL)
					break;
				mbuf_send[nb_pkt]->port = dev_id;
				mbuf_send[nb_pkt]->data_len =
				fread(rte_pktmbuf_mtod(mbuf_send[nb_pkt],
					void *), 1, buf_size, file);
				mbuf_send[nb_pkt]->pkt_len =
					mbuf_send[nb_pkt]->data_len;
				pkts_send[nb_pkt]->buf_addr = mbuf_send[nb_pkt];
			}
		}

		ret = rte_rawdev_enqueue_buffers(dev_id, pkts_send, nb_pkt,
						(void *)queue_id);
		if (ret < 0) {
			printf("Enqueue failed with err %d\n", ret);
			for (j = 0; j < nb_pkt; j++)
				rte_pktmbuf_free(mbuf_send[j]);
			goto clean;
		}
		nb_tx = ret;
		while (nb_tx != nb_pkt && retry < BURST_TX_RETRIES) {
			rte_delay_us(1);
			ret = rte_rawdev_enqueue_buffers(dev_id,
					&pkts_send[nb_tx], nb_pkt - nb_tx,
					(void *)queue_id);
			if (ret < 0) {
				printf("Enqueue failed with err %d\n", ret);
				for (j = nb_tx; j < nb_pkt; j++)
					rte_pktmbuf_free(mbuf_send[j]);
				goto clean;
			}
			nb_tx += ret;
		}
		count -= nb_pkt;
	}

	/* Clear register after file sending done. */
	rte_rawdev_set_attr(dev_id, "spad_user_0", 0);
	rte_rawdev_set_attr(dev_id, "spad_user_1", 0);
	printf("Done sending file.\n");

clean:
	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		free(pkts_send[i]);
	fclose(file);
}

cmdline_parse_token_string_t cmd_send_file_send =
	TOKEN_STRING_INITIALIZER(struct cmd_sendfile_result, send_string,
				 "send");
cmdline_parse_token_string_t cmd_send_file_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_sendfile_result, filepath, NULL);


cmdline_parse_inst_t cmd_send_file = {
	.f = cmd_sendfile_parsed,
	.data = NULL,
	.help_str = "send <file_path>",
	.tokens = {
		(void *)&cmd_send_file_send,
		(void *)&cmd_send_file_filepath,
		NULL,
	},
};

#define RECV_FILE_LEN 30
static int
start_polling_recv_file(void *param)
{
	struct rte_rawdev_buf *pkts_recv[NTB_MAX_PKT_BURST];
	struct ntb_fwd_lcore_conf *conf = param;
	struct rte_mbuf *mbuf;
	char filepath[RECV_FILE_LEN];
	uint64_t val, size, file_len;
	uint16_t nb_rx, i, file_no;
	size_t queue_id = 0;
	FILE *file;
	int ret;

	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		pkts_recv[i] = (struct rte_rawdev_buf *)
				malloc(sizeof(struct rte_rawdev_buf));

	file_no = 0;
	while (!conf->stopped) {
		snprintf(filepath, RECV_FILE_LEN, "ntb_recv_file%d", file_no);
		file = fopen(filepath, "w");
		if (file == NULL) {
			printf("Fail to open the file.\n");
			return -EINVAL;
		}

		rte_rawdev_get_attr(dev_id, "spad_user_0", &val);
		size = val << 32;
		rte_rawdev_get_attr(dev_id, "spad_user_1", &val);
		size |= val;

		if (!size) {
			fclose(file);
			continue;
		}

		file_len = 0;
		nb_rx = NTB_MAX_PKT_BURST;
		while (file_len < size && !conf->stopped) {
			ret = rte_rawdev_dequeue_buffers(dev_id, pkts_recv,
					pkt_burst, (void *)queue_id);
			if (ret < 0) {
				printf("Dequeue failed with err %d\n", ret);
				fclose(file);
				goto clean;
			}
			nb_rx = ret;
			ntb_port_stats[0].rx += nb_rx;
			for (i = 0; i < nb_rx; i++) {
				mbuf = pkts_recv[i]->buf_addr;
				fwrite(rte_pktmbuf_mtod(mbuf, void *), 1,
					mbuf->data_len, file);
				file_len += mbuf->data_len;
				rte_pktmbuf_free(mbuf);
				pkts_recv[i]->buf_addr = NULL;
			}
		}

		printf("Received file (size: %" PRIu64 ") from peer to %s.\n",
			size, filepath);
		fclose(file);
		file_no++;
	}

clean:
	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		free(pkts_recv[i]);
	return 0;
}

static int
start_iofwd_per_lcore(void *param)
{
	struct rte_rawdev_buf *ntb_buf[NTB_MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst[NTB_MAX_PKT_BURST];
	struct ntb_fwd_lcore_conf *conf = param;
	struct ntb_fwd_stream fs;
	uint16_t nb_rx, nb_tx;
	int i, j, ret;

	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		ntb_buf[i] = (struct rte_rawdev_buf *)
			     malloc(sizeof(struct rte_rawdev_buf));

	while (!conf->stopped) {
		for (i = 0; i < conf->nb_stream; i++) {
			fs = fwd_streams[conf->stream_id + i];
			if (fs.tx_ntb) {
				nb_rx = rte_eth_rx_burst(fs.rx_port,
						fs.qp_id, pkts_burst,
						pkt_burst);
				if (unlikely(nb_rx == 0))
					continue;
				for (j = 0; j < nb_rx; j++)
					ntb_buf[j]->buf_addr = pkts_burst[j];
				ret = rte_rawdev_enqueue_buffers(fs.tx_port,
						ntb_buf, nb_rx,
						(void *)(size_t)fs.qp_id);
				if (ret < 0) {
					printf("Enqueue failed with err %d\n",
						ret);
					for (j = 0; j < nb_rx; j++)
						rte_pktmbuf_free(pkts_burst[j]);
					goto clean;
				}
				nb_tx = ret;
				ntb_port_stats[0].tx += nb_tx;
				ntb_port_stats[1].rx += nb_rx;
			} else {
				ret = rte_rawdev_dequeue_buffers(fs.rx_port,
						ntb_buf, pkt_burst,
						(void *)(size_t)fs.qp_id);
				if (ret < 0) {
					printf("Dequeue failed with err %d\n",
						ret);
					goto clean;
				}
				nb_rx = ret;
				if (unlikely(nb_rx == 0))
					continue;
				for (j = 0; j < nb_rx; j++)
					pkts_burst[j] = ntb_buf[j]->buf_addr;
				nb_tx = rte_eth_tx_burst(fs.tx_port,
					fs.qp_id, pkts_burst, nb_rx);
				ntb_port_stats[1].tx += nb_tx;
				ntb_port_stats[0].rx += nb_rx;
			}
			if (unlikely(nb_tx < nb_rx)) {
				do {
					rte_pktmbuf_free(pkts_burst[nb_tx]);
				} while (++nb_tx < nb_rx);
			}
		}
	}

clean:
	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		free(ntb_buf[i]);

	return 0;
}

static int
start_rxonly_per_lcore(void *param)
{
	struct rte_rawdev_buf *ntb_buf[NTB_MAX_PKT_BURST];
	struct ntb_fwd_lcore_conf *conf = param;
	struct ntb_fwd_stream fs;
	uint16_t nb_rx;
	int i, j, ret;

	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		ntb_buf[i] = (struct rte_rawdev_buf *)
			     malloc(sizeof(struct rte_rawdev_buf));

	while (!conf->stopped) {
		for (i = 0; i < conf->nb_stream; i++) {
			fs = fwd_streams[conf->stream_id + i];
			ret = rte_rawdev_dequeue_buffers(fs.rx_port,
			      ntb_buf, pkt_burst, (void *)(size_t)fs.qp_id);
			if (ret < 0) {
				printf("Dequeue failed with err %d\n", ret);
				goto clean;
			}
			nb_rx = ret;
			if (unlikely(nb_rx == 0))
				continue;
			ntb_port_stats[0].rx += nb_rx;

			for (j = 0; j < nb_rx; j++)
				rte_pktmbuf_free(ntb_buf[j]->buf_addr);
		}
	}

clean:
	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		free(ntb_buf[i]);

	return 0;
}


static int
start_txonly_per_lcore(void *param)
{
	struct rte_rawdev_buf *ntb_buf[NTB_MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst[NTB_MAX_PKT_BURST];
	struct ntb_fwd_lcore_conf *conf = param;
	struct ntb_fwd_stream fs;
	uint16_t nb_pkt, nb_tx;
	int i, j, ret;

	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		ntb_buf[i] = (struct rte_rawdev_buf *)
			     malloc(sizeof(struct rte_rawdev_buf));

	while (!conf->stopped) {
		for (i = 0; i < conf->nb_stream; i++) {
			fs = fwd_streams[conf->stream_id + i];
			if (rte_mempool_get_bulk(mbuf_pool, (void **)pkts_burst,
				  pkt_burst) == 0) {
				for (nb_pkt = 0; nb_pkt < pkt_burst; nb_pkt++) {
					pkts_burst[nb_pkt]->port = dev_id;
					pkts_burst[nb_pkt]->data_len =
						pkts_burst[nb_pkt]->buf_len -
						RTE_PKTMBUF_HEADROOM;
					pkts_burst[nb_pkt]->pkt_len =
						pkts_burst[nb_pkt]->data_len;
					ntb_buf[nb_pkt]->buf_addr =
						pkts_burst[nb_pkt];
				}
			} else {
				for (nb_pkt = 0; nb_pkt < pkt_burst; nb_pkt++) {
					pkts_burst[nb_pkt] =
						rte_pktmbuf_alloc(mbuf_pool);
					if (pkts_burst[nb_pkt] == NULL)
						break;
					pkts_burst[nb_pkt]->port = dev_id;
					pkts_burst[nb_pkt]->data_len =
						pkts_burst[nb_pkt]->buf_len -
						RTE_PKTMBUF_HEADROOM;
					pkts_burst[nb_pkt]->pkt_len =
						pkts_burst[nb_pkt]->data_len;
					ntb_buf[nb_pkt]->buf_addr =
						pkts_burst[nb_pkt];
				}
			}
			ret = rte_rawdev_enqueue_buffers(fs.tx_port, ntb_buf,
					nb_pkt, (void *)(size_t)fs.qp_id);
			if (ret < 0) {
				printf("Enqueue failed with err %d\n", ret);
				for (j = 0; j < nb_pkt; j++)
					rte_pktmbuf_free(pkts_burst[j]);
				goto clean;
			}
			nb_tx = ret;
			ntb_port_stats[0].tx += nb_tx;
			if (unlikely(nb_tx < nb_pkt)) {
				do {
					rte_pktmbuf_free(pkts_burst[nb_tx]);
				} while (++nb_tx < nb_pkt);
			}
		}
	}

clean:
	for (i = 0; i < NTB_MAX_PKT_BURST; i++)
		free(ntb_buf[i]);

	return 0;
}

static int
ntb_fwd_config_setup(void)
{
	uint16_t i;

	/* Make sure iofwd has valid ethdev. */
	if (fwd_mode == IOFWD && eth_port_id >= RTE_MAX_ETHPORTS) {
		printf("No ethdev, cannot be in iofwd mode.");
		return -EINVAL;
	}

	if (fwd_mode == IOFWD) {
		fwd_streams = rte_zmalloc("ntb_fwd: fwd_streams",
			sizeof(struct ntb_fwd_stream) * num_queues * 2,
			RTE_CACHE_LINE_SIZE);
		for (i = 0; i < num_queues; i++) {
			fwd_streams[i * 2].qp_id = i;
			fwd_streams[i * 2].tx_port = dev_id;
			fwd_streams[i * 2].rx_port = eth_port_id;
			fwd_streams[i * 2].tx_ntb = 1;

			fwd_streams[i * 2 + 1].qp_id = i;
			fwd_streams[i * 2 + 1].tx_port = eth_port_id;
			fwd_streams[i * 2 + 1].rx_port = dev_id;
			fwd_streams[i * 2 + 1].tx_ntb = 0;
		}
		return 0;
	}

	if (fwd_mode == RXONLY || fwd_mode == FILE_TRANS) {
		/* Only support 1 queue in file-trans for in order. */
		if (fwd_mode == FILE_TRANS)
			num_queues = 1;

		fwd_streams = rte_zmalloc("ntb_fwd: fwd_streams",
				sizeof(struct ntb_fwd_stream) * num_queues,
				RTE_CACHE_LINE_SIZE);
		for (i = 0; i < num_queues; i++) {
			fwd_streams[i].qp_id = i;
			fwd_streams[i].tx_port = RTE_MAX_ETHPORTS;
			fwd_streams[i].rx_port = dev_id;
			fwd_streams[i].tx_ntb = 0;
		}
		return 0;
	}

	if (fwd_mode == TXONLY) {
		fwd_streams = rte_zmalloc("ntb_fwd: fwd_streams",
				sizeof(struct ntb_fwd_stream) * num_queues,
				RTE_CACHE_LINE_SIZE);
		for (i = 0; i < num_queues; i++) {
			fwd_streams[i].qp_id = i;
			fwd_streams[i].tx_port = dev_id;
			fwd_streams[i].rx_port = RTE_MAX_ETHPORTS;
			fwd_streams[i].tx_ntb = 1;
		}
	}
	return 0;
}

static void
assign_stream_to_lcores(void)
{
	struct ntb_fwd_lcore_conf *conf;
	struct ntb_fwd_stream *fs;
	uint16_t nb_streams, sm_per_lcore, sm_id, i;
	uint32_t lcore_id;
	uint8_t lcore_num, nb_extra;

	lcore_num = rte_lcore_count();
	/* Exclude main core */
	lcore_num--;

	nb_streams = (fwd_mode == IOFWD) ? num_queues * 2 : num_queues;

	sm_per_lcore = nb_streams / lcore_num;
	nb_extra = nb_streams % lcore_num;
	sm_id = 0;
	i = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		conf = &fwd_lcore_conf[lcore_id];

		if (i < nb_extra) {
			conf->nb_stream = sm_per_lcore + 1;
			conf->stream_id = sm_id;
			sm_id = sm_id + sm_per_lcore + 1;
		} else {
			conf->nb_stream = sm_per_lcore;
			conf->stream_id = sm_id;
			sm_id = sm_id + sm_per_lcore;
		}

		i++;
		if (sm_id >= nb_streams)
			break;
	}

	/* Print packet forwarding config. */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		conf = &fwd_lcore_conf[lcore_id];

		if (!conf->nb_stream)
			continue;

		printf("Streams on Lcore %u :\n", lcore_id);
		for (i = 0; i < conf->nb_stream; i++) {
			fs = &fwd_streams[conf->stream_id + i];
			if (fwd_mode == IOFWD)
				printf(" + Stream %u : %s%u RX -> %s%u TX,"
					" Q=%u\n", conf->stream_id + i,
					fs->tx_ntb ? "Eth" : "NTB", fs->rx_port,
					fs->tx_ntb ? "NTB" : "Eth", fs->tx_port,
					fs->qp_id);
			if (fwd_mode == FILE_TRANS || fwd_mode == RXONLY)
				printf(" + Stream %u : %s%u RX only\n",
					conf->stream_id, "NTB", fs->rx_port);
			if (fwd_mode == TXONLY)
				printf(" + Stream %u : %s%u TX only\n",
					conf->stream_id, "NTB", fs->tx_port);
		}
	}
}

static void
start_pkt_fwd(void)
{
	struct ntb_fwd_lcore_conf *conf;
	struct rte_eth_link eth_link;
	uint32_t lcore_id;
	int ret, i;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	ret = ntb_fwd_config_setup();
	if (ret < 0) {
		printf("Cannot start traffic. Please reset fwd mode.\n");
		return;
	}

	/* If using iofwd, checking ethdev link status first. */
	if (fwd_mode == IOFWD) {
		printf("Checking eth link status...\n");
		/* Wait for eth link up at most 100 times. */
		for (i = 0; i < 100; i++) {
			ret = rte_eth_link_get(eth_port_id, &eth_link);
			if (ret < 0) {
				printf("Link get failed with err %d\n", ret);
				return;
			}
			if (eth_link.link_status) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text),
					&eth_link);
				printf("Eth%u %s\n", eth_port_id,
				       link_status_text);
				break;
			}
		}
		if (!eth_link.link_status) {
			printf("Eth%u link down. Cannot start traffic.\n",
				eth_port_id);
			return;
		}
	}

	assign_stream_to_lcores();
	in_test = 1;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		conf = &fwd_lcore_conf[lcore_id];

		if (!conf->nb_stream)
			continue;

		conf->stopped = 0;
		if (fwd_mode == FILE_TRANS)
			rte_eal_remote_launch(start_polling_recv_file,
					      conf, lcore_id);
		else if (fwd_mode == IOFWD)
			rte_eal_remote_launch(start_iofwd_per_lcore,
					      conf, lcore_id);
		else if (fwd_mode == RXONLY)
			rte_eal_remote_launch(start_rxonly_per_lcore,
					      conf, lcore_id);
		else if (fwd_mode == TXONLY)
			rte_eal_remote_launch(start_txonly_per_lcore,
					      conf, lcore_id);
	}
}

/* *** START FWD PARAMETERS *** */
struct cmd_start_result {
	cmdline_fixed_string_t start;
};

static void
cmd_start_parsed(__rte_unused void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	start_pkt_fwd();
}

cmdline_parse_token_string_t cmd_start_start =
		TOKEN_STRING_INITIALIZER(struct cmd_start_result, start, "start");

cmdline_parse_inst_t cmd_start = {
	.f = cmd_start_parsed,
	.data = NULL,
	.help_str = "start pkt fwd between ntb and ethdev",
	.tokens = {
		(void *)&cmd_start_start,
		NULL,
	},
};

/* *** STOP *** */
struct cmd_stop_result {
	cmdline_fixed_string_t stop;
};

static void
cmd_stop_parsed(__rte_unused void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct ntb_fwd_lcore_conf *conf;
	uint32_t lcore_id;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		conf = &fwd_lcore_conf[lcore_id];

		if (!conf->nb_stream)
			continue;

		if (conf->stopped)
			continue;

		conf->stopped = 1;
	}
	printf("\nWaiting for lcores to finish...\n");
	rte_eal_mp_wait_lcore();
	in_test = 0;
	printf("\nDone.\n");
}

cmdline_parse_token_string_t cmd_stop_stop =
		TOKEN_STRING_INITIALIZER(struct cmd_stop_result, stop, "stop");

cmdline_parse_inst_t cmd_stop = {
	.f = cmd_stop_parsed,
	.data = NULL,
	.help_str = "stop: Stop packet forwarding",
	.tokens = {
		(void *)&cmd_stop_stop,
		NULL,
	},
};

static void
ntb_stats_clear(void)
{
	int nb_ids, i;
	uint32_t *ids;

	/* Clear NTB dev stats */
	nb_ids = rte_rawdev_xstats_names_get(dev_id, NULL, 0);
	if (nb_ids  < 0) {
		printf("Error: Cannot get count of xstats\n");
		return;
	}
	ids = malloc(sizeof(uint32_t) * nb_ids);
	for (i = 0; i < nb_ids; i++)
		ids[i] = i;
	rte_rawdev_xstats_reset(dev_id, ids, nb_ids);
	printf("\n  statistics for NTB port %d cleared\n", dev_id);

	/* Clear Ethdev stats if have any */
	if (fwd_mode == IOFWD && eth_port_id != RTE_MAX_ETHPORTS) {
		rte_eth_stats_reset(eth_port_id);
		printf("\n  statistics for ETH port %d cleared\n", eth_port_id);
	}
}

static inline void
ntb_calculate_throughput(uint16_t port) {
	uint64_t diff_pkts_rx, diff_pkts_tx, diff_cycles;
	uint64_t mpps_rx, mpps_tx;
	static uint64_t prev_pkts_rx[2];
	static uint64_t prev_pkts_tx[2];
	static uint64_t prev_cycles[2];

	diff_cycles = prev_cycles[port];
	prev_cycles[port] = rte_rdtsc();
	if (diff_cycles > 0)
		diff_cycles = prev_cycles[port] - diff_cycles;
	diff_pkts_rx = (ntb_port_stats[port].rx > prev_pkts_rx[port]) ?
		(ntb_port_stats[port].rx - prev_pkts_rx[port]) : 0;
	diff_pkts_tx = (ntb_port_stats[port].tx > prev_pkts_tx[port]) ?
		(ntb_port_stats[port].tx - prev_pkts_tx[port]) : 0;
	prev_pkts_rx[port] = ntb_port_stats[port].rx;
	prev_pkts_tx[port] = ntb_port_stats[port].tx;
	mpps_rx = diff_cycles > 0 ?
		diff_pkts_rx * rte_get_tsc_hz() / diff_cycles : 0;
	mpps_tx = diff_cycles > 0 ?
		diff_pkts_tx * rte_get_tsc_hz() / diff_cycles : 0;
	printf("  Throughput (since last show)\n");
	printf("  Rx-pps: %12"PRIu64"\n  Tx-pps: %12"PRIu64"\n",
			mpps_rx, mpps_tx);

}

static void
ntb_stats_display(void)
{
	struct rte_rawdev_xstats_name *xstats_names;
	struct rte_eth_stats stats;
	uint64_t *values;
	uint32_t *ids;
	int nb_ids, i;

	printf("###### statistics for NTB port %d #######\n", dev_id);

	/* Get NTB dev stats and stats names */
	nb_ids = rte_rawdev_xstats_names_get(dev_id, NULL, 0);
	if (nb_ids  < 0) {
		printf("Error: Cannot get count of xstats\n");
		return;
	}
	xstats_names = malloc(sizeof(struct rte_rawdev_xstats_name) * nb_ids);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstats lookup\n");
		return;
	}
	if (nb_ids != rte_rawdev_xstats_names_get(
			dev_id, xstats_names, nb_ids)) {
		printf("Error: Cannot get xstats lookup\n");
		free(xstats_names);
		return;
	}
	ids = malloc(sizeof(uint32_t) * nb_ids);
	for (i = 0; i < nb_ids; i++)
		ids[i] = i;
	values = malloc(sizeof(uint64_t) * nb_ids);
	if (nb_ids != rte_rawdev_xstats_get(dev_id, ids, values, nb_ids)) {
		printf("Error: Unable to get xstats\n");
		free(xstats_names);
		free(values);
		free(ids);
		return;
	}

	/* Display NTB dev stats */
	for (i = 0; i < nb_ids; i++)
		printf("  %s: %"PRIu64"\n", xstats_names[i].name, values[i]);
	ntb_calculate_throughput(0);

	/* Get Ethdev stats if have any */
	if (fwd_mode == IOFWD && eth_port_id != RTE_MAX_ETHPORTS) {
		printf("###### statistics for ETH port %d ######\n",
			eth_port_id);
		rte_eth_stats_get(eth_port_id, &stats);
		printf("  RX-packets: %"PRIu64"\n", stats.ipackets);
		printf("  RX-bytes: %"PRIu64"\n", stats.ibytes);
		printf("  RX-errors: %"PRIu64"\n", stats.ierrors);
		printf("  RX-missed: %"PRIu64"\n", stats.imissed);
		printf("  TX-packets: %"PRIu64"\n", stats.opackets);
		printf("  TX-bytes: %"PRIu64"\n", stats.obytes);
		printf("  TX-errors: %"PRIu64"\n", stats.oerrors);
		ntb_calculate_throughput(1);
	}

	free(xstats_names);
	free(values);
	free(ids);
}

/* *** SHOW/CLEAR PORT STATS *** */
struct cmd_stats_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t stats;
};

static void
cmd_stats_parsed(void *parsed_result,
		 __rte_unused struct cmdline *cl,
		 __rte_unused void *data)
{
	struct cmd_stats_result *res = parsed_result;
	if (!strcmp(res->show, "clear"))
		ntb_stats_clear();
	else
		ntb_stats_display();
}

cmdline_parse_token_string_t cmd_stats_show =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_result, show, "show#clear");
cmdline_parse_token_string_t cmd_stats_port =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_result, port, "port");
cmdline_parse_token_string_t cmd_stats_stats =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_result, stats, "stats");


cmdline_parse_inst_t cmd_stats = {
	.f = cmd_stats_parsed,
	.data = NULL,
	.help_str = "show|clear port stats",
	.tokens = {
		(void *)&cmd_stats_show,
		(void *)&cmd_stats_port,
		(void *)&cmd_stats_stats,
		NULL,
	},
};

/* *** SET FORWARDING MODE *** */
struct cmd_set_fwd_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t fwd;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_fwd_mode_parsed(__rte_unused void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_set_fwd_mode_result *res = parsed_result;
	int i;

	if (in_test) {
		printf("Please stop traffic first.\n");
		return;
	}

	for (i = 0; i < MAX_FWD_MODE; i++) {
		if (!strcmp(res->mode, fwd_mode_s[i])) {
			fwd_mode = i;
			return;
		}
	}
	printf("Invalid %s packet forwarding mode.\n", res->mode);
}

cmdline_parse_token_string_t cmd_setfwd_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, set, "set");
cmdline_parse_token_string_t cmd_setfwd_fwd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, fwd, "fwd");
cmdline_parse_token_string_t cmd_setfwd_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, mode,
				"file-trans#iofwd#txonly#rxonly");

cmdline_parse_inst_t cmd_set_fwd_mode = {
	.f = cmd_set_fwd_mode_parsed,
	.data = NULL,
	.help_str = "set forwarding mode as file-trans|rxonly|txonly|iofwd",
	.tokens = {
		(void *)&cmd_setfwd_set,
		(void *)&cmd_setfwd_fwd,
		(void *)&cmd_setfwd_mode,
		NULL,
	},
};

/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help,
	(cmdline_parse_inst_t *)&cmd_send_file,
	(cmdline_parse_inst_t *)&cmd_start,
	(cmdline_parse_inst_t *)&cmd_stop,
	(cmdline_parse_inst_t *)&cmd_stats,
	(cmdline_parse_inst_t *)&cmd_set_fwd_mode,
	(cmdline_parse_inst_t *)&cmd_quit,
	NULL,
};

/* prompt function, called from main on MAIN lcore */
static void
prompt(void)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "ntb> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\nSignal %d received, preparing to exit...\n", signum);
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}
}

#define OPT_BUF_SIZE         "buf-size"
#define OPT_FWD_MODE         "fwd-mode"
#define OPT_NB_DESC          "nb-desc"
#define OPT_TXFREET          "txfreet"
#define OPT_BURST            "burst"
#define OPT_QP               "qp"

enum {
	/* long options mapped to a short option */
	OPT_NO_ZERO_COPY_NUM = 1,
	OPT_BUF_SIZE_NUM,
	OPT_FWD_MODE_NUM,
	OPT_NB_DESC_NUM,
	OPT_TXFREET_NUM,
	OPT_BURST_NUM,
	OPT_QP_NUM,
};

static const char short_options[] =
	"i" /* interactive mode */
	;

static const struct option lgopts[] = {
	{OPT_BUF_SIZE,     1, NULL, OPT_BUF_SIZE_NUM     },
	{OPT_FWD_MODE,     1, NULL, OPT_FWD_MODE_NUM     },
	{OPT_NB_DESC,      1, NULL, OPT_NB_DESC_NUM      },
	{OPT_TXFREET,      1, NULL, OPT_TXFREET_NUM      },
	{OPT_BURST,        1, NULL, OPT_BURST_NUM        },
	{OPT_QP,           1, NULL, OPT_QP_NUM           },
	{0,                0, NULL, 0                    }
};

static void
ntb_usage(const char *prgname)
{
	printf("%s [EAL options] -- [options]\n"
	       "-i: run in interactive mode.\n"
	       "-qp=N: set number of queues as N (N > 0, default: 1).\n"
	       "--fwd-mode=N: set fwd mode (N: file-trans | rxonly | "
	       "txonly | iofwd, default: file-trans)\n"
	       "--buf-size=N: set mbuf dataroom size as N (0 < N < 65535,"
	       " default: 2048).\n"
	       "--nb-desc=N: set number of descriptors as N (%u <= N <= %u,"
	       " default: 1024).\n"
	       "--txfreet=N: set tx free thresh for NTB driver as N. (N >= 0)\n"
	       "--burst=N: set pkt burst as N (0 < N <= %u default: 32).\n",
	       prgname, NTB_MIN_DESC_SIZE, NTB_MAX_DESC_SIZE,
	       NTB_MAX_PKT_BURST);
}

static void
ntb_parse_args(int argc, char **argv)
{
	char *prgname = argv[0], **argvopt = argv;
	int opt, opt_idx, n, i;

	while ((opt = getopt_long(argc, argvopt, short_options,
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case 'i':
			printf("Interactive-mode selected.\n");
			interactive = 1;
			break;
		case OPT_QP_NUM:
			n = atoi(optarg);
			if (n > 0)
				num_queues = n;
			else
				rte_exit(EXIT_FAILURE, "q must be > 0.\n");
			break;
		case OPT_BUF_SIZE_NUM:
			n = atoi(optarg);
			if (n > RTE_PKTMBUF_HEADROOM && n <= 0xFFFF)
				ntb_buf_size = n;
			else
				rte_exit(EXIT_FAILURE, "buf-size must be > "
					"%u and < 65536.\n",
					RTE_PKTMBUF_HEADROOM);
			break;
		case OPT_FWD_MODE_NUM:
			for (i = 0; i < MAX_FWD_MODE; i++) {
				if (!strcmp(optarg, fwd_mode_s[i])) {
					fwd_mode = i;
					break;
				}
			}
			if (i == MAX_FWD_MODE)
				rte_exit(EXIT_FAILURE, "Unsupported mode. "
				"(Should be: file-trans | rxonly | txonly "
				"| iofwd)\n");
			break;
		case OPT_NB_DESC_NUM:
			n = atoi(optarg);
			if (n >= NTB_MIN_DESC_SIZE && n <= NTB_MAX_DESC_SIZE)
				nb_desc = n;
			else
				rte_exit(EXIT_FAILURE, "nb-desc must be within"
					" [%u, %u].\n", NTB_MIN_DESC_SIZE,
					NTB_MAX_DESC_SIZE);
			break;
		case OPT_TXFREET_NUM:
			n = atoi(optarg);
			if (n >= 0)
				tx_free_thresh = n;
			else
				rte_exit(EXIT_FAILURE, "txfreet must be"
					" >= 0\n");
			break;
		case OPT_BURST_NUM:
			n = atoi(optarg);
			if (n > 0 && n <= NTB_MAX_PKT_BURST)
				pkt_burst = n;
			else
				rte_exit(EXIT_FAILURE, "burst must be within "
					"(0, %u].\n", NTB_MAX_PKT_BURST);
			break;

		default:
			ntb_usage(prgname);
			rte_exit(EXIT_FAILURE,
				 "Command line is incomplete or incorrect.\n");
			break;
		}
	}
}

static void
ntb_mempool_mz_free(__rte_unused struct rte_mempool_memhdr *memhdr,
		void *opaque)
{
	const struct rte_memzone *mz = opaque;
	rte_memzone_free(mz);
}

static struct rte_mempool *
ntb_mbuf_pool_create(uint16_t mbuf_seg_size, uint32_t nb_mbuf,
		     struct ntb_dev_info ntb_info,
		     struct ntb_dev_config *ntb_conf,
		     unsigned int socket_id)
{
	size_t mz_len, total_elt_sz, max_mz_len, left_sz;
	struct rte_pktmbuf_pool_private mbp_priv;
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	struct rte_mempool *mp;
	uint64_t align;
	uint32_t mz_id;
	int ret;

	snprintf(pool_name, sizeof(pool_name), "ntb_mbuf_pool_%u", socket_id);
	mp = rte_mempool_create_empty(pool_name, nb_mbuf,
				      (mbuf_seg_size + sizeof(struct rte_mbuf)),
				      MEMPOOL_CACHE_SIZE,
				      sizeof(struct rte_pktmbuf_pool_private),
				      socket_id, 0);
	if (mp == NULL)
		return NULL;

	if (rte_mempool_set_ops_byname(mp, rte_mbuf_best_mempool_ops(), NULL)) {
		printf("error setting mempool handler\n");
		goto fail;
	}

	memset(&mbp_priv, 0, sizeof(mbp_priv));
	mbp_priv.mbuf_data_room_size = mbuf_seg_size;
	mbp_priv.mbuf_priv_size = 0;
	rte_pktmbuf_pool_init(mp, &mbp_priv);

	ntb_conf->mz_list = rte_zmalloc("ntb_memzone_list",
				sizeof(struct rte_memzone *) *
				ntb_info.mw_cnt, 0);
	if (ntb_conf->mz_list == NULL)
		goto fail;

	/* Put ntb header on mw0. */
	if (ntb_info.mw_size[0] < ntb_info.ntb_hdr_size) {
		printf("mw0 (size: %" PRIu64 ") is not enough for ntb hdr"
		       " (size: %u)\n", ntb_info.mw_size[0],
		       ntb_info.ntb_hdr_size);
		goto fail;
	}

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;
	left_sz = total_elt_sz * nb_mbuf;
	for (mz_id = 0; mz_id < ntb_info.mw_cnt; mz_id++) {
		/* If populated mbuf is enough, no need to reserve extra mz. */
		if (!left_sz)
			break;
		snprintf(mz_name, sizeof(mz_name), "ntb_mw_%d", mz_id);
		align = ntb_info.mw_size_align ? ntb_info.mw_size[mz_id] :
			RTE_CACHE_LINE_SIZE;
		/* Reserve ntb header space on memzone 0. */
		max_mz_len = mz_id ? ntb_info.mw_size[mz_id] :
			     ntb_info.mw_size[mz_id] - ntb_info.ntb_hdr_size;
		mz_len = left_sz <= max_mz_len ? left_sz :
			(max_mz_len / total_elt_sz * total_elt_sz);
		if (!mz_len)
			continue;
		mz = rte_memzone_reserve_aligned(mz_name, mz_len, socket_id,
					RTE_MEMZONE_IOVA_CONTIG, align);
		if (mz == NULL) {
			printf("Cannot allocate %" PRIu64 " aligned memzone"
				" %u\n", align, mz_id);
			goto fail;
		}
		left_sz -= mz_len;

		/* Reserve ntb header space on memzone 0. */
		if (mz_id)
			ret = rte_mempool_populate_iova(mp, mz->addr, mz->iova,
					mz->len, ntb_mempool_mz_free,
					(void *)(uintptr_t)mz);
		else
			ret = rte_mempool_populate_iova(mp,
					(void *)((size_t)mz->addr +
					ntb_info.ntb_hdr_size),
					mz->iova + ntb_info.ntb_hdr_size,
					mz->len - ntb_info.ntb_hdr_size,
					ntb_mempool_mz_free,
					(void *)(uintptr_t)mz);
		if (ret <= 0) {
			rte_memzone_free(mz);
			rte_mempool_free(mp);
			return NULL;
		}

		ntb_conf->mz_list[mz_id] = mz;
	}
	if (left_sz) {
		printf("mw space is not enough for mempool.\n");
		goto fail;
	}

	ntb_conf->mz_num = mz_id;
	rte_mempool_obj_iter(mp, rte_pktmbuf_init, NULL);

	return mp;
fail:
	rte_mempool_free(mp);
	return NULL;
}

int
main(int argc, char **argv)
{
	struct rte_eth_conf eth_pconf = eth_port_conf;
	struct rte_rawdev_info ntb_rawdev_conf;
	struct rte_rawdev_info ntb_rawdev_info;
	struct rte_eth_dev_info ethdev_info;
	struct rte_eth_rxconf eth_rx_conf;
	struct rte_eth_txconf eth_tx_conf;
	struct ntb_queue_conf ntb_q_conf;
	struct ntb_dev_config ntb_conf;
	struct ntb_dev_info ntb_info;
	uint64_t ntb_link_status;
	uint32_t nb_mbuf;
	int ret, i;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization.\n");

	if (rte_lcore_count() < 2)
		rte_exit(EXIT_FAILURE, "Need at least 2 cores\n");

	/* Find 1st ntb rawdev. */
	for (i = 0; i < RTE_RAWDEV_MAX_DEVS; i++)
		if (rte_rawdevs[i].driver_name &&
		    (strncmp(rte_rawdevs[i].driver_name, "raw_ntb",
		    NTB_DRV_NAME_LEN) == 0) && (rte_rawdevs[i].attached == 1))
			break;

	if (i == RTE_RAWDEV_MAX_DEVS)
		rte_exit(EXIT_FAILURE, "Cannot find any ntb device.\n");

	dev_id = i;

	argc -= ret;
	argv += ret;

	ntb_parse_args(argc, argv);

	rte_rawdev_set_attr(dev_id, NTB_QUEUE_SZ_NAME, nb_desc);
	printf("Set queue size as %u.\n", nb_desc);
	rte_rawdev_set_attr(dev_id, NTB_QUEUE_NUM_NAME, num_queues);
	printf("Set queue number as %u.\n", num_queues);
	ntb_rawdev_info.dev_private = (rte_rawdev_obj_t)(&ntb_info);
	rte_rawdev_info_get(dev_id, &ntb_rawdev_info, sizeof(ntb_info));

	nb_mbuf = nb_desc * num_queues * 2 * 2 + rte_lcore_count() *
		  MEMPOOL_CACHE_SIZE;
	mbuf_pool = ntb_mbuf_pool_create(ntb_buf_size, nb_mbuf, ntb_info,
					 &ntb_conf, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool.\n");

	ntb_conf.num_queues = num_queues;
	ntb_conf.queue_size = nb_desc;
	ntb_rawdev_conf.dev_private = (rte_rawdev_obj_t)(&ntb_conf);
	ret = rte_rawdev_configure(dev_id, &ntb_rawdev_conf, sizeof(ntb_conf));
	if (ret)
		rte_exit(EXIT_FAILURE, "Can't config ntb dev: err=%d, "
			"port=%u\n", ret, dev_id);

	ntb_q_conf.tx_free_thresh = tx_free_thresh;
	ntb_q_conf.nb_desc = nb_desc;
	ntb_q_conf.rx_mp = mbuf_pool;
	for (i = 0; i < num_queues; i++) {
		/* Setup rawdev queue */
		ret = rte_rawdev_queue_setup(dev_id, i, &ntb_q_conf,
				sizeof(ntb_q_conf));
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Failed to setup ntb queue %u.\n", i);
	}

	/* Waiting for peer dev up at most 100s.*/
	printf("Checking ntb link status...\n");
	for (i = 0; i < 1000; i++) {
		rte_rawdev_get_attr(dev_id, NTB_LINK_STATUS_NAME,
				    &ntb_link_status);
		if (ntb_link_status) {
			printf("Peer dev ready, ntb link up.\n");
			break;
		}
		rte_delay_ms(100);
	}
	rte_rawdev_get_attr(dev_id, NTB_LINK_STATUS_NAME, &ntb_link_status);
	if (ntb_link_status == 0)
		printf("Expire 100s. Link is not up. Please restart app.\n");

	ret = rte_rawdev_start(dev_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_rawdev_start: err=%d, port=%u\n",
			ret, dev_id);

	/* Find 1st ethdev */
	eth_port_id = rte_eth_find_next(0);

	if (eth_port_id < RTE_MAX_ETHPORTS) {
		rte_eth_dev_info_get(eth_port_id, &ethdev_info);
		eth_pconf.rx_adv_conf.rss_conf.rss_hf &=
				ethdev_info.flow_type_rss_offloads;
		ret = rte_eth_dev_configure(eth_port_id, num_queues,
					    num_queues, &eth_pconf);
		if (ret)
			rte_exit(EXIT_FAILURE, "Can't config ethdev: err=%d, "
				"port=%u\n", ret, eth_port_id);
		eth_rx_conf = ethdev_info.default_rxconf;
		eth_rx_conf.offloads = eth_pconf.rxmode.offloads;
		eth_tx_conf = ethdev_info.default_txconf;
		eth_tx_conf.offloads = eth_pconf.txmode.offloads;

		/* Setup ethdev queue if ethdev exists */
		for (i = 0; i < num_queues; i++) {
			ret = rte_eth_rx_queue_setup(eth_port_id, i, nb_desc,
					rte_eth_dev_socket_id(eth_port_id),
					&eth_rx_conf, mbuf_pool);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"Failed to setup eth rxq %u.\n", i);
			ret = rte_eth_tx_queue_setup(eth_port_id, i, nb_desc,
					rte_eth_dev_socket_id(eth_port_id),
					&eth_tx_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"Failed to setup eth txq %u.\n", i);
		}

		ret = rte_eth_dev_start(eth_port_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, "
				"port=%u\n", ret, eth_port_id);
	}

	/* initialize port stats */
	memset(&ntb_port_stats, 0, sizeof(ntb_port_stats));

	/* Set default fwd mode if user doesn't set it. */
	if (fwd_mode == MAX_FWD_MODE && eth_port_id < RTE_MAX_ETHPORTS) {
		printf("Set default fwd mode as iofwd.\n");
		fwd_mode = IOFWD;
	}
	if (fwd_mode == MAX_FWD_MODE) {
		printf("Set default fwd mode as file-trans.\n");
		fwd_mode = FILE_TRANS;
	}

	if (interactive) {
		sleep(1);
		prompt();
	} else {
		start_pkt_fwd();
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
