..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

dpdk-proc-info Application
==========================

The dpdk-proc-info application is a Data Plane Development Kit (DPDK) application
that runs as a DPDK secondary process and is capable of retrieving port
statistics, resetting port statistics, printing DPDK memory information and
displaying debug information for port.
This application extends the original functionality that was supported by
dump_cfg.

Running the Application
-----------------------
The application has a number of command line options:

.. code-block:: console

   ./<build_dir>/app/dpdk-proc-info -- -m | [-p PORTMASK] [--stats | --xstats |
   --stats-reset | --xstats-reset] [ --show-port | --show-tm | --show-crypto |
   --show-ring[=name] | --show-mempool[=name] | --iter-mempool=name |
   --show-port-private | --version | --firmware-version | --show-rss-reta |
   --show-module-eeprom | --show-rx-descriptor queue_id:offset:num |
   --show-tx-descriptor queue_id:offset:num | --show-edev-queue-xstats=queue_num:evdev_id |
   --show-edev-port-xstats=port_num :evdev_id | --edev-dump-xstats=evdev_id |
   --edev-reset-xstats=evdev_id | --show-edev-device-xstats=evdev_id]

Parameters
~~~~~~~~~~
**-p PORTMASK**: Hexadecimal bitmask of ports to configure.

**--stats**
The stats parameter controls the printing of generic port statistics. If no
port mask is specified stats are printed for all DPDK ports.

**--xstats**
The xstats parameter controls the printing of extended port statistics. If no
port mask is specified xstats are printed for all DPDK ports.

**--stats-reset**
The stats-reset parameter controls the resetting of generic port statistics. If
no port mask is specified, the generic stats are reset for all DPDK ports.

**--xstats-reset**
The xstats-reset parameter controls the resetting of extended port statistics.
If no port mask is specified xstats are reset for all DPDK ports.

**-m**: Print DPDK memory information.

**--show-port**
The show-port parameter displays port level various configuration information
associated to RX port queue pair.

**--show-tm**
The show-tm parameter displays per port traffic manager settings, current
configurations and statistics.

**--show-crypto**
The show-crypto parameter displays available cryptodev configurations,
settings and stats per node.

**--show-ring[=name]**
The show-ring parameter display current allocation of all ring with
debug information. Specifying the name allows to display details for specific
ring. For invalid or no ring name, whole list is dump.

**--show-mempool[=name]**
The show-mempool parameter display current allocation of all mempool
debug information. Specifying the name allows to display details for specific
mempool. For invalid or no mempool name, whole list is dump.

**--iter-mempool=name**
The iter-mempool parameter iterates and displays mempool elements specified
by name. For invalid or no mempool name no elements are displayed.

**--show-port-private**
The show-port-private parameter displays ports private information.

**--version**
The version parameter displays DPDK version.

**--firmware-version**
The firmware-version parameter displays ethdev firmware version.

**--show-rss-reta**
The show-rss-reta parameter displays ports rss redirection table.

**--show-module-eeprom**
The show-module-eeprom parameter displays ports module eeprom information.

**--show-rx-descriptor queue_id:offset:num**
The show-rx-descriptor parameter displays ports Rx descriptor information
specified by queue_id, offset and num.
queue_id: A Rx queue identifier on this port.
offset: The offset of the descriptor starting from tail.
num: The number of the descriptors to dump.

**--show-tx-descriptor queue_id:offset:num**
The show-tx-descriptor parameter displays ports Tx descriptor information
specified by queue_id, offset and num.
queue_id: A Tx queue identifier on this port.
offset: The offset of the descriptor starting from tail.
num: The number of the descriptors to dump.

**--show-edev-queue-xstats queue_num:evdev_id**
The show-edev-queue-xstats parameter enables stats for specified queue or all queues.
queue_num: The queue number to get queue xstats for this specified queue or * for all queues.
evdev_id: Id of the eventdev device to display xstats.

**--show-edev-port-xstats port_num:evdev_id**
The show-edev-port-xstats parameter enables stats for specified port or all ports.
port_num: The port number to get port xstats for this specified port or * for all ports.
evdev_id: Id of the eventdev device to display xstats.

**--edev-dump-xstats evdev_id**
The edev-dump-xstats parameter dumps all eventdev stats.
evdev_id: Id of the eventdev device to display xstats.

**--edev-reset-xstats evdev_id**
The edev-reset-xstats parameter resets eventdev xstats after reading.
evdev_id: Id of the eventdev device to display xstats.

**--show-edev-device-xstats evdev_id**
The show-edev-device-xstats parameter displays eventdev device xstats.
evdev_id: Id of the eventdev device to display xstats.

Limitations
-----------

* dpdk-proc-info should run alongside primary process with same DPDK version.

* When running ``dpdk-proc-info`` with shared library mode, it is required to
  pass the same NIC PMD libraries as used for the primary application. Any
  mismatch in PMD library arguments can lead to undefined behavior and results
  affecting primary application too.

* Stats retrieval using ``dpdk-proc-info`` is not supported for virtual devices like PCAP and TAP.

* Since default DPDK EAL arguments for ``dpdk-proc-info`` are ``-c1, -n4 & --proc-type=secondary``,
  It is not expected that the user passes any EAL arguments.
