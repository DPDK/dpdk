..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

.. include:: <isonum.txt>

Packet copying using Intel\ |reg| QuickData Technology
======================================================

Overview
--------

This sample is intended as a demonstration of the basic components of a DPDK
forwarding application and example of how to use IOAT driver API to make
packets copies.

Also while forwarding, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX port MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

This application can be used to compare performance of using software packet
copy with copy done using a DMA device for different sizes of packets.
The example will print out statistics each second. The stats shows
received/send packets and packets dropped or failed to copy.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ioat`` sub-directory.


Running the Application
-----------------------

In order to run the hardware copy application, the copying device
needs to be bound to user-space IO driver.

Refer to the "IOAT Rawdev Driver" chapter in the "Rawdev Drivers" document
for information on using the driver.

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ioat [EAL options] -- [-p MASK] [-q NQ] [-s RS] [-c <sw|hw>]
        [--[no-]mac-updating]

where,

*   p MASK: A hexadecimal bitmask of the ports to configure (default is all)

*   q NQ: Number of Rx queues used per port equivalent to CBDMA channels
    per port (default is 1)

*   c CT: Performed packet copy type: software (sw) or hardware using
    DMA (hw) (default is hw)

*   s RS: Size of IOAT rawdev ring for hardware copy mode or rte_ring for
    software copy mode (default is 2048)

*   --[no-]mac-updating: Whether MAC address of packets should be changed
    or not (default is mac-updating)

The application can be launched in various configurations depending on
provided parameters. The app can use up to 2 lcores: one of them receives
incoming traffic and makes a copy of each packet. The second lcore then
updates MAC address and sends the copy. If one lcore per port is used,
both operations are done sequentially. For each configuration an additional
lcore is needed since the main lcore does not handle traffic but is
responsible for configuration, statistics printing and safe shutdown of
all ports and devices.

The application can use a maximum of 8 ports.

To run the application in a Linux environment with 3 lcores (the main lcore,
plus two forwarding cores), a single port (port 0), software copying and MAC
updating issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-ioat -l 0-2 -n 2 -- -p 0x1 --mac-updating -c sw

To run the application in a Linux environment with 2 lcores (the main lcore,
plus one forwarding core), 2 ports (ports 0 and 1), hardware copying and no MAC
updating issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-ioat -l 0-1 -n 1 -- -p 0x3 --no-mac-updating -c hw

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with
``rte_`` and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).
The ``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    /* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. code-block:: c

    nb_mbufs = RTE_MAX(rte_eth_dev_count_avail() * (nb_rxd + nb_txd
        + MAX_PKT_BURST + rte_lcore_count() * MEMPOOL_CACHE_SIZE),
        MIN_POOL_SIZE);

    /* Create the mbuf pool */
    ioat_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
        MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (ioat_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes the ports:

.. code-block:: c

    /* Initialise each port */
    RTE_ETH_FOREACH_DEV(portid) {
        port_init(portid, ioat_pktmbuf_pool);
    }

Each port is configured using ``port_init()`` function. The Ethernet
ports are configured with local settings using the ``rte_eth_dev_configure()``
function and the ``port_conf`` struct. The RSS is enabled so that
multiple Rx queues could be used for packet receiving and copying by
multiple CBDMA channels per port:

.. code-block:: c

    /* configuring port to use RSS for multiple RX queues */
    static const struct rte_eth_conf port_conf = {
        .rxmode = {
            .mq_mode        = ETH_MQ_RX_RSS,
            .max_rx_pkt_len = RTE_ETHER_MAX_LEN
        },
        .rx_adv_conf = {
            .rss_conf = {
                .rss_key = NULL,
                .rss_hf = ETH_RSS_PROTO_MASK,
            }
        }
    };

For this example the ports are set up with the number of Rx queues provided
with -q option and 1 Tx queue using the ``rte_eth_rx_queue_setup()``
and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. code-block:: c

    ret = rte_eth_dev_start(portid);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
            ret, portid);


Finally the Rx port is set in promiscuous mode:

.. code-block:: c

    rte_eth_promiscuous_enable(portid);


After that each port application assigns resources needed.

.. code-block:: c

    check_link_status(ioat_enabled_port_mask);

    if (!cfg.nb_ports) {
        rte_exit(EXIT_FAILURE,
            "All available ports are disabled. Please set portmask.\n");
    }

    /* Check if there is enough lcores for all ports. */
    cfg.nb_lcores = rte_lcore_count() - 1;
    if (cfg.nb_lcores < 1)
        rte_exit(EXIT_FAILURE,
            "There should be at least one worker lcore.\n");

    ret = 0;

    if (copy_mode == COPY_MODE_IOAT_NUM) {
        assign_rawdevs();
    } else /* copy_mode == COPY_MODE_SW_NUM */ {
        assign_rings();
    }

