..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Multi-process_Support:

Multi-process Support
=====================

In the DPDK, multi-process support is designed to allow a group of DPDK processes
to work together in a simple transparent manner to perform packet processing,
or other workloads.
To support this functionality,
a number of additions have been made to the core DPDK Environment Abstraction Layer (EAL).

The EAL has been modified to allow different types of DPDK processes to be spawned,
each with different permissions on the hugepage memory used by the applications.
For now, there are two types of process specified:

*   primary processes, which can initialize and which have full permissions on shared memory

*   secondary processes, which cannot initialize shared memory,
    but can attach to pre- initialized shared memory and create objects in it.

Standalone DPDK processes are primary processes,
while secondary processes can only run alongside a primary process or
after a primary process has already configured the hugepage shared memory for them.

.. note::

    Secondary processes should run alongside primary process with same DPDK version.

    Secondary processes which requires access to physical devices in Primary process, must
    be passed with the same allow and block options.

To support these two process types, and other multi-process setups described later,
two additional command-line parameters are available to the EAL:

*   ``--proc-type:`` for specifying a given process instance as the primary or secondary DPDK instance

*   ``--file-prefix:`` to allow processes that do not want to co-operate to have different memory regions

A number of example applications are provided that demonstrate how multiple DPDK processes can be used together.
These are more fully documented in the "Multi- process Sample Application" chapter
in the *DPDK Sample Application's User Guide*.

Memory Sharing
--------------

The key element in getting a multi-process application working using the DPDK is to ensure that
memory resources are properly shared among the processes making up the multi-process application.
Once there are blocks of shared memory available that can be accessed by multiple processes,
then issues such as inter-process communication (IPC) becomes much simpler.

On application start-up in a primary or standalone process,
the DPDK records to memory-mapped files the details of the memory configuration it is using - hugepages in use,
the virtual addresses they are mapped at, the number of memory channels present, etc.
When a secondary process is started, these files are read and the EAL recreates the same memory configuration
in the secondary process so that all memory zones are shared between processes and all pointers to that memory are valid,
and point to the same objects, in both processes.

.. note::

    Refer to `Multi-process Limitations`_ for details of
    how Linux kernel Address-Space Layout Randomization (ASLR) can affect memory sharing.

    If the primary process was run with ``--legacy-mem`` or
    ``--single-file-segments`` switch, secondary processes must be run with the
    same switch specified. Otherwise, memory corruption may occur.

.. _figure_multi_process_memory:

.. figure:: img/multi_process_memory.*

   Memory Sharing in the DPDK Multi-process Sample Application


The EAL also supports an auto-detection mode (set by EAL ``--proc-type=auto`` flag ),
whereby a DPDK process is started as a secondary instance if a primary instance is already running.

Deployment Models
-----------------

Symmetric/Peer Processes
~~~~~~~~~~~~~~~~~~~~~~~~

DPDK multi-process support can be used to create a set of peer processes where each process performs the same workload.
This model is equivalent to having multiple threads each running the same main-loop function,
as is done in most of the supplied DPDK sample applications.
In this model, the first of the processes spawned should be spawned using the ``--proc-type=primary`` EAL flag,
while all subsequent instances should be spawned using the ``--proc-type=secondary`` flag.

The simple_mp and symmetric_mp sample applications demonstrate this usage model.
They are described in the "Multi-process Sample Application" chapter in the *DPDK Sample Application's User Guide*.

Asymmetric/Non-Peer Processes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An alternative deployment model that can be used for multi-process applications
is to have a single primary process instance that acts as a load-balancer or
server distributing received packets among worker or client threads, which are run as secondary processes.
In this case, extensive use of rte_ring objects is made, which are located in shared hugepage memory.

The client_server_mp sample application shows this usage model.
It is described in the "Multi-process Sample Application" chapter in the *DPDK Sample Application's User Guide*.

Running Multiple Independent DPDK Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to the above scenarios involving multiple DPDK processes working together,
it is possible to run multiple DPDK processes concurrently,
where those processes are all working independently.
Support for this usage scenario is provided using the ``--file-prefix`` parameter to the EAL.

The EAL puts shared runtime files in a directory based on standard conventions.
If ``$RUNTIME_DIRECTORY`` is defined in the environment,
it is used (as ``$RUNTIME_DIRECTORY/dpdk``).
Otherwise, if DPDK is run as root user, it uses ``/var/run/dpdk``
or if run as non-root user then the ``/tmp/dpdk`` (or ``$XDG_RUNTIME_DIRECTORY/dpdk``) is used.
Hugepage files on each hugetlbfs filesystem use the ``rtemap_X`` filename,
where X is in the range 0 to the maximum number of hugepages -1.
Similarly, it creates shared configuration files, memory mapped in each process,
using the ``.rte_config`` filename.
The rte part of the filenames of each of the above is configurable using the file-prefix parameter.

