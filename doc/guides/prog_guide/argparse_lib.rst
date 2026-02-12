.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 HiSilicon Limited

Argparse Library
================

The argparse library provides argument parsing functionality,
this library makes it easy to write user-friendly command-line program.

Features and Capabilities
-------------------------

- Support parsing optional argument (which could take with no-value,
  required-value and optional-value).

- Support parsing positional argument (which must take with required-value).

- Support getopt-style argument reordering for non-flag arguments as an alternative to positional arguments.

- Support automatic generate usage information.

- Support issue errors when provide with invalid arguments.

- Support parsing argument by two ways:

  #. autosave: used for parsing known value types;
  #. callback: will invoke user callback to parse.

- Supports automatic help and usage information.

Usage Guide
-----------

The following code demonstrates how to use:

.. code-block:: C

   static int
   argparse_user_callback(uint32_t index, const char *value, void *opaque)
   {
      if (index == 1) {
         /* process "--ddd" argument, because it is configured as no-value,
          * the parameter 'value' is NULL.
          */
         ...
      } else if (index == 2) {
         /* process "--eee" argument, because it is configured as
          * required-value, the parameter 'value' must not be NULL.
          */
         ...
      } else if (index == 3) {
         /* process "--fff" argument, because it is configured as
          * optional-value, the parameter 'value' maybe NULL or not NULL,
          * depend on input.
          */
         ...
      } else if (index == 300) {
         /* process "ppp" argument, because it's a positional argument,
          * the parameter 'value' must not be NULL.
          */
         ...
      } else {
         return -EINVAL;
      }
   }

   static int aaa_val, bbb_val, ccc_val, ooo_val;

   static struct rte_argparse obj = {
      .prog_name = "test-demo",
      .usage = "[EAL options] -- [optional parameters] [positional parameters]",
      .descriptor = NULL,
      .epilog = NULL,
      .exit_on_error = true,
      .callback = argparse_user_callback,
      .args = {
         { "--aaa", "-a", "aaa argument", &aaa_val, (void *)100, RTE_ARGPARSE_VALUE_NONE,     RTE_ARGPARSE_VALUE_TYPE_INT },
         { "--bbb", "-b", "bbb argument", &bbb_val, NULL,        RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_INT },
         { "--ccc", "-c", "ccc argument", &ccc_val, (void *)200, RTE_ARGPARSE_VALUE_OPTIONAL, RTE_ARGPARSE_VALUE_TYPE_INT },
         { "--ddd", "-d", "ddd argument", NULL,     (void *)1,   RTE_ARGPARSE_VALUE_NONE },
         { "--eee", "-e", "eee argument", NULL,     (void *)2,   RTE_ARGPARSE_VALUE_REQUIRED },
         { "--fff", "-f", "fff argument", NULL,     (void *)3,   RTE_ARGPARSE_VALUE_OPTIONAL },
         { "ooo",   NULL, "ooo argument", &ooo_val, NULL,        RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_INT },
         { "ppp",   NULL, "ppp argument", NULL,     (void *)300, RTE_ARGPARSE_VALUE_REQUIRED },
      },
   };

   int
   main(int argc, char **argv)
   {
      ...
      ret = rte_argparse_parse(&obj, argc, argv);
      ...
   }

