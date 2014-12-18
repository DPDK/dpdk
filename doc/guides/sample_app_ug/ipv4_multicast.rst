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

IPv4 Multicast Sample Application
=================================

The IPv4 Multicast application is a simple example of packet processing
using the Data Plane Development Kit (DPDK).
The application performs L3 multicasting.

Overview
--------

The application demonstrates the use of zero-copy buffers for packet forwarding.
The initialization and run-time paths are very similar to those of the L2 forwarding application
(see Chapter 9 "L2 Forwarding Sample Application (in Real and Virtualized Environments)" for details more information).
This guide highlights the differences between the two applications.
There are two key differences from the L2 Forwarding sample application:

*   The IPv4 Multicast sample application makes use of indirect buffers.

*   The forwarding decision is taken based on information read from the input packet's IPv4 header.

The lookup method is the Four-byte Key (FBK) hash-based method.
The lookup table is composed of pairs of destination IPv4 address (the FBK)
and a port mask associated with that IPv4 address.

For convenience and simplicity, this sample application does not take IANA-assigned multicast addresses into account,
but instead equates the last four bytes of the multicast group (that is, the last four bytes of the destination IP address)
with the mask of ports to multicast packets to.
Also, the application does not consider the Ethernet addresses;
it looks only at the IPv4 destination address for any given packet.

Building the Application
------------------------

To compile the application:

#.  Go to the sample application directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/ipv4_multicast

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

.. note::

    The compiled application is written to the build subdirectory.
    To have the application written to a different location,
    the O=/path/to/build/directory option may be specified in the make command.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./build/ipv4_multicast [EAL options] -- -p PORTMASK [-q NQ]

where,

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   -q NQ: determines the number of queues per lcore

.. note::

    Unlike the basic L2/L3 Forwarding sample applications,
    NUMA support is not provided in the IPv4 Multicast sample application.

Typically, to run the IPv4 Multicast sample application, issue the following command (as root):

.. code-block:: console

    ./build/ipv4_multicast -c 0x00f -n 3 -- -p 0x3 -q 1

In this command:

*   The -c option enables cores 0, 1, 2 and 3

*   The -n option specifies 3 memory channels

*   The -p option enables ports 0 and 1

*   The -q option assigns 1 queue to each lcore

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.
As mentioned in the overview section,
the initialization and run-time paths are very similar to those of the L2 Forwarding sample application
(see Chapter 9 "L2 Forwarding Sample Application in Real and Virtualized Environments" for more information).
The following sections describe aspects that are specific to the IPv4 Multicast sample application.

Memory Pool Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~

The IPv4 Multicast sample application uses three memory pools.
Two of the pools are for indirect buffers used for packet duplication purposes.
Memory pools for indirect buffers are initialized differently from the memory pool for direct buffers:

.. code-block:: c

    packet_pool = rte_mempool_create("packet_pool", NB_PKT_MBUF, PKT_MBUF_SIZE, 32, sizeof(struct rte_pktmbuf_pool_private),
                                     rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL, rte_socket_id(), 0);

    header_pool = rte_mempool_create("header_pool", NB_HDR_MBUF, HDR_MBUF_SIZE, 32, 0, NULL, NULL, rte_pktmbuf_init, NULL, rte_socket_id(), 0);
    clone_pool = rte_mempool_create("clone_pool", NB_CLONE_MBUF,
    CLONE_MBUF_SIZE, 32, 0, NULL, NULL, rte_pktmbuf_init, NULL, rte_socket_id(), 0);

The reason for this is because indirect buffers are not supposed to hold any packet data and
therefore can be initialized with lower amount of reserved memory for each buffer.

Hash Initialization
~~~~~~~~~~~~~~~~~~~

The hash object is created and loaded with the pre-configured entries read from a global array:

.. code-block:: c

    static int

    init_mcast_hash(void)
    {
        uint32_t i;
        mcast_hash_params.socket_id = rte_socket_id();

        mcast_hash = rte_fbk_hash_create(&mcast_hash_params);
        if (mcast_hash == NULL){
            return -1;
        }

        for (i = 0; i < N_MCAST_GROUPS; i ++){
            if (rte_fbk_hash_add_key(mcast_hash, mcast_group_table[i].ip, mcast_group_table[i].port_mask) < 0) {
		        return -1;
            }
        }
        return 0;
    }

Forwarding
~~~~~~~~~~

All forwarding is done inside the mcast_forward() function.
Firstly, the Ethernet* header is removed from the packet and the IPv4 address is extracted from the IPv4 header:

.. code-block:: c

    /* Remove the Ethernet header from the input packet */

    iphdr = (struct ipv4_hdr *)rte_pktmbuf_adj(m, sizeof(struct ether_hdr));
    RTE_MBUF_ASSERT(iphdr != NULL);
    dest_addr = rte_be_to_cpu_32(iphdr->dst_addr);

