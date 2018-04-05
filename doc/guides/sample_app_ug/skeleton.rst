..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Basic Forwarding Sample Application
===================================

The Basic Forwarding sample application is a simple *skeleton* example of a
forwarding application.

It is intended as a demonstration of the basic components of a DPDK forwarding
application. For more detailed implementations see the L2 and L3 forwarding
sample applications.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``skeleton`` sub-directory.

Running the Application
-----------------------

To run the example in a ``linuxapp`` environment:

.. code-block:: console

    ./build/basicfwd -l 1 -n 4

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. code-block:: c

    mbuf_pool = rte_mempool_create("MBUF_POOL",
                                   NUM_MBUFS * nb_ports,
                                   MBUF_SIZE,
                                   MBUF_CACHE_SIZE,
                                   sizeof(struct rte_pktmbuf_pool_private),
                                   rte_pktmbuf_pool_init, NULL,
                                   rte_pktmbuf_init,      NULL,
                                   rte_socket_id(),
                                   0);

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function which is explained in the next section:

.. code-block:: c

    RTE_ETH_FOREACH_DEV(portid) {
        if (port_init(portid, mbuf_pool) != 0) {
            rte_exit(EXIT_FAILURE,
                     "Cannot init port %" PRIu8 "\n", portid);
        }
    }


Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.


.. code-block:: c

	lcore_main();

The ``lcore_main()`` function is explained below.



The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the Basic
Forwarding application is shown below:

.. code-block:: c

    static inline int
    port_init(uint16_t port, struct rte_mempool *mbuf_pool)
    {
        struct rte_eth_conf port_conf = port_conf_default;
        const uint16_t rx_rings = 1, tx_rings = 1;
        struct ether_addr addr;
        int retval;
        uint16_t q;

        if (!rte_eth_dev_is_valid_port(port))
            return -1;

        /* Configure the Ethernet device. */
        retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
        if (retval != 0)
            return retval;

        /* Allocate and set up 1 RX queue per Ethernet port. */
        for (q = 0; q < rx_rings; q++) {
            retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL, mbuf_pool);
            if (retval < 0)
                return retval;
        }

        /* Allocate and set up 1 TX queue per Ethernet port. */
        for (q = 0; q < tx_rings; q++) {
            retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL);
            if (retval < 0)
                return retval;
        }

        /* Start the Ethernet port. */
        retval = rte_eth_dev_start(port);
        if (retval < 0)
            return retval;

        /* Enable RX in promiscuous mode for the Ethernet device. */
        rte_eth_promiscuous_enable(port);

        return 0;
    }

The Ethernet ports are configured with default settings using the
``rte_eth_dev_configure()`` function and the ``port_conf_default`` struct:

.. code-block:: c

    static const struct rte_eth_conf port_conf_default = {
        .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
    };

For this example the ports are set up with 1 RX and 1 TX queue using the
``rte_eth_rx_queue_setup()`` and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. code-block:: c

        retval  = rte_eth_dev_start(port);


Finally the RX port is set in promiscuous mode:

.. code-block:: c

        rte_eth_promiscuous_enable(port);


The Lcores Main
~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores. For the Basic Forwarding application the lcore function
looks like the following:

.. code-block:: c

    static __attribute__((noreturn)) void
    lcore_main(void)
    {
        uint16_t port;

        /*
         * Check that the port is on the same NUMA node as the polling thread
         * for best performance.
         */
        RTE_ETH_FOREACH_DEV(port)
            if (rte_eth_dev_socket_id(port) > 0 &&
                    rte_eth_dev_socket_id(port) !=
                            (int)rte_socket_id())
                printf("WARNING, port %u is on remote NUMA node to "
                        "polling thread.\n\tPerformance will "
                        "not be optimal.\n", port);

        printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
                rte_lcore_id());

        /* Run until the application is quit or killed. */
        for (;;) {
            /*
             * Receive packets on a port and forward them on the paired
             * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
             */
            RTE_ETH_FOREACH_DEV(port) {

                /* Get burst of RX packets, from first port of pair. */
                struct rte_mbuf *bufs[BURST_SIZE];
                const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                        bufs, BURST_SIZE);

                if (unlikely(nb_rx == 0))
                    continue;

                /* Send burst of TX packets, to second port of pair. */
                const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
                        bufs, nb_rx);

                /* Free any unsent packets. */
                if (unlikely(nb_tx < nb_rx)) {
                    uint16_t buf;
                    for (buf = nb_tx; buf < nb_rx; buf++)
                        rte_pktmbuf_free(bufs[buf]);
                }
            }
        }
    }


The main work of the application is done within the loop:

.. code-block:: c

        for (;;) {
            RTE_ETH_FOREACH_DEV(port) {

                /* Get burst of RX packets, from first port of pair. */
                struct rte_mbuf *bufs[BURST_SIZE];
                const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                        bufs, BURST_SIZE);

                if (unlikely(nb_rx == 0))
                    continue;

                /* Send burst of TX packets, to second port of pair. */
                const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
                        bufs, nb_rx);

                /* Free any unsent packets. */
                if (unlikely(nb_tx < nb_rx)) {
                    uint16_t buf;
                    for (buf = nb_tx; buf < nb_rx; buf++)
                        rte_pktmbuf_free(bufs[buf]);
                }
            }
        }

Packets are received in bursts on the RX ports and transmitted in bursts on
the TX ports. The ports are grouped in pairs with a simple mapping scheme
using the an XOR on the port number::

    0 -> 1
    1 -> 0

    2 -> 3
    3 -> 2

    etc.

The ``rte_eth_tx_burst()`` function frees the memory buffers of packets that
are transmitted. If packets fail to transmit, ``(nb_tx < nb_rx)``, then they
must be freed explicitly using ``rte_pktmbuf_free()``.

The forwarding loop can be interrupted and the application closed using
``Ctrl-C``.
