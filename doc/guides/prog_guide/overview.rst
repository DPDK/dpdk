..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Overview
========

This section gives a global overview of the architecture of Data Plane Development Kit (DPDK).

The main goal of the DPDK is to provide a simple,
complete framework for fast packet processing in data plane applications.
Users may use the code to understand some of the techniques employed,
to build upon for prototyping or to add their own protocol stacks.
Alternative ecosystem options that use the DPDK are available.

The framework creates a set of libraries for specific environments
through the creation of an Environment Abstraction Layer (EAL),
which may be specific to a mode of the Intel® architecture (32-bit or 64-bit),
Linux* user space compilers or a specific platform.
These environments are created through the use of meson files and configuration files.
Once the EAL library is created, the user may link with the library to create their own applications.
Other libraries, outside of EAL, including the Hash,
Longest Prefix Match (LPM) and rings libraries are also provided.
Sample applications are provided to help show the user how to use various features of the DPDK.

The DPDK implements a run to completion model for packet processing,
where all resources must be allocated prior to calling Data Plane applications,
running as execution units on logical processing cores.
The model does not support a scheduler and all devices are accessed by polling.
The primary reason for not using interrupts is the performance overhead imposed by interrupt processing.

In addition to the run-to-completion model,
a pipeline model may also be used by passing packets or messages between cores via the rings.
This allows work to be performed in stages and may allow more efficient use of code on cores.

Development Environment
-----------------------

The DPDK project installation requires Linux and the associated toolchain,
such as one or more compilers, assembler, meson utility,
editor and various libraries to create the DPDK components and libraries.

Once these libraries are created for the specific environment and architecture,
they may then be used to create the user's data plane application.

When creating applications for the Linux user space, the glibc library is used.

See the *DPDK Getting Started Guide* for information on setting up the development environment.

Environment Abstraction Layer
-----------------------------

The Environment Abstraction Layer (EAL) provides a generic interface
that hides the environment specifics from the applications and libraries.
The services provided by the EAL are:

*   DPDK loading and launching

*   Support for multi-process and multi-thread execution types

*   Core affinity/assignment procedures

*   System memory allocation/de-allocation

*   Atomic/lock operations

*   Time reference

*   PCI bus access

*   Trace and debug functions

*   CPU feature identification

*   Interrupt handling

*   Alarm operations

*   Memory management (malloc)

The EAL is fully described in :ref:`Environment Abstraction Layer <Environment_Abstraction_Layer>`.

Core Components
---------------

The *core components* are a set of libraries that provide all the elements needed
for high-performance packet processing applications.

.. _figure_architecture-overview:

.. figure:: img/architecture-overview.*

   Core Components Architecture


Ring Manager (librte_ring)
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ring structure provides a lockless multi-producer, multi-consumer FIFO API in a finite size table.
It has some advantages over lockless queues; easier to implement, adapted to bulk operations and faster.
A ring is used by the :ref:`Memory Pool Manager (librte_mempool) <Mempool_Library>`
and may be used as a general communication mechanism between cores
and/or execution blocks connected together on a logical core.

This ring buffer and its usage are fully described in :ref:`Ring Library <Ring_Library>`.

Memory Pool Manager (librte_mempool)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Memory Pool Manager is responsible for allocating pools of objects in memory.
A pool is identified by name and uses a ring to store free objects.
It provides some other optional services,
such as a per-core object cache and an alignment helper to ensure that objects are padded to spread them equally on all RAM channels.

This memory pool allocator is described in  :ref:`Mempool Library <Mempool_Library>`.

Network Packet Buffer Management (librte_mbuf)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The mbuf library provides the facility to create and destroy buffers
that may be used by the DPDK application to store message buffers.
The message buffers are created at startup time and stored in a mempool, using the DPDK mempool library.

This library provides an API to allocate/free mbufs, manipulate
packet buffers which are used to carry network packets.

Network Packet Buffer Management is described in :ref:`Mbuf Library <Mbuf_Library>`.

Timer Manager (librte_timer)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This library provides a timer service to DPDK execution units,
providing the ability to execute a function asynchronously.
It can be periodic function calls, or just a one-shot call.
It uses the timer interface provided by the Environment Abstraction Layer (EAL)
to get a precise time reference and can be initiated on a per-core basis as required.

The library documentation is available in :ref:`Timer Library <Timer_Library>`.

Ethernet* Poll Mode Driver Architecture
---------------------------------------

The DPDK includes Poll Mode Drivers (PMDs) for 1 GbE, 10 GbE and 40GbE, and para virtualized virtio
Ethernet controllers which are designed to work without asynchronous, interrupt-based signaling mechanisms.

See  :ref:`Poll Mode Driver <Poll_Mode_Driver>`.

Packet Forwarding Algorithm Support
-----------------------------------

The DPDK includes Hash (librte_hash) and Longest Prefix Match (LPM,librte_lpm)
libraries to support the corresponding packet forwarding algorithms.

See :ref:`Hash Library <Hash_Library>` and  :ref:`LPM Library <LPM_Library>` for more information.

librte_net
----------

The librte_net library is a collection of IP protocol definitions and convenience macros.
It is based on code from the FreeBSD* IP stack and contains protocol numbers (for use in IP headers),
IP-related macros, IPv4/IPv6 header structures and TCP, UDP and SCTP header structures.
