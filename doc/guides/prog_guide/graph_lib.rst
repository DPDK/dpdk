..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2020 Marvell International Ltd.

Graph Library and Inbuilt Nodes
===============================

Graph architecture abstracts the data processing functions as a ``node`` and
``links`` them together to create a complex ``graph`` to enable reusable/modular
data processing functions.

The graph library provides API to enable graph framework operations such as
create, lookup, dump and destroy on graph and node operations such as clone,
edge update, and edge shrink, etc. The API also allows to create the stats
cluster to monitor per graph and per node stats.

Features
--------

Features of the Graph library are:

- Nodes as plugins.
- Support for out of tree nodes.
- Inbuilt nodes for packet processing.
- Multi-process support.
- Low overhead graph walk and node enqueue.
- Low overhead statistics collection infrastructure.
- Support to export the graph as a Graphviz dot file. See ``rte_graph_export()``.
- Allow having another graph walk implementation in the future by segregating
  the fast path(``rte_graph_worker.h``) and slow path code.

Advantages of Graph architecture
--------------------------------

- Memory latency is the enemy for high-speed packet processing, moving the
  similar packet processing code to a node will reduce the I cache and D
  caches misses.
- Exploits the probability that most packets will follow the same nodes in the
  graph.
- Allow SIMD instructions for packet processing of the node.-
- The modular scheme allows having reusable nodes for the consumers.
- The modular scheme allows us to abstract the vendor HW specific
  optimizations as a node.

Performance tuning parameters
-----------------------------

- Test with various burst size values (256, 128, 64, 32) using
  RTE_GRAPH_BURST_SIZE config option.
  The testing shows, on x86 and arm64 servers, The sweet spot is 256 burst
  size. While on arm64 embedded SoCs, it is either 64 or 128.
- Disable node statistics (using ``RTE_LIBRTE_GRAPH_STATS`` config option)
  if not needed.

Programming model
-----------------

Anatomy of Node:
~~~~~~~~~~~~~~~~

.. _figure_anatomy_of_a_node:

.. figure:: img/anatomy_of_a_node.*

   Anatomy of a node

The node is the basic building block of the graph framework.

A node consists of:

process():
^^^^^^^^^^

The callback function will be invoked by worker thread using
``rte_graph_walk()`` function when there is data to be processed by the node.
A graph node process the function using ``process()`` and enqueue to next
downstream node using ``rte_node_enqueue*()`` function.

Context memory:
^^^^^^^^^^^^^^^

It is memory allocated by the library to store the node-specific context
information. This memory will be used by process(), init(), fini() callbacks.

init():
^^^^^^^

The callback function will be invoked by ``rte_graph_create()`` on when
a node gets attached to a graph.

fini():
^^^^^^^

The callback function will be invoked by ``rte_graph_destroy()`` on when a
node gets detached to a graph.

Node name:
^^^^^^^^^^

It is the name of the node. When a node registers to graph library, the library
gives the ID as ``rte_node_t`` type. Both ID or Name shall be used lookup the
node. ``rte_node_from_name()``, ``rte_node_id_to_name()`` are the node
lookup functions.

nb_edges:
^^^^^^^^^

The number of downstream nodes connected to this node. The ``next_nodes[]``
stores the downstream nodes objects. ``rte_node_edge_update()`` and
``rte_node_edge_shrink()`` functions shall be used to update the ``next_node[]``
objects. Consumers of the node APIs are free to update the ``next_node[]``
objects till ``rte_graph_create()`` invoked.

next_node[]:
^^^^^^^^^^^^

The dynamic array to store the downstream nodes connected to this node. Downstream
node should not be current node itself or a source node.

Source node:
^^^^^^^^^^^^

Source nodes are static nodes created using ``RTE_NODE_REGISTER`` by passing
``flags`` as ``RTE_NODE_SOURCE_F``.
While performing the graph walk, the ``process()`` function of all the source
nodes will be called first. So that these nodes can be used as input nodes for a graph.

Node creation and registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Node implementer creates the node by implementing ops and attributes of
  ``struct rte_node_register``.

* The library registers the node by invoking RTE_NODE_REGISTER on library load
  using the constructor scheme. The constructor scheme used here to support multi-process.

Link the Nodes to create the graph topology
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _figure_link_the_nodes:

.. figure:: img/link_the_nodes.*

   Topology after linking the nodes

Once nodes are available to the program, Application or node public API
functions can links them together to create a complex packet processing graph.

There are multiple different types of strategies to link the nodes.

Method (a):
^^^^^^^^^^^
Provide the ``next_nodes[]`` at the node registration time. See  ``struct rte_node_register::nb_edges``.
This is a use case to address the static node scheme where one knows upfront the
``next_nodes[]`` of the node.

