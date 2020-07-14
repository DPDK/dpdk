..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. _coding_style:

DPDK Coding Style
=================

Description
-----------

This document specifies the preferred style for source files in the DPDK source tree.
It is based on the Linux Kernel coding guidelines and the FreeBSD 7.2 Kernel Developer's Manual (see man style(9)), but was heavily modified for the needs of the DPDK.

General Guidelines
------------------

The rules and guidelines given in this document cannot cover every situation, so the following general guidelines should be used as a fallback:

* The code style should be consistent within each individual file.
* In the case of creating new files, the style should be consistent within each file in a given directory or module.
* The primary reason for coding standards is to increase code readability and comprehensibility, therefore always use whatever option will make the code easiest to read.

Line length is recommended to be not more than 80 characters, including comments.
[Tab stop size should be assumed to be 8-characters wide].

.. note::

	The above is recommendation, and not a hard limit.
	However, it is expected that the recommendations should be followed in all but the rarest situations.

C Comment Style
---------------

Usual Comments
~~~~~~~~~~~~~~

These comments should be used in normal cases.
To document a public API, a doxygen-like format must be used: refer to :ref:`doxygen_guidelines`.

.. code-block:: c

 /*
  * VERY important single-line comments look like this.
  */

 /* Most single-line comments look like this. */

 /*
  * Multi-line comments look like this.  Make them real sentences. Fill
  * them so they look like real paragraphs.
  */

License Header
~~~~~~~~~~~~~~

Each file must begin with a special comment containing the
`Software Package Data Exchange (SPDX) License Identfier <https://spdx.org/using-spdx-license-identifier>`_.

Generally this is the BSD License, except for code granted special exceptions.
The SPDX licences identifier is sufficient, a file should not contain
an additional text version of the license (boilerplate).

After any copyright header, a blank line should be left before any other contents, e.g. include statements in a C file.

C Preprocessor Directives
-------------------------

Header Includes
~~~~~~~~~~~~~~~

In DPDK sources, the include files should be ordered as following:

#. libc includes (system includes first)
#. DPDK EAL includes
#. DPDK misc libraries includes
#. application-specific includes

Include files from the local application directory are included using quotes, while includes from other paths are included using angle brackets: "<>".

Example:

.. code-block:: c

 #include <stdio.h>
 #include <stdlib.h>

 #include <rte_eal.h>

 #include <rte_ring.h>
 #include <rte_mempool.h>

 #include "application.h"

Header File Guards
~~~~~~~~~~~~~~~~~~

Headers should be protected against multiple inclusion with the usual:

.. code-block:: c

   #ifndef _FILE_H_
   #define _FILE_H_

   /* Code */

   #endif /* _FILE_H_ */


Macros
~~~~~~

Do not ``#define`` or declare names except with the standard DPDK prefix: ``RTE_``.
This is to ensure there are no collisions with definitions in the application itself.

The names of "unsafe" macros (ones that have side effects), and the names of macros for manifest constants, are all in uppercase.

The expansions of expression-like macros are either a single token or have outer parentheses.
If a macro is an inline expansion of a function, the function name is all in lowercase and the macro has the same name all in uppercase.
If the macro encapsulates a compound statement, enclose it in a do-while loop, so that it can be used safely in if statements.
Any final statement-terminating semicolon should be supplied by the macro invocation rather than the macro, to make parsing easier for pretty-printers and editors.

For example:

.. code-block:: c

 #define MACRO(x, y) do {                                        \
         variable = (x) + (y);                                   \
         (y) += 2;                                               \
 } while(0)

.. note::

 Wherever possible, enums and inline functions should be preferred to macros, since they provide additional degrees of type-safety and can allow compilers to emit extra warnings about unsafe code.

Conditional Compilation
~~~~~~~~~~~~~~~~~~~~~~~

* When code is conditionally compiled using ``#ifdef`` or ``#if``, a comment may be added following the matching
  ``#endif`` or ``#else`` to permit the reader to easily discern where conditionally compiled code regions end.
