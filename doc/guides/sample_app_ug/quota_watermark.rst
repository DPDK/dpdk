..  BSD LICENSE
    Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Quota and Watermark Sample Application
======================================

The Quota and Watermark sample application is a simple example of packet
processing using Data Plane Development Kit (DPDK) that showcases the use
of a quota as the maximum number of packets enqueue/dequeue at a time and
low and high thresholds, or watermarks, to signal low and high ring usage
respectively.

Additionally, it shows how the thresholds can be used to feedback congestion notifications to data producers by
temporarily stopping processing overloaded rings and sending Ethernet flow control frames.

This sample application is split in two parts:

*   qw - The core quota and watermark sample application

*   qwctl - A command line tool to alter quota and watermarks while qw is running

Overview
--------

The Quota and Watermark sample application performs forwarding for each packet that is received on a given port.
The destination port is the adjacent port from the enabled port mask, that is,
if the first four ports are enabled (port mask 0xf), ports 0 and 1 forward into each other,
and ports 2 and 3 forward into each other.
The MAC addresses of the forwarded Ethernet frames are not affected.

Internally, packets are pulled from the ports by the master logical core and put on a variable length processing pipeline,
each stage of which being connected by rings, as shown in :numref:`figure_pipeline_overview`.

.. _figure_pipeline_overview:

.. figure:: img/pipeline_overview.*

   Pipeline Overview


An adjustable quota value controls how many packets are being moved through the pipeline per enqueue and dequeue.
Adjustable threshold values associated with the rings control a back-off mechanism that
tries to prevent the pipeline from being overloaded by:

*   Stopping enqueuing on rings for which the usage has crossed the high watermark threshold

*   Sending Ethernet pause frames

*   Only resuming enqueuing on a ring once its usage goes below a global low watermark threshold

This mechanism allows congestion notifications to go up the ring pipeline and
eventually lead to an Ethernet flow control frame being send to the source.

On top of serving as an example of quota and watermark usage,
this application can be used to benchmark ring based processing pipelines performance using a traffic- generator,
as shown in :numref:`figure_ring_pipeline_perf_setup`.

.. _figure_ring_pipeline_perf_setup:

.. figure:: img/ring_pipeline_perf_setup.*

   Ring-based Processing Pipeline Performance Setup


Compiling the Application
-------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/quota_watermark

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

The core application, qw, has to be started first.

Once it is up and running, one can alter quota and watermarks while it runs using the control application, qwctl.

Running the Core Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application requires a single command line option:

.. code-block:: console

    ./qw/build/qw [EAL options] -- -p PORTMASK

where,

-p PORTMASK: A hexadecimal bitmask of the ports to configure

To run the application in a linuxapp environment with four logical cores and ports 0 and 2,
issue the following command:

.. code-block:: console

    ./qw/build/qw -l 0-3 -n 4 -- -p 5

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Running the Control Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The control application requires a number of command line options:

.. code-block:: console

    ./qwctl/build/qwctl [EAL options] --proc-type=secondary

The --proc-type=secondary option is necessary for the EAL to properly initialize the control application to
use the same huge pages as the core application and  thus be able to access its rings.

To run the application in a linuxapp environment on logical core 0, issue the following command:

.. code-block:: console

    ./qwctl/build/qwctl -l 0 -n 4 --proc-type=secondary

Refer to the *DPDK Getting Started* Guide for general information on running applications and
the Environment Abstraction Layer (EAL) options.

qwctl is an interactive command line that let the user change variables in a running instance of qw.
The help command gives a list of available commands:

.. code-block:: console

    $ qwctl > help

Code Overview
-------------

The following sections provide a quick guide to the application's source code.

Core Application - qw
~~~~~~~~~~~~~~~~~~~~~

EAL and Drivers Setup
^^^^^^^^^^^^^^^^^^^^^

The EAL arguments are parsed at the beginning of the main() function:

.. code-block:: c

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");

    argc -= ret;
    argv += ret;

Then, a call to init_dpdk(), defined in init.c, is made to initialize the poll mode drivers:

.. code-block:: c

    void
    init_dpdk(void)
    {
        int ret;

        /* Bind the drivers to usable devices */

        ret = rte_pci_probe();
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_pci_probe(): error %d\n", ret);

        if (rte_eth_dev_count() < 2)
            rte_exit(EXIT_FAILURE, "Not enough Ethernet port available\n");
    }

To fully understand this code, it is recommended to study the chapters that relate to the *Poll Mode Driver*
in the *DPDK Getting Started Guide* and the *DPDK API Reference*.

Shared Variables Setup
^^^^^^^^^^^^^^^^^^^^^^

The quota and high and low watermark shared variables are put into an rte_memzone using a call to setup_shared_variables():

