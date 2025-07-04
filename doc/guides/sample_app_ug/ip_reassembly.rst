..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

IP Reassembly Sample Application
================================

The L3 Forwarding application is a simple example of packet processing using the DPDK.
The application performs L3 forwarding with reassembly for fragmented IPv4 and IPv6 packets.

Overview
--------

The application demonstrates the use of the DPDK libraries to implement packet forwarding
with reassembly for IPv4 and IPv6 fragmented packets.
The initialization and run- time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
The main difference from the L2 Forwarding sample application is that
it reassembles fragmented IPv4 and IPv6 packets before forwarding.
The maximum allowed size of reassembled packet is 9.5 KB.

There are two key differences from the L2 Forwarding sample application:

*   The first difference is that the forwarding decision is taken based on information read from the input packet's IP header.

*   The second difference is that the application differentiates between IP and non-IP traffic by means of offload flags.

The Longest Prefix Match (LPM for IPv4, LPM6 for IPv6) table is used to store/lookup an outgoing port number,
associated with that IPv4 address. Any unmatched packets are forwarded to the originating port.


Compiling the Application
-------------------------

To compile the sample application, see :doc:`compiling`.

The application is located in the ``ip_reassembly`` sub-directory.


Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_reassembly [EAL options] -- -p PORTMASK [-q NQ] [--maxflows=FLOWS>] [--flowttl=TTL[(s|ms)]]

where:

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   -q NQ: Number of RX queues per lcore

*   --maxflows=FLOWS: determines maximum number of active fragmented flows (1-65535). Default value: 4096.

*   --flowttl=TTL[(s|ms)]: determines maximum Time To Live for fragmented packet.
    If all fragments of the packet wouldn't appear within given time-out,
    then they are considered as invalid and will be dropped.
    Valid range is 1ms - 3600s. Default value: 1s.

To run the example in a Linux environment with 2 lcores (2,4) over 2 ports(0,2)
with 1 Rx queue per lcore:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_reassembly -l 2,4 -n 3 -- -p 5
    ...
    Initializing port 0 on lcore 2... Address:00:1B:21:76:FA:2C, rxq=0 txq=2,0 txq=4,1
    done: Link Up - speed 10000 Mbps - full-duplex
    Skipping disabled port 1
    Initializing port 2 on lcore 4... Address:00:1B:21:5C:FF:54, rxq=0 txq=2,0 txq=4,1
    done: Link Up - speed 10000 Mbps - full-duplex
    Skipping disabled port 3IP_FRAG: Socket 0: adding route 100.10.0.0/16 (port 0)
    IP_RSMBL: Socket 0: adding route 100.20.0.0/16 (port 1)
    ...
    IP_RSMBL: Socket 0: adding route 0101:0101:0101:0101:0101:0101:0101:0101/48 (port 0)
    IP_RSMBL: Socket 0: adding route 0201:0101:0101:0101:0101:0101:0101:0101/48 (port 1)
    ...
    IP_RSMBL: entering main loop on lcore 4
    IP_RSMBL: -- lcoreid=4 portid=2
    IP_RSMBL: entering main loop on lcore 2
    IP_RSMBL: -- lcoreid=2 portid=0

To run the example in a Linux environment with 1 lcore (4) over 2 ports(0,2)
with 2 Rx queues per lcore:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ip_reassembly -l 4 -n 3 -- -p 5 -q 2

To test the application, flows should be set up in the flow generator that match the values in the
l3fwd_ipv4_route_array and/or l3fwd_ipv6_route_array table.

Please note that in order to test this application,
the traffic generator should be generating valid fragmented IP packets.
For IPv6, the only supported case is when no other extension headers other than
the fragment extension header are present in the packet.

The default l3fwd_ipv4_route_array table is:

.. literalinclude:: ../../../examples/ip_reassembly/main.c
    :language: c
    :start-after: Default l3fwd_ipv4_route_array table. 8<
    :end-before: >8 End of default l3fwd_ipv4_route_array table.

The default l3fwd_ipv6_route_array table is:

.. literalinclude:: ../../../examples/ip_reassembly/main.c
    :language: c
    :start-after: Default l3fwd_ipv6_route_array table. 8<
    :end-before: >8 End of default l3fwd_ipv6_route_array table.

