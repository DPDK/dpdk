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

    ./<build_dir>/examples/dpdk-server [EAL options] -- -p PORTMASK -n NUM_NODES -f NUM_FLOWS

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


.. literalinclude:: ../../../examples/server_node_efd/efd_server/init.c
    :language: c
    :start-after: Create EFD table. 8<
    :end-before: >8 End of creation EFD table.

After initialization, packets are received from the enabled ports, and the IPv4
address from the packets is used as a key to look up in the EFD table,
which tells the node where the packet has to be distributed.

.. literalinclude:: ../../../examples/server_node_efd/efd_server/main.c
    :language: c
    :start-after: Processing packets. 8<
    :end-before: >8 End of process_packets.

The burst of packets received is enqueued in temporary buffers (per node),
and enqueued in the shared ring between the server and the node.
After this, a new burst of packets is received and this process is
repeated infinitely.

.. literalinclude:: ../../../examples/server_node_efd/efd_server/main.c
    :language: c
    :start-after: Flush rx queue. 8<
    :end-before: >8 End of sending a burst of traffic to a node.

The second process, the back-end node, receives the packets from the shared
ring with the server and send them out, if they belong to the node.

At initialization, it attaches to the server process memory, to have
access to the shared ring, parameters and statistics.

.. literalinclude:: ../../../examples/server_node_efd/efd_node/node.c
    :language: c
    :start-after: Attaching to the server process memory. 8<
    :end-before: >8 End of attaching to the server process memory.
    :dedent: 1

Then, the hash table that contains the flows that will be handled
by the node is created and populated.

.. literalinclude:: ../../../examples/server_node_efd/efd_node/node.c
    :language: c
    :start-after: Creation of hash table. 8<
    :end-before: >8 End of creation of hash table.

After initialization, packets are dequeued from the shared ring
(from the server) and, like in the server process,
the IPv4 address from the packets is used as a key to look up in the hash table.
If there is a hit, packet is stored in a buffer, to be eventually transmitted
in one of the enabled ports. If key is not there, packet is dropped, since the
flow is not handled by the node.

.. literalinclude:: ../../../examples/server_node_efd/efd_node/node.c
    :language: c
    :start-after: Packets dequeued from the shared ring. 8<
    :end-before: >8 End of packets dequeuing.

Finally, note that both processes updates statistics, such as transmitted, received
and dropped packets, which are shown and refreshed by the server app.

.. literalinclude:: ../../../examples/server_node_efd/efd_server/main.c
    :language: c
    :start-after: Display recorded statistics. 8<
    :end-before: >8 End of displaying the recorded statistics.
