..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 Mellanox Technologies, Ltd

Flow Filtering Sample Application
=================================

Overview
--------

The flow filtering sample application provides a simple example of creating flow rules.

It serves as a demonstration of the fundamental components of flow rules.

It demonstrates how to create rules and configure them, using both template and non template API.


Compiling the Application
-------------------------

To compile the sample application, see :doc:`compiling`.

The application is located in the ``flow_filtering`` sub-directory.


Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

   dpdk-flow_filtering -n <number of channels> -a <pci_dev>,dv_flow_en=<1|2> -- [--[non-]template]

where,

``--[non-]template``
  Specify whether to use the template API (default is template API).

For more details on template API please refer to :ref:`flow_template_api`.

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


Structure
---------

The example is built from 2 main files:

- ``main.c``: Contains the application logic, including initializations and the main loop.
- ``flow_skeleton.c``: Implements the creation of flow rules.

Additionally, the ``snippets`` directory includes code snippets showcasing various features
that can override the basic ``flow_skeleton.c`` implementation.


Application Flow
----------------

Initialization
~~~~~~~~~~~~~~

Begin by setting up the Environment Abstraction Layer (EAL) using ``rte_eal_init()``.
This function initializes EAL with arguments ``argc`` and ``argv``,
returning the number of parsed arguments:

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Initialize EAL. 8<
   :end-before: >8 End of Initialization of EAL.
   :dedent: 1

Allocate a memory pool for managing mbufs used within the application:

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Allocates a mempool to hold the mbufs. 8<
   :end-before: >8 End of allocating a mempool to hold the mbufs.
   :dedent: 1

Some snippets may require different configuration of the port and flow attributes,
those configuration are defined in the snippet file.

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Add snippet-specific configuration. 8<
   :end-before: >8 End of snippet-specific configuration.
   :dedent: 1

Initialize the ports using the user-defined ``init_port()`` function,
configuring Ethernet ports with default settings, including both Rx and Tx queues for a single port:

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Initializes all the ports using the user defined init_port(). 8<
   :end-before: >8 End of Initializing the ports using user defined init_port().
   :dedent: 1

For template API, the flow API requires preallocating resources.
The function ``rte_flow_configure()`` should be called after configuring the Ethernet device
and before creating any flow rules to set up flow queues for asynchronous operations.

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Adds rules engine configuration. 8<
   :end-before: >8 End of adding rules engine configuration.
   :dedent: 1

Creating the Flow Rule
~~~~~~~~~~~~~~~~~~~~~~

This section is the core of the flow filtering functionality involves creating flow rules.
The flow rules are created using two primary approaches: template API and non-template API.
Both template and non-template API configure flow rules using attributes (like ingress or egress),
pattern items (for matching packet data), and actions (for operations on matched packets).
However, template API extend this by introducing pattern templates and actions templates,
which define reusable matching criteria and action lists, respectively.
These templates are then combined in a template table to optimize resource allocation and management.
In contrast, non-template API handle each rule individually without such shared templates.

This is handled by the ``generate_flow_skeleton()`` function in ``flow_skeleton.c``.

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Function responsible for creating the flow rule. 8<
   :end-before: >8 End of function responsible for creating the flow rule.
   :dedent: 1

This part of the code defines necessary data structures,
as well as configures action and pattern structures for the rule.
Common for both template and non-template API.

.. literalinclude:: ../../../examples/flow_filtering/flow_skeleton.c
   :language: c
   :start-after: Set the common action and pattern structures 8<
   :end-before: >8 End of setting the common action and pattern structures.
   :dedent: 1

For template API, this part of the code creates the necessary template tables and finally create the rule.

.. literalinclude:: ../../../examples/flow_filtering/flow_skeleton.c
   :language: c
   :start-after: Create a flow rule using template API 8<
   :end-before: >8 End of creating a flow rule using template API.
   :dedent: 1

For non-template API, validate the rule and create it.

.. literalinclude:: ../../../examples/flow_filtering/flow_skeleton.c
   :language: c
   :start-after: Validate and create the rule 8<
   :end-before: >8 End of validating and creating the rule.
   :dedent: 1

Main Loop Execution
~~~~~~~~~~~~~~~~~~~

Launch the ``main_loop()`` function from ``main.c``,
which reading the packets from all queues and printing for each packet the destination queue:

.. literalinclude:: ../../../examples/flow_filtering/main.c
   :language: c
   :start-after: Launching main_loop(). 8<
   :end-before: >8 End of launching main_loop().
   :dedent: 1

Exiting the Application
~~~~~~~~~~~~~~~~~~~~~~~

To terminate the application, use ``Ctrl-C``.
This action closes the port and device using ``rte_eth_dev_stop`` and ``rte_eth_dev_close``.


Flow API Snippets
------------------

The ``snippets`` directory offers additional customization options through code snippets.
These snippets cover various aspects of flow configuration, allowing developers to reuse them.

These snippets are categorized by usage and can be copied, pasted, and modified as needed.
They are maintained and compiled alongside other examples, ensuring up-to-date functionality.


Using Snippets
--------------

Developers can customize flow rules by modifying ``flow_skeleton.c``
and utilizing functions from ``snippets`` directory.
For example, within ``snippet_match_ipv4_flow.c``, developers can find the functions:

- ``snippet_ipv4_flow_create_actions()`` for defining actions,
- ``snippet_ipv4_flow_create_patterns()`` for setting packet matching patterns,
- ``snippet_ipv4_flow_create_table()`` for creating the patterns and actions template table.

To use a different snippet, simply update the include statement in ``flow_skeleton.c``
to point to the desired snippet file, this will change the default created flow.

Some snippets may require different configuration,
those configuration are defined in the snippet file:

- ``snippet_init_ipv4`` for configuration of the port and flow attributes.

In order to use them the developer should include the snippet header file in main.c
