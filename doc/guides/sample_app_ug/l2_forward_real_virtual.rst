..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _l2_fwd_app_real_and_virtual:

L2 Forwarding Sample Application (in Real and Virtualized Environments)
=======================================================================

The L2 Forwarding sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK) which
also takes advantage of Single Root I/O Virtualization (SR-IOV) features in a virtualized environment.

.. note::

    Please note that previously a separate L2 Forwarding in Virtualized Environments sample application was used,
    however, in later DPDK versions these sample applications have been merged.

Overview
--------

The L2 Forwarding sample application, which can operate in real and virtualized environments,
performs L2 forwarding for each packet that is received on an RX_PORT.
The destination port is the adjacent port from the enabled portmask, that is,
if the first four ports are enabled (portmask 0xf),
ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other.
Also, if MAC addresses updating is enabled, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX_PORT MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

This application can be used to benchmark performance using a traffic-generator, as shown in the :numref:`figure_l2_fwd_benchmark_setup`,
or in a virtualized environment as shown in :numref:`figure_l2_fwd_virtenv_benchmark_setup`.

.. _figure_l2_fwd_benchmark_setup:

.. figure:: img/l2_fwd_benchmark_setup.*

   Performance Benchmark Setup (Basic Environment)

.. _figure_l2_fwd_virtenv_benchmark_setup:

.. figure:: img/l2_fwd_virtenv_benchmark_setup.*

   Performance Benchmark Setup (Virtualized Environment)

This application may be used for basic VM to VM communication as shown in :numref:`figure_l2_fwd_vm2vm`,
when MAC addresses updating is disabled.

.. _figure_l2_fwd_vm2vm:

.. figure:: img/l2_fwd_vm2vm.*

   Virtual Machine to Virtual Machine communication.

The L2 Forwarding application can also be used as a starting point for developing a new application based on the DPDK.

.. _l2_fwd_vf_setup:

Virtual Function Setup Instructions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This application can use the virtual function available in the system and
therefore can be used in a virtual machine without passing through
the whole Network Device into a guest machine in a virtualized scenario.
The virtual functions can be enabled in the host machine or the hypervisor with the respective physical function driver.

For example, in a Linux* host machine, it is possible to enable a virtual function using the following command:

.. code-block:: console

    modprobe ixgbe max_vfs=2,2

