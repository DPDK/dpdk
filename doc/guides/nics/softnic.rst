..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Soft NIC Poll Mode Driver
=========================

The Soft NIC allows building custom NIC pipelines in software. The Soft NIC pipeline
is DIY and reconfigurable through ``firmware`` (DPDK Packet Framework script).

The Soft NIC leverages the DPDK Packet Framework libraries (librte_port,
librte_table and librte_pipeline) to make it modular, flexible and extensible
with new functionality. Please refer to DPDK Programmer's Guide, Chapter
``Packet Framework`` and DPDK Sample Application User Guide,
Chapter ``IP Pipeline Application`` for more details.

The Soft NIC is configured through the standard DPDK ethdev API (ethdev, flow,
QoS, security). The internal framework is not externally visible.

Key benefits:
 - Can be used to augment missing features to HW NICs.
 - Allows consumption of advanced DPDK features without application redesign.
 - Allows out-of-the-box performance boost of DPDK consumers applications simply by
   instantiating this type of Ethernet device.

Flow
----
* ``Device creation``: Each Soft NIC instance is a virtual device.

* ``Device start``: The Soft NIC firmware script is executed every time the device
  is started. The firmware script typically creates several internal objects,
  such as: memory pools, SW queues, traffic manager, action profiles, pipelines,
  etc.

* ``Device stop``: All the internal objects that were previously created by the
  firmware script during device start are now destroyed.

* ``Device run``: Each Soft NIC device needs one or several CPU cores to run.
  The firmware script maps each internal pipeline to a CPU core. Multiple
  pipelines can be mapped to the same CPU core. In order for a given pipeline
  assigned to CPU core X to run, the application needs to periodically call on
  CPU core X the `rte_pmd_softnic_run()` function for the current Soft NIC
  device.

* ``Application run``: The application reads packets from the Soft NIC device RX
  queues and writes packets to the Soft NIC device TX queues.

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in ``System Requirements``
section of :ref:`the DPDK documentation <linux_gsg>` or refer to *DPDK
Release Notes*.


Runtime Configuration
---------------------

The user can specify below arguments in EAL ``--vdev`` options to create the
Soft NIC device instance:

        --vdev "net_softnic0,firmware=firmware.cli,conn_port=8086"

#.  ``firmware``: path to the firmware script used for Soft NIC configuration.
    The example "firmware" script is provided at `drivers/net/softnic/`.
    (Optional: No, Default = NA)

#.  ``conn_port``: tcp connection port (non-zero value) used by remote client
    (for examples- telnet, netcat, etc.) to connect and configure Soft NIC device in run-time.
    (Optional: yes, Default value: 0, no connection with external client)

#.  ``cpu_id``: numa node id. (Optional: yes, Default value: 0)

#.  ``tm_n_queues``: number of traffic manager's scheduler queues. The traffic manager
    is based on DPDK *librte_sched* library. (Optional: yes, Default value: 65,536 queues)

#.  ``tm_qsize0``: size of scheduler queue 0 (traffic class 0) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize1``: size of scheduler queue 1 (traffic class 1) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize2``: size of scheduler queue 2 (traffic class 2) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize3``: size of scheduler queue 3 (traffic class 3) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize4``: size of scheduler queue 4 (traffic class 4) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize5``: size of scheduler queue 5 (traffic class 5) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize6``: size of scheduler queue 6 (traffic class 6) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize7``: size of scheduler queue 7 (traffic class 7) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize8``: size of scheduler queue 8 (traffic class 8) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize9``: size of scheduler queue 9 (traffic class 9) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize10``: size of scheduler queue 10 (traffic class 10) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize11``: size of scheduler queue 11 (traffic class 11) of the pipes/subscribers.
    (Optional: yes, Default: 64)

#.  ``tm_qsize12``: size of scheduler queue 12 (traffic class 12) of the pipes/subscribers.
    (Optional: yes, Default: 64)


Soft NIC testing
----------------

* Run testpmd application with Soft NIC device with loopback feature
  enabled on Soft NIC port:

    .. code-block:: console

         ./dpdk-testpmd -c 0x7 -s 0x4 --vdev 'net_softnic0,firmware=<script path>/firmware.cli,cpu_id=0,conn_port=8086' -- -i
              --portmask=0x2

    .. code-block:: console

        ...
        Interactive-mode selected
        Set softnic packet forwarding mode
        ...
        Configuring Port 0 (socket 0)
        Port 0: 90:E2:BA:37:9D:DC
        Configuring Port 1 (socket 0)

        ; SPDX-License-Identifier: BSD-3-Clause
        ; Copyright(c) 2018 Intel Corporation

        link LINK dev 0000:02:00.0

        pipeline RX period 10 offset_port_id 0
        pipeline RX port in bsz 32 link LINK rxq 0
        pipeline RX port out bsz 32 swq RXQ0
        pipeline RX table match stub
        pipeline RX port in 0 table 0

        pipeline TX period 10 offset_port_id 0
        pipeline TX port in bsz 32 swq TXQ0
        pipeline TX port out bsz 32 link LINK txq 0
        pipeline TX table match stub
        pipeline TX port in 0 table 0

        thread 2 pipeline RX enable
        thread 2 pipeline TX enable
        Port 1: 00:00:00:00:00:00
        Checking link statuses...
        Done
        testpmd>