Then, the packet is checked to see if it has a multicast destination address and
if the routing table has any ports assigned to the destination address:

.. code-block:: c

    if (!IS_IPV4_MCAST(dest_addr) ||
       (hash = rte_fbk_hash_lookup(mcast_hash, dest_addr)) <= 0 ||
       (port_mask = hash & enabled_port_mask) == 0) {
           rte_pktmbuf_free(m);
           return;
    }

Then, the number of ports in the destination portmask is calculated with the help of the bitcnt() function:

.. code-block:: c

    /* Get number of bits set. */

    static inline uint32_t bitcnt(uint32_t v)
    {
        uint32_t n;

        for (n = 0; v != 0; v &= v - 1, n++)
           ;
        return (n);
    }

This is done to determine which forwarding algorithm to use.
This is explained in more detail in the next section.

Thereafter, a destination Ethernet address is constructed:

.. code-block:: c

    /* construct destination ethernet address */

    dst_eth_addr = ETHER_ADDR_FOR_IPV4_MCAST(dest_addr);

Since Ethernet addresses are also part of the multicast process, each outgoing packet carries the same destination Ethernet address.
The destination Ethernet address is constructed from the lower 23 bits of the multicast group ORed
with the Ethernet address 01:00:5e:00:00:00, as per RFC 1112:

.. code-block:: c

    #define ETHER_ADDR_FOR_IPV4_MCAST(x) \
        (rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)

Then, packets are dispatched to the destination ports according to the portmask associated with a multicast group:

.. code-block:: c

    for (port = 0; use_clone != port_mask; port_mask >>= 1, port++) {
        /* Prepare output packet and send it out. */

        if ((port_mask & 1) != 0) {
            if (likely ((mc = mcast_out_pkt(m, use_clone)) != NULL))
                mcast_send_pkt(mc, &dst_eth_addr.as_addr, qconf, port);
            else if (use_clone == 0)
                 rte_pktmbuf_free(m);
       }
    }

The actual packet transmission is done in the mcast_send_pkt() function:

.. code-block:: c

    static inline void mcast_send_pkt(struct rte_mbuf *pkt, struct ether_addr *dest_addr, struct lcore_queue_conf *qconf, uint8_t port)
    {
        struct ether_hdr *ethdr;
        uint16_t len;

        /* Construct Ethernet header. */

        ethdr = (struct ether_hdr *)rte_pktmbuf_prepend(pkt, (uint16_t) sizeof(*ethdr));

        RTE_MBUF_ASSERT(ethdr != NULL);

        ether_addr_copy(dest_addr, &ethdr->d_addr);
        ether_addr_copy(&ports_eth_addr[port], &ethdr->s_addr);
        ethdr->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);

        /* Put new packet into the output queue */

        len = qconf->tx_mbufs[port].len;
        qconf->tx_mbufs[port].m_table[len] = pkt;
        qconf->tx_mbufs[port].len = ++len;

        /* Transmit packets */

        if (unlikely(MAX_PKT_BURST == len))
            send_burst(qconf, port);
    }

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

.. code-block:: c

    use_clone = (port_num <= MCAST_CLONE_PORTS && m->pkt.nb_segs <= MCAST_CLONE_SEGS);

It is the mcast_out_pkt() function that performs the packet duplication (either with or without actually cloning the buffers):

.. code-block:: c

    static inline struct rte_mbuf *mcast_out_pkt(struct rte_mbuf *pkt, int use_clone)
    {
        struct rte_mbuf *hdr;

        /* Create new mbuf for the header. */

        if (unlikely ((hdr = rte_pktmbuf_alloc(header_pool)) == NULL))
            return (NULL);

        /* If requested, then make a new clone packet. */

        if (use_clone != 0 && unlikely ((pkt = rte_pktmbuf_clone(pkt, clone_pool)) == NULL)) {
            rte_pktmbuf_free(hdr);
            return (NULL);
        }

        /* prepend new header */

        hdr->pkt.next = pkt;

        /* update header's fields */

        hdr->pkt.pkt_len = (uint16_t)(hdr->pkt.data_len + pkt->pkt.pkt_len);
        hdr->pkt.nb_segs = (uint8_t)(pkt->pkt.nb_segs + 1);

        /* copy metadata from source packet */

        hdr->pkt.in_port = pkt->pkt.in_port;
        hdr->pkt.vlan_macip = pkt->pkt.vlan_macip;
        hdr->pkt.hash = pkt->pkt.hash;
        hdr->ol_flags = pkt->ol_flags;
        rte_mbuf_sanity_check(hdr, RTE_MBUF_PKT, 1);

        return (hdr);
    }
