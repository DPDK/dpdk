..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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

Generic Receive Offload Library
===============================

Generic Receive Offload (GRO) is a widely used SW-based offloading
technique to reduce per-packet processing overhead. It gains performance
by reassembling small packets into large ones. To enable more flexibility
to applications, DPDK implements GRO as a standalone library. Applications
explicitly use the GRO library to merge small packets into large ones.

The GRO library assumes all input packets have correct checksums. In
addition, the GRO library doesn't re-calculate checksums for merged
packets. If input packets are IP fragmented, the GRO library assumes
they are complete packets (i.e. with L4 headers).

Currently, the GRO library implements TCP/IPv4 packet reassembly.

Reassembly Modes
----------------

The GRO library provides two reassembly modes: lightweight and
heavyweight mode. If applications want to merge packets in a simple way,
they can use the lightweight mode API. If applications want more
fine-grained controls, they can choose the heavyweight mode API.

Lightweight Mode
~~~~~~~~~~~~~~~~

The ``rte_gro_reassemble_burst()`` function is used for reassembly in
lightweight mode. It tries to merge N input packets at a time, where
N should be less than or equal to ``RTE_GRO_MAX_BURST_ITEM_NUM``.

In each invocation, ``rte_gro_reassemble_burst()`` allocates temporary
reassembly tables for the desired GRO types. Note that the reassembly
table is a table structure used to reassemble packets and different GRO
types (e.g. TCP/IPv4 GRO and TCP/IPv6 GRO) have different reassembly table
structures. The ``rte_gro_reassemble_burst()`` function uses the reassembly
tables to merge the N input packets.

For applications, performing GRO in lightweight mode is simple. They
just need to invoke ``rte_gro_reassemble_burst()``. Applications can get
GROed packets as soon as ``rte_gro_reassemble_burst()`` returns.

Heavyweight Mode
~~~~~~~~~~~~~~~~

The ``rte_gro_reassemble()`` function is used for reassembly in heavyweight
mode. Compared with the lightweight mode, performing GRO in heavyweight mode
is relatively complicated.

Before performing GRO, applications need to create a GRO context object
by calling ``rte_gro_ctx_create()``. A GRO context object holds the
reassembly tables of desired GRO types. Note that all update/lookup
operations on the context object are not thread safe. So if different
processes or threads want to access the same context object simultaneously,
some external syncing mechanisms must be used.

Once the GRO context is created, applications can then use the
``rte_gro_reassemble()`` function to merge packets. In each invocation,
``rte_gro_reassemble()`` tries to merge input packets with the packets
in the reassembly tables. If an input packet is an unsupported GRO type,
or other errors happen (e.g. SYN bit is set), ``rte_gro_reassemble()``
returns the packet to applications. Otherwise, the input packet is either
merged or inserted into a reassembly table.

When applications want to get GRO processed packets, they need to use
``rte_gro_timeout_flush()`` to flush them from the tables manually.

TCP/IPv4 GRO
------------

TCP/IPv4 GRO supports merging small TCP/IPv4 packets into large ones,
using a table structure called the TCP/IPv4 reassembly table.

TCP/IPv4 Reassembly Table
~~~~~~~~~~~~~~~~~~~~~~~~~

A TCP/IPv4 reassembly table includes a "key" array and an "item" array.
The key array keeps the criteria to merge packets and the item array
keeps the packet information.

Each key in the key array points to an item group, which consists of
packets which have the same criteria values but can't be merged. A key
in the key array includes two parts:

* ``criteria``: the criteria to merge packets. If two packets can be
  merged, they must have the same criteria values.

* ``start_index``: the item array index of the first packet in the item
  group.

Each element in the item array keeps the information of a packet. An item
in the item array mainly includes three parts:

* ``firstseg``: the mbuf address of the first segment of the packet.

* ``lastseg``: the mbuf address of the last segment of the packet.

* ``next_pkt_index``: the item array index of the next packet in the same
  item group. TCP/IPv4 GRO uses ``next_pkt_index`` to chain the packets
  that have the same criteria value but can't be merged together.

Procedure to Reassemble a Packet
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To reassemble an incoming packet needs three steps:

#. Check if the packet should be processed. Packets with one of the
   following properties aren't processed and are returned immediately:

   * FIN, SYN, RST, URG, PSH, ECE or CWR bit is set.

   * L4 payload length is 0.

#.  Traverse the key array to find a key which has the same criteria
    value with the incoming packet. If found, go to the next step.
    Otherwise, insert a new key and a new item for the packet.

#. Locate the first packet in the item group via ``start_index``. Then
   traverse all packets in the item group via ``next_pkt_index``. If a
   packet is found which can be merged with the incoming one, merge them
   together. If one isn't found, insert the packet into this item group.
   Note that to merge two packets is to link them together via mbuf's
   ``next`` field.

When packets are flushed from the reassembly table, TCP/IPv4 GRO updates
packet header fields for the merged packets. Note that before reassembling
the packet, TCP/IPv4 GRO doesn't check if the checksums of packets are
correct. Also, TCP/IPv4 GRO doesn't re-calculate checksums for merged
packets.
