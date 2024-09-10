#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_tailq.h>

#define SNIFFER_LOG_FILE "/var/log/sniffer.log"

static volatile bool force_quit  = false;

static int fwd_lanuch_one_lcore(__rte_unused void *dummy)
{
	while (!force_quit) {
		run_vswitch();
	}
	return 0;
}

static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

static int init_dev() {
    uint16_t port;
    char name[RTE_ETH_NAME_MAX_LEN];

    RTE_ETH_FOREACH_DEV(port){
        rte_eth_dev_get_name_by_port(port, name);
        printf("port:%s portid: %d\n", name, port);
    }
}

int main(int argc, char **argv)
{
	int ret;
	FILE *fp;
	uint16_t portid;
	unsigned lcoreid;

	fp = fopen(PMD_LOG_FILE, "w+");
	if (fp) {
		rte_openlog_stream(fp);
	}

	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	}

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	argc -= ret;
	argv += ret;
	if (argc > 1)
		parse_args(argc, argv);

	// ret = init_vswitch();
	// if (ret) {
	// 	rte_exit(EXIT_FAILURE, "Error with init vswitch\n");
	// }

	ret = init_dev();
	if (ret) {
		rte_exit(EXIT_FAILURE, "Error with init netdev\n");
	}

	// ret = rte_eth_dev_callback_register(RTE_ETH_ALL, RTE_ETH_EVENT_NEW,
	// 	gmnet2_eth_event_callback, NULL);
	// if (ret) {
	// 	rte_exit(EXIT_FAILURE, "Error register callback event_new\n");
	// }

	//ret = rte_eth_dev_callback_register(RTE_ETH_ALL, RTE_ETH_EVENT_QUEUE_STATE,
	//	gmnet2_eth_event_callback, NULL);
	//if (ret) {
	//	rte_exit(EXIT_FAILURE, "Error register callback event_queue_state\n");
	//}

	rte_eal_mp_remote_launch(fwd_lanuch_one_lcore, NULL, SKIP_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcoreid) {
        if (rte_eal_wait_lcore(lcoreid) < 0) {
            ret = -EINVAL;
            break;
        }
    }
	force_quit = true;

	RTE_ETH_FOREACH_DEV(portid) {
		printf("Closing port %d...\n", portid);
		ret = rte_eth_dev_stop(portid);
		if (ret)
			printf("rte_eth_dev_stop: err=%d, port=%d\n", 
				ret, portid);
		//rte_eth_dev_close(portid);
		printf("Done\n");
	}
	printf("Bye...\n");

	return ret;
}