* This comment should be used only for (subjectively) long regions, regions greater than 20 lines, or where a series of nested ``#ifdef``'s may be confusing to the reader.
  Exceptions may be made for cases where code is conditionally not compiled for the purposes of lint(1), or other tools, even though the uncompiled region may be small.
* The comment should be separated from the ``#endif`` or ``#else`` by a single space.
* For short conditionally compiled regions, a closing comment should not be used.
* The comment for ``#endif`` should match the expression used in the corresponding ``#if`` or ``#ifdef``.
* The comment for ``#else`` and ``#elif`` should match the inverse of the expression(s) used in the preceding ``#if`` and/or ``#elif`` statements.
* In the comments, the subexpression ``defined(FOO)`` is abbreviated as "FOO".
  For the purposes of comments, ``#ifndef FOO`` is treated as ``#if !defined(FOO)``.

.. code-block:: c

 #ifdef KTRACE
 #include <sys/ktrace.h>
 #endif

 #ifdef COMPAT_43
 /* A large region here, or other conditional code. */
 #else /* !COMPAT_43 */
 /* Or here. */
 #endif /* COMPAT_43 */

 #ifndef COMPAT_43
 /* Yet another large region here, or other conditional code. */
 #else /* COMPAT_43 */
 /* Or here. */
 #endif /* !COMPAT_43 */

.. note::

 Conditional compilation should be used only when absolutely necessary, as it increases the number of target binaries that need to be built and tested.

C Types
-------

Integers
~~~~~~~~

For fixed/minimum-size integer values, the project uses the form uintXX_t (from stdint.h) instead of older BSD-style integer identifiers of the form u_intXX_t.

Enumerations
~~~~~~~~~~~~

* Enumeration values are all uppercase.

.. code-block:: c

 enum enumtype { ONE, TWO } et;

* Enum types should be used in preference to macros #defining a set of (sequential) values.
* Enum types should be prefixed with ``rte_`` and the elements by a suitable prefix [generally starting ``RTE_<enum>_`` - where <enum> is a shortname for the enum type] to avoid namespace collisions.

Bitfields
~~~~~~~~~

The developer should group bitfields that are included in the same integer, as follows:

.. code-block:: c

 struct grehdr {
   uint16_t rec:3,
       srr:1,
       seq:1,
       key:1,
       routing:1,
       csum:1,
       version:3,
       reserved:4,
       ack:1;
 /* ... */
 }

Variable Declarations
~~~~~~~~~~~~~~~~~~~~~

In declarations, do not put any whitespace between asterisks and adjacent tokens, except for tokens that are identifiers related to types.
(These identifiers are the names of basic types, type qualifiers, and typedef-names other than the one being declared.)
Separate these identifiers from asterisks using a single space.

For example:

.. code-block:: c

   int *x;         /* no space after asterisk */
   int * const x;  /* space after asterisk when using a type qualifier */

* All externally-visible variables should have an ``rte_`` prefix in the name to avoid namespace collisions.
* Do not use uppercase letters - either in the form of ALL_UPPERCASE, or CamelCase - in variable names.
  Lower-case letters and underscores only.

Structure Declarations
~~~~~~~~~~~~~~~~~~~~~~

* In general, when declaring variables in new structures, declare them sorted by use, then by size (largest to smallest), and then in alphabetical order.
  Sorting by use means that commonly used variables are used together and that the structure layout makes logical sense.
  Ordering by size then ensures that as little padding is added to the structure as possible.
* For existing structures, additions to structures should be added to the end so for backward compatibility reasons.
* Each structure element gets its own line.
* Try to make the structure readable by aligning the member names using spaces as shown below.
* Names following extremely long types, which therefore cannot be easily aligned with the rest, should be separated by a single space.

.. code-block:: c

 struct foo {
         struct foo      *next;          /* List of active foo. */
         struct mumble   amumble;        /* Comment for mumble. */
         int             bar;            /* Try to align the comments. */
         struct verylongtypename *baz;   /* Won't fit with other members */
 };


