..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

L2 Forwarding Sample Application (in Real and Virtualized Environments) with core load statistics.
==================================================================================================

The L2 Forwarding sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK) which
also takes advantage of Single Root I/O Virtualization (SR-IOV) features in a virtualized environment.

.. note::

    This application is a variation of L2 Forwarding sample application. It demonstrate possible
    scheme of job stats library usage therefore some parts of this document is identical with original
    L2 forwarding application.

Overview
--------

The L2 Forwarding sample application, which can operate in real and virtualized environments,
performs L2 forwarding for each packet that is received.
The destination port is the adjacent port from the enabled portmask, that is,
if the first four ports are enabled (portmask 0xf),
ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other.
Also, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX port MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

This application can be used to benchmark performance using a traffic-generator, as shown in the :numref:`figure_l2_fwd_benchmark_setup_jobstats`.

The application can also be used in a virtualized environment as shown in :numref:`figure_l2_fwd_virtenv_benchmark_setup_jobstats`.

The L2 Forwarding application can also be used as a starting point for developing a new application based on the DPDK.

.. _figure_l2_fwd_benchmark_setup_jobstats:

.. figure:: img/l2_fwd_benchmark_setup.*

   Performance Benchmark Setup (Basic Environment)

.. _figure_l2_fwd_virtenv_benchmark_setup_jobstats:

.. figure:: img/l2_fwd_virtenv_benchmark_setup.*

   Performance Benchmark Setup (Virtualized Environment)


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

