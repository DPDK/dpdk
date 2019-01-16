..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Generic Receive Offload Library
===============================

Generic Receive Offload (GRO) is a widely used SW-based offloading
technique to reduce per-packet processing overheads. By reassembling
small packets into larger ones, GRO enables applications to process
fewer large packets directly, thus reducing the number of packets to
be processed. To benefit DPDK-based applications, like Open vSwitch,
DPDK also provides own GRO implementation. In DPDK, GRO is implemented
as a standalone library. Applications explicitly use the GRO library to
reassemble packets.

Overview
--------

In the GRO library, there are many GRO types which are defined by packet
types. One GRO type is in charge of process one kind of packets. For
example, TCP/IPv4 GRO processes TCP/IPv4 packets.

Each GRO type has a reassembly function, which defines own algorithm and
table structure to reassemble packets. We assign input packets to the
corresponding GRO functions by MBUF->packet_type.

The GRO library doesn't check if input packets have correct checksums and
doesn't re-calculate checksums for merged packets. The GRO library
assumes the packets are complete (i.e., MF==0 && frag_off==0), when IP
fragmentation is possible (i.e., DF==0). Additionally, it complies RFC
6864 to process the IPv4 ID field.

Currently, the GRO library provides GRO supports for TCP/IPv4 packets and
VxLAN packets which contain an outer IPv4 header and an inner TCP/IPv4
packet.

Two Sets of API
---------------

For different usage scenarios, the GRO library provides two sets of API.
The one is called the lightweight mode API, which enables applications to
merge a small number of packets rapidly; the other is called the
heavyweight mode API, which provides fine-grained controls to
applications and supports to merge a large number of packets.

Lightweight Mode API
~~~~~~~~~~~~~~~~~~~~

The lightweight mode only has one function ``rte_gro_reassemble_burst()``,
which process N packets at a time. Using the lightweight mode API to
merge packets is very simple. Calling ``rte_gro_reassemble_burst()`` is
enough. The GROed packets are returned to applications as soon as it
finishes.

In ``rte_gro_reassemble_burst()``, table structures of different GRO
types are allocated in the stack. This design simplifies applications'
operations. However, limited by the stack size, the maximum number of
packets that ``rte_gro_reassemble_burst()`` can process in an invocation
should be less than or equal to ``RTE_GRO_MAX_BURST_ITEM_NUM``.

Heavyweight Mode API
~~~~~~~~~~~~~~~~~~~~

Compared with the lightweight mode, using the heavyweight mode API is
relatively complex. Firstly, applications need to create a GRO context
by ``rte_gro_ctx_create()``. ``rte_gro_ctx_create()`` allocates tables
structures in the heap and stores their pointers in the GRO context.
Secondly, applications use ``rte_gro_reassemble()`` to merge packets.
If input packets have invalid parameters, ``rte_gro_reassemble()``
returns them to applications. For example, packets of unsupported GRO
types or TCP SYN packets are returned. Otherwise, the input packets are
either merged with the existed packets in the tables or inserted into the
tables. Finally, applications use ``rte_gro_timeout_flush()`` to flush
packets from the tables, when they want to get the GROed packets.

Note that all update/lookup operations on the GRO context are not thread
safe. So if different processes or threads want to access the same
context object simultaneously, some external syncing mechanisms must be
used.

Reassembly Algorithm
--------------------

The reassembly algorithm is used for reassembling packets. In the GRO
library, different GRO types can use different algorithms. In this
section, we will introduce an algorithm, which is used by TCP/IPv4 GRO
and VxLAN GRO.

Challenges
~~~~~~~~~~

The reassembly algorithm determines the efficiency of GRO. There are two
challenges in the algorithm design:

- a high cost algorithm/implementation would cause packet dropping in a
  high speed network.

- packet reordering makes it hard to merge packets. For example, Linux
  GRO fails to merge packets when encounters packet reordering.

The above two challenges require our algorithm is:

- lightweight enough to scale fast networking speed

- capable of handling packet reordering

In DPDK GRO, we use a key-based algorithm to address the two challenges.

Key-based Reassembly Algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:numref:`figure_gro-key-algorithm` illustrates the procedure of the
key-based algorithm. Packets are classified into "flows" by some header
fields (we call them as "key"). To process an input packet, the algorithm
searches for a matched "flow" (i.e., the same value of key) for the
packet first, then checks all packets in the "flow" and tries to find a
"neighbor" for it. If find a "neighbor", merge the two packets together.
If can't find a "neighbor", store the packet into its "flow". If can't
find a matched "flow", insert a new "flow" and store the packet into the
"flow".

.. note::
        Packets in the same "flow" that can't merge are always caused
        by packet reordering.

The key-based algorithm has two characters:

- classifying packets into "flows" to accelerate packet aggregation is
  simple (address challenge 1).

- storing out-of-order packets makes it possible to merge later (address
  challenge 2).

.. _figure_gro-key-algorithm:

.. figure:: img/gro-key-algorithm.*
   :align: center

   Key-based Reassembly Algorithm

TCP/IPv4 GRO
------------

The table structure used by TCP/IPv4 GRO contains two arrays: flow array
and item array. The flow array keeps flow information, and the item array
keeps packet information.

Header fields used to define a TCP/IPv4 flow include:

- source and destination: Ethernet and IP address, TCP port

- TCP acknowledge number

TCP/IPv4 packets whose FIN, SYN, RST, URG, PSH, ECE or CWR bit is set
won't be processed.

Header fields deciding if two packets are neighbors include:

- TCP sequence number

- IPv4 ID. The IPv4 ID fields of the packets, whose DF bit is 0, should
  be increased by 1.

VxLAN GRO
---------

The table structure used by VxLAN GRO, which is in charge of processing
VxLAN packets with an outer IPv4 header and inner TCP/IPv4 packet, is
similar with that of TCP/IPv4 GRO. Differently, the header fields used
to define a VxLAN flow include:

- outer source and destination: Ethernet and IP address, UDP port

- VxLAN header (VNI and flag)

- inner source and destination: Ethernet and IP address, TCP port

Header fields deciding if packets are neighbors include:

- outer IPv4 ID. The IPv4 ID fields of the packets, whose DF bit in the
  outer IPv4 header is 0, should be increased by 1.

- inner TCP sequence number

- inner IPv4 ID. The IPv4 ID fields of the packets, whose DF bit in the
  inner IPv4 header is 0, should be increased by 1.

.. note::
        We comply RFC 6864 to process the IPv4 ID field. Specifically,
        we check IPv4 ID fields for the packets whose DF bit is 0 and
        ignore IPv4 ID fields for the packets whose DF bit is 1.
        Additionally, packets which have different value of DF bit can't
        be merged.

GRO Library Limitations
-----------------------

- GRO library uses MBUF->l2_len/l3_len/l4_len/outer_l2_len/
  outer_l3_len/packet_type to get protocol headers for the
  input packet, rather than parsing the packet header. Therefore,
  before call GRO APIs to merge packets, user applications
  must set MBUF->l2_len/l3_len/l4_len/outer_l2_len/outer_l3_len/
  packet_type to the same values as the protocol headers of the
  packet.

- GRO library doesn't support to process the packets with IPv4
  Options or VLAN tagged.

- GRO library just supports to process the packet organized
  in a single MBUF. If the input packet consists of multiple
  MBUFs (i.e. chained MBUFs), GRO reassembly behaviors are
  unknown.
