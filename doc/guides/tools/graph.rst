.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Marvell.

dpdk-graph Application
======================

The ``dpdk-graph`` tool is a Data Plane Development Kit (DPDK)
application that allows exercising various graph use cases.
This application has a generic framework to add new graph based use cases
to verify functionality.
Each use case is defined as a ``<usecase>.cli`` file.
Based on the input file, application creates a graph to cater the use case.

Also this application framework can be used by other graph-based applications.


Running the Application
-----------------------

The application has a number of command line options
which can be provided in following syntax:

.. code-block:: console

   dpdk-graph [EAL Options] -- [application options]

EAL Options
~~~~~~~~~~~

Following are the EAL command-line options that can be used in conjunction
with the ``dpdk-graph`` application.
See the DPDK Getting Started Guides for more information on these options.

``-l <CORELIST>``

   List the set of cores to be used.

Application Options
~~~~~~~~~~~~~~~~~~~

Following are the application command-line options:

``-h``

   Set the host IPv4 address over which telnet session can be opened.
   It is an optional parameter. Default host address is ``0.0.0.0``.

``-p``

   Set the L4 port number over which telnet session can be opened.
	It is an optional parameter. Default port is ``8086``.

``-s``

   Script name with absolute path which specifies the use case.
   It is a mandatory parameter which will be used
   to create desired graph for a given use case.

``--enable-graph-stats``

   Enable graph statistics printing on console.
   By default, graph statistics are disabled.

``--enable-graph-feature-arc``

   Enable feature arc functionality in graph nodes.
   By default, feature arc is disabled.

``--help``

   Dumps application usage.


Supported Use cases
-------------------

l3fwd
~~~~~

This use case is supported for both H/W and PCAP vdev network devices.
To demonstrate, corresponding ``.cli`` files are available at ``app/graph/examples/``
named as ``l3fwd.cli`` and ``l3fwd_pcap.cli`` respectively.

l2fwd
~~~~~

This use case is supported for both H/W and PCAP vdev network devices.
To demonstrate, corresponding ``.cli`` files are available at ``app/graph/examples/``
named as ``l2fwd.cli`` and ``l2fwd_pcap.cli`` respectively.

Example Commands
^^^^^^^^^^^^^^^^
For H/W devices

.. code-block:: console

   ./dpdk-graph -l 0-7 -a 0002:02:00.0 -a 0002:03:00.0 --
                -s <dpdk_root_dir>/app/graph/examples/l3fwd.cli

   ./dpdk-graph -l 0-7 -a 0002:02:00.0 -a 0002:03:00.0 --
                -s <dpdk_root_dir>/app/graph/examples/l2fwd.cli

For net_pcapX devices

.. code-block:: console

   ./dpdk-graph -l 0-7 --vdev=net_pcap0,rx_pcap=in_net_pcap0.pcap,tx_pcap=out_net_pcap1.pcap
                        --vdev=net_pcap1,rx_pcap=in_net_pcap1.pcap,tx_pcap=out_net_pcap0.pcap
                        -- -s <dpdk_root_dir>/app/graph/examples/l3fwd_pcap.cli

   ./dpdk-graph -l 0-7 --vdev=net_pcap0,rx_pcap=in_net_pcap0.pcap,tx_pcap=out_net_pcap1.pcap
                        --vdev=net_pcap1,rx_pcap=in_net_pcap1.pcap,tx_pcap=out_net_pcap0.pcap
                        -- -s <dpdk_root_dir>/app/graph/examples/l2fwd_pcap.cli

Verifying traffic
^^^^^^^^^^^^^^^^^

``l3fwd.cli`` and ``l3fwd_pcap.cli`` creates setup with two network ports.
Routing between these ports are done by lookup node routing information.
For current use case, following routing table is used:

.. code-block:: console

   DIP        port
   10.0.2.2    1
   20.0.2.2    0

On the successful execution of ``l3fwd.cli`` or ``l3fwd_pcap.cli``,
user needs to send traffic with mentioned DIP.

For net_pcapX devices, required pcap file should be created and passed to application.
These pcap files can be created in several ways.
Scapy is one of the method to get the same:

.. code-block:: console

   # scapy
   >>> pkts=[Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="10.0.2.2")]
   >>>
   >>> wrpcap("in_net_pcap1.pcap",pkts)
   >>>
   >>> pkts=[Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="29.0.0.1", dst="20.0.2.2"),
             Ether(dst="FA:09:F9:D7:E0:9D", src="10:70:1d:2f:42:2d")/IP(src="28.0.0.1", dst="20.0.2.2")]
   >>>
   >>> wrpcap("in_net_pcap0.pcap",pkts)
   >>> quit


Supported CLI commands
----------------------

This section provides details on commands which can be used in ``<usecase>.cli``
file to express the requested use case configuration.

