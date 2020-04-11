..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2020 Marvell International Ltd.

L3 Forwarding Graph Sample Application
======================================

The L3 Forwarding Graph application is a simple example of packet processing
using the DPDK Graph framework. The application performs L3 forwarding using
Graph framework and nodes written for graph framework.

Overview
--------

The application demonstrates the use of the graph framework and graph nodes
``ethdev_rx``, ``ip4_lookup``, ``ip4_rewrite``, ``ethdev_tx`` and ``pkt_drop`` in DPDK to
implement packet forwarding.

The initialization is very similar to those of the :doc:`l3_forward`.
There is also additional initialization of graph for graph object creation
and configuration per lcore.
Run-time path is main thing that differs from L3 forwarding sample application.
Difference is that forwarding logic starting from Rx, followed by LPM lookup,
TTL update and finally Tx is implemented inside graph nodes. These nodes are
interconnected in graph framework. Application main loop needs to walk over
graph using ``rte_graph_walk()`` with graph objects created one per slave lcore.

The lookup method is as per implementation of ``ip4_lookup`` graph node.
The ID of the output interface for the input packet is the next hop returned by
the LPM lookup. The set of LPM rules used by the application is statically
configured and provided to ``ip4_lookup`` graph node and ``ip4_rewrite`` graph node
using node control API ``rte_node_ip4_route_add()`` and ``rte_node_ip4_rewrite_add()``.

In the sample application, only IPv4 forwarding is supported as of now.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l3fwd-graph`` sub-directory.

Running the Application
-----------------------

The application has a number of command line options similar to l3fwd::

    ./l3fwd-graph [EAL options] -- -p PORTMASK
                                   [-P]
                                   --config(port,queue,lcore)[,(port,queue,lcore)]
                                   [--eth-dest=X,MM:MM:MM:MM:MM:MM]
                                   [--enable-jumbo [--max-pkt-len PKTLEN]]
                                   [--no-numa]
                                   [--per-port-pool]

Where,

* ``-p PORTMASK:`` Hexadecimal bitmask of ports to configure

* ``-P:`` Optional, sets all ports to promiscuous mode so that packets are accepted regardless of the packet's Ethernet MAC destination address.
  Without this option, only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.

* ``--config (port,queue,lcore)[,(port,queue,lcore)]:`` Determines which queues from which ports are mapped to which cores.

* ``--eth-dest=X,MM:MM:MM:MM:MM:MM:`` Optional, ethernet destination for port X.

* ``--enable-jumbo:`` Optional, enables jumbo frames.

* ``--max-pkt-len:`` Optional, under the premise of enabling jumbo, maximum packet length in decimal (64-9600).

* ``--no-numa:`` Optional, disables numa awareness.

* ``--per-port-pool:`` Optional, set to use independent buffer pools per port. Without this option, single buffer pool is used for all ports.

For example, consider a dual processor socket platform with 8 physical cores, where cores 0-7 and 16-23 appear on socket 0,
while cores 8-15 and 24-31 appear on socket 1.

To enable L3 forwarding between two ports, assuming that both ports are in the same socket, using two cores, cores 1 and 2,
(which are in the same socket too), use the following command:

.. code-block:: console

    ./build/l3fwd-graph -l 1,2 -n 4 -- -p 0x3 --config="(0,0,1),(1,0,2)"

In this command:

*   The -l option enables cores 1, 2

*   The -p option enables ports 0 and 1

*   The --config option enables one queue on each port and maps each (port,queue) pair to a specific core.
    The following table shows the mapping in this example:

+----------+-----------+-----------+-------------------------------------+
| **Port** | **Queue** | **lcore** | **Description**                     |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 0        | 0         | 1         | Map queue 0 from port 0 to lcore 1. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+
| 1        | 0         | 2         | Map queue 0 from port 1 to lcore 2. |
|          |           |           |                                     |
+----------+-----------+-----------+-------------------------------------+

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

