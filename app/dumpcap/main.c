/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Microsoft Corporation
 *
 * DPDK application to dump network traffic
 * This is designed to look and act like the Wireshark
 * dumpcap program.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_bpf.h>
#include <rte_config.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_pcapng.h>
#include <rte_pdump.h>
#include <rte_ring.h>
#include <rte_string_fns.h>
#include <rte_time.h>
#include <rte_version.h>

#include <pcap/pcap.h>
#include <pcap/bpf.h>

#define RING_NAME "capture-ring"
#define MONITOR_INTERVAL  (500 * 1000)
#define MBUF_POOL_CACHE_SIZE 32
#define BURST_SIZE 32
#define SLEEP_THRESHOLD 1000

/* command line flags */
static const char *progname;
static bool quit_signal;
static bool group_read;
static bool quiet;
static bool promiscuous_mode = true;
static bool use_pcapng = true;
static char *output_name;
static const char *filter_str;
static unsigned int ring_size = 2048;
static const char *capture_comment;
static uint32_t snaplen = RTE_MBUF_DEFAULT_BUF_SIZE;
static bool dump_bpf;
static struct {
	uint64_t  duration;	/* nanoseconds */
	unsigned long packets;  /* number of packets in file */
	size_t size;		/* file size (bytes) */
} stop;

/* Running state */
static struct rte_bpf_prm *bpf_prm;
static uint64_t start_time, end_time;
static uint64_t packets_received;
static size_t file_size;

struct interface {
	TAILQ_ENTRY(interface) next;
	uint16_t port;
	char name[RTE_ETH_NAME_MAX_LEN];

	struct rte_rxtx_callback *rx_cb[RTE_MAX_QUEUES_PER_PORT];
};

TAILQ_HEAD(interface_list, interface);
static struct interface_list interfaces = TAILQ_HEAD_INITIALIZER(interfaces);
static struct interface *port2intf[RTE_MAX_ETHPORTS];

/* Can do either pcap or pcapng format output */
typedef union {
	rte_pcapng_t  *pcapng;
	pcap_dumper_t *dumper;
} dumpcap_out_t;

static void usage(void)
{
	printf("Usage: %s [options] ...\n\n", progname);
	printf("Capture Interface:\n"
	       "  -i <interface>           name or port index of interface\n"
	       "  -f <capture filter>      packet filter in libpcap filter syntax\n");
	printf("  -s <snaplen>, --snapshot-length <snaplen>\n"
	       "                           packet snapshot length (def: %u)\n",
	       RTE_MBUF_DEFAULT_BUF_SIZE);
	printf("  -p, --no-promiscuous-mode\n"
	       "                           don't capture in promiscuous mode\n"
	       "  -D, --list-interfaces    print list of interfaces and exit\n"
	       "  -d                       print generated BPF code for capture filter\n"
	       "\n"
	       "Stop conditions:\n"
	       "  -c <packet count>        stop after n packets (def: infinite)\n"
	       "  -a <autostop cond.> ..., --autostop <autostop cond.> ...\n"
	       "                           duration:NUM - stop after NUM seconds\n"
	       "                           filesize:NUM - stop this file after NUM kB\n"
	       "                            packets:NUM - stop after NUM packets\n"
	       "Output (files):\n"
	       "  -w <filename>            name of file to save (def: tempfile)\n"
	       "  -g                       enable group read access on the output file(s)\n"
	       "  -n                       use pcapng format instead of pcap (default)\n"
	       "  -P                       use libpcap format instead of pcapng\n"
	       "  --capture-comment <comment>\n"
	       "                           add a capture comment to the output file\n"
	       "\n"
	       "Miscellaneous:\n"
	       "  -q                       don't report packet capture counts\n"
	       "  -v, --version            print version information and exit\n"
	       "  -h, --help               display this help and exit\n"
	       "\n"
	       "Use Ctrl-C to stop capturing at any time.\n");
}

static const char *version(void)
{
	static char str[128];

	snprintf(str, sizeof(str),
		 "%s 1.0 (%s)\n", progname, rte_version());
	return str;
}

