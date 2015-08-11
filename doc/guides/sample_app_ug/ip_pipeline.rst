..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
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

Internet Protocol (IP) Pipeline Application
===========================================

Application overview
--------------------

The *Internet Protocol (IP) Pipeline* application is intended to be a vehicle for rapid development of packet processing
applications running on multi-core CPUs.

The application provides a library of reusable functional blocks called pipelines.
These pipelines can be seen as prefabricated blocks that can be instantiated and inter-connected through packet queues
to create complete applications (super-pipelines).

Pipelines are created and inter-connected through the application configuration file.
By using different configuration files, different applications are effectively created, therefore this application
can be seen as an application generator.
The configuration of each pipeline can be updated at run-time through the application Command Line Interface (CLI).

Main application components are:

**A Library of reusable pipelines**

 * Each pipeline represents a functional block, e.g. flow classification, firewall, routing, master, etc.

 * Each pipeline type can be instantiated several times in the same application, which each instance configured
   separately and mapped to a single CPU core.
   Each CPU core can run one or several pipeline instances, which can be of same or different type.

 * Pipeline instances are inter-connected through packet queues (for packet processing) and message queues
   (for run-time configuration).

 * Pipelines are implemented using DPDK Packet Framework.

 * More pipeline types can always be built and added to the existing pipeline types.

**The Configuration file**

 * The configuration file defines the application structure.
   By using different configuration files, different applications are created.

 * All the application resources are created and configured through the application configuration file:
   pipeline instances, buffer pools, links (i.e. network interfaces), hardware device RX/TX queues,
   software queues, traffic manager devices, EAL startup arguments, etc.

 * The configuration file syntax is “define by reference”, meaning that resources are defined as they are referenced.
   First time a resource name is detected, it is registered with default parameters.
   Optionally, the resource parameters can be further refined through a configuration file section dedicated to
   that resource.

 * Command Line Interface (CLI)

**Global CLI commands: link configuration, etc.**

 * Common pipeline CLI commands: ping (keep-alive), statistics, etc.

 * Pipeline type specific CLI commands: used to configure instances of specific pipeline type.
   These commands are registered with the application when the pipeline type is registered.
   For example, the commands for routing pipeline instances include: route add, route delete, route list, etc.

 * CLI commands can be grouped into scripts that can be invoked at initialization and at runtime.


Design goals
------------


Rapid development
~~~~~~~~~~~~~~~~~

This application enables rapid development through quick connectivity of standard components called pipelines.
These components are built using DPDK Packet Framework and encapsulate packet processing features at different levels:
ports, tables, actions, pipelines and complete applications.

Pipeline instances are instantiated, configured and inter-connected through low complexity configuration files loaded
during application initialization.
Each pipeline instance is mapped to a single CPU core, with each CPU core able to run one or multiple pipeline
instances of same or different types. By loading a different configuration file, a different application is
effectively started.


Flexibility
~~~~~~~~~~~

Each packet processing application is typically represented as a chain of functional stages which is often called
the functional pipeline of the application.
These stages are mapped to CPU cores to create chains of CPU cores (pipeline model), clusters of CPU cores
(run-to-completion model) or chains of clusters of CPU cores (hybrid model).

This application allows all the above programming models.
By applying changes to the configuration file, the application provides the flexibility to reshuffle its
building blocks in different ways until the configuration providing the best performance is identified.


Move pipelines around
^^^^^^^^^^^^^^^^^^^^^

The mapping of pipeline instances to CPU cores can be reshuffled through the configuration file.
One or several pipeline instances can be mapped to the same CPU core.

.. _figure_ip_pipelines_1:

.. figure:: img/ip_pipelines_1.*

   Example of moving pipeline instances across different CPU cores


Move tables around
^^^^^^^^^^^^^^^^^^

There is some degree of flexibility for moving tables from one pipeline instance to another.
Based on the configuration arguments passed to each pipeline instance in the configuration file, specific tables
can be enabled or disabled.
This way, a specific table can be “moved” from pipeline instance A to pipeline instance B by simply disabling its
associated functionality for pipeline instance A while enabling it for pipeline instance B.

Due to requirement to have simple syntax for the configuration file, moving tables across different pipeline
instances is not as flexible as the mapping of pipeline instances to CPU cores, or mapping actions to pipeline tables.
Complete flexibility in moving tables from one pipeline to another could be achieved through a complex pipeline
description language that would detail the structural elements of the pipeline (ports, tables and actions) and
their connectivity, resulting in complex syntax for the configuration file, which is not acceptable.
Good configuration file readability through simple syntax is preferred.

*Example*: the IP routing pipeline can run the routing function only (with ARP function run by a different
pipeline instance), or it can run both the routing and ARP functions as part of the same pipeline instance.


.. _figure_ip_pipelines_2:

.. figure:: img/ip_pipelines_2.*

   Example of moving tables across different pipeline instances


Move actions around
^^^^^^^^^^^^^^^^^^^

When it makes sense, packet processing actions can be moved from one pipeline instance to another.
Based on the configuration arguments passed to each pipeline instance in the configuration file, specific actions
can be enabled or disabled.
This way, a specific action can be "moved" from pipeline instance A to pipeline instance B by simply disabling its
associated functionality for pipeline instance A while enabling it for pipeline instance B.

*Example*: The flow actions of accounting, traffic metering, application identification, NAT, etc can be run as part
of the flow classification pipeline instance or split across several flow actions pipeline instances, depending on
the number of flow instances and their compute requirements.


.. _figure_ip_pipelines_3:

.. figure:: img/ip_pipelines_3.*

   Example of moving actions across different tables and pipeline instances


Performance
~~~~~~~~~~~

Performance of the application is the highest priority requirement.
Flexibility is not provided at the expense of performance.

The purpose of flexibility is to provide an incremental development methodology that allows monitoring the
performance evolution:

