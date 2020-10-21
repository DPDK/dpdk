..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

Design
======

Environment or Architecture-specific Sources
--------------------------------------------

In DPDK and DPDK applications, some code is specific to an architecture (i686, x86_64) or to an executive environment (freebsd or linux) and so on.
As far as is possible, all such instances of architecture or env-specific code should be provided via standard APIs in the EAL.

By convention, a file is common if it is not located in a directory indicating that it is specific.
For instance, a file located in a subdir of "x86_64" directory is specific to this architecture.
A file located in a subdir of "linux" is specific to this execution environment.

.. note::

   Code in DPDK libraries and applications should be generic.
   The correct location for architecture or executive environment specific code is in the EAL.

When absolutely necessary, there are several ways to handle specific code:

* Use a ``#ifdef`` with a build definition macro in the C code.
  This can be done when the differences are small and they can be embedded in the same C file:

  .. code-block:: c

     #ifdef RTE_ARCH_I686
     toto();
     #else
     titi();
     #endif

* Use build definition macros and conditions in the Meson build file. This is done when the differences are more significant.
  In this case, the code is split into two separate files that are architecture or environment specific.
  This should only apply inside the EAL library.

Per Architecture Sources
~~~~~~~~~~~~~~~~~~~~~~~~

The following macro options can be used:

* ``RTE_ARCH`` is a string that contains the name of the architecture.
* ``RTE_ARCH_I686``, ``RTE_ARCH_X86_64``, ``RTE_ARCH_X86_X32``, ``RTE_ARCH_PPC_64``, ``RTE_ARCH_ARM``, ``RTE_ARCH_ARMv7`` or ``RTE_ARCH_ARM64`` are defined only if we are building for those architectures.

Per Execution Environment Sources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following macro options can be used:

* ``RTE_EXEC_ENV`` is a string that contains the name of the executive environment.
* ``RTE_EXEC_ENV_FREEBSD``, ``RTE_EXEC_ENV_LINUX`` or ``RTE_EXEC_ENV_WINDOWS`` are defined only if we are building for this execution environment.

Mbuf features
-------------

The ``rte_mbuf`` structure must be kept small (128 bytes).

In order to add new features without wasting buffer space for unused features,
some fields and flags can be registered dynamically in a shared area.
The "dynamic" mbuf area is the default choice for the new features.

The "dynamic" area is eating the remaining space in mbuf,
and some existing "static" fields may need to become "dynamic".

Adding a new static field or flag must be an exception matching many criteria
like (non exhaustive): wide usage, performance, size.


Library Statistics
------------------

Description
~~~~~~~~~~~

This document describes the guidelines for DPDK library-level statistics counter
support. This includes guidelines for turning library statistics on and off and
requirements for preventing ABI changes when implementing statistics.


Mechanism to allow the application to turn library statistics on and off
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Having runtime support for enabling/disabling library statistics is recommended,
as build-time options should be avoided. However, if build-time options are used,
for example as in the table library, the options can be set using c_args.
When this flag is set, all the counters supported by current library are
collected for all the instances of every object type provided by the library.
When this flag is cleared, none of the counters supported by the current library
are collected for any instance of any object type provided by the library:


Prevention of ABI changes due to library statistics support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The layout of data structures and prototype of functions that are part of the
library API should not be affected by whether the collection of statistics
counters is turned on or off for the current library. In practical terms, this
means that space should always be allocated in the API data structures for
statistics counters and the statistics related API functions are always built
into the code, regardless of whether the statistics counter collection is turned
on or off for the current library.

When the collection of statistics counters for the current library is turned
off, the counters retrieved through the statistics related API functions should
have a default value of zero.


Motivation to allow the application to turn library statistics on and off
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is highly recommended that each library provides statistics counters to allow
an application to monitor the library-level run-time events. Typical counters
are: number of packets received/dropped/transmitted, number of buffers
allocated/freed, number of occurrences for specific events, etc.

However, the resources consumed for library-level statistics counter collection
have to be spent out of the application budget and the counters collected by
some libraries might not be relevant to the current application. In order to
avoid any unwanted waste of resources and/or performance impacts, the
application should decide at build time whether the collection of library-level
statistics counters should be turned on or off for each library individually.

Library-level statistics counters can be relevant or not for specific
applications:

* For Application A, counters maintained by Library X are always relevant and
  the application needs to use them to implement certain features, such as traffic
  accounting, logging, application-level statistics, etc. In this case,
  the application requires that collection of statistics counters for Library X is
  always turned on.

* For Application B, counters maintained by Library X are only useful during the
  application debug stage and are not relevant once debug phase is over. In this
  case, the application may decide to turn on the collection of Library X
  statistics counters during the debug phase and at a later stage turn them off.

* For Application C, counters maintained by Library X are not relevant at all.
  It might be that the application maintains its own set of statistics counters
  that monitor a different set of run-time events (e.g. number of connection
  requests, number of active users, etc). It might also be that the application
  uses multiple libraries (Library X, Library Y, etc) and it is interested in the
  statistics counters of Library Y, but not in those of Library X. In this case,
  the application may decide to turn the collection of statistics counters off for
  Library X and on for Library Y.

The statistics collection consumes a certain amount of CPU resources (cycles,
cache bandwidth, memory bandwidth, etc) that depends on:

* Number of libraries used by the current application that have statistics
  counters collection turned on.

* Number of statistics counters maintained by each library per object type
  instance (e.g. per port, table, pipeline, thread, etc).

* Number of instances created for each object type supported by each library.

* Complexity of the statistics logic collection for each counter: when only
  some occurrences of a specific event are valid, additional logic is typically
  needed to decide whether the current occurrence of the event should be counted
  or not. For example, in the event of packet reception, when only TCP packets
  with destination port within a certain range should be recorded, conditional
  branches are usually required. When processing a burst of packets that have been
  validated for header integrity, counting the number of bits set in a bitmask
  might be needed.

PF and VF Considerations
------------------------

The primary goal of DPDK is to provide a userspace dataplane. Managing VFs from
a PF driver is a control plane feature and developers should generally rely on
the Linux Kernel for that.

Developers should work with the Linux Kernel community to get the required
functionality upstream. PF functionality should only be added to DPDK for
testing and prototyping purposes while the kernel work is ongoing. It should
also be marked with an "EXPERIMENTAL" tag. If the functionality isn't
upstreamable then a case can be made to maintain the PF functionality in DPDK
without the EXPERIMENTAL tag.
