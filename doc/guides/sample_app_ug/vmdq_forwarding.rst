..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

VMDq Forwarding Sample Application
==========================================

The VMDq Forwarding sample application is a simple example of packet processing using the DPDK.
The application performs L2 forwarding using VMDq to divide the incoming traffic into queues.
The traffic splitting is performed in hardware by the VMDq feature of the Intel® 82599 and X710/XL710 Ethernet Controllers.

Overview
--------

This sample application can be used as a starting point for developing a new application that is based on the DPDK and
uses VMDq for traffic partitioning.

VMDq filters split the incoming packets up into different "pools" - each with its own set of RX queues - based upon
the MAC address and VLAN ID within the VLAN tag of the packet.

All traffic is read from a single incoming port and output on another port, without any processing being performed.
With Intel® 82599 NIC, for example, the traffic is split into 128 queues on input, where each thread of the application reads from
multiple queues. When run with 8 threads, that is, with the -c FF option, each thread receives and forwards packets from 16 queues.

As supplied, the sample application configures the VMDq feature to have 32 pools with 4 queues each.
The Intel® 82599 10 Gigabit Ethernet Controller NIC also supports the splitting of traffic into 16 pools of 2 queues.
While the Intel® X710 or XL710 Ethernet Controller NICs support many configurations of VMDq pools of 4 or 8 queues each.
And queues numbers for each VMDq pool can be changed by setting RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM
in config/rte_config.h file.
The nb-pools and enable-rss parameters can be passed on the command line, after the EAL parameters:

.. code-block:: console

    ./<build_dir>/examples/dpdk-vmdq [EAL options] -- -p PORTMASK --nb-pools NP --enable-rss

where, NP can be 8, 16 or 32, rss is disabled by default.

In Linux* user space, the application can display statistics with the number of packets received on each queue.
To have the application display the statistics, send a SIGHUP signal to the running application process.

The VMDq Forwarding sample application is in many ways simpler than the L2 Forwarding application
(see :doc:`l2_forward_real_virtual`)
as it performs unidirectional L2 forwarding of packets from one port to a second port.
No command-line options are taken by this application apart from the standard EAL command-line options.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``vmdq`` sub-directory.

Running the Application
-----------------------

To run the example in a Linux environment:

.. code-block:: console

    user@target:~$ ./<build_dir>/examples/dpdk-vmdq -l 0-3 -n 4 -- -p 0x3 --nb-pools 16

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization
~~~~~~~~~~~~~~

The EAL, driver and PCI configuration is performed largely as in the L2 Forwarding sample application,
as is the creation of the mbuf pool.
See :doc:`l2_forward_real_virtual`.
Where this example application differs is in the configuration of the NIC port for RX.

The VMDq hardware feature is configured at port initialization time by setting the appropriate values in the
rte_eth_conf structure passed to the rte_eth_dev_configure() API.
Initially in the application,
a default structure is provided for VMDq configuration to be filled in later by the application.

.. code-block:: c

    /* empty vmdq configuration structure. Filled in programmatically */
    static const struct rte_eth_conf vmdq_conf_default = {
        .rxmode = {
            .mq_mode        = ETH_MQ_RX_VMDQ_ONLY,
            .split_hdr_size = 0,
        },

        .txmode = {
            .mq_mode = ETH_MQ_TX_NONE,
        },
        .rx_adv_conf = {
            /*
            * should be overridden separately in code with
            * appropriate values
            */
            .vmdq_rx_conf = {
                .nb_queue_pools = ETH_8_POOLS,
                .enable_default_pool = 0,
                .default_pool = 0,
                .nb_pool_maps = 0,
                .pool_map = {{0, 0},},
            },
        },
    };

The get_eth_conf() function fills in an rte_eth_conf structure with the appropriate values,
based on the global vlan_tags array.
For the VLAN IDs, each one can be allocated to possibly multiple pools of queues.
For destination MAC, each VMDq pool will be assigned with a MAC address. In this sample, each VMDq pool
is assigned to the MAC like 52:54:00:12:<port_id>:<pool_id>, that is,
the MAC of VMDq pool 2 on port 1 is 52:54:00:12:01:02.

.. code-block:: c

    const uint16_t vlan_tags[] = {
        0,  1,  2,  3,  4,  5,  6,  7,
        8,  9, 10, 11,	12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63,
    };

    /* pool mac addr template, pool mac addr is like: 52 54 00 12 port# pool# */
    static struct rte_ether_addr pool_addr_template = {
        .addr_bytes = {0x52, 0x54, 0x00, 0x12, 0x00, 0x00}
    };

    /*
     * Builds up the correct configuration for vmdq based on the vlan tags array
     * given above, and determine the queue number and pool map number according to
     * valid pool number
     */
    static inline int
    get_eth_conf(struct rte_eth_conf *eth_conf, uint32_t num_pools)
    {
        struct rte_eth_vmdq_rx_conf conf;
        unsigned i;

        conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
        conf.nb_pool_maps = num_pools;
        conf.enable_default_pool = 0;
        conf.default_pool = 0; /* set explicit value, even if not used */

        for (i = 0; i < conf.nb_pool_maps; i++) {
            conf.pool_map[i].vlan_id = vlan_tags[i];
            conf.pool_map[i].pools = (1UL << (i % num_pools));
        }

        (void)(rte_memcpy(eth_conf, &vmdq_conf_default, sizeof(*eth_conf)));
        (void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &conf,
            sizeof(eth_conf->rx_adv_conf.vmdq_rx_conf)));
        return 0;
    }

    ......

    /*
     * Set mac for each pool.
     * There is no default mac for the pools in i40.
     * Removes this after i40e fixes this issue.
     */
    for (q = 0; q < num_pools; q++) {
    	struct rte_ether_addr mac;
    	mac = pool_addr_template;
    	mac.addr_bytes[4] = port;
    	mac.addr_bytes[5] = q;
    	printf("Port %u vmdq pool %u set mac %02x:%02x:%02x:%02x:%02x:%02x\n",
    		port, q,
    		mac.addr_bytes[0], mac.addr_bytes[1],
    		mac.addr_bytes[2], mac.addr_bytes[3],
    		mac.addr_bytes[4], mac.addr_bytes[5]);
    	retval = rte_eth_dev_mac_addr_add(port, &mac,
    			q + vmdq_pool_base);
    	if (retval) {
    		printf("mac addr add failed at pool %d\n", q);
    		return retval;
    	}
    }

Once the network port has been initialized using the correct VMDq values,
the initialization of the port's RX and TX hardware rings is performed similarly to that
in the L2 Forwarding sample application.
See :doc:`l2_forward_real_virtual` for more information.

Statistics Display
~~~~~~~~~~~~~~~~~~

When run in a Linux environment,
the VMDq Forwarding sample application can display statistics showing the number of packets read from each RX queue.
This is provided by way of a signal handler for the SIGHUP signal,
which simply prints to standard output the packet counts in grid form.
Each row of the output is a single pool with the columns being the queue number within that pool.

To generate the statistics output, use the following command:

.. code-block:: console

    user@host$ sudo killall -HUP vmdq_app

Please note that the statistics output will appear on the terminal where the vmdq_app is running,
rather than the terminal from which the HUP signal was sent.
