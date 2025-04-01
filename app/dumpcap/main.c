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
#include <rte_bitops.h>
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
#include <rte_string_fns.h>
#include <rte_thread.h>
#include <rte_time.h>
#include <rte_version.h>

#include <pcap/pcap.h>
#include <pcap/bpf.h>

#define MONITOR_INTERVAL  (500 * 1000)
#define MBUF_POOL_CACHE_SIZE 32
#define BURST_SIZE 32
#define SLEEP_THRESHOLD 1000

/* command line flags */
static const char *progname;
static RTE_ATOMIC(bool) quit_signal;
static bool group_read;
static bool quiet;
static bool use_pcapng = true;
static char *output_name;
static const char *tmp_dir = "/tmp";
static unsigned int ring_size = 2048;
static const char *capture_comment;
static const char *file_prefix;
static const char *lcore_arg;
static bool dump_bpf;
static bool show_interfaces;
static bool print_stats;

/* capture limit options */
static struct {
	time_t  duration;	/* seconds */
	unsigned long packets;  /* number of packets in file */
	size_t size;		/* file size (bytes) */
} stop;

/* Running state */
static time_t start_time;
static uint64_t packets_received;
static size_t file_size;

/* capture options */
struct capture_options {
	const char *filter;
	uint32_t snap_len;
	bool promisc_mode;
} capture = {
	.snap_len = RTE_MBUF_DEFAULT_BUF_SIZE,
	.promisc_mode = true,
};

struct interface {
	TAILQ_ENTRY(interface) next;
	uint16_t port;
	struct rte_bpf *filter;
	uint64_t filteraccept;
	struct capture_options opts;
	char name[RTE_ETH_NAME_MAX_LEN];

	struct rte_eth_stats start_stats;
	const char *ifname;
	const char *ifdescr;
};

static int timestamp_dynfield = -1;
static uint64_t timestamp_dynflag;
static int mirror_origin_dynfield = -1;
static uint64_t mirror_origin_dynflag;
static uint64_t mirror_ingress_dynflag;
static uint64_t mirror_egress_dynflag;

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
	       "  -i <interface>, --interface <interface>\n"
	       "                           name or port index of interface\n"
	       "  -f <capture filter>      packet filter in libpcap filter syntax\n");
	printf("  --ifname <name>          name to use in the capture file\n");
	printf("  --ifdescr <description>\n");
	printf("                           description to use in the capture file\n");
	printf("  -s <snaplen>, --snapshot-length <snaplen>\n"
	       "                           packet snapshot length (def: %u)\n",
	       RTE_MBUF_DEFAULT_BUF_SIZE);
	printf("  -p, --no-promiscuous-mode\n"
	       "                           don't capture in promiscuous mode\n"
	       "  -D, --list-interfaces    print list of interfaces and exit\n"
	       "  -d                       print generated BPF code for capture filter\n"
	       "  -S                       print statistics for each interface once per second\n"
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
	       "  --temp-dir <directory>   write temporary files to this directory\n"
	       "                           (default: /tmp)\n"
	       "\n"
	       "Miscellaneous:\n"
	       "  --lcore=<core>           CPU core to run on (default: any)\n"
	       "  --file-prefix=<prefix>   prefix to use for multi-process\n"
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
		stop.duration = interval;
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
static struct interface *add_interface(const char *name)
{
	struct interface *intf;

	if (strlen(name) >= RTE_ETH_NAME_MAX_LEN)
		rte_exit(EXIT_FAILURE, "invalid name for interface: '%s'\n", name);

	intf = malloc(sizeof(*intf));
	if (!intf)
		rte_exit(EXIT_FAILURE, "no memory for interface\n");

	memset(intf, 0, sizeof(*intf));
	rte_strscpy(intf->name, name, sizeof(intf->name));
	intf->opts = capture;
	intf->port = -1;	/* port set later after EAL init */

	TAILQ_INSERT_TAIL(&interfaces, intf, next);
	return intf;
}

/* Name has been set but need to lookup port after eal_init */
static void find_interfaces(void)
{
	struct interface *intf;

	TAILQ_FOREACH(intf, &interfaces, next) {
		/* if name is valid then just record port */
		if (rte_eth_dev_get_port_by_name(intf->name, &intf->port) != 0) {
			/* maybe got passed port number string as name */
			intf->port = get_uint(intf->name, "port_number", UINT16_MAX);
			if (rte_eth_dev_get_name_by_port(intf->port, intf->name) < 0)
				rte_exit(EXIT_FAILURE, "Invalid port number %u\n",
					 intf->port);
		}

		if (rte_eth_stats_get(intf->port, &intf->start_stats) < 0)
			rte_exit(EXIT_FAILURE, "Could not read stats for port %u\n", intf->port);
	}
}

/*
 * Choose interface to capture if no -i option given.
 * Select the first DPDK port, this matches what dumpcap does.
 */
static void set_default_interface(void)
{
	struct interface *intf;
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;

	RTE_ETH_FOREACH_DEV(p) {
		if (rte_eth_dev_get_name_by_port(p, name) < 0)
			continue;

		intf = add_interface(name);
		intf->port = p;

		if (rte_eth_stats_get(intf->port, &intf->start_stats) < 0)
			rte_exit(EXIT_FAILURE, "Could not read stats for default port %u\n", intf->port);
		return;
	}
	rte_exit(EXIT_FAILURE, "No usable interfaces found\n");
}

/* Display list of possible interfaces that can be used. */
static void dump_interfaces(void)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;

	RTE_ETH_FOREACH_DEV(p) {
		if (rte_eth_dev_get_name_by_port(p, name) < 0)
			continue;
		printf("%u. %s\n", p, name);
	}

	exit(0);
}