This command enables two Virtual Functions on each of Physical Function of the NIC,
with two physical ports in the PCI configuration space.
It is important to note that enabled Virtual Function 0 and 2 would belong to Physical Function 0
and Virtual Function 1 and 3 would belong to Physical Function 1,
in this case enabling a total of four Virtual Functions.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l2fwd`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd [EAL options] -- -p PORTMASK
                                   [-q NQ]
                                   --[no-]mac-updating
                                   [--portmap="(port, port)[,(port, port)]"]

where,

*   p PORTMASK: A hexadecimal bitmask of the ports to configure

*   q NQ: A number of queues (=ports) per lcore (default is 1)

*   --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)

*   --portmap="(port,port)[,(port,port)]": Determines forwarding ports mapping.

To run the application in linux environment with 4 lcores, 16 ports and 8 RX queues per lcore and MAC address
updating enabled, issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-l2fwd -l 0-3 -n 4 -- -q 8 -p ffff

To run the application in linux environment with 4 lcores, 4 ports, 8 RX queues
per lcore, to forward RX traffic of ports 0 & 1 on ports 2 & 3 respectively and
vice versa, issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-l2fwd -l 0-3 -n 4 -- -q 8 -p f --portmap="(0,2)(1,3)"

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

.. _l2_fwd_app_cmd_arguments:

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The L2 Forwarding sample application takes specific parameters,
in addition to Environment Abstraction Layer (EAL) arguments.
The preferred way to parse parameters is to use the getopt() function,
since it is part of a well-defined and portable library.

The parsing of arguments is done in the l2fwd_parse_args() function.
The method of argument parsing is not described here.
Refer to the *glibc getopt(3)* man page for details.

EAL arguments are parsed first, then application-specific arguments.
This is done at the beginning of the main() function:

.. code-block:: c

    /* init EAL */

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    argc -= ret;
    argv += ret;

    /* parse application arguments (after the EAL ones) */

    ret = l2fwd_parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

.. _l2_fwd_app_mbuf_init:

Mbuf Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

Once the arguments are parsed, the mbuf pool is created.
The mbuf pool contains a set of mbuf objects that will be used by the driver
and the application to store network packet data:

.. code-block:: c

    /* create the mbuf pool */

    l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
	MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
	rte_socket_id());

    if (l2fwd_pktmbuf_pool == NULL)
        rte_panic("Cannot init mbuf pool\n");

The rte_mempool is a generic structure used to handle pools of objects.
In this case, it is necessary to create a pool that will be used by the driver.
The number of allocated pkt mbufs is NB_MBUF, with a data room size of
RTE_MBUF_DEFAULT_BUF_SIZE each.
A per-lcore cache of 32 mbufs is kept.
The memory is allocated in NUMA socket 0,
but it is possible to extend this code to allocate one mbuf pool per socket.

The rte_pktmbuf_pool_create() function uses the default mbuf pool and mbuf
initializers, respectively rte_pktmbuf_pool_init() and rte_pktmbuf_init().
An advanced application may want to use the mempool API to create the
mbuf pool with more control.

.. _l2_fwd_app_dvr_init:

Driver Initialization
~~~~~~~~~~~~~~~~~~~~~

The main part of the code in the main() function relates to the initialization of the driver.
To fully understand this code, it is recommended to study the chapters that related to the Poll Mode Driver
in the *DPDK Programmer's Guide* - Rel 1.4 EAR and the *DPDK API Reference*.

.. code-block:: c

    /* reset l2fwd_dst_ports */

    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
        l2fwd_dst_ports[portid] = 0;

    last_port = 0;

    /*
     * Each logical core is assigned a dedicated TX queue on each port.
     */

    RTE_ETH_FOREACH_DEV(portid) {
        /* skip ports that are not enabled */

        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
           continue;

        if (nb_ports_in_mask % 2) {
            l2fwd_dst_ports[portid] = last_port;
            l2fwd_dst_ports[last_port] = portid;
        }
        else
           last_port = portid;

        nb_ports_in_mask++;

        rte_eth_dev_info_get((uint8_t) portid, &dev_info);
    }

The next step is to configure the RX and TX queues.
For each port, there is only one RX queue (only one lcore is able to poll a given port).
The number of TX queues depends on the number of available lcores.
The rte_eth_dev_configure() function is used to configure the number of queues for a port:

.. code-block:: c

    ret = rte_eth_dev_configure((uint8_t)portid, 1, 1, &port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: "
            "err=%d, port=%u\n",
            ret, portid);

.. _l2_fwd_app_rx_init:

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q option,
which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four ports with one lcore.
If there are 16 ports on the target (and if the portmask argument is -p ffff ),
the application will need four lcores to poll all the ports.

.. code-block:: c

    ret = rte_eth_rx_queue_setup((uint8_t) portid, 0, nb_rxd, SOCKET0, &rx_conf, l2fwd_pktmbuf_pool);
    if (ret < 0)

        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: "
            "err=%d, port=%u\n",
            ret, portid);

The list of queues that must be polled for a given lcore is stored in a private structure called struct lcore_queue_conf.

.. code-block:: c

    struct lcore_queue_conf {
        unsigned n_rx_port;
        unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
        struct mbuf_table tx_mbufs[L2FWD_MAX_PORTS];
    } rte_cache_aligned;

    struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

The values n_rx_port and rx_port_list[] are used in the main packet processing loop
(see :ref:`l2_fwd_app_rx_tx_packets`).

.. _l2_fwd_app_tx_init:

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port. For every port, a single TX queue is initialized.

.. code-block:: c

    /* init one TX queue on each port */

    fflush(stdout);

    ret = rte_eth_tx_queue_setup((uint8_t) portid, 0, nb_txd, rte_eth_dev_socket_id(portid), &tx_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n", ret, (unsigned) portid);

The global configuration for TX queues is stored in a static structure:

.. code-block:: c

    static const struct rte_eth_txconf tx_conf = {
        .tx_thresh = {
            .pthresh = TX_PTHRESH,
            .hthresh = TX_HTHRESH,
            .wthresh = TX_WTHRESH,
        },
        .tx_free_thresh = RTE_TEST_TX_DESC_DEFAULT + 1, /* disable feature */
    };

.. _l2_fwd_app_rx_tx_packets:

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the l2fwd_main_loop() function, the main task is to read ingress packets from the RX queues.
This is done using the following code:

.. code-block:: c

    /*
     * Read packet from RX queues
     */

    for (i = 0; i < qconf->n_rx_port; i++) {
        portid = qconf->rx_port_list[i];
        nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,  pkts_burst, MAX_PKT_BURST);

        for (j = 0; j < nb_rx; j++) {
            m = pkts_burst[j];
            rte_prefetch0[rte_pktmbuf_mtod(m, void *)); l2fwd_simple_forward(m, portid);
        }
    }

Packets are read in a burst of size MAX_PKT_BURST.
The rte_eth_rx_burst() function writes the mbuf pointers in a local table and returns the number of available mbufs in the table.

Then, each mbuf in the table is processed by the l2fwd_simple_forward() function.
The processing is very simple: process the TX port from the RX port, then replace the source and destination MAC addresses if MAC
addresses updating is enabled.

.. note::

    In the following code, one line for getting the output port requires some explanation.

During the initialization process, a static array of destination ports (l2fwd_dst_ports[]) is filled such that for each source port,
a destination port is assigned that is either the next or previous enabled port from the portmask.
Naturally, the number of ports in the portmask must be even, otherwise, the application exits.

.. code-block:: c

    static void
    l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
    {
        struct rte_ether_hdr *eth;
        void *tmp;
        unsigned dst_port;

        dst_port = l2fwd_dst_ports[portid];

        eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

        /* 02:00:00:00:00:xx */

        tmp = &eth->d_addr.addr_bytes[0];

        *((uint64_t *)tmp) = 0x000000000002 + ((uint64_t) dst_port << 40);

        /* src addr */

        rte_ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

        l2fwd_send_packet(m, (uint8_t) dst_port);
    }

Then, the packet is sent using the l2fwd_send_packet (m, dst_port) function.
For this test application, the processing is exactly the same for all packets arriving on the same RX port.
Therefore, it would have been possible to call the l2fwd_send_burst() function directly from the main loop
to send all the received packets on the same TX port,
using the burst-oriented send function, which is more efficient.

However, in real-life applications (such as, L3 routing),
packet N is not necessarily forwarded on the same port as packet N-1.
The application is implemented to illustrate that, so the same approach can be reused in a more complex application.

The l2fwd_send_packet() function stores the packet in a per-lcore and per-txport table.
If the table is full, the whole packets table is transmitted using the l2fwd_send_burst() function:

.. code-block:: c

    /* Send the packet on an output interface */

    static int
    l2fwd_send_packet(struct rte_mbuf *m, uint16_t port)
    {
        unsigned lcore_id, len;
        struct lcore_queue_conf *qconf;

        lcore_id = rte_lcore_id();
        qconf = &lcore_queue_conf[lcore_id];
        len = qconf->tx_mbufs[port].len;
        qconf->tx_mbufs[port].m_table[len] = m;
        len++;

        /* enough pkts to be sent */

        if (unlikely(len == MAX_PKT_BURST)) {
            l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
            len = 0;
        }

        qconf->tx_mbufs[port].len = len; return 0;
    }

To ensure that no packets remain in the tables, each lcore does a draining of TX queue in its main loop.
This technique introduces some latency when there are not many packets to send,
however it improves performance:

.. code-block:: c

    cur_tsc = rte_rdtsc();

    /*
     *   TX burst queue drain
     */

    diff_tsc = cur_tsc - prev_tsc;

    if (unlikely(diff_tsc > drain_tsc)) {
        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
            if (qconf->tx_mbufs[portid].len == 0)
                continue;

            l2fwd_send_burst(&lcore_queue_conf[lcore_id], qconf->tx_mbufs[portid].len, (uint8_t) portid);

            qconf->tx_mbufs[portid].len = 0;
        }

        /* if timer is enabled */

        if (timer_period > 0) {
            /* advance the timer */

            timer_tsc += diff_tsc;

            /* if timer has reached its timeout */

            if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
                /* do this only on main core */
                if (lcore_id == rte_get_main_lcore()) {
                    print_stats();

                    /* reset the timer */
                    timer_tsc = 0;
                }
            }
        }

        prev_tsc = cur_tsc;
    }
