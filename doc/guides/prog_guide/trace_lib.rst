..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2020 Marvell International Ltd.

Trace Library
=============

Overview
--------

*Tracing* is a technique used to understand what goes on in a running software
system. The software used for tracing is called a *tracer*, which is
conceptually similar to a tape recorder.
When recording, specific instrumentation points placed in the software source
code generate events that are saved on a giant tape: a trace file.
The trace file then later can be opened in *trace viewers* to visualize and
analyze the trace events with timestamps and multi-core views.
Such a mechanism will be useful for resolving a wide range of problems such as
multi-core synchronization issues, latency measurements, finding out the
post analysis information like CPU idle time, etc that would otherwise be
extremely challenging to get.

Tracing is often compared to *logging*. However, tracers and loggers are two
different tools, serving two different purposes.
Tracers are designed to record much lower-level events that occur much more
frequently than log messages, often in the range of thousands per second, with
very little execution overhead.
Logging is more appropriate for a very high-level analysis of less frequent
events: user accesses, exceptional conditions (errors and warnings, for
example), database transactions, instant messaging communications, and such.
Simply put, logging is one of the many use cases that can be satisfied with
tracing.

DPDK tracing library features
-----------------------------

- A framework to add tracepoints in control and fast path APIs with minimum
  impact on performance.
  Typical trace overhead is ~20 cycles and instrumentation overhead is 1 cycle.
- Enable and disable the tracepoints at runtime.
- Save the trace buffer to the filesystem at any point in time.
- Support ``overwrite`` and ``discard`` trace mode operations.
- String-based tracepoint object lookup.
- Enable and disable a set of tracepoints based on regular expression and/or
  globbing.
- Generate trace in ``Common Trace Format (CTF)``. ``CTF`` is an open-source
  trace format and is compatible with ``LTTng``.
  For detailed information, refer to
  `Common Trace Format <https://diamon.org/ctf/>`_.

How to add a tracepoint?
------------------------

This section steps you through the details of adding a simple tracepoint.

.. _create_tracepoint_header_file:

Create the tracepoint header file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

 #include <rte_trace_point.h>

 RTE_TRACE_POINT(
        app_trace_string,
        RTE_TRACE_POINT_ARGS(const char *str),
        rte_trace_point_emit_string(str);
 )

The above macro creates ``app_trace_string`` tracepoint.
The user can choose any name for the tracepoint.
However, when adding a tracepoint in the DPDK library, the
``rte_<library_name>_trace_[<domain>_]<name>`` naming convention must be
followed.
The examples are ``rte_eal_trace_generic_str``, ``rte_mempool_trace_create``.

The ``RTE_TRACE_POINT`` macro expands from above definition as the following
function template:

.. code-block:: c

 static __rte_always_inline void
 app_trace_string(const char *str)
 {
         /* Trace subsystem hooks */
         ...
         rte_trace_point_emit_string(str);
 }

The consumer of this tracepoint can invoke
``app_trace_string(const char *str)`` to emit the trace event to the trace
buffer.

Register the tracepoint
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

 #include <rte_trace_point_register.h>

 #include <my_tracepoint.h>

 RTE_TRACE_POINT_REGISTER(app_trace_string, app.trace.string)

The above code snippet registers the ``app_trace_string`` tracepoint to
trace library. Here, the ``my_tracepoint.h`` is the header file
that the user created in the first step :ref:`create_tracepoint_header_file`.

The second argument for the ``RTE_TRACE_POINT_REGISTER`` is the name for the
tracepoint. This string will be used for tracepoint lookup or regular
expression and/or glob based tracepoint operations.
There is no requirement for the tracepoint function and its name to be similar.
However, it is recommended to have a similar name for a better naming
convention.

.. note::

   The ``rte_trace_point_register.h`` header must be included before any
   inclusion of the ``rte_trace_point.h`` header.

.. note::

   The ``RTE_TRACE_POINT_REGISTER`` defines the placeholder for the
   ``rte_trace_point_t`` tracepoint object. The user must export a
   ``__<trace_function_name>`` symbol in the library ``.map`` file for this
   tracepoint to be used out of the library, in shared builds.
   For example, ``__app_trace_string`` will be the exported symbol in the
   above example.

Fast path tracepoint
--------------------

In order to avoid performance impact in fast path code, the library introduced
``RTE_TRACE_POINT_FP``. When adding the tracepoint in fast path code,
the user must use ``RTE_TRACE_POINT_FP`` instead of ``RTE_TRACE_POINT``.

``RTE_TRACE_POINT_FP`` is compiled out by default and it can be enabled using
the ``enable_trace_fp`` option for meson build.

Event record mode
-----------------

Event record mode is an attribute of trace buffers. Trace library exposes the
following modes:

Overwrite
   When the trace buffer is full, new trace events overwrites the existing
   captured events in the trace buffer.
Discard
   When the trace buffer is full, new trace events will be discarded.

The mode can be configured either using EAL command line parameter
``--trace-mode`` on application boot up or use ``rte_trace_mode_set()`` API to
configure at runtime.

Trace file location
-------------------

On ``rte_trace_save()`` or ``rte_eal_cleanup()`` invocation, the library saves
the trace buffers to the filesystem. By default, the trace files are stored in
``$HOME/dpdk-traces/rte-yyyy-mm-dd-[AP]M-hh-mm-ss/``.
It can be overridden by the ``--trace-dir=<directory path>`` EAL command line
option.

