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

    $ ./build/cmdline -l 0-3 -n 4

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

EAL Initialization and cmdline Start
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first task is the initialization of the Environment Abstraction Layer (EAL).
This is achieved as follows:

.. code-block:: c

    int main(int argc, char **argv)
    {
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
            rte_panic("Cannot init EAL\n");

Then, a new command line object is created and started to interact with the user through the console:

.. code-block:: c

    cl = cmdline_stdin_new(main_ctx, "example> ");
    cmdline_interact(cl);
    cmdline_stdin_exit(cl);

The cmd line_interact() function returns when the user types **Ctrl-d** and in this case,
the application exits.

Defining a cmdline Context
~~~~~~~~~~~~~~~~~~~~~~~~~~

A cmdline context is a list of commands that are listed in a NULL-terminated table, for example:

.. code-block:: c

    cmdline_parse_ctx_t main_ctx[] = {
        (cmdline_parse_inst_t *) &cmd_obj_del_show,
        (cmdline_parse_inst_t *) &cmd_obj_add,
        (cmdline_parse_inst_t *) &cmd_help,
         NULL,
    };

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

.. code-block:: c

    struct cmd_obj_add_result {
        cmdline_fixed_string_t action;
        cmdline_fixed_string_t name;
        struct object *obj;
    };

    static void cmd_obj_del_show_parsed(void *parsed_result, struct cmdline *cl, attribute ((unused)) void *data)
    {
       /* ... */
    }

    cmdline_parse_token_string_t cmd_obj_action = TOKEN_STRING_INITIALIZER(struct cmd_obj_del_show_result, action, "show#del");

    parse_token_obj_list_t cmd_obj_obj = TOKEN_OBJ_LIST_INITIALIZER(struct cmd_obj_del_show_result, obj, &global_obj_list);

    cmdline_parse_inst_t cmd_obj_del_show = {
        .f = cmd_obj_del_show_parsed, /* function to call */
        .data = NULL,  /* 2nd arg of func */
        .help_str = "Show/del an object",
        .tokens = { /* token list, NULL terminated */
            (void *)&cmd_obj_action,
            (void *)&cmd_obj_obj,
             NULL,
        },
    };

This command is composed of two tokens:

*   The first token is a string token that can be show or del.

*   The second token is an object that was previously added using the add command in the global_obj_list variable.

Once the command is parsed, the rte_cmdline application fills a cmd_obj_del_show_result structure.
A pointer to this structure is given as an argument to the callback function and can be used in the body of this function.