In this example, the arguments which start with a hyphen (-) are optional
arguments (they're ``--aaa``/``--bbb``/``--ccc``/``--ddd``/``--eee``/``--fff``);
and the arguments which don't start with a hyphen (-) are positional arguments
(they're ``ooo``/``ppp``).

Every argument must be set whether to carry a value (one of
``RTE_ARGPARSE_VALUE_NONE``, ``RTE_ARGPARSE_VALUE_REQUIRED`` and
``RTE_ARGPARSE_VALUE_OPTIONAL``).

.. note::

   Positional argument must set ``RTE_ARGPARSE_VALUE_REQUIRED``.

User Input Requirements
~~~~~~~~~~~~~~~~~~~~~~~

For optional arguments which take no-value,
the following mode is supported (take above ``--aaa`` as an example):

- The single mode: ``--aaa`` or ``-a``.

For optional arguments which take required-value,
the following two modes are supported (take above ``--bbb`` as an example):

- The kv mode: ``--bbb=1234`` or ``-b=1234`` or ``-b1234``.

- The split mode: ``--bbb 1234`` or ``-b 1234`` or ``-b1234``.

For optional arguments which take optional-value,
the following two modes are supported (take above ``--ccc`` as an example):

- The single mode: ``--ccc`` or ``-c``.

- The kv mode: ``--ccc=123`` or ``-c=123`` or ``-c123```.

For positional arguments which must take required-value,
their values are parsing in the order defined.

.. note::

   The compact mode is not supported.
   Take above ``-a`` and ``-d`` as an example, don't support ``-ad`` input.

Parsing by autosave way
~~~~~~~~~~~~~~~~~~~~~~~

Argument of known value type (e.g. ``RTE_ARGPARSE_VALUE_TYPE_INT``)
could be parsed using this autosave way,
and its result will save in the ``val_saver`` field.

In the above example, the arguments ``--aaa``/``--bbb``/``--ccc`` and ``ooo``
both use this way, the parsing is as follows:

- For argument ``--aaa``, it is configured as no-value,
  so the ``aaa_val`` will be set to ``val_set`` field
  which is 100 in the above example.

- For argument ``--bbb``, it is configured as required-value,
  so the ``bbb_val`` will be set to user input's value
  (e.g. will be set to 1234 with input ``--bbb 1234``).

- For argument ``--ccc``, it is configured as optional-value,
  if user only input ``--ccc`` then the ``ccc_val`` will be set to ``val_set``
  field which is 200 in the above example;
  if user input ``--ccc=123``, then the ``ccc_val`` will be set to 123.

- For argument ``ooo``, it is positional argument,
  the ``ooo_val`` will be set to user input's value.

Support of non-flag/positional arguments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For arguments which are not flags (i.e. don't start with a hyphen '-'),
there are two ways in which they can be handled by the library:

#. Positional arguments: these are defined in the ``args`` array with a NULL ``short_name`` field,
   and long_name field that does not start with a hyphen '-'.
   They are parsed as required-value arguments.

#. As ignored, or unhandled arguments: if the ``ignore_non_flag_args`` field in the ``rte_argparse`` object is set to true,
   then any non-flag arguments will be ignored by the parser and moved to the end of the argument list.
   In this mode, no positional arguments are allowed.
   The return value from ``rte_argparse_parse()`` will indicate the position of the first ignored non-flag argument.

Supported Value Types
~~~~~~~~~~~~~~~~~~~~~

The argparse library supports automatic parsing of several data types when using
the autosave method. The parsed values are automatically converted from string
input to the appropriate data type and stored in the ``val_saver`` field.

Integer Types
^^^^^^^^^^^^^

The library supports parsing various integer types:

- ``RTE_ARGPARSE_VALUE_TYPE_INT`` - signed integer
- ``RTE_ARGPARSE_VALUE_TYPE_U8`` - unsigned 8-bit integer
- ``RTE_ARGPARSE_VALUE_TYPE_U16`` - unsigned 16-bit integer
- ``RTE_ARGPARSE_VALUE_TYPE_U32`` - unsigned 32-bit integer
- ``RTE_ARGPARSE_VALUE_TYPE_U64`` - unsigned 64-bit integer

.. code-block:: C

   static int my_int;
   static uint16_t my_port;
   static uint32_t my_count;

   static struct rte_argparse obj = {
      .args = {
         { "--number", "-n", "Integer value", &my_int, NULL, RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_INT },
         { "--port", "-p", "Port number", &my_port, NULL, RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_U16 },
         { "--count", "-c", "Count value", &my_count, (void *)1000, RTE_ARGPARSE_VALUE_OPTIONAL, RTE_ARGPARSE_VALUE_TYPE_U32 },
         ARGPARSE_ARG_END(),
      },
   };

String Type
^^^^^^^^^^^

String arguments are parsed using ``RTE_ARGPARSE_VALUE_TYPE_STR``.
When using this type, the input value is saved to the provided pointer without any parsing or validation.

.. code-block:: C

   static const char *my_string;

   static struct rte_argparse obj = {
      .args = {
         { "--name", "-n", "Name string", &my_string, NULL, RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_STR },
         ARGPARSE_ARG_END(),
      },
   };

Boolean Type
^^^^^^^^^^^^

Boolean arguments are parsed using ``RTE_ARGPARSE_VALUE_TYPE_BOOL`` and accept the following input formats:

- ``true``, ``false`` (case-sensitive)
- ``1``, ``0`` (numeric format)

.. code-block:: C

   static bool my_flag;

   static struct rte_argparse obj = {
      .args = {
         { "--enable", "-e", "Enable feature", &my_flag, NULL, RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_BOOL },
         ARGPARSE_ARG_END(),
      },
   };

Corelist Type
^^^^^^^^^^^^^

The argparse library supports automatic parsing of CPU core lists using the
``RTE_ARGPARSE_VALUE_TYPE_CORELIST`` value type. This feature allows users to
specify CPU cores in a flexible format similar to other DPDK applications.

.. code-block:: C

   #include <rte_os.h>  /* for CPU set operations */

   static rte_cpuset_t cores;

   static struct rte_argparse obj = {
      .args = {
         { "--cores", "-c", "CPU cores to use", &cores, NULL, RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_CORELIST },
         ARGPARSE_ARG_END(),
      },
   };

The corelist parsing supports the following input formats:

- **Single core**: ``--cores 5`` (sets core 5)
- **Multiple cores**: ``--cores 1,2,5`` (sets cores 1, 2, and 5)
- **Core ranges**: ``--cores 1-5`` (sets cores 1, 2, 3, 4, and 5)
- **Mixed format**: ``--cores 0,2-4,7`` (sets cores 0, 2, 3, 4, and 7)
- **Reverse ranges**: ``--cores 5-1`` (equivalent to 1-5, sets cores 1, 2, 3, 4, and 5)
- **Empty corelist**: ``--cores ""`` (sets no cores)

The parsed result is stored in an ``rte_cpuset_t`` structure that can be used
with standard CPU set operations.

Parsing by callback way
~~~~~~~~~~~~~~~~~~~~~~~

It could also choose to use callback to parse,
just define a unique index for the argument
and make the ``val_save`` field to be NULL also zero value-type.

In the example at the top of this section,
the arguments ``--ddd``/``--eee``/``--fff`` and ``ppp`` all use this way.

Multiple times argument
~~~~~~~~~~~~~~~~~~~~~~~

If want to support the ability to enter the same argument multiple times,
then should mark ``RTE_ARGPARSE_FLAG_SUPPORT_MULTI`` in the ``flags`` field.
For example:

.. code-block:: C

   {
      .long_name = "--xyz",
      .short_name = "-x",
      .desc = "xyz argument",
      .val_set = (void *)10,
      .val_mode = RTE_ARGPARSE_VALUE_REQUIRED,
      // val_type is implicitly RTE_ARGPARSE_VALUE_TYPE_NONE,
      .flags = RTE_ARGPARSE_FLAG_SUPPORT_MULTI,
   },

Then the user input could contain multiple ``--xyz`` arguments.

.. note::

   The multiple times argument only support with optional argument
   and must be parsed by callback way.

Help and Usage Information
~~~~~~~~~~~~~~~~~~~~~~~~~~

The argparse library supports automatic generation of help and usage information.
When the input arguments include ``-h`` or ``--help``,
it will print the usage information to standard output.
If the default help output is not what is wanted,
the user can provide a custom help printing function by setting the ``print_help`` field in the ``rte_argparse`` object.
(If this field is set to NULL, the default help printing function will be used.)

If the custom help printing function wants to use the text produced by the default help function,
it can call the function ``rte_argparse_print_help()`` to get the help text printed to an output stream,
for example: stdout or stderr.
