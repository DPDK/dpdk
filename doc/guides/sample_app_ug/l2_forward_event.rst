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

    ./<build_dir>/examples/dpdk-l2fwd-event [EAL options] -- -p PORTMASK
                                                        [-q NQ]
                                                        [--[no-]mac-updating]
                                                        [--mode=MODE]
                                                        [--eventq-sched=SCHED_MODE]
                                                        [--event-vector [--event-vector-size SIZE] [--event-vector-tmo NS]]

where,

*   p PORTMASK: A hexadecimal bitmask of the ports to configure

*   q NQ: A number of queues (=ports) per lcore (default is 1)

*   --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default).

*   --mode=MODE: Packet transfer mode for I/O, poll or eventdev. Eventdev by default.

*   --eventq-sched=SCHED_MODE: Event queue schedule mode, Ordered, Atomic or Parallel. Atomic by default.

*   --config: Configure forwarding port pair mapping. Alternate port pairs by default.

*   --event-vector: Enable event vectorization. Only valid if --mode=eventdev.

*   --event-vector-size: Max vector size if event vectorization is enabled.

*   --event-vector-tmo: Max timeout to form vector in nanoseconds if event vectorization is enabled.

Sample usage commands are given below to run the application into different mode:

Poll mode with 4 lcores, 16 ports and 8 RX queues per lcore and MAC address updating enabled,
issue the command:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-event -l 0-3 -n 4 -- -q 8 -p ffff --mode=poll

Eventdev mode with 4 lcores, 16 ports , sched method ordered and MAC address updating enabled,
issue the command:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-event -l 0-3 -n 4 -- -p ffff --eventq-sched=ordered

or

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-event -l 0-3 -n 4 -- -q 8 -p ffff --mode=eventdev --eventq-sched=ordered

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

    ./<build_dir>/examples/dpdk-l2fwd-event -l 0-7 -s 0-3 -n 4 --vdev event_sw0 -- -q 8 -p ffff --mode=eventdev --eventq-sched=ordered

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

.. literalinclude:: ../../../examples/l2fwd-event/main.c
        :language: c
        :start-after: Init EAL. 8<
        :end-before: >8 End of init EAL.
        :dedent: 1

Mbuf Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

Once the arguments are parsed, the mbuf pool is created.
The mbuf pool contains a set of mbuf objects that will be used by the driver
and the application to store network packet data:

.. literalinclude:: ../../../examples/l2fwd-event/main.c
        :language: c
        :start-after: Create the mbuf pool. 8<
        :end-before: >8 End of creation of mbuf pool.
        :dedent: 1

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

.. literalinclude:: ../../../examples/l2fwd-event/main.c
        :language: c
        :start-after: Reset l2fwd_dst_ports. 8<
        :end-before: >8 End of reset l2fwd_dst_ports.
        :dedent: 1

The next step is to configure the RX and TX queues. For each port, there is only
one RX queue (only one lcore is able to poll a given port). The number of TX
queues depends on the number of available lcores. The rte_eth_dev_configure()
function is used to configure the number of queues for a port:

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_common.c
        :language: c
        :start-after: Configure RX and TX queue. 8<
        :end-before: >8 End of configuration RX and TX queue.
        :dedent: 2

RX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The application uses one lcore to poll one or several ports, depending on the -q
option, which specifies the number of queues per lcore.

For example, if the user specifies -q 4, the application is able to poll four
ports with one lcore. If there are 16 ports on the target (and if the portmask
argument is -p ffff ), the application will need four lcores to poll all the
ports.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_common.c
        :language: c
        :start-after: Using lcore to poll one or several ports. 8<
        :end-before: >8 End of using lcore to poll one or several ports.
        :dedent: 2

The list of queues that must be polled for a given lcore is stored in a private
structure called struct lcore_queue_conf.

.. literalinclude:: ../../../examples/l2fwd/main.c
        :language: c
        :start-after: List of queues to be polled for a given lcore. 8<
        :end-before: >8 End of list of queues to be polled for a given lcore.

