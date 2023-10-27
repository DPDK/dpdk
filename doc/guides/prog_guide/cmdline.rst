..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2023 Intel Corporation.

Command-line Library
====================

Since its earliest versions, DPDK has included a command-line library -
primarily for internal use by, for example, ``dpdk-testpmd`` and the ``dpdk-test`` binaries,
but the library is also exported on install and can be used by any end application.
This chapter covers the basics of the command-line library and how to use it in an application.

Library Features
----------------

The DPDK command-line library supports the following features:

* Tab-completion available for interactive terminal sessions

* Ability to read and process commands taken from an input file, e.g. startup script

* Parameterized commands able to take multiple parameters with different datatypes:

   * Strings
   * Signed/unsigned 16/32/64-bit integers
   * IP Addresses
   * Ethernet Addresses

* Ability to multiplex multiple commands to a single callback function

Adding Command-line to an Application
-------------------------------------

Adding a command-line instance to an application involves a number of coding steps.

#. Define the result structure for the command, specifying the command parameters

#. Provide an initializer for each field in the result

#. Define the callback function for the command

#. Provide a parse result structure instance for the command, linking the callback to the command

#. Add the parse result structure to a command-line context

#. Within your main application code, create a new command-line instance passing in the context.

The next few subsections will cover each of these steps in more detail,
working through an example to add two commands to a command-line instance.
Those two commands will be:

#. ``quit`` - as the name suggests, to close the application

#. ``show port stats <n>`` - to display on-screen the statistics for a given ethernet port

.. note::

   For further examples of use of the command-line, see
   :doc:`cmdline example application <../sample_app_ug/cmd_line>`

Defining Command Result Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first structure to be defined is the structure which will be created on successful parse of a command.
This structure contains one member field for each token, or word, in the command.
The simplest case is for a one-word command, like ``quit``.
For this, we only need to define a structure with a single string parameter to contain that word.

.. code-block:: c

   struct cmd_quit_result {
      cmdline_fixed_string_t quit;
   };

For readability, the name of the struct member should match that of the token in the command.

For our second command, we need a structure with four member fields in it,
as there are four words/tokens in our command.
The first three are strings, and the final one is a 16-bit numeric value.
The resulting struct looks like:

.. code-block:: c

   struct cmd_show_port_stats_result {
      cmdline_fixed_string_t show;
      cmdline_fixed_string_t port;
      cmdline_fixed_string_t stats;
      uint16_t n;
   };

As before, we choose names to match the tokens in the command.
Since our numeric parameter is a 16-bit value, we use ``uint16_t`` type for it.
Any of the standard sized integer types can be used as parameters, depending on the desired result.

Beyond the standard integer types,
the library also allows variable parameters to be of a number of other types,
as called out in the feature list above.

* For variable string parameters,
  the type should be ``cmdline_fixed_string_t`` - the same as for fixed tokens,
  but these will be initialized differently (as described below).

* For ethernet addresses use type ``struct rte_ether_addr``

* For IP addresses use type ``cmdline_ipaddr_t``

Providing Field Initializers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each field of our result structure needs an initializer.
For fixed string tokens, like "quit", "show" and "port", the initializer will be the string itself.

.. code-block:: c

   static cmdline_parse_token_string_t cmd_quit_quit_tok =
      TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

The convention for naming used here is to include the base name of the overall result structure -
``cmd_quit`` in this case,
as well as the name of the field within that structure - ``quit`` in this case, followed by ``_tok``.
(This is why there is a double ``quit`` in the name above).

This naming convention is seen in our second example,
which also demonstrates how to define a numeric initializer.


.. code-block:: c

   static cmdline_parse_token_string_t cmd_show_port_stats_show_tok =
      TOKEN_STRING_INITIALIZER(struct cmd_show_port_stats_result, show, "show");
   static cmdline_parse_token_string_t cmd_show_port_stats_port_tok =
      TOKEN_STRING_INITIALIZER(struct cmd_show_port_stats_result, port, "port");
   static cmdline_parse_token_string_t cmd_show_port_stats_stats_tok =
      TOKEN_STRING_INITIALIZER(struct cmd_show_port_stats_result, stats, "stats");
   static cmdline_parse_token_num_t cmd_show_port_stats_n_tok =
      TOKEN_NUM_INITIALIZER(struct cmd_show_port_stats_result, n, RTE_UINT16);

For variable string tokens, the same ``TOKEN_STRING_INITIALIZER`` macro should be used.
However, the final parameter should be ``NULL`` rather than a hard-coded token string.

For numeric parameters, the final parameter to the ``TOKEN_NUM_INITIALIZER`` macro should be the
cmdline type matching the variable type defined in the result structure,
e.g. RTE_UINT8, RTE_UINT32, etc.

For IP addresses, the macro ``TOKEN_IPADDR_INITIALIZER`` should be used.

For ethernet addresses, the macro ``TOKEN_ETHERADDR_INITIALIZER`` should be used.

Defining Callback Function
~~~~~~~~~~~~~~~~~~~~~~~~~~

For each command, we need to define a function to be called once the command has been recognised.
The callback function should have type:

.. code:: c

   void (*f)(void *, struct cmdline *, void *)

where the first parameter is a pointer to the result structure defined above,
the second parameter is the command-line instance,
and the final parameter is a user-defined pointer provided when we associate the callback with the command.
Most callback functions only use the first parameter, or none at all,
but the additional two parameters provide some extra flexibility,
to allow the callback to work with non-global state in your application.

