..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Flow Classify Sample Application
================================

The Flow Classify sample application is based on the simple *skeleton* example
of a forwarding application.

It is intended as a demonstration of the basic components of a DPDK forwarding
application which uses the Flow Classify library API's.

Please refer to the
:doc:`../prog_guide/flow_classify_lib`
for more information.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``flow_classify`` sub-directory.

Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    ./<build_dir>/examples/dpdk-flow_classify -c 4 -n 4 -- /
    --rule_ipv4="../ipv4_rules_file.txt"

Please refer to the *DPDK Getting Started Guide*, section
:doc:`../linux_gsg/build_sample_apps`
for general information on running applications and the Environment Abstraction
Layer (EAL) options.


Sample ipv4_rules_file.txt
--------------------------

.. code-block:: console

    #file format:
    #src_ip/masklen dst_ip/masklen src_port : mask dst_port : mask proto/mask priority
    #
    2.2.2.3/24 2.2.2.7/24 32 : 0xffff 33 : 0xffff 17/0xff 0
    9.9.9.3/24 9.9.9.7/24 32 : 0xffff 33 : 0xffff 17/0xff 1
    9.9.9.3/24 9.9.9.7/24 32 : 0xffff 33 : 0xffff 6/0xff 2
    9.9.8.3/24 9.9.8.7/24 32 : 0xffff 33 : 0xffff 6/0xff 3
    6.7.8.9/24 2.3.4.5/24 32 : 0x0000 33 : 0x0000 132/0xff 4

Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.

ACL field definitions for the IPv4 5 tuple rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following field definitions are used when creating the ACL table during
initialisation of the ``Flow Classify`` application

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Creation of ACL table during initialization of application. 8<
    :end-before: >8 End of creation of ACL table.

The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).
The ``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Initialize the Environment Abstraction Layer (EAL). 8<
    :end-before: >8 End of initialization of EAL.
    :dedent: 1

It then parses the flow_classify application arguments

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Parse application arguments (after the EAL ones). 8<
    :end-before: >8 End of parse application arguments.
    :dedent: 1

The ``main()`` function also allocates a mempool to hold the mbufs
(Message Buffers) used by the application:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Creates a new mempool in memory to hold the mbufs. 8<
    :end-before: >8 End of creation of new mempool in memory.
    :dedent: 1

mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function which is explained in the next section:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Initialize all ports. 8<
    :end-before: >8 End of initialization of all ports.
    :dedent: 1

The ``main()`` function creates the ``flow classifier object`` and adds an ``ACL
table`` to the flow classifier.

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Creation of flow classifier object. 8<
    :end-before: >8 End of creation of flow classifier object.

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Memory allocation. 8<
    :end-before: >8 End of initialization of table create params.
    :dedent: 1

It then reads the ipv4_rules_file.txt file and initialises the parameters for
the ``rte_flow_classify_table_entry_add`` API.
This API adds a rule to the ACL table.

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Read file of IPv4 tuple rules. 8<
    :end-before: >8 End of reading file of IPv4 5 tuple rules.
    :dedent: 1

Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.

.. code-block:: c

    lcore_main(cls_app);

The ``lcore_main()`` function is explained below.

The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the Basic
Forwarding application is shown below:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Initializing port using global settings. 8<
    :end-before: >8 End of initializing a given port.

The Ethernet ports are configured with default settings using the
``rte_eth_dev_configure()`` function.

For this example the ports are set up with 1 RX and 1 TX queue using the
``rte_eth_rx_queue_setup()`` and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Start the Ethernet port. 8<
    :end-before: >8 End of starting the Ethernet port.
    :dedent: 1


Finally the RX port is set in promiscuous mode:

.. code-block:: c

    retval = rte_eth_promiscuous_enable(port);

The Add Rules function
~~~~~~~~~~~~~~~~~~~~~~

The ``add_rules`` function reads the ``ipv4_rules_file.txt`` file and calls the
``add_classify_rule`` function which calls the
``rte_flow_classify_table_entry_add`` API.

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Reads file and calls the add_classify_rule function. 8<
    :end-before: >8 End of add_rules.


The Lcore Main function
~~~~~~~~~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores.
The ``lcore_main`` function calls the ``rte_flow_classifier_query`` API.
For the Basic Forwarding application the ``lcore_main`` function looks like the
following:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Flow classify data. 8<
    :end-before: >8 End of flow classify data.

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Classifying the packets. 8<
    :end-before: >8 End of lcore main.

The main work of the application is done within the loop:

.. literalinclude:: ../../../examples/flow_classify/flow_classify.c
    :language: c
    :start-after: Run until the application is quit or killed. 8<
    :end-before: >8 End of main loop.
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
