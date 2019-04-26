..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Test Pipeline Application
=========================

The Test Pipeline application illustrates the use of the DPDK Packet Framework tool suite.
Its purpose is to demonstrate the performance of single-table DPDK pipelines.

Overview
--------

The application uses three CPU cores:

*   Core A ("RX core") receives traffic from the NIC ports and feeds core B with traffic through SW queues.

*   Core B ("Pipeline core") implements a single-table DPDK pipeline
    whose type is selectable through specific command line parameter.
    Core B receives traffic from core A through software queues,
    processes it according to the actions configured in the table entries that
    are hit by the input packets and feeds it to core C through another set of software queues.

*   Core C ("TX core") receives traffic from core B through software queues and sends it to the NIC ports for transmission.

.. _figure_test_pipeline_app:

.. figure:: img/test_pipeline_app.*

   Test Pipeline Application

Compiling the Application
-------------------------
To compile the sample application see :doc:`compiling`

The application is located in the ``$RTE_SDK/app/test-pipeline`` directory.


Running the Application
-----------------------

Application Command Line
~~~~~~~~~~~~~~~~~~~~~~~~

The application execution command line is:

.. code-block:: console

    ./test-pipeline [EAL options] -- -p PORTMASK --TABLE_TYPE

The -c or -l EAL CPU coremask/corelist option has to contain exactly 3 CPU cores.
The first CPU core in the core mask is assigned for core A, the second for core B and the third for core C.

The PORTMASK parameter must contain 2 or 4 ports.

Table Types and Behavior
~~~~~~~~~~~~~~~~~~~~~~~~

:numref:`table_test_pipeline_1` describes the table types used and how they are populated.

The hash tables are pre-populated with 16 million keys.
For hash tables, the following parameters can be selected:

*   **Configurable key size implementation or fixed (specialized) key size implementation (e.g. hash-8-ext or hash-spec-8-ext).**
    The key size specialized implementations are expected to provide better performance for 8-byte and 16-byte key sizes,
    while the key-size-non-specialized implementation is expected to provide better performance for larger key sizes;

*   **Key size (e.g. hash-spec-8-ext or hash-spec-16-ext).**
    The available options are 8, 16 and 32 bytes;

*   **Table type (e.g. hash-spec-16-ext or hash-spec-16-lru).**
    The available options are ext (extendable bucket) or lru (least recently used).

.. _table_test_pipeline_1:

.. table:: Table Types

   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | **#** | **TABLE_TYPE**         | **Description of Core B Table**                          | **Pre-added Table Entries**                           |
   |       |                        |                                                          |                                                       |
   +=======+========================+==========================================================+=======================================================+
   | 1     | none                   | Core B is not implementing a DPDK pipeline.              | N/A                                                   |
   |       |                        | Core B is implementing a pass-through from its input set |                                                       |
   |       |                        | of software queues to its output set of software queues. |                                                       |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 2     | stub                   | Stub table. Core B is implementing the same pass-through | N/A                                                   |
   |       |                        | functionality as described for the "none" option by      |                                                       |
   |       |                        | using the DPDK Packet Framework by using one             |                                                       |
   |       |                        | stub table for each input NIC port.                      |                                                       |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 3     | hash-[spec]-8-lru      | LRU hash table with 8-byte key size and 16 million       | 16 million entries are successfully added to the      |
   |       |                        | entries.                                                 | hash table with the following key format:             |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [4-byte index, 4 bytes of 0]                          |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The action configured for all table entries is        |
   |       |                        |                                                          | "Sendto output port", with the output port index      |
   |       |                        |                                                          | uniformly distributed for the range of output ports.  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The default table rule (used in the case of a lookup  |
   |       |                        |                                                          | miss) is to drop the packet.                          |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | At run time, core A is creating the following lookup  |
   |       |                        |                                                          | key and storing it into the packet meta data for      |
   |       |                        |                                                          | core B to use for table lookup:                       |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [destination IPv4 address, 4 bytes of 0]              |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 4     | hash-[spec]-8-ext      | Extendable bucket hash table with 8-byte key size        | Same as hash-[spec]-8-lru table entries, above.       |
   |       |                        | and 16 million entries.                                  |                                                       |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 5     | hash-[spec]-16-lru     | LRU hash table with 16-byte key size and 16 million      | 16 million entries are successfully added to the hash |
   |       |                        | entries.                                                 | table with the following key format:                  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [4-byte index, 12 bytes of 0]                         |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The action configured for all table entries is        |
   |       |                        |                                                          | "Send to output port", with the output port index     |
   |       |                        |                                                          | uniformly distributed for the range of output ports.  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The default table rule (used in the case of a lookup  |
   |       |                        |                                                          | miss) is to drop the packet.                          |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | At run time, core A is creating the following lookup  |
   |       |                        |                                                          | key and storing it into the packet meta data for core |
   |       |                        |                                                          | B to use for table lookup:                            |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [destination IPv4 address, 12 bytes of 0]             |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 6     | hash-[spec]-16-ext     | Extendable bucket hash table with 16-byte key size       | Same as hash-[spec]-16-lru table entries, above.      |
   |       |                        | and 16 million entries.                                  |                                                       |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 7     | hash-[spec]-32-lru     | LRU hash table with 32-byte key size and 16 million      | 16 million entries are successfully added to the hash |
   |       |                        | entries.                                                 | table with the following key format:                  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [4-byte index, 28 bytes of 0].                        |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The action configured for all table entries is        |
   |       |                        |                                                          | "Send to output port", with the output port index     |
   |       |                        |                                                          | uniformly distributed for the range of output ports.  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The default table rule (used in the case of a lookup  |
   |       |                        |                                                          | miss) is to drop the packet.                          |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | At run time, core A is creating the following lookup  |
   |       |                        |                                                          | key and storing it into the packet meta data for      |
   |       |                        |                                                          | Lpmcore B to use for table lookup:                    |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [destination IPv4 address, 28 bytes of 0]             |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 8     | hash-[spec]-32-ext     | Extendable bucket hash table with 32-byte key size       | Same as hash-[spec]-32-lru table entries, above.      |
   |       |                        | and 16 million entries.                                  |                                                       |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 9     | lpm                    | Longest Prefix Match (LPM) IPv4 table.                   | In the case of two ports, two routes                  |
   |       |                        |                                                          | are added to the table:                               |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.0.0.0/9 => send to output port 0]                  |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.128.0.0/9 => send to output port 1]                |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | In case of four ports, four entries are added to the  |
   |       |                        |                                                          | table:                                                |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.0.0.0/10 => send to output port 0]                 |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.64.0.0/10 => send to output port 1]                |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.128.0.0/10 => send to output port 2]               |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [0.192.0.0/10 => send to output port 3]               |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The default table rule (used in the case of a lookup  |
   |       |                        |                                                          | miss) is to drop the packet.                          |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | At run time, core A is storing the IPv4 destination   |
   |       |                        |                                                          | within the packet meta data to be later used by core  |
   |       |                        |                                                          | B as the lookup key.                                  |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+
   | 10    | acl                    | Access Control List (ACL) table                          | In the case of two ports, two ACL rules are added to  |
   |       |                        |                                                          | the table:                                            |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [priority = 0 (highest),                              |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | IPv4 source = ANY,                                    |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | IPv4 destination = 0.0.0.0/9,                         |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | L4 protocol = ANY,                                    |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | TCP source port = ANY,                                |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | TCP destination port = ANY                            |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | => send to output port 0]                             |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | [priority = 0 (highest),                              |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | IPv4 source = ANY,                                    |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | IPv4 destination = 0.128.0.0/9,                       |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | L4 protocol = ANY,                                    |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | TCP source port = ANY,                                |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | TCP destination port = ANY                            |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | => send to output port 0].                            |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          |                                                       |
   |       |                        |                                                          | The default table rule (used in the case of a lookup  |
   |       |                        |                                                          | miss) is to drop the packet.                          |
   |       |                        |                                                          |                                                       |
   +-------+------------------------+----------------------------------------------------------+-------------------------------------------------------+

Input Traffic
~~~~~~~~~~~~~

Regardless of the table type used for the core B pipeline,
the same input traffic can be used to hit all table entries with uniform distribution,
which results in uniform distribution of packets sent out on the set of output NIC ports.
The profile for input traffic is TCP/IPv4 packets with:

*   destination IP address as A.B.C.D with A fixed to 0 and B, C,D random

*   source IP address fixed to 0.0.0.0

*   destination TCP port fixed to 0

*   source TCP port fixed to 0
