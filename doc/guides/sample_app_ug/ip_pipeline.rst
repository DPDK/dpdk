..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2018 Intel Corporation.

Internet Protocol (IP) Pipeline Application
===========================================

Application overview
--------------------

The *Internet Protocol (IP) Pipeline* application is intended to be a vehicle for rapid development of packet processing
applications on multi-core CPUs.

Following OpenFlow and P4 design principles, the application can be used to create functional blocks called pipelines out
of input/output ports, tables and actions in a modular way. Multiple pipelines can be inter-connected through packet queues
to create complete applications (super-pipelines).

The pipelines are mapped to application threads, with each pipeline executed by a single thread and each thread able to run
one or several pipelines. The possibilities of creating pipelines out of ports, tables and actions, connecting multiple
pipelines together and mapping the pipelines to execution threads are endless, therefore this application can be seen as
a true application generator.

Pipelines are created and managed through Command Line Interface (CLI):

 * Any standard TCP client (e.g. telnet, netcat, custom script, etc) is typically able to connect to the application, send
   commands through the network and wait for the response before pushing the next command.

 * All the application objects are created and managed through CLI commands:
    * 'Primitive' objects used to create pipeline ports: memory pools, links (i.e. network interfaces), SW queues, traffic managers, etc.
    * Action profiles: used to define the actions to be executed by pipeline input/output ports and tables.
    * Pipeline components: input/output ports, tables, pipelines, mapping of pipelines to execution threads.

Running the application
-----------------------

The application startup command line is::

   dpdk-ip_pipeline [EAL_ARGS] -- [-s SCRIPT_FILE] [-h HOST] [-p PORT]

The application startup arguments are:

``-s SCRIPT_FILE``

 * Optional: Yes

 * Default: Not present

 * Argument: Path to the CLI script file to be run at application startup.
   No CLI script file will run at startup if this argument is not present.

``-h HOST``

 * Optional: Yes

 * Default: ``0.0.0.0``

 * Argument: IP Address of the host running ip pipeline application to be used by
   remote TCP based client (telnet, netcat, etc.) for connection.

``-p PORT``

 * Optional: Yes

 * Default: ``8086``

 * Argument: TCP port number at which the ip pipeline is running.
   This port number should be used by remote TCP client (such as telnet, netcat, etc.) to connect to host application.

Refer to *DPDK Getting Started Guide* for general information on running applications and the Environment Abstraction Layer (EAL) options.

The following is an example command to run ip pipeline application configured for layer 2 forwarding:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-ip_pipeline -c 0x3 -- -s examples/route_ecmp.cli

The application should start successfully and display as follows:

.. code-block:: console

    EAL: Detected 40 lcore(s)
    EAL: Detected 2 NUMA nodes
    EAL: Multi-process socket /var/run/.rte_unix
    EAL: Probing VFIO support...
    EAL: PCI device 0000:02:00.0 on NUMA socket 0
    EAL:   probe driver: 8086:10fb net_ixgbe
    ...

To run remote client (e.g. telnet) to communicate with the ip pipeline application:

.. code-block:: console

    $ telnet 127.0.0.1 8086

When running a telnet client as above, command prompt is displayed:

.. code-block:: console

    Trying 127.0.0.1...
    Connected to 127.0.0.1.
    Escape character is '^]'.

    Welcome to IP Pipeline!

    pipeline>

Once application and telnet client start running, messages can be sent from client to application.
At any stage, telnet client can be terminated using the quit command.


Application stages
------------------

Initialization
~~~~~~~~~~~~~~

During this stage, EAL layer is initialised and application specific arguments are parsed. Furthermore, the data structures
(i.e. linked lists) for application objects are initialized. In case of any initialization error, an error message
is displayed and the application is terminated.

.. _ip_pipeline_runtime:

Run-time
~~~~~~~~

The main thread is creating and managing all the application objects based on CLI input.

Each data plane thread runs one or several pipelines previously assigned to it in round-robin order. Each data plane thread
executes two tasks in time-sharing mode:

#. *Packet processing task*: Process bursts of input packets read from the pipeline input ports.

#. *Message handling task*: Periodically, the data plane thread pauses the packet processing task and polls for request
   messages send by the main thread. Examples: add/remove pipeline to/from current data plane thread, add/delete rules
   to/from given table of a specific pipeline owned by the current data plane thread, read statistics, etc.