For example, for the fragmented input IPv4 packet with destination address: 100.10.1.1,
a reassembled IPv4 packet be sent out from port #0 to the destination address 100.10.1.1
once all the fragments are collected.

Explanation
-----------

The following sections provide in-depth explanation of the sample application code.
As mentioned in the overview section, the initialization and run-time paths
are very similar to those of the :doc:`l2_forward_real_virtual`.
The following sections describe aspects that are specific to the IP reassemble sample application.

IPv4 Fragment Table Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This application uses the :doc:`../prog_guide/ip_fragment_reassembly_lib` library.
The fragment table maintains information about already received fragments of the packet.
Each IP packet is uniquely identified by triple <Source IP address>, <Destination IP address>, <ID>.
To avoid lock contention, each Rx queue has its own fragment table.
The application cannot handle when different fragments of the same packet
arrive through different Rx queues.
Each table entry can hold information about packets
consisting of up to ``RTE_LIBRTE_IP_FRAG_MAX_FRAG`` fragments.

.. literalinclude:: ../../../examples/ip_reassembly/main.c
    :language: c
    :start-after: Each table entry holds information about packet fragmentation. 8<
    :end-before: >8 End of holding packet fragmentation.
    :dedent: 1

Mempools Initialization
~~~~~~~~~~~~~~~~~~~~~~~

The reassembly application demands a lot of mbuf's to be allocated.
At any given time, up to (2 \* max_flow_num \* RTE_LIBRTE_IP_FRAG_MAX_FRAG \* <maximum number of mbufs per packet>)
can be stored inside the fragment table waiting for remaining fragments.
To keep mempool size under reasonable limits
and to avoid a situation when one Rx queue can starve other queues,
each Rx queue uses its own mempool.

.. literalinclude:: ../../../examples/ip_reassembly/main.c
    :language: c
    :start-after: mbufs stored in the fragment table. 8<
    :end-before: >8 End of mbufs stored in the fragmentation table.
    :dedent: 1

Packet Reassembly and Forwarding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each input packet, the packet forwarding operation is done by the l3fwd_simple_forward() function.
If the packet is an IPv4 or IPv6 fragment, then it calls ``rte_ipv4_reassemble_packet()`` for IPv4 packets,
or ``rte_ipv6_reassemble_packet()`` for IPv6 packets.
These functions either return a pointer to a valid mbuf that contains a reassembled packet,
or NULL (if the packet can't be reassembled for some reason).
Then, ``l3fwd_simple_forward()`` continues with the code for the packet forwarding decision
(that is, the identification of the output interface for the packet) and
actual transmit of the packet.

The ``rte_ipv4_reassemble_packet()`` or ``rte_ipv6_reassemble_packet()`` are responsible for:

#.  Searching the fragment table for entry with packet's <IP Source Address, IP Destination Address, Packet ID>

#.  If the entry is found, then check if that entry already timed-out.
    If yes, then free all previously received fragments,
    and remove information about them from the entry.

#.  If no entry with such key is found, then try to create a new one by one of two ways:

    #.  Use as empty entry

    #.  Delete a timed-out entry, free mbufs associated with it mbufs and store a new entry with specified key in it.

#.  Update the entry with new fragment information and check
    if a packet can be reassembled (the packet's entry contains all fragments).

    #.  If yes, then, reassemble the packet, mark table's entry as empty and return the reassembled mbuf to the caller.

    #.  If no, then just return a NULL to the caller.

If at any stage of packet processing a reassembly function encounters an error
(can't insert new entry into the Fragment table, or invalid/timed-out fragment),
then it will free all associated with the packet fragments,
mark the table entry as invalid and return NULL to the caller.

Debug logging and Statistics Collection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``RTE_LIBRTE_IP_FRAG_TBL_STAT`` controls statistics collection for the IP fragment table.
This macro is disabled by default, but it can be enabled by modifying the appropriate line
in ``config/rte_config.h``.
To make ip_reassembly print the statistics to the standard output,
the user must send either an USR1, INT or TERM signal to the process.
For all of these signals, the ip_reassembly process prints Fragment table statistics for each Rx queue,
plus the INT and TERM will cause process termination as usual.
