..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Packet Framework Library
========================

Design Objectives
-----------------

The main design objectives for the DPDK Packet Framework are:

*   Provide standard methodology to build complex packet processing pipelines.
    Provide reusable and extensible templates for the commonly used pipeline functional blocks;

*   Provide capability to switch between pure software and hardware-accelerated implementations for the same pipeline functional block;

*   Provide the best trade-off between flexibility and performance.
    Hardcoded pipelines usually provide the best performance, but are not flexible,
    while developing flexible frameworks is never a problem, but performance is usually low;

*   Provide a framework that is logically similar to Open Flow.

Overview
--------

Packet processing applications are frequently structured as pipelines of multiple stages,
with the logic of each stage glued around a lookup table.
For each incoming packet, the table defines the set of actions to be applied to the packet,
as well as the next stage to send the packet to.

The DPDK Packet Framework minimizes the development effort required to build packet processing pipelines
by defining a standard methodology for pipeline development,
as well as providing libraries of reusable templates for the commonly used pipeline blocks.

The pipeline is constructed by connecting the set of input ports with the set of output ports
through the set of tables in a tree-like topology.
As a result of lookup operation for the current packet in the current table,
one of the table entries (on lookup hit) or the default table entry (on lookup miss)
provides the set of actions to be applied on the current packet,
as well as the next hop for the packet, which can be either another table, an output port or packet drop.

An example of packet processing pipeline is presented in :numref:`packet_framework_figure_32`:

.. _packet_framework_figure_32:

.. figure:: img/figure32.*

   Example of Packet Processing Pipeline where Input Ports 0 and 1
   are Connected with Output Ports 0, 1 and 2 through Tables 0 and 1


Port Library Design
-------------------

Port Types
~~~~~~~~~~

:numref:`packet_framework_table_qos_19` is a non-exhaustive list of ports
that can be implemented with the Packet Framework.

.. _packet_framework_table_qos_19:

.. table:: Port Types

   +---+------------------+---------------------------------------------------------------------------------------+
   | # | Port type        | Description                                                                           |
   |   |                  |                                                                                       |
   +===+==================+=======================================================================================+
   | 1 | SW ring          | SW circular buffer used for message passing between the application threads. Uses     |
   |   |                  | the DPDK rte_ring primitive. Expected to be the most commonly used type of            |
   |   |                  | port.                                                                                 |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 2 | HW ring          | Queue of buffer descriptors used to interact with NIC, switch or accelerator ports.   |
   |   |                  | For NIC ports, it uses the DPDK rte_eth_rx_queue or rte_eth_tx_queue                  |
   |   |                  | primitives.                                                                           |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 3 | IP reassembly    | Input packets are either IP fragments or complete IP datagrams. Output packets are    |
   |   |                  | complete IP datagrams.                                                                |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 4 | IP fragmentation | Input packets are jumbo (IP datagrams with length bigger than MTU) or non-jumbo       |
   |   |                  | packets. Output packets are non-jumbo packets.                                        |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 5 | Traffic manager  | Traffic manager attached to a specific NIC output port, performing congestion         |
   |   |                  | management and hierarchical scheduling according to pre-defined SLAs.                 |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 6 | Source           | Input port used as packet generator. Similar to Linux kernel /dev/zero character      |
   |   |                  | device.                                                                               |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 7 | Sink             | Output port used to drop all input packets. Similar to Linux kernel /dev/null         |
   |   |                  | character device.                                                                     |
   |   |                  |                                                                                       |
   +---+------------------+---------------------------------------------------------------------------------------+
   | 8 | Sym_crypto       | Output port used to extract DPDK Cryptodev operations from a fixed offset of the      |
   |   |                  | packet and then enqueue to the Cryptodev PMD. Input port used to dequeue the          |
   |   |                  | Cryptodev operations from the Cryptodev PMD and then retrieve the packets from them.  |
   +---+------------------+---------------------------------------------------------------------------------------+

Port Interface
~~~~~~~~~~~~~~

Each port is unidirectional, i.e. either input port or output port.
Each input/output port is required to implement an abstract interface that
defines the initialization and run-time operation of the port.
The port abstract interface is described below.

.. table:: 20 Port Abstract Interface

   +---+----------------+-----------------------------------------------------------------------------------------+
   | # | Port Operation | Description                                                                             |
   |   |                |                                                                                         |
   +===+================+=========================================================================================+
   | 1 | Create         | Create the low-level port object (e.g. queue). Can internally allocate memory.          |
   |   |                |                                                                                         |
   +---+----------------+-----------------------------------------------------------------------------------------+
   | 2 | Free           | Free the resources (e.g. memory) used by the low-level port object.                     |
   |   |                |                                                                                         |
   +---+----------------+-----------------------------------------------------------------------------------------+
   | 3 | RX             | Read a burst of input packets. Non-blocking operation. Only defined for input ports.    |
   |   |                |                                                                                         |
   +---+----------------+-----------------------------------------------------------------------------------------+
   | 4 | TX             | Write a burst of input packets. Non-blocking operation. Only defined for output ports.  |
   |   |                |                                                                                         |
   +---+----------------+-----------------------------------------------------------------------------------------+
   | 5 | Flush          | Flush the output buffer. Only defined for output ports.                                 |
   |   |                |                                                                                         |
   +---+----------------+-----------------------------------------------------------------------------------------+