Examples
--------

.. _table_examples:

.. tabularcolumns:: |p{3cm}|p{5cm}|p{4cm}|p{4cm}|

.. table:: Pipeline examples provided with the application

   +-----------------------+----------------------+----------------+------------------------------------+
   | Name                  | Table(s)             | Actions        | Messages                           |
   +=======================+======================+================+====================================+
   | L2fwd                 | Stub                 | Forward        | 1. Mempool create                  |
   |                       |                      |                | 2. Link create                     |
   | Note: Implemented     |                      |                | 3. Pipeline create                 |
   | using pipeline with   |                      |                | 4. Pipeline port in/out            |
   | a simple pass-through |                      |                | 5. Pipeline table                  |
   | connection between    |                      |                | 6. Pipeline port in table          |
   | input and output      |                      |                | 7. Pipeline enable                 |
   | ports.                |                      |                | 8. Pipeline table rule add         |
   +-----------------------+----------------------+----------------+------------------------------------+
   | Flow classification   | Exact match          | Forward        | 1. Mempool create                  |
   |                       |                      |                | 2. Link create                     |
   |                       | * Key = byte array   |                | 3. Pipeline create                 |
   |                       |    (16 bytes)        |                | 4. Pipeline port in/out            |
   |                       | * Offset = 278       |                | 5. Pipeline table                  |
   |                       | * Table size = 64K   |                | 6. Pipeline port in table          |
   |                       |                      |                | 7. Pipeline enable                 |
   |                       |                      |                | 8. Pipeline table rule add default |
   |                       |                      |                | 9. Pipeline table rule add         |
   +-----------------------+----------------------+----------------+------------------------------------+
   | Firewall              | ACL                  | Allow/Drop     | 1. Mempool create                  |
   |                       |                      |                | 2. Link create                     |
   |                       | * Key = n-tuple      |                | 3. Pipeline create                 |
   |                       | * Offset = 270       |                | 4. Pipeline port in/out            |
   |                       | * Table size = 4K    |                | 5. Pipeline table                  |
   |                       |                      |                | 6. Pipeline port in table          |
   |                       |                      |                | 7. Pipeline enable                 |
   |                       |                      |                | 8. Pipeline table rule add default |
   |                       |                      |                | 9. Pipeline table rule add         |
   +-----------------------+----------------------+----------------+------------------------------------+
   | IP routing            | LPM (IPv4)           | Forward        | 1. Mempool Create                  |
   |                       |                      |                | 2. Link create                     |
   |                       | * Key = IP dest addr |                | 3. Pipeline create                 |
   |                       | * Offset = 286       |                | 4. Pipeline port in/out            |
   |                       | * Table size = 4K    |                | 5. Pipeline table                  |
   |                       |                      |                | 6. Pipeline port in table          |
   |                       |                      |                | 7. Pipeline enable                 |
   |                       |                      |                | 8. Pipeline table rule add default |
   |                       |                      |                | 9. Pipeline table rule add         |
   +-----------------------+----------------------+----------------+------------------------------------+
   | Equal-cost multi-path | LPM (IPv4)           | Forward,       | 1. Mempool Create                  |
   | routing (ECMP)        |                      | load balance,  | 2. Link create                     |
   |                       | * Key = IP dest addr | encap ether    | 3. Pipeline create                 |
   |                       | * Offset = 286       |                | 4. Pipeline port in/out            |
   |                       | * Table size = 4K    |                | 5. Pipeline table (LPM)            |
   |                       |                      |                | 6. Pipeline table (Array)          |
   |                       |                      |                | 7. Pipeline port in table (LPM)    |
   |                       | Array                |                | 8. Pipeline enable                 |
   |                       |                      |                | 9. Pipeline table rule add default |
   |                       | * Key = Array index  |                | 10. Pipeline table rule add(LPM)   |
   |                       | * Offset = 256       |                | 11. Pipeline table rule add(Array) |
   |                       | * Size = 64K         |                |                                    |
   |                       |                      |                |                                    |
   +-----------------------+----------------------+----------------+------------------------------------+

Command Line Interface (CLI)
----------------------------

Link
~~~~

 Link configuration ::

   link <link_name>
    dev <device_name>|port <port_id>
    rxq <n_queues> <queue_size> <mempool_name>
    txq <n_queues> <queue_size> promiscuous on | off
    [rss <qid_0> ... <qid_n>]

 Note: The PCI device name must be specified in the Domain:Bus:Device.Function format.


