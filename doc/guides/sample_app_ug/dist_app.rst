..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Distributor Sample Application
==============================

The distributor sample application is a simple example of packet distribution
to cores using the Data Plane Development Kit (DPDK). It also makes use of
Intel Speed Select Technology - Base Frequency (Intel SST-BF) to pin the
distributor to the higher frequency core if available.

Overview
--------

The distributor application performs the distribution of packets that are received
on an RX_PORT to different cores. When processed by the cores, the destination
port of a packet is the port from the enabled port mask adjacent to the one on
which the packet was received, that is, if the first four ports are enabled
(port mask 0xf), ports 0 and 1 RX/TX into each other, and ports 2 and 3 RX/TX
into each other.

This application can be used to benchmark performance using the traffic
generator as shown in the figure below.

.. _figure_dist_perf:

.. figure:: img/dist_perf.*

   Performance Benchmarking Setup (Basic Environment)

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``distributor`` sub-directory.

Running the Application
-----------------------

#. The application has a number of command line options:

   ..  code-block:: console

       ./<build-dir>/examples/dpdk-distributor [EAL options] -- -p PORTMASK

   where,

   *   -p PORTMASK: Hexadecimal bitmask of ports to configure

#. To run the application in linux environment with 10 lcores, 4 ports,
   issue the command:

   ..  code-block:: console

       $ ./<build-dir>/examples/dpdk-distributor -l 1-9,22 -n 4 -- -p f

#. Refer to the DPDK Getting Started Guide for general information on running
   applications and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The distributor application consists of four types of threads: a receive
thread (``lcore_rx()``), a distributor thread (``lcore_dist()``), a set of
worker threads (``lcore_worker()``), and a transmit thread(``lcore_tx()``).
How these threads work together is shown in :numref:`figure_dist_app` below.
The ``main()`` function launches  threads of these four types.  Each thread
has a while loop which will be doing processing and which is terminated
only upon SIGINT or ctrl+C.

The receive thread receives the packets using ``rte_eth_rx_burst()`` and will
enqueue them to an rte_ring. The distributor thread will dequeue the packets
from the ring and assign them to workers (using ``rte_distributor_process()`` API).
This assignment is based on the tag (or flow ID) of the packet - indicated by
the hash field in the mbuf. For IP traffic, this field is automatically filled
by the NIC with the "usr" hash value for the packet, which works as a per-flow
tag.  The distributor thread communicates with the worker threads using a
cache-line swapping mechanism, passing up to 8 mbuf pointers at a time
(one cache line) to each worker.

More than one worker thread can exist as part of the application, and these
worker threads do simple packet processing by requesting packets from
the distributor, doing a simple XOR operation on the input port mbuf field
(to indicate the output port which will be used later for packet transmission)
and then finally returning the packets back to the distributor thread.

The distributor thread will then call the distributor api
``rte_distributor_returned_pkts()`` to get the processed packets, and will enqueue
them to another rte_ring for transfer to the TX thread for transmission on the
output port. The transmit thread will dequeue the packets from the ring and
transmit them on the output port specified in packet mbuf.

Users who wish to terminate the running of the application have to press ctrl+C
(or send SIGINT to the app). Upon this signal, a signal handler provided
in the application will terminate all running threads gracefully and print
final statistics to the user.

.. _figure_dist_app:

.. figure:: img/dist_app.*

   Distributor Sample Application Layout


Intel SST-BF Support
--------------------

In DPDK 19.05, support was added to the power management library for
Intel-SST-BF, a technology that allows some cores to run at a higher
frequency than others. An application note for Intel SST-BF is available,
and is entitled
`Intel Speed Select Technology – Base Frequency - Enhancing Performance <https://builders.intel.com/docs/networkbuilders/intel-speed-select-technology-base-frequency-enhancing-performance.pdf>`_

The distributor application was also enhanced to be aware of these higher
frequency SST-BF cores, and when starting the application, if high frequency
SST-BF cores are present in the core mask, the application will identify these
cores and pin the workloads appropriately. The distributor core is usually
the bottleneck, so this is given first choice of the high frequency SST-BF
cores, followed by the rx core and the tx core.

Debug Logging Support
---------------------

Debug logging is provided as part of the application; the user needs to uncomment
the line "#define DEBUG" defined in start of the application in main.c to enable debug logs.

Statistics
----------

The main function will print statistics on the console every second. These
statistics include the number of packets enqueued and dequeued at each stage
in the application, and also key statistics per worker, including how many
packets of each burst size (1-8) were sent to each worker thread.

Application Initialization
--------------------------

Command line parsing is done in the same way as it is done in the L2 Forwarding Sample
Application. See :ref:`l2_fwd_app_cmd_arguments`.

Mbuf pool initialization is done in the same way as it is done in the L2 Forwarding
Sample Application. See :ref:`l2_fwd_app_mbuf_init`.

Driver Initialization is done in same way as it is done in the L2 Forwarding Sample
Application. See :ref:`l2_fwd_app_dvr_init`.

RX queue initialization is done in the same way as it is done in the L2 Forwarding
Sample Application. See :ref:`l2_fwd_app_rx_init`.

TX queue initialization is done in the same way as it is done in the L2 Forwarding
Sample Application. See :ref:`l2_fwd_app_tx_init`.