.. _l3_fwd_graph_explanation:

Explanation
-----------

The following sections provide some explanation of the sample application code.
As mentioned in the overview section, the initialization is similar to that of
the :doc:`l3_forward`. Run-time path though similar in functionality to that of
:doc:`l3_forward`, major part of the implementation is in graph nodes via used
via ``librte_node`` library.
The following sections describe aspects that are specific to the L3 Forwarding
Graph sample application.

Graph Node Pre-Init Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After device configuration and device Rx, Tx queue setup is complete,
a minimal config of port id, num_rx_queues, num_tx_queues, mempools etc will
be passed to *ethdev_** node ctrl API ``rte_node_eth_config()``. This will be
lead to the clone of ``ethdev_rx`` and ``ethdev_tx`` nodes as ``ethdev_rx-X-Y`` and
``ethdev_tx-X`` where X, Y represent port id and queue id associated with them.
In case of ``ethdev_tx-X`` nodes, tx queue id assigned per instance of the node
is same as graph id.

These cloned nodes along with existing static nodes such as ``ip4_lookup`` and
``ip4_rewrite`` will be used in graph creation to associate node's to lcore
specific graph object.

.. code-block:: c

    RTE_ETH_FOREACH_DEV(portid)
    {

        /* ... */
        ret = rte_eth_dev_configure(portid, nb_rx_queue,
                                    n_tx_queue, &local_port_conf);
        /* ... */

        /* Init one TX queue per couple (lcore,port) */
        queueid = 0;
        for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
            /* ... */
            ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
                                         socketid, txconf);
            /* ... */
            queueid++;
        }

        /* Setup ethdev node config */
        ethdev_conf[nb_conf].port_id = portid;
        ethdev_conf[nb_conf].num_rx_queues = nb_rx_queue;
        ethdev_conf[nb_conf].num_tx_queues = n_tx_queue;
        if (!per_port_pool)
            ethdev_conf[nb_conf].mp = pktmbuf_pool[0];
        else
          ethdev_conf[nb_conf].mp = pktmbuf_pool[portid];
        ethdev_conf[nb_conf].mp_count = NB_SOCKETS;

        nb_conf++;
        printf("\n");
    }

    for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
        /* Init RX queues */
        for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
            /* ... */
            if (!per_port_pool)
                ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd, socketid,
                                             &rxq_conf, pktmbuf_pool[0][socketid]);
            else
              ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd, socketid,
                                           &rxq_conf, pktmbuf_pool[portid][socketid]);
            /* ... */
        }
    }

    /* Ethdev node config, skip rx queue mapping */
    ret = rte_node_eth_config(ethdev_conf, nb_conf, nb_graphs);

Graph Initialization
~~~~~~~~~~~~~~~~~~~~

Now a graph needs to be created with a specific set of nodes for every lcore.
A graph object returned after graph creation is a per lcore object and
cannot be shared between lcores. Since ``ethdev_tx-X`` node is per port node,
it can be associated with all the graphs created as all the lcores should have
Tx capability for every port. But ``ethdev_rx-X-Y`` node is created per
(port, rx_queue_id), so they should be associated with a graph based on
the application argument ``--config`` specifying rx queue mapping to lcore.

.. note::

    The Graph creation will fail if the passed set of shell node pattern's
    are not sufficient to meet their inter-dependency or even one node is not
    found with a given regex node pattern.