For more information, refer to :doc:`../linux_gsg/linux_eal_parameters` for
trace EAL command line options.

View and analyze the recorded events
------------------------------------

Once the trace directory is available, the user can view/inspect the recorded
events.

There are many tools you can use to read DPDK traces:

1. ``babeltrace`` is a command-line utility that converts trace formats; it
supports the format that DPDK trace library produces, CTF, as well as a
basic text output that can be grep'ed.
The babeltrace command is part of the Open Source Babeltrace project.

2. ``Trace Compass`` is a graphical user interface for viewing and analyzing
any type of logs or traces, including DPDK traces.

Use the babeltrace command-line tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to list all the recorded events of a trace is to pass its path
to babeltrace with no options::

    babeltrace </path-to-trace-events/rte-yyyy-mm-dd-[AP]M-hh-mm-ss/>

``babeltrace`` finds all traces recursively within the given path and prints
all their events, merging them in chronological order.

You can pipe the output of the babeltrace into a tool like grep(1) for further
filtering. Below example grep the events for ``ethdev`` only::

    babeltrace /tmp/my-dpdk-trace | grep ethdev

You can pipe the output of babeltrace into a tool like wc(1) to count the
recorded events. Below example count the number of ``ethdev`` events::

    babeltrace /tmp/my-dpdk-trace | grep ethdev | wc --lines

Use the tracecompass GUI tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Tracecompass`` is another tool to view/analyze the DPDK traces which gives
a graphical view of events. Like ``babeltrace``, tracecompass also provides
an interface to search for a particular event.
To use ``tracecompass``, following are the minimum required steps:

- Install ``tracecompass`` to the localhost. Variants are available for Linux,
  Windows, and OS-X.
- Launch ``tracecompass`` which will open a graphical window with trace
  management interfaces.
- Open a trace using ``File->Open Trace`` option and select metadata file which
  is to be viewed/analyzed.

For more details, refer
`Trace Compass <https://www.eclipse.org/tracecompass/>`_.

Quick start
-----------

This section steps you through the details of generating trace and viewing it.

- Start the dpdk-test::

    echo "quit" | ./build/app/test/dpdk-test --no-huge --trace=.*

- View the traces with babeltrace viewer::

    babeltrace $HOME/dpdk-traces/rte-yyyy-mm-dd-[AP]M-hh-mm-ss/

Implementation details
----------------------

As DPDK trace library is designed to generate traces that uses ``Common Trace
Format (CTF)``. ``CTF`` specification consists of the following units to create
a trace.

- ``Stream`` Sequence of packets.
- ``Packet`` Header and one or more events.
- ``Event`` Header and payload.

For detailed information, refer to
`Common Trace Format <https://diamon.org/ctf/>`_.

The implementation details broadly divided into the following areas:

Trace metadata creation
~~~~~~~~~~~~~~~~~~~~~~~

Based on the ``CTF`` specification, one of a CTF trace's streams is mandatory:
the metadata stream. It contains exactly what you would expect: data about the
trace itself. The metadata stream contains a textual description of the binary
layouts of all the other streams.

This description is written using the Trace Stream Description Language (TSDL),
a declarative language that exists only in the realm of CTF.
The purpose of the metadata stream is to make CTF readers know how to parse a
trace's binary streams of events without CTF specifying any fixed layout.
The only stream layout known in advance is, in fact, the metadata stream's one.

The internal ``trace_metadata_create()`` function generates the metadata.

Trace memory
~~~~~~~~~~~~

The trace memory will be allocated through an internal function
``__rte_trace_mem_per_thread_alloc()``. The trace memory will be allocated
per thread to enable lock less trace-emit function.
The memory for the trace memory for DPDK lcores will be allocated on
``rte_eal_init()`` if the trace is enabled through a EAL option.
For non DPDK threads, on the first trace emission, the memory will be
allocated.

Trace memory layout
~~~~~~~~~~~~~~~~~~~

.. _table_trace_mem_layout:

.. table:: Trace memory layout.

  +-------------------+
  |   packet.header   |
  +-------------------+
  |   packet.context  |
  +-------------------+
  |   trace 0 header  |
  +-------------------+
  |   trace 0 payload |
  +-------------------+
  |   trace 1 header  |
  +-------------------+
  |   trace 1 payload |
  +-------------------+
  |   trace N header  |
  +-------------------+
  |   trace N payload |
  +-------------------+

packet.header
^^^^^^^^^^^^^

.. _table_packet_header:

.. table:: Packet header layout.

  +-------------------+
  |   uint32_t magic  |
  +-------------------+
  |   rte_uuid_t uuid |
  +-------------------+

packet.context
^^^^^^^^^^^^^^

.. _table_packet_context:

.. table:: Packet context layout.

  +----------------------+
  |  uint32_t thread_id  |
  +----------------------+
  | char thread_name[32] |
  +----------------------+

trace.header
^^^^^^^^^^^^

.. _table_trace_header:

.. table:: Trace header layout.

  +----------------------+
  | event_id  [63:48]    |
  +----------------------+
  | timestamp [47:0]     |
  +----------------------+

The trace header is 64 bits, it consists of 48 bits of timestamp and 16 bits
event ID.

The ``packet.header`` and ``packet.context`` will be written in the slow path
at the time of trace memory creation. The ``trace.header`` and trace payload
will be emitted when the tracepoint function is invoked.