Depending on mode set (whether copy should be done by software or by hardware)
special structures are assigned to each port. If software copy was chosen,
application have to assign ring structures for packet exchanging between lcores
assigned to ports.

.. code-block:: c

    static void
    assign_rings(void)
    {
        uint32_t i;

        for (i = 0; i < cfg.nb_ports; i++) {
            char ring_name[20];

            snprintf(ring_name, 20, "rx_to_tx_ring_%u", i);
            /* Create ring for inter core communication */
            cfg.ports[i].rx_to_tx_ring = rte_ring_create(
                    ring_name, ring_size,
                    rte_socket_id(), RING_F_SP_ENQ);

            if (cfg.ports[i].rx_to_tx_ring == NULL)
                rte_exit(EXIT_FAILURE, "%s\n",
                        rte_strerror(rte_errno));
        }
    }


When using hardware copy each Rx queue of the port is assigned an
IOAT device (``assign_rawdevs()``) using IOAT Rawdev Driver API
functions:

.. code-block:: c

    static void
    assign_rawdevs(void)
    {
        uint16_t nb_rawdev = 0, rdev_id = 0;
        uint32_t i, j;

        for (i = 0; i < cfg.nb_ports; i++) {
            for (j = 0; j < cfg.ports[i].nb_queues; j++) {
                struct rte_rawdev_info rdev_info = { 0 };

                do {
                    if (rdev_id == rte_rawdev_count())
                        goto end;
                    rte_rawdev_info_get(rdev_id++, &rdev_info, 0);
                } while (strcmp(rdev_info.driver_name,
                    IOAT_PMD_RAWDEV_NAME_STR) != 0);

                cfg.ports[i].ioat_ids[j] = rdev_id - 1;
                configure_rawdev_queue(cfg.ports[i].ioat_ids[j]);
                ++nb_rawdev;
            }
        }
    end:
        if (nb_rawdev < cfg.nb_ports * cfg.ports[0].nb_queues)
            rte_exit(EXIT_FAILURE,
                "Not enough IOAT rawdevs (%u) for all queues (%u).\n",
                nb_rawdev, cfg.nb_ports * cfg.ports[0].nb_queues);
        RTE_LOG(INFO, IOAT, "Number of used rawdevs: %u.\n", nb_rawdev);
    }


The initialization of hardware device is done by ``rte_rawdev_configure()``
function using ``rte_rawdev_info`` struct. After configuration the device is
started using ``rte_rawdev_start()`` function. Each of the above operations
is done in ``configure_rawdev_queue()``.

.. code-block:: c

    static void
    configure_rawdev_queue(uint32_t dev_id)
    {
        struct rte_ioat_rawdev_config dev_config = { .ring_size = ring_size };
        struct rte_rawdev_info info = { .dev_private = &dev_config };

        if (rte_rawdev_configure(dev_id, &info, sizeof(dev_config)) != 0) {
            rte_exit(EXIT_FAILURE,
                "Error with rte_rawdev_configure()\n");
        }
        if (rte_rawdev_start(dev_id) != 0) {
            rte_exit(EXIT_FAILURE,
                "Error with rte_rawdev_start()\n");
        }
    }

If initialization is successful, memory for hardware device
statistics is allocated.

Finally ``main()`` function starts all packet handling lcores and starts
printing stats in a loop on the main lcore. The application can be
interrupted and closed using ``Ctrl-C``. The main lcore waits for
all worker lcores to finish, deallocates resources and exits.

The processing lcores launching function are described below.

The Lcores Launching Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As described above, ``main()`` function invokes ``start_forwarding_cores()``
function in order to start processing for each lcore:

.. code-block:: c

    static void start_forwarding_cores(void)
    {
        uint32_t lcore_id = rte_lcore_id();

        RTE_LOG(INFO, IOAT, "Entering %s on lcore %u\n",
                __func__, rte_lcore_id());

        if (cfg.nb_lcores == 1) {
            lcore_id = rte_get_next_lcore(lcore_id, true, true);
            rte_eal_remote_launch((lcore_function_t *)rxtx_main_loop,
                NULL, lcore_id);
        } else if (cfg.nb_lcores > 1) {
            lcore_id = rte_get_next_lcore(lcore_id, true, true);
            rte_eal_remote_launch((lcore_function_t *)rx_main_loop,
                NULL, lcore_id);

            lcore_id = rte_get_next_lcore(lcore_id, true, true);
            rte_eal_remote_launch((lcore_function_t *)tx_main_loop, NULL,
                lcore_id);
        }
    }