/* Parse numeric argument from command line */
static unsigned long get_uint(const char *arg, const char *name,
			     unsigned int limit)
{
	unsigned long u;
	char *endp;

	u = strtoul(arg, &endp, 0);
	if (*arg == '\0' || *endp != '\0')
		rte_exit(EXIT_FAILURE,
			 "Specified %s \"%s\" is not a valid number\n",
			 name, arg);
	if (limit && u > limit)
		rte_exit(EXIT_FAILURE,
			 "Specified %s \"%s\" is too large (greater than %u)\n",
			 name, arg, limit);

	return u;
}

/* Set auto stop values */
static void auto_stop(char *opt)
{
	char *value, *endp;

	value = strchr(opt, ':');
	if (value == NULL)
		rte_exit(EXIT_FAILURE,
			 "Missing colon in auto stop parameter\n");

	*value++ = '\0';
	if (strcmp(opt, "duration") == 0) {
		double interval = strtod(value, &endp);

		if (*value == '\0' || *endp != '\0' || interval <= 0)
			rte_exit(EXIT_FAILURE,
				 "Invalid duration \"%s\"\n", value);
		stop.duration = NSEC_PER_SEC * interval;
	} else if (strcmp(opt, "filesize") == 0) {
		stop.size = get_uint(value, "filesize", 0) * 1024;
	} else if (strcmp(opt, "packets") == 0) {
		stop.packets = get_uint(value, "packets", 0);
	} else {
		rte_exit(EXIT_FAILURE,
			 "Unknown autostop parameter \"%s\"\n", opt);
	}
}

/* Add interface to list of interfaces to capture */
static void add_interface(uint16_t port, const char *name)
{
	struct interface *intf;

	intf = malloc(sizeof(*intf));
	if (!intf)
		rte_exit(EXIT_FAILURE, "no memory for interface\n");

	memset(intf, 0, sizeof(*intf));
	rte_strscpy(intf->name, name, sizeof(intf->name));

	printf("Capturing on '%s'\n", name);

	port2intf[port] = intf;
	TAILQ_INSERT_TAIL(&interfaces, intf, next);
}

/* Select all valid DPDK interfaces */
static void select_all_interfaces(void)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;

	RTE_ETH_FOREACH_DEV(p) {
		if (rte_eth_dev_get_name_by_port(p, name) < 0)
			continue;
		add_interface(p, name);
	}
}

/*
 * Choose interface to capture if no -i option given.
 * Select the first DPDK port, this matches what dumpcap does.
 */
static void set_default_interface(void)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;

	RTE_ETH_FOREACH_DEV(p) {
		if (rte_eth_dev_get_name_by_port(p, name) < 0)
			continue;
		add_interface(p, name);
		return;
	}
	rte_exit(EXIT_FAILURE, "No usable interfaces found\n");
}

/* Lookup interface by name or port and add it to the list */
static void select_interface(const char *arg)
{
	uint16_t port;

	if (strcmp(arg, "*"))
		select_all_interfaces();
	else if (rte_eth_dev_get_port_by_name(arg, &port) == 0)
		add_interface(port, arg);
	else {
		char name[RTE_ETH_NAME_MAX_LEN];

		port = get_uint(arg, "port_number", UINT16_MAX);
		if (rte_eth_dev_get_name_by_port(port, name) < 0)
			rte_exit(EXIT_FAILURE, "Invalid port number %u\n",
				 port);
		add_interface(port, name);
	}
}

/* Display list of possible interfaces that can be used. */
static void show_interfaces(void)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;

	RTE_ETH_FOREACH_DEV(p) {
		if (rte_eth_dev_get_name_by_port(p, name) < 0)
			continue;
		printf("%u. %s\n", p, name);
	}
}