* Major structures should be declared at the top of the file in which they are used, or in separate header files if they are used in multiple source files.
* Use of the structures should be by separate variable declarations and those declarations must be extern if they are declared in a header file.
* Externally visible structure definitions should have the structure name prefixed by ``rte_`` to avoid namespace collisions.

.. note::

    Uses of ``bool`` in structures are not preferred as is wastes space and
    it's also not clear as to what type size the bool is. A preferred use of
    ``bool`` is mainly as a return type from functions that return true/false,
    and maybe local variable functions.

    Ref: `LKML <https://lkml.org/lkml/2017/11/21/384>`_

Queues
~~~~~~

Use queue(3) macros rather than rolling your own lists, whenever possible.
Thus, the previous example would be better written:

.. code-block:: c

 #include <sys/queue.h>

 struct foo {
         LIST_ENTRY(foo) link;      /* Use queue macros for foo lists. */
         struct mumble   amumble;   /* Comment for mumble. */
         int             bar;       /* Try to align the comments. */
         struct verylongtypename *baz;   /* Won't fit with other members */
 };
 LIST_HEAD(, foo) foohead;          /* Head of global foo list. */


DPDK also provides an optimized way to store elements in lockless rings.
This should be used in all data-path code, when there are several consumer and/or producers to avoid locking for concurrent access.

Typedefs
~~~~~~~~

Avoid using typedefs for structure types.

For example, use:

.. code-block:: c

 struct my_struct_type {
 /* ... */
 };

 struct my_struct_type my_var;


rather than:

.. code-block:: c

 typedef struct my_struct_type {
 /* ... */
 } my_struct_type;

 my_struct_type my_var


Typedefs are problematic because they do not properly hide their underlying type;
for example, you need to know if the typedef is the structure itself, as shown above, or a pointer to the structure.
In addition, they must be declared exactly once, whereas an incomplete structure type can be mentioned as many times as necessary.
Typedefs are difficult to use in stand-alone header files.
The header that defines the typedef must be included before the header that uses it, or by the header that uses it (which causes namespace pollution),
or there must be a back-door mechanism for obtaining the typedef.

Note that #defines used instead of typedefs also are problematic (since they do not propagate the pointer type correctly due to direct text replacement).
For example, ``#define pint int *`` does not work as expected, while ``typedef int *pint`` does work.
As stated when discussing macros, typedefs should be preferred to macros in cases like this.

When convention requires a typedef; make its name match the struct tag.
Avoid typedefs ending in ``_t``, except as specified in Standard C or by POSIX.

.. note::

	It is recommended to use typedefs to define function pointer types, for reasons of code readability.
	This is especially true when the function type is used as a parameter to another function.

For example:

.. code-block:: c

	/**
	 * Definition of a remote launch function.
	 */
	typedef int (lcore_function_t)(void *);

	/* launch a function of lcore_function_t type */
	int rte_eal_remote_launch(lcore_function_t *f, void *arg, unsigned slave_id);


C Indentation
-------------

General
~~~~~~~

* Indentation is a hard tab, that is, a tab character, not a sequence of spaces,

.. note::

	Global whitespace rule in DPDK, use tabs for indentation, spaces for alignment.

* Do not put any spaces before a tab for indentation.
* If you have to wrap a long statement, put the operator at the end of the line, and indent again.
* For control statements (if, while, etc.), continuation it is recommended that the next line be indented by two tabs, rather than one,
  to prevent confusion as to whether the second line of the control statement forms part of the statement body or not.
  Alternatively, the line continuation may use additional spaces to line up to an appropriately point on the preceding line, for example, to align to an opening brace.

.. note::

	As with all style guidelines, code should match style already in use in an existing file.