In addition to specifying the file-prefix parameter,
any DPDK applications that are to be run side-by-side must explicitly limit their memory use.
This is less of a problem on Linux, as by default, applications will not
allocate more memory than they need. However if ``--legacy-mem`` is used, DPDK
will attempt to preallocate all memory it can get to, and memory use must be
explicitly limited. This is done by passing the ``-m`` flag to each process to
specify how much hugepage memory, in megabytes, each process can use (or passing
``--socket-mem`` to specify how much hugepage memory on each socket each process
can use).

.. note::

    Independent DPDK instances running side-by-side on a single machine cannot share any network ports.
    Any network ports being used by one process should be blocked by every other process.

Running Multiple Independent Groups of DPDK Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the same way that it is possible to run independent DPDK applications side- by-side on a single system,
this can be trivially extended to multi-process groups of DPDK applications running side-by-side.
In this case, the secondary processes must use the same ``--file-prefix`` parameter
as the primary process whose shared memory they are connecting to.

.. note::

    All restrictions and issues with multiple independent DPDK processes running side-by-side
    apply in this usage scenario also.

Multi-process Limitations
-------------------------

There are a number of limitations to what can be done when running DPDK multi-process applications.
Some of these are documented below:

*   The multi-process feature requires that the exact same hugepage memory mappings be present in all applications.
    This makes secondary process startup process generally unreliable. Disabling
    Linux security feature - Address-Space Layout Randomization (ASLR) may
    help getting more consistent mappings, but not necessarily more reliable -
    if the mappings are wrong, they will be consistently wrong!

.. warning::

    Disabling Address-Space Layout Randomization (ASLR) may have security implications,
    so it is recommended that it be disabled only when absolutely necessary,
    and only when the implications of this change have been understood.

*   All DPDK processes running as a single application and using shared memory must have distinct coremask/corelist arguments.
    It is not possible to have a primary and secondary instance, or two secondary instances,
    using any of the same logical cores.
    Attempting to do so can cause corruption of memory pool caches, among other issues.

*   The delivery of interrupts, such as Ethernet* device link status interrupts, do not work in secondary processes.
    All interrupts are triggered inside the primary process only.
    Any application needing interrupt notification in multiple processes should provide its own mechanism
    to transfer the interrupt information from the primary process to any secondary process that needs the information.

*   The use of function pointers between multiple processes running based of different compiled binaries is not supported,
    since the location of a given function in one process may be different to its location in a second.
    This prevents the librte_hash library from behaving properly as in a multi-process instance,
    since it uses a pointer to the hash function internally.

To work around this issue, it is recommended that multi-process applications perform the hash calculations by directly calling
the hashing function from the code and then using the rte_hash_add_with_hash()/rte_hash_lookup_with_hash() functions
instead of the functions which do the hashing internally, such as rte_hash_add()/rte_hash_lookup().

*   Depending upon the hardware in use, and the number of DPDK processes used,
    it may not be possible to have HPET timers available in each DPDK instance.
    The minimum number of HPET comparators available to Linux* userspace can be just a single comparator,
    which means that only the first, primary DPDK process instance can open and mmap  /dev/hpet.
    If the number of required DPDK processes exceeds that of the number of available HPET comparators,
    the TSC (which is the default timer in this release) must be used as a time source across all processes instead of the HPET.

Communication between multiple processes
----------------------------------------

While there are multiple ways one can approach inter-process communication in
DPDK, there is also a native DPDK IPC API available. It is not intended to be
performance-critical, but rather is intended to be a convenient, general
purpose API to exchange short messages between primary and secondary processes.

DPDK IPC API supports the following communication modes:

* Unicast message from secondary to primary
* Broadcast message from primary to all secondaries

In other words, any IPC message sent in a primary process will be delivered to
all secondaries, while any IPC message sent in a secondary process will only be
delivered to primary process. Unicast from primary to secondary or from
secondary to secondary is not supported.

There are three types of communications that are available within DPDK IPC API:

* Message
* Synchronous request
* Asynchronous request

A "message" type does not expect a response and is meant to be a best-effort
notification mechanism, while the two types of "requests" are meant to be a two
way communication mechanism, with the requester expecting a response from the
other side.

Both messages and requests will trigger a named callback on the receiver side.
These callbacks will be called from within a dedicated IPC or interrupt thread
that are not part of EAL lcore threads.

Registering for incoming messages
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before any messages can be received, a callback will need to be registered.
This is accomplished by calling ``rte_mp_action_register()`` function. This
function accepts a unique callback name, and a function pointer to a callback
that will be called when a message or a request matching this callback name
arrives.

If the application is no longer willing to receive messages intended for a
specific callback function, ``rte_mp_action_unregister()`` function can be
called to ensure that callback will not be triggered again.