The application is located in the ``l2fwd-jobstats`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./build/l2fwd-jobstats [EAL options] -- -p PORTMASK [-q NQ] [-l]

where,

*   p PORTMASK: A hexadecimal bitmask of the ports to configure

*   q NQ: A number of queues (=ports) per lcore (default is 1)

*   l: Use locale thousands separator when formatting big numbers.

To run the application in linux environment with 4 lcores, 16 ports, 8 RX queues per lcore and
thousands  separator printing, issue the command:

.. code-block:: console

    $ ./build/l2fwd-jobstats -l 0-3 -n 4 -- -q 8 -p ffff -l

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The L2 Forwarding sample application takes specific parameters,
in addition to Environment Abstraction Layer (EAL) arguments
(see `Running the Application`_).
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
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

The rte_mempool is a generic structure used to handle pools of objects.
In this case, it is necessary to create a pool that will be used by the driver.
The number of allocated pkt mbufs is NB_MBUF, with a data room size of
RTE_MBUF_DEFAULT_BUF_SIZE each.
A per-lcore cache of MEMPOOL_CACHE_SIZE mbufs is kept.
The memory is allocated in rte_socket_id() socket,
but it is possible to extend this code to allocate one mbuf pool per socket.

The rte_pktmbuf_pool_create() function uses the default mbuf pool and mbuf
initializers, respectively rte_pktmbuf_pool_init() and rte_pktmbuf_init().
An advanced application may want to use the mempool API to create the
mbuf pool with more control.

Driver Initialization
~~~~~~~~~~~~~~~~~~~~~

The main part of the code in the main() function relates to the initialization of the driver.
To fully understand this code, it is recommended to study the chapters that related to the Poll Mode Driver
in the *DPDK Programmer's Guide* and the *DPDK API Reference*.

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

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q option,
which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four ports with one lcore.
If there are 16 ports on the target (and if the portmask argument is -p ffff ),
the application will need four lcores to poll all the ports.

.. code-block:: c

    ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
                rte_eth_dev_socket_id(portid),
                NULL,
                l2fwd_pktmbuf_pool);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                ret, (unsigned) portid);

The list of queues that must be polled for a given lcore is stored in a private structure called struct lcore_queue_conf.

.. code-block:: c

    struct lcore_queue_conf {
        unsigned n_rx_port;
        unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
        truct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

        struct rte_timer rx_timers[MAX_RX_QUEUE_PER_LCORE];
        struct rte_jobstats port_fwd_jobs[MAX_RX_QUEUE_PER_LCORE];

        struct rte_timer flush_timer;
        struct rte_jobstats flush_job;
        struct rte_jobstats idle_job;
        struct rte_jobstats_context jobs_context;

        rte_atomic16_t stats_read_pending;
        rte_spinlock_t lock;
    } __rte_cache_aligned;

Values of struct lcore_queue_conf:

*   n_rx_port and rx_port_list[] are used in the main packet processing loop
    (see Section `Receive, Process and Transmit Packets`_ later in this chapter).

*   rx_timers and flush_timer are used to ensure forced TX on low packet rate.

*   flush_job, idle_job and jobs_context are librte_jobstats objects used for managing l2fwd jobs.

*   stats_read_pending and lock are used during job stats read phase.

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port. For every port, a single TX queue is initialized.

.. code-block:: c

    /* init one TX queue on each port */

    fflush(stdout);
    ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
            rte_eth_dev_socket_id(portid),
            NULL);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                ret, (unsigned) portid);

Jobs statistics initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
There are several statistics objects available:

*   Flush job statistics

.. code-block:: c

    rte_jobstats_init(&qconf->flush_job, "flush", drain_tsc, drain_tsc,
            drain_tsc, 0);

    rte_timer_init(&qconf->flush_timer);
    ret = rte_timer_reset(&qconf->flush_timer, drain_tsc, PERIODICAL,
                lcore_id, &l2fwd_flush_job, NULL);

    if (ret < 0) {
        rte_exit(1, "Failed to reset flush job timer for lcore %u: %s",
                    lcore_id, rte_strerror(-ret));
    }

*   Statistics per RX port

.. code-block:: c

    rte_jobstats_init(job, name, 0, drain_tsc, 0, MAX_PKT_BURST);
    rte_jobstats_set_update_period_function(job, l2fwd_job_update_cb);

    rte_timer_init(&qconf->rx_timers[i]);
    ret = rte_timer_reset(&qconf->rx_timers[i], 0, PERIODICAL, lcore_id,
            l2fwd_fwd_job, (void *)(uintptr_t)i);

    if (ret < 0) {
        rte_exit(1, "Failed to reset lcore %u port %u job timer: %s",
                    lcore_id, qconf->rx_port_list[i], rte_strerror(-ret));
    }

Following parameters are passed to rte_jobstats_init():

*   0 as minimal poll period

*   drain_tsc as maximum poll period

*   MAX_PKT_BURST as desired target value (RX burst size)

Main loop
~~~~~~~~~

The forwarding path is reworked comparing to original L2 Forwarding application.
In the l2fwd_main_loop() function three loops are placed.

.. code-block:: c

    for (;;) {
        rte_spinlock_lock(&qconf->lock);

        do {
            rte_jobstats_context_start(&qconf->jobs_context);

            /* Do the Idle job:
             * - Read stats_read_pending flag
             * - check if some real job need to be executed
             */
            rte_jobstats_start(&qconf->jobs_context, &qconf->idle_job);

            do {
                uint8_t i;
                uint64_t now = rte_get_timer_cycles();

                need_manage = qconf->flush_timer.expire < now;
                /* Check if we was esked to give a stats. */
                stats_read_pending =
                        rte_atomic16_read(&qconf->stats_read_pending);
                need_manage |= stats_read_pending;

                for (i = 0; i < qconf->n_rx_port && !need_manage; i++)
                    need_manage = qconf->rx_timers[i].expire < now;

            } while (!need_manage);
            rte_jobstats_finish(&qconf->idle_job, qconf->idle_job.target);

            rte_timer_manage();
            rte_jobstats_context_finish(&qconf->jobs_context);
        } while (likely(stats_read_pending == 0));

        rte_spinlock_unlock(&qconf->lock);
        rte_pause();
    }

First infinite for loop is to minimize impact of stats reading. Lock is only locked/unlocked when asked.

Second inner while loop do the whole jobs management. When any job is ready, the use rte_timer_manage() is used to call the job handler.
In this place functions l2fwd_fwd_job() and l2fwd_flush_job() are called when needed.
Then rte_jobstats_context_finish() is called to mark loop end - no other jobs are ready to execute. By this time stats are ready to be read
and if stats_read_pending is set, loop breaks allowing stats to be read.

Third do-while loop is the idle job (idle stats counter). Its only purpose is monitoring if any job is ready or stats job read is pending
for this lcore. Statistics from this part of code is considered as the headroom available for additional processing.

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main task of l2fwd_fwd_job() function is to read ingress packets from the RX queue of particular port and forward it.
This is done using the following code:

.. code-block:: c

    total_nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst,
            MAX_PKT_BURST);

    for (j = 0; j < total_nb_rx; j++) {
        m = pkts_burst[j];
        rte_prefetch0(rte_pktmbuf_mtod(m, void *));
        l2fwd_simple_forward(m, portid);
    }

Packets are read in a burst of size MAX_PKT_BURST.
Then, each mbuf in the table is processed by the l2fwd_simple_forward() function.
The processing is very simple: process the TX port from the RX port, then replace the source and destination MAC addresses.

The rte_eth_rx_burst() function writes the mbuf pointers in a local table and returns the number of available mbufs in the table.

After first read second try is issued.

.. code-block:: c

    if (total_nb_rx == MAX_PKT_BURST) {
        const uint16_t nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst,
                MAX_PKT_BURST);

        total_nb_rx += nb_rx;
        for (j = 0; j < nb_rx; j++) {
            m = pkts_burst[j];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            l2fwd_simple_forward(m, portid);
        }
    }

This second read is important to give job stats library a feedback how many packets was processed.

.. code-block:: c

    /* Adjust period time in which we are running here. */
    if (rte_jobstats_finish(job, total_nb_rx) != 0) {
        rte_timer_reset(&qconf->rx_timers[port_idx], job->period, PERIODICAL,
                lcore_id, l2fwd_fwd_job, arg);
    }

To maximize performance exactly MAX_PKT_BURST is expected (the target value) to be read for each l2fwd_fwd_job() call.
If total_nb_rx is smaller than target value job->period will be increased. If it is greater the period will be decreased.

.. note::

    In the following code, one line for getting the output port requires some explanation.

During the initialization process, a static array of destination ports (l2fwd_dst_ports[]) is filled such that for each source port,
a destination port is assigned that is either the next or previous enabled port from the portmask.
Naturally, the number of ports in the portmask must be even, otherwise, the application exits.

.. code-block:: c

    static void
    l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
    {
        struct ether_hdr *eth;
        void *tmp;
        unsigned dst_port;

        dst_port = l2fwd_dst_ports[portid];

        eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

        /* 02:00:00:00:00:xx */

        tmp = &eth->d_addr.addr_bytes[0];

        *((uint64_t *)tmp) = 0x000000000002 + ((uint64_t) dst_port << 40);

        /* src addr */

        ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

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

To ensure that no packets remain in the tables, the flush job exists. The l2fwd_flush_job()
is called periodically to for each lcore draining TX queue of each port.
This technique introduces some latency when there are not many packets to send,
however it improves performance:

.. code-block:: c

    static void
    l2fwd_flush_job(__rte_unused struct rte_timer *timer, __rte_unused void *arg)
    {
        uint64_t now;
        unsigned lcore_id;
        struct lcore_queue_conf *qconf;
        struct mbuf_table *m_table;
        uint16_t portid;

        lcore_id = rte_lcore_id();
        qconf = &lcore_queue_conf[lcore_id];

        rte_jobstats_start(&qconf->jobs_context, &qconf->flush_job);

        now = rte_get_timer_cycles();
        lcore_id = rte_lcore_id();
        qconf = &lcore_queue_conf[lcore_id];
        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
            m_table = &qconf->tx_mbufs[portid];
            if (m_table->len == 0 || m_table->next_flush_time <= now)
                continue;

            l2fwd_send_burst(qconf, portid);
        }


        /* Pass target to indicate that this job is happy of time interval
         * in which it was called. */
        rte_jobstats_finish(&qconf->flush_job, qconf->flush_job.target);
    }
