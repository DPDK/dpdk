..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

.. _figure_multi_process_memory:

.. figure:: img/multi_process_memory.*

   Memory Sharing in the DPDK Multi-process Sample Application


The EAL also supports an auto-detection mode (set by EAL ``--proc-type=auto`` flag ),
whereby an DPDK process is started as a secondary instance if a primary instance is already running.

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
it is possible to run multiple DPDK processes side-by-side,
where those processes are all working independently.
Support for this usage scenario is provided using the ``--file-prefix`` parameter to the EAL.

By default, the EAL creates hugepage files on each hugetlbfs filesystem using the rtemap_X filename,
where X is in the range 0 to the maximum number of hugepages -1.
Similarly, it creates shared configuration files, memory mapped in each process, using the /var/run/.rte_config filename,
when run as root (or $HOME/.rte_config when run as a non-root user;
if filesystem and device permissions are set up to allow this).
The rte part of the filenames of each of the above is configurable using the file-prefix parameter.

In addition to specifying the file-prefix parameter,
any DPDK applications that are to be run side-by-side must explicitly limit their memory use.
This is done by passing the -m flag to each process to specify how much hugepage memory, in megabytes,
each process can use (or passing ``--socket-mem`` to specify how much hugepage memory on each socket each process can use).

.. note::

    Independent DPDK instances running side-by-side on a single machine cannot share any network ports.
    Any network ports being used by one process should be blacklisted in every other process.

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
    The Linux security feature - Address-Space Layout Randomization (ASLR) can interfere with this mapping,
    so it may be necessary to disable this feature in order to reliably run multi-process applications.

.. warning::

    Disabling Address-Space Layout Randomization (ASLR) may have security implications,
    so it is recommended that it be disabled only when absolutely necessary,
    and only when the implications of this change have been understood.

*   All DPDK processes running as a single application and using shared memory must have distinct coremask arguments.
    It is not possible to have a primary and secondary instance, or two secondary instances,
    using any of the same logical cores.
    Attempting to do so can cause corruption of memory pool caches, among other issues.

*   The delivery of interrupts, such as Ethernet* device link status interrupts, do not work in secondary processes.
    All interrupts are triggered inside the primary process only.
    Any application needing interrupt notification in multiple processes should provide its own mechanism
    to transfer the interrupt information from the primary process to any secondary process that needs the information.

*   The use of function pointers between multiple processes running based of different compiled binaries is not supported,
    since the location of a given function in one process may be different to its location in a second.
    This prevents the librte_hash library from behaving properly as in a multi-threaded instance,
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