static void compile_filter(void)
{
	struct bpf_program bf;
	pcap_t *pcap;

	pcap = pcap_open_dead(DLT_EN10MB, snaplen);
	if (!pcap)
		rte_exit(EXIT_FAILURE, "can not open pcap\n");

	if (pcap_compile(pcap, &bf, filter_str,
			 1, PCAP_NETMASK_UNKNOWN) != 0)
		rte_exit(EXIT_FAILURE, "pcap filter string not valid (%s)\n",
			 pcap_geterr(pcap));

	bpf_prm = rte_bpf_convert(&bf);
	if (bpf_prm == NULL)
		rte_exit(EXIT_FAILURE,
			 "bpf convert failed: %s(%d)\n",
			 rte_strerror(rte_errno), rte_errno);

	if (dump_bpf) {
		printf("cBPF program (%u insns)\n", bf.bf_len);
		bpf_dump(&bf, 1);
		printf("\neBPF program (%u insns)\n", bpf_prm->nb_ins);
		rte_bpf_dump(stdout, bpf_prm->ins, bpf_prm->nb_ins);
		exit(0);
	}

	/* Don't care about original program any more */
	pcap_freecode(&bf);
	pcap_close(pcap);
}

/*
 * Parse command line options.
 * These are chosen to be similar to dumpcap command.
 */
static void parse_opts(int argc, char **argv)
{
	static const struct option long_options[] = {
		{ "autostop",        required_argument, NULL, 'a' },
		{ "capture-comment", required_argument, NULL, 0 },
		{ "help",            no_argument,       NULL, 'h' },
		{ "interface",       required_argument, NULL, 'i' },
		{ "list-interfaces", no_argument,       NULL, 'D' },
		{ "no-promiscuous-mode", no_argument,   NULL, 'p' },
		{ "output-file",     required_argument, NULL, 'w' },
		{ "ring-buffer",     required_argument, NULL, 'b' },
		{ "snapshot-length", required_argument, NULL, 's' },
		{ "version",         no_argument,       NULL, 'v' },
		{ NULL },
	};
	int option_index, c;

	for (;;) {
		c = getopt_long(argc, argv, "a:b:c:dDf:ghi:nN:pPqs:vw:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			switch (option_index) {
			case 0:
				capture_comment = optarg;
				break;
			default:
				usage();
				exit(1);
			}
			break;
		case 'a':
			auto_stop(optarg);
			break;
		case 'b':
			rte_exit(EXIT_FAILURE,
				 "multiple files not implemented\n");
			break;
		case 'c':
			stop.packets = get_uint(optarg, "packet_count", 0);
			break;
		case 'd':
			dump_bpf = true;
			break;
		case 'D':
			show_interfaces();
			exit(0);
		case 'f':
			filter_str = optarg;
			break;
		case 'g':
			group_read = true;
			break;
		case 'h':
			printf("%s\n\n", version());
			usage();
			exit(0);
		case 'i':
			select_interface(optarg);
			break;
		case 'n':
			use_pcapng = true;
			break;
		case 'N':
			ring_size = get_uint(optarg, "packet_limit", 0);
			break;
		case 'p':
			promiscuous_mode = false;
			break;
		case 'P':
			use_pcapng = false;
			break;
		case 'q':
			quiet = true;
			break;
		case 's':
			snaplen = get_uint(optarg, "snap_len", 0);
			break;
		case 'w':
			output_name = optarg;
			break;
		case 'v':
			printf("%s\n", version());
			exit(0);
		default:
			fprintf(stderr, "Invalid option: %s\n",
				argv[optind - 1]);
			usage();
			exit(1);
		}
	}
}

static void
signal_handler(int sig_num __rte_unused)
{
	__atomic_store_n(&quit_signal, true, __ATOMIC_RELAXED);
}

/* Return the time since 1/1/1970 in nanoseconds */
static uint64_t create_timestamp(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return rte_timespec_to_ns(&now);
}

static void
cleanup_pdump_resources(void)
{
	struct interface *intf;

	TAILQ_FOREACH(intf, &interfaces, next) {
		rte_pdump_disable(intf->port,
				  RTE_PDUMP_ALL_QUEUES, RTE_PDUMP_FLAG_RXTX);
		if (promiscuous_mode)
			rte_eth_promiscuous_disable(intf->port);
	}
}

