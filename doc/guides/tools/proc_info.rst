..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

dpdk-procinfo Application
=========================

The dpdk-procinfo application is a Data Plane Development Kit (DPDK) application
that runs as a DPDK secondary process and is capable of retrieving port
statistics, resetting port statistics, printing DPDK memory information and
displaying debug information for port.
This application extends the original functionality that was supported by
dump_cfg.

Running the Application
-----------------------
The application has a number of command line options:

.. code-block:: console

   ./<build_dir>/app/dpdk-procinfo -- -m | [-p PORTMASK] [--stats | --xstats |
   --stats-reset | --xstats-reset] [ --show-port | --show-tm | --show-crypto |
   --show-ring[=name] | --show-mempool[=name] | --iter-mempool=name ]

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

Limitations
-----------

* dpdk-procinfo should run alongside primary process with same DPDK version.

* When running ``dpdk-procinfo`` with shared library mode, it is required to
  pass the same NIC PMD libraries as used for the primary application. Any
  mismatch in PMD library arguments can lead to undefined behavior and results
  affecting primary application too.

* Stats retrieval using ``dpdk-procinfo`` is not supported for virtual devices like PCAP and TAP.

* Since default DPDK EAL arguments for ``dpdk-procinfo`` are ``-c1, -n4 & --proc-type=secondary``,
  It is not expected that the user passes any EAL arguments.
