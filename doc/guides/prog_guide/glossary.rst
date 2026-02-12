..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Glossary
========

ACL
   An `Access Control List (ACL)
   <https://en.wikipedia.org/wiki/Access-control_list#Networking_ACLs>`_
   is a set of rules that define who can access a resource and what actions they can perform.

API
   Application Programming Interface

ASLR
   `Address-Space Layout Randomization (ASLR)
   <https://en.wikipedia.org/wiki/Address_space_layout_randomization>`_
   is a security technique that protects against buffer overflow attacks
   by randomizing the location of executables in memory.

BSD
   `Berkeley Software Distribution (BSD)
   <https://en.wikipedia.org/wiki/Berkeley_Software_Distribution>`_
   is a version of Unix operating system.

Clr
   Clear

CIDR
   `Classless Inter-Domain Routing (CIDR)
   <https://datatracker.ietf.org/doc/html/rfc1918>`_
   is a method of assigning IP address
   that improves data routing efficiency on the internet
   and is used in IPv4 and IPv6.

Control Plane
   A `Control Plane
   <https://en.wikipedia.org/wiki/Control_plane>`_
   is a concept in networking that refers to the part of the system
   responsible for managing and making decisions
   about where and how data packets are forwarded within a network.

Core
   A core may include several lcores or threads if the processor supports
   `Simultaneous Multi-Threading (SMT)
   <https://en.wikipedia.org/wiki/Simultaneous_multithreading>`_.

Core Components
   A set of libraries provided by DPDK which are used by nearly all applications
   and upon which other DPDK libraries and drivers depend.
   For example, EAL, ring, mempool and mbuf.

CPU
   Central Processing Unit

CRC
   `Cyclic Redundancy Check (CRC)
   <https://en.wikipedia.org/wiki/Cyclic_redundancy_check>`_
   is an algorithm that detects errors in data transmission and storage.

Data Plane
   In contrast to the control plane, the `data plane
   <https://en.wikipedia.org/wiki/Data_plane>`_
   in a network architecture includes the layers involved
   when processing and forwarding data packets between communicating endpoints.
   These layers must be highly optimized to achieve good performance.

DIMM
   A `Dual In-line Memory Module (DIMM)
   <https://en.wikipedia.org/wiki/DIMM>`_
   is a module containing one or several Random Access Memory (RAM)
   or Dynamic RAM (DRAM) chips on a printed circuit board
   that connects it directly to the computer motherboard.

Doxygen
   `Doxygen <https://www.doxygen.nl>`_ is a documentation generator
   used in DPDK to generate the API reference.

DPDK
   Data Plane Development Kit

DRAM
   `Dynamic Random Access Memory (DRAM)
   <https://en.wikipedia.org/wiki/Dynamic_random-access_memory>`_
   is a type of Random Access Memory (RAM)
   that is used in computers to temporarily store information.

EAL
   :doc:`Environment Abstraction Layer (EAL) <env_abstraction_layer>`
   is the core DPDK library that provides a generic interface
   that hides the environment specifics from the applications and libraries.

EAL Thread
   An EAL thread is typically a thread that runs packet processing tasks.
   These threads are often pinned to logical cores (lcores),
   which helps to ensure that packet processing tasks are executed
   with minimal interruption and maximal performance
   by utilizing specific CPU resources dedicated to those tasks.
   EAL threads can also handle other tasks like managing buffers, queues, and I/O operations.

FIFO
   `First In First Out (FIFO)
   <https://en.wikipedia.org/wiki/FIFO_(computing_and_electronics)>`_
   is a method for manipulating a data structure
   where the oldest (first) entry, or "head" of the queue, is processed first.

FPGA
   `Field Programmable Gate Array (FPGA)
   <https://en.wikipedia.org/wiki/Field-programmable_gate_array>`_
   i an integrated circuit with a programmable hardware fabric
   that can be reconfigured to suit different purposes.

FW
   Firmware

GbE
   Gigabit Ethernet

HW
   Hardware

HPET
   High Precision Event Timer; a hardware timer that provides a precise time
   reference on x86 platforms.

Huge Pages
   `Huge pages
   <https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html>`_
   are memory page sizes, larger than the default page size,
   which are supported by the host CPU.
   These pages are generally megabytes or even gigabytes in size, depending on platform,
   compared to the default page size on most platforms which is measured in kilobytes, e.g. 4k.
   Where the operating system provides access to huge page memory,
   DPDK will take advantage of those huge pages for increased performance.

ID
   Identifier

IOCTL
   Input/Output Control (IOCTL)
   is a system call that allows applications to communicate with device drivers
   to perform specific input/output operations.

I/O
   Input/Output

IP
   Internet Protocol

IPv4
   Internet Protocol version 4

IPv6
   Internet Protocol version 6

lcore
   A logical execution unit of the processor,
   sometimes called a hardware thread or EAL thread;
   also known as logical core.