The function launches Rx/Tx processing functions on configured lcores
using ``rte_eal_remote_launch()``. The configured ports, their number
and number of assigned lcores are stored in user-defined
``rxtx_transmission_config`` struct:

.. code-block:: c

    struct rxtx_transmission_config {
        struct rxtx_port_config ports[RTE_MAX_ETHPORTS];
        uint16_t nb_ports;
        uint16_t nb_lcores;
    };

The structure is initialized in 'main()' function with the values
corresponding to ports and lcores configuration provided by the user.

The Lcores Processing Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For receiving packets on each port, the ``ioat_rx_port()`` function is used.
The function receives packets on each configured Rx queue. Depending on the
mode the user chose, it will enqueue packets to IOAT rawdev channels and
then invoke copy process (hardware copy), or perform software copy of each
packet using ``pktmbuf_sw_copy()`` function and enqueue them to an rte_ring:

.. code-block:: c

    /* Receive packets on one port and enqueue to IOAT rawdev or rte_ring. */
    static void
    ioat_rx_port(struct rxtx_port_config *rx_config)
    {
        uint32_t nb_rx, nb_enq, i, j;
        struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
        for (i = 0; i < rx_config->nb_queues; i++) {

            nb_rx = rte_eth_rx_burst(rx_config->rxtx_port, i,
                pkts_burst, MAX_PKT_BURST);

            if (nb_rx == 0)
                continue;

            port_statistics.rx[rx_config->rxtx_port] += nb_rx;

            if (copy_mode == COPY_MODE_IOAT_NUM) {
                /* Perform packet hardware copy */
                nb_enq = ioat_enqueue_packets(pkts_burst,
                    nb_rx, rx_config->ioat_ids[i]);
                if (nb_enq > 0)
                    rte_ioat_perform_ops(rx_config->ioat_ids[i]);
            } else {
                /* Perform packet software copy, free source packets */
                int ret;
                struct rte_mbuf *pkts_burst_copy[MAX_PKT_BURST];

                ret = rte_mempool_get_bulk(ioat_pktmbuf_pool,
                    (void *)pkts_burst_copy, nb_rx);

                if (unlikely(ret < 0))
                    rte_exit(EXIT_FAILURE,
                        "Unable to allocate memory.\n");

                for (j = 0; j < nb_rx; j++)
                    pktmbuf_sw_copy(pkts_burst[j],
                        pkts_burst_copy[j]);

                rte_mempool_put_bulk(ioat_pktmbuf_pool,
                    (void *)pkts_burst, nb_rx);

                nb_enq = rte_ring_enqueue_burst(
                    rx_config->rx_to_tx_ring,
                    (void *)pkts_burst_copy, nb_rx, NULL);

                /* Free any not enqueued packets. */
                rte_mempool_put_bulk(ioat_pktmbuf_pool,
                    (void *)&pkts_burst_copy[nb_enq],
                    nb_rx - nb_enq);
            }

            port_statistics.copy_dropped[rx_config->rxtx_port] +=
                (nb_rx - nb_enq);
        }
    }

The packets are received in burst mode using ``rte_eth_rx_burst()``
function. When using hardware copy mode the packets are enqueued in
copying device's buffer using ``ioat_enqueue_packets()`` which calls
``rte_ioat_enqueue_copy()``. When all received packets are in the
buffer the copy operations are started by calling ``rte_ioat_perform_ops()``.
Function ``rte_ioat_enqueue_copy()`` operates on physical address of
the packet. Structure ``rte_mbuf`` contains only physical address to
start of the data buffer (``buf_iova``). Thus the address is adjusted
by ``addr_offset`` value in order to get the address of ``rearm_data``
member of ``rte_mbuf``. That way both the packet data and metadata can
be copied in a single operation. This method can be used because the mbufs
are direct mbufs allocated by the apps. If another app uses external buffers,
or indirect mbufs, then multiple copy operations must be used.