For our two example commands, the relevant callback functions would look very similar in definition.
However, within the function body,
we assume that the user would need to reference the result structure to extract the port number in
the second case.

.. code:: c

   void
   cmd_quit_parsed(void *parsed_result, struct cmdline *cl, void *data)
   {
      quit = 1;
   }
   void
   cmd_show_port_stats_parsed(void *parsed_result, struct cmdline *cl, void *data)
   {
      struct cmd_show_port_stats_result *res = parsed_result;
      uint16_t port_id = res->n;
      ...
   }


Associating Callback and Command
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``cmdline_parse_inst_t`` type defines a "parse instance",
i.e. a sequence of tokens to be matched and then an associated function to be called.
Also included in the instance type are a field for help text for the command,
and any additional user-defined parameter to be passed to the callback functions referenced above.
For example, for our simple "quit" command:

.. code-block:: c

   static cmdline_parse_inst_t cmd_quit = {
       .f = cmd_quit_parsed,
       .data = NULL,
       .help_str = "Close the application",
       .tokens = {
           (void *)&cmd_quit_quit_tok,
           NULL
       }
   };

In this case, we firstly identify the callback function to be called,
then set the user-defined parameter to NULL,
provide a help message to be given, on request, to the user explaining the command,
before finally listing out the single token to be matched for this command instance.

For our second, port stats, example,
as well as making things a little more complicated by having multiple tokens to be matched,
we can also demonstrate passing in a parameter to the function.
Let us suppose that our application does not always use all the ports available to it,
but instead only uses a subset of the ports, stored in an array called ``active_ports``.
Our stats command, therefore, should only display stats for the currently in-use ports,
so we pass this ``active_ports`` array.
(For simplicity of illustration, we shall assume that the array uses a terminating marker,
e.g. -1 for the end of the port list, so we don't need to pass in a length parameter too.)

.. code-block:: c

   extern int16_t active_ports[];
   ...
   static cmdline_parse_inst_t cmd_show_port_stats = {
       .f = cmd_show_port_stats_parsed,
       .data = active_ports,
       .help_str = "Show statistics for active network ports",
       .tokens = {
           (void *)&cmd_show_port_stats_show_tok,
           (void *)&cmd_show_port_stats_port_tok,
           (void *)&cmd_show_port_stats_stats_tok,
           (void *)&cmd_show_port_stats_n_tok,
           NULL
       }
   };


Adding Command to Command-line Context
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that we have configured each individual command and callback,
we need to merge these into a single array of command-line "contexts".
This context array will be used to create the actual command-line instance in the application.
Thankfully, each context entry is the same as each parse instance,
so our array is defined by simply listing out the previously defined command parse instances.

.. code-block:: c

   static cmdline_parse_ctx_t ctx[] = {
       &cmd_quit,
       &cmd_show_port_stats,
       NULL
   };

The context list must be terminated by a NULL entry.

Creating a Command-line Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once we have our ``ctx`` variable defined,
we now just need to call the API to create the new command-line instance in our application.
The basic API is ``cmdline_new`` which will create an interactive command-line with all commands available.
However, if additional features for interactive use - such as tab-completion -
are desired, it is recommended that ``cmdline_new_stdin`` be used instead.

A pattern that can be used in applications is to use ``cmdline_new`` for processing any startup commands,
either from file or from the environment (as is done in the "dpdk-test" application),
and then using ``cmdline_stdin_new`` thereafter to handle the interactive part.
For example, to handle a startup file and then provide an interactive prompt:

.. code-block:: c

   struct cmdline *cl;
   int fd = open(startup_file, O_RDONLY);

   if (fd >= 0) {
       cl = cmdline_new(ctx, "", fd, STDOUT_FILENO);
       if (cl == NULL) {
           /* error handling */
       }
       cmdline_interact(cl);
       cmdline_quit(cl);
       close(fd);
   }

   cl = cmdline_stdin_new(ctx, "Proxy>> ");
   if (cl == NULL) {
       /* error handling */
   }
   cmdline_interact(cl);
   cmdline_stdin_exit(cl);


Multiplexing Multiple Commands to a Single Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To reduce the amount of boiler-plate code needed when creating a command-line for an application,
it is possible to merge a number of commands together to have them call a separate function.
This can be done in a number of different ways:

* A callback function can be used as the target for a number of different commands.
  Which command was used for entry to the function can be determined by examining the first parameter,
  ``parsed_result`` in our examples above.

* For simple string commands, multiple options can be concatenated using the "#" character.
  For example: ``exit#quit``, specified as a token initializer,
  will match either on the string "exit" or the string "quit".

As a concrete example,
these two techniques are used in the DPDK unit test application ``dpdk-test``,
where a single command ``cmdline_parse_t`` instance is used for all the "dump_<item>" test cases.

.. literalinclude:: ../../../app/test/commands.c
    :language: c
    :start-after: Add the dump_* tests cases 8<
    :end-before: >8 End of add the dump_* tests cases


Examples of Command-line Use in DPDK
------------------------------------

To help the user follow the steps provided above,
the following DPDK files can be consulted for examples of command-line use.

.. note::

   This is not an exhaustive list of examples of command-line use in DPDK.
   It is simply a list of a few files that may be of use to the application developer.
   Some of these referenced files contain more complex examples of use that others.

* ``commands.c/.h`` in ``examples/cmdline``

* ``mp_commands.c/.h`` in ``examples/multi_process/simple_mp``

* ``commands.c`` in ``app/test``
