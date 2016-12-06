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

.. _Mbuf_Library:

Mbuf Library
============

The mbuf library provides the ability to allocate and free buffers (mbufs)
that may be used by the DPDK application to store message buffers.
The message buffers are stored in a mempool, using the :ref:`Mempool Library <Mempool_Library>`.

A rte_mbuf struct can carry network packet buffers
or generic control buffers (indicated by the CTRL_MBUF_FLAG).
This can be extended to other types.
The rte_mbuf header structure is kept as small as possible and currently uses
just two cache lines, with the most frequently used fields being on the first
of the two cache lines.

Design of Packet Buffers
------------------------

For the storage of the packet data (including protocol headers), two approaches were considered:

#.  Embed metadata within a single memory buffer the structure followed by a fixed size area for the packet data.

#.  Use separate memory buffers for the metadata structure and for the packet data.

The advantage of the first method is that it only needs one operation to allocate/free the whole memory representation of a packet.
On the other hand, the second method is more flexible and allows
the complete separation of the allocation of metadata structures from the allocation of packet data buffers.

The first method was chosen for the DPDK.
The metadata contains control information such as message type, length,
offset to the start of the data and a pointer for additional mbuf structures allowing buffer chaining.

Message buffers that are used to carry network packets can handle buffer chaining
where multiple buffers are required to hold the complete packet.
This is the case for jumbo frames that are composed of many mbufs linked together through their next field.

For a newly allocated mbuf, the area at which the data begins in the message buffer is
RTE_PKTMBUF_HEADROOM bytes after the beginning of the buffer, which is cache aligned.
Message buffers may be used to carry control information, packets, events,
and so on between different entities in the system.
Message buffers may also use their buffer pointers to point to other message buffer data sections or other structures.

:numref:`figure_mbuf1` and :numref:`figure_mbuf2` show some of these scenarios.

.. _figure_mbuf1:

.. figure:: img/mbuf1.*

   An mbuf with One Segment


.. _figure_mbuf2:

.. figure:: img/mbuf2.*

   An mbuf with Three Segments


The Buffer Manager implements a fairly standard set of buffer access functions to manipulate network packets.

Buffers Stored in Memory Pools
------------------------------

The Buffer Manager uses the :ref:`Mempool Library <Mempool_Library>` to allocate buffers.
Therefore, it ensures that the packet header is interleaved optimally across the channels and ranks for L3 processing.
An mbuf contains a field indicating the pool that it originated from.
When calling rte_ctrlmbuf_free(m) or rte_pktmbuf_free(m), the mbuf returns to its original pool.

Constructors
------------

Packet and control mbuf constructors are provided by the API.
The rte_pktmbuf_init() and rte_ctrlmbuf_init() functions initialize some fields in the mbuf structure that
are not modified by the user once created (mbuf type, origin pool, buffer start address, and so on).
This function is given as a callback function to the rte_mempool_create() function at pool creation time.

Allocating and Freeing mbufs
----------------------------

Allocating a new mbuf requires the user to specify the mempool from which the mbuf should be taken.
For any newly-allocated mbuf, it contains one segment, with a length of 0.
The offset to data is initialized to have some bytes of headroom in the buffer (RTE_PKTMBUF_HEADROOM).

Freeing a mbuf means returning it into its original mempool.
The content of an mbuf is not modified when it is stored in a pool (as a free mbuf).
Fields initialized by the constructor do not need to be re-initialized at mbuf allocation.

When freeing a packet mbuf that contains several segments, all of them are freed and returned to their original mempool.

Manipulating mbufs
------------------

This library provides some functions for manipulating the data in a packet mbuf. For instance:

    *  Get data length

    *  Get a pointer to the start of data

    *  Prepend data before data

    *   Append data after data

    *   Remove data at the beginning of the buffer (rte_pktmbuf_adj())

    *   Remove data at the end of the buffer (rte_pktmbuf_trim()) Refer to the *DPDK API Reference* for details.

Meta Information
----------------

Some information is retrieved by the network driver and stored in an mbuf to make processing easier.
For instance, the VLAN, the RSS hash result (see :ref:`Poll Mode Driver <Poll_Mode_Driver>`)
and a flag indicating that the checksum was computed by hardware.

An mbuf also contains the input port (where it comes from), and the number of segment mbufs in the chain.

For chained buffers, only the first mbuf of the chain stores this meta information.

For instance, this is the case on RX side for the IEEE1588 packet
timestamp mechanism, the VLAN tagging and the IP checksum computation.

On TX side, it is also possible for an application to delegate some
processing to the hardware if it supports it. For instance, the
PKT_TX_IP_CKSUM flag allows to offload the computation of the IPv4
checksum.

The following examples explain how to configure different TX offloads on
a vxlan-encapsulated tcp packet:
``out_eth/out_ip/out_udp/vxlan/in_eth/in_ip/in_tcp/payload``

- calculate checksum of out_ip::

    mb->l2_len = len(out_eth)
    mb->l3_len = len(out_ip)
    mb->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CSUM
    set out_ip checksum to 0 in the packet

  This is supported on hardware advertising DEV_TX_OFFLOAD_IPV4_CKSUM.

