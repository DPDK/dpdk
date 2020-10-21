..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

VMDQ and DCB Forwarding Sample Application
==========================================

The VMDQ and DCB Forwarding sample application is a simple example of packet processing using the DPDK.
The application performs L2 forwarding using VMDQ and DCB to divide the incoming traffic into queues.
The traffic splitting is performed in hardware by the VMDQ and DCB features of the Intel® 82599 and X710/XL710 Ethernet Controllers.

Overview
--------

This sample application can be used as a starting point for developing a new application that is based on the DPDK and
uses VMDQ and DCB for traffic partitioning.

The VMDQ and DCB filters work on MAC and VLAN traffic to divide the traffic into input queues on the basis of the Destination MAC
address, VLAN ID and VLAN user priority fields.
VMDQ filters split the traffic into 16 or 32 groups based on the Destination MAC and VLAN ID.
Then, DCB places each packet into one of queues within that group, based upon the VLAN user priority field.

All traffic is read from a single incoming port (port 0) and output on port 1, without any processing being performed.
With Intel® 82599 NIC, for example, the traffic is split into 128 queues on input, where each thread of the application reads from
multiple queues. When run with 8 threads, that is, with the -c FF option, each thread receives and forwards packets from 16 queues.

As supplied, the sample application configures the VMDQ feature to have 32 pools with 4 queues each as indicated in :numref:`figure_vmdq_dcb_example`.
The Intel® 82599 10 Gigabit Ethernet Controller NIC also supports the splitting of traffic into 16 pools of 8 queues. While the
Intel® X710 or XL710 Ethernet Controller NICs support many configurations of VMDQ pools of 4 or 8 queues each. For simplicity, only 16
or 32 pools is supported in this sample. And queues numbers for each VMDQ pool can be changed by setting RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM
in config/rte_config.h file.
The nb-pools, nb-tcs and enable-rss parameters can be passed on the command line, after the EAL parameters:

.. code-block:: console

    ./<build_dir>/examples/dpdk-vmdq_dcb [EAL options] -- -p PORTMASK --nb-pools NP --nb-tcs TC --enable-rss

where, NP can be 16 or 32, TC can be 4 or 8, rss is disabled by default.

.. _figure_vmdq_dcb_example:

.. figure:: img/vmdq_dcb_example.*

   Packet Flow Through the VMDQ and DCB Sample Application


In Linux* user space, the application can display statistics with the number of packets received on each queue.
To have the application display the statistics, send a SIGHUP signal to the running application process.

The VMDQ and DCB Forwarding sample application is in many ways simpler than the L2 Forwarding application
(see :doc:`l2_forward_real_virtual`)
as it performs unidirectional L2 forwarding of packets from one port to a second port.
No command-line options are taken by this application apart from the standard EAL command-line options.

.. note::

    Since VMD queues are being used for VMM, this application works correctly
    when VTd is disabled in the BIOS or Linux* kernel (intel_iommu=off).

Compiling the Application
-------------------------



To compile the sample application see :doc:`compiling`.

The application is located in the ``vmdq_dcb`` sub-directory.

Running the Application
-----------------------

To run the example in a linux environment:

.. code-block:: console

    user@target:~$ ./<build_dir>/examples/dpdk-vmdq_dcb -l 0-3 -n 4 -- -p 0x3 --nb-pools 32 --nb-tcs 4

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

The VMDQ and DCB hardware feature is configured at port initialization time by setting the appropriate values in the
rte_eth_conf structure passed to the rte_eth_dev_configure() API.
Initially in the application,
a default structure is provided for VMDQ and DCB configuration to be filled in later by the application.

.. code-block:: c

    /* empty vmdq+dcb configuration structure. Filled in programmatically */
    static const struct rte_eth_conf vmdq_dcb_conf_default = {
        .rxmode = {
            .mq_mode        = ETH_MQ_RX_VMDQ_DCB,
            .split_hdr_size = 0,
        },
        .txmode = {
            .mq_mode = ETH_MQ_TX_VMDQ_DCB,
        },
        /*
         * should be overridden separately in code with
         * appropriate values
         */
        .rx_adv_conf = {
            .vmdq_dcb_conf = {
                .nb_queue_pools = ETH_32_POOLS,
                .enable_default_pool = 0,
                .default_pool = 0,
                .nb_pool_maps = 0,
                .pool_map = {{0, 0},},
                .dcb_tc = {0},
            },
            .dcb_rx_conf = {
                .nb_tcs = ETH_4_TCS,
                /** Traffic class each UP mapped to. */
                .dcb_tc = {0},
            },
            .vmdq_rx_conf = {
                .nb_queue_pools = ETH_32_POOLS,
                .enable_default_pool = 0,
                .default_pool = 0,
                .nb_pool_maps = 0,
                .pool_map = {{0, 0},},
            },
        },
        .tx_adv_conf = {
            .vmdq_dcb_tx_conf = {
                .nb_queue_pools = ETH_32_POOLS,
                .dcb_tc = {0},
            },
        },
    };