static void compile_filters(void)
{
	struct interface *intf;

	TAILQ_FOREACH(intf, &interfaces, next) {
		struct rte_bpf_prm *bpf_prm;
		struct bpf_program bf;
		pcap_t *pcap;

		/* cache for filter packets */
		port2intf[intf->port] = intf;

		pcap = pcap_open_dead(DLT_EN10MB, intf->opts.snap_len);
		if (!pcap)
			rte_exit(EXIT_FAILURE, "can not open pcap\n");

		if (pcap_compile(pcap, &bf, intf->opts.filter,
				 1, PCAP_NETMASK_UNKNOWN) != 0) {
			fprintf(stderr,
				"Invalid capture filter \"%s\": for interface '%s'\n",
				intf->opts.filter, intf->name);
			rte_exit(EXIT_FAILURE, "\n%s\n",
				 pcap_geterr(pcap));
		}

		bpf_prm = rte_bpf_convert(&bf);
		if (bpf_prm == NULL)
			rte_exit(EXIT_FAILURE,
				 "BPF convert interface '%s'\n%s(%d)\n",
				 intf->name,
				 rte_strerror(rte_errno), rte_errno);

		if (dump_bpf) {
			printf("cBPF program (%u insns)\n", bf.bf_len);
			bpf_dump(&bf, 1);
			printf("\neBPF program (%u insns)\n",
			       bpf_prm->nb_ins);
			rte_bpf_dump(stdout, bpf_prm->ins, bpf_prm->nb_ins);
			exit(0);
		}


		intf->filter = rte_bpf_load(bpf_prm);
		if (intf->filter == NULL)
			rte_exit(EXIT_FAILURE,
				 "BPF load failed '%s'\n%s(%d)\n",
				 intf->name, rte_strerror(rte_errno), rte_errno);

		/* Don't care about original program any more */
		pcap_freecode(&bf);
		pcap_close(pcap);
	}
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
		{ "file-prefix",     required_argument, NULL, 0 },
		{ "help",            no_argument,       NULL, 'h' },
		{ "ifdescr",	     required_argument, NULL, 0 },
		{ "ifname",	     required_argument, NULL, 0 },
		{ "interface",       required_argument, NULL, 'i' },
		{ "lcore",           required_argument, NULL, 0 },
		{ "list-interfaces", no_argument,       NULL, 'D' },
		{ "no-promiscuous-mode", no_argument,   NULL, 'p' },
		{ "output-file",     required_argument, NULL, 'w' },
		{ "ring-buffer",     required_argument, NULL, 'b' },
		{ "snapshot-length", required_argument, NULL, 's' },
		{ "temp-dir",        required_argument, NULL, 0 },
		{ "version",         no_argument,       NULL, 'v' },
		{ NULL },
	};
	int option_index, c;
	struct interface *last_intf = NULL;
	uint32_t len;

	for (;;) {
		c = getopt_long(argc, argv, "a:b:c:dDf:ghi:nN:pPqSs:vw:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0: {
			const char *longopt
				= long_options[option_index].name;

			if (!strcmp(longopt, "capture-comment")) {
				capture_comment = optarg;
			} else if (!strcmp(longopt, "lcore")) {
				lcore_arg = optarg;
			} else if (!strcmp(longopt, "file-prefix")) {
				file_prefix = optarg;
			} else if (!strcmp(longopt, "temp-dir")) {
				tmp_dir = optarg;
			} else if (!strcmp(longopt, "ifdescr")) {
				if (last_intf == NULL)
					rte_exit(EXIT_FAILURE,
						 "--ifdescr must be specified after a -i option\n");
				last_intf->ifdescr = optarg;
			} else if (!strcmp(longopt, "ifname")) {
				if (last_intf == NULL)
					rte_exit(EXIT_FAILURE,
						 "--ifname must be specified after a -i option\n");
				last_intf->ifname = optarg;
			} else {
				usage();
				exit(1);
			}
			break;
		}
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
			show_interfaces = true;
			break;
		case 'f':
			if (last_intf == NULL)
				capture.filter = optarg;
			else
				last_intf->opts.filter = optarg;
			break;
		case 'g':
			group_read = true;
			break;
		case 'h':
			printf("%s\n\n", version());
			usage();
			exit(0);
		case 'i':
			last_intf = add_interface(optarg);
			break;
		case 'n':
			use_pcapng = true;
			break;
		case 'N':
			ring_size = get_uint(optarg, "packet_limit", 0);
			break;
		case 'p':
			/* Like dumpcap this option can occur multiple times.
			 *
			 * If used before the first occurrence of the -i option,
			 * no interface will be put into the promiscuous mode.
			 * If used after an -i option, the interface specified
			 * by the last -i option occurring before this option
			 * will not be put into the promiscuous mode.
			 */
			if (last_intf == NULL)
				capture.promisc_mode = false;
			else
				last_intf->opts.promisc_mode = false;
			break;
		case 'P':
			use_pcapng = false;
			break;
		case 'q':
			quiet = true;
			break;
		case 's':
			len = get_uint(optarg, "snap_len", 0);
			if (last_intf == NULL)
				capture.snap_len = len;
			else
				last_intf->opts.snap_len = len;
			break;
		case 'S':
			print_stats = true;
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
	rte_atomic_store_explicit(&quit_signal, true, rte_memory_order_relaxed);
}


