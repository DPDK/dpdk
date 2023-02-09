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
graph using ``rte_graph_walk()`` with graph objects created one per worker lcore.

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

    ./dpdk-l3fwd-graph [EAL options] -- -p PORTMASK
                                   [-P]
                                   --config(port,queue,lcore)[,(port,queue,lcore)]
                                   [--eth-dest=X,MM:MM:MM:MM:MM:MM]
                                   [--max-pkt-len PKTLEN]
                                   [--no-numa]
                                   [--per-port-pool]
                                   [--pcap-enable]
                                   [--pcap-num-cap]
                                   [--pcap-file-name]

Where,

* ``-p PORTMASK:`` Hexadecimal bitmask of ports to configure

* ``-P:`` Optional, sets all ports to promiscuous mode so that packets are accepted regardless of the packet's Ethernet MAC destination address.
  Without this option, only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.

* ``--config (port,queue,lcore)[,(port,queue,lcore)]:`` Determines which queues from which ports are mapped to which cores.

* ``--eth-dest=X,MM:MM:MM:MM:MM:MM:`` Optional, ethernet destination for port X.

* ``--max-pkt-len:`` Optional, maximum packet length in decimal (64-9600).

* ``--no-numa:`` Optional, disables numa awareness.

* ``--per-port-pool:`` Optional, set to use independent buffer pools per port. Without this option, single buffer pool is used for all ports.

* ``--pcap-enable:`` Optional, Enables packet capture in pcap format on each node with mbuf and node metadata.

* ``--pcap-num-cap:`` Optional, Number of packets to be captured per core.

* ``--pcap-file-name:`` Optional, Pcap filename to capture packets in.

For example, consider a dual processor socket platform with 8 physical cores, where cores 0-7 and 16-23 appear on socket 0,
while cores 8-15 and 24-31 appear on socket 1.

To enable L3 forwarding between two ports, assuming that both ports are in the same socket, using two cores, cores 1 and 2,
(which are in the same socket too), use the following command:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l3fwd-graph -l 1,2 -n 4 -- -p 0x3 --config="(0,0,1),(1,0,2)"

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

To enable pcap trace on each graph, use following command:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l3fwd-graph -l 1,2 -n 4 -- -p 0x3 --config="(0,0,1),(1,0,2)" --pcap-enable --pcap-num-cap=<number of packets> --pcap-file-name "</path/to/file>"

In this command:

*   The --pcap-enable option enables pcap trace on graph nodes.

*   The --pcap-num-cap option enables user to configure number packets to be captured per graph. Default 1024 packets per graph are captured.

*   The --pcap-file-name option enables user to give filename in which packets are to be captured.

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

.. literalinclude:: ../../../examples/l3fwd-graph/main.c
    :language: c
    :start-after: Initialize all ports. 8<
    :end-before: >8 End of graph creation.
    :dedent: 1

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

.. literalinclude:: ../../../examples/l3fwd-graph/main.c
    :language: c
    :start-after: Graph initialization. 8<
    :end-before: >8 End of graph initialization.
    :dedent: 1

Forwarding data(Route, Next-Hop) addition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once graph objects are created, node specific info like routes and rewrite
headers will be provided run-time using ``rte_node_ip4_route_add()`` and
``rte_node_ip4_rewrite_add()`` API.

.. note::

    Since currently ``ip4_lookup`` and ``ip4_rewrite`` nodes don't support
    lock-less mechanisms(RCU, etc) to add run-time forwarding data like route and
    rewrite data, forwarding data is added before packet processing loop is
    launched on worker lcore.

.. literalinclude:: ../../../examples/l3fwd-graph/main.c
    :language: c
    :start-after: Add route to ip4 graph infra. 8<
    :end-before: >8 End of adding route to ip4 graph infa.
    :dedent: 1

Packet Forwarding using Graph Walk
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that all the device configurations are done, graph creations are done and
forwarding data is updated with nodes, worker lcores will be launched with graph
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

.. literalinclude:: ../../../examples/l3fwd-graph/main.c
    :language: c
    :start-after: Main processing loop. 8<
    :end-before: >8 End of main processing loop.