Method (b):
^^^^^^^^^^^
Use ``rte_node_edge_get()``, ``rte_node_edge_update()``, ``rte_node_edge_shrink()``
to update the ``next_nodes[]`` links for the node runtime but before graph create.

Method (c):
^^^^^^^^^^^
Use ``rte_node_clone()`` to clone a already existing node, created using RTE_NODE_REGISTER.
When ``rte_node_clone()`` invoked, The library, would clone all the attributes
of the node and creates a new one. The name for cloned node shall be
``"parent_node_name-user_provided_name"``.

This method enables the use case of Rx and Tx nodes where multiple of those nodes
need to be cloned based on the number of CPU available in the system.
The cloned nodes will be identical, except the ``"context memory"``.
Context memory will have information of port, queue pair in case of Rx and Tx
ethdev nodes.

Create the graph object
~~~~~~~~~~~~~~~~~~~~~~~
Now that the nodes are linked, Its time to create a graph by including
the required nodes. The application can provide a set of node patterns to
form a graph object. The ``famish()`` API used underneath for the pattern
matching to include the required nodes. After the graph create any changes to
nodes or graph is not allowed.

The ``rte_graph_create()`` API shall be used to create the graph.

Example of a graph object creation:

.. code-block:: console

   {"ethdev_rx-0-0", ip4*, ethdev_tx-*"}

In the above example, A graph object will be created with ethdev Rx
node of port 0 and queue 0, all ipv4* nodes in the system,
and ethdev tx node of all ports.

Multicore graph processing
~~~~~~~~~~~~~~~~~~~~~~~~~~
In the current graph library implementation, specifically,
``rte_graph_walk()`` and ``rte_node_enqueue*`` fast path API functions
are designed to work on single-core to have better performance.
The fast path API works on graph object, So the multi-core graph
processing strategy would be to create graph object PER WORKER.

In fast path
~~~~~~~~~~~~
Typical fast-path code looks like below, where the application
gets the fast-path graph object using ``rte_graph_lookup()``
on the worker thread and run the ``rte_graph_walk()`` in a tight loop.

.. code-block:: c

    struct rte_graph *graph = rte_graph_lookup("worker0");

    while (!done) {
        rte_graph_walk(graph);
    }

Context update when graph walk in action
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The fast-path object for the node is ``struct rte_node``.

It may be possible that in slow-path or after the graph walk-in action,
the user needs to update the context of the node hence access to
``struct rte_node *`` memory.

``rte_graph_foreach_node()``, ``rte_graph_node_get()``,
``rte_graph_node_get_by_name()`` APIs can be used to to get the
``struct rte_node*``. ``rte_graph_foreach_node()`` iterator function works on
``struct rte_graph *`` fast-path graph object while others works on graph ID or name.

Get the node statistics using graph cluster
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The user may need to know the aggregate stats of the node across
multiple graph objects. Especially the situation where each graph object bound
to a worker thread.

Introduced a graph cluster object for statistics.
``rte_graph_cluster_stats_create()`` API shall be used for creating a
graph cluster with multiple graph objects and ``rte_graph_cluster_stats_get()``
to get the aggregate node statistics.

An example statistics output from ``rte_graph_cluster_stats_get()``

.. code-block:: diff

    +---------+-----------+-------------+---------------+-----------+---------------+-----------+
    |Node     |calls      |objs         |realloc_count  |objs/call  |objs/sec(10E6) |cycles/call|
    +---------------------+-------------+---------------+-----------+---------------+-----------+
    |node0    |12977424   |3322220544   |5              |256.000    |3047.151872    |20.0000    |
    |node1    |12977653   |3322279168   |0              |256.000    |3047.210496    |17.0000    |
    |node2    |12977696   |3322290176   |0              |256.000    |3047.221504    |17.0000    |
    |node3    |12977734   |3322299904   |0              |256.000    |3047.231232    |17.0000    |
    |node4    |12977784   |3322312704   |1              |256.000    |3047.243776    |17.0000    |
    |node5    |12977825   |3322323200   |0              |256.000    |3047.254528    |17.0000    |
    +---------+-----------+-------------+---------------+-----------+---------------+-----------+

Node writing guidelines
~~~~~~~~~~~~~~~~~~~~~~~

The ``process()`` function of a node is the fast-path function and that needs
to be written carefully to achieve max performance.

Broadly speaking, there are two different types of nodes.

Static nodes
~~~~~~~~~~~~
The first kind of nodes are those that have a fixed ``next_nodes[]`` for the
complete burst (like ethdev_rx, ethdev_tx) and it is simple to write.
``process()`` function can move the obj burst to the next node either using
``rte_node_next_stream_move()`` or using ``rte_node_next_stream_get()`` and
``rte_node_next_stream_put()``.

Intermediate nodes
~~~~~~~~~~~~~~~~~~
The second kind of such node is ``intermediate nodes`` that decide what is the
``next_node[]`` to send to on a per-packet basis. In these nodes,

* Firstly, there has to be the best possible packet processing logic.

* Secondly, each packet needs to be queued to its next node.

This can be done using ``rte_node_enqueue_[x1|x2|x4]()`` APIs if
they are to single next or ``rte_node_enqueue_next()`` that takes array of nexts.

In scenario where multiple intermediate nodes are present but most of the time
each node using the same next node for all its packets, the cost of moving every
pointer from current node's stream to next node's stream could be avoided.
This is called home run and ``rte_node_next_stream_move()`` could be used to
just move stream from the current node to the next node with least number of cycles.
Since this can be avoided only in the case where all the packets are destined
to the same next node, node implementation should be also having worst-case
handling where every packet could be going to different next node.

Example of intermediate node implementation with home run:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
1. Start with speculation that next_node = node->ctx.
This could be the next_node application used in the previous function call of this node.

2. Get the next_node stream array with required space using
``rte_node_next_stream_get(next_node, space)``.

3. while n_left_from > 0 (i.e packets left to be sent) prefetch next pkt_set
and process current pkt_set to find their next node

4. if all the next nodes of the current pkt_set match speculated next node,
just count them as successfully speculated(``last_spec``) till now and
continue the loop without actually moving them to the next node. else if there is
a mismatch, copy all the pkt_set pointers that were ``last_spec`` and move the
current pkt_set to their respective next's nodes using ``rte_enqueue_next_x1()``.
Also, one of the next_node can be updated as speculated next_node if it is more
probable. Finally, reset ``last_spec`` to zero.

5. if n_left_from != 0 then goto 3) to process remaining packets.