/* Instead of capturing, it tracks interface statistics */
static void statistics_loop(void)
{
	struct rte_eth_stats stats;
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t p;
	int r;

	printf("%-15s  %10s  %10s\n",
	       "Interface", "Received", "Dropped");

	while (!rte_atomic_load_explicit(&quit_signal, rte_memory_order_relaxed)) {
		RTE_ETH_FOREACH_DEV(p) {
			if (rte_eth_dev_get_name_by_port(p, name) < 0)
				continue;

			r = rte_eth_stats_get(p, &stats);
			if (r < 0) {
				fprintf(stderr,
					"stats_get for port %u failed: %d (%s)\n",
					p, r, strerror(-r));
				return;
			}

			printf("%-15s  %10"PRIu64"  %10"PRIu64"\n",
			       name, stats.ipackets,
			       stats.imissed + stats.ierrors + stats.rx_nombuf);
		}
		sleep(1);
	}
}

static void
disable_mirror_ports(uint16_t mirror_port)
{
	struct interface *intf;

	TAILQ_FOREACH(intf, &interfaces, next) {
		rte_eth_remove_mirror(intf->port, mirror_port);
		if (intf->opts.promisc_mode)
			rte_eth_promiscuous_disable(intf->port);
	}
}

/* Alarm signal handler, used to check that primary process */
static void
monitor_primary(void *arg __rte_unused)
{
	if (rte_atomic_load_explicit(&quit_signal, rte_memory_order_relaxed))
		return;

	if (rte_eal_primary_proc_alive(NULL)) {
		rte_eal_alarm_set(MONITOR_INTERVAL, monitor_primary, NULL);
	} else {
		fprintf(stderr,
			"Primary process is no longer active, exiting...\n");
		rte_atomic_store_explicit(&quit_signal, true, rte_memory_order_relaxed);
	}
}

