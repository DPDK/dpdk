..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Basic Forwarding Sample Application
===================================

The Basic Forwarding sample application is a simple *skeleton* example of a
forwarding application.

It is intended as a demonstration of the basic components of a DPDK forwarding
application. For more detailed implementations see the L2 and L3 forwarding
sample applications.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``skeleton`` sub-directory.

Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    ./<build_dir>/examples/dpdk-skeleton -l 1 -n 4

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


Explanation
-----------

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

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
    :language: c
    :start-after: Initializion the Environment Abstraction Layer (EAL). 8<
    :end-before: >8 End of initialization the Environment Abstraction Layer (EAL).
    :dedent: 1


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
    :language: c
    :start-after: Allocates mempool to hold the mbufs. 8<
    :end-before: >8 End of allocating mempool to hold mbuf.
    :dedent: 1

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function which is explained in the next section:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
    :language: c
    :start-after: Initializing all ports. 8<
    :end-before: >8 End of initializing all ports.
    :dedent: 1

Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.


.. literalinclude:: ../../../examples/skeleton/basicfwd.c
    :language: c
    :start-after: Called on single lcore. 8<
    :end-before: >8 End of called on single lcore.
    :dedent: 1

The ``lcore_main()`` function is explained below.



The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the Basic
Forwarding application is shown below:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
    :language: c
    :start-after: Main functional part of port initialization. 8<
    :end-before: >8 End of main functional part of port initialization.

The Ethernet ports are configured with default settings using the
``rte_eth_dev_configure()`` function.

For this example the ports are set up with 1 RX and 1 TX queue using the
``rte_eth_rx_queue_setup()`` and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
        :language: c
        :start-after: Starting Ethernet port. 8<
        :end-before: >8 End of starting of ethernet port.
        :dedent: 1


Finally the RX port is set in promiscuous mode:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
        :language: c
        :start-after: Enable RX in promiscuous mode for the Ethernet device.
        :end-before: End of setting RX port in promiscuous mode.
        :dedent: 1


The Lcores Main
~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores. For the Basic Forwarding application the lcore function
looks like the following:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
        :language: c
        :start-after: Basic forwarding application lcore. 8<
        :end-before: >8 End Basic forwarding application lcore.

The main work of the application is done within the loop:

.. literalinclude:: ../../../examples/skeleton/basicfwd.c
        :language: c
        :start-after: Main work of application loop. 8<
        :end-before: >8 End of loop.
        :dedent: 1

Packets are received in bursts on the RX ports and transmitted in bursts on
the TX ports. The ports are grouped in pairs with a simple mapping scheme
using the an XOR on the port number::

    0 -> 1
    1 -> 0

    2 -> 3
    3 -> 2

    etc.

The ``rte_eth_tx_burst()`` function frees the memory buffers of packets that
are transmitted. If packets fail to transmit, ``(nb_tx < nb_rx)``, then they
must be freed explicitly using ``rte_pktmbuf_free()``.

The forwarding loop can be interrupted and the application closed using
``Ctrl-C``.