Mempool
~~~~~~~

 Mempool create ::

   mempool <mempool_name> buffer <buffer_size>
   pool <pool_size> cache <cache_size> cpu <cpu_id>


Software queue
~~~~~~~~~~~~~~

  Create software queue ::

   swq <swq_name> size <size> cpu <cpu_id>


Traffic manager
~~~~~~~~~~~~~~~

 Add traffic manager subport profile ::

  tmgr subport profile
   <tb_rate> <tb_size>
   <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate> <tc4_rate>
   <tc5_rate> <tc6_rate> <tc7_rate> <tc8_rate>
   <tc9_rate> <tc10_rate> <tc11_rate> <tc12_rate>
   <tc_period>

 Add traffic manager pipe profile ::

  tmgr pipe profile
   <tb_rate> <tb_size>
   <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate> <tc4_rate>
   <tc5_rate> <tc6_rate> <tc7_rate> <tc8_rate>
   <tc9_rate> <tc10_rate> <tc11_rate> <tc12_rate>
   <tc_period>
   <tc_ov_weight>
   <wrr_weight0..3>

 Create traffic manager port ::

  tmgr <tmgr_name>
   rate <rate>
   spp <n_subports_per_port>
   pps <n_pipes_per_subport>
   fo <frame_overhead>
   mtu <mtu>
   cpu <cpu_id>

 Configure traffic manager subport ::

  tmgr <tmgr_name>
   subport <subport_id>
   profile <subport_profile_id>

 Configure traffic manager pipe ::

  tmgr <tmgr_name>
   subport <subport_id>
   pipe from <pipe_id_first> to <pipe_id_last>
   profile <pipe_profile_id>


Tap
~~~

 Create tap port ::

  tap <name>


Cryptodev
~~~~~~~~~

  Create cryptodev port ::

   cryptodev <cryptodev_name>
    dev <DPDK Cryptodev PMD name>
    queue <n_queues> <queue_size>

Action profile
~~~~~~~~~~~~~~

 Create action profile for pipeline input port ::

  port in action profile <profile_name>
   [filter match | mismatch offset <key_offset> mask <key_mask> key <key_value> port <port_id>]
   [balance offset <key_offset> mask <key_mask> port <port_id0> ... <port_id15>]

 Create action profile for the pipeline table ::

  table action profile <profile_name>
   ipv4 | ipv6
   offset <ip_offset>
   fwd
   [balance offset <key_offset> mask <key_mask> outoffset <out_offset>]
   [meter srtcm | trtcm
       tc <n_tc>
       stats none | pkts | bytes | both]
   [tm spp <n_subports_per_port> pps <n_pipes_per_subport>]
   [encap ether | vlan | qinq | mpls | pppoe]
   [nat src | dst
       proto udp | tcp]
   [ttl drop | fwd
       stats none | pkts]
   [stats pkts | bytes | both]
   [sym_crypto cryptodev <cryptodev_name>
       mempool_create <mempool_name> mempool_init <mempool_name>]
   [time]


Pipeline
~~~~~~~~

Create pipeline ::

  pipeline <pipeline_name>
   period <timer_period_ms>
   offset_port_id <offset_port_id>
   cpu <cpu_id>

Create pipeline input port ::

  pipeline <pipeline_name> port in
   bsz <burst_size>
   link <link_name> rxq <queue_id>
   | swq <swq_name>
   | tmgr <tmgr_name>
   | tap <tap_name> mempool <mempool_name> mtu <mtu>
   | source mempool <mempool_name> file <file_name> bpp <n_bytes_per_pkt>
   [action <port_in_action_profile_name>]
   [disabled]

Create pipeline output port ::

  pipeline <pipeline_name> port out
   bsz <burst_size>
   link <link_name> txq <txq_id>
   | swq <swq_name>
   | tmgr <tmgr_name>
   | tap <tap_name>
   | sink [file <file_name> pkts <max_n_pkts>]