/* Setup handler to check when primary exits. */
static void
enable_primary_monitor(void)
{
	int ret;

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
	struct interface *intf;

	fputc('\n', stderr);

	TAILQ_FOREACH(intf, &interfaces, next) {
		uint64_t ifrecv, ifdrop;
		struct rte_eth_stats stats;
		double percent;

		if (rte_eth_stats_get(intf->port, &stats) < 0)
			continue;

		ifrecv = stats.ipackets - intf->start_stats.ipackets;
		ifdrop = (stats.rx_nombuf - intf->start_stats.rx_nombuf)
			+ (stats.imissed - intf->start_stats.imissed);

		if (use_pcapng)
			rte_pcapng_write_stats(out.pcapng, intf->port, ifrecv, ifdrop, NULL);

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
	int eal_argc = RTE_DIM(args);
	rte_cpuset_t cpuset = { };
	char **eal_argv;
	unsigned int i;

	if (file_prefix != NULL)
		eal_argc += 2;

	if (lcore_arg != NULL)
		eal_argc += 2;

	/* DPDK API requires mutable versions of command line arguments. */
	eal_argv = calloc(eal_argc + 1, sizeof(char *));
	if (eal_argv == NULL)
		rte_panic("No memory\n");

	eal_argv[0] = strdup(progname);
	for (i = 1; i < RTE_DIM(args); i++)
		eal_argv[i] = strdup(args[i]);

	if (lcore_arg != NULL) {
		eal_argv[i++] = strdup("--lcores");
		eal_argv[i++] = strdup(lcore_arg);
	}

	if (file_prefix != NULL) {
		eal_argv[i++] = strdup("--file-prefix");
		eal_argv[i++] = strdup(file_prefix);
	}

	for (i = 0; i < (unsigned int)eal_argc; i++) {
		if (eal_argv[i] == NULL)
			rte_panic("No memory\n");
	}

	/*
	 * Need to get the original cpuset, before EAL init changes
	 * the affinity of this thread (main lcore).
	 */
	if (lcore_arg == NULL &&
	    rte_thread_get_affinity_by_id(rte_thread_self(), &cpuset) != 0)
		rte_panic("rte_thread_getaffinity failed\n");

	if (rte_eal_init(eal_argc, eal_argv) < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed: is primary process running?\n");

	/*
	 * If no lcore argument was specified, then run this program as a normal process
	 * which can be scheduled on any non-isolated CPU.
	 */
	if (lcore_arg == NULL &&
	    rte_thread_set_affinity_by_id(rte_thread_self(), &cpuset) != 0)
		rte_exit(EXIT_FAILURE, "Can not restore original CPU affinity\n");
}

/* Create ring port to redirect packet to.
 * This could be much simpler if the ring PMD API (rte_eth_from_rings)
 * worked from a secondary process, but it doesn't.
 */
static struct rte_ring *
create_ring_dev(char *vdev_name, uint16_t *mirror_port)
{
	struct rte_eth_dev_owner owner = { 0 };
	struct rte_eth_conf dev_conf = { 0 };
	struct rte_ring *ring = NULL;
	char ring_name[RTE_RING_NAMESIZE];
	char vdev_args[128];
	size_t size, log2;
	uint16_t port;
	int ret;

	/* Find next power of 2 >= size. */
	size = ring_size;
	log2 = sizeof(size) * 8 - rte_clz64(size - 1);
	size = 1u << log2;

	if (size != ring_size) {
		fprintf(stderr, "Ring size %u rounded up to %zu\n",
			ring_size, size);
		ring_size = size;
	}

	ret = rte_eth_dev_owner_new(&owner.id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_dev_owner_new failed: %s\n",
			 strerror(-ret));

	/* Give the vdev a unique name */
	snprintf(ring_name, sizeof(ring_name), "dumpcap%"PRIu64, owner.id);
	ring = rte_ring_create(ring_name, ring_size, rte_socket_id(),
			       RING_F_MP_RTS_ENQ | RING_F_SC_DEQ);
	if (ring == NULL)
		rte_exit(EXIT_FAILURE, "Could not create ring :%s\n",
			 rte_strerror(rte_errno));

	strlcpy(owner.name, ring_name, sizeof(owner.name));

	snprintf(vdev_name, RTE_DEV_NAME_MAX_LEN,
		 "net_ring-dumpcap%"PRIu64, owner.id);
	snprintf(vdev_args, sizeof(vdev_args),
		 "ring=%s,timestamp", ring_name);

	if (rte_eal_hotplug_add("vdev", vdev_name, vdev_args) < 0)
		rte_exit(EXIT_FAILURE,
			 "rte_eal_hotplug_add of %s failed:%s\n",
			 vdev_name, rte_strerror(rte_errno));

	ret = rte_eth_dev_get_port_by_name(vdev_name, &port);
	if (ret != 0) {
		fprintf(stderr, "Could not port for %s: %s\n",
			vdev_name, strerror(-ret));
		goto unplug;
	}

	ret = rte_eth_dev_owner_set(port, &owner);
	if (ret != 0) {
		fprintf(stderr, "Could not set owner for port for %u: %s\n",
			port, strerror(-ret));
		goto unplug;
	}

	ret = rte_eth_dev_configure(port, 1, 1, &dev_conf);
	if (ret < 0) {
		fprintf(stderr, "Could not configure port %u: %s\n",
			port, strerror(-ret));
		goto unplug;
	}

	*mirror_port = port;
	return ring;

unplug:
	rte_eal_hotplug_remove("vdev", vdev_name);
	rte_ring_free(ring);
	rte_eal_cleanup();
	exit(EXIT_FAILURE);
}

static struct rte_mempool *create_mempool(void)
{
	const struct interface *intf;
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	size_t num_mbufs = 2 * ring_size;
	struct rte_mempool *mp;
	uint32_t data_size = 128;

	snprintf(pool_name, sizeof(pool_name), "capture_%d", getpid());

	/* Common pool so size mbuf for biggest snap length */
	TAILQ_FOREACH(intf, &interfaces, next) {
		uint32_t mbuf_size = rte_pcapng_mbuf_size(intf->opts.snap_len);

		if (mbuf_size > data_size)
			data_size = mbuf_size;
	}

	mp = rte_pktmbuf_pool_create_by_ops(pool_name, num_mbufs,
					    MBUF_POOL_CACHE_SIZE, 0,
					    data_size,
					    rte_socket_id(), "ring_mp_mc");
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
			 "%s/%s_%u_%s_%s.%s", tmp_dir,
			 progname, intf->port, intf->name, ts,
			 use_pcapng ? "pcapng" : "pcap");
		output_name = tmp_path;
	}

	if (strcmp(output_name, "-") == 0)
		fd = STDOUT_FILENO;
	else {
		mode_t mode = group_read ? 0640 : 0600;

		fprintf(stderr, "File: %s\n", output_name);
		fd = open(output_name, O_WRONLY | O_CREAT, mode);
		if (fd < 0)
			rte_exit(EXIT_FAILURE, "Can not open \"%s\": %s\n",
				 output_name, strerror(errno));
	}

	if (use_pcapng) {
		struct interface *intf;
		char *os = get_os_info();

		ret.pcapng = rte_pcapng_fdopen(fd, os, NULL,
					   version(), capture_comment);
		if (ret.pcapng == NULL)
			rte_exit(EXIT_FAILURE, "pcapng_fdopen failed: %s\n",
				 rte_strerror(rte_errno));
		free(os);

		TAILQ_FOREACH(intf, &interfaces, next) {
			if (rte_pcapng_add_interface(ret.pcapng, intf->port, intf->ifname,
						     intf->ifdescr, intf->opts.filter) < 0)
				rte_exit(EXIT_FAILURE, "rte_pcapng_add_interface %u failed\n",
					intf->port);
		}
	} else {
		pcap_t *pcap;

		pcap = pcap_open_dead_with_tstamp_precision(DLT_EN10MB,
							    capture.snap_len,
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

static int enable_mirror(uint16_t mirror_port, struct rte_mempool *mpool)
{
	struct interface *intf;
	unsigned int count = 0;
	int ret;

	ret = rte_eth_dev_start(mirror_port);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not start mirror port %u: %s\n",
			 mirror_port, strerror(-ret));

	TAILQ_FOREACH(intf, &interfaces, next) {
		struct rte_eth_mirror_conf conf = {
			.mp = mpool,
			.target = mirror_port,
			.snaplen = intf->opts.snap_len,
			.direction = RTE_ETH_MIRROR_DIRECTION_INGRESS |
				     RTE_ETH_MIRROR_DIRECTION_EGRESS,
		};

		ret = rte_eth_add_mirror(intf->port, &conf);
		if (ret < 0) {
			fprintf(stderr, "Port mirror on %u:%s failed %s\n",
				intf->port, intf->name, strerror(-ret));
			return -1;
		}

		if (intf->opts.promisc_mode) {
			if (rte_eth_promiscuous_get(intf->port) == 1) {
				/* promiscuous already enabled */
				intf->opts.promisc_mode = false;
			} else if (rte_eth_promiscuous_enable(intf->port) < 0) {
				fprintf(stderr, "port %u:%s set promiscuous failed\n",
					intf->port, intf->name);
				intf->opts.promisc_mode = false;
			}
		}
		++count;
	}
	return count;
}

static void setup_mbuf(void)
{
	int offset;

	offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find timestamp dynamic field\n");
	else
		timestamp_dynfield = offset;


	offset = rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find timestamp flag\n");
	else
		timestamp_dynflag = RTE_BIT64(offset);

	offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_MIRROR_ORIGIN, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find mirror origin dynamic field\n");
	else
		mirror_origin_dynfield = offset;

	offset = rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_MIRROR_ORIGIN, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find mirror origin dynamic flag\n");
	else
		mirror_origin_dynflag = RTE_BIT64(offset);

	offset = rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_MIRROR_INGRESS, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find mirror ingress flag\n");
	else
		mirror_ingress_dynflag = RTE_BIT64(offset);

	offset = rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_MIRROR_EGRESS, NULL);
	if (offset < 0)
		fprintf(stderr, "Could not find mirror egress flag\n");
	else
		mirror_egress_dynflag = RTE_BIT64(offset);
}

static void show_capturing(unsigned int count)
{
	struct interface *intf;

	fputs("Capturing on ", stdout);
	TAILQ_FOREACH(intf, &interfaces, next) {
		if (intf != TAILQ_FIRST(&interfaces)) {
			if (count > 2)
				putchar(',');
			putchar(' ');
			if (TAILQ_NEXT(intf, next) == NULL)
				fputs("and ", stdout);
		}
		printf("'%s'", intf->name);
	}
	putchar('\n');
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

static ssize_t
pcapng_write_packets(rte_pcapng_t *pcapng, struct rte_mbuf *pkts[], uint16_t n)
{
	struct rte_mbuf *towrite[BURST_SIZE];
	unsigned int count = 0;
	unsigned int i;

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];
		enum rte_pcapng_direction direction = RTE_PCAPNG_DIRECTION_UNKNOWN;
		uint32_t orig_len = m->pkt_len;
		uint16_t queue_id = UINT16_MAX;
		uint64_t ts;

		/* If mirror has origin info then use it */
		if (m->ol_flags & mirror_origin_dynflag) {
			struct rte_mbuf_origin *origin
				= RTE_MBUF_DYNFIELD(m, mirror_origin_dynfield, rte_mbuf_origin_t *);
			orig_len = origin->original_len;
			queue_id = origin->queue_id;
		}

		if (m->ol_flags & mirror_ingress_dynflag)
			direction = RTE_PCAPNG_DIRECTION_IN;
		else if (m->ol_flags & mirror_egress_dynflag)
			direction = RTE_PCAPNG_DIRECTION_OUT;

		if (likely(timestamp_dynfield >= 0 && (m->ol_flags & timestamp_dynflag)))
			ts = *RTE_MBUF_DYNFIELD(m, timestamp_dynfield, rte_mbuf_timestamp_t *);
		else
			ts = rte_get_tsc_cycles();

		if (unlikely(rte_pcapng_insert(m, queue_id, direction, orig_len, ts, NULL) < 0))
			continue;  /* skip no headroom? */

		towrite[count++] = m;
	}

	return rte_pcapng_write_packets(pcapng, towrite, count);
}

