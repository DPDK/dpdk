..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Glossary
========


ACL
   Access Control List

API
   Application Programming Interface

ASLR
   Linux* kernel Address-Space Layout Randomization

BSD
   Berkeley Software Distribution

Clr
   Clear

CIDR
   Classless Inter-Domain Routing

Control Plane
   The control plane is concerned with the routing of packets and with
   providing a start or end point.

Core
   A core may include several lcores or threads if the processor supports
   hyperthreading.

Core Components
   A set of libraries provided by the DPDK, including eal, ring, mempool,
   mbuf, timers, and so on.

CPU
   Central Processing Unit

CRC
   Cyclic Redundancy Check

Data Plane
   In contrast to the control plane, the data plane in a network architecture
   are the layers involved when forwarding packets.  These layers must be
   highly optimized to achieve good performance.

DIMM
   Dual In-line Memory Module

Doxygen
   A documentation generator used in the DPDK to generate the API reference.

DPDK
   Data Plane Development Kit

DRAM
   Dynamic Random Access Memory

EAL
   The Environment Abstraction Layer (EAL) provides a generic interface that
   hides the environment specifics from the applications and libraries.  The
   services expected from the EAL are: development kit loading and launching,
   core affinity/ assignment procedures, system memory allocation/description,
   PCI bus access, inter-partition communication.

FIFO
   First In First Out

FPGA
   Field Programmable Gate Array

GbE
   Gigabit Ethernet

HW
   Hardware

HPET
   High Precision Event Timer; a hardware timer that provides a precise time
   reference on x86 platforms.

ID
   Identifier

IOCTL
   Input/Output Control

I/O
   Input/Output

IP
   Internet Protocol

IPv4
   Internet Protocol version 4

IPv6
   Internet Protocol version 6

lcore
   A logical execution unit of the processor, sometimes called a *hardware
   thread*.

KNI
   Kernel Network Interface

L1
   Layer 1

L2
   Layer 2

L3
   Layer 3

L4
   Layer 4

LAN
   Local Area Network

LPM
   Longest Prefix Match

main lcore
   The execution unit that executes the main() function and that launches
   other lcores.

master lcore
   Deprecated name for *main lcore*. No longer used.

mbuf
   An mbuf is a data structure used internally to carry messages (mainly
   network packets).  The name is derived from BSD stacks.  To understand the
   concepts of packet buffers or mbuf, refer to *TCP/IP Illustrated, Volume 2:
   The Implementation*.

MESI
   Modified Exclusive Shared Invalid (CPU cache coherency protocol)

MTU
   Maximum Transfer Unit

NIC
   Network Interface Card

OOO
   Out Of Order (execution of instructions within the CPU pipeline)

NUMA
   Non-uniform Memory Access

PCI
   Peripheral Connect Interface

PHY
   An abbreviation for the physical layer of the OSI model.

pktmbuf
   An *mbuf* carrying a network packet.

PMD
   Poll Mode Driver

QoS
   Quality of Service

RCU
   Read-Copy-Update algorithm, an alternative to simple rwlocks.

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

Slave lcore
   Deprecated name for *worker lcore*. No longer used.

Socket
   A physical CPU, that includes several *cores*.

SLA
   Service Level Agreement

srTCM
   Single Rate Three Color Marking

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
   Translation Lookaside Buffer

TLS
   Thread Local Storage

trTCM
   Two Rate Three Color Marking

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
   Weighted Random Early Detection

WRR
   Weighted Round Robin