/* Alarm signal handler, used to check that primary process */
static void
monitor_primary(void *arg __rte_unused)
{
	if (__atomic_load_n(&quit_signal, __ATOMIC_RELAXED))
		return;

	if (rte_eal_primary_proc_alive(NULL)) {
		rte_eal_alarm_set(MONITOR_INTERVAL, monitor_primary, NULL);
	} else {
		fprintf(stderr,
			"Primary process is no longer active, exiting...\n");
		__atomic_store_n(&quit_signal, true, __ATOMIC_RELAXED);
	}
}

/* Setup handler to check when primary exits. */
static void
enable_primary_monitor(void)
{
	int ret;

	/* Once primary exits, so will pdump. */
	ret = rte_eal_alarm_set(MONITOR_INTERVAL, monitor_primary, NULL);
	if (ret < 0)
		fprintf(stderr, "Fail to enable monitor:%d\n", ret);
}

static void
disable_primary_monitor(void)
{
	int ret;

	ret = rte_eal_alarm_cancel(monitor_primary, NULL);
	if (ret < 0)
		fprintf(stderr, "Fail to disable monitor:%d\n", ret);
}

static void
report_packet_stats(dumpcap_out_t out)
{
	struct rte_pdump_stats pdump_stats;
	struct interface *intf;
	uint64_t ifrecv, ifdrop;
	double percent;

	fputc('\n', stderr);
	TAILQ_FOREACH(intf, &interfaces, next) {
		if (rte_pdump_stats(intf->port, &pdump_stats) < 0)
			continue;

		/* do what Wiretap does */
		ifrecv = pdump_stats.accepted + pdump_stats.filtered;
		ifdrop = pdump_stats.nombuf + pdump_stats.ringfull;

		if (use_pcapng)
			rte_pcapng_write_stats(out.pcapng, intf->port, NULL,
					       start_time, end_time,
					       ifrecv, ifdrop);

		if (ifrecv == 0)
			percent = 0;
		else
			percent = 100. * ifrecv / (ifrecv + ifdrop);

		fprintf(stderr,
			"Packets received/dropped on interface '%s': "
			"%"PRIu64 "/%" PRIu64 " (%.1f)\n",
			intf->name, ifrecv, ifdrop, percent);
	}
}

/*
 * Start DPDK EAL with arguments.
 * Unlike most DPDK programs, this application does not use the
 * typical EAL command line arguments.
 * We don't want to expose all the DPDK internals to the user.
 */