.. code-block:: c

    static uint32_t
    ioat_enqueue_packets(struct rte_mbuf **pkts,
        uint32_t nb_rx, uint16_t dev_id)
    {
        int ret;
        uint32_t i;
        struct rte_mbuf *pkts_copy[MAX_PKT_BURST];

        const uint64_t addr_offset = RTE_PTR_DIFF(pkts[0]->buf_addr,
            &pkts[0]->rearm_data);

        ret = rte_mempool_get_bulk(ioat_pktmbuf_pool,
                (void *)pkts_copy, nb_rx);

        if (unlikely(ret < 0))
            rte_exit(EXIT_FAILURE, "Unable to allocate memory.\n");

        for (i = 0; i < nb_rx; i++) {
            /* Perform data copy */
            ret = rte_ioat_enqueue_copy(dev_id,
                pkts[i]->buf_iova
                    - addr_offset,
                pkts_copy[i]->buf_iova
                    - addr_offset,
                rte_pktmbuf_data_len(pkts[i])
                    + addr_offset,
                (uintptr_t)pkts[i],
                (uintptr_t)pkts_copy[i],
                0 /* nofence */);

            if (ret != 1)
                break;
        }

        ret = i;
        /* Free any not enqueued packets. */
        rte_mempool_put_bulk(ioat_pktmbuf_pool, (void *)&pkts[i], nb_rx - i);
        rte_mempool_put_bulk(ioat_pktmbuf_pool, (void *)&pkts_copy[i],
            nb_rx - i);

        return ret;
    }


All completed copies are processed by ``ioat_tx_port()`` function. When using
hardware copy mode the function invokes ``rte_ioat_completed_ops()``
on each assigned IOAT channel to gather copied packets. If software copy
mode is used the function dequeues copied packets from the rte_ring. Then each
packet MAC address is changed if it was enabled. After that copies are sent
in burst mode using `` rte_eth_tx_burst()``.


.. code-block:: c

    /* Transmit packets from IOAT rawdev/rte_ring for one port. */
    static void
    ioat_tx_port(struct rxtx_port_config *tx_config)
    {
        uint32_t i, j, nb_dq = 0;
        struct rte_mbuf *mbufs_src[MAX_PKT_BURST];
        struct rte_mbuf *mbufs_dst[MAX_PKT_BURST];

        for (i = 0; i < tx_config->nb_queues; i++) {
            if (copy_mode == COPY_MODE_IOAT_NUM) {
                /* Deque the mbufs from IOAT device. */
                nb_dq = rte_ioat_completed_ops(
                    tx_config->ioat_ids[i], MAX_PKT_BURST,
                    (void *)mbufs_src, (void *)mbufs_dst);
            } else {
                /* Deque the mbufs from rx_to_tx_ring. */
                nb_dq = rte_ring_dequeue_burst(
                    tx_config->rx_to_tx_ring, (void *)mbufs_dst,
                    MAX_PKT_BURST, NULL);
            }

            if (nb_dq == 0)
                return;

            if (copy_mode == COPY_MODE_IOAT_NUM)
                rte_mempool_put_bulk(ioat_pktmbuf_pool,
                    (void *)mbufs_src, nb_dq);

            /* Update macs if enabled */
            if (mac_updating) {
                for (j = 0; j < nb_dq; j++)
                    update_mac_addrs(mbufs_dst[j],
                        tx_config->rxtx_port);
            }

            const uint16_t nb_tx = rte_eth_tx_burst(
                tx_config->rxtx_port, 0,
                (void *)mbufs_dst, nb_dq);

            port_statistics.tx[tx_config->rxtx_port] += nb_tx;

            /* Free any unsent packets. */
            if (unlikely(nb_tx < nb_dq))
                rte_mempool_put_bulk(ioat_pktmbuf_pool,
                (void *)&mbufs_dst[nb_tx],
                    nb_dq - nb_tx);
        }
    }

The Packet Copying Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to perform packet copy there is a user-defined function
``pktmbuf_sw_copy()`` used. It copies a whole packet by copying
metadata from source packet to new mbuf, and then copying a data
chunk of source packet. Both memory copies are done using
``rte_memcpy()``:

.. code-block:: c

    static inline void
    pktmbuf_sw_copy(struct rte_mbuf *src, struct rte_mbuf *dst)
    {
        /* Copy packet metadata */
        rte_memcpy(&dst->rearm_data,
            &src->rearm_data,
            offsetof(struct rte_mbuf, cacheline1)
                - offsetof(struct rte_mbuf, rearm_data));

        /* Copy packet data */
        rte_memcpy(rte_pktmbuf_mtod(dst, char *),
            rte_pktmbuf_mtod(src, char *), src->data_len);
    }

The metadata in this example is copied from ``rearm_data`` member of
``rte_mbuf`` struct up to ``cacheline1``.

In order to understand why software packet copying is done as shown
above please refer to the "Mbuf Library" section of the
*DPDK Programmer's Guide*.
