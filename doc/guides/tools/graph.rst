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

``-c <COREMASK>`` or ``-l <CORELIST>``

   Set the hexadecimal bit mask of the cores to run on.
   The CORELIST is a list of cores to be used.

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

``--help``

   Dumps application usage.


Supported CLI commands
----------------------

This section provides details on commands which can be used in ``<usecase>.cli``
file to express the requested use case configuration.

.. table:: Exposed CLIs
   :widths: auto

   +--------------------------------------+-----------------------------------+---------+----------+
   |               Command                |             Description           | Dynamic | Optional |
   +======================================+===================================+=========+==========+
   | | mempool <mempool_name> size        | | Command to create mempool which |   No    |    No    |
   | | <mbuf_size> buffers                | | will be further associated to   |         |          |
   | | <number_of_buffers>                | | RxQ to dequeue the packets.     |         |          |
   | | cache <cache_size> numa <numa_id>  |                                   |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | help mempool                         | | Command to dump mempool help    |   Yes   |    Yes   |
   |                                      | | message.                        |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | | ethdev <ethdev_name> rxq <n_queues>| | Command to create DPDK port with|   No    |    No    |
   | | txq <n_queues> <mempool_name>      | | given number of Rx and Tx queues|         |          |
   |                                      | | . Also attach RxQ with given    |         |          |
   |                                      | | mempool. Each port can have     |         |          |
   |                                      | | single mempool only i.e. all    |         |          |
   |                                      | | RxQs will share the same mempool|         |          |
   |                                      | | .                               |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | ethdev <ethdev_name> mtu <mtu_sz>    | | Command to configure MTU of DPDK|   Yes   |    Yes   |
   |                                      | | port.                           |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   |  | ethdev <ethdev_name> promiscuous  | | Command to enable/disable       |   Yes   |    Yes   |
   |  | <on/off>                          | | promiscuous mode on DPDK port.  |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | ethdev <ethdev_name> show            | | Command to dump current ethdev  |   Yes   |    Yes   |
   |                                      | | configuration.                  |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | ethdev <ethdev_name> stats           | | Command to dump current ethdev  |   Yes   |    Yes   |
   |                                      | | statistics.                     |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | | ethdev <ethdev_name> ip4 addr add  | | Command to configure IPv4       |   Yes   |    Yes   |
   | | <ip> netmask <mask>                | | address on given PCI device. It |         |          |
   |                                      | | is needed if user wishes to use |         |          |
   |                                      | | ``ipv4_lookup`` node.           |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | | ethdev <ethdev_name> ip6 addr add  | | Command to configure IPv6       |   Yes   |    Yes   |
   | | <ip> netmask <mask>                | | address on given PCI device. It |         |          |
   |                                      | | is needed if user wishes to use |         |          |
   |                                      | | ``ipv6_lookup`` node.           |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+
   | help ethdev                          | | Command to dump ethdev help     |   Yes   |    Yes   |
   |                                      | | message.                        |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+


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