6. if last_spec == nb_objs, All the objects passed were successfully speculated
to single next node. So, the current stream can be moved to next node using
``rte_node_next_stream_move(node, next_node)``.
This is the ``home run`` where memcpy of buffer pointers to next node is avoided.

7. Update the ``node->ctx`` with more probable next node.

Graph object memory layout
--------------------------
.. _figure_graph_mem_layout:

.. figure:: img/graph_mem_layout.*

   Memory layout

Understanding the memory layout helps to debug the graph library and
improve the performance if needed.

Graph object consists of a header, circular buffer to store the pending
stream when walking over the graph, and variable-length memory to store
the ``rte_node`` objects.

The graph_nodes_mem_create() creates and populate this memory. The functions
such as ``rte_graph_walk()`` and ``rte_node_enqueue_*`` use this memory
to enable fastpath services.

Inbuilt Nodes
-------------

DPDK provides a set of nodes for data processing. The following section
details the documentation for the same.

ethdev_rx
~~~~~~~~~
This node does ``rte_eth_rx_burst()`` into stream buffer passed to it
(src node stream) and does ``rte_node_next_stream_move()`` only when
there are packets received. Each ``rte_node`` works only on one Rx port and
queue that it gets from node->ctx. For each (port X, rx_queue Y),
a rte_node is cloned from  ethdev_rx_base_node as ``ethdev_rx-X-Y`` in
``rte_node_eth_config()`` along with updating ``node->ctx``.
Each graph needs to be associated  with a unique rte_node for a (port, rx_queue).

ethdev_tx
~~~~~~~~~
This node does ``rte_eth_tx_burst()`` for a burst of objs received by it.
It sends the burst to a fixed Tx Port and Queue information from
node->ctx. For each (port X), this ``rte_node`` is cloned from
ethdev_tx_node_base as "ethdev_tx-X" in ``rte_node_eth_config()``
along with updating node->context.

Since each graph doesn't need more than one Txq, per port, a Txq is assigned
based on graph id to each rte_node instance. Each graph needs to be associated
with a rte_node for each (port).

pkt_drop
~~~~~~~~
This node frees all the objects passed to it considering them as
``rte_mbufs`` that need to be freed.

ip4_lookup
~~~~~~~~~~
This node is an intermediate node that does LPM lookup for the received
ipv4 packets and the result determines each packets next node.

On successful LPM lookup, the result contains the ``next_node`` id and
``next-hop`` id with which the packet needs to be further processed.

On LPM lookup failure, objects are redirected to pkt_drop node.
``rte_node_ip4_route_add()`` is control path API to add ipv4 routes.
To achieve home run, node use ``rte_node_stream_move()`` as mentioned in above
sections.

ip4_rewrite
~~~~~~~~~~~
This node gets packets from ``ip4_lookup`` node with next-hop id for each
packet is embedded in ``node_mbuf_priv1(mbuf)->nh``. This id is used
to determine the L2 header to be written to the packet before sending
the packet out to a particular ethdev_tx node.
``rte_node_ip4_rewrite_add()`` is control path API to add next-hop info.

null
~~~~
This node ignores the set of objects passed to it and reports that all are
processed.
