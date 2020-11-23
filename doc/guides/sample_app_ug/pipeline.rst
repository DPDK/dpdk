..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Pipeline Application
====================

Application overview
--------------------

This application showcases the features of the Software Switch (SWX) pipeline that is aligned with the P4 language.

Each pipeline is created using a specification file that can either be manually developed or generated using a P4 compiler.

Each pipeline is built through the CLI, either by invoking commands one by one, or through a CLI script.
The CLI can also be used to update the pipeline tables or poll the pipeline statistics.

Each pipeline is mapped to a specific application thread. Multiple pipelines can be mapped to the same thread.

Running the application
-----------------------

The application startup command line is::

   dpdk-pipeline [EAL_ARGS] -- [-s SCRIPT_FILE] [-h HOST] [-p PORT]

The application startup arguments are:

``-s SCRIPT_FILE``

 * Optional: Yes

 * Default: Not present

 * Argument: Path to the CLI script file to be run at application startup.
   No CLI script file will run at startup if this argument is not present.

``-h HOST``

 * Optional: Yes

 * Default: ``0.0.0.0``

 * Argument: IP Address of the host running the application to be used by
   remote TCP based client (telnet, netcat, etc.) for connection.

``-p PORT``

 * Optional: Yes

 * Default: ``8086``

 * Argument: TCP port number at which the application is running.
   This port number should be used by remote TCP client (such as telnet, netcat, etc.) to connect to host application.

Refer to *DPDK Getting Started Guide* for general information on running applications and the Environment Abstraction Layer (EAL) options.

The following is an example command to run the application configured for the VXLAN encapsulation example:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-pipeline -c 0x3 -- -s examples/vxlan.cli

The application should start successfully and display as follows:

.. code-block:: console

    EAL: Detected 40 lcore(s)
    EAL: Detected 2 NUMA nodes
    EAL: Multi-process socket /var/run/.rte_unix
    EAL: Probing VFIO support...
    EAL: PCI device 0000:02:00.0 on NUMA socket 0
    EAL:   probe driver: 8086:10fb net_ixgbe
    ...

To run remote client (e.g. telnet) to communicate with the application:

.. code-block:: console

    $ telnet 0.0.0.0 8086

When running a telnet client as above, command prompt is displayed:

.. code-block:: console

    Trying 0.0.0.0...
    Connected to 0.0.0.0.
    Escape character is '^]'.

    Welcome!

    pipeline>

Once application and telnet client start running, messages can be sent from client to application.


Application stages
------------------

Initialization
~~~~~~~~~~~~~~

During this stage, EAL layer is initialised and application specific arguments are parsed. Furthermore, the data structures
for application objects are initialized. In case of any initialization error, an error message is displayed and the application
is terminated.

Run-time
~~~~~~~~

The main thread is creating and managing all the application objects based on CLI input.

Each data plane thread runs one or several pipelines previously assigned to it in round-robin order. Each data plane thread
executes two tasks in time-sharing mode:

1. *Packet processing task*: Process bursts of input packets read from the pipeline input ports.

2. *Message handling task*: Periodically, the data plane thread pauses the packet processing task and polls for request
   messages send by the main thread. Examples: add/remove pipeline to/from current data plane thread, add/delete rules
   to/from given table of a specific pipeline owned by the current data plane thread, read statistics, etc.