L1
   Layer 1 - `Physical Layer
   <https://en.wikipedia.org/wiki/Physical_layer>`_
   is responsible for sending and receiving signals to transmit bits.

L2
   Layer 2 - `Datalink Layer
   <https://en.wikipedia.org/wiki/Data_link_layer>`_
   is responsible for local delivery of frames between nodes.
   Example: Ethernet.

L3
   Layer 3 - `Network Layer
   <https://en.wikipedia.org/wiki/Network_layer>`_
   is responsible for packet routing.
   Example: IP.

L4
   Layer 4 - `Transport Layer
   <https://en.wikipedia.org/wiki/Transport_layer>`_
   is responsible for datagram or segment communication.
   Examples include UDP and TCP.

LAN
   Local Area Network

LPM
   `Longest Prefix Match
   <https://en.wikipedia.org/wiki/Longest_prefix_match>`_
   is an IP routing lookup algorithm
   where the entry selected is that which matches the longest prefix of the lookup key.

main lcore
   The logical core or thread that executes the ``main()`` function
   and that launches tasks on other logical cores used by the application.

mbuf
   An mbuf is a data structure used internally to carry messages (mainly
   network packets).  The name is derived from BSD stacks.  To understand the
   concepts of packet buffers or mbuf, refer to *TCP/IP Illustrated, Volume 2:
   The Implementation*.

MTU
   `Maximum Transfer Unit (MTU)
   <https://en.wikipedia.org/wiki/Maximum_transmission_unit>`_
   is the size of the largest protocol data unit (PDU)
   that can be communicated in a single network layer transaction.
   In general, it relates to Ethernet frame size.

NIC
   `Network Interface Card (NIC)
   <https://en.wikipedia.org/wiki/Maximum_transmission_unit>`_
   is a hardware component, usually a circuit board or chip,
   installed on a computer so it can connect to a network.

OOO
   Out Of Order (execution of instructions within the CPU pipeline)

NUMA
   `Non-Uniform Memory Access (NUMA)
   <https://en.wikipedia.org/wiki/Non-uniform_memory_access>`_
   is a memory design where locality has an impact.

PCI
   Peripheral Connect Interface

PHY
   An abbreviation for the physical layer of the OSI model.

PIE
   Proportional Integral Controller Enhanced (RFC8033)

pktmbuf
   An *mbuf* carrying a network packet.

PMD
   A Poll Mode Driver (PMD) is a driver in DPDK,
   continuously polling as default behavior instead of waiting for a HW interrupt.

PMU
   Performance Monitoring Unit

QoS
   Quality of Service

RCU
   Read-Copy-Update algorithm, an alternative to simple rwlocks.
   It is a synchronization mechanism that allows multiple threads
   to read and update shared data structures without using locks.

Rd
   Read

RED
   Random Early Detection

RSS
   Receive Side Scaling

RTE
   Run Time Environment. Provides a fast and simple framework for fast packet
   processing, in a lightweight environment as a Linux* application and using
   Poll Mode Drivers (PMDs) to increase speed.

Rx
   Reception

Socket
   For historical reasons, the term "socket" is used in the DPDK
   to refer to both physical sockets, as well as NUMA nodes.
   As a general rule, the term should be understood to mean "NUMA node"
   unless it is clear from context that it is referring to physical CPU sockets.

SLA
   Service Level Agreement

srTCM
   `Single Rate Three Color Marking (srTCM)
   <https://datatracker.ietf.org/doc/html/rfc2697>`_
   is a metering technique marking packets either green, yellow, or red.

SRTD
   Scheduler Round Trip Delay

SW
   Software

Target
   In the DPDK, the target is a combination of architecture, machine,
   executive environment and toolchain.  For example:
   i686-native-linux-gcc.

TCP
   Transmission Control Protocol

TC
   Traffic Class

TLB
   Translation Lookaside Buffer (TLB)
   is memory cache that stores the recent translations of virtual memory to physical memory
   to enable faster retrieval.

TLS
   `Thread Local Storage (TLS)
   <https://en.wikipedia.org/wiki/Thread-local_storage>`_
   is memory local to a thread.
   TLS can also relates to the :ref:`cryptographic protocol <howto_security_tls>`.

trTCM
   `Two Rate Three Color Marking (trTCM)
   <https://datatracker.ietf.org/doc/html/rfc2698>`
   is a metering technique based on committed and peak rates,
   and marking packets either green, yellow, or red.

TSC
   Time Stamp Counter

Tx
   Transmission

TUN/TAP
   TUN and TAP are virtual network kernel devices.

VLAN
   Virtual Local Area Network

Wr
   Write

Worker lcore
   Any *lcore* that is not the *main lcore*.

WRED
   Weighted Random Early Detection (WRED)
   is a queueing discipline that allows the router to drop random packets to prevent tail drop.
   This is helpful for TCP/IP connections.

WRR
   Weighted Round Robin (WRR)
   is a scheduling algorithm used to distribute workloads
   across multiple resources based on assigned weights.