* Apply incremental changes in the configuration (e.g. mapping on pipeline instances to CPU cores)
  in order to identify the configuration providing the best performance for a given application;

* Add more processing incrementally (e.g. by enabling more actions for specific pipeline instances) until
  the application is feature complete while checking the performance impact at each step.


Debug capabilities
~~~~~~~~~~~~~~~~~~

The application provides a significant set of debug capabilities:

* Command Line Interface (CLI) support for statistics polling: pipeline instance ping (keep-alive checks),
  pipeline instance statistics per input port/output port/table, link statistics, etc;

* Logging: Turn on/off application log messages based on priority level;

Running the application
-----------------------

The application startup command line is::

   ip_pipeline [-f CONFIG_FILE] [-s SCRIPT_FILE] -p PORT_MASK [-l LOG_LEVEL]

The application startup arguments are:

``-f CONFIG_FILE``

 * Optional: Yes

 * Default: ``./config/ip_pipeline.cfg``

 * Argument: Path to the configuration file to be loaded by the application.
   Please refer to the :ref:`ip_pipeline_configuration_file` for details on how to write the configuration file.

``-s SCRIPT_FILE``

 * Optional: Yes

 * Default: Not present

 * Argument: Path to the CLI script file to be run by the master pipeline at application startup.
   No CLI script file will be run at startup of this argument is not present.

``-p PORT_MASK``

 * Optional: No

 * Default: N/A

 * Argument: Hexadecimal mask of NIC port IDs to be used by the application.
   First port enabled in this mask will be referenced as LINK0 as part of the application configuration file,
   next port as LINK1, etc.

``-l LOG_LEVEL``

 * Optional: Yes

 * Default: 1 (High priority)

 * Argument: Log level to determine which application messages are to be printed to standard output.
   Available log levels are: 0 (None), 1 (High priority), 2 (Low priority).
   Only application messages whose priority is higher than or equal to the application log level will be printed.


Application stages
------------------


Configuration
~~~~~~~~~~~~~

During this stage, the application configuration file is parsed and its content is loaded into the application data
structures.
In case of any configuration file parse error, an error message is displayed and the application is terminated.
Please refer to the :ref:`ip_pipeline_configuration_file` for a description of the application configuration file format.


Configuration checking
~~~~~~~~~~~~~~~~~~~~~~

In the absence of any parse errors, the loaded content of application data structures is checked for overall consistency.
In case of any configuration check error, an error message is displayed and the application is terminated.


Initialization
~~~~~~~~~~~~~~

During this stage, the application resources are initialized and the handles to access them are saved into the
application data structures.
In case of any initialization error, an error message is displayed and the application is terminated.

The typical resources to be initialized are: pipeline instances, buffer pools, links (i.e. network interfaces),
hardware device RX/TX queues, software queues, traffic management devices, etc.


.. _ip_pipeline_runtime:

Run-time
~~~~~~~~

Each CPU core runs the pipeline instances assigned to it in time sharing mode and in round robin order:

1. *Packet processing task*: The pipeline run-time code is typically a packet *processing* task built on top of
   DPDK Packet Framework rte_pipeline library, which reads bursts of packets from the pipeline input ports,
   performs table lookups and executes the identified actions for all tables in the pipeline, with packet
   eventually written to pipeline output ports or dropped.

2. *Message handling task*: Each CPU core will also periodically execute the *message handling* code of each
   of the pipelines mapped to it.
   The pipeline message handling code is processing the messages that are pending in the pipeline input message
   queues, which are typically sent by the master CPU core for the on-the-fly pipeline configuration: check
   that pipeline is still alive (ping), add/delete entries in the pipeline tables, get statistics, etc.
   The frequency of executing the message handling code is usually much smaller than the frequency of executing
   the packet processing work.

Please refer to the :ref:`ip_pipeline_pipeline_section` for more details about the application pipeline module encapsulation.

.. _ip_pipeline_configuration_file:

Configuration file syntax
-------------------------

Syntax overview
~~~~~~~~~~~~~~~

The syntax of the configuration file is designed to be simple, which favors readability.
The configuration file is parsed using the DPDK library librte_cfgfile, which supports simple
`INI file format <http://en.wikipedia.org/wiki/INI_file>`__ for configuration files.

As result, the configuration file is split into several sections, with each section containing one or more entries.
The scope of each entry is its section, and each entry specifies a variable that is assigned a specific value.
Any text after the ``;`` character is considered a comment and is therefore ignored.

The following are application specific: number of sections, name of each section, number of entries of each section,
name of the variables used for each section entry, the value format (e.g. signed/unsigned integer, string, etc)
and range of each section entry variable.

Generic example of configuration file section:

.. code-block:: ini

    [<section_name>]

    <variable_name_1> = <value_1>

    ...

    <variable_name_N> = <value_N>


Application resources present in the configuration file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_resource_name:

.. tabularcolumns:: |p{4cm}|p{6cm}|p{6cm}|

.. table:: Application resource names in the configuration file

   +--------------------------+-----------------------------+-------------------------------------------------+
   | Resource type            | Format                      | Examples                                        |
   +==========================+=============================+=================================================+
   | Pipeline                 | ``PIPELINE<ID>``            | ``PIPELINE0``, ``PIPELINE1``                    |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Mempool                  | ``MEMPOOL<ID>``             | ``MEMPOOL0``, ``MEMPOOL1``                      |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Link (network interface) | ``LINK<ID>``                | ``LINK0``, ``LINK1``                            |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Link RX queue            | ``RXQ<LINK_ID>.<QUEUE_ID>`` | ``RXQ0.0``, ``RXQ1.5``                          |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Link TX queue            | ``TXQ<LINK_ID>.<QUEUE_ID>`` | ``TXQ0.0``, ``TXQ1.5``                          |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Software queue           | ``SWQ<ID>``                 | ``SWQ0``, ``SWQ1``                              |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Traffic Manager          | ``TM<LINK_ID>``             | ``TM0``, ``TM1``                                |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Source                   | ``SOURCE<ID>``              | ``SOURCE0``, ``SOURCE1``                        |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Sink                     | ``SINK<ID>``                | ``SINK0``, ``SINK1``                            |
   +--------------------------+-----------------------------+-------------------------------------------------+
   | Message queue            | ``MSGQ<ID>``                | ``MSGQ0``, ``MSGQ1``,                           |
   |                          | ``MSGQ-REQ-PIPELINE<ID>``   | ``MSGQ-REQ-PIPELINE2``, ``MSGQ-RSP-PIPELINE2,`` |
   |                          | ``MSGQ-RSP-PIPELINE<ID>``   | ``MSGQ-REQ-CORE-s0c1``, ``MSGQ-RSP-CORE-s0c1``  |
   |                          | ``MSGQ-REQ-CORE-<CORE_ID>`` |                                                 |
   |                          | ``MSGQ-RSP-CORE-<CORE_ID>`` |                                                 |
   +--------------------------+-----------------------------+-------------------------------------------------+

