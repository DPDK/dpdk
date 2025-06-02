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
- Node specific xstat counts.
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

nb_xstats:
^^^^^^^^^^

The number of xstats that this node can report.
The ``xstat_desc[]`` stores the xstat descriptions which will later be propagated to stats.

xstat_desc[]:
^^^^^^^^^^^^^

The dynamic array to store the xstat descriptions that will be reported by this node.

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
functions can link them together to create a complex packet processing graph.

There are multiple different types of strategies to link the nodes.

Method (a):
^^^^^^^^^^^
Provide the ``next_nodes[]`` at the node registration time. See ``struct rte_node_register::nb_edges``.
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
form a graph object. The ``fnmatch()`` API used underneath for the pattern
matching to include the required nodes. After the graph create any changes to
nodes or graph is not allowed.

The ``rte_graph_create()`` API shall be used to create the graph.

Example of a graph object creation:

.. code-block:: console

   {"ethdev_rx-0-0", ip4*, ethdev_tx-*"}

In the above example, A graph object will be created with ethdev Rx
node of port 0 and queue 0, all ipv4* nodes in the system,
and ethdev tx node of all ports.

Graph models
~~~~~~~~~~~~
There are two different kinds of graph walking models. User can select the model using
``rte_graph_worker_model_set()`` API. If the application decides to use only one model,
the fast path check can be avoided by defining the model with RTE_GRAPH_MODEL_SELECT.
For example:

.. code-block:: c

  #define RTE_GRAPH_MODEL_SELECT RTE_GRAPH_MODEL_RTC
  #include "rte_graph_worker.h"

RTC (Run-To-Completion)
^^^^^^^^^^^^^^^^^^^^^^^
This is the default graph walking model. Specifically, ``rte_graph_walk_rtc()`` and
``rte_node_enqueue*`` fast path API functions are designed to work on single-core to
have better performance. The fast path API works on graph object, So the multi-core
graph processing strategy would be to create graph object PER WORKER.

Example:

Graph: node-0 -> node-1 -> node-2 @Core0.

.. code-block:: diff

    + - - - - - - - - - - - - - - - - - - - - - +
    '                  Core #0                  '
    '                                           '
    ' +--------+     +---------+     +--------+ '
    ' | Node-0 | --> | Node-1  | --> | Node-2 | '
    ' +--------+     +---------+     +--------+ '
    '                                           '
    + - - - - - - - - - - - - - - - - - - - - - +

Dispatch model
^^^^^^^^^^^^^^
The dispatch model enables a cross-core dispatching mechanism which employs
a scheduling work-queue to dispatch streams to other worker cores which
being associated with the destination node.

Use ``rte_graph_model_mcore_dispatch_lcore_affinity_set()`` to set lcore affinity
with the node.
Each worker core will have a graph repetition. Use ``rte_graph_clone()`` to clone
graph for each worker and use``rte_graph_model_mcore_dispatch_core_bind()`` to
bind graph with the worker core.

Example:

Graph topo: node-0 -> Core1; node-1 -> node-2; node-2 -> node-3.
Config graph: node-0 @Core0; node-1/3 @Core1; node-2 @Core2.

.. code-block:: diff

    + - - - - - -+     +- - - - - - - - - - - - - +     + - - - - - -+
    '  Core #0   '     '          Core #1         '     '  Core #2   '
    '            '     '                          '     '            '
    ' +--------+ '     ' +--------+    +--------+ '     ' +--------+ '
    ' | Node-0 | - - - ->| Node-1 |    | Node-3 |<- - - - | Node-2 | '
    ' +--------+ '     ' +--------+    +--------+ '     ' +--------+ '
    '            '     '     |                    '     '      ^     '
    + - - - - - -+     +- - -|- - - - - - - - - - +     + - - -|- - -+
                             |                                 |
                             + - - - - - - - - - - - - - - - - +


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
``rte_graph_node_get_by_name()`` APIs can be used to get the
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

#. Start with speculation that next_node = node->ctx.
   This could be the next_node application used in the previous function call of this node.

#. Get the next_node stream array with required space using
   ``rte_node_next_stream_get(next_node, space)``.

#. while n_left_from > 0 (i.e packets left to be sent) prefetch next pkt_set
   and process current pkt_set to find their next node

#. if all the next nodes of the current pkt_set match speculated next node,
   just count them as successfully speculated(``last_spec``) till now and
   continue the loop without actually moving them to the next node. else if there is
   a mismatch, copy all the pkt_set pointers that were ``last_spec`` and move the
   current pkt_set to their respective next's nodes using ``rte_enqueue_next_x1()``.
   Also, one of the next_node can be updated as speculated next_node if it is more
   probable. Finally, reset ``last_spec`` to zero.

#. if n_left_from != 0 then goto 3) to process remaining packets.

#. if last_spec == nb_objs, All the objects passed were successfully speculated
   to single next node. So, the current stream can be moved to next node using
   ``rte_node_next_stream_move(node, next_node)``.
   This is the ``home run`` where memcpy of buffer pointers to next node is avoided.

#. Update the ``node->ctx`` with more probable next node.

Graph object memory layout
--------------------------
.. _figure_graph_mem_layout:

.. figure:: img/graph_mem_layout.*

   Memory layout

Understanding the memory layout helps to debug the graph library and
improve the performance if needed.

Graph object consists of a header, circular buffer to store the pending stream
when walking over the graph, variable-length memory to store the ``rte_node`` objects,
and variable-length memory to store the xstat reported by each ``rte_node``.

The graph_nodes_mem_create() creates and populate this memory. The functions
such as ``rte_graph_walk()`` and ``rte_node_enqueue_*`` use this memory
to enable fastpath services.

Graph feature arc
-----------------

Introduction
~~~~~~~~~~~~

Graph feature arc is an abstraction to manage more than one network protocols
(or features) in a graph application with:

* Runtime network configurability
* Overloading of default node packet path
* Control/Data plane synchronization

.. note::

   Feature arc abstraction is introduced as an optional functionality
   to the graph library.
   Feature arc is a ``NOP`` to application which skips
   :ref:`feature arc initialization <Feature_Arc_Initialization>`

Runtime network configurability
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Feature arc facilitates to enable/disable protocols at runtime from control thread.
In fast path, it provides API to steer packets across nodes of those protocols
which are enabled from control thread.

Feature arc uses ``index`` object to enable/disable a protocol which is generic
to cater all the possibilities of configuring a protocol.
Examples of ``index`` object are
interface index, route index, flow index, classification index etc.

Runtime configuration of one ``index`` is independent of another ``index`` configuration.
In other words, packet received on an interface0 are steered independent
from packets received on interface1.
Both interface0 and interface1 can have separate sets of protocols enabled at the same time.

Feature arc also provides mechanism to express ``protocol sequencing order`` for packets.
If more than one protocols are active in a network layer,
packets may be required to be steered among protocol nodes in a specific order.
For example: in a typical firewall,
IPv4, IPsec and IP tables may be enabled at the same time in IP layer.
Feature arc provides mechanism to express sequence order
in which protocol nodes are to be traversed by packets received/sent on an interface.

Default node packet path overloading
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each network function has defined node packet path.
As an example, IPv4 router as a forwarder includes nodes performing -
packet ingress, ethernet reception, IPv4 reception, IPv4 lookup,
ethernet rewrite and packet egress.
Feature arc provides application to overload default node path
by providing hook points (like netfilter)
to insert out-of-tree or another protocol nodes in packet path.

.. _Control_Data_Plane_Synchronization:

Control/Data plane synchronization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Feature arc does not stop worker cores for any runtime control plane updates.
i.e. any feature (protocol) enable/disable at runtime does not stop worker cores.
Control plane feature enable/disable API also provides RCU mechanism, if needed.

When a feature is enabled in control plane, certain resources may be allocated
by application specific to ``[feature, index]``.
For example when IPsec is enabled either on an interface (policy based IPsec)
or route (route based IPsec), a security association(SA) would be allocated/initialized
and attached to interface/route.
Feature arc API is provided to pass SA from control thread to worker threads
for applying it (SA) on packets received/sent via interface or SA tunnel route.

Furthermore, when IPsec gets disabled for same ``[feature, index]`` in later point of time,
cleanup would be required to free resources associated with SA.
Cleanup can only be done in control thread when it ensures that no worker thread is using the SA.
For this use case, application can use RCU mechanism provided with enable/disable API.
See :ref:`notifier_cb <Feature_Notifier_Cb>`.

Objects
~~~~~~~

Feature
^^^^^^^

Feature is analogous to a protocol.

.. _Feature_Nodes:

Features nodes
^^^^^^^^^^^^^^

A feature node is a node which performs specific feature processing in a given network layer.
Feature nodes incorporates fast path feature arc API in their ``process()`` function
and are part of a unique arc.

Not all nodes in graph required to be made feature nodes.

.. _Start_Node:

Start node
^^^^^^^^^^

A node through which packets enters feature arc path is called ``Start node``.
It is a node which provides a hook point to overload node packet path.
Each feature arc object has unique ``start node``.
It can be a new node or any existing node in a graph.
Start node is not counted as a feature node in an arc.

.. _End_Feature_Node:

End feature node
^^^^^^^^^^^^^^^^

An end feature node is a feature node through which packets exits feature arc path.
It is required for exiting packets, from feature arc path,
which are getting processed by feature node
which is getting disabled at runtime in control thread.
It is always the last feature node in an arc.
As an exception to other feature nodes,
this node does not uses any feature arc fast path API.

Feature arc
^^^^^^^^^^^

.. _Figure_Arc_1:

.. figure:: img/feature_arc-1.*
   :alt: feature-arc-1
   :width: 350px
   :align: center

   Feature arc representation

An ordered list of feature nodes in a given network layer is called as feature arc.
It consists of three objects:

- :ref:`Start node <Start_Node>`
- :ref:`End feature node <End_Feature_Node>`
- :ref:`Zero or more feature nodes <Feature_Nodes>`

In order to :ref:`create <Feature_Arc_Registration>` a feature arc object,
only ``start node`` and ``end feature node`` are required.
Once created, feature nodes can be :ref:`added <Feature_Registration>` to the arc.

Feature data
^^^^^^^^^^^^

It is a fast path object which hold information to steer packets across nodes.

Programming model
~~~~~~~~~~~~~~~~~

Arc/Feature Registrations
^^^^^^^^^^^^^^^^^^^^^^^^^

Feature arc and feature registrations happens using constructor based macros.
While feature arc registration creates a feature arc object,
the feature registration adds provided node to a feature arc object.

.. note::

   During registration, no memory is allocated associated with any feature arc.
   Actual memory allocation, object creation and connecting of nodes via edges
   corresponding to all registered feature arcs happens as part of
   :ref:`feature arc initialization <Feature_Arc_Initialization>`.

.. _Feature_Arc_Registration:

Feature arc registration
************************

A feature arc object creation require ``feature arc registration``.
Once registered, feature arc is created as part of
:ref:`initialization <Feature_Arc_Initialization>`.
A feature arc is registered via ``RTE_GRAPH_FEATURE_ARC_REGISTER()``.
An arc shown in :ref:`figure <Figure_Arc_1>` can be registered as follows:

.. code-block:: c

   /* Existing nodes */
   RTE_NODE_REGISTER(Node-A);
   RTE_NODE_REGISTER(Node-B);

   /* Define End feature node: Node-B */
   struct rte_graph_feature_register Node-B-feature = {
       .feature_name = "Node-B-feature",
       .arc_name = "Arc1",
       .feature_process_fn = nodeB_process_fn(),
       .feature_node = &Node-B,
   };

   /* Arc1 registration */
   struct rte_graph_feature_arc_register arc1 = {
       .arc_name = "Arc1",
       .max_indexes = RTE_MAX_ETHPORTS,
       .start_node = &Node-A,
       .start_node_feature_process_fn = nodeA_feature_process_fn(),
       .end_feature_node = &Node-B-feature,
   };

   /* Call constructor */
    RTE_GRAPH_FEATURE_ARC_REGISTER(arc1);

.. note::

   Feature arc can also be created using ``rte_graph_feature_arc_create()`` API as well.

.. _Feature_Registration:

Feature registration
********************

A feature registration means defining a feature node
which would be added to a unique ``arc``.
A feature nodes needs to know ``arc name`` to which it wants to connect to.
Registration happens via ``RTE_GRAPH_FEATURE_REGISTER()``.

A ``Feature-1`` shown in :ref:`figure <Figure_Arc_1>` can be registered as follows:

.. code-block:: c

   /* Existing node */
   RTE_NODE_REGISTER(Feature-1);

   /* Define feature node: Feature-1 */
   struct rte_graph_feature_register Feature-1 = {
       .feature_name = "Feature-1",
       .arc_name = "Arc1",
       .feature_process_fn = feature1_process_fn(),
       .feature_node = &Feature-1,
   };

   /* Call constructor */
   RTE_GRAPH_FEATURE_REGISTER(Feature-1);

.. note::

   A feature node can be out-of-tree application node
   which is willing to connect to an arc defined by DPDK built-in nodes.
   This way application can hook its node to standard node packet path.

Advance parameters
``````````````````

.. _Figure_Arc_2:

.. figure:: img/feature_arc-2.*
   :alt: feature-arc-2
   :width: 550px
   :align: center

   Feature registration advance parameters

Feature registration have some advance parameters to control the feature node.
As shown in above figure, ``Custom Feature`` and ``Feature-2`` nodes
can be added to existing arc as follows:

.. code-block:: c

   /* Define feature node: Custom-Feature */
   struct rte_graph_feature_register Custom-Feature = {
       .feature_name = "Custom-Feature",
       .arc_name = "Arc1",
           ...
           ...
           ...
       .notifier_cb = Custom-Feature_notifier_fn(),  /* Optional notifier function */
       .runs_after = "Feature-1",
   };

   /* Define feature node: Feature-2 */
   struct rte_graph_feature_register Feature-2 = {
       .feature_name = "Feature-2",
       .arc_name = "Arc1",
           ...
           ...
           ...
       .override_index_cb = Feature-2_override_index_cb(),
       .runs_after = "Feature-1",
       .runs_before = "Custom-Feature",
   };

runs_after/runs_before
......................

These parameters are used to express the sequencing order of feature nodes.
If ``Custom Feature`` needs to run after ``Feature-1``,
it can be defined as shown above.
Similarly, if ``Feature-2`` needs to run before ``Custom-Feature`` but after ``Feature-1``,
it can be done as shown above.

.. _Feature_Notifier_Cb:

notifier_cb()
.............

If non-NULL, every feature enable/disable in control plane
will invoke the notifier callback on control thread.
This notifier callback can be used to destroy resources for ``[feature, index]`` pair
during ``feature disable`` which might have allocated during feature enable.

``notifier_cb()`` is called, at runtime, for every enable/disable of ``[feature, index]``
from control thread.

If RCU is provided to enable/disable API,
notifier_cb() is called after ``rte_rcu_qsbr_synchronize()``.
Application also needs to call ``rte_rcu_qsbr_quiescent()`` in worker thread
(preferably after every ``rte_graph_walk()`` iteration)

override_index_cb()
....................

A feature arc is :ref:`registered <Feature_Arc_Registration>`
to operate on certain number of ``max_indexes``.
If particular feature like to overload this ``max_indexes``
with a larger value, it can do so by returning larger value in this callback.
In case of multiple features, largest value returned by any feature
would be selected for creating feature arc.

.. _Feature_Arc_Initialization:

Initializing Feature arc
^^^^^^^^^^^^^^^^^^^^^^^^

Following code shows how to initialize feature arc sub-system.
``rte_graph_feature_arc_init()`` API is used to initialize a feature arc sub-system.
If not called, feature arc has no impact on application.

.. code-block:: c

   struct rte_graph_param *graph_param = app_get_graph_param();

   /* Initialize feature arc before graph create */
   rte_graph_feature_arc_init(0);

   rte_graph_create(graph_param);

.. note::

   ``rte_graph_feature_arc_init()`` API should be called before ``rte_graph_create()``.
   If not called, feature arc is a ``NOP`` to application.

Runtime feature enable/disable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A feature can be enabled or disabled at runtime from control thread
using ``rte_graph_feature_enable()`` and ``rte_graph_feature_disable()`` functions respectively.

.. code-block:: c

   struct rte_rcu_qsbr *rcu_qsbr = app_get_rcu_qsbr();
   rte_graph_feature_arc_t _arc;
   uint16_t app_cookie;

   if (rte_graph_feature_arc_lookup_by_name("Arc1", &_arc) < 0) {
       RTE_LOG(ERR, GRAPH, "Arc1 not found\n");
       return -ENOENT;
   }
   app_cookie = 100; /* Specific to ['Feature-1`, `port-0`] */

   /* Enable feature */
   rte_graph_feature_enable(_arc, 0 /* port-0 */,
                            "Feature-1" /* Name of the node feature */,
                            app_cookie, rcu_qsbr);

   /* Disable feature */
   rte_graph_feature_disable(_arc, 0 /* port-0 */,
                             "Feature-1" /* Name of the node feature */,
                             rcu_qsbr);

.. note::

   RCU argument is optional argument to enable/disable API.
   See :ref:`control/data plane synchronization <Control_Data_Plane_Synchronization>`
   and :ref:`notifier_cb <Feature_Notifier_Cb>` for more details on when RCU is needed.

Fast path traversal rules
^^^^^^^^^^^^^^^^^^^^^^^^^

``Start node``
**************

If feature arc is :ref:`initialized <Feature_Arc_Initialization>`,
``start_node_feature_process_fn()`` will be called by ``rte_graph_walk()``
instead of node's original ``process()``.
This function should allow packets to enter arc path
whenever any feature is enabled at runtime.

.. code-block:: c

   static int nodeA_init(const struct rte_graph *graph, struct rte_node *node)
   {
       rte_graph_feature_arc_t _arc;

       if (rte_graph_feature_arc_lookup_by_name("Arc1", &_arc) < 0) {
           RTE_LOG(ERR, GRAPH, "Arc1 not found\n");
           return -ENOENT;
       }

       /* Save arc in node context */
       node->ctx = _arc;
       return 0;
   }

   int nodeA_process_inline(struct rte_graph *graph, struct rte_node *node,
                            void **objs, uint16_t nb_objs,
                            struct rte_graph_feature_arc *arc,
                            const int do_arc_processing)
   {
       for(uint16_t i = 0; i < nb_objs; i++) {
           struct rte_mbuf *mbuf = objs[i];
           rte_edge_t edge_to_child = 0; /* By default to Node-B */

           if (do_arc_processing) {
               struct rte_graph_feature_arc_mbuf_dynfields *dyn =
                   rte_graph_feature_arc_mbuf_dynfields_get(mbuf, arc->mbuf_dyn_offset);

               if (rte_graph_feature_data_first_feature_get(mbuf, mbuf->port,
                                                            &dyn->feature_data,
                                                            &edge_to_child) < 0) {

                   /* Some feature is enabled, edge_to_child is overloaded */
               }
           }
           /* enqueue as usual */
           rte_node_enqueue_x1(graph, node, mbuf, edge_to_child);
      }
   }

   int nodeA_feature_process_fn(struct rte_graph *graph, struct rte_node *node,
                                void **objs, uint16_t nb_objs)
   {
       struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(node->ctx);

       if (unlikely(rte_graph_feature_arc_has_any_feature(arc)))
           return nodeA_process_inline(graph, node, objs, nb_objs, arc, 1 /* do arc processing */);
       else
           return nodeA_process_inline(graph, node, objs, nb_objs, NULL, 0 /* skip arc processing */);
   }

``Feature nodes``
*****************

Following code-snippet explains fast path traversal rule for ``Feature-1``
:ref:`feature node <Feature_Nodes>` shown in :ref:`figure <Figure_Arc_2>`.

.. code-block:: c

   static int Feature1_node_init(const struct rte_graph *graph, struct rte_node *node)
   {
       rte_graph_feature_arc_t _arc;

       if (rte_graph_feature_arc_lookup_by_name("Arc1", &_arc) < 0) {
           RTE_LOG(ERR, GRAPH, "Arc1 not found\n");
           return -ENOENT;
       }

       /* Save arc in node context */
       node->ctx = _arc;
       return 0;
   }

   int feature1_process_inline(struct rte_graph *graph, struct rte_node *node,
                               void **objs, uint16_t nb_objs,
                               struct rte_graph_feature_arc *arc)
   {
       for (uint16_t i = 0; i < nb_objs; i++) {
           struct rte_mbuf *mbuf = objs[i];
           rte_edge_t edge_to_child = 0; /* By default to Node-B */

           struct rte_graph_feature_arc_mbuf_dynfields *dyn =
                   rte_graph_feature_arc_mbuf_dynfields_get(mbuf, arc->mbuf_dyn_offset);

           /* Get feature app cookie for mbuf */
           uint16_t app_cookie = rte_graph_feature_data_app_cookie_get(mbuf, &dyn->feature_data);

           if (feature_local_lookup(app_cookie) {

               /* Packets is relevant to this feature. Move packet from arc path */
               edge_to_child = X;

           } else {

               /* Packet not relevant to this feature.
                * Send this packet to next enabled feature.
                */
               rte_graph_feature_data_next_feature_get(mbuf, &dyn->feature_data,
                                                       &edge_to_child);
           }

           /* enqueue as usual */
           rte_node_enqueue_x1(graph, node, mbuf, edge_to_child);
      }
   }

   int feature1_process_fn(struct rte_graph *graph, struct rte_node *node,
                           void **objs, uint16_t nb_objs)
   {
       struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(node->ctx);

       return feature1_process_inline(graph, node, objs, nb_objs, arc);
   }

``End feature node``
********************

An end feature node is a feature node through which packets exits feature arc path.
It should not use any feature arc fast path API.

Feature arc destroy
^^^^^^^^^^^^^^^^^^^

``rte_graph_feature_arc_destroy()`` can be used to free a arc object.

Feature arc cleanup
^^^^^^^^^^^^^^^^^^^

``rte_graph_feature_arc_cleanup()`` can be used to free all resources
associated with feature arc module.

Inbuilt Nodes
-------------

DPDK provides a set of nodes for data processing.
The following diagram depicts inbuilt nodes data flow.

.. _figure_graph_inbuit_node_flow:

.. figure:: img/graph_inbuilt_node_flow.*

   Inbuilt nodes data flow

Following section details the documentation for individual inbuilt node.

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

ip4_lookup_fib
~~~~~~~~~~~~~~

This node is an intermediate node that does FIB lookup
for the received IPv4 packets
and the result determines each packets next node.

ip4_rewrite
~~~~~~~~~~~
This node gets packets from ``ip4_lookup`` node with next-hop id for each
packet is embedded in ``node_mbuf_priv1(mbuf)->nh``. This id is used
to determine the L2 header to be written to the packet before sending
the packet out to a particular ethdev_tx node.
``rte_node_ip4_rewrite_add()`` is control path API to add next-hop info.

ip4_reassembly
~~~~~~~~~~~~~~
This node is an intermediate node that reassembles ipv4 fragmented packets,
non-fragmented packets pass through the node un-effected.
The node rewrites its stream and moves it to the next node.
The fragment table and death row table should be setup via the
``rte_node_ip4_reassembly_configure`` API.

ip6_lookup
~~~~~~~~~~
This node is an intermediate node that does LPM lookup for the received
IPv6 packets and the result determines each packets next node.

On successful LPM lookup, the result contains the ``next_node`` ID
and `next-hop`` ID with which the packet needs to be further processed.

On LPM lookup failure, objects are redirected to ``pkt_drop`` node.
``rte_node_ip6_route_add()`` is control path API to add IPv6 routes.
To achieve home run, node use ``rte_node_stream_move()``
as mentioned in above sections.

ip6_rewrite
~~~~~~~~~~~
This node gets packets from ``ip6_lookup`` node with next-hop ID
for each packet is embedded in ``node_mbuf_priv1(mbuf)->nh``.
This ID is used to determine the L2 header to be written to the packet
before sending the packet out to a particular ``ethdev_tx`` node.
``rte_node_ip6_rewrite_add()`` is control path API to add next-hop info.

null
~~~~
This node ignores the set of objects passed to it and reports that all are
processed.

kernel_tx
~~~~~~~~~
This node is an exit node that forwards the packets to kernel.
It will be used to forward any control plane traffic to kernel stack from DPDK.
It uses a raw socket interface to transmit the packets,
it uses the packet's destination IP address in sockaddr_in address structure
and ``sendto`` function to send data on the raw socket.
After sending the burst of packets to kernel,
this node frees up the packet buffers.

kernel_rx
~~~~~~~~~
This node is a source node which receives packets from kernel
and forwards to any of the intermediate nodes.
It uses the raw socket interface to receive packets from kernel.
Uses ``poll`` function to poll on the socket fd
for ``POLLIN`` events to read the packets from raw socket
to stream buffer and does ``rte_node_next_stream_move()``
when there are received packets.

ip4_local
~~~~~~~~~
This node is an intermediate node that does ``packet_type`` lookup for
the received ipv4 packets and the result determines each packets next node.

On successful ``packet_type`` lookup, for any IPv4 protocol the result
contains the ``next_node`` id and ``next-hop`` id with which the packet
needs to be further processed.

On packet_type lookup failure, objects are redirected to ``pkt_drop`` node.
``rte_node_ip4_route_add()`` is control path API to add ipv4 address with 32 bit
depth to receive to packets.
To achieve home run, node use ``rte_node_stream_move()`` as mentioned in above
sections.

udp4_input
~~~~~~~~~~
This node is an intermediate node that does udp destination port lookup for
the received ipv4 packets and the result determines each packets next node.

User registers a new node ``udp4_input`` into graph library during initialization
and attach user specified node as edege to this node using
``rte_node_udp4_usr_node_add()``, and create empty hash table with destination
port and node id as its feilds.

After successful addition of user node as edege, edge id is returned to the user.

User would register ``ip4_lookup`` table with specified ip address and 32 bit as mask
for ip filtration using api ``rte_node_ip4_route_add()``.

After graph is created user would update hash table with custom port with
and previously obtained edge id using API ``rte_node_udp4_dst_port_add()``.

When packet is received lpm look up is performed if ip is matched the packet
is handed over to ip4_local node, then packet is verified for udp proto and
on success packet is enqueued to ``udp4_input`` node.

Hash lookup is performed in ``udp4_input`` node with registered destination port
and destination port in UDP packet , on success packet is handed to ``udp_user_node``.
