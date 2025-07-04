..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Multi-process Sample Application
================================

This chapter describes example multi-processing applications that are included in the DPDK.

Example Applications
--------------------

Building the Sample Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The multi-process example applications are built the same way as other sample applications,
and as documented in the *DPDK Getting Started Guide*.


To compile the sample application see :doc:`compiling`.

The applications are located in the ``multi_process`` sub-directory.

.. note::

   If only a specific multi-process application needs to be built,
   the final make command can be run just in that application's directory.

Basic Multi-process Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``examples/simple_mp`` folder contains a basic example application
that demonstrates how two DPDK processes can work together
to share information using queues and memory pools.

Running the Application
^^^^^^^^^^^^^^^^^^^^^^^

To run the application, start ``simple_mp`` binary in one terminal,
passing at least two cores in the corelist:

.. code-block:: console

    ./<build_dir>/examples/dpdk-simple_mp -l 0-1 -n 4 --proc-type=primary

For the first DPDK process run, the proc-type flag can be omitted or set to auto,
since all DPDK processes will default to being a primary instance,
meaning they have control over the hugepage shared memory regions.
The process should start successfully and display a command prompt as follows:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-simple_mp -l 0-1 -n 4 --proc-type=primary
    ...
    EAL: Requesting 2 pages of size 1073741824
    EAL: Requesting 768 pages of size 2097152
    EAL: Ask a virtual area of 0x40000000 bytes
    EAL: Virtual area found at 0x7ff200000000 (size = 0x40000000)
    ...
    EAL: check module finished
    EAL: Main core 0 is ready (tid=54e41820)
    EAL: Core 1 is ready (tid=53b32700)

    Starting core 1

    simple_mp >

To run the secondary process to communicate with the primary process,
again run the same binary setting at least two cores in the corelist:

.. code-block:: console

    ./<build_dir>/examples/dpdk-simple_mp -l 2-3 -n 4 --proc-type=secondary

When running a secondary process such as above,
the ``proc-type`` parameter can be specified as auto.
Omitting the parameter will cause the process to try and start as a primary
rather than secondary process.

The process starts up, displaying similar status messages to the primary instance
as it initializes then prints a command prompt.

Once both processes are running, messages can be sent between them using the send command.
At any stage, either process can be terminated using the quit command.

.. code-block:: console

   EAL: Main core 10 is ready (tid=b5f89820)             EAL: Main core 8 is ready (tid=864a3820)
   EAL: Core 11 is ready (tid=84ffe700)                  EAL: Core 9 is ready (tid=85995700)
   Starting core 11                                      Starting core 9
   simple_mp > send hello_secondary                      simple_mp > core 9: Received 'hello_secondary'
   simple_mp > core 11: Received 'hello_primary'         simple_mp > send hello_primary
   simple_mp > quit                                      simple_mp > quit

.. note::

    If the primary instance is terminated, the secondary instance must also be shut-down and restarted after the primary.
    This is necessary because the primary instance will clear and reset the shared memory regions on startup,
    invalidating the secondary process's pointers.
    The secondary process can be stopped and restarted without affecting the primary process.

How the Application Works
^^^^^^^^^^^^^^^^^^^^^^^^^

This application uses two queues and a single memory pool created in the primary process.
The secondary process then uses lookup functions to attach to these objects.

.. literalinclude:: ../../../examples/multi_process/simple_mp/main.c
        :language: c
        :start-after: Start of ring structure. 8<
        :end-before: >8 End of ring structure.
        :dedent: 1

Note, however, that the named ring structure used as send_ring in the primary process is the recv_ring in the secondary process.

The application has two threads:

sender
   Reads from stdin, converts them to messages, and enqueues them to the ring.

receiver
   Dequeues any messages on the ring, prints them, then frees the buffer.

Symmetric Multi-process Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The symmetric multi process example demonstrates how a set of processes can run in parallel,
with each process performing the same set of packet- processing operations.
The following diagram shows the data-flow through the application, using two processes.

.. _figure_sym_multi_proc_app:

.. figure:: img/sym_multi_proc_app.*

   Example Data Flow in a Symmetric Multi-process Application

Each process reads packets from each of the network ports in use.
RSS distributes incoming packets on each port to different hardware Rx queues.
Each process reads a different RX queue on each port and so does not contend with any other process for that queue access.
Each process writes outgoing packets to a different Tx queue on each port.

Running the Application
^^^^^^^^^^^^^^^^^^^^^^^

The first instance of the ``symmetric_mp`` process is the primary instance, with the EAL arguments:

``-p <portmask>``
  The ``portmask`` is a hexadecimal bitmask of what ports on the system are to be used.
  For example: ``-p 3`` to use ports 0 and 1 only.