``LINK`` instances are created implicitly based on the ``PORT_MASK`` application startup argument.
``LINK0`` is the first port enabled in the ``PORT_MASK``, port 1 is the next one, etc.
The LINK ID is different than the DPDK PMD-level NIC port ID, which is the actual position in the bitmask mentioned above.
For example, if bit 5 is the first bit set in the bitmask, then ``LINK0`` is having the PMD ID of 5.
This mechanism creates a contiguous LINK ID space and isolates the configuration file against changes in the board
PCIe slots where NICs are plugged in.

``RXQ``, ``TXQ`` and ``TM`` instances have the LINK ID as part of their name.
For example, ``RXQ2.1``, ``TXQ2.1`` and ``TM2`` are all associated with ``LINK2``.


Rules to parse the configuration file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main rules used to parse the configuration file are:

1. Application resource name determines the type of resource based on the name prefix.

   *Example*: all software queues need to start with ``SWQ`` prefix, so ``SWQ0`` and ``SWQ5`` are valid software
   queue names.

2. An application resource is defined by creating a configuration file section with its name.
   The configuration file section allows fine tuning on any of the resource parameters.
   Some resource parameters are mandatory, in which case it is required to have them specified as part of the
   section, while some others are optional, in which case they get assigned their default value when not present.

   *Example*: section ``SWQ0`` defines a software queue named SWQ0, whose parameters are detailed as part of this section.

3. An application resource can also be defined by referencing it.
   Referencing a resource takes place by simply using its name as part of the value assigned to a variable in any
   configuration file section.
   In this case, the resource is registered with all its parameters having their default values.
   Optionally, a section with the resource name can be added to the configuration file to fine tune some or all
   of the resource parameters.

   *Example*: in section ``PIPELINE3``, variable ``pktq_in`` includes ``SWQ5`` as part of its list, which results
   in defining a software queue named ``SWQ5``; when there is no ``SWQ5`` section present in the configuration file,
   ``SWQ5`` gets registered with default parameters.


.. _ip_pipeline_pipeline_section:

PIPELINE section
~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_pipeline_section_1:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{1.5cm}|

.. table:: Configuration file PIPELINE section (1/2)

   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | Section       | Description                                               | Optional      | Range                  | Default value  |
   +===============+===========================================================+===============+========================+================+
   | type          | Pipeline type. Defines the functionality to be            | NO            | See "List              | N/A            |
   |               | executed.                                                 |               | of pipeline types"     |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | core          | CPU core to run the current pipeline.                     | YES           | See "CPU Core          | CPU socket 0,  |
   |               |                                                           |               | notation"              | core 0,        |
   |               |                                                           |               |                        | hyper-thread 0 |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | pktq_in       | Packet queues to serve as input ports for the             | YES           | List of input          | Empty list     |
   |               | current pipeline instance. The acceptable packet          |               | packet queue IDs       |                |
   |               | queue types are: ``RXQ``, ``SWQ``, ``TM`` and ``SOURCE``. |               |                        |                |
   |               | First device in this list is used as pipeline input port  |               |                        |                |
   |               | 0, second as pipeline input port 1, etc.                  |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | pktq_out      | Packet queues to serve as output ports for the            | YES           | List of output         | Empty list     |
   |               | current pipeline instance. The acceptable packet          |               | packet queue IDs.      |                |
   |               | queue types are: ``TXQ``, ``SWQ``, ``TM`` and ``SINK``.   |               |                        |                |
   |               | First device in this list is used as pipeline output      |               |                        |                |
   |               | port 0, second as pipeline output port 1, etc.            |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+

.. _table_ip_pipelines_pipeline_section_2:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{1.5cm}|

.. table:: Configuration file PIPELINE section (2/2)

   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | Section       | Description                                               | Optional      | Range                  | Default value  |
   +===============+===========================================================+===============+========================+================+
   | msgq_in       | Input message queues. These queues contain                | YES           | List of message        | Empty list     |
   |               | request messages that need to be handled by the           |               | queue IDs              |                |
   |               | current pipeline instance. The type and format of         |               |                        |                |
   |               | request messages is defined by the pipeline type.         |               |                        |                |
   |               | For each pipeline instance, there is an input             |               |                        |                |
   |               | message queue defined implicitly, whose name is:          |               |                        |                |
   |               | ``MSGQ-REQ-<PIPELINE_ID>``. This message queue            |               |                        |                |
   |               | should not be mentioned as part of msgq_in list.          |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | msgq_out      | Output message queues. These queues are used by           | YES           | List of message        | Empty list     |
   |               | the current pipeline instance to write response           |               | queue IDs              |                |
   |               | messages as result of request messages being              |               |                        |                |
   |               | handled. The type and format of response                  |               |                        |                |
   |               | messages is defined by the pipeline type.                 |               |                        |                |
   |               | For each pipeline instance, there is an output            |               |                        |                |
   |               | message queue defined implicitly, whose name is:          |               |                        |                |
   |               | ``MSGQ-RSP-<PIPELINE_ID>``. This message queue            |               |                        |                |
   |               | should not be mentioned as part of msgq_out list.         |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | timer_period  | Time period, measured in milliseconds,                    | YES           | milliseconds           | 1 ms           |
   |               | for handling the input message queues.                    |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+
   | <any other>   | Arguments to be passed to the current pipeline            | Depends on    | Depends on             | Depends on     |
   |               | instance. Format of the arguments, their type,            | pipeline type | pipeline type          | pipeline type  |
   |               | whether each argument is optional or mandatory            |               |                        |                |
   |               | and its default value (when optional) are defined         |               |                        |                |
   |               | by the pipeline type.                                     |               |                        |                |
   |               | The value of the arguments is applicable to the           |               |                        |                |
   |               | current pipeline instance only.                           |               |                        |                |
   +---------------+-----------------------------------------------------------+---------------+------------------------+----------------+


