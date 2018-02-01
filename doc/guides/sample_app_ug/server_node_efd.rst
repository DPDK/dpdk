..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2017 Intel Corporation.

Server-Node EFD Sample Application
==================================

This sample application demonstrates the use of EFD library as a flow-level
load balancer, for more information about the EFD Library please refer to the
DPDK programmer's guide.

This sample application is a variant of the
:ref:`client-server sample application <multi_process_app>`
where a specific target node is specified for every and each flow
(not in a round-robin fashion as the original load balancing sample application).

Overview
--------

The architecture of the EFD flow-based load balancer sample application is
presented in the following figure.

.. _figure_efd_sample_app_overview:

.. figure:: img/server_node_efd.*

   Using EFD as a Flow-Level Load Balancer

As shown in :numref:`figure_efd_sample_app_overview`,
the sample application consists of a front-end node (server)
using the EFD library to create a load-balancing table for flows,
for each flow a target backend worker node is specified. The EFD table does not
store the flow key (unlike a regular hash table), and hence, it can
individually load-balance millions of flows (number of targets * maximum number
of flows fit in a flow table per target) while still fitting in CPU cache.

It should be noted that although they are referred to as nodes, the frontend
server and worker nodes are processes running on the same platform.

Front-end Server
~~~~~~~~~~~~~~~~

Upon initializing, the frontend server node (process) creates a flow
distributor table (based on the EFD library) which is populated with flow
information and its intended target node.

The sample application assigns a specific target node_id (process) for each of
the IP destination addresses as follows:

.. code-block:: c

    node_id = i % num_nodes; /* Target node id is generated */
    ip_dst = rte_cpu_to_be_32(i); /* Specific ip destination address is
                                     assigned to this target node */

then the pair of <key,target> is inserted into the flow distribution table.

The main loop of the server process receives a burst of packets, then for
each packet, a flow key (IP destination address) is extracted. The flow
distributor table is looked up and the target node id is returned.  Packets are
then enqueued to the specified target node id.

It should be noted that flow distributor table is not a membership test table.
I.e. if the key has already been inserted the target node id will be correct,
but for new keys the flow distributor table will return a value (which can be
valid).

Backend Worker Nodes
~~~~~~~~~~~~~~~~~~~~

Upon initializing, the worker node (process) creates a flow table (a regular
hash table that stores the key default size 1M flows) which is populated with
only the flow information that is serviced at this node. This flow key is
essential to point out new keys that have not been inserted before.

The worker node's main loop is simply receiving packets then doing a hash table
lookup. If a match occurs then statistics are updated for flows serviced by
this node. If no match is found in the local hash table then this indicates
that this is a new flow, which is dropped.


Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``server_node_efd`` sub-directory.

Running the Application
-----------------------

The application has two binaries to be run: the front-end server
and the back-end node.

The frontend server (server) has the following command line options::

    ./server [EAL options] -- -p PORTMASK -n NUM_NODES -f NUM_FLOWS

Where,

* ``-p PORTMASK:`` Hexadecimal bitmask of ports to configure
* ``-n NUM_NODES:`` Number of back-end nodes that will be used
* ``-f NUM_FLOWS:`` Number of flows to be added in the EFD table (1 million, by default)

The back-end node (node) has the following command line options::

    ./node [EAL options] -- -n NODE_ID

Where,

* ``-n NODE_ID:`` Node ID, which cannot be equal or higher than NUM_MODES


First, the server app must be launched, with the number of nodes that will be run.
Once it has been started, the node instances can be run, with different NODE_ID.
These instances have to be run as secondary processes, with ``--proc-type=secondary``
in the EAL options, which will attach to the primary process memory, and therefore,
they can access the queues created by the primary process to distribute packets.

To successfully run the application, the command line used to start the
application has to be in sync with the traffic flows configured on the traffic
generator side.

For examples of application command lines and traffic generator flows, please
refer to the DPDK Test Report. For more details on how to set up and run the
sample applications provided with DPDK package, please refer to the
:ref:`DPDK Getting Started Guide for Linux <linux_gsg>` and
:ref:`DPDK Getting Started Guide for FreeBSD <freebsd_gsg>`.


Explanation
-----------

As described in previous sections, there are two processes in this example.

The first process, the front-end server, creates and populates the EFD table,
which is used to distribute packets to nodes, which the number of flows
specified in the command line (1 million, by default).


