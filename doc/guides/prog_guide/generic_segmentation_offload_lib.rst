..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Generic Segmentation Offload Library
====================================

Overview
--------
Generic Segmentation Offload (GSO) is a widely used software implementation of
TCP Segmentation Offload (TSO), which reduces per-packet processing overhead.
Much like TSO, GSO gains performance by enabling upper layer applications to
process a smaller number of large packets (e.g. MTU size of 64KB), instead of
processing higher numbers of small packets (e.g. MTU size of 1500B), thus
reducing per-packet overhead.

For example, GSO allows guest kernel stacks to transmit over-sized TCP segments
that far exceed the kernel interface's MTU; this eliminates the need to segment
packets within the guest, and improves the data-to-overhead ratio of both the
guest-host link, and PCI bus. The expectation of the guest network stack in this
scenario is that segmentation of egress frames will take place either in the NIC
HW, or where that hardware capability is unavailable, either in the host
application, or network stack.

Bearing that in mind, the GSO library enables DPDK applications to segment
packets in software. Note however, that GSO is implemented as a standalone
library, and not via a 'fallback' mechanism (i.e. for when TSO is unsupported
in the underlying hardware); that is, applications must explicitly invoke the
GSO library to segment packets. The size of GSO segments ``(segsz)`` is
configurable by the application.

Limitations
-----------

#. The GSO library doesn't check if input packets have correct checksums.

#. In addition, the GSO library doesn't re-calculate checksums for segmented
   packets (that task is left to the application).

#. IP fragments are unsupported by the GSO library.

#. The egress interface's driver must support multi-segment packets.

#. Currently, the GSO library supports the following IPv4 packet types:

 - TCP
 - UDP
 - VxLAN
 - GRE

  See `Supported GSO Packet Types`_ for further details.

Packet Segmentation
-------------------

The ``rte_gso_segment()`` function is the GSO library's primary
segmentation API.

Before performing segmentation, an application must create a GSO context object
``(struct rte_gso_ctx)``, which provides the library with some of the
information required to understand how the packet should be segmented. Refer to
`How to Segment a Packet`_ for additional details on same. Once the GSO context
has been created, and populated, the application can then use the
``rte_gso_segment()`` function to segment packets.

The GSO library typically stores each segment that it creates in two parts: the
first part contains a copy of the original packet's headers, while the second
part contains a pointer to an offset within the original packet. This mechanism
is explained in more detail in `GSO Output Segment Format`_.

The GSO library supports both single- and multi-segment input mbufs.

GSO Output Segment Format
~~~~~~~~~~~~~~~~~~~~~~~~~
To reduce the number of expensive memcpy operations required when segmenting a
packet, the GSO library typically stores each segment that it creates as a
two-part mbuf (technically, this is termed a 'two-segment' mbuf; however, since
the elements produced by the API are also called 'segments', for clarity the
term 'part' is used here instead).

The first part of each output segment is a direct mbuf and contains a copy of
the original packet's headers, which must be prepended to each output segment.
These headers are copied from the original packet into each output segment.

The second part of each output segment, represents a section of data from the
original packet, i.e. a data segment. Rather than copy the data directly from
the original packet into the output segment (which would impact performance
considerably), the second part of each output segment is an indirect mbuf,
which contains no actual data, but simply points to an offset within the
original packet.

The combination of the 'header' segment and the 'data' segment constitutes a
single logical output GSO segment of the original packet. This is illustrated
in :numref:`figure_gso-output-segment-format`.

.. _figure_gso-output-segment-format:

.. figure:: img/gso-output-segment-format.*
   :align: center

   Two-part GSO output segment

In one situation, the output segment may contain additional 'data' segments.
This only occurs when:

- the input packet on which GSO is to be performed is represented by a
  multi-segment mbuf.

- the output segment is required to contain data that spans the boundaries
  between segments of the input multi-segment mbuf.

The GSO library traverses each segment of the input packet, and produces
numerous output segments; for optimal performance, the number of output
segments is kept to a minimum. Consequently, the GSO library maximizes the
amount of data contained within each output segment; i.e. each output segment
``segsz`` bytes of data. The only exception to this is in the case of the very
final output segment; if ``pkt_len`` % ``segsz``, then the final segment is
smaller than the rest.

In order for an output segment to meet its MSS, it may need to include data from
multiple input segments. Due to the nature of indirect mbufs (each indirect mbuf
can point to only one direct mbuf), the solution here is to add another indirect
mbuf to the output segment; this additional segment then points to the next
input segment. If necessary, this chaining process is repeated, until the sum of
all of the data 'contained' in the output segment reaches ``segsz``. This
ensures that the amount of data contained within each output segment is uniform,
with the possible exception of the last segment, as previously described.