.. table:: Exposed CLIs
   :widths: auto

   +--------------------------------------+-----------------------------------+-------------------+----------+
   |               Command                |             Description           |     Scope         | Optional |
   +======================================+===================================+===================+==========+
   | | graph <usecases> [bsz <size>]      | | Command to express the desired  | :ref:`1 <scopes>` |    No    |
   | | [tmo <ns>] [coremask <bitmask>]    | | use case. Also enables/disable  |                   |          |
   | | model <rtc/mcd/default> pcap_enable| | pcap capturing.                 |                   |          |
   | | <0/1> num_pcap_pkts <num> pcap_file|                                   |                   |          |
   | | <output_capture_file>              |                                   |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | graph start                          | | Command to start the graph.     | :ref:`1 <scopes>` |    No    |
   |                                      | | This command triggers that no   |                   |          |
   |                                      | | more commands are left to be    |                   |          |
   |                                      | | parsed and graph initialization |                   |          |
   |                                      | | can be started now. It must be  |                   |          |
   |                                      | | the last command in usecase.cli |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | graph stats show                     | | Command to dump current graph   | :ref:`2 <scopes>` |    Yes   |
   |                                      | | statistics.                     |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help graph                           | | Command to dump graph help      | :ref:`2 <scopes>` |    Yes   |
   |                                      | | message.                        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | mempool <mempool_name> size        | | Command to create mempool which | :ref:`1 <scopes>` |    No    |
   | | <mbuf_size> buffers                | | will be further associated to   |                   |          |
   | | <number_of_buffers>                | | RxQ to dequeue the packets.     |                   |          |
   | | cache <cache_size> numa <numa_id>  |                                   |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help mempool                         | | Command to dump mempool help    | :ref:`2 <scopes>` |    Yes   |
   |                                      | | message.                        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev <ethdev_name> rxq <n_queues>| | Command to create DPDK port with| :ref:`1 <scopes>` |    No    |
   | | txq <n_queues> <mempool_name>      | | given number of Rx and Tx queues|                   |          |
   |                                      | | . Also attach RxQ with given    |                   |          |
   |                                      | | mempool. Each port can have     |                   |          |
   |                                      | | single mempool only i.e. all    |                   |          |
   |                                      | | RxQs will share the same mempool|                   |          |
   |                                      | | .                               |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | ethdev <ethdev_name> mtu <mtu_sz>    | | Command to configure MTU of DPDK| :ref:`3 <scopes>` |    Yes   |
   |                                      | | port.                           |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev forward <tx_dev_name>       | | Command to configure port       | :ref:`1 <scopes>` |    Yes   |
   | | <rx_dev_name>                      | | forwarding of DPDK              |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev <ethdev_name> promiscuous   | | Command to enable/disable       | :ref:`3 <scopes>` |    Yes   |
   | | <on/off>                           | | promiscuous mode on DPDK port.  |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | ethdev <ethdev_name> show            | | Command to dump current ethdev  | :ref:`2 <scopes>` |    Yes   |
   |                                      | | configuration.                  |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | ethdev <ethdev_name> stats           | | Command to dump current ethdev  | :ref:`2 <scopes>` |    Yes   |
   |                                      | | statistics.                     |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev <ethdev_name> ip4 addr add  | | Command to configure IPv4       | :ref:`3 <scopes>` |    Yes   |
   | | <ip> netmask <mask>                | | address on given PCI device. It |                   |          |
   |                                      | | is needed if user wishes to use |                   |          |
   |                                      | | ``ipv4_lookup`` node.           |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev <ethdev_name> ip6 addr add  | | Command to configure IPv6       | :ref:`3 <scopes>` |    Yes   |
   | | <ip> netmask <mask>                | | address on given PCI device. It |                   |          |
   |                                      | | is needed if user wishes to use |                   |          |
   |                                      | | ``ipv6_lookup`` node.           |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help ethdev                          | | Command to dump ethdev help     | :ref:`2 <scopes>` |    Yes   |
   |                                      | | message.                        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ipv4_lookup route add ipv4 <ip>    | | Command to add a route into     | :ref:`3 <scopes>` |    Yes   |
   | |  netmask <mask> via <ip>           | | ``ipv4_lookup`` LPM table or    |                   |          |
   |                                      | | FIB. It is needed if user wishes|                   |          |
   |                                      | | to route the packets based on   |                   |          |
   |                                      | | LPM lookup table or FIB.        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ipv4_lookup mode <lpm|fib>         | | Command to set ipv4 lookup mode | :ref:`1 <scopes>` |    Yes   |
   |                                      | | to either LPM or FIB. By default|                   |          |
   |                                      | | the lookup mode is LPM.         |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help ipv4_lookup                     | | Command to dump ``ipv4_lookup`` | :ref:`2 <scopes>` |    Yes   |
   |                                      | | help message.                   |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ipv6_lookup route add ipv6 <ip>    | | Command to add a route into     | :ref:`3 <scopes>` |    Yes   |
   | |  netmask <mask> via <ip>           | | ``ipv6_lookup`` LPM table or.   |                   |          |
   |                                      | | FIB. It is needed if user wishes|                   |          |
   |                                      | | to route the packets based on   |                   |          |
   |                                      | | LPM6 lookup table or FIB.       |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ipv6_lookup mode <lpm|fib>         | | Command to set ipv6 lookup mode | :ref:`1 <scopes>` |    Yes   |
   |                                      | | to either LPM or FIB. By default|                   |          |
   |                                      | | the lookup mode is LPM.         |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help ipv6_lookup                     | | Command to dump ``ipv6_lookup`` | :ref:`2 <scopes>` |    Yes   |
   |                                      | | help message.                   |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | neigh add ipv4 <ip> <mac>            | | Command to add a neighbour      | :ref:`3 <scopes>` |    Yes   |
   |                                      | | information into                |                   |          |
   |                                      | | ``ipv4_rewrite`` node.          |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | neigh add ipv6 <ip> <mac>            | | Command to add a neighbour      | :ref:`3 <scopes>` |    Yes   |
   |                                      | | information into                |                   |          |
   |                                      | | ``ipv6_rewrite`` node.          |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help neigh                           | | Command to dump neigh help      | :ref:`2 <scopes>` |    Yes   |
   |                                      | | message.                        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | ethdev_rx map port <ethdev_name>   | | Command to add port-queue-core  | :ref:`1 <scopes>` |    No    |
   | | queue <q_num> core <core_id>       | | mapping to ``ethdev_rx`` node.  |                   |          |
   |                                      | | ``ethdev_rx`` node instance will|                   |          |
   |                                      | | be pinned on given core and will|                   |          |
   |                                      | | poll on requested port/queue    |                   |          |
   |                                      | | pair.                           |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help ethdev_rx                       | | Command to dump ethdev_rx help  | :ref:`2 <scopes>` |    Yes   |
   |                                      | | message.                        |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | help feature                         | | Command to dump feature arc     | :ref:`2 <scopes>` |    Yes   |
   |                                      | | help message.                   |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | feature arcs                         | | Command to dump all created     | :ref:`2 <scopes>` |    Yes   |
   |                                      | | feature arcs                    |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | feature enable <arc name>          | | Enable <feature name> of <arc   | :ref:`2 <scopes>` |    Yes   |
   | | <feature name> <interface index>   | | name> on an interface.          |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+
   | | feature disable <arc name>         | | Disable <feature name> of <arc  | :ref:`2 <scopes>` |    Yes   |
   | | <feature name> <interface index>   | | name> on an interface.          |                   |          |
   +--------------------------------------+-----------------------------------+-------------------+----------+