The Software Switch (SWX) Pipeline
----------------------------------

The Software Switch (SWX) pipeline is designed to combine the DPDK performance with the flexibility of the P4-16 language [1]. It can be used either by itself
to code a complete software switch or data plane application, or in combination with the open-source P4 compiler P4C [2], acting as a P4C back-end that allows
the P4 programs to be translated to the DPDK SWX API and run on multi-core CPUs.

The main features of the SWX pipeline are:

*   Nothing is hard-wired, everything is dynamically defined: The packet headers (i.e. the network protocols), the packet meta-data, the actions, the tables
    and the pipeline itself are dynamically defined instead of selected from a predefined set.

*   Instructions: The actions and the life of the packet through the pipeline are defined with instructions that manipulate the pipeline objects mentioned
    above. The pipeline is the main function of the packet program, with actions as subroutines triggered by the tables.

*   Call external plugins: Extern objects and functions can be defined to call functionality that cannot be efficiently implemented with the existing
    pipeline-oriented instruction set, such as: error detecting/correcting codes, cryptographic operations, meters, statistics counter arrays, heuristics, etc.

*   Better control plane interaction: Transaction-oriented table update mechanism that supports multi-table atomic updates. Multiple tables can be updated in a
    single step with only the before-update and the after-update table entries visible to the packets. Alignment with the P4Runtime [3] protocol.

*   Performance: Multiple packets are in-flight within the pipeline at any moment. Each packet is owned by a different time-sharing thread in
    run-to-completion, with the thread pausing before memory access operations such as packet I/O and table lookup to allow the memory prefetch to complete.
    The instructions are verified and translated at initialization time with no run-time impact. The instructions are also optimized to detect and "fuse"
    frequently used patterns into vector-like instructions transparently to the user.

The main SWX pipeline components are:

*   Input and output ports: Each port instantiates a port type that defines the port operations, e.g. Ethernet device port, PCAP port, etc. The RX interface
    of the input ports and the TX interface of the output ports are single packet based, with packet batching typically implemented internally by each port for
    performance reasons.

*   Structure types: Each structure type is used to define the logical layout of a memory block, such as: packet headers, packet meta-data, action data stored
    in a table entry, mailboxes of extern objects and functions. Similar to C language structs, each structure type is a well defined sequence of fields, with
    each field having a unique name and a constant size.

*   Packet headers: Each packet typically has one or multiple headers. The headers are extracted from the input packet as part of the packet parsing operation,
    which is likely executed immediately after the packet reception. As result of the extract operation, each header is logically removed from the packet, so
    once the packet parsing operation is completed, the input packet is reduced to opaque payload. Just before transmission, one or several headers are pushed
    in front of each output packet through the emit operation; these headers can be part of the set of headers that were previously extracted from the input
    packet (and potentially modified afterwards) or some new headers whose contents is generated by the pipeline (e.g. by reading them from tables). The format
    of each packet header is defined by instantiating a structure type.

*   Packet meta-data: The packet meta-data is filled in by the pipeline (e.g. by reading it from tables) or computed by the pipeline. It is not sent out unless
    some of the meta-data fields are explicitly written into the headers emitted into the output packet. The format of the packet meta-data is defined by
    instantiating a structure type.

*   Extern objects and functions: Used to plug into the pipeline any functionality that cannot be efficiently implemented with the existing pipeline instruction
    set. Each extern object and extern function has its own mailbox, which is used to pass the input arguments to and retrieve the output arguments from the
    extern object member functions or the extern function.  The mailbox format is defined by instantiating a structure type.

*   Instructions: The pipeline and its actions are defined with instructions from a predefined instruction set. The instructions are used to receive and
    transmit the current packet, extract and emit headers from/into the packet, read/write the packet headers, packet meta-data and mailboxes, start table
    lookup operations, read the action arguments from the table entry, call extern object member functions or extern functions. See the rte_swx_pipeline.h file
    for the complete list of instructions.

*   Actions: The pipeline actions are dynamically defined through instructions as opposed to predefined. Essentially, the actions are subroutines of the
    pipeline program and their execution is triggered by the table lookup. The input arguments of each action are read from the table entry (in case of table
    lookup hit) or the default table action (in case of table lookup miss) and are read-only; their format is defined by instantiating a structure type. The
    actions have read-write access to the packet headers and meta-data.

*   Table: Each pipeline typically has one or more lookup tables. The match fields of each table are flexibly selected from the packet headers and meta-data
    defined for the current pipeline. The set of table actions is flexibly selected for each table from the set of actions defined for the current pipeline. The
    tables can be looked at as special pipeline operators that result in one of the table actions being called, depending on the result of the table lookup
    operation.

*   Pipeline: The pipeline represents the main program that defines the life of the packet, with subroutines (actions) executed on table lookup. As packets
    go through the pipeline, the packet headers and meta-data are transformed along the way.

References:

[1] P4-16 specification: https://p4.org/specs/

[2] P4-16 compiler: https://github.com/p4lang/p4c

[3] P4Runtime specification: https://p4.org/specs/