.. code-block:: c

    static const char *const default_patterns[] = {
        "ip4*",
        "ethdev_tx-*",
        "pkt_drop",
    };
    const char **node_patterns;
    uint16_t nb_pattern;

    /* ... */

    /* Create a graph object per lcore with common nodes and
     * lcore specific nodes based on application arguments
     */
    nb_patterns = RTE_DIM(default_patterns);
    node_patterns = malloc((MAX_RX_QUEUE_PER_LCORE + nb_patterns) *
                           sizeof(*node_patterns));
    memcpy(node_patterns, default_patterns,
           nb_patterns * sizeof(*node_patterns));

    memset(&graph_conf, 0, sizeof(graph_conf));

    /* Common set of nodes in every lcore's graph object */
    graph_conf.node_patterns = node_patterns;

    for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
        /* ... */

        /* Skip graph creation if no source exists */
        if (!qconf->n_rx_queue)
            continue;

        /* Add rx node patterns of this lcore based on --config */
        for (i = 0; i < qconf->n_rx_queue; i++) {
            graph_conf.node_patterns[nb_patterns + i] =
                                qconf->rx_queue_list[i].node_name;
        }

        graph_conf.nb_node_patterns = nb_patterns + i;
        graph_conf.socket_id = rte_lcore_to_socket_id(lcore_id);

        snprintf(qconf->name, sizeof(qconf->name), "worker_%u", lcore_id);

        graph_id = rte_graph_create(qconf->name, &graph_conf);

        /* ... */

        qconf->graph = rte_graph_lookup(qconf->name);

        /* ... */
    }

Forwarding data(Route, Next-Hop) addition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once graph objects are created, node specific info like routes and rewrite
headers will be provided run-time using ``rte_node_ip4_route_add()`` and
``rte_node_ip4_rewrite_add()`` API.

.. note::

    Since currently ``ip4_lookup`` and ``ip4_rewrite`` nodes don't support
    lock-less mechanisms(RCU, etc) to add run-time forwarding data like route and
    rewrite data, forwarding data is added before packet processing loop is
    launched on slave lcore.

.. code-block:: c

    /* Add route to ip4 graph infra */
    for (i = 0; i < IPV4_L3FWD_LPM_NUM_ROUTES; i++) {
        /* ... */

        dst_port = ipv4_l3fwd_lpm_route_array[i].if_out;
        next_hop = i;

        /* ... */
        ret = rte_node_ip4_route_add(ipv4_l3fwd_lpm_route_array[i].ip,
                                     ipv4_l3fwd_lpm_route_array[i].depth, next_hop,
                                     RTE_NODE_IP4_LOOKUP_NEXT_REWRITE);

        /* ... */

        memcpy(rewrite_data, val_eth + dst_port, rewrite_len);

        /* Add next hop for a given destination */
        ret = rte_node_ip4_rewrite_add(next_hop, rewrite_data,
                                       rewrite_len, dst_port);

        RTE_LOG(INFO, L3FWD_GRAPH, "Added route %s, next_hop %u\n",
                route_str, next_hop);
    }

Packet Forwarding using Graph Walk
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that all the device configurations are done, graph creations are done and
forwarding data is updated with nodes, slave lcores will be launched with graph
main loop. Graph main loop is very simple in the sense that it needs to
continuously call a non-blocking API ``rte_graph_walk()`` with it's lcore
specific graph object that was already created.

.. note::

    rte_graph_walk() will walk over all the sources nodes i.e ``ethdev_rx-X-Y``
    associated with a given graph and Rx the available packets and enqueue them
    to the following node ``ip4_lookup`` which then will enqueue them to ``ip4_rewrite``
    node if LPM lookup succeeds. ``ip4_rewrite`` node then will update Ethernet header
    as per next-hop data and transmit the packet via port 'Z' by enqueuing
    to ``ethdev_tx-Z`` node instance in its graph object.

.. code-block:: c

    /* Main processing loop */
    static int
    graph_main_loop(void *conf)
    {
        // ...

        lcore_id = rte_lcore_id();
        qconf = &lcore_conf[lcore_id];
        graph = qconf->graph;

        RTE_LOG(INFO, L3FWD_GRAPH,
                "Entering main loop on lcore %u, graph %s(%p)\n", lcore_id,
                qconf->name, graph);

        /* Walk over graph until signal to quit */
        while (likely(!force_quit))
            rte_graph_walk(graph);
        return 0;
    }
