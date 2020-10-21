..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

IP Fragmentation Sample Application
===================================

The IPv4 Fragmentation application is a simple example of packet processing
using the Data Plane Development Kit (DPDK).
The application does L3 forwarding with IPv4 and IPv6 packet fragmentation.

Overview
--------

The application demonstrates the use of zero-copy buffers for packet fragmentation.
The initialization and run-time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
This guide highlights the differences between the two applications.

There are three key differences from the L2 Forwarding sample application:

*   The first difference is that the IP Fragmentation sample application makes use of indirect buffers.

*   The second difference is that the forwarding decision is taken
    based on information read from the input packet's IP header.

*   The third difference is that the application differentiates between
    IP and non-IP traffic by means of offload flags.

The Longest Prefix Match (LPM for IPv4, LPM6 for IPv6) table is used to store/lookup an outgoing port number,
associated with that IP address.
Any unmatched packets are forwarded to the originating port.

By default, input frame sizes up to 9.5 KB are supported.
Before forwarding, the input IP packet is fragmented to fit into the "standard" Ethernet* v2 MTU (1500 bytes).

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ip_fragmentation`` sub-directory.

Running the Application
-----------------------

The LPM object is created and loaded with the pre-configured entries read from
global l3fwd_ipv4_route_array and l3fwd_ipv6_route_array tables.
For each input packet, the packet forwarding decision
(that is, the identification of the output interface for the packet) is taken as a result of LPM lookup.
If the IP packet size is greater than default output MTU,
then the input packet is fragmented and several fragments are sent via the output interface.

Application usage:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_fragmentation [EAL options] -- -p PORTMASK [-q NQ]

where:

*   -p PORTMASK is a hexadecimal bitmask of ports to configure

*   -q NQ is the number of queue (=ports) per lcore (the default is 1)

To run the example in linux environment with 2 lcores (2,4) over 2 ports(0,2) with 1 RX queue per lcore:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_fragmentation -l 2,4 -n 3 -- -p 5
    EAL: coremask set to 14
    EAL: Detected lcore 0 on socket 0
    EAL: Detected lcore 1 on socket 1
    EAL: Detected lcore 2 on socket 0
    EAL: Detected lcore 3 on socket 1
    EAL: Detected lcore 4 on socket 0
    ...

    Initializing port 0 on lcore 2... Address:00:1B:21:76:FA:2C, rxq=0 txq=2,0 txq=4,1
    done: Link Up - speed 10000 Mbps - full-duplex
    Skipping disabled port 1
    Initializing port 2 on lcore 4... Address:00:1B:21:5C:FF:54, rxq=0 txq=2,0 txq=4,1
    done: Link Up - speed 10000 Mbps - full-duplex
    Skipping disabled port 3IP_FRAG: Socket 0: adding route 100.10.0.0/16 (port 0)
    IP_FRAG: Socket 0: adding route 100.20.0.0/16 (port 1)
    ...
    IP_FRAG: Socket 0: adding route 0101:0101:0101:0101:0101:0101:0101:0101/48 (port 0)
    IP_FRAG: Socket 0: adding route 0201:0101:0101:0101:0101:0101:0101:0101/48 (port 1)
    ...
    IP_FRAG: entering main loop on lcore 4
    IP_FRAG: -- lcoreid=4 portid=2
    IP_FRAG: entering main loop on lcore 2
    IP_FRAG: -- lcoreid=2 portid=0

To run the example in linux environment with 1 lcore (4) over 2 ports(0,2) with 2 RX queues per lcore:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_fragmentation -l 4 -n 3 -- -p 5 -q 2

To test the application, flows should be set up in the flow generator that match the values in the
l3fwd_ipv4_route_array and/or l3fwd_ipv6_route_array table.

The default l3fwd_ipv4_route_array table is:

.. code-block:: c

    struct l3fwd_ipv4_route l3fwd_ipv4_route_array[] = {
        {RTE_IPV4(100, 10, 0, 0), 16, 0},
        {RTE_IPV4(100, 20, 0, 0), 16, 1},
        {RTE_IPV4(100, 30, 0, 0), 16, 2},
        {RTE_IPV4(100, 40, 0, 0), 16, 3},
        {RTE_IPV4(100, 50, 0, 0), 16, 4},
        {RTE_IPV4(100, 60, 0, 0), 16, 5},
        {RTE_IPV4(100, 70, 0, 0), 16, 6},
        {RTE_IPV4(100, 80, 0, 0), 16, 7},
    };

The default l3fwd_ipv6_route_array table is:

.. code-block:: c

    struct l3fwd_ipv6_route l3fwd_ipv6_route_array[] = {
        {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 0},
        {{2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 1},
        {{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 2},
        {{4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 3},
        {{5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 4},
        {{6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 5},
        {{7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 6},
        {{8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 7},
    };

For example, for the input IPv4 packet with destination address: 100.10.1.1 and packet length 9198 bytes,
seven IPv4 packets will be sent out from port #0 to the destination address 100.10.1.1:
six of those packets will have length 1500 bytes and one packet will have length 318 bytes.
IP Fragmentation sample application provides basic NUMA support
in that all the memory structures are allocated on all sockets that have active lcores on them.


Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.