/* Write multiple packets in older pcap format */
static ssize_t
pcap_write_packets(pcap_dumper_t *dumper,
		   struct rte_mbuf *pkts[], uint16_t n)
{
	uint8_t temp_data[RTE_ETHER_MAX_JUMBO_FRAME_LEN];
	struct pcap_pkthdr header;
	uint16_t i;
	size_t total = 0;

	gettimeofday(&header.ts, NULL);

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];
		size_t len, caplen;

		len = caplen = rte_pktmbuf_pkt_len(m);
		if (unlikely(!rte_pktmbuf_is_contiguous(m) && len > sizeof(temp_data)))
			caplen = sizeof(temp_data);

		header.len = len;
		header.caplen = caplen;

		pcap_dump((u_char *)dumper, &header,
			  rte_pktmbuf_read(m, 0, caplen, temp_data));

		total += sizeof(header) + caplen;
	}

	return total;
}

static unsigned int
filter_burst(struct interface *intf, struct rte_mbuf *pkts[], unsigned int n)
{
	uint64_t results[BURST_SIZE];
	struct rte_mbuf *towrite[BURST_SIZE];
	unsigned int i, count;
	uint32_t matches;

	matches = rte_bpf_exec_burst(intf->filter, (void **)pkts, results, n);
	if (matches == n)
		return n;
	intf->filteraccept += matches;

	for (i = 0, count = 0; i < n; i++) {
		if (results[i])
			towrite[count++] = pkts[i];
		else
			rte_pktmbuf_free(pkts[i]);
	}
	memcpy(pkts, towrite, count * sizeof(struct rte_mbuf *));
	return count;
}