static void dpdk_init(void)
{
	static const char * const args[] = {
		"dumpcap", "--proc-type", "secondary",
		"--log-level", "notice"

	};
	const int eal_argc = RTE_DIM(args);
	char **eal_argv;
	unsigned int i;

	/* DPDK API requires mutable versions of command line arguments. */
	eal_argv = calloc(eal_argc + 1, sizeof(char *));
	if (eal_argv == NULL)
		rte_panic("No memory\n");

	eal_argv[0] = strdup(progname);
	for (i = 1; i < RTE_DIM(args); i++)
		eal_argv[i] = strdup(args[i]);

	if (rte_eal_init(eal_argc, eal_argv) < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed: is primary process running?\n");

	if (rte_eth_dev_count_avail() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports found\n");
}

/* Create packet ring shared between callbacks and process */
static struct rte_ring *create_ring(void)
{
	struct rte_ring *ring;
	size_t size, log2;

	/* Find next power of 2 >= size. */
	size = ring_size;
	log2 = sizeof(size) * 8 - __builtin_clzl(size - 1);
	size = 1u << log2;

	if (size != ring_size) {
		fprintf(stderr, "Ring size %u rounded up to %zu\n",
			ring_size, size);
		ring_size = size;
	}

	ring = rte_ring_lookup(RING_NAME);
	if (ring == NULL) {
		ring = rte_ring_create(RING_NAME, ring_size,
					rte_socket_id(), 0);
		if (ring == NULL)
			rte_exit(EXIT_FAILURE, "Could not create ring :%s\n",
				 rte_strerror(rte_errno));
	}
	return ring;
}

static struct rte_mempool *create_mempool(void)
{
	static const char pool_name[] = "capture_mbufs";
	size_t num_mbufs = 2 * ring_size;
	struct rte_mempool *mp;

	mp = rte_mempool_lookup(pool_name);
	if (mp)
		return mp;

	mp = rte_pktmbuf_pool_create_by_ops(pool_name, num_mbufs,
					    MBUF_POOL_CACHE_SIZE, 0,
					    rte_pcapng_mbuf_size(snaplen),
					    rte_socket_id(), "ring_mp_sc");
	if (mp == NULL)
		rte_exit(EXIT_FAILURE,
			 "Mempool (%s) creation failed: %s\n", pool_name,
			 rte_strerror(rte_errno));

	return mp;
}

/*
 * Get Operating System information.
 * Returns an string allocated via malloc().
 */
static char *get_os_info(void)
{
	struct utsname uts;
	char *osname = NULL;

	if (uname(&uts) < 0)
		return NULL;

	if (asprintf(&osname, "%s %s",
		     uts.sysname, uts.release) == -1)
		return NULL;

	return osname;
}

static dumpcap_out_t create_output(void)
{
	dumpcap_out_t ret;
	static char tmp_path[PATH_MAX];
	int fd;

	/* If no filename specified make a tempfile name */
	if (output_name == NULL) {
		struct interface *intf;
		struct tm *tm;
		time_t now;
		char ts[32];

		intf = TAILQ_FIRST(&interfaces);
		now = time(NULL);
		tm = localtime(&now);
		if (!tm)
			rte_panic("localtime failed\n");

		strftime(ts, sizeof(ts), "%Y%m%d%H%M%S", tm);

		snprintf(tmp_path, sizeof(tmp_path),
			 "/tmp/%s_%u_%s_%s.%s",
			 progname, intf->port, intf->name, ts,
			 use_pcapng ? "pcapng" : "pcap");
		output_name = tmp_path;
	}

	if (strcmp(output_name, "-") == 0)
		fd = STDOUT_FILENO;
	else {
		mode_t mode = group_read ? 0640 : 0600;

		fd = open(output_name, O_WRONLY | O_CREAT, mode);
		if (fd < 0)
			rte_exit(EXIT_FAILURE, "Can not open \"%s\": %s\n",
				 output_name, strerror(errno));
	}

	if (use_pcapng) {
		char *os = get_os_info();

		ret.pcapng = rte_pcapng_fdopen(fd, os, NULL,
					   version(), capture_comment);
		if (ret.pcapng == NULL)
			rte_exit(EXIT_FAILURE, "pcapng_fdopen failed: %s\n",
				 strerror(rte_errno));
		free(os);
	} else {
		pcap_t *pcap;

		pcap = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, snaplen,
							    PCAP_TSTAMP_PRECISION_NANO);
		if (pcap == NULL)
			rte_exit(EXIT_FAILURE, "pcap_open_dead failed\n");

		ret.dumper = pcap_dump_fopen(pcap, fdopen(fd, "w"));
		if (ret.dumper == NULL)
			rte_exit(EXIT_FAILURE, "pcap_dump_fopen failed: %s\n",
				 pcap_geterr(pcap));
	}

	return ret;
}

static void enable_pdump(struct rte_ring *r, struct rte_mempool *mp)
{
	struct interface *intf;
	uint32_t flags;
	int ret;

	flags = RTE_PDUMP_FLAG_RXTX;
	if (use_pcapng)
		flags |= RTE_PDUMP_FLAG_PCAPNG;

	TAILQ_FOREACH(intf, &interfaces, next) {
		if (promiscuous_mode) {
			ret = rte_eth_promiscuous_enable(intf->port);
			if (ret != 0)
				fprintf(stderr,
					"port %u set promiscuous enable failed: %d\n",
					intf->port, ret);
		}

		ret = rte_pdump_enable_bpf(intf->port, RTE_PDUMP_ALL_QUEUES,
					   flags, snaplen,
					   r, mp, bpf_prm);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Packet dump enable failed: %s\n",
				 rte_strerror(-ret));
	}
}

