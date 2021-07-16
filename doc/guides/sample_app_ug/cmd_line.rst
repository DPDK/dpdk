..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Command Line Sample Application
===============================

This chapter describes the Command Line sample application that
is part of the Data Plane Development Kit (DPDK).

Overview
--------

The Command Line sample application is a simple application that
demonstrates the use of the command line interface in the DPDK.
This application is a readline-like interface that can be used
to debug a DPDK application, in a Linux* application environment.

.. note::

    The rte_cmdline library should not be used in production code since
    it is not validated to the same standard as other DPDK libraries.
    See also the "rte_cmdline library should not be used in production code due to limited testing" item
    in the "Known Issues" section of the Release Notes.

The Command Line sample application supports some of the features of the GNU readline library such as, completion,
cut/paste and some other special bindings that make configuration and debug faster and easier.

The application shows how the rte_cmdline application can be extended to handle a list of objects.
There are three simple commands:

*   add obj_name IP: Add a new object with an IP/IPv6 address associated to it.

*   del obj_name: Delete the specified object.

*   show obj_name: Show the IP associated with the specified object.

.. note::

    To terminate the application, use **Ctrl-d**.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`

The application is located in the ``cmd_line`` sub-directory.

Running the Application
-----------------------

To run the application in linux environment, issue the following command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-cmdline -l 0-3 -n 4

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

EAL Initialization and cmdline Start
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first task is the initialization of the Environment Abstraction Layer (EAL).
This is achieved as follows:

.. literalinclude:: ../../../examples/cmdline/main.c
    :language: c
    :start-after: Initialization of the Environment Abstraction Layer (EAL). 8<
    :end-before: >8 End of initialization of Environment Abstraction Layer (EAL).

Then, a new command line object is created and started to interact with the user through the console:

.. literalinclude:: ../../../examples/cmdline/main.c
    :language: c
    :start-after: Creating a new command line object. 8<
    :end-before: >8 End of creating a new command line object.
    :dedent: 1

The cmd line_interact() function returns when the user types **Ctrl-d** and in this case,
the application exits.

Defining a cmdline Context
~~~~~~~~~~~~~~~~~~~~~~~~~~

A cmdline context is a list of commands that are listed in a NULL-terminated table, for example:

.. literalinclude:: ../../../examples/cmdline/commands.c
    :language: c
    :start-after: Cmdline context list of commands in NULL-terminated table. 8<
    :end-before: >8 End of context list.

Each command (of type cmdline_parse_inst_t) is defined statically.
It contains a pointer to a callback function that is executed when the command is parsed,
an opaque pointer, a help string and a list of tokens in a NULL-terminated table.

The rte_cmdline application provides a list of pre-defined token types:

*   String Token: Match a static string, a list of static strings or any string.

*   Number Token: Match a number that can be signed or unsigned, from 8-bit to 32-bit.

*   IP Address Token: Match an IPv4 or IPv6 address or network.

*   Ethernet* Address Token: Match a MAC address.

In this example, a new token type obj_list is defined and implemented
in the parse_obj_list.c and parse_obj_list.h files.

For example, the cmd_obj_del_show command is defined as shown below:

.. literalinclude:: ../../../examples/cmdline/commands.c
    :language: c
    :start-after: Show or delete tokens. 8<
    :end-before: >8 End of show or delete tokens.

This command is composed of two tokens:

*   The first token is a string token that can be show or del.

*   The second token is an object that was previously added using the add command in the global_obj_list variable.

Once the command is parsed, the rte_cmdline application fills a cmd_obj_del_show_result structure.
A pointer to this structure is given as an argument to the callback function and can be used in the body of this function.
