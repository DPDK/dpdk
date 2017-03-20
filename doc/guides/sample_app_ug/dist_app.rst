..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

Distributor Sample Application
==============================

The distributor sample application is a simple example of packet distribution
to cores using the Data Plane Development Kit (DPDK).

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

#.  Go to the sample application directory:

    ..  code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/distributor

#.  Set the target (a default target is used if not specified). For example:

    ..  code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the DPDK Getting Started Guide for possible RTE_TARGET values.

#.  Build the application:

    ..  code-block:: console

        make

Running the Application
-----------------------

#. The application has a number of command line options:

   ..  code-block:: console

       ./build/distributor_app [EAL options] -- -p PORTMASK

   where,

   *   -p PORTMASK: Hexadecimal bitmask of ports to configure

#. To run the application in linuxapp environment with 10 lcores, 4 ports,
   issue the command:

   ..  code-block:: console

       $ ./build/distributor_app -l 1-9,22 -n 4 -- -p f

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