/*
 * Show current count of captured packets
 * with backspaces to overwrite last value.
 */
static void show_count(uint64_t count)
{
	unsigned int i;
	static unsigned int bt;

	for (i = 0; i < bt; i++)
		fputc('\b', stderr);

	bt = fprintf(stderr, "%"PRIu64" ", count);
}

/* Write multiple packets in older pcap format */
static ssize_t
pcap_write_packets(pcap_dumper_t *dumper,
		   struct rte_mbuf *pkts[], uint16_t n)
{
	uint8_t temp_data[snaplen];
	struct pcap_pkthdr header;
	uint16_t i;
	size_t total = 0;

	gettimeofday(&header.ts, NULL);

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];

		header.len = rte_pktmbuf_pkt_len(m);
		header.caplen = RTE_MIN(header.len, snaplen);

		pcap_dump((u_char *)dumper, &header,
			  rte_pktmbuf_read(m, 0, header.caplen, temp_data));

		total += sizeof(header) + header.len;
	}

	return total;
}

/* Process all packets in ring and dump to capture file */
static int process_ring(dumpcap_out_t out, struct rte_ring *r)
{
	struct rte_mbuf *pkts[BURST_SIZE];
	unsigned int avail, n;
	static unsigned int empty_count;
	ssize_t written;

	n = rte_ring_sc_dequeue_burst(r, (void **) pkts, BURST_SIZE,
				      &avail);
	if (n == 0) {
		/* don't consume endless amounts of cpu if idle */
		if (empty_count < SLEEP_THRESHOLD)
			++empty_count;
		else
			usleep(10);
		return 0;
	}

	empty_count = (avail == 0);

	if (use_pcapng)
		written = rte_pcapng_write_packets(out.pcapng, pkts, n);
	else
		written = pcap_write_packets(out.dumper, pkts, n);

	rte_pktmbuf_free_bulk(pkts, n);

	if (written < 0)
		return -1;

	file_size += written;
	packets_received += n;
	if (!quiet)
		show_count(packets_received);

	return 0;
}

int main(int argc, char **argv)
{
	struct rte_ring *r;
	struct rte_mempool *mp;
	dumpcap_out_t out;

	progname = argv[0];

	parse_opts(argc, argv);
	dpdk_init();

	if (filter_str)
		compile_filter();

	if (TAILQ_EMPTY(&interfaces))
		set_default_interface();

	r = create_ring();
	mp = create_mempool();
	out = create_output();

	start_time = create_timestamp();
	enable_pdump(r, mp);

	signal(SIGINT, signal_handler);
	signal(SIGPIPE, SIG_IGN);

	enable_primary_monitor();

	if (!quiet) {
		fprintf(stderr, "Packets captured: ");
		show_count(0);
	}

	while (!__atomic_load_n(&quit_signal, __ATOMIC_RELAXED)) {
		if (process_ring(out, r) < 0) {
			fprintf(stderr, "pcapng file write failed; %s\n",
				strerror(errno));
			break;
		}

		if (stop.size && file_size >= stop.size)
			break;

		if (stop.packets && packets_received >= stop.packets)
			break;

		if (stop.duration != 0 &&
		    create_timestamp() - start_time > stop.duration)
			break;
	}

	end_time = create_timestamp();
	disable_primary_monitor();

	if (rte_eal_primary_proc_alive(NULL))
		report_packet_stats(out);

	if (use_pcapng)
		rte_pcapng_close(out.pcapng);
	else
		pcap_dump_close(out.dumper);

	cleanup_pdump_resources();
	rte_free(bpf_filter);
	rte_ring_free(r);
	rte_mempool_free(mp);

	return rte_eal_cleanup() ? EXIT_FAILURE : 0;
}