* Start forwarding

    .. code-block:: console

         testpmd> start
         softnic packet forwarding - ports=1 - cores=1 - streams=1 - NUMA support enabled, MP over anonymous pages disabled
         Logical Core 1 (socket 0) forwards packets on 1 streams:
         RX P=2/Q=0 (socket 0) -> TX P=2/Q=0 (socket 0) peer=02:00:00:00:00:02

         softnic packet forwarding packets/burst=32
         nb forwarding cores=1 - nb forwarding ports=1
         port 0: RX queue number: 1 Tx queue number: 1
         Rx offloads=0x1000 Tx offloads=0x0
         RX queue: 0
         RX desc=512 - RX free threshold=32
         RX threshold registers: pthresh=8 hthresh=8  wthresh=0
         RX Offloads=0x0
         TX queue: 0
         TX desc=512 - TX free threshold=32
         TX threshold registers: pthresh=32 hthresh=0  wthresh=0
         TX offloads=0x0 - TX RS bit threshold=32
         port 1: RX queue number: 1 Tx queue number: 1
         Rx offloads=0x0 Tx offloads=0x0
         RX queue: 0
         RX desc=0 - RX free threshold=0
         RX threshold registers: pthresh=0 hthresh=0  wthresh=0
         RX Offloads=0x0
         TX queue: 0
         TX desc=0 - TX free threshold=0
         TX threshold registers: pthresh=0 hthresh=0  wthresh=0
         TX offloads=0x0 - TX RS bit threshold=0

* Softnic device can be configured using remote client (e.g. telnet). However,
  testpmd application doesn't support configuration through telnet :

    .. code-block:: console

        $ telnet 127.0.0.1 8086
        Trying 127.0.0.1...
        Connected to 127.0.0.1.
        Escape character is '^]'.

        Welcome to Soft NIC!

        softnic>

* Add/update Soft NIC pipeline table match-action entries from telnet client:

    .. code-block:: console

        softnic> pipeline RX table 0 rule add match default action fwd port 0
        softnic> pipeline TX table 0 rule add match default action fwd port 0

Soft NIC Firmware
-----------------

The Soft NIC firmware, for example- `softnic/firmware.cli`, consists of following CLI commands
for creating and managing software based NIC pipelines. For more details, please refer to CLI
command description provided in `softnic/rte_eth_softnic_cli.c`.

* Physical port for packets send/receive:

    .. code-block:: console

        link LINK dev 0000:02:00.0

* Pipeline create:

    .. code-block:: console

        pipeline RX period 10 offset_port_id 0           (Soft NIC rx-path pipeline)
        pipeline TX period 10 offset_port_id 0           (Soft NIC tx-path pipeline)

* Pipeline input/output port create

    .. code-block:: console

        pipeline RX port in bsz 32 link LINK rxq 0      (Soft NIC rx pipeline input port)
        pipeline RX port out bsz 32 swq RXQ0            (Soft NIC rx pipeline output port)
        pipeline TX port in bsz 32 swq TXQ0             (Soft NIC tx pipeline input port)
        pipeline TX port out bsz 32 link LINK txq 0     (Soft NIC tx pipeline output port)

* Pipeline table create

    .. code-block:: console

        pipeline RX table match stub             (Soft NIC rx pipeline match-action table)
        pipeline TX table match stub             (Soft NIC tx pipeline match-action table)

* Pipeline input port connection with table

    .. code-block:: console

        pipeline RX port in 0 table 0          (Soft NIC rx pipeline input port 0 connection with table 0)
        pipeline TX port in 0 table 0          (Soft NIC tx pipeline input port 0 connection with table 0)

* Pipeline table match-action rules add

    .. code-block:: console

        pipeline RX table 0 rule add match default action fwd port 0        (Soft NIC rx pipeline table 0 rule)
        pipeline TX table 0 rule add match default action fwd port 0        (Soft NIC tx pipeline table 0 rule)

* Enable pipeline on CPU thread

    .. code-block:: console

        thread 2 pipeline RX enable        (Soft NIC rx pipeline enable on cpu thread id 2)
        thread 2 pipeline TX enable        (Soft NIC tx pipeline enable on cpu thread id 2)

QoS API Support:
----------------

SoftNIC PMD implements ethdev traffic management APIs ``rte_tm.h`` that
allow building and committing traffic manager hierarchy, configuring hierarchy
nodes of the Quality of Service (QoS) scheduler supported by DPDK librte_sched
library. Furthermore, APIs for run-time update to the traffic manager hierarchy
are supported by PMD.