.. code-block:: c

    void
    setup_shared_variables(void)
    {
           const struct rte_memzone *qw_memzone;

           qw_memzone = rte_memzone_reserve(QUOTA_WATERMARK_MEMZONE_NAME,
                          3 * sizeof(int), rte_socket_id(), 0);
           if (qw_memzone == NULL)
                   rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

           quota = qw_memzone->addr;
           low_watermark = (unsigned int *) qw_memzone->addr + 1;
           high_watermark = (unsigned int *) qw_memzone->addr + 2;
    }

These three variables are initialized to a default value in main() and
can be changed while qw is running using the qwctl control program.

Application Arguments
^^^^^^^^^^^^^^^^^^^^^

The qw application only takes one argument: a port mask that specifies which ports should be used by the application.
At least two ports are needed to run the application and there should be an even number of ports given in the port mask.

The port mask parsing is done in parse_qw_args(), defined in args.c.

Mbuf Pool Initialization
^^^^^^^^^^^^^^^^^^^^^^^^

Once the application's arguments are parsed, an mbuf pool is created.
It contains a set of mbuf objects that are used by the driver and the application to store network packets:

.. code-block:: c

    /* Create a pool of mbuf to store packets */
    mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", MBUF_PER_POOL, 32, 0,
					 MBUF_DATA_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_panic("%s\n", rte_strerror(rte_errno));

The rte_mempool is a generic structure used to handle pools of objects.
In this case, it is necessary to create a pool that will be used by the driver.

The number of allocated pkt mbufs is MBUF_PER_POOL, with a data room size
of MBUF_DATA_SIZE each.
A per-lcore cache of 32 mbufs is kept.
The memory is allocated in on the master lcore's socket, but it is possible to extend this code to allocate one mbuf pool per socket.

The rte_pktmbuf_pool_create() function uses the default mbuf pool and mbuf
initializers, respectively rte_pktmbuf_pool_init() and rte_pktmbuf_init().
An advanced application may want to use the mempool API to create the
mbuf pool with more control.

Ports Configuration and Pairing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each port in the port mask is configured and a corresponding ring is created in the master lcore's array of rings.
This ring is the first in the pipeline and will hold the packets directly coming from the port.

.. code-block:: c

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
        if (is_bit_set(port_id, portmask)) {
            configure_eth_port(port_id);
            init_ring(master_lcore_id, port_id);
        }

    pair_ports();

The configure_eth_port() and init_ring() functions are used to configure a port and a ring respectively and are defined in init.c.
They make use of the DPDK APIs defined in rte_eth.h and rte_ring.h.

pair_ports() builds the port_pairs[] array so that its key-value pairs are a mapping between reception and transmission ports.
It is defined in init.c.

Logical Cores Assignment
^^^^^^^^^^^^^^^^^^^^^^^^

The application uses the master logical core to poll all the ports for new packets and enqueue them on a ring associated with the port.

Each logical core except the last runs pipeline_stage() after a ring for each used port is initialized on that core.
pipeline_stage() on core X dequeues packets from core X-1's rings and enqueue them on its own rings. See :numref:`figure_threads_pipelines`.

.. code-block:: c

    /* Start pipeline_stage() on all the available slave lcore but the last */

    for (lcore_id = 0 ; lcore_id < last_lcore_id; lcore_id++) {
        if (rte_lcore_is_enabled(lcore_id) && lcore_id != master_lcore_id) {
            for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
                if (is_bit_set(port_id, portmask))
                    init_ring(lcore_id, port_id);

                rte_eal_remote_launch(pipeline_stage, NULL, lcore_id);
        }
    }

The last available logical core runs send_stage(),
which is the last stage of the pipeline dequeuing packets from the last ring in the pipeline and
sending them out on the destination port setup by pair_ports().

.. code-block:: c

    /* Start send_stage() on the last slave core */

    rte_eal_remote_launch(send_stage, NULL, last_lcore_id);

Receive, Process and Transmit Packets
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _figure_threads_pipelines:

.. figure:: img/threads_pipelines.*

   Threads and Pipelines


In the receive_stage() function running on the master logical core,
the main task is to read ingress packets from the RX ports and enqueue them
on the port's corresponding first ring in the pipeline.
This is done using the following code:

.. code-block:: c

    lcore_id = rte_lcore_id();

    /* Process each port round robin style */

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
            if (!is_bit_set(port_id, portmask))
                    continue;

            ring = rings[lcore_id][port_id];

            if (ring_state[port_id] != RING_READY) {
                    if (rte_ring_count(ring) > *low_watermark)
                            continue;
                    else
                            ring_state[port_id] = RING_READY;
            }

            /* Enqueue received packets on the RX ring */
            nb_rx_pkts = rte_eth_rx_burst(port_id, 0, pkts,
                            (uint16_t) *quota);
            ret = rte_ring_enqueue_bulk(ring, (void *) pkts,
                            nb_rx_pkts, &free);
            if (RING_SIZE - free > *high_watermark) {
                    ring_state[port_id] = RING_OVERLOADED;
                    send_pause_frame(port_id, 1337);
            }

            if (ret == 0) {

                    /*
                     * Return  mbufs to the pool,
                     * effectively dropping packets
                     */
                    for (i = 0; i < nb_rx_pkts; i++)
                            rte_pktmbuf_free(pkts[i]);
            }
    }

For each port in the port mask, the corresponding ring's pointer is fetched into ring and that ring's state is checked:

*   If it is in the RING_READY state, \*quota packets are grabbed from the port and put on the ring.
    Should this operation make the ring's usage cross its high watermark,
    the ring is marked as overloaded and an Ethernet flow control frame is sent to the source.

*   If it is not in the RING_READY state, this port is ignored until the ring's usage crosses the \*low_watermark  value.

The pipeline_stage() function's task is to process and move packets from the preceding pipeline stage.
This thread is running on most of the logical cores to create and arbitrarily long pipeline.

.. code-block:: c

    lcore_id = rte_lcore_id();

    previous_lcore_id = get_previous_lcore_id(lcore_id);

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
            if (!is_bit_set(port_id, portmask))
                    continue;

            tx = rings[lcore_id][port_id];
            rx = rings[previous_lcore_id][port_id];

            if (ring_state[port_id] != RING_READY) {
                    if (rte_ring_count(tx) > *low_watermark)
                            continue;
                    else
                            ring_state[port_id] = RING_READY;
            }

            /* Dequeue up to quota mbuf from rx */
            nb_dq_pkts = rte_ring_dequeue_burst(rx, pkts,
                            *quota, NULL);
            if (unlikely(nb_dq_pkts < 0))
                    continue;

            /* Enqueue them on tx */
            ret = rte_ring_enqueue_bulk(tx, pkts,
                            nb_dq_pkts, &free);
            if (RING_SIZE - free > *high_watermark)
                    ring_state[port_id] = RING_OVERLOADED;

            if (ret == 0) {

                    /*
                     * Return  mbufs to the pool,
                     * effectively dropping packets
                     */
                    for (i = 0; i < nb_dq_pkts; i++)
                            rte_pktmbuf_free(pkts[i]);
            }
    }

The thread's logic works mostly like receive_stage(),
except that packets are moved from ring to ring instead of port to ring.

In this example, no actual processing is done on the packets,
but pipeline_stage() is an ideal place to perform any processing required by the application.

Finally, the send_stage() function's task is to read packets from the last ring in a pipeline and
send them on the destination port defined in the port_pairs[] array.
It is running on the last available logical core only.

.. code-block:: c

    lcore_id = rte_lcore_id();

    previous_lcore_id = get_previous_lcore_id(lcore_id);

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
        if (!is_bit_set(port_id, portmask)) continue;

        dest_port_id = port_pairs[port_id];
        tx = rings[previous_lcore_id][port_id];

        if (rte_ring_empty(tx)) continue;

        /* Dequeue packets from tx and send them */

        nb_dq_pkts = rte_ring_dequeue_burst(tx, (void *) tx_pkts, *quota);
        nb_tx_pkts = rte_eth_tx_burst(dest_port_id, 0, tx_pkts, nb_dq_pkts);
    }

For each port in the port mask, up to \*quota packets are pulled from the last ring in its pipeline and
sent on the destination port paired with the current port.

Control Application - qwctl
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The qwctl application uses the rte_cmdline library to provide the user with an interactive command line that
can be used to modify and inspect parameters in a running qw application.
Those parameters are the global quota and low_watermark value as well as each ring's built-in high watermark.

Command Definitions
^^^^^^^^^^^^^^^^^^^

The available commands are defined in commands.c.

It is advised to use the cmdline sample application user guide as a reference for everything related to the rte_cmdline library.

Accessing Shared Variables
^^^^^^^^^^^^^^^^^^^^^^^^^^

The setup_shared_variables() function retrieves the shared variables quota and
low_watermark from the rte_memzone previously created by qw.

.. code-block:: c

    static void
    setup_shared_variables(void)
    {
        const struct rte_memzone *qw_memzone;

        qw_memzone = rte_memzone_lookup(QUOTA_WATERMARK_MEMZONE_NAME);
        if (qw_memzone == NULL)
            rte_exit(EXIT_FAILURE, "Couldn't find memzone\n");

        quota = qw_memzone->addr;

        low_watermark = (unsigned int *) qw_memzone->addr + 1;
        high_watermark = (unsigned int *) qw_memzone->addr + 2;
    }
