..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Link Status Interrupt Sample Application
========================================

The Link Status Interrupt sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK) that
demonstrates how network link status changes for a network port can be captured and
used by a DPDK application.

Overview
--------

The Link Status Interrupt sample application registers a user space callback for the link status interrupt of each port
and performs L2 forwarding for each packet that is received on an RX_PORT.
The following operations are performed:

*   RX_PORT and TX_PORT are paired with available ports one-by-one according to the core mask

*   The source MAC address is replaced by the TX_PORT MAC address

*   The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID

This application can be used to demonstrate the usage of link status interrupt and its user space callbacks
and the behavior of L2 forwarding each time the link status changes.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``link_status_interrupt`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./build/link_status_interrupt [EAL options] -- -p PORTMASK [-q NQ][-T PERIOD]

where,

*   -p PORTMASK: A hexadecimal bitmask of the ports to configure

*   -q NQ: A number of queues (=ports) per lcore (default is 1)

*   -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default)

To run the application in a linuxapp environment with 4 lcores, 4 memory channels, 16 ports and 8 RX queues per lcore,
issue the command:

.. code-block:: console

    $ ./build/link_status_interrupt -l 0-3 -n 4-- -q 8 -p ffff

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The Link Status Interrupt sample application takes specific parameters,
in addition to Environment Abstraction Layer (EAL) arguments (see Section `Running the Application`_).

Command line parsing is done in the same way as it is done in the L2 Forwarding Sample Application.
See :ref:`l2_fwd_app_cmd_arguments` for more information.

Mbuf Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

Mbuf pool initialization is done in the same way as it is done in the L2 Forwarding Sample Application.
See :ref:`l2_fwd_app_mbuf_init` for more information.

Driver Initialization
~~~~~~~~~~~~~~~~~~~~~

The main part of the code in the main() function relates to the initialization of the driver.
To fully understand this code, it is recommended to study the chapters that related to the Poll Mode Driver in the
*DPDK Programmer's Guide and the DPDK API Reference*.

.. code-block:: c

    if (rte_pci_probe() < 0)
        rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");

    /*
     * Each logical core is assigned a dedicated TX queue on each port.
     */

    RTE_ETH_FOREACH_DEV(portid) {
        /* skip ports that are not enabled */

        if ((lsi_enabled_port_mask & (1 << portid)) == 0)
            continue;

        /* save the destination port id */

        if (nb_ports_in_mask % 2) {
            lsi_dst_ports[portid] = portid_last;
            lsi_dst_ports[portid_last] = portid;
        }
        else
            portid_last = portid;

        nb_ports_in_mask++;

        rte_eth_dev_info_get((uint8_t) portid, &dev_info);
    }

Observe that:

*   rte_pci_probe()  parses the devices on the PCI bus and initializes recognized devices.

The next step is to configure the RX and TX queues.
For each port, there is only one RX queue (only one lcore is able to poll a given port).
The number of TX queues depends on the number of available lcores.
The rte_eth_dev_configure() function is used to configure the number of queues for a port:

.. code-block:: c

    ret = rte_eth_dev_configure((uint8_t) portid, 1, 1, &port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, portid);

The global configuration is stored in a static structure:

.. code-block:: c

    static const struct rte_eth_conf port_conf = {
        .rxmode = {
            .split_hdr_size = 0,
            .header_split = 0,   /**< Header Split disabled */
            .hw_ip_checksum = 0, /**< IP checksum offload disabled */
            .hw_vlan_filter = 0, /**< VLAN filtering disabled */
            .hw_strip_crc= 0,    /**< CRC stripped by hardware */
        },
        .txmode = {},
        .intr_conf = {
            .lsc = 1, /**< link status interrupt feature enabled */
        },
    };

Configuring lsc to 0 (the default) disables the generation of any link status change interrupts in kernel space
and no user space interrupt event is received.
The public interface rte_eth_link_get() accesses the NIC registers directly to update the link status.
Configuring lsc to non-zero enables the generation of link status change interrupts in kernel space
when a link status change is present and calls the user space callbacks registered by the application.
The public interface rte_eth_link_get() just reads the link status in a global structure
that would be updated in the interrupt host thread only.

Interrupt Callback Registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application can register one or more callbacks to a specific port and interrupt event.
An example callback function that has been written as indicated below.

.. code-block:: c

    static void
    lsi_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *param)
    {
        struct rte_eth_link link;

        RTE_SET_USED(param);

        printf("\n\nIn registered callback...\n");

        printf("Event type: %s\n", type == RTE_ETH_EVENT_INTR_LSC ? "LSC interrupt" : "unknown event");

        rte_eth_link_get_nowait(port_id, &link);

        if (link.link_status) {
            printf("Port %d Link Up - speed %u Mbps - %s\n\n", port_id, (unsigned)link.link_speed,
                  (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex"));
        } else
            printf("Port %d Link Down\n\n", port_id);
    }

This function is called when a link status interrupt is present for the right port.
The port_id indicates which port the interrupt applies to.
The type parameter identifies the interrupt event type,
which currently can be RTE_ETH_EVENT_INTR_LSC only, but other types can be added in the future.
The param parameter is the address of the parameter for the callback.
This function should be implemented with care since it will be called in the interrupt host thread,
which is different from the main thread of its caller.

The application registers the lsi_event_callback and a NULL parameter to the link status interrupt event on each port:

.. code-block:: c

    rte_eth_dev_callback_register((uint8_t)portid, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);

This registration can be done only after calling the rte_eth_dev_configure() function and before calling any other function.
If lsc is initialized with 0, the callback is never called since no interrupt event would ever be present.

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q option,
which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four ports with one lcore.
If there are 16 ports on the target (and if the portmask argument is -p ffff),
the application will need four lcores to poll all the ports.

.. code-block:: c

    ret = rte_eth_rx_queue_setup((uint8_t) portid, 0, nb_rxd, SOCKET0, &rx_conf, lsi_pktmbuf_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: err=%d, port=%u\n", ret, portid);

The list of queues that must be polled for a given lcore is stored in a private structure called struct lcore_queue_conf.

.. code-block:: c

    struct lcore_queue_conf {
        unsigned n_rx_port;
        unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE]; unsigned tx_queue_id;
        struct mbuf_table tx_mbufs[LSI_MAX_PORTS];
    } rte_cache_aligned;

    struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

The n_rx_port and rx_port_list[] fields are used in the main packet processing loop
(see `Receive, Process and Transmit Packets`_).

The global configuration for the RX queues is stored in a static structure:

.. code-block:: c

    static const struct rte_eth_rxconf rx_conf = {
        .rx_thresh = {
            .pthresh = RX_PTHRESH,
            .hthresh = RX_HTHRESH,
            .wthresh = RX_WTHRESH,
        },
    };

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port.
For every port, a single TX queue is initialized.

.. code-block:: c

    /* init one TX queue logical core on each port */

    fflush(stdout);

    ret = rte_eth_tx_queue_setup(portid, 0, nb_txd, rte_eth_dev_socket_id(portid), &tx_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d,port=%u\n", ret, (unsigned) portid);

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

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the lsi_main_loop() function, the main task is to read ingress packets from the RX queues.
This is done using the following code:

.. code-block:: c

    /*
     *   Read packet from RX queues
     */

    for (i = 0; i < qconf->n_rx_port; i++) {
        portid = qconf->rx_port_list[i];
        nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst, MAX_PKT_BURST);
        port_statistics[portid].rx += nb_rx;

        for (j = 0; j < nb_rx; j++) {
            m = pkts_burst[j];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            lsi_simple_forward(m, portid);
        }
    }

Packets are read in a burst of size MAX_PKT_BURST.
The rte_eth_rx_burst() function writes the mbuf pointers in a local table and returns the number of available mbufs in the table.

Then, each mbuf in the table is processed by the lsi_simple_forward() function.
The processing is very simple: processes the TX port from the RX port and then replaces the source and destination MAC addresses.

.. note::

    In the following code, the two lines for calculating the output port require some explanation.
    If portId is even, the first line does nothing (as portid & 1 will be 0), and the second line adds 1.
    If portId is odd, the first line subtracts one and the second line does nothing.
    Therefore, 0 goes to 1, and 1 to 0, 2 goes to 3 and 3 to 2, and so on.

.. code-block:: c

    static void
    lsi_simple_forward(struct rte_mbuf *m, unsigned portid)
    {
        struct ether_hdr *eth;
        void *tmp;
        unsigned dst_port = lsi_dst_ports[portid];

        eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

        /* 02:00:00:00:00:xx */

        tmp = &eth->d_addr.addr_bytes[0];

        *((uint64_t *)tmp) = 0x000000000002 + (dst_port << 40);

        /* src addr */
        ether_addr_copy(&lsi_ports_eth_addr[dst_port], &eth->s_addr);

        lsi_send_packet(m, dst_port);
    }

Then, the packet is sent using the lsi_send_packet(m, dst_port) function.
For this test application, the processing is exactly the same for all packets arriving on the same RX port.
Therefore, it would have been possible to call the lsi_send_burst() function directly from the main loop
to send all the received packets on the same TX port using
the burst-oriented send function, which is more efficient.

However, in real-life applications (such as, L3 routing),
packet N is not necessarily forwarded on the same port as packet N-1.
The application is implemented to illustrate that so the same approach can be reused in a more complex application.

The lsi_send_packet() function stores the packet in a per-lcore and per-txport table.
If the table is full, the whole packets table is transmitted using the lsi_send_burst() function:

.. code-block:: c

    /* Send the packet on an output interface */

    static int
    lsi_send_packet(struct rte_mbuf *m, uint16_t port)
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
            lsi_send_burst(qconf, MAX_PKT_BURST, port);
            len = 0;
        }
        qconf->tx_mbufs[port].len = len;

        return 0;
    }

To ensure that no packets remain in the tables, each lcore does a draining of the TX queue in its main loop.
This technique introduces some latency when there are not many packets to send.
However, it improves performance:

.. code-block:: c

    cur_tsc = rte_rdtsc();

    /*
     *    TX burst queue drain
     */

    diff_tsc = cur_tsc - prev_tsc;

    if (unlikely(diff_tsc > drain_tsc)) {
        /* this could be optimized (use queueid instead of * portid), but it is not called so often */

        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
            if (qconf->tx_mbufs[portid].len == 0)
                continue;

            lsi_send_burst(&lcore_queue_conf[lcore_id],
            qconf->tx_mbufs[portid].len, (uint8_t) portid);
            qconf->tx_mbufs[portid].len = 0;
        }

        /* if timer is enabled */

        if (timer_period > 0) {
            /* advance the timer */

            timer_tsc += diff_tsc;

            /* if timer has reached its timeout */

            if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
                /* do this only on master core */

                if (lcore_id == rte_get_master_lcore()) {
                    print_stats();

                    /* reset the timer */
                    timer_tsc = 0;
                }
            }
        }
        prev_tsc = cur_tsc;
   }