- calculate checksum of out_ip and out_udp::

    mb->l2_len = len(out_eth)
    mb->l3_len = len(out_ip)
    mb->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CSUM | PKT_TX_UDP_CKSUM
    set out_ip checksum to 0 in the packet
    set out_udp checksum to pseudo header using rte_ipv4_phdr_cksum()

  This is supported on hardware advertising DEV_TX_OFFLOAD_IPV4_CKSUM
  and DEV_TX_OFFLOAD_UDP_CKSUM.

- calculate checksum of in_ip::

    mb->l2_len = len(out_eth + out_ip + out_udp + vxlan + in_eth)
    mb->l3_len = len(in_ip)
    mb->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CSUM
    set in_ip checksum to 0 in the packet

  This is similar to case 1), but l2_len is different. It is supported
  on hardware advertising DEV_TX_OFFLOAD_IPV4_CKSUM.
  Note that it can only work if outer L4 checksum is 0.

- calculate checksum of in_ip and in_tcp::

    mb->l2_len = len(out_eth + out_ip + out_udp + vxlan + in_eth)
    mb->l3_len = len(in_ip)
    mb->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CSUM | PKT_TX_TCP_CKSUM
    set in_ip checksum to 0 in the packet
    set in_tcp checksum to pseudo header using rte_ipv4_phdr_cksum()

  This is similar to case 2), but l2_len is different. It is supported
  on hardware advertising DEV_TX_OFFLOAD_IPV4_CKSUM and
  DEV_TX_OFFLOAD_TCP_CKSUM.
  Note that it can only work if outer L4 checksum is 0.

- segment inner TCP::

    mb->l2_len = len(out_eth + out_ip + out_udp + vxlan + in_eth)
    mb->l3_len = len(in_ip)
    mb->l4_len = len(in_tcp)
    mb->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM |
      PKT_TX_TCP_SEG;
    set in_ip checksum to 0 in the packet
    set in_tcp checksum to pseudo header without including the IP
      payload length using rte_ipv4_phdr_cksum()

  This is supported on hardware advertising DEV_TX_OFFLOAD_TCP_TSO.
  Note that it can only work if outer L4 checksum is 0.

- calculate checksum of out_ip, in_ip, in_tcp::

    mb->outer_l2_len = len(out_eth)
    mb->outer_l3_len = len(out_ip)
    mb->l2_len = len(out_udp + vxlan + in_eth)
    mb->l3_len = len(in_ip)
    mb->ol_flags |= PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IP_CKSUM  | \
      PKT_TX_IP_CKSUM |  PKT_TX_TCP_CKSUM;
    set out_ip checksum to 0 in the packet
    set in_ip checksum to 0 in the packet
    set in_tcp checksum to pseudo header using rte_ipv4_phdr_cksum()

  This is supported on hardware advertising DEV_TX_OFFLOAD_IPV4_CKSUM,
  DEV_TX_OFFLOAD_UDP_CKSUM and DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM.

The list of flags and their precise meaning is described in the mbuf API
documentation (rte_mbuf.h). Also refer to the testpmd source code
(specifically the csumonly.c file) for details.

.. _direct_indirect_buffer:

Direct and Indirect Buffers
---------------------------

A direct buffer is a buffer that is completely separate and self-contained.
An indirect buffer behaves like a direct buffer but for the fact that the buffer pointer and
data offset in it refer to data in another direct buffer.
This is useful in situations where packets need to be duplicated or fragmented,
since indirect buffers provide the means to reuse the same packet data across multiple buffers.

A buffer becomes indirect when it is "attached" to a direct buffer using the rte_pktmbuf_attach() function.
Each buffer has a reference counter field and whenever an indirect buffer is attached to the direct buffer,
the reference counter on the direct buffer is incremented.
Similarly, whenever the indirect buffer is detached, the reference counter on the direct buffer is decremented.
If the resulting reference counter is equal to 0, the direct buffer is freed since it is no longer in use.

There are a few things to remember when dealing with indirect buffers.
First of all, an indirect buffer is never attached to another indirect buffer.
Attempting to attach buffer A to indirect buffer B that is attached to C, makes rte_pktmbuf_attach() automatically attach A to C, effectively cloning B.
Secondly, for a buffer to become indirect, its reference counter must be equal to 1,
that is, it must not be already referenced by another indirect buffer.
Finally, it is not possible to reattach an indirect buffer to the direct buffer (unless it is detached first).

While the attach/detach operations can be invoked directly using the recommended rte_pktmbuf_attach() and rte_pktmbuf_detach() functions,
it is suggested to use the higher-level rte_pktmbuf_clone() function,
which takes care of the correct initialization of an indirect buffer and can clone buffers with multiple segments.

Since indirect buffers are not supposed to actually hold any data,
the memory pool for indirect buffers should be configured to indicate the reduced memory consumption.
Examples of the initialization of a memory pool for indirect buffers (as well as use case examples for indirect buffers)
can be found in several of the sample applications, for example, the IPv4 Multicast sample application.

Debug
-----

In debug mode (CONFIG_RTE_MBUF_DEBUG is enabled),
the functions of the mbuf library perform sanity checks before any operation (such as, buffer corruption, bad type, and so on).

Use Cases
---------

All networking application should use mbufs to transport network packets.