``--num-procs <N>``
  The ``N`` is the total number of ``symmetric_mp`` instances
  that will be run side-by-side to perform packet processing.
  This parameter is used to configure the appropriate number of receive queues on each network port.

``--proc-id <n>``
  Where ``n`` is a numeric value in the range ``0 <= n < N``
  (number of processes, specified above).
  This identifies which ``symmetric_mp`` instance is being run,
  so that each process can read a unique receive queue on each network port.

The secondary instance must be started with similar EAL parameters.
Example:

.. code-block:: console

    # ./<build_dir>/examples/dpdk-symmetric_mp -l 1 -n 4 --proc-type=auto -- -p 3 --num-procs=4 --proc-id=0
    # ./<build_dir>/examples/dpdk-symmetric_mp -l 2 -n 4 --proc-type=auto -- -p 3 --num-procs=4 --proc-id=1
    # ./<build_dir>/examples/dpdk-symmetric_mp -l 3 -n 4 --proc-type=auto -- -p 3 --num-procs=4 --proc-id=2
    # ./<build_dir>/examples/dpdk-symmetric_mp -l 4 -n 4 --proc-type=auto -- -p 3 --num-procs=4 --proc-id=3

.. note::

   In the above example, ``auto`` is used so the first instance becomes the primary process.

How the Application Works
^^^^^^^^^^^^^^^^^^^^^^^^^

The primary instance creates the memory pool and initializes the network ports.

.. literalinclude:: ../../../examples/multi_process/symmetric_mp/main.c
        :language: c
        :start-after: Primary instance initialized. 8<
        :end-before: >8 End of primary instance initialization.
        :dedent: 1

The secondary instance gets the port information and exported by the primary process.
The memory pool is accessed by doing a lookup for it by name:

.. code-block:: c

   if (proc_type == RTE_PROC_SECONDARY)
       mp = rte_mempool_lookup(_SMP_MBUF_POOL);
   else
       mp = rte_mempool_create(_SMP_MBUF_POOL, NB_MBUFS, MBUF_SIZE, ... )

The main loop of each process, both primary and secondary, is the same.
Each process reads from each port using the queue corresponding to its proc-id parameter,
and writes to the corresponding transmit queue on the output port.

Client-Server Multi-process Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The example multi-process application demonstrates a client-server type multi-process design.
A single server process receives a set of packets from the ports
and distributes these packets using round-robin ordering to the client processes,
Each client processes packets and does level-2 forwarding
by sending each packet out on a different network port.

The following diagram shows the data-flow through the application, using two client processes.

.. _figure_client_svr_sym_multi_proc_app:

.. figure:: img/client_svr_sym_multi_proc_app.*

   Example Data Flow in a Client-Server Symmetric Multi-process Application


Running the Application
^^^^^^^^^^^^^^^^^^^^^^^

The server process must be run as the primary process to set up all memory structures.
In addition to the EAL parameters, the application-specific parameters are:

*   -p <portmask >, where portmask is a hexadecimal bitmask of what ports on the system are to be used.
    For example: -p 3 to use ports 0 and 1 only.

*   -n <num-clients>, where the num-clients parameter is the number of client processes that will process the packets received
    by the server application.

.. note::

   In the server process, has a single thread using the lowest numbered lcore
   in the corelist, performs all packet I/O.
   If corelist parameter specifies with more than a single lcore,
   an additional lcore will be used for a thread to print packet count periodically.

The server application stores configuration data in shared memory,
including the network ports used.
The only application parameter needed by a client process is its client instance ID.
To run a server application on lcore 1 (with lcore 2 printing statistics)
along with two client processes running on lcores 3 and 4,
the commands are:

.. code-block:: console

    # ./<build_dir>/examples/dpdk-mp_server -l 1-2 -n 4 -- -p 3 -n 2
    # ./<build_dir>/examples/dpdk-mp_client -l 3 -n 4 --proc-type=auto -- -n 0
    # ./<build_dir>/examples/dpdk-mp_client -l 4 -n 4 --proc-type=auto -- -n 1

.. note::

    If the server application dies and needs to be restarted, all client applications also need to be restarted,
    as there is no support in the server application for it to run as a secondary process.
    Any client processes that need restarting can be restarted without affecting the server process.

How the Application Works
^^^^^^^^^^^^^^^^^^^^^^^^^

The server (primary) process performs the initialization of network port and data structure
and stores its port configuration data in a memory zone in hugepage shared memory.
The client process does not need the portmask parameter passed in via the command line.
The server process is the primary process, and the client processes are secondary processes.

The server operates by reading packets from each network port
and distributing those packets to the client queues.
The client reads from the ring and routes the packet to a different network port.
The routing used is very simple: all packets received on the first NIC port
are transmitted back out on the second port and vice versa.
Similarly, packets are routed between the 3rd and 4th network ports and so on.
