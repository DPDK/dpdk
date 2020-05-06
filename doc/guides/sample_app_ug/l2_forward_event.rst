..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _l2_fwd_event_app:

L2 Forwarding Eventdev Sample Application
=========================================

The L2 Forwarding eventdev sample application is a simple example of packet
processing using the Data Plane Development Kit (DPDK) to demonstrate usage of
poll and event mode packet I/O mechanism.

Overview
--------

The L2 Forwarding eventdev sample application, performs L2 forwarding for each
packet that is received on an RX_PORT. The destination port is the adjacent port
from the enabled portmask, that is, if the first four ports are enabled (portmask=0x0f),
ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other.
Also, if MAC addresses updating is enabled, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX_PORT MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

Application receives packets from RX_PORT using below mentioned methods:

*   Poll mode

*   Eventdev mode (default)

This application can be used to benchmark performance using a traffic-generator,
as shown in the :numref:`figure_l2fwd_event_benchmark_setup`.

.. _figure_l2fwd_event_benchmark_setup:

.. figure:: img/l2_fwd_benchmark_setup.*

   Performance Benchmark Setup (Basic Environment)

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l2fwd-event`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./build/l2fwd-event [EAL options] -- -p PORTMASK [-q NQ] --[no-]mac-updating --mode=MODE --eventq-sched=SCHED_MODE

where,

*   p PORTMASK: A hexadecimal bitmask of the ports to configure

*   q NQ: A number of queues (=ports) per lcore (default is 1)

*   --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default).

*   --mode=MODE: Packet transfer mode for I/O, poll or eventdev. Eventdev by default.

*   --eventq-sched=SCHED_MODE: Event queue schedule mode, Ordered, Atomic or Parallel. Atomic by default.

*   --config: Configure forwarding port pair mapping. Alternate port pairs by default.

Sample usage commands are given below to run the application into different mode:

Poll mode with 4 lcores, 16 ports and 8 RX queues per lcore and MAC address updating enabled,
issue the command:

.. code-block:: console

    ./build/l2fwd-event -l 0-3 -n 4 -- -q 8 -p ffff --mode=poll

Eventdev mode with 4 lcores, 16 ports , sched method ordered and MAC address updating enabled,
issue the command:

.. code-block:: console

    ./build/l2fwd-event -l 0-3 -n 4 -- -p ffff --eventq-sched=ordered

or

.. code-block:: console

    ./build/l2fwd-event -l 0-3 -n 4 -- -q 8 -p ffff --mode=eventdev --eventq-sched=ordered

Refer to the *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

To run application with S/W scheduler, it uses following DPDK services:

*   Software scheduler
*   Rx adapter service function
*   Tx adapter service function

Application needs service cores to run above mentioned services. Service cores
must be provided as EAL parameters along with the --vdev=event_sw0 to enable S/W
scheduler. Following is the sample command:

.. code-block:: console

    ./build/l2fwd-event -l 0-7 -s 0-3 -n 4 --vdev event_sw0 -- -q 8 -p ffff --mode=eventdev --eventq-sched=ordered

Explanation
-----------

The following sections provide some explanation of the code.

.. _l2_fwd_event_app_cmd_arguments:

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The L2 Forwarding eventdev sample application takes specific parameters,
in addition to Environment Abstraction Layer (EAL) arguments.
The preferred way to parse parameters is to use the getopt() function,
since it is part of a well-defined and portable library.

The parsing of arguments is done in the **l2fwd_parse_args()** function for non
eventdev parameters and in **parse_eventdev_args()** for eventdev parameters.
The method of argument parsing is not described here. Refer to the
*glibc getopt(3)* man page for details.

EAL arguments are parsed first, then application-specific arguments.
This is done at the beginning of the main() function and eventdev parameters
are parsed in eventdev_resource_setup() function during eventdev setup:

.. code-block:: c

    /* init EAL */

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Invalid EAL arguments\n");

    argc -= ret;
    argv += ret;

    /* parse application arguments (after the EAL ones) */

    ret = l2fwd_parse_args(argc, argv);
    if (ret < 0)
        rte_panic("Invalid L2FWD arguments\n");
    .
    .
    .

    /* Parse eventdev command line options */
    ret = parse_eventdev_args(argc, argv);
    if (ret < 0)
        return ret;




.. _l2_fwd_event_app_mbuf_init:

Mbuf Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

Once the arguments are parsed, the mbuf pool is created.
The mbuf pool contains a set of mbuf objects that will be used by the driver
and the application to store network packet data:

.. code-block:: c

    /* create the mbuf pool */

    l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
                                                 MEMPOOL_CACHE_SIZE, 0,
                                                 RTE_MBUF_DEFAULT_BUF_SIZE,
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

.. _l2_fwd_event_app_drv_init:

Driver Initialization
~~~~~~~~~~~~~~~~~~~~~

The main part of the code in the main() function relates to the initialization
of the driver. To fully understand this code, it is recommended to study the
chapters that related to the Poll Mode and Event mode Driver in the
*DPDK Programmer's Guide* - Rel 1.4 EAR and the *DPDK API Reference*.

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

The next step is to configure the RX and TX queues. For each port, there is only
one RX queue (only one lcore is able to poll a given port). The number of TX
queues depends on the number of available lcores. The rte_eth_dev_configure()
function is used to configure the number of queues for a port:

.. code-block:: c

    ret = rte_eth_dev_configure((uint8_t)portid, 1, 1, &port_conf);
    if (ret < 0)
        rte_panic("Cannot configure device: err=%d, port=%u\n",
                  ret, portid);

.. _l2_fwd_event_app_rx_init:

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q
option, which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four
ports with one lcore. If there are 16 ports on the target (and if the portmask
argument is -p ffff ), the application will need four lcores to poll all the
ports.

.. code-block:: c

    ret = rte_eth_rx_queue_setup((uint8_t) portid, 0, nb_rxd, SOCKET0,
                                 &rx_conf, l2fwd_pktmbuf_pool);
    if (ret < 0)

        rte_panic("rte_eth_rx_queue_setup: err=%d, port=%u\n",
                  ret, portid);

The list of queues that must be polled for a given lcore is stored in a private
structure called struct lcore_queue_conf.

.. code-block:: c

    struct lcore_queue_conf {
        unsigned n_rx_port;
        unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
        struct mbuf_table tx_mbufs[L2FWD_MAX_PORTS];
    } rte_cache_aligned;

    struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

The values n_rx_port and rx_port_list[] are used in the main packet processing
loop (see :ref:`l2_fwd_event_app_rx_tx_packets`).

.. _l2_fwd_event_app_tx_init:

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port. For every port, a single TX
queue is initialized.

.. code-block:: c

    /* init one TX queue on each port */

    fflush(stdout);

    ret = rte_eth_tx_queue_setup((uint8_t) portid, 0, nb_txd,
                                 rte_eth_dev_socket_id(portid), &tx_conf);
    if (ret < 0)
        rte_panic("rte_eth_tx_queue_setup:err=%d, port=%u\n",
                  ret, (unsigned) portid);

To configure eventdev support, application setups following components:

*   Event dev
*   Event queue
*   Event Port
*   Rx/Tx adapters
*   Ethernet ports

.. _l2_fwd_event_app_event_dev_init:

Event device Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Application can use either H/W or S/W based event device scheduler
implementation and supports single instance of event device. It configures event
device as per below configuration

.. code-block:: c

   struct rte_event_dev_config event_d_conf = {
        .nb_event_queues = ethdev_count, /* Dedicated to each Ethernet port */
        .nb_event_ports = num_workers, /* Dedicated to each lcore */
        .nb_events_limit  = 4096,
        .nb_event_queue_flows = 1024,
        .nb_event_port_dequeue_depth = 128,
        .nb_event_port_enqueue_depth = 128
   };

   ret = rte_event_dev_configure(event_d_id, &event_d_conf);
   if (ret < 0)
        rte_panic("Error in configuring event device\n");

In case of S/W scheduler, application runs eventdev scheduler service on service
core. Application retrieves service id and finds the best possible service core to
run S/W scheduler.

.. code-block:: c

        rte_event_dev_info_get(evt_rsrc->event_d_id, &evdev_info);
        if (evdev_info.event_dev_cap  & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED) {
                ret = rte_event_dev_service_id_get(evt_rsrc->event_d_id,
                                &service_id);
                if (ret != -ESRCH && ret != 0)
                        rte_panic("Error in starting eventdev service\n");
                l2fwd_event_service_enable(service_id);
        }

.. _l2_fwd_app_event_queue_init:

Event queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~
Each Ethernet device is assigned a dedicated event queue which will be linked
to all available event ports i.e. each lcore can dequeue packets from any of the
Ethernet ports.

.. code-block:: c

   struct rte_event_queue_conf event_q_conf = {
        .nb_atomic_flows = 1024,
        .nb_atomic_order_sequences = 1024,
        .event_queue_cfg = 0,
        .schedule_type = RTE_SCHED_TYPE_ATOMIC,
        .priority = RTE_EVENT_DEV_PRIORITY_HIGHEST
   };

   /* User requested sched mode */
   event_q_conf.schedule_type = eventq_sched_mode;
   for (event_q_id = 0; event_q_id < ethdev_count; event_q_id++) {
        ret = rte_event_queue_setup(event_d_id, event_q_id,
                                            &event_q_conf);
        if (ret < 0)
              rte_panic("Error in configuring event queue\n");
   }

In case of S/W scheduler, an extra event queue is created which will be used for
Tx adapter service function for enqueue operation.

.. _l2_fwd_app_event_port_init:

Event port Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~
Each worker thread is assigned a dedicated event port for enq/deq operations
to/from an event device. All event ports are linked with all available event
queues.

.. code-block:: c

   struct rte_event_port_conf event_p_conf = {
        .dequeue_depth = 32,
        .enqueue_depth = 32,
        .new_event_threshold = 4096
   };

   for (event_p_id = 0; event_p_id < num_workers; event_p_id++) {
        ret = rte_event_port_setup(event_d_id, event_p_id,
                                   &event_p_conf);
        if (ret < 0)
              rte_panic("Error in configuring event port %d\n", event_p_id);

        ret = rte_event_port_link(event_d_id, event_p_id, NULL,
                                  NULL, 0);
        if (ret < 0)
              rte_panic("Error in linking event port %d to queue\n",
                        event_p_id);
   }

In case of S/W scheduler, an extra event port is created by DPDK library which
is retrieved  by the application and same will be used by Tx adapter service.

.. code-block:: c

        ret = rte_event_eth_tx_adapter_event_port_get(tx_adptr_id, &tx_port_id);
        if (ret)
                rte_panic("Failed to get Tx adapter port id: %d\n", ret);

        ret = rte_event_port_link(event_d_id, tx_port_id,
                                  &evt_rsrc.evq.event_q_id[
                                        evt_rsrc.evq.nb_queues - 1],
                                  NULL, 1);
        if (ret != 1)
                rte_panic("Unable to link Tx adapter port to Tx queue:err=%d\n",
                          ret);

.. _l2_fwd_event_app_adapter_init:

Rx/Tx adapter Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Each Ethernet port is assigned a dedicated Rx/Tx adapter for H/W scheduler. Each
Ethernet port's Rx queues are connected to its respective event queue at
priority 0 via Rx adapter configuration and Ethernet port's tx queues are
connected via Tx adapter.

.. code-block:: c

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;
		ret = rte_event_eth_rx_adapter_create(adapter_id, event_d_id,
						&evt_rsrc->def_p_conf);
		if (ret)
			rte_panic("Failed to create rx adapter[%d]\n",
                                  adapter_id);

		/* Configure user requested sched type*/
		eth_q_conf.ev.sched_type = rsrc->sched_type;
		eth_q_conf.ev.queue_id = evt_rsrc->evq.event_q_id[q_id];
		ret = rte_event_eth_rx_adapter_queue_add(adapter_id, port_id,
							 -1, &eth_q_conf);
		if (ret)
			rte_panic("Failed to add queues to Rx adapter\n");

		ret = rte_event_eth_rx_adapter_start(adapter_id);
		if (ret)
			rte_panic("Rx adapter[%d] start Failed\n", adapter_id);

		evt_rsrc->rx_adptr.rx_adptr[adapter_id] = adapter_id;
		adapter_id++;
		if (q_id < evt_rsrc->evq.nb_queues)
			q_id++;
	}

	adapter_id = 0;
	RTE_ETH_FOREACH_DEV(port_id) {
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;
		ret = rte_event_eth_tx_adapter_create(adapter_id, event_d_id,
						&evt_rsrc->def_p_conf);
		if (ret)
			rte_panic("Failed to create tx adapter[%d]\n",
                                  adapter_id);

		ret = rte_event_eth_tx_adapter_queue_add(adapter_id, port_id,
							 -1);
		if (ret)
			rte_panic("Failed to add queues to Tx adapter\n");

		ret = rte_event_eth_tx_adapter_start(adapter_id);
		if (ret)
			rte_panic("Tx adapter[%d] start Failed\n", adapter_id);

		evt_rsrc->tx_adptr.tx_adptr[adapter_id] = adapter_id;
		adapter_id++;
	}

For S/W scheduler instead of dedicated adapters, common Rx/Tx adapters are
configured which will be shared among all the Ethernet ports. Also DPDK library
need service cores to run internal services for Rx/Tx adapters. Application gets
service id for Rx/Tx adapters and after successful setup it runs the services
on dedicated service cores.

.. code-block:: c

	for (i = 0; i < evt_rsrc->rx_adptr.nb_rx_adptr; i++) {
		ret = rte_event_eth_rx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->rx_adptr.rx_adptr[i], &caps);
		if (ret < 0)
			rte_panic("Failed to get Rx adapter[%d] caps\n",
                                  evt_rsrc->rx_adptr.rx_adptr[i]);
		ret = rte_event_eth_rx_adapter_service_id_get(
                                                evt_rsrc->event_d_id,
                                                &service_id);
		if (ret != -ESRCH && ret != 0)
			rte_panic("Error in starting Rx adapter[%d] service\n",
                                  evt_rsrc->rx_adptr.rx_adptr[i]);
		l2fwd_event_service_enable(service_id);
	}

	for (i = 0; i < evt_rsrc->tx_adptr.nb_tx_adptr; i++) {
		ret = rte_event_eth_tx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->tx_adptr.tx_adptr[i], &caps);
		if (ret < 0)
			rte_panic("Failed to get Rx adapter[%d] caps\n",
                                  evt_rsrc->tx_adptr.tx_adptr[i]);
		ret = rte_event_eth_tx_adapter_service_id_get(
				evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_panic("Error in starting Rx adapter[%d] service\n",
                                  evt_rsrc->tx_adptr.tx_adptr[i]);
		l2fwd_event_service_enable(service_id);
	}

.. _l2_fwd_event_app_rx_tx_packets:

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the **l2fwd_main_loop()** function, the main task is to read ingress packets from
the RX queues. This is done using the following code:

.. code-block:: c

    /*
     * Read packet from RX queues
     */

    for (i = 0; i < qconf->n_rx_port; i++) {
        portid = qconf->rx_port_list[i];
        nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,  pkts_burst,
                                 MAX_PKT_BURST);

        for (j = 0; j < nb_rx; j++) {
            m = pkts_burst[j];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            l2fwd_simple_forward(m, portid);
        }
    }

Packets are read in a burst of size MAX_PKT_BURST. The rte_eth_rx_burst()
function writes the mbuf pointers in a local table and returns the number of
available mbufs in the table.

Then, each mbuf in the table is processed by the l2fwd_simple_forward()
function. The processing is very simple: process the TX port from the RX port,
then replace the source and destination MAC addresses if MAC addresses updating
is enabled.

During the initialization process, a static array of destination ports
(l2fwd_dst_ports[]) is filled such that for each source port, a destination port
is assigned that is either the next or previous enabled port from the portmask.
If number of ports are odd in portmask then packet from last port will be
forwarded to first port i.e. if portmask=0x07, then forwarding will take place
like p0--->p1, p1--->p2, p2--->p0.

Also to optimize enqueue operation, l2fwd_simple_forward() stores incoming mbufs
up to MAX_PKT_BURST. Once it reaches up to limit, all packets are transmitted to
destination ports.

.. code-block:: c

   static void
   l2fwd_simple_forward(struct rte_mbuf *m, uint32_t portid)
   {
       uint32_t dst_port;
       int32_t sent;
       struct rte_eth_dev_tx_buffer *buffer;

       dst_port = l2fwd_dst_ports[portid];

       if (mac_updating)
           l2fwd_mac_updating(m, dst_port);

       buffer = tx_buffer[dst_port];
       sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
       if (sent)
       port_statistics[dst_port].tx += sent;
   }

For this test application, the processing is exactly the same for all packets
arriving on the same RX port. Therefore, it would have been possible to call
the rte_eth_tx_buffer() function directly from the main loop to send all the
received packets on the same TX port, using the burst-oriented send function,
which is more efficient.

However, in real-life applications (such as, L3 routing),
packet N is not necessarily forwarded on the same port as packet N-1.
The application is implemented to illustrate that, so the same approach can be
reused in a more complex application.

To ensure that no packets remain in the tables, each lcore does a draining of TX
queue in its main loop. This technique introduces some latency when there are
not many packets to send, however it improves performance:

.. code-block:: c

        cur_tsc = rte_rdtsc();

        /*
        * TX burst queue drain
        */
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc)) {
                for (i = 0; i < qconf->n_rx_port; i++) {
                        portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
                        buffer = tx_buffer[portid];
                        sent = rte_eth_tx_buffer_flush(portid, 0,
                                                       buffer);
                        if (sent)
                                port_statistics[portid].tx += sent;
                }

                /* if timer is enabled */
                if (timer_period > 0) {
                        /* advance the timer */
                        timer_tsc += diff_tsc;

                        /* if timer has reached its timeout */
                        if (unlikely(timer_tsc >= timer_period)) {
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

In the **l2fwd_event_loop()** function, the main task is to read ingress
packets from the event ports. This is done using the following code:

.. code-block:: c

        /* Read packet from eventdev */
        nb_rx = rte_event_dequeue_burst(event_d_id, event_p_id,
                                        events, deq_len, 0);
        if (nb_rx == 0) {
                rte_pause();
                continue;
        }

        for (i = 0; i < nb_rx; i++) {
                mbuf[i] = events[i].mbuf;
                rte_prefetch0(rte_pktmbuf_mtod(mbuf[i], void *));
        }


Before reading packets, deq_len is fetched to ensure correct allowed deq length
by the eventdev.
The rte_event_dequeue_burst() function writes the mbuf pointers in a local table
and returns the number of available mbufs in the table.

Then, each mbuf in the table is processed by the l2fwd_eventdev_forward()
function. The processing is very simple: process the TX port from the RX port,
then replace the source and destination MAC addresses if MAC addresses updating
is enabled.

During the initialization process, a static array of destination ports
(l2fwd_dst_ports[]) is filled such that for each source port, a destination port
is assigned that is either the next or previous enabled port from the portmask.
If number of ports are odd in portmask then packet from last port will be
forwarded to first port i.e. if portmask=0x07, then forwarding will take place
like p0--->p1, p1--->p2, p2--->p0.

l2fwd_eventdev_forward() does not stores incoming mbufs. Packet will forwarded
be to destination ports via Tx adapter or generic event dev enqueue API
depending H/W or S/W scheduler is used.

.. code-block:: c

	nb_tx = rte_event_eth_tx_adapter_enqueue(event_d_id, port_id, ev,
						 nb_rx);
	while (nb_tx < nb_rx && !rsrc->force_quit)
		nb_tx += rte_event_eth_tx_adapter_enqueue(
				event_d_id, port_id,
				ev + nb_tx, nb_rx - nb_tx);