The values n_rx_port and rx_port_list[] are used in the main packet processing
loop (see :ref:`l2_fwd_event_app_rx_tx_packets`).

.. _l2_fwd_event_app_tx_init:

TX Queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~

Each lcore should be able to transmit on any port. For every port, a single TX
queue is initialized.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_common.c
        :language: c
        :start-after: Init one TX queue on each port. 8<
        :end-before: >8 End of init one TX queue on each port.
        :dedent: 2

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

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event_generic.c
        :language: c
        :start-after: Configures event device as per below configuration. 8<
        :end-before: >8 End of configuration event device as per below configuration.
        :dedent: 1

In case of S/W scheduler, application runs eventdev scheduler service on service
core. Application retrieves service id and finds the best possible service core to
run S/W scheduler.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event.c
        :language: c
        :start-after: Running eventdev scheduler service on service core. 8<
        :end-before: >8 End of running eventdev scheduler service on service core.
        :dedent: 1

Event queue Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~
Each Ethernet device is assigned a dedicated event queue which will be linked
to all available event ports i.e. each lcore can dequeue packets from any of the
Ethernet ports.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event_generic.c
        :language: c
        :start-after: Event queue initialization. 8<
        :end-before: >8 End of event queue initialization.
        :dedent: 1

In case of S/W scheduler, an extra event queue is created which will be used for
Tx adapter service function for enqueue operation.

.. _l2_fwd_app_event_port_init:

Event port Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~
Each worker thread is assigned a dedicated event port for enq/deq operations
to/from an event device. All event ports are linked with all available event
queues.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event_generic.c
        :language: c
        :start-after: Event port initialization. 8<
        :end-before: >8 End of event port initialization.
        :dedent: 1

In case of S/W scheduler, an extra event port is created by DPDK library which
is retrieved  by the application and same will be used by Tx adapter service.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event_generic.c
        :language: c
        :start-after: Extra port created. 8<
        :end-before: >8 End of extra port created.
        :dedent: 1

Rx/Tx adapter Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Each Ethernet port is assigned a dedicated Rx/Tx adapter for H/W scheduler. Each
Ethernet port's Rx queues are connected to its respective event queue at
priority 0 via Rx adapter configuration and Ethernet port's tx queues are
connected via Tx adapter.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event_internal_port.c
        :language: c
        :start-after: Assigned ethernet port. 8<
        :end-before: >8 End of assigned ethernet port.
        :dedent: 1

For S/W scheduler instead of dedicated adapters, common Rx/Tx adapters are
configured which will be shared among all the Ethernet ports. Also DPDK library
need service cores to run internal services for Rx/Tx adapters. Application gets
service id for Rx/Tx adapters and after successful setup it runs the services
on dedicated service cores.

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event.c
        :language: c
        :start-after: Gets service ID for RX/TX adapters. 8<
        :end-before: >8 End of get service ID for RX/TX adapters.
        :dedent: 1

.. _l2_fwd_event_app_rx_tx_packets:

Receive, Process and Transmit Packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the **l2fwd_main_loop()** function, the main task is to read ingress packets from
the RX queues. This is done using the following code:

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_poll.c
        :language: c
        :start-after: Reading ingress packets. 8<
        :end-before: >8 End of reading ingress packets.
        :dedent: 2

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

.. literalinclude:: ../../../examples/l2fwd/main.c
        :language: c
        :start-after: Simple forward. 8<
        :end-before: >8 End of simple forward.

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

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_poll.c
        :language: c
        :start-after: Draining TX queue in main loop. 8<
        :end-before: >8 End of draining TX queue in main loop.
        :dedent: 2

In the **l2fwd_event_loop()** function, the main task is to read ingress
packets from the event ports. This is done using the following code:

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event.c
        :language: c
        :start-after: Read packet from eventdev. 8<
        :end-before: >8 End of reading packets from eventdev.
        :dedent: 2


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

.. literalinclude:: ../../../examples/l2fwd-event/l2fwd_event.c
        :language: c
        :start-after: Read packet from eventdev. 8<
        :end-before: >8 End of reading packets from eventdev.
        :dedent: 2