SoftNIC PMD also implements ethdev traffic metering and policing APIs
``rte_mtr.h`` that enables metering and marking of the packets with the
appropriate color (green, yellow or red), according to the traffic metering
algorithm. For the meter output color, policer actions like
`keep the packet color same`, `change the packet color` or `drop the packet`
can be configured.

.. Note::

    The SoftNIC does not support the meter object shared by several flows,
    thus only supports creating meter object private to the flow. Once meter
    object is successfully created, it can be linked to the specific flow by
    specifying the ``meter`` flow action in the flow rule.

Flow API support:
-----------------

The SoftNIC PMD implements ethdev flow APIs ``rte_flow.h`` that allow validating
flow rules, adding flow rules to the SoftNIC pipeline as table rules, deleting
and querying the flow rules. The PMD provides new cli command for creating the
flow group and their mapping to the SoftNIC pipeline and table. This cli should
be configured as part of firmware file.

    .. code-block:: console

        flowapi map group <group_id> ingress | egress pipeline <pipeline_name> \
            table <table_id>

From the flow attributes of the flow, PMD uses the group id to get the mapped
pipeline and table. PMD supports number of flow actions such as
``JMP, QUEUE, RSS, DROP, COUNT, METER, VXLAN`` etc.

.. Note::

    The flow must have one terminating actions i.e.
    ``JMP or RSS or QUEUE or DROP``. For the count and drop actions the
    underlying PMD doesn't support the functionality yet. So it is not
    recommended for use.

The flow API can be tested with the help of testpmd application. The SoftNIC
firmware specifies CLI commands for port configuration, pipeline creation,
action profile creation and table creation. Once application gets initialized,
the flow rules can be added through the testpmd CLI.
The PMD will translate the flow rules to the SoftNIC pipeline tables rules.

Example:
~~~~~~~~
Example demonstrates the flow queue action using the SoftNIC firmware and testpmd
commands.

* Prepare SoftNIC firmware

    .. code-block:: console

        link LINK0 dev 0000:83:00.0
        link LINK1 dev 0000:81:00.0
        pipeline RX period 10 offset_port_id 0
        pipeline RX port in bsz 32 link LINK0 rxq 0
        pipeline RX port in bsz 32 link LINK1 rxq 0
        pipeline RX port out bsz 32 swq RXQ0
        pipeline RX port out bsz 32 swq RXQ1
        table action profile AP0 ipv4 offset 278 fwd
        pipeline RX table match hash ext key 16 mask
            00FF0000FFFFFFFFFFFFFFFFFFFFFFFF \
            offset 278 buckets 16K size 65K action AP0
        pipeline RX port in 0 table 0
        pipeline RX port in 1 table 0
        flowapi map group 0 ingress pipeline RX table 0
        pipeline TX period 10 offset_port_id 0
        pipeline TX port in bsz 32 swq TXQ0
        pipeline TX port in bsz 32 swq TXQ1
        pipeline TX port out bsz 32 link LINK0 txq 0
        pipeline TX port out bsz 32 link LINK1 txq 0
        pipeline TX table match hash ext key 16 mask
            00FF0000FFFFFFFFFFFFFFFFFFFFFFFF \
            offset 278 buckets 16K size 65K action AP0
        pipeline TX port in 0 table 0
        pipeline TX port in 1 table 0
        pipeline TX table 0 rule add match hash ipv4_5tuple
            1.10.11.12 2.20.21.22 100 200 6 action fwd port 0
        pipeline TX table 0 rule add match hash ipv4_5tuple
            1.10.11.13 2.20.21.23 100 200 6 action fwd port 1
        thread 2 pipeline RX enable
        thread 2 pipeline TX enable

* Run testpmd:

    .. code-block:: console

        ./<build_dir>/app/dpdk-testpmd -c 0x7 -s 0x4 -n 4 \
                                    --vdev 'net_softnic0, \
                                    firmware=./drivers/net/softnic/ \
                                        firmware.cli, \
                                    cpu_id=1,conn_port=8086' -- \
                                    -i --rxq=2, \
                                    --txq=2, --disable-rss --portmask=0x4

* Configure flow rules on softnic:

    .. code-block:: console

        flow create 2 group 0 ingress pattern eth / ipv4 proto mask 255 src \
            mask 255.255.255.255 dst mask  255.255.255.255 src spec
            1.10.11.12 dst spec 2.20.21.22 proto spec 6 / tcp src mask 65535 \
            dst mask 65535 src spec 100 dst spec 200 / end actions queue \
            index 0 / end
        flow create 2 group 0 ingress pattern eth / ipv4 proto mask 255 src \
            mask 255.255.255.255 dst mask  255.255.255.255 src spec 1.10.11.13 \
            dst spec 2.20.21.23 proto spec 6 / tcp src mask 65535 dst mask \
            65535 src spec 100 dst spec 200 / end actions queue index 1 / end