Sending messages
~~~~~~~~~~~~~~~~

To send a message, a ``rte_mp_msg`` descriptor must be populated first. The list
of fields to be populated are as follows:

* ``name`` - message name. This name must match receivers' callback name.
* ``param`` - message data (up to 256 bytes).
* ``len_param`` - length of message data.
* ``fds`` - file descriptors to pass long with the data (up to 8 fd's).
* ``num_fds`` - number of file descriptors to send.

Once the structure is populated, calling ``rte_mp_sendmsg()`` will send the
descriptor either to all secondary processes (if sent from primary process), or
to primary process (if sent from secondary process). The function will return
a value indicating whether sending the message succeeded or not.

Sending requests
~~~~~~~~~~~~~~~~

Sending requests involves waiting for the other side to reply, so they can block
for a relatively long time.

To send a request, a message descriptor ``rte_mp_msg`` must be populated.
Additionally, a ``timespec`` value must be specified as a timeout, after which
IPC will stop waiting and return.

For synchronous requests, the ``rte_mp_reply`` descriptor must also be created.
This is where the responses will be stored.
The list of fields that will be populated by IPC are as follows:

* ``nb_sent`` - number indicating how many requests were sent (i.e. how many
  peer processes were active at the time of the request).
* ``nb_received`` - number indicating how many responses were received (i.e. of
  those peer processes that were active at the time of request, how many have
  replied)
* ``msgs`` - pointer to where all of the responses are stored. The order in
  which responses appear is undefined. When doing synchronous requests, this
  memory must be freed by the requestor after request completes!

For asynchronous requests, a function pointer to the callback function must be
provided instead. This callback will be called when the request either has timed
out, or will have received a response to all the messages that were sent.

.. warning::

    When an asynchronous request times out, the callback will be called not by
    a dedicated IPC thread, but rather from EAL interrupt thread. Because of
    this, it may not be possible for DPDK to trigger another interrupt-based
    event (such as an alarm) while handling asynchronous IPC callback.

When the callback is called, the original request descriptor will be provided
(so that it would be possible to determine for which sent message this is a
callback to), along with a response descriptor like the one described above.
When doing asynchronous requests, there is no need to free the resulting
``rte_mp_reply`` descriptor.

Receiving and responding to messages
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To receive a message, a name callback must be registered using the
``rte_mp_action_register()`` function. The name of the callback must match the
``name`` field in sender's ``rte_mp_msg`` message descriptor in order for this
message to be delivered and for the callback to be trigger.

The callback's definition is ``rte_mp_t``, and consists of the incoming message
pointer ``msg``, and an opaque pointer ``peer``. Contents of ``msg`` will be
identical to ones sent by the sender.

If a response is required, a new ``rte_mp_msg`` message descriptor must be
constructed and sent via ``rte_mp_reply()`` function, along with ``peer``
pointer. The resulting response will then be delivered to the correct requestor.

.. warning::
    Simply returning a value when processing a request callback will not send a
    response to the request - it must always be explicitly sent even in case
    of errors. Implementation of error signalling rests with the application,
    there is no built-in way to indicate success or error for a request. Failing
    to do so will cause the requestor to time out while waiting on a response.

Misc considerations
~~~~~~~~~~~~~~~~~~~~~~~~

Due to the underlying IPC implementation being single-threaded, recursive
requests (i.e. sending a request while responding to another request) is not
supported. However, since sending messages (not requests) does not involve an
IPC thread, sending messages while processing another message or request is
supported.

Since the memory subsystem uses IPC internally, memory allocations and IPC must
not be mixed: it is not safe to use IPC inside a memory-related callback, nor is
it safe to allocate/free memory inside IPC callbacks. Attempting to do so may
lead to a deadlock.

Asynchronous request callbacks may be triggered either from IPC thread or from
interrupt thread, depending on whether the request has timed out. It is
therefore suggested to avoid waiting for interrupt-based events (such as alarms)
inside asynchronous IPC request callbacks. This limitation does not apply to
messages or synchronous requests.

If callbacks spend a long time processing the incoming requests, the requestor
might time out, so setting the right timeout value on the requestor side is
imperative.

If some of the messages timed out, ``nb_sent`` and ``nb_received`` fields in the
``rte_mp_reply`` descriptor will not have matching values. This is not treated
as error by the IPC API, and it is expected that the user will be responsible
for deciding how to handle such cases.

If a callback has been registered, IPC will assume that it is safe to call it.
This is important when registering callbacks during DPDK initialization.
During initialization, IPC will consider the receiving side as non-existing if
the callback has not been registered yet. However, once the callback has been
registered, it is expected that IPC should be safe to trigger it, even if the
rest of the DPDK initialization hasn't finished yet.