The get_eth_conf() function fills in an rte_eth_conf structure with the appropriate values,
based on the global vlan_tags array,
and dividing up the possible user priority values equally among the individual queues
(also referred to as traffic classes) within each pool. With Intel® 82599 NIC,
if the number of pools is 32, then the user priority fields are allocated 2 to a queue.
If 16 pools are used, then each of the 8 user priority fields is allocated to its own queue within the pool.
With Intel® X710/XL710 NICs, if number of tcs is 4, and number of queues in pool is 8,
then the user priority fields are allocated 2 to one tc, and a tc has 2 queues mapping to it, then
RSS will determine the destination queue in 2.
For the VLAN IDs, each one can be allocated to possibly multiple pools of queues,
so the pools parameter in the rte_eth_vmdq_dcb_conf structure is specified as a bitmask value.
For destination MAC, each VMDQ pool will be assigned with a MAC address. In this sample, each VMDQ pool
is assigned to the MAC like 52:54:00:12:<port_id>:<pool_id>, that is,
the MAC of VMDQ pool 2 on port 1 is 52:54:00:12:01:02.

.. code-block:: c

    const uint16_t vlan_tags[] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31
    };

    /* pool mac addr template, pool mac addr is like: 52 54 00 12 port# pool# */
    static struct rte_ether_addr pool_addr_template = {
        .addr_bytes = {0x52, 0x54, 0x00, 0x12, 0x00, 0x00}
    };

    /* Builds up the correct configuration for vmdq+dcb based on the vlan tags array
     * given above, and the number of traffic classes available for use. */
    static inline int
    get_eth_conf(struct rte_eth_conf *eth_conf)
    {
        struct rte_eth_vmdq_dcb_conf conf;
        struct rte_eth_vmdq_rx_conf  vmdq_conf;
        struct rte_eth_dcb_rx_conf   dcb_conf;
        struct rte_eth_vmdq_dcb_tx_conf tx_conf;
        uint8_t i;

        conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
        vmdq_conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
        tx_conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
        conf.nb_pool_maps = num_pools;
        vmdq_conf.nb_pool_maps = num_pools;
        conf.enable_default_pool = 0;
        vmdq_conf.enable_default_pool = 0;
        conf.default_pool = 0; /* set explicit value, even if not used */
        vmdq_conf.default_pool = 0;

        for (i = 0; i < conf.nb_pool_maps; i++) {
            conf.pool_map[i].vlan_id = vlan_tags[i];
            vmdq_conf.pool_map[i].vlan_id = vlan_tags[i];
            conf.pool_map[i].pools = 1UL << i ;
            vmdq_conf.pool_map[i].pools = 1UL << i;
        }
        for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++){
            conf.dcb_tc[i] = i % num_tcs;
            dcb_conf.dcb_tc[i] = i % num_tcs;
            tx_conf.dcb_tc[i] = i % num_tcs;
        }
        dcb_conf.nb_tcs = (enum rte_eth_nb_tcs)num_tcs;
        (void)(rte_memcpy(eth_conf, &vmdq_dcb_conf_default, sizeof(*eth_conf)));
        (void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_dcb_conf, &conf,
                  sizeof(conf)));
        (void)(rte_memcpy(&eth_conf->rx_adv_conf.dcb_rx_conf, &dcb_conf,
                  sizeof(dcb_conf)));
        (void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &vmdq_conf,
                  sizeof(vmdq_conf)));
        (void)(rte_memcpy(&eth_conf->tx_adv_conf.vmdq_dcb_tx_conf, &tx_conf,
                  sizeof(tx_conf)));
        if (rss_enable) {
            eth_conf->rxmode.mq_mode= ETH_MQ_RX_VMDQ_DCB_RSS;
            eth_conf->rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP |
                                ETH_RSS_UDP |
                                ETH_RSS_TCP |
                                ETH_RSS_SCTP;
        }
        return 0;
    }

    ......

    /* Set mac for each pool.*/
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

Once the network port has been initialized using the correct VMDQ and DCB values,
the initialization of the port's RX and TX hardware rings is performed similarly to that
in the L2 Forwarding sample application.
See :doc:`l2_forward_real_virtual` for more information.

Statistics Display
~~~~~~~~~~~~~~~~~~

When run in a linux environment,
the VMDQ and DCB Forwarding sample application can display statistics showing the number of packets read from each RX queue.
This is provided by way of a signal handler for the SIGHUP signal,
which simply prints to standard output the packet counts in grid form.
Each row of the output is a single pool with the columns being the queue number within that pool.

To generate the statistics output, use the following command:

.. code-block:: console

    user@host$ sudo killall -HUP vmdq_dcb_app

Please note that the statistics output will appear on the terminal where the vmdq_dcb_app is running,
rather than the terminal from which the HUP signal was sent.
