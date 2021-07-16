..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

PTP Client Sample Application
=============================

The PTP (Precision Time Protocol) client sample application is a simple
example of using the DPDK IEEE1588 API to communicate with a PTP master clock
to synchronize the time on the NIC and, optionally, on the Linux system.

Note, PTP is a time syncing protocol and cannot be used within DPDK as a
time-stamping mechanism. See the following for an explanation of the protocol:
`Precision Time Protocol
<https://en.wikipedia.org/wiki/Precision_Time_Protocol>`_.


Limitations
-----------

The PTP sample application is intended as a simple reference implementation of
a PTP client using the DPDK IEEE1588 API.
In order to keep the application simple the following assumptions are made:

* The first discovered master is the main for the session.
* Only L2 PTP packets are supported.
* Only the PTP v2 protocol is supported.
* Only the slave clock is implemented.


How the Application Works
-------------------------

.. _figure_ptpclient_highlevel:

.. figure:: img/ptpclient.*

   PTP Synchronization Protocol

The PTP synchronization in the sample application works as follows:

* Master sends *Sync* message - the slave saves it as T2.
* Master sends *Follow Up* message and sends time of T1.
* Slave sends *Delay Request* frame to PTP Master and stores T3.
* Master sends *Delay Response* T4 time which is time of received T3.

The adjustment for slave can be represented as:

   adj = -[(T2-T1)-(T4 - T3)]/2

If the command line parameter ``-T 1`` is used the application also
synchronizes the PTP PHC clock with the Linux kernel clock.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ptpclient`` sub-directory.


Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    ./<build_dir>/examples/dpdk-ptpclient -l 1 -n 4 -- -p 0x1 -T 0

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

* ``-p portmask``: Hexadecimal portmask.
* ``-T 0``: Update only the PTP slave clock.
* ``-T 1``: Update the PTP slave clock and synchronize the Linux Kernel to the PTP clock.


Code Explanation
----------------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Initialize the Environment Abstraction Layer (EAL). 8<
    :end-before: >8 End of initialization of EAL.
    :dedent: 1

And than we parse application specific arguments

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Parse specific arguments. 8<
    :end-before: >8 End of parsing specific arguments.
    :dedent: 1

The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Creates a new mempool in memory to hold the mbufs. 8<
    :end-before:  >8 End of a new mempool in memory to hold the mbufs.
    :dedent: 1

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function with portmask provided by user:

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Initialize all ports. 8<
    :end-before: >8 End of initialization of all ports.
    :dedent: 1


Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.

.. code-block:: c

	lcore_main();

The ``lcore_main()`` function is explained below.


The Lcores Main
~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores.

The main work of the application is done within the loop:

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Read packet from RX queues. 8<
    :end-before: >8 End of read packets from RX queues.
    :dedent: 2

Packets are received one by one on the RX ports and, if required, PTP response
packets are transmitted on the TX ports.

If the offload flags in the mbuf indicate that the packet is a PTP packet then
the packet is parsed to determine which type:

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Packet is parsed to determine which type. 8<
    :end-before: >8 End of packet is parsed to determine which type.
    :dedent: 3


All packets are freed explicitly using ``rte_pktmbuf_free()``.

The forwarding loop can be interrupted and the application closed using
``Ctrl-C``.


PTP parsing
~~~~~~~~~~~

The ``parse_ptp_frames()`` function processes PTP packets, implementing slave
PTP IEEE1588 L2 functionality.

.. literalinclude:: ../../../examples/ptpclient/ptpclient.c
    :language: c
    :start-after: Parse ptp frames. 8<
    :end-before:  >8 End of function processes PTP packets.

There are 3 types of packets on the RX path which we must parse to create a minimal
implementation of the PTP slave client:

* SYNC packet.
* FOLLOW UP packet
* DELAY RESPONSE packet.

When we parse the *FOLLOW UP* packet we also create and send a *DELAY_REQUEST* packet.
Also when we parse the *DELAY RESPONSE* packet, and all conditions are met we adjust the PTP slave clock.