.. code-block:: c

 while (really_long_variable_name_1 == really_long_variable_name_2 &&
     var3 == var4){  /* confusing to read as */
     x = y + z;      /* control stmt body lines up with second line of */
     a = b + c;      /* control statement itself if single indent used */
 }

 if (really_long_variable_name_1 == really_long_variable_name_2 &&
         var3 == var4){  /* two tabs used */
     x = y + z;          /* statement body no longer lines up */
     a = b + c;
 }

 z = a + really + long + statement + that + needs +
         two + lines + gets + indented + on + the +
         second + and + subsequent + lines;


* Do not add whitespace at the end of a line.

* Do not add whitespace or a blank line at the end of a file.


Control Statements and Loops
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Include a space after keywords (if, while, for, return, switch).
* Do not use braces (``{`` and ``}``) for control statements with zero or just a single statement, unless that statement is more than a single line in which case the braces are permitted.

.. code-block:: c

 for (p = buf; *p != '\0'; ++p)
         ;       /* nothing */
 for (;;)
         stmt;
 for (;;) {
         z = a + really + long + statement + that + needs +
                 two + lines + gets + indented + on + the +
                 second + and + subsequent + lines;
 }
 for (;;) {
         if (cond)
                 stmt;
 }
 if (val != NULL)
         val = realloc(val, newsize);


* Parts of a for loop may be left empty.

.. code-block:: c

 for (; cnt < 15; cnt++) {
         stmt1;
         stmt2;
 }

* Closing and opening braces go on the same line as the else keyword.
* Braces that are not necessary should be left out.

.. code-block:: c

 if (test)
         stmt;
 else if (bar) {
         stmt;
         stmt;
 } else
         stmt;


Function Calls
~~~~~~~~~~~~~~

* Do not use spaces after function names.
* Commas should have a space after them.
* No spaces after ``(`` or ``[`` or preceding the ``]`` or ``)`` characters.

.. code-block:: c

	error = function(a1, a2);
	if (error != 0)
		exit(error);


Operators
~~~~~~~~~

* Unary operators do not require spaces, binary operators do.
* Do not use parentheses unless they are required for precedence or unless the statement is confusing without them.
  However, remember that other people may be more easily confused than you.

Exit
~~~~

Exits should be 0 on success, or 1 on failure.

.. code-block:: c

         exit(0);        /*
                          * Avoid obvious comments such as
                          * "Exit 0 on success."
                          */
 }

Local Variables
~~~~~~~~~~~~~~~

* Variables should be declared at the start of a block of code rather than in the middle.
  The exception to this is when the variable is ``const`` in which case the declaration must be at the point of first use/assignment.
* When declaring variables in functions, multiple variables per line are OK.
  However, if multiple declarations would cause the line to exceed a reasonable line length, begin a new set of declarations on the next line rather than using a line continuation.
* Be careful to not obfuscate the code by initializing variables in the declarations, only the last variable on a line should be initialized.
  If multiple variables are to be initialized when defined, put one per line.
* Do not use function calls in initializers, except for ``const`` variables.

.. code-block:: c

 int i = 0, j = 0, k = 0;  /* bad, too many initializer */

 char a = 0;        /* OK, one variable per line with initializer */
 char b = 0;

 float x, y = 0.0;  /* OK, only last variable has initializer */


Casts and sizeof
~~~~~~~~~~~~~~~~

* Casts and sizeof statements are not followed by a space.
* Always write sizeof statements with parenthesis.
  The redundant parenthesis rules do not apply to sizeof(var) instances.

C Function Definition, Declaration and Use
-------------------------------------------

Prototypes
~~~~~~~~~~

* It is recommended (and generally required by the compiler) that all non-static functions are prototyped somewhere.
* Functions local to one source module should be declared static, and should not be prototyped unless absolutely necessary.
* Functions used from other parts of code (external API) must be prototyped in the relevant include file.
* Function prototypes should be listed in a logical order, preferably alphabetical unless there is a compelling reason to use a different ordering.
* Functions that are used locally in more than one module go into a separate header file, for example, "extern.h".
* Do not use the ``__P`` macro.
* Functions that are part of an external API should be documented using Doxygen-like comments above declarations. See :ref:`doxygen_guidelines` for details.
* Functions that are part of the external API must have an ``rte_`` prefix on the function name.
* Do not use uppercase letters - either in the form of ALL_UPPERCASE, or CamelCase - in function names. Lower-case letters and underscores only.
* When prototyping functions, associate names with parameter types, for example:

