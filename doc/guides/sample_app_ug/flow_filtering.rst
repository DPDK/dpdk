..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 Mellanox Technologies, Ltd

Basic RTE Flow Filtering Sample Application
===========================================

The Basic RTE flow filtering sample application is a simple example of a
creating a RTE flow rule.

It is intended as a demonstration of the basic components RTE flow rules.


Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.


Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    ./<build_dir>/examples/dpdk-flow_filtering -l 1 -n 1

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


Explanation
-----------

The example is built from 2 files,
``main.c`` which holds the example logic and ``flow_blocks.c`` that holds the
implementation for building the flow rule.

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function located in ``main.c`` file performs the initialization
and runs the main loop function.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Initialize EAL. 8<
    :end-before: >8 End of Initialization of EAL.
    :dedent: 1


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Allocates a mempool to hold the mbufs. 8<
    :end-before: >8 End of allocating a mempool to hold the mbufs.
    :dedent: 1

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``init_port()`` function which is explained in the next section:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Initializes all the ports using the user defined init_port(). 8<
    :end-before: >8 End of Initializing the ports using user defined init_port().
    :dedent: 1

Once the initialization is complete, we set the flow rule using the
following code:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Create flow for send packet with. 8<
    :end-before: >8 End of creating flow for send packet with.
    :dedent: 1

In the last part the application is ready to launch the
``main_loop()`` function. Which is explained below.


.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Launching main_loop(). 8<
    :end-before: >8 End of launching main_loop().
    :dedent: 1

The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the flow filtering
application is shown below:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Port initialization used in flow filtering. 8<
    :end-before: >8 End of Port initialization used in flow filtering.

The Ethernet port is configured with default settings using the
``rte_eth_dev_configure()`` function and the ``port_conf_default`` struct:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Ethernet port configured with default settings. 8<
    :end-before: >8 End of ethernet port configured with default settings.
    :dedent: 1

For this example we are configuring number of rx and tx queues that are connected
to a single port.

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Configuring number of RX and TX queues connected to single port. 8<
    :end-before: >8 End of Configuring RX and TX queues connected to single port.
    :dedent: 1

In the next step we create and apply the flow rule. which is to send packets
with destination ip equals to 192.168.1.1 to queue number 1. The detail
explanation of the ``generate_ipv4_flow()`` appears later in this document:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Create flow for send packet with. 8<
    :end-before: >8 End of create flow and the flow rule.
    :dedent: 1

We are setting the RX port to promiscuous mode:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Setting the RX port to promiscuous mode. 8<
    :end-before: >8 End of setting the RX port to promiscuous mode.
    :dedent: 1

The last step is to start the port.

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Starting the port. 8<
    :end-before: >8 End of starting the port.
    :dedent: 1


The main_loop function
~~~~~~~~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function to handle
the main loop. For the flow filtering application the main_loop function
looks like the following:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Main_loop for flow filtering. 8<
    :end-before: >8 End of reading the packets from all queues.

The main work of the application is reading the packets from all
queues and printing for each packet the destination queue:

.. literalinclude:: ../../../examples/flow_filtering/main.c
    :language: c
    :start-after: Reading the packets from all queues. 8<
    :end-before: >8 End of main_loop for flow filtering.


The forwarding loop can be interrupted and the application closed using
``Ctrl-C``. Which results in closing the port and the device using
``rte_eth_dev_stop`` and ``rte_eth_dev_close``

The generate_ipv4_flow function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The generate_ipv4_flow function is responsible for creating the flow rule.
This function is located in the ``flow_blocks.c`` file.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Function responsible for creating the flow rule. 8<
    :end-before: >8 End of function responsible for creating the flow rule.

The first part of the function is declaring the structures that will be used.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Declaring structs being used. 8<
    :end-before: >8 End of declaring structs being used.
    :dedent: 1

The following part create the flow attributes, in our case ingress.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Set the rule attribute, only ingress packets will be checked. 8<
    :end-before: >8 End of setting the rule attribute.
    :dedent: 1

The third part defines the action to be taken when a packet matches
the rule. In this case send the packet to queue.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Function responsible for creating the flow rule. 8<
    :end-before: >8 End of setting the rule attribute.

The fourth part is responsible for creating the pattern and is built from
number of steps. In each step we build one level of the pattern starting with
the lowest one.

Setting the first level of the pattern ETH:

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Set this level to allow all. 8<
    :end-before: >8 End of setting the first level of the pattern.
    :dedent: 1

Setting the second level of the pattern IP:

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Setting the second level of the pattern. 8<
    :end-before: >8 End of setting the second level of the pattern.
    :dedent: 1

Closing the pattern part.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: The final level must be always type end. 8<
    :end-before: >8 End of final level must be always type end.
    :dedent: 1

The last part of the function is to validate the rule and create it.

.. literalinclude:: ../../../examples/flow_filtering/flow_blocks.c
    :language: c
    :start-after: Validate the rule and create it. 8<
    :end-before: >8 End of validation the rule and create it.
    :dedent: 1