:numref:`figure_gso-three-seg-mbuf` illustrates an example of a three-part
output segment. In this example, the output segment needs to include data from
the end of one input segment, and the beginning of another. To achieve this,
an additional indirect mbuf is chained to the second part of the output segment,
and is attached to the next input segment (i.e. it points to the data in the
next input segment).

.. _figure_gso-three-seg-mbuf:

.. figure:: img/gso-three-seg-mbuf.*
   :align: center

   Three-part GSO output segment

Supported GSO Packet Types
--------------------------

TCP/IPv4 GSO
~~~~~~~~~~~~
TCP/IPv4 GSO supports segmentation of suitably large TCP/IPv4 packets, which
may also contain an optional VLAN tag.

UDP/IPv4 GSO
~~~~~~~~~~~~
UDP/IPv4 GSO supports segmentation of suitably large UDP/IPv4 packets, which
may also contain an optional VLAN tag. UDP GSO is the same as IP fragmentation.
Specifically, UDP GSO treats the UDP header as a part of the payload and
does not modify it during segmentation. Therefore, after UDP GSO, only the
first output packet has the original UDP header, and others just have l2
and l3 headers.

VxLAN GSO
~~~~~~~~~
VxLAN packets GSO supports segmentation of suitably large VxLAN packets,
which contain an outer IPv4 header, inner TCP/IPv4 headers, and optional
inner and/or outer VLAN tag(s).

GRE GSO
~~~~~~~
GRE GSO supports segmentation of suitably large GRE packets, which contain
an outer IPv4 header, inner TCP/IPv4 headers, and an optional VLAN tag.

How to Segment a Packet
-----------------------

To segment an outgoing packet, an application must:

#. First create a GSO context ``(struct rte_gso_ctx)``; this contains:

   - a pointer to the mbuf pool for allocating the direct buffers, which are
     used to store the GSO segments' packet headers.

   - a pointer to the mbuf pool for allocating indirect buffers, which are
     used to locate GSO segments' packet payloads.

     .. note::

       An application may use the same pool for both direct and indirect
       buffers. However, since indirect mbufs simply store a pointer, the
       application may reduce its memory consumption by creating a separate memory
       pool, containing smaller elements, for the indirect pool.


   - the size of each output segment, including packet headers and payload,
     measured in bytes.

   - the bit mask of required GSO types. The GSO library uses the same macros as
     those that describe a physical device's TX offloading capabilities (i.e.
     ``DEV_TX_OFFLOAD_*_TSO``) for gso_types. For example, if an application
     wants to segment TCP/IPv4 packets, it should set gso_types to
     ``DEV_TX_OFFLOAD_TCP_TSO``. The only other supported values currently
     supported for gso_types are ``DEV_TX_OFFLOAD_VXLAN_TNL_TSO``, and
     ``DEV_TX_OFFLOAD_GRE_TNL_TSO``; a combination of these macros is also
     allowed.

   - a flag, that indicates whether the IPv4 headers of output segments should
     contain fixed or incremental ID values.

2. Set the appropriate ol_flags in the mbuf.

   - The GSO library use the value of an mbuf's ``ol_flags`` attribute to
     determine how a packet should be segmented. It is the application's
     responsibility to ensure that these flags are set.

   - For example, in order to segment TCP/IPv4 packets, the application should
     add the ``PKT_TX_IPV4`` and ``PKT_TX_TCP_SEG`` flags to the mbuf's
     ol_flags.

   - If checksum calculation in hardware is required, the application should
     also add the ``PKT_TX_TCP_CKSUM`` and ``PKT_TX_IP_CKSUM`` flags.

#. Check if the packet should be processed. Packets with one of the
   following properties are not processed and are returned immediately:

   - Packet length is less than ``segsz`` (i.e. GSO is not required).

   - Packet type is not supported by GSO library (see
     `Supported GSO Packet Types`_).

   - Application has not enabled GSO support for the packet type.

   - Packet's ol_flags have been incorrectly set.

#. Allocate space in which to store the output GSO segments. If the amount of
   space allocated by the application is insufficient, segmentation will fail.

#. Invoke the GSO segmentation API, ``rte_gso_segment()``.

#. If required, update the L3 and L4 checksums of the newly-created segments.
   For tunneled packets, the outer IPv4 headers' checksums should also be
   updated. Alternatively, the application may offload checksum calculation
   to HW.