.. _scopes:

1. Script only
2. Telnet only
3. Script and telnet both

Runtime configuration
---------------------

Application allows some configuration to be modified at runtime using a telnet session.
Application initiates a telnet server with host address ``0.0.0.0`` and port number ``8086``
by default.

If user passes ``-h`` and ``-p`` options while running application,
then corresponding IP address and port number will be used for telnet session.

After successful launch of application,
client can connect to application using given host & port
and console will be accessed with prompt ``graph>``.

Command to access a telnet session:

.. code-block:: console

   telnet <host> <port>

Example: ``dpdk-graph`` is started with ``-h 10.28.35.207`` and ``-p 50000`` then

.. code-block:: console

   $ telnet 10.28.35.207 50000
   Trying 10.28.35.207...
   Connected to 10.28.35.207.
   Escape character is '^]'.

   Welcome!

   graph>
   graph>
   graph> help ethdev

   ----------------------------- ethdev command help -----------------------------
   ethdev <ethdev_name> rxq <n_queues> txq <n_queues> <mempool_name>
   ethdev <ethdev_name> ip4 addr add <ip> netmask <mask>
   ethdev <ethdev_name> ip6 addr add <ip> netmask <mask>
   ethdev forward <tx_dev_name> <rx_dev_name>
   ethdev <ethdev_name> promiscuous <on/off>
   ethdev <ethdev_name> mtu <mtu_sz>
   ethdev <ethdev_name> stats
   ethdev <ethdev_name> show
   graph>

To exit the telnet session, type ``Ctrl + ]``.
This changes the ``graph>`` command prompt to ``telnet>`` command prompt.
Now running ``close`` or ``quit`` command on ``telnet>`` prompt
will terminate the telnet session.


Created graph for use case
--------------------------

On the successful execution of ``<usecase>.cli`` file, corresponding graph will be created.
This section mentions the created graph for each use case.

l3fwd
~~~~~

.. _figure_l3fwd_graph:

.. figure:: img/graph-usecase-l3fwd.*

l2fwd
~~~~~

.. _figure_l2fwd_graph:

.. figure:: img/graph-usecase-l2fwd.*