/* Make sure configuration of mbuf pool has enough headroom for both vlans */
static_assert(RTE_PKTMBUF_HEADROOM > 2 * sizeof(struct rte_vlan_hdr),
	"not enough mbuf headroom for vlan insertion");

/*
 * More general version of rte_vlan_insert()
 * Note: mbufs are allocated by rte_eth_mirror_burst() from the
 * pool that was passed when setting up mirror.
 */
static void
vlan_insert(struct rte_mbuf *m, uint16_t ether_type, uint16_t tci)
{
	struct rte_ether_hdr *nh, tmph;
	const struct rte_ether_hdr *oh;
	struct rte_vlan_hdr *vh;

	RTE_ASSERT(!RTE_MBUF_DIRECT(m));
	RTE_ASSERT(rte_mbuf_refcnt_read(m) == 1);

	oh = rte_pktmbuf_read(m, 0, sizeof(*oh), &tmph);
	RTE_ASSERT(oh != NULL);

	/* overlay new header */
	nh = (struct rte_ether_hdr *)
		rte_pktmbuf_prepend(m, sizeof(struct rte_vlan_hdr));

	RTE_ASSERT(nh != NULL);

	memmove(nh, oh, 2 * RTE_ETHER_ADDR_LEN);
	nh->ether_type = rte_cpu_to_be_16(ether_type);

	vh = (struct rte_vlan_hdr *) (nh + 1);
	vh->vlan_tci = rte_cpu_to_be_16(tci);
}

