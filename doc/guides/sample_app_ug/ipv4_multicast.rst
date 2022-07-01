..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

IPv4 Multicast Sample Application
=================================

The IPv4 Multicast application is a simple example of packet processing
using the Data Plane Development Kit (DPDK).
The application performs L3 multicasting.

Overview
--------

The application demonstrates the use of zero-copy buffers for packet forwarding.
The initialization and run-time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
This guide highlights the differences between the two applications.
There are two key differences from the L2 Forwarding sample application:

*   The IPv4 Multicast sample application makes use of indirect buffers.

*   The forwarding decision is taken based on information read from the input packet's IPv4 header.

The lookup method is the Four-byte Key (FBK) hash-based method.
The lookup table is composed of pairs of destination IPv4 address (the FBK)
and a port mask associated with that IPv4 address.
By default, the following IP addresses and their respective port masks are added:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
   :language: c
   :start-after: mcast_group_table
   :end-before: };

.. note::

    The max port mask supported in the given hash table is 0xf, so only first
    four ports can be supported.
    If using non-consecutive ports, use the destination IPv4 address accordingly.

For convenience and simplicity, this sample application does not take IANA-assigned multicast addresses into account,
but instead equates the last four bytes of the multicast group (that is, the last four bytes of the destination IP address)
with the mask of ports to multicast packets to.
Also, the application does not consider the Ethernet addresses;
it looks only at the IPv4 destination address for any given packet.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ipv4_multicast`` sub-directory.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ipv4_multicast [EAL options] -- -p PORTMASK [-q NQ]

where,

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   -q NQ: determines the number of queues per lcore

.. note::

    Unlike the basic L2/L3 Forwarding sample applications,
    NUMA support is not provided in the IPv4 Multicast sample application.

Typically, to run the IPv4 Multicast sample application, issue the following command (as root):

.. code-block:: console

    ./<build_dir>/examples/dpdk-ipv4_multicast -l 0-3 -n 3 -- -p 0x3 -q 1

In this command:

*   The -l option enables cores 0, 1, 2 and 3

*   The -n option specifies 3 memory channels

*   The -p option enables ports 0 and 1

*   The -q option assigns 1 queue to each lcore

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.
As mentioned in the overview section,
the initialization and run-time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
The following sections describe aspects that are specific to the IPv4 Multicast sample application.

Memory Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~

The IPv4 Multicast sample application uses three memory pools.
Two of the pools are for indirect buffers used for packet duplication purposes.
Memory pools for indirect buffers are initialized differently from the memory pool for direct buffers:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Create the mbuf pools. 8<
    :end-before: >8 End of create mbuf pools.
    :dedent: 1

The reason for this is because indirect buffers are not supposed to hold any packet data and
therefore can be initialized with lower amount of reserved memory for each buffer.

Hash Initialization
~~~~~~~~~~~~~~~~~~~

The hash object is created and loaded with the pre-configured entries read from a global array:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Hash object is created and loaded. 8<
    :end-before: >8 End of hash object is created and loaded.

Forwarding
~~~~~~~~~~

All forwarding is done inside the mcast_forward() function.
Firstly, the Ethernet* header is removed from the packet and the IPv4 address is extracted from the IPv4 header:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Remove the Ethernet header from the input packet. 8<
    :end-before: >8 End of removing the Ethernet header from the input packet.
    :dedent: 1

Then, the packet is checked to see if it has a multicast destination address and
if the routing table has any ports assigned to the destination address:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Check valid multicast address. 8<
    :end-before: >8 End of valid multicast address check.
    :dedent: 1

Then, the number of ports in the destination portmask is calculated with the help of the bitcnt() function:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Get number of bits set. 8<
    :end-before: >8 End of getting number of bits set.

This is done to determine which forwarding algorithm to use.
This is explained in more detail in the next section.

Thereafter, a destination Ethernet address is constructed:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Construct destination ethernet address. 8<
    :end-before: >8 End of constructing destination ethernet address.
    :dedent: 1

Since Ethernet addresses are also part of the multicast process, each outgoing packet carries the same destination Ethernet address.
The destination Ethernet address is constructed from the lower 23 bits of the multicast group OR-ed
with the Ethernet address 01:00:5e:00:00:00, as per RFC 1112:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Construct Ethernet multicast address from IPv4 multicast Address. 8<
    :end-before: >8 End of Construction of multicast address from IPv4 multicast address.

Then, packets are dispatched to the destination ports according to the portmask associated with a multicast group:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Packets dispatched to destination ports. 8<
    :end-before: >8 End of packets dispatched to destination ports.
    :dedent: 1

The actual packet transmission is done in the mcast_send_pkt() function:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Write new Ethernet header to outgoing packets. 8<
    :end-before: >8 End of writing new Ethernet headers.

Buffer Cloning
~~~~~~~~~~~~~~

This is the most important part of the application since it demonstrates the use of zero- copy buffer cloning.
There are two approaches for creating the outgoing packet and although both are based on the data zero-copy idea,
there are some differences in the detail.

The first approach creates a clone of the input packet, for example,
walk though all segments of the input packet and for each of segment,
create a new buffer and attach that new buffer to the segment
(refer to rte_pktmbuf_clone() in the rte_mbuf library for more details).
A new buffer is then allocated for the packet header and is prepended to the cloned buffer.

The second approach does not make a clone, it just increments the reference counter for all input packet segment,
allocates a new buffer for the packet header and prepends it to the input packet.

Basically, the first approach reuses only the input packet's data, but creates its own copy of packet's metadata.
The second approach reuses both input packet's data and metadata.

The advantage of first approach is that each outgoing packet has its own copy of the metadata,
so we can safely modify the data pointer of the input packet.
That allows us to skip creation if the output packet is for the last destination port
and instead modify input packet's header in place.
For example, for N destination ports, we need to invoke mcast_out_pkt() (N-1) times.

The advantage of the second approach is that there is less work to be done for each outgoing packet,
that is, the "clone" operation is skipped completely.
However, there is a price to pay.
The input packet's metadata must remain intact, so for N destination ports,
we need to invoke mcast_out_pkt() (N) times.

Therefore, for a small number of outgoing ports (and segments in the input packet),
first approach is faster.
As the number of outgoing ports (and/or input segments) grows, the second approach becomes more preferable.

Depending on the number of segments or the number of ports in the outgoing portmask,
either the first (with cloning) or the second (without cloning) approach is taken:

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: Should we use rte_pktmbuf_clone() or not. 8<
    :end-before: >8 End of using rte_pktmbuf_clone().
    :dedent: 1

It is the mcast_out_pkt() function that performs the packet duplication (either with or without actually cloning the buffers):

.. literalinclude:: ../../../examples/ipv4_multicast/main.c
    :language: c
    :start-after: mcast_out_pkt 8<
    :end-before: >8 End of mcast_out_kt.