.. code-block:: c

    static void
    create_efd_table(void)
    {
        uint8_t socket_id = rte_socket_id();

        /* create table */
        efd_table = rte_efd_create("flow table", num_flows * 2, sizeof(uint32_t),
                        1 << socket_id, socket_id);

        if (efd_table == NULL)
            rte_exit(EXIT_FAILURE, "Problem creating the flow table\n");
    }

    static void
    populate_efd_table(void)
    {
        unsigned int i;
        int32_t ret;
        uint32_t ip_dst;
        uint8_t socket_id = rte_socket_id();
        uint64_t node_id;

        /* Add flows in table */
        for (i = 0; i < num_flows; i++) {
            node_id = i % num_nodes;

            ip_dst = rte_cpu_to_be_32(i);
            ret = rte_efd_update(efd_table, socket_id,
                            (void *)&ip_dst, (efd_value_t)node_id);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Unable to add entry %u in "
                                    "EFD table\n", i);
        }

        printf("EFD table: Adding 0x%x keys\n", num_flows);
    }

After initialization, packets are received from the enabled ports, and the IPv4
address from the packets is used as a key to look up in the EFD table,
which tells the node where the packet has to be distributed.

.. code-block:: c

    static void
    process_packets(uint32_t port_num __rte_unused, struct rte_mbuf *pkts[],
            uint16_t rx_count, unsigned int socket_id)
    {
        uint16_t i;
        uint8_t node;
        efd_value_t data[EFD_BURST_MAX];
        const void *key_ptrs[EFD_BURST_MAX];

        struct ipv4_hdr *ipv4_hdr;
        uint32_t ipv4_dst_ip[EFD_BURST_MAX];

        for (i = 0; i < rx_count; i++) {
            /* Handle IPv4 header.*/
            ipv4_hdr = rte_pktmbuf_mtod_offset(pkts[i], struct ipv4_hdr *,
                    sizeof(struct ether_hdr));
            ipv4_dst_ip[i] = ipv4_hdr->dst_addr;
            key_ptrs[i] = (void *)&ipv4_dst_ip[i];
        }

        rte_efd_lookup_bulk(efd_table, socket_id, rx_count,
                    (const void **) key_ptrs, data);
        for (i = 0; i < rx_count; i++) {
            node = (uint8_t) ((uintptr_t)data[i]);

            if (node >= num_nodes) {
                /*
                 * Node is out of range, which means that
                 * flow has not been inserted
                 */
                flow_dist_stats.drop++;
                rte_pktmbuf_free(pkts[i]);
            } else {
                flow_dist_stats.distributed++;
                enqueue_rx_packet(node, pkts[i]);
            }
        }

        for (i = 0; i < num_nodes; i++)
            flush_rx_queue(i);
    }

The burst of packets received is enqueued in temporary buffers (per node),
and enqueued in the shared ring between the server and the node.
After this, a new burst of packets is received and this process is
repeated infinitely.

.. code-block:: c

    static void
    flush_rx_queue(uint16_t node)
    {
        uint16_t j;
        struct node *cl;

        if (cl_rx_buf[node].count == 0)
            return;

        cl = &nodes[node];
        if (rte_ring_enqueue_bulk(cl->rx_q, (void **)cl_rx_buf[node].buffer,
                cl_rx_buf[node].count, NULL) != cl_rx_buf[node].count){
            for (j = 0; j < cl_rx_buf[node].count; j++)
                rte_pktmbuf_free(cl_rx_buf[node].buffer[j]);
            cl->stats.rx_drop += cl_rx_buf[node].count;
        } else
            cl->stats.rx += cl_rx_buf[node].count;

        cl_rx_buf[node].count = 0;
    }

The second process, the back-end node, receives the packets from the shared
ring with the server and send them out, if they belong to the node.

At initialization, it attaches to the server process memory, to have
access to the shared ring, parameters and statistics.

.. code-block:: c

    rx_ring = rte_ring_lookup(get_rx_queue_name(node_id));
    if (rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot get RX ring - "
                "is server process running?\n");

    mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
    if (mp == NULL)
        rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

    mz = rte_memzone_lookup(MZ_SHARED_INFO);
    if (mz == NULL)
        rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
    info = mz->addr;
    tx_stats = &(info->tx_stats[node_id]);
    filter_stats = &(info->filter_stats[node_id]);

Then, the hash table that contains the flows that will be handled
by the node is created and populated.

.. code-block:: c

    static struct rte_hash *
    create_hash_table(const struct shared_info *info)
    {
        uint32_t num_flows_node = info->num_flows / info->num_nodes;
        char name[RTE_HASH_NAMESIZE];
        struct rte_hash *h;

        /* create table */
        struct rte_hash_parameters hash_params = {
            .entries = num_flows_node * 2, /* table load = 50% */
            .key_len = sizeof(uint32_t), /* Store IPv4 dest IP address */
            .socket_id = rte_socket_id(),
            .hash_func_init_val = 0,
        };

        snprintf(name, sizeof(name), "hash_table_%d", node_id);
        hash_params.name = name;
        h = rte_hash_create(&hash_params);

        if (h == NULL)
            rte_exit(EXIT_FAILURE,
                    "Problem creating the hash table for node %d\n",
                    node_id);
        return h;
    }

    static void
    populate_hash_table(const struct rte_hash *h, const struct shared_info *info)
    {
        unsigned int i;
        int32_t ret;
        uint32_t ip_dst;
        uint32_t num_flows_node = 0;
        uint64_t target_node;

        /* Add flows in table */
        for (i = 0; i < info->num_flows; i++) {
            target_node = i % info->num_nodes;
            if (target_node != node_id)
                continue;

            ip_dst = rte_cpu_to_be_32(i);

            ret = rte_hash_add_key(h, (void *) &ip_dst);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Unable to add entry %u "
                        "in hash table\n", i);
            else
                num_flows_node++;

        }

        printf("Hash table: Adding 0x%x keys\n", num_flows_node);
    }