/* Filtering and pcap file format require that VLAN not be offloaded */
static void
remove_vlan_offload(struct rte_mbuf *pkts[], unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];

		if (m->ol_flags & mirror_ingress_dynflag) {
			if (m->ol_flags & RTE_MBUF_F_RX_VLAN_STRIPPED)
				vlan_insert(m, RTE_ETHER_TYPE_VLAN, m->vlan_tci);
			if (m->ol_flags & RTE_MBUF_F_RX_QINQ_STRIPPED)
				vlan_insert(m, RTE_ETHER_TYPE_QINQ, m->vlan_tci_outer);
		}
		if (m->ol_flags & mirror_egress_dynflag) {
			if (m->ol_flags & RTE_MBUF_F_TX_VLAN)
				vlan_insert(m, RTE_ETHER_TYPE_VLAN, m->vlan_tci);
			if (m->ol_flags & RTE_MBUF_F_TX_QINQ)
				vlan_insert(m, RTE_ETHER_TYPE_QINQ, m->vlan_tci_outer);

		}
	}
}

static ssize_t
process_burst(uint16_t in_port, dumpcap_out_t out,
	      struct rte_mbuf *pkts[], unsigned int n)
{
	static bool warn_once = true;
	struct interface *intf;
	ssize_t written;

	if (in_port >= RTE_MAX_ETHPORTS)
		goto invalid_port;

	intf = port2intf[in_port];
	if (intf == NULL)
		goto invalid_port;

	remove_vlan_offload(pkts, n);

	if (intf->filter) {
		n = filter_burst(intf, pkts, n);
		if (n == 0)
			return 0;
	}