.. code-block:: c

 void function1(int fd); /* good */
 void function2(int);    /* bad */

* Short function prototypes should be contained on a single line.
  Longer prototypes, e.g. those with many parameters, can be split across multiple lines.
  The second and subsequent lines should be further indented as for line statement continuations as described in the previous section.

.. code-block:: c

 static char *function1(int _arg, const char *_arg2,
        struct foo *_arg3,
        struct bar *_arg4,
        struct baz *_arg5);
 static void usage(void);

.. note::

	Unlike function definitions, the function prototypes do not need to place the function return type on a separate line.

Definitions
~~~~~~~~~~~

* The function type should be on a line by itself preceding the function.
* The opening brace of the function body should be on a line by itself.

.. code-block:: c

 static char *
 function(int a1, int a2, float fl, int a4)
 {


* Do not declare functions inside other functions.
  ANSI C states that such declarations have file scope regardless of the nesting of the declaration.
  Hiding file declarations in what appears to be a local scope is undesirable and will elicit complaints from a good compiler.
* Old-style (K&R) function declaration should not be used, use ANSI function declarations instead as shown below.
* Long argument lists should be wrapped as described above in the function prototypes section.

.. code-block:: c

 /*
  * All major routines should have a comment briefly describing what
  * they do. The comment before the "main" routine should describe
  * what the program does.
  */
 int
 main(int argc, char *argv[])
 {
         char *ep;
         long num;
         int ch;

C Statement Style and Conventions
---------------------------------

NULL Pointers
~~~~~~~~~~~~~

* NULL is the preferred null pointer constant.
  Use NULL instead of ``(type *)0`` or ``(type *)NULL``, except where the compiler does not know the destination type e.g. for variadic args to a function.
* Test pointers against NULL, for example, use:

.. code-block:: c

 if (p == NULL) /* Good, compare pointer to NULL */

 if (!p) /* Bad, using ! on pointer */


* Do not use ! for tests unless it is a boolean, for example, use:

.. code-block:: c

	if (*p == '\0') /* check character against (char)0 */

Return Value
~~~~~~~~~~~~

* Functions which create objects, or allocate memory, should return pointer types, and NULL on error.
  The error type should be indicated may setting the variable ``rte_errno`` appropriately.
* Functions which work on bursts of packets, such as RX-like or TX-like functions, should return the number of packets handled.
* Other functions returning int should generally behave like system calls:
  returning 0 on success and -1 on error, setting ``rte_errno`` to indicate the specific type of error.
* Where already standard in a given library, the alternative error approach may be used where the negative value is not -1 but is instead ``-errno`` if relevant, for example, ``-EINVAL``.
  Note, however, to allow consistency across functions returning integer or pointer types, the previous approach is preferred for any new libraries.
* For functions where no error is possible, the function type should be ``void`` not ``int``.
* Routines returning ``void *`` should not have their return values cast to any pointer type.
  (Typecasting can prevent the compiler from warning about missing prototypes as any implicit definition of a function returns int,
  which, unlike ``void *``, needs a typecast to assign to a pointer variable.)

.. note::

	The above rule about not typecasting ``void *`` applies to malloc, as well as to DPDK functions.

* Values in return statements should not be enclosed in parentheses.

Logging and Errors
~~~~~~~~~~~~~~~~~~

In the DPDK environment, use the logging interface provided:

.. code-block:: c

 /* register log types for this application */
 int my_logtype1 = rte_log_register("myapp.log1");
 int my_logtype2 = rte_log_register("myapp.log2");

 /* set global log level to INFO */
 rte_log_set_global_level(RTE_LOG_INFO);

 /* only display messages higher than NOTICE for log2 (default
  * is DEBUG) */
 rte_log_set_level(my_logtype2, RTE_LOG_NOTICE);

 /* enable all PMD logs (whose identifier string starts with "pmd.") */
 rte_log_set_level_pattern("pmd.*", RTE_LOG_DEBUG);

 /* log in debug level */
 rte_log_set_global_level(RTE_LOG_DEBUG);
 RTE_LOG(DEBUG, my_logtype1, "this is a debug level message\n");
 RTE_LOG(INFO, my_logtype1, "this is a info level message\n");
 RTE_LOG(WARNING, my_logtype1, "this is a warning level message\n");
 RTE_LOG(WARNING, my_logtype2, "this is a debug level message (not displayed)\n");

 /* log in info level */
 rte_log_set_global_level(RTE_LOG_INFO);
 RTE_LOG(DEBUG, my_logtype1, "debug level message (not displayed)\n");

Branch Prediction
~~~~~~~~~~~~~~~~~

* When a test is done in a critical zone (called often or in a data path) the code can use the ``likely()`` and ``unlikely()`` macros to indicate the expected, or preferred fast path.
  They are expanded as a compiler builtin and allow the developer to indicate if the branch is likely to be taken or not. Example:

.. code-block:: c

 #include <rte_branch_prediction.h>
 if (likely(x > 1))
   do_stuff();

.. note::

	The use of ``likely()`` and ``unlikely()`` should only be done in performance critical paths,
	and only when there is a clearly preferred path, or a measured performance increase gained from doing so.
	These macros should be avoided in non-performance-critical code.

Static Variables and Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* All functions and variables that are local to a file must be declared as ``static`` because it can often help the compiler to do some optimizations (such as, inlining the code).
* Functions that should be inlined should to be declared as ``static inline`` and can be defined in a .c or a .h file.

.. note::
	Static functions defined in a header file must be declared as ``static inline`` in order to prevent compiler warnings about the function being unused.

Const Attribute
~~~~~~~~~~~~~~~

The ``const`` attribute should be used as often as possible when a variable is read-only.

Inline ASM in C code
~~~~~~~~~~~~~~~~~~~~

The ``asm`` and ``volatile`` keywords do not have underscores. The AT&T syntax should be used.
Input and output operands should be named to avoid confusion, as shown in the following example:

.. code-block:: c

	asm volatile("outb %[val], %[port]"
		: :
		[port] "dN" (port),
		[val] "a" (val));

Control Statements
~~~~~~~~~~~~~~~~~~

* Forever loops are done with for statements, not while statements.
* Elements in a switch statement that cascade should have a FALLTHROUGH comment. For example:

.. code-block:: c

         switch (ch) {         /* Indent the switch. */
         case 'a':             /* Don't indent the case. */
                 aflag = 1;    /* Indent case body one tab. */
                 /* FALLTHROUGH */
         case 'b':
                 bflag = 1;
                 break;
         case '?':
         default:
                 usage();
                 /* NOTREACHED */
         }

Dynamic Logging
---------------

DPDK provides infrastructure to perform logging during runtime. This is very
useful for enabling debug output without recompilation. To enable or disable
logging of a particular topic, the ``--log-level`` parameter can be provided
to EAL, which will change the log level. DPDK code can register topics,
which allows the user to adjust the log verbosity for that specific topic.

In general, the naming scheme is as follows: ``type.section.name``

 * Type is the type of component, where ``lib``, ``pmd``, ``bus`` and ``user``
   are the common options.
 * Section refers to a specific area, for example a poll-mode-driver for an
   ethernet device would use ``pmd.net``, while an eventdev PMD uses
   ``pmd.event``.
 * The name identifies the individual item that the log applies to.
   The name section must align with
   the directory that the PMD code resides. See examples below for clarity.

Examples:

 * The virtio network PMD in ``drivers/net/virtio`` uses ``pmd.net.virtio``
 * The eventdev software poll mode driver in ``drivers/event/sw`` uses ``pmd.event.sw``
 * The octeontx mempool driver in ``drivers/mempool/octeontx`` uses ``pmd.mempool.octeontx``
 * The DPDK hash library in ``lib/librte_hash`` uses ``lib.hash``

Specializations
~~~~~~~~~~~~~~~

In addition to the above logging topic, any PMD or library can further split
logging output by using "specializations". A specialization could be the
difference between initialization code, and logs of events that occur at runtime.

An example could be the initialization log messages getting one
specialization, while another specialization handles mailbox command logging.
Each PMD, library or component can create as many specializations as required.

A specialization looks like this:

 * Initialization output: ``type.section.name.init``
 * PF/VF mailbox output: ``type.section.name.mbox``

A real world example is the i40e poll mode driver which exposes two
specializations, one for initialization ``pmd.net.i40e.init`` and the other for
the remaining driver logs ``pmd.net.i40e.driver``.

Note that specializations have no formatting rules, but please follow
a precedent if one exists. In order to see all current log topics and
specializations, run the ``app/test`` binary, and use the ``dump_log_types``

Python Code
-----------

All Python code should work with Python 2.7+ and 3.2+ and be compliant with
`PEP8 (Style Guide for Python Code) <https://www.python.org/dev/peps/pep-0008/>`_.

The ``pep8`` tool can be used for testing compliance with the guidelines.

Integrating with the Build System
---------------------------------

DPDK supports being built in two different ways:

* using ``make`` - or more specifically "GNU make", i.e. ``gmake`` on FreeBSD
* using the tools ``meson`` and ``ninja``

Any new library or driver to be integrated into DPDK should support being
built with both systems. While building using ``make`` is a legacy approach, and
most build-system enhancements are being done using ``meson`` and ``ninja``
there are no plans at this time to deprecate the legacy ``make`` build system.

Therefore all new component additions should include both a ``Makefile`` and a
``meson.build`` file, and should be added to the component lists in both the
``Makefile`` and ``meson.build`` files in the relevant top-level directory:
either ``lib`` directory or a ``driver`` subdirectory.

Makefile Contents
~~~~~~~~~~~~~~~~~

The ``Makefile`` for the component should be of the following format, where
``<name>`` corresponds to the name of the library in question, e.g. hash,
lpm, etc. For drivers, the same format of Makefile is used.

.. code-block:: none

	# pull in basic DPDK definitions, including whether library is to be
	# built or not
	include $(RTE_SDK)/mk/rte.vars.mk

	# library name
	LIB = librte_<name>.a

	# any library cflags needed. Generally add "-O3 $(WERROR_FLAGS)"
	CFLAGS += -O3
	CFLAGS += $(WERROR_FLAGS)

	# the symbol version information for the library
	EXPORT_MAP := rte_<name>_version.map

	# all source filenames are stored in SRCS-y
	SRCS-$(CONFIG_RTE_LIBRTE_<NAME>) += rte_<name>.c

	# install includes
	SYMLINK-$(CONFIG_RTE_LIBRTE_<NAME>)-include += rte_<name>.h

	# pull in rules to build the library
	include $(RTE_SDK)/mk/rte.lib.mk

Meson Build File Contents - Libraries
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``meson.build`` file for a new DPDK library should be of the following basic
format.

.. code-block:: python

	sources = files('file1.c', ...)
	headers = files('file1.h', ...)


This will build based on a number of conventions and assumptions within the DPDK
itself, for example, that the library name is the same as the directory name in
which the files are stored.

For a library ``meson.build`` file, there are number of variables which can be
set, some mandatory, others optional. The mandatory fields are:

sources
	**Default Value = []**.
	This variable should list out the files to be compiled up to create the
	library. Files must be specified using the meson ``files()`` function.


The optional fields are:

build
	**Default Value = true**
	Used to optionally compile a library, based on its dependencies or
	environment. When set to "false" the ``reason`` value, explained below, should
	also be set to explain to the user why the component is not being built.
	A simple example of use would be:

.. code-block:: python

	if not is_linux
	        build = false
	        reason = 'only supported on Linux'
	endif


cflags
	**Default Value = [<-march/-mcpu flags>]**.
	Used to specify any additional cflags that need to be passed to compile
	the sources in the library.

deps
	**Default Value = ['eal']**.
	Used to list the internal library dependencies of the library. It should
	be assigned to using ``+=`` rather than overwriting using ``=``.  The
	dependencies should be specified as strings, each one giving the name of
	a DPDK library, without the ``librte_`` prefix. Dependencies are handled
	recursively, so specifying e.g. ``mempool``, will automatically also
	make the library depend upon the mempool library's dependencies too -
	``ring`` and ``eal``. For libraries that only depend upon EAL, this
	variable may be omitted from the ``meson.build`` file.  For example:

.. code-block:: python

	deps += ['ethdev']


ext_deps
	**Default Value = []**.
	Used to specify external dependencies of this library. They should be
	returned as dependency objects, as returned from the meson
	``dependency()`` or ``find_library()`` functions. Before returning
	these, they should be checked to ensure the dependencies have been
	found, and, if not, the ``build`` variable should be set to ``false``.
	For example:

.. code-block:: python

	my_dep = dependency('libX', required: 'false')
	if my_dep.found()
		ext_deps += my_dep
	else
		build = false
	endif


headers
	**Default Value = []**.
	Used to return the list of header files for the library that should be
	installed to $PREFIX/include when ``ninja install`` is run. As with
	source files, these should be specified using the meson ``files()``
	function.

includes:
	**Default Value = []**.
	Used to indicate any additional header file paths which should be
	added to the header search path for other libs depending on this
	library. EAL uses this so that other libraries building against it
	can find the headers in subdirectories of the main EAL directory. The
	base directory of each library is always given in the include path,
	it does not need to be specified here.

name
	**Default Value = library name derived from the directory name**.
	If a library's .so or .a file differs from that given in the directory
	name, the name should be specified using this variable. In practice,
	since the convention is that for a library called ``librte_xyz.so``, the
	sources are stored in a directory ``lib/librte_xyz``, this value should
	never be needed for new libraries.

.. note::

	The name value also provides the name used to find the function version
	map file, as part of the build process, so if the directory name and
	library names differ, the ``version.map`` file should be named
	consistently with the library, not the directory

objs
	**Default Value = []**.
	This variable can be used to pass to the library build some pre-built
	objects that were compiled up as part of another target given in the
	included library ``meson.build`` file.

reason
	**Default Value = '<unknown reason>'**.
	This variable should be used when a library is not to be built i.e. when
	``build`` is set to "false", to specify the reason why a library will not be
	built. For missing dependencies this should be of the form
	``'missing dependency, "libname"'``.

use_function_versioning
	**Default Value = false**.
	Specifies if the library in question has ABI versioned functions. If it
	has, this value should be set to ensure that the C files are compiled
	twice with suitable parameters for each of shared or static library
	builds.

Meson Build File Contents - Drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For drivers, the values are largely the same as for libraries. The variables
supported are:

build
	As above.

cflags
	As above.

deps
	As above.

ext_deps
	As above.

includes
	**Default Value = <driver directory>** Some drivers include a base
	directory for additional source files and headers, so we have this
	variable to allow the headers from that base directory to be found when
	compiling driver sources. Should be appended to using ``+=`` rather than
	overwritten using ``=``.  The values appended should be meson include
	objects got using the ``include_directories()`` function. For example:

.. code-block:: python

	includes += include_directories('base')

name
	As above, though note that each driver class can define it's own naming
	scheme for the resulting ``.so`` files.

objs
	As above, generally used for the contents of the ``base`` directory.

pkgconfig_extra_libs
	**Default Value = []**
	This variable is used to pass additional library link flags through to
	the DPDK pkgconfig file generated, for example, to track any additional
	libraries that may need to be linked into the build - especially when
	using static libraries. Anything added here will be appended to the end
	of the ``pkgconfig --libs`` output.

reason
	As above.

sources [mandatory]
	As above

version
	As above