CPU core notation
^^^^^^^^^^^^^^^^^

The CPU Core notation is::

    <CPU core> ::= [s|S<CPU socket ID>][c|C]<CPU core ID>[h|H]

For example::

    CPU socket 0, core 0, hyper-thread 0: 0, c0, s0c0

    CPU socket 0, core 0, hyper-thread 1: 0h, c0h, s0c0h

    CPU socket 3, core 9, hyper-thread 1: s3c9h


MEMPOOL section
~~~~~~~~~~~~~~~

.. _table_ip_pipelines_mempool_section:

.. tabularcolumns:: |p{2.5cm}|p{6cm}|p{1.5cm}|p{1.5cm}|p{3cm}|

.. table:: Configuration file MEMPOOL section

   +---------------+-----------------------------------------------+----------+----------+---------------------------+
   | Section       | Description                                   | Optional | Type     | Default value             |
   +===============+===============================================+==========+==========+===========================+
   | buffer_size   | Buffer size (in bytes) for the current        | YES      | uint32_t | 2048                      |
   |               | buffer pool.                                  |          |          | + sizeof(struct rte_mbuf) |
   |               |                                               |          |          | + HEADROOM                |
   +---------------+-----------------------------------------------+----------+----------+---------------------------+
   | pool_size     | Number of buffers in the current buffer pool. | YES      | uint32_t | 32K                       |
   +---------------+-----------------------------------------------+----------+----------+---------------------------+
   | cache_size    | Per CPU thread cache size (in number of       | YES      | uint32_t | 256                       |
   |               | buffers) for the current buffer pool.         |          |          |                           |
   +---------------+-----------------------------------------------+----------+----------+---------------------------+
   | cpu           | CPU socket ID where to allocate memory for    | YES      | uint32_t | 0                         |
   |               | the current buffer pool.                      |          |          |                           |
   +---------------+-----------------------------------------------+----------+----------+---------------------------+


LINK section
~~~~~~~~~~~~

.. _table_ip_pipelines_link_section:

.. tabularcolumns:: |p{3cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{2cm}|

.. table:: Configuration file LINK section

   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | Section entry   | Description                                  | Optional | Type     | Default value     |
   +=================+==============================================+==========+==========+===================+
   | arp_q           | NIC RX queue where ARP packets should        | YES      | 0 .. 127 | 0 (default queue) |
   |                 | be filtered.                                 |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | tcp_syn_local_q | NIC RX queue where TCP packets with SYN      | YES      | 0 .. 127 | 0 (default queue) |
   |                 | flag should be filtered.                     |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | ip_local_q      | NIC RX queue where IP packets with local     | YES      | 0 .. 127 | 0 (default queue) |
   |                 | destination should be filtered.              |          |          |                   |
   |                 | When TCP, UDP and SCTP local queues are      |          |          |                   |
   |                 | defined, they take higher priority than this |          |          |                   |
   |                 | queue.                                       |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | tcp_local_q     | NIC RX queue where TCP packets with local    | YES      | 0 .. 127 | 0 (default queue) |
   |                 | destination should be filtered.              |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | udp_local_q     | NIC RX queue where TCP packets with local    | YES      | 0 .. 127 | 0 (default queue) |
   |                 | destination should be filtered.              |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | sctp_local_q    | NIC RX queue where TCP packets with local    | YES      | 0 .. 127 | 0 (default queue) |
   |                 | destination should be filtered.              |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+
   | promisc         | Indicates whether current link should be     | YES      | YES/NO   | YES               |
   |                 | started in promiscuous mode.                 |          |          |                   |
   +-----------------+----------------------------------------------+----------+----------+-------------------+


RXQ section
~~~~~~~~~~~

.. _table_ip_pipelines_rxq_section:

.. tabularcolumns:: |p{3cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{2cm}|

.. table:: Configuration file RXQ section

   +---------------+--------------------------------------------+----------+----------+---------------+
   | Section       | Description                                | Optional | Type     | Default value |
   +===============+============================================+==========+==========+===============+
   | mempool       | Mempool to use for buffer allocation for   | YES      | uint32_t | MEMPOOL0      |
   |               | current NIC RX queue. The mempool ID has   |          |          |               |
   |               | to be associated with a valid instance     |          |          |               |
   |               | defined in the mempool entry of the global |          |          |               |
   |               | section.                                   |          |          |               |
   +---------------+--------------------------------------------+----------+----------+---------------+
   | Size          | NIC RX queue size (number of descriptors)  | YES      | uint32_t | 128           |
   +---------------+--------------------------------------------+----------+----------+---------------+
   | burst         | Read burst size (number of descriptors)    | YES      | uint32_t | 32            |
   +---------------+--------------------------------------------+----------+----------+---------------+


TXQ section
~~~~~~~~~~~

.. _table_ip_pipelines_txq_section:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{2cm}|p{1.5cm}|

.. table:: Configuration file TXQ section

   +---------------+----------------------------------------------+----------+------------------+---------------+
   | Section       | Description                                  | Optional | Type             | Default value |
   +===============+==============================================+==========+==================+===============+
   | size          | NIC TX queue size (number of descriptors)    | YES      | uint32_t         | 512           |
   |               |                                              |          | power of 2       |               |
   |               |                                              |          | > 0              |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | burst         | Write burst size (number of descriptors)     | YES      | uint32_t         | 32            |
   |               |                                              |          | power of 2       |               |
   |               |                                              |          | 0 < burst < size |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | dropless      | When dropless is set to NO, packets can be   | YES      | YES/NO           | NO            |
   |               | dropped if not enough free slots are         |          |                  |               |
   |               | currently available in the queue, so the     |          |                  |               |
   |               | write operation to the queue is non-         |          |                  |               |
   |               | blocking.                                    |          |                  |               |
   |               | When dropless is set to YES, packets cannot  |          |                  |               |
   |               | be dropped if not enough free slots are      |          |                  |               |
   |               | currently available in the queue, so the     |          |                  |               |
   |               | write operation to the queue is blocking, as |          |                  |               |
   |               | the write operation is retried until enough  |          |                  |               |
   |               | free slots become available and all the      |          |                  |               |
   |               | packets are successfully written to the      |          |                  |               |
   |               | queue.                                       |          |                  |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | n_retries     | Number of retries. Valid only when dropless  | YES      | uint32_t         | 0             |
   |               | is set to YES. When set to 0, it indicates   |          |                  |               |
   |               | unlimited number of retries.                 |          |                  |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+


SWQ section
~~~~~~~~~~~

.. _table_ip_pipelines_swq_section:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{1.5cm}|

.. table:: Configuration file SWQ section

   +---------------+----------------------------------------------+----------+------------------+---------------+
   | Section       | Description                                  | Optional | Type             | Default value |
   +===============+==============================================+==========+==================+===============+
   | size          | Queue size (number of packets)               | YES      | uint32_t         | 256           |
   |               |                                              |          | power of 2       |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | burst_read    | Read burst size (number of packets)          | YES      | uint32_t         | 32            |
   |               |                                              |          | power of 2       |               |
   |               |                                              |          | 0 < burst < size |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | burst_write   | Write burst size (number of packets)         | YES      | uint32_t         | 32            |
   |               |                                              |          | power of 2       |               |
   |               |                                              |          | 0 < burst < size |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | dropless      | When dropless is set to NO, packets can be   | YES      | YES/NO           | NO            |
   |               | dropped if not enough free slots are         |          |                  |               |
   |               | currently available in the queue, so the     |          |                  |               |
   |               | write operation to the queue is non-         |          |                  |               |
   |               | blocking.                                    |          |                  |               |
   |               | When dropless is set to YES, packets cannot  |          |                  |               |
   |               | be dropped if not enough free slots are      |          |                  |               |
   |               | currently available in the queue, so the     |          |                  |               |
   |               | write operation to the queue is blocking, as |          |                  |               |
   |               | the write operation is retried until enough  |          |                  |               |
   |               | free slots become available and all the      |          |                  |               |
   |               | packets are successfully written to the      |          |                  |               |
   |               | queue.                                       |          |                  |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | n_retries     | Number of retries. Valid only when dropless  | YES      | uint32_t         | 0             |
   |               | is set to YES. When set to 0, it indicates   |          |                  |               |
   |               | unlimited number of retries.                 |          |                  |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+
   | cpu           | CPU socket ID where to allocate memory       | YES      | uint32_t         | 0             |
   |               | for this SWQ.                                |          |                  |               |
   +---------------+----------------------------------------------+----------+------------------+---------------+


TM section
~~~~~~~~~~

.. _table_ip_pipelines_tm_section:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{1.5cm}|

.. table:: Configuration file TM section

   +---------------+---------------------------------------------+----------+----------+---------------+
   | Section       | Description                                 | Optional | Type     | Default value |
   +===============+=============================================+==========+==========+===============+
   | Cfg           | File name to parse for the TM configuration | YES      | string   | tm_profile    |
   |               | to be applied. The syntax of this file is   |          |          |               |
   |               | described in the examples/qos_sched DPDK    |          |          |               |
   |               | application documentation.                  |          |          |               |
   +---------------+---------------------------------------------+----------+----------+---------------+
   | burst_read    | Read burst size (number of packets)         | YES      | uint32_t | 64            |
   +---------------+---------------------------------------------+----------+----------+---------------+
   | burst_write   | Write burst size (number of packets)        | YES      | uint32_t | 32            |
   +---------------+---------------------------------------------+----------+----------+---------------+


SOURCE section
~~~~~~~~~~~~~~

.. _table_ip_pipelines_source_section:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{2cm}|

.. table:: Configuration file SOURCE section

   +---------------+---------------------------------------+----------+----------+---------------+
   | Section       | Description                           | Optional | Type     | Default value |
   +===============+=======================================+==========+==========+===============+
   | Mempool       | Mempool to use for buffer allocation. | YES      | uint32_t | MEMPOOL0      |
   +---------------+---------------------------------------+----------+----------+---------------+
   | Burst         | Read burst size (number of packets)   |          | uint32_t | 32            |
   +---------------+---------------------------------------+----------+----------+---------------+


SINK section
~~~~~~~~~~~~

Currently, there are no parameters to be passed to a sink device, so
SINK section is not allowed.

MSGQ section
~~~~~~~~~~~~

.. _table_ip_pipelines_msgq_section:

.. tabularcolumns:: |p{2.5cm}|p{7cm}|p{1.5cm}|p{1.5cm}|p{1.5cm}|

.. table:: Configuration file MSGQ section

   +---------+--------------------------------------------+----------+------------+---------------+
   | Section | Description                                | Optional | Type       | Default value |
   +=========+============================================+==========+============+===============+
   | size    | Queue size (number of packets)             | YES      | uint32_t   | 64            |
   |         |                                            |          | != 0       |               |
   |         |                                            |          | power of 2 |               |
   +---------+--------------------------------------------+----------+------------+---------------+
   | cpu     | CPU socket ID where to allocate memory for | YES      | uint32_t   | 0             |
   |         | the current queue.                         |          |            |               |
   +---------+--------------------------------------------+----------+------------+---------------+


EAL section
~~~~~~~~~~~

The application generates the EAL parameters rather than reading them from the command line.

The CPU core mask parameter is generated based on the core entry of all PIPELINE sections.
All the other EAL parameters can be set from this section of the application configuration file.


Library of pipeline types
-------------------------

Pipeline module
~~~~~~~~~~~~~~~

A pipeline is a self-contained module that implements a packet processing function and is typically implemented on
top of the DPDK Packet Framework *librte_pipeline* library.
The application provides a run-time mechanism to register different pipeline types.

Depending on the required configuration, each registered pipeline type (pipeline class) is instantiated one or
several times, with each pipeline instance (pipeline object) assigned to one of the available CPU cores.
Each CPU core can run one or more pipeline instances, which might be of same or different types.
For more information of the CPU core threading model, please refer to the :ref:`ip_pipeline_runtime` section.


Pipeline type
^^^^^^^^^^^^^

Each pipeline type is made up of a back-end and a front-end. The back-end represents the packet processing engine
of the pipeline, typically implemented using the DPDK Packet Framework libraries, which reads packets from the
input packet queues, handles them and eventually writes them to the output packet queues or drops them.
The front-end represents the run-time configuration interface of the pipeline, which is exposed as CLI commands.
The front-end communicates with the back-end through message queues.

.. _table_ip_pipelines_back_end:

.. tabularcolumns:: |p{1cm}|p{2cm}|p{12cm}|

.. table:: Pipeline back-end

   +------------+------------------+--------------------------------------------------------------------+
   | Field name | Field type       | Description                                                        |
   +============+==================+====================================================================+
   | f_init     | Function pointer | Function to initialize the back-end of the current pipeline        |
   |            |                  | instance. Typical work implemented by this function for the        |
   |            |                  | current pipeline instance:                                         |
   |            |                  | Memory allocation;                                                 |
   |            |                  | Parse the pipeline type specific arguments;                        |
   |            |                  | Initialize the pipeline input ports, output ports and tables,      |
   |            |                  | interconnect input ports to tables;                                |
   |            |                  | Set the message handlers.                                          |
   +------------+------------------+--------------------------------------------------------------------+
   | f_free     | Function pointer | Function to free the resources allocated by the back-end of the    |
   |            |                  | current pipeline instance.                                         |
   +------------+------------------+--------------------------------------------------------------------+
   | f_run      | Function pointer | Set to NULL for pipelines implemented using the DPDK library       |
   |            |                  | librte_pipeline (typical case), and to non-NULL otherwise. This    |
   |            |                  | mechanism is made available to support quick integration of        |
   |            |                  | legacy code.                                                       |
   |            |                  | This function is expected to provide the packet processing         |
   |            |                  | related code to be called as part of the CPU thread dispatch       |
   |            |                  | loop, so this function is not allowed to contain an infinite loop. |
   +------------+------------------+--------------------------------------------------------------------+
   | f_timer    | Function pointer | Function to read the pipeline input message queues, handle         |
   |            |                  | the request messages, create response messages and write           |
   |            |                  | the response queues. The format of request and response            |
   |            |                  | messages is defined by each pipeline type, with the exception      |
   |            |                  | of some requests which are mandatory for all pipelines (e.g.       |
   |            |                  | ping, statistics).                                                 |
   +------------+------------------+--------------------------------------------------------------------+
   | f_track    | Function pointer | See section Tracking pipeline output port to physical link         |
   +------------+------------------+--------------------------------------------------------------------+


.. _table_ip_pipelines_front_end:

.. tabularcolumns:: |p{1cm}|p{2cm}|p{12cm}|

.. table:: Pipeline front-end

   +------------+-----------------------+-------------------------------------------------------------------+
   | Field name | Field type            | Description                                                       |
   +============+=======================+===================================================================+
   | f_init     | Function pointer      | Function to initialize the front-end of the current pipeline      |
   |            |                       | instance.                                                         |
   +------------+-----------------------+-------------------------------------------------------------------+
   | f_free     | Function pointer      | Function to free the resources allocated by the front-end of      |
   |            |                       | the current pipeline instance.                                    |
   +------------+-----------------------+-------------------------------------------------------------------+
   | cmds       | Array of CLI commands | Array of CLI commands to be registered to the application CLI     |
   |            |                       | for the current pipeline type. Even though the CLI is executed    |
   |            |                       | by a different pipeline (typically, this is the master pipeline), |
   |            |                       | from modularity perspective is more efficient to keep the         |
   |            |                       | message client side (part of the front-end) together with the     |
   |            |                       | message server side (part of the back-end).                       |
   +------------+-----------------------+-------------------------------------------------------------------+


Tracking pipeline output port to physical link
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each pipeline instance is a standalone block that does not have visibility into the other pipeline instances or
the application-level pipeline inter-connectivity.
In some cases, it is useful for a pipeline instance to get application level information related to pipeline
connectivity, such as to identify the output link (e.g. physical NIC port) where one of its output ports connected,
either directly or indirectly by traversing other pipeline instances.

Tracking can be successful or unsuccessful.
Typically, tracking for a specific pipeline instance is successful when each one of its input ports can be mapped
to a single output port, meaning that all packets read from the current input port can only go out on a single
output port.
Depending on the pipeline type, some exceptions may be allowed: a small portion of the packets, considered exception
packets, are sent out on an output port that is pre-configured for this purpose.

For pass-through pipeline type, the tracking is always successful.
For pipeline types as flow classification, firewall or routing, the tracking is only successful when the number of
output ports for the current pipeline instance is 1.

This feature is used by the IP routing pipeline for adding/removing implicit routes every time a link is brought
up/down.


Table copies
^^^^^^^^^^^^

Fast table copy: pipeline table used by pipeline for the packet processing task, updated through messages, table
data structures are optimized for lookup operation.

Slow table copy: used by the configuration layer, typically updated through CLI commands, kept in sync with the fast
copy (its update triggers the fast copy update).
Required for executing advanced table queries without impacting the packet processing task, therefore the slow copy
is typically organized using different criteria than the fast copy.

Examples:

* Flow classification: Search through current set of flows (e.g. list all flows with a specific source IP address);

* Firewall: List rules in descending order of priority;

* Routing table: List routes sorted by prefix depth and their type (local, remote, default);

* ARP: List entries sorted per output interface.


Packet meta-data
^^^^^^^^^^^^^^^^

Packet meta-data field offsets provided as argument to pipeline instances are essentially defining the data structure
for the packet meta-data used by the current application use-case.
It is very useful to put it in the configuration file as a comment in order to facilitate the readability of the
configuration file.

The reason to use field offsets for defining the data structure for the packet meta-data is due to the C language
limitation of not being able to define data structures at run-time.
Feature to consider: have the configuration file parser automatically generate and print the data structure defining
the packet meta-data for the current application use-case.

Packet meta-data typically contains:

1. Pure meta-data: intermediate data per packet that is computed internally, passed between different tables of
   the same pipeline instance (e.g. lookup key for the ARP table is obtained from the routing table), or between
   different pipeline instances (e.g. flow ID, traffic metering color, etc);

2. Packet fields: typically, packet header fields that are read directly from the packet, or read from the packet
   and saved (duplicated) as a working copy at a different location within the packet meta-data (e.g. Diffserv
   5-tuple, IP destination address, etc).

Several strategies are used to design the packet meta-data, as described in the next subsections.


Store packet meta-data in a different cache line as the packet headers
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

This approach is able to support protocols with variable header length, like MPLS, where the offset of IP header
from the start of the packet (and, implicitly, the offset of the IP header in the packet buffer) is not fixed.
Since the pipelines typically require the specification of a fixed offset to the packet fields (e.g. Diffserv
5-tuple, used by the flow classification pipeline, or the IP destination address, used by the IP routing pipeline),
the workaround is to have the packet RX pipeline copy these fields at fixed offsets within the packet meta-data.

As this approach duplicates some of the packet fields, it requires accessing more cache lines per packet for filling
in selected packet meta-data fields (on RX), as well as flushing selected packet meta-data fields into the
packet (on TX).

Example:

.. code-block:: ini


    ; struct app_pkt_metadata {
    ;	uint32_t ip_da;
    ;      uint32_t hash;
    ;      uint32_t flow_id;
    ;      uint32_t color;
    ; } __attribute__((__packed__));
    ;

    [PIPELINE1]
    ; Packet meta-data offsets
    ip_da_offset = 0;   Used by: routing
    hash_offset = 4;    Used by: RX, flow classification
    flow_id_offset = 8; Used by: flow classification, flow actions
    color_offset = 12;  Used by: flow actions, routing


Overlay the packet meta-data in the same cache line with the packet headers
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

This approach is minimizing the number of cache line accessed per packet by storing the packet metadata in the
same cache line with the packet headers.
To enable this strategy, either some headroom is reserved for meta-data at the beginning of the packet headers
cache line (e.g. if 16 bytes are needed for meta-data, then the packet headroom can be set to 128+16 bytes, so
that NIC writes the first byte of the packet at offset 16 from the start of the first packet cache line),
or meta-data is reusing the space of some packet headers that are discarded from the packet (e.g. input Ethernet
header).

Example:

.. code-block:: ini

    ; struct app_pkt_metadata {
    ;     uint8_t headroom[RTE_PKTMBUF_HEADROOM]; /* 128 bytes (default) */
    ;     union {
    ;         struct {
    ;             struct ether_hdr ether; /* 14 bytes */
    ;             struct qinq_hdr qinq; /* 8 bytes */
    ;         };
    ;         struct {
    ;             uint32_t hash;
    ;             uint32_t flow_id;
    ;             uint32_t color;
    ;         };
    ;     };
    ;     struct ipv4_hdr ip; /* 20 bytes */
    ; } __attribute__((__packed__));
    ;
    [PIPELINE2]
    ; Packet meta-data offsets
    qinq_offset = 142;    Used by: RX, flow classification
    ip_da_offset = 166;   Used by: routing
    hash_offset = 128;    Used by: RX, flow classification
    flow_id_offset = 132; Used by: flow classification, flow actions
    color_offset = 136;   Used by: flow actions, routing


List of pipeline types
~~~~~~~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_types:

.. tabularcolumns:: |p{3cm}|p{5cm}|p{4cm}|p{4cm}|

.. table:: List of pipeline types provided with the application

   +-----------------------+-----------------------------+-----------------------+------------------------------------------+
   | Name                  | Table(s)                    | Actions               | Messages                                 |
   +=======================+=============================+=======================+==========================================+
   | Pass-through          | Passthrough                 | 1. Pkt metadata build | 1. Ping                                  |
   |                       |                             | 2. Flow hash          | 2. Stats                                 |
   | Note: depending on    |                             | 3. Pkt checks         |                                          |
   | port type, can be     |                             | 4. Load balancing     |                                          |
   | used for RX, TX, IP   |                             |                       |                                          |
   | fragmentation, IP     |                             |                       |                                          |
   | reassembly or Traffic |                             |                       |                                          |
   | Management            |                             |                       |                                          |
   +-----------------------+-----------------------------+-----------------------+------------------------------------------+
   | Flow classification   | Exact match                 | 1. Flow ID            | 1. Ping                                  |
   |                       |                             |                       |                                          |
   |                       | * Key = byte array          | 2. Flow stats         | 2. Stats                                 |
   |                       |   (source: pkt metadata)    | 3. Metering           | 3. Flow stats                            |
   |                       | * Data = action dependent   | 4. Network Address    | 4. Action stats                          |
   |                       |                             | 5. Translation (NAT)  | 5. Flow add/ update/ delete              |
   |                       |                             |                       | 6. Default flow add/ update/ delete      |
   |                       |                             |                       | 7. Action update                         |
   +-----------------------+-----------------------------+-----------------------+------------------------------------------+
   | Flow actions          | Array                       | 1. Flow stats         | 1. Ping                                  |
   |                       |                             |                       |                                          |
   |                       | * Key = Flow ID             | 2. Metering           | 2. Stats                                 |
   |                       |   (source: pkt metadata)    | 3. Network Address    | 3. Action stats                          |
   |                       | * Data = action dependent   | 4. Translation (NAT)  | 4. Action update                         |
   +-----------------------+-----------------------------+-----------------------+------------------------------------------+
   | Firewall              | ACL                         | 1. Allow/Drop         | 1. Ping                                  |
   |                       |                             |                       |                                          |
   |                       | * Key = n-tuple             |                       | 2. Stats                                 |
   |                       |   (source: pkt headers)     |                       | 3. Rule add/ update/ delete              |
   |                       | * Data = none               |                       | 4. Default rule add/ update/ delete      |
   +-----------------------+-----------------------------+-----------------------+------------------------------------------+
   | IP routing            | LPM (IPv4 or IPv6,          | 1. TTL decrement and  | 1. Ping                                  |
   |                       | depending on pipeline type) | 2. IPv4 checksum      | 2. Stats                                 |
   |                       |                             |                       |                                          |
   |                       | * Key = IP destination      | 3. update             | 3. Route add/ update/ delete             |
   |                       |   (source: pkt metadata)    | 4. Header             | 4. Default route add/ update/ delete     |
   |                       | * Data = Dependent on       | 5. encapsulation      | 5. ARP entry add/ update/ delete         |
   |                       |   actions and next hop      | 6. (based on next hop | 6. Default ARP entry add/ update/ delete |
   |                       |   type                      | 7. type)              |                                          |
   |                       |                             |                       |                                          |
   |                       | Hash table (for ARP, only   |                       |                                          |
   |                       |                             |                       |                                          |
   |                       | when ARP is enabled)        |                       |                                          |
   |                       |                             |                       |                                          |
   |                       | * Key = (Port ID,           |                       |                                          |
   |                       |   next hop IP address)      |                       |                                          |
   |                       |   (source: pkt meta-data)   |                       |                                          |
   |                       | * Data: MAC address         |                       |                                          |
   +-----------------------+-----------------------------+-----------------------+------------------------------------------+



Command Line Interface (CLI)
----------------------------

Global CLI commands
~~~~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_cli_commands:

.. tabularcolumns:: |p{3cm}|p{6cm}|p{6cm}|

.. table:: Global CLI commands

   +---------+---------------------------------------+--------------------------------------------+
   | Command | Description                           | Syntax                                     |
   +=========+=======================================+============================================+
   | run     | Run CLI commands script file.         | run <file>                                 |
   |         |                                       | <file> = path to file with CLI commands to |
   |         |                                       | execute                                    |
   +---------+---------------------------------------+--------------------------------------------+
   | quit    | Gracefully terminate the application. | quit                                       |
   +---------+---------------------------------------+--------------------------------------------+


CLI commands for link configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_runtime_config:

.. tabularcolumns:: |p{3cm}|p{6cm}|p{6cm}|

.. table:: List of run-time configuration commands for link configuration

   +-------------+--------------------+--------------------------------------------+
   | Command     | Description        | Syntax                                     |
   +=============+====================+============================================+
   | link config | Link configuration | link <link ID> config <IP address> <depth> |
   +-------------+--------------------+--------------------------------------------+
   | link up     | Link up            | link <link ID> up                          |
   +-------------+--------------------+--------------------------------------------+
   | link down   | Link down          | link <link ID> down                        |
   +-------------+--------------------+--------------------------------------------+
   | link ls     | Link list          | link ls                                    |
   +-------------+--------------------+--------------------------------------------+


CLI commands common for all pipeline types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _table_ip_pipelines_mandatory:

.. tabularcolumns:: |p{3cm}|p{6cm}|p{6cm}|

.. table:: CLI commands mandatory for all pipelines

   +--------------------+------------------------------------------------------+----------------------------------------------+
   | Command            | Description                                          | Syntax                                       |
   +====================+======================================================+==============================================+
   | ping               | Check whether specific pipeline instance is alive.   | p <pipeline ID> ping                         |
   |                    | The master pipeline sends a ping request             |                                              |
   |                    | message to given pipeline instance and waits for     |                                              |
   |                    | a response message back.                             |                                              |
   |                    | Timeout message is displayed when the response       |                                              |
   |                    | message is not received before the timer             |                                              |
   |                    | expires.                                             |                                              |
   +--------------------+------------------------------------------------------+----------------------------------------------+
   | stats              | Display statistics for specific pipeline input port, | p <pipeline ID> stats port in <port in ID>   |
   |                    | output port or table.                                | p <pipeline ID> stats port out <port out ID> |
   |                    |                                                      | p <pipeline ID> stats table <table ID>       |
   +--------------------+------------------------------------------------------+----------------------------------------------+
   | input port enable  | Enable given input port for specific pipeline        | p <pipeline ID> port in <port ID> enable     |
   |                    | instance.                                            |                                              |
   +--------------------+------------------------------------------------------+----------------------------------------------+
   | input port disable | Disable given input port for specific pipeline       | p <pipeline ID> port in <port ID> disable    |
   |                    | instance.                                            |                                              |
   +--------------------+------------------------------------------------------+----------------------------------------------+

Pipeline type specific CLI commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The pipeline specific CLI commands are part of the pipeline type front-end.