Create pipeline table ::

  pipeline <pipeline_name> table
       match
       acl
           ipv4 | ipv6
           offset <ip_header_offset>
           size <n_rules>
       | array
           offset <key_offset>
           size <n_keys>
       | hash
           ext | lru
           key <key_size>
           mask <key_mask>
           offset <key_offset>
           buckets <n_buckets>
           size <n_keys>
       | lpm
           ipv4 | ipv6
           offset <ip_header_offset>
           size <n_rules>
       | stub
   [action <table_action_profile_name>]

Connect pipeline input port to table ::

  pipeline <pipeline_name> port in <port_id> table <table_id>

Display statistics for specific pipeline input port, output port
or table ::

  pipeline <pipeline_name> port in <port_id> stats read [clear]
  pipeline <pipeline_name> port out <port_id> stats read [clear]
  pipeline <pipeline_name> table <table_id> stats read [clear]

Enable given input port for specific pipeline instance ::

  pipeline <pipeline_name> port out <port_id> disable

Disable given input port for specific pipeline instance ::

  pipeline <pipeline_name> port out <port_id> disable

Add default rule to table for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> rule add
     match
        default
     action
        fwd
           drop
           | port <port_id>
           | meta
           | table <table_id>

Add rule to table for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> rule add

  match
     acl
        priority <priority>
        ipv4 | ipv6 <sa> <sa_depth> <da> <da_depth>
        <sp0> <sp1> <dp0> <dp1> <proto>
     | array <pos>
     | hash
        raw <key>
        | ipv4_5tuple <sa> <da> <sp> <dp> <proto>
        | ipv6_5tuple <sa> <da> <sp> <dp> <proto>
        | ipv4_addr <addr>
        | ipv6_addr <addr>
        | qinq <svlan> <cvlan>
     | lpm
        ipv4 | ipv6 <addr> <depth>

  action
     fwd
        drop
        | port <port_id>
        | meta
        | table <table_id>
     [balance <out0> ... <out7>]
     [meter
        tc0 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
        [tc1 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
        tc2 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
        tc3 meter <meter_profile_id> policer g <pa> y <pa> r <pa>]]
     [tm subport <subport_id> pipe <pipe_id>]
     [encap
        ether <da> <sa>
        | vlan <da> <sa> <pcp> <dei> <vid>
        | qinq <da> <sa> <pcp> <dei> <vid> <pcp> <dei> <vid>
        | mpls unicast | multicast
           <da> <sa>
           label0 <label> <tc> <ttl>
           [label1 <label> <tc> <ttl>
           [label2 <label> <tc> <ttl>
           [label3 <label> <tc> <ttl>]]]
        | pppoe <da> <sa> <session_id>]
     [nat ipv4 | ipv6 <addr> <port>]
     [ttl dec | keep]
     [stats]
     [time]
     [sym_crypto
        encrypt | decrypt
        type
        | cipher
           cipher_algo <algo> cipher_key <key> cipher_iv <iv>
        | cipher_auth
           cipher_algo <algo> cipher_key <key> cipher_iv <iv>
           auth_algo <algo> auth_key <key> digest_size <size>
        | aead
           aead_algo <algo> aead_key <key> aead_iv <iv> aead_aad <aad>
           digest_size <size>
        data_offset <data_offset>]

  where:
     <pa> ::= g | y | r | drop

Add bulk rules to table for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> rule add bulk <file_name> <n_rules>

  Where:
  - file_name = path to file
  - File line format = match <match> action <action>

Delete table rule for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> rule delete
     match <match>

Delete default table rule for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> rule delete
     match
        default

Add meter profile to the table for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id> meter profile <meter_profile_id>
   add srtcm cir <cir> cbs <cbs> ebs <ebs>
   | trtcm cir <cir> pir <pir> cbs <cbs> pbs <pbs>

Delete meter profile from the table for specific pipeline instance ::

  pipeline <pipeline_name> table <table_id>
   meter profile <meter_profile_id> delete


Update the dscp table for meter or traffic manager action for specific
pipeline instance ::

   pipeline <pipeline_name> table <table_id> dscp <file_name>

   Where:
      - file_name = path to file
      - exactly 64 lines
      - File line format = <tc_id> <tc_queue_id> <color>, with <color> as: g | y | r


Pipeline enable/disable
~~~~~~~~~~~~~~~~~~~~~~~

   Enable given pipeline instance for specific data plane thread ::

    thread <thread_id> pipeline <pipeline_name> enable


   Disable given pipeline instance for specific data plane thread ::

    thread <thread_id> pipeline <pipeline_name> disable
