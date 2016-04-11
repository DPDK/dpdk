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

IP Fragmentation and Reassembly Library
=======================================

The IP Fragmentation and Reassembly Library implements IPv4 and IPv6 packet fragmentation and reassembly.

Packet fragmentation
--------------------

Packet fragmentation routines divide input packet into number of fragments.
Both rte_ipv4_fragment_packet() and rte_ipv6_fragment_packet() functions assume that input mbuf data
points to the start of the IP header of the packet (i.e. L2 header is already stripped out).
To avoid copying of the actual packet's data zero-copy technique is used (rte_pktmbuf_attach).
For each fragment two new mbufs are created:

*   Direct mbuf -- mbuf that will contain L3 header of the new fragment.

*   Indirect mbuf -- mbuf that is attached to the mbuf with the original packet.
    It's data field points to the start of the original packets data plus fragment offset.

Then L3 header is copied from the original mbuf into the 'direct' mbuf and updated to reflect new fragmented status.
Note that for IPv4, header checksum is not recalculated and is set to zero.

Finally 'direct' and 'indirect' mbufs for each fragment are linked together via mbuf's next filed to compose a packet for the new fragment.

The caller has an ability to explicitly specify which mempools should be used to allocate 'direct' and 'indirect' mbufs from.

For more information about direct and indirect mbufs, refer to :ref:`direct_indirect_buffer`.

Packet reassembly
-----------------

IP Fragment Table
~~~~~~~~~~~~~~~~~

Fragment table maintains information about already received fragments of the packet.

Each IP packet is uniquely identified by triple <Source IP address>, <Destination IP address>, <ID>.

Note that all update/lookup operations on Fragment Table are not thread safe.
So if different execution contexts (threads/processes) will access the same table simultaneously,
then some external syncing mechanism have to be provided.

Each table entry can hold information about packets consisting of up to RTE_LIBRTE_IP_FRAG_MAX (by default: 4) fragments.

Code example, that demonstrates creation of a new Fragment table:

.. code-block:: c

    frag_cycles = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S * max_flow_ttl;
    bucket_num = max_flow_num + max_flow_num / 4;
    frag_tbl = rte_ip_frag_table_create(max_flow_num, bucket_entries, max_flow_num, frag_cycles, socket_id);

Internally Fragment table is a simple hash table.
The basic idea is to use two hash functions and <bucket_entries> \* associativity.
This provides 2 \* <bucket_entries> possible locations in the hash table for each key.
When the collision occurs and all 2 \* <bucket_entries> are occupied,
instead of reinserting existing keys into alternative locations, ip_frag_tbl_add() just returns a failure.

Also, entries that resides in the table longer then <max_cycles> are considered as invalid,
and could be removed/replaced by the new ones.

Note that reassembly demands a lot of mbuf's to be allocated.
At any given time up to (2 \* bucket_entries \* RTE_LIBRTE_IP_FRAG_MAX \* <maximum number of mbufs per packet>)
can be stored inside Fragment Table waiting for remaining fragments.

Packet Reassembly
~~~~~~~~~~~~~~~~~

Fragmented packets processing and reassembly is done by the rte_ipv4_frag_reassemble_packet()/rte_ipv6_frag_reassemble_packet.
Functions. They either return a pointer to valid mbuf that contains reassembled packet,
or NULL (if the packet can't be reassembled for some reason).

These functions are responsible for:

#.  Search the Fragment Table for entry with packet's <IPv4 Source Address, IPv4 Destination Address, Packet ID>.

#.  If the entry is found, then check if that entry already timed-out.
    If yes, then free all previously received fragments, and remove information about them from the entry.

#.  If no entry with such key is found, then try to create a new one by one of two ways:

    a) Use as empty entry.

    b) Delete a timed-out entry, free mbufs associated with it mbufs and store a new entry with specified key in it.

#.  Update the entry with new fragment information and check if a packet can be reassembled
    (the packet's entry contains all fragments).

    a) If yes, then, reassemble the packet, mark table's entry as empty and return the reassembled mbuf to the caller.

    b) If no, then return a NULL to the caller.

If at any stage of packet processing an error is encountered
(e.g: can't insert new entry into the Fragment Table, or invalid/timed-out fragment),
then the function will free all associated with the packet fragments,
mark the table entry as invalid and return NULL to the caller.

Debug logging and Statistics Collection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The RTE_LIBRTE_IP_FRAG_TBL_STAT config macro controls statistics collection for the Fragment Table.
This macro is not enabled by default.

The RTE_LIBRTE_IP_FRAG_DEBUG controls debug logging of IP fragments processing and reassembling.
This macro is disabled by default.
Note that while logging contains a lot of detailed information,
it slows down packet processing and might cause the loss of a lot of packets.
