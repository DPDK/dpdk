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

L3 Forwarding Sample Application
================================

The L3 Forwarding application is a simple example of packet processing using the DPDK.
The application performs L3 forwarding.

Overview
--------

The application demonstrates the use of the hash and LPM libraries in the DPDK to implement packet forwarding.
The initialization and run-time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
The main difference from the L2 Forwarding sample application is that the forwarding decision
is made based on information read from the input packet.

The lookup method is either hash-based or LPM-based and is selected at run time. When the selected lookup method is hash-based,
a hash object is used to emulate the flow classification stage.
The hash object is used in correlation with a flow table to map each input packet to its flow at runtime.

The hash lookup key is represented by a DiffServ 5-tuple composed of the following fields read from the input packet:
Source IP Address, Destination IP Address, Protocol, Source Port and Destination Port.
The ID of the output interface for the input packet is read from the identified flow table entry.
The set of flows used by the application is statically configured and loaded into the hash at initialization time.
When the selected lookup method is LPM based, an LPM object is used to emulate the forwarding stage for IPv4 packets.
The LPM object is used as the routing table to identify the next hop for each input packet at runtime.

The LPM lookup key is represented by the Destination IP Address field read from the input packet.
The ID of the output interface for the input packet is the next hop returned by the LPM lookup.
The set of LPM rules used by the application is statically configured and loaded into the LPM object at initialization time.

In the sample application, hash-based forwarding supports IPv4 and IPv6. LPM-based forwarding supports IPv4 only.

Compiling the Application
-------------------------

To compile the application:

#.  Go to the sample application directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/l3fwd

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

The application has a number of command line options::

    ./l3fwd [EAL options] -- -p PORTMASK
                             [-P]
                             [-E]
                             [-L]
                             --config(port,queue,lcore)[,(port,queue,lcore)]
                             [--eth-dest=X,MM:MM:MM:MM:MM:MM]
                             [--enable-jumbo [--max-pkt-len PKTLEN]]
                             [--no-numa]
                             [--hash-entry-num]
                             [--ipv6]
                             [--parse-ptype]

Where,

* ``-p PORTMASK:`` Hexadecimal bitmask of ports to configure

* ``-P:`` Optional, sets all ports to promiscuous mode so that packets are accepted regardless of the packet's Ethernet MAC destination address.
  Without this option, only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.

* ``-E:`` Optional, enable exact match.

* ``-L:`` Optional, enable longest prefix match.

* ``--config (port,queue,lcore)[,(port,queue,lcore)]:`` Determines which queues from which ports are mapped to which cores.

* ``--eth-dest=X,MM:MM:MM:MM:MM:MM:`` Optional, ethernet destination for port X.

* ``--enable-jumbo:`` Optional, enables jumbo frames.

* ``--max-pkt-len:`` Optional, under the premise of enabling jumbo, maximum packet length in decimal (64-9600).

* ``--no-numa:`` Optional, disables numa awareness.

* ``--hash-entry-num:`` Optional, specifies the hash entry number in hexadecimal to be setup.

* ``--ipv6:`` Optional, set if running ipv6 packets.

* ``--parse-ptype:`` Optional, set to use software to analyze packet type. Without this option, hardware will check the packet type.

For example, consider a dual processor socket platform where cores 0-7 and 16-23 appear on socket 0, while cores 8-15 and 24-31 appear on socket 1.
Let's say that the programmer wants to use memory from both NUMA nodes, the platform has only two ports, one connected to each NUMA node,
and the programmer wants to use two cores from each processor socket to do the packet processing.

To enable L3 forwarding between two ports, using two cores, cores 1 and 2, from each processor,
while also taking advantage of local memory access by optimizing around NUMA, the programmer must enable two queues from each port,
pin to the appropriate cores and allocate memory from the appropriate NUMA node. This is achieved using the following command:

.. code-block:: console

    ./build/l3fwd -c 606 -n 4 -- -p 0x3 --config="(0,0,1),(0,1,2),(1,0,9),(1,1,10)"

In this command:

*   The -c option enables cores 0, 1, 2, 3

*   The -p option enables ports 0 and 1

*   The --config option enables two queues on each port and maps each (port,queue) pair to a specific core.
    Logic to enable multiple RX queues using RSS and to allocate memory from the correct NUMA nodes
    is included in the application and is done transparently.
    The following table shows the mapping in this example:

+----------+-----------+-----------+-------------------------------------+
| **Port** | **Queue** | **lcore** | **Description**                     |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 0        | 0         | 0         | Map queue 0 from port 0 to lcore 0. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 0        | 1         | 2         | Map queue 1 from port 0 to lcore 2. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 1        | 0         | 1         | Map queue 0 from port 1 to lcore 1. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 1        | 1         | 3         | Map queue 1 from port 1 to lcore 3. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

.. _l3_fwd_explanation:

Explanation
-----------

The following sections provide some explanation of the sample application code. As mentioned in the overview section,
the initialization and run-time paths are very similar to those of the :doc:`l2_forward_real_virtual`.
The following sections describe aspects that are specific to the L3 Forwarding sample application.

Hash Initialization
~~~~~~~~~~~~~~~~~~~

The hash object is created and loaded with the pre-configured entries read from a global array,
and then generate the expected 5-tuple as key to keep consistence with those of real flow
for the convenience to execute hash performance test on 4M/8M/16M flows.

.. note::

    The Hash initialization will setup both ipv4 and ipv6 hash table,
    and populate the either table depending on the value of variable ipv6.
    To support the hash performance test with up to 8M single direction flows/16M bi-direction flows,
    populate_ipv4_many_flow_into_table() function will populate the hash table with specified hash table entry number(default 4M).

.. note::

    Value of global variable ipv6 can be specified with --ipv6 in the command line.
    Value of global variable hash_entry_number,
    which is used to specify the total hash entry number for all used ports in hash performance test,
    can be specified with --hash-entry-num VALUE in command line, being its default value 4.

.. code-block:: c

    #if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)

        static void
        setup_hash(int socketid)
        {
            // ...

            if (hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
                if (ipv6 == 0) {
                    /* populate the ipv4 hash */
                    populate_ipv4_many_flow_into_table(ipv4_l3fwd_lookup_struct[socketid], hash_entry_number);
                } else {
                    /* populate the ipv6 hash */
                    populate_ipv6_many_flow_into_table( ipv6_l3fwd_lookup_struct[socketid], hash_entry_number);
                }
            } else
                if (ipv6 == 0) {
                    /* populate the ipv4 hash */
                    populate_ipv4_few_flow_into_table(ipv4_l3fwd_lookup_struct[socketid]);
                } else {
                    /* populate the ipv6 hash */
                    populate_ipv6_few_flow_into_table(ipv6_l3fwd_lookup_struct[socketid]);
                }
            }
        }
    #endif

LPM Initialization
~~~~~~~~~~~~~~~~~~

The LPM object is created and loaded with the pre-configured entries read from a global array.

.. code-block:: c

    #if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)

    static void
    setup_lpm(int socketid)
    {
        unsigned i;
        int ret;
        char s[64];

        /* create the LPM table */

        snprintf(s, sizeof(s), "IPV4_L3FWD_LPM_%d", socketid);

        ipv4_l3fwd_lookup_struct[socketid] = rte_lpm_create(s, socketid, IPV4_L3FWD_LPM_MAX_RULES, 0);

        if (ipv4_l3fwd_lookup_struct[socketid] == NULL)
            rte_exit(EXIT_FAILURE, "Unable to create the l3fwd LPM table"
                " on socket %d\n", socketid);

        /* populate the LPM table */

        for (i = 0; i < IPV4_L3FWD_NUM_ROUTES; i++) {
            /* skip unused ports */

            if ((1 << ipv4_l3fwd_route_array[i].if_out & enabled_port_mask) == 0)
                continue;

            ret = rte_lpm_add(ipv4_l3fwd_lookup_struct[socketid], ipv4_l3fwd_route_array[i].ip,
           	                    ipv4_l3fwd_route_array[i].depth, ipv4_l3fwd_route_array[i].if_out);

            if (ret < 0) {
                rte_exit(EXIT_FAILURE, "Unable to add entry %u to the "
                        "l3fwd LPM table on socket %d\n", i, socketid);
            }

            printf("LPM: Adding route 0x%08x / %d (%d)\n",
                (unsigned)ipv4_l3fwd_route_array[i].ip, ipv4_l3fwd_route_array[i].depth, ipv4_l3fwd_route_array[i].if_out);
        }
    }
    #endif