	packets_received += n;
	if (use_pcapng)
		written = pcapng_write_packets(out.pcapng, pkts, n);
	else
		written = pcap_write_packets(out.dumper, pkts, n);

	if (written > 0)
		file_size += written;

	rte_pktmbuf_free_bulk(pkts, n);
	return written;

invalid_port:
	if (warn_once) {
		warn_once = false;
		fprintf(stderr, "Packet in ring from invalid port %u\n", in_port);
	}

	rte_pktmbuf_free_bulk(pkts, n);
	return 0;
}

/* Process all packets in ring and dump to capture file */
static int process_ring(dumpcap_out_t out, struct rte_ring *ring)
{
	struct rte_mbuf *pkts[BURST_SIZE];
	uint16_t in_port;
	unsigned int i, n, start, avail;
	static unsigned int empty_count;
	int ret;

	n = rte_ring_sc_dequeue_burst(ring, (void **)pkts, BURST_SIZE, &avail);
	if (n == 0) {
		/* don't consume endless amounts of cpu if idle */
		if (empty_count < SLEEP_THRESHOLD)
			++empty_count;
		else
			usleep(10);
		return 0;
	}

	empty_count = (avail == 0);

	/* process all packets mirrored from same port */
	start = 0;
	in_port = pkts[start]->port;
	for (i = 1; i < n; i++) {
		if (likely(pkts[i]->port == in_port))
			continue;
		ret = process_burst(in_port, out, pkts + start, i - start);
		if (ret < 0)
			return ret;	/* stop on file write error */

		start = i;
		in_port = pkts[i]->port;
	}

	ret = process_burst(in_port, out, pkts + start, n - start);
	if (ret < 0)
		return ret;

	if (!quiet)
		show_count(packets_received);

	return 0;
}

int main(int argc, char **argv)
{
	struct rte_mempool *mp;
	struct sigaction action = {
		.sa_flags = SA_RESTART,
		.sa_handler = signal_handler,
	};
	struct sigaction origaction;
	struct rte_ring *ring = NULL;
	char vdev_name[RTE_DEV_NAME_MAX_LEN];
	uint16_t mirror_port = UINT16_MAX;
	dumpcap_out_t out;
	char *p;
	int ret;

	p = strrchr(argv[0], '/');
	if (p == NULL)
		progname = argv[0];
	else
		progname = p + 1;

	parse_opts(argc, argv);
	dpdk_init();

	if (show_interfaces)
		dump_interfaces();

	if (rte_eth_dev_count_avail() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports found\n");

	if (TAILQ_EMPTY(&interfaces))
		set_default_interface();
	else
		find_interfaces();

	compile_filters();

	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGPIPE, &action, NULL);
	sigaction(SIGHUP, NULL, &origaction);
	if (origaction.sa_handler == SIG_DFL)
		sigaction(SIGHUP, &action, NULL);

	enable_primary_monitor();

	if (print_stats) {
		statistics_loop();
		exit(0);
	}

	mp = create_mempool();
	ring = create_ring_dev(vdev_name, &mirror_port);
	out = create_output();

	ret = enable_mirror(mirror_port, mp);
	if (ret < 0)
		goto cleanup;

	setup_mbuf();

	show_capturing(ret);

	start_time = time(NULL);

	if (!quiet) {
		fprintf(stderr, "Packets captured: ");
		show_count(0);
	}

	while (!rte_atomic_load_explicit(&quit_signal, rte_memory_order_relaxed)) {
		if (process_ring(out, ring) < 0) {
			fprintf(stderr, "pcapng file write failed; %s\n",
				strerror(errno));
			break;
		}

		if (stop.size && file_size >= stop.size)
			break;

		if (stop.packets && packets_received >= stop.packets)
			break;

		if (stop.duration != 0 &&
		    time(NULL) - start_time > stop.duration)
			break;
	}

	if (rte_eal_primary_proc_alive(NULL))
		report_packet_stats(out);

cleanup:
	if (use_pcapng)
		rte_pcapng_close(out.pcapng);
	else
		pcap_dump_close(out.dumper);

	disable_primary_monitor();
	disable_mirror_ports(mirror_port);

	rte_eal_hotplug_remove("vdev", vdev_name);
	rte_ring_free(ring);
	rte_mempool_free(mp);

	return rte_eal_cleanup() ? EXIT_FAILURE : 0;
}