After initialization, packets are dequeued from the shared ring
(from the server) and, like in the server process,
the IPv4 address from the packets is used as a key to look up in the hash table.
If there is a hit, packet is stored in a buffer, to be eventually transmitted
in one of the enabled ports. If key is not there, packet is dropped, since the
flow is not handled by the node.

.. code-block:: c

    static inline void
    handle_packets(struct rte_hash *h, struct rte_mbuf **bufs, uint16_t num_packets)
    {
        struct ipv4_hdr *ipv4_hdr;
        uint32_t ipv4_dst_ip[PKT_READ_SIZE];
        const void *key_ptrs[PKT_READ_SIZE];
        unsigned int i;
        int32_t positions[PKT_READ_SIZE] = {0};

        for (i = 0; i < num_packets; i++) {
            /* Handle IPv4 header.*/
            ipv4_hdr = rte_pktmbuf_mtod_offset(bufs[i], struct ipv4_hdr *,
                    sizeof(struct ether_hdr));
            ipv4_dst_ip[i] = ipv4_hdr->dst_addr;
            key_ptrs[i] = &ipv4_dst_ip[i];
        }
        /* Check if packets belongs to any flows handled by this node */
        rte_hash_lookup_bulk(h, key_ptrs, num_packets, positions);

        for (i = 0; i < num_packets; i++) {
            if (likely(positions[i] >= 0)) {
                filter_stats->passed++;
                transmit_packet(bufs[i]);
            } else {
                filter_stats->drop++;
                /* Drop packet, as flow is not handled by this node */
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }

Finally, note that both processes updates statistics, such as transmitted, received
and dropped packets, which are shown and refreshed by the server app.

.. code-block:: c

    static void
    do_stats_display(void)
    {
        unsigned int i, j;
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        uint64_t port_tx[RTE_MAX_ETHPORTS], port_tx_drop[RTE_MAX_ETHPORTS];
        uint64_t node_tx[MAX_NODES], node_tx_drop[MAX_NODES];

        /* to get TX stats, we need to do some summing calculations */
        memset(port_tx, 0, sizeof(port_tx));
        memset(port_tx_drop, 0, sizeof(port_tx_drop));
        memset(node_tx, 0, sizeof(node_tx));
        memset(node_tx_drop, 0, sizeof(node_tx_drop));

        for (i = 0; i < num_nodes; i++) {
            const struct tx_stats *tx = &info->tx_stats[i];

            for (j = 0; j < info->num_ports; j++) {
                const uint64_t tx_val = tx->tx[info->id[j]];
                const uint64_t drop_val = tx->tx_drop[info->id[j]];

                port_tx[j] += tx_val;
                port_tx_drop[j] += drop_val;
                node_tx[i] += tx_val;
                node_tx_drop[i] += drop_val;
            }
        }

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PORTS\n");
        printf("-----\n");
        for (i = 0; i < info->num_ports; i++)
            printf("Port %u: '%s'\t", (unsigned int)info->id[i],
                    get_printable_mac_addr(info->id[i]));
        printf("\n\n");
        for (i = 0; i < info->num_ports; i++) {
            printf("Port %u - rx: %9"PRIu64"\t"
                    "tx: %9"PRIu64"\n",
                    (unsigned int)info->id[i], info->rx_stats.rx[i],
                    port_tx[i]);
        }

        printf("\nSERVER\n");
        printf("-----\n");
        printf("distributed: %9"PRIu64", drop: %9"PRIu64"\n",
                flow_dist_stats.distributed, flow_dist_stats.drop);

        printf("\nNODES\n");
        printf("-------\n");
        for (i = 0; i < num_nodes; i++) {
            const unsigned long long rx = nodes[i].stats.rx;
            const unsigned long long rx_drop = nodes[i].stats.rx_drop;
            const struct filter_stats *filter = &info->filter_stats[i];

            printf("Node %2u - rx: %9llu, rx_drop: %9llu\n"
                    "            tx: %9"PRIu64", tx_drop: %9"PRIu64"\n"
                    "            filter_passed: %9"PRIu64", "
                    "filter_drop: %9"PRIu64"\n",
                    i, rx, rx_drop, node_tx[i], node_tx_drop[i],
                    filter->passed, filter->drop);
        }

        printf("\n");
    }