Packet Forwarding for Hash-based Lookups
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each input packet, the packet forwarding operation is done by the l3fwd_simple_forward()
or simple_ipv4_fwd_4pkts() function for IPv4 packets or the simple_ipv6_fwd_4pkts() function for IPv6 packets.
The l3fwd_simple_forward() function provides the basic functionality for both IPv4 and IPv6 packet forwarding
for any number of burst packets received,
and the packet forwarding decision (that is, the identification of the output interface for the packet)
for hash-based lookups is done by the  get_ipv4_dst_port() or get_ipv6_dst_port() function.
The get_ipv4_dst_port() function is shown below:

.. code-block:: c

    static inline uint8_t
    get_ipv4_dst_port(void *ipv4_hdr, uint8_t portid, lookup_struct_t *ipv4_l3fwd_lookup_struct)
    {
        int ret = 0;
        union ipv4_5tuple_host key;

        ipv4_hdr = (uint8_t *)ipv4_hdr + offsetof(struct ipv4_hdr, time_to_live);

        m128i data = _mm_loadu_si128(( m128i*)(ipv4_hdr));

        /* Get 5 tuple: dst port, src port, dst IP address, src IP address and protocol */

        key.xmm = _mm_and_si128(data, mask0);

        /* Find destination port */

        ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);

        return (uint8_t)((ret < 0)? portid : ipv4_l3fwd_out_if[ret]);
    }

The get_ipv6_dst_port() function is similar to the get_ipv4_dst_port() function.

The simple_ipv4_fwd_4pkts() and simple_ipv6_fwd_4pkts() function are optimized for continuous 4 valid ipv4 and ipv6 packets,
they leverage the multiple buffer optimization to boost the performance of forwarding packets with the exact match on hash table.
The key code snippet of simple_ipv4_fwd_4pkts() is shown below:

.. code-block:: c

    static inline void
    simple_ipv4_fwd_4pkts(struct rte_mbuf* m[4], uint8_t portid, struct lcore_conf *qconf)
    {
        // ...

        data[0] = _mm_loadu_si128(( m128i*)(rte_pktmbuf_mtod(m[0], unsigned char *) + sizeof(struct ether_hdr) + offsetof(struct ipv4_hdr, time_to_live)));
        data[1] = _mm_loadu_si128(( m128i*)(rte_pktmbuf_mtod(m[1], unsigned char *) + sizeof(struct ether_hdr) + offsetof(struct ipv4_hdr, time_to_live)));
        data[2] = _mm_loadu_si128(( m128i*)(rte_pktmbuf_mtod(m[2], unsigned char *) + sizeof(struct ether_hdr) + offsetof(struct ipv4_hdr, time_to_live)));
        data[3] = _mm_loadu_si128(( m128i*)(rte_pktmbuf_mtod(m[3], unsigned char *) + sizeof(struct ether_hdr) + offsetof(struct ipv4_hdr, time_to_live)));

        key[0].xmm = _mm_and_si128(data[0], mask0);
        key[1].xmm = _mm_and_si128(data[1], mask0);
        key[2].xmm = _mm_and_si128(data[2], mask0);
        key[3].xmm = _mm_and_si128(data[3], mask0);

        const void *key_array[4] = {&key[0], &key[1], &key[2],&key[3]};

        rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0], 4, ret);

        dst_port[0] = (ret[0] < 0)? portid:ipv4_l3fwd_out_if[ret[0]];
        dst_port[1] = (ret[1] < 0)? portid:ipv4_l3fwd_out_if[ret[1]];
        dst_port[2] = (ret[2] < 0)? portid:ipv4_l3fwd_out_if[ret[2]];
        dst_port[3] = (ret[3] < 0)? portid:ipv4_l3fwd_out_if[ret[3]];

        // ...
    }

The simple_ipv6_fwd_4pkts() function is similar to the simple_ipv4_fwd_4pkts() function.

Known issue: IP packets with extensions or IP packets which are not TCP/UDP cannot work well at this mode.

Packet Forwarding for LPM-based Lookups
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each input packet, the packet forwarding operation is done by the l3fwd_simple_forward() function,
but the packet forwarding decision (that is, the identification of the output interface for the packet)
for LPM-based lookups is done by the get_ipv4_dst_port() function below:

.. code-block:: c

    static inline uint8_t
    get_ipv4_dst_port(struct ipv4_hdr *ipv4_hdr, uint8_t portid, lookup_struct_t *ipv4_l3fwd_lookup_struct)
    {
        uint8_t next_hop;

        return (uint8_t) ((rte_lpm_lookup(ipv4_l3fwd_lookup_struct, rte_be_to_cpu_32(ipv4_hdr->dst_addr), &next_hop) == 0)? next_hop : portid);
    }
