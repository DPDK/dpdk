.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2026 Intel Corporation

dpdk-telemetry-watcher Tool
===========================

The ``dpdk-telemetry-watcher`` tool monitors DPDK telemetry statistics
continuously on the command line.
It wraps the ``dpdk-telemetry.py`` script to provide real-time statistics display capabilities.


Running the Tool
----------------

The tool has a number of command line options:

.. code-block:: console

   dpdk-telemetry-watcher.py [options] stat1 stat2 ...


Options
-------

.. program:: dpdk-telemetry-watcher.py

.. option:: -h, --help

   Display usage information and quit

.. option:: -f FILE_PREFIX, --file-prefix FILE_PREFIX

   Provide file-prefix for DPDK runtime directory.
   Passed to ``dpdk-telemetry.py`` to identify the target DPDK application.
   Default is ``rte``.

.. option:: -i INSTANCE, --instance INSTANCE

   Provide instance number for DPDK application
   when multiple applications share the same file-prefix.
   Passed to ``dpdk-telemetry.py`` to identify the target DPDK application instance.
   Default is ``0``.

.. option:: -l, --list

   List all possible file-prefixes and exit.
   This is useful to discover which DPDK applications are currently running.

.. option:: -t TIMEOUT, --timeout TIMEOUT

   Number of iterations to run before exiting.
   If not specified, the tool runs indefinitely until interrupted with Ctrl+C.

.. option:: -d, --delta

   Display delta values instead of absolute values.
   This shows the change in statistics since the last iteration,
   which is useful for monitoring per-second rates.

.. option:: stat

   Statistics to monitor in format ``command.field``.
   Multiple statistics can be specified and will be displayed in columns.
   See the `Statistics Format`_ section below for details on specifying statistics.


Statistics Format
-----------------

Specify statistics in the format ``command.field`` where:

* ``command`` is a telemetry command (e.g., ``/ethdev/stats,0``)
* ``field`` is a field name from the command's JSON response (e.g., ``ipackets``)

To discover available commands and fields, follow the steps below:

1. Use ``dpdk-telemetry.py`` interactively to explore available commands
2. Use the ``/`` command to list all available telemetry endpoints
3. Query specific commands to see their response format

Example telemetry commands:

* ``/ethdev/list`` - List all Ethernet devices
* ``/ethdev/stats,N`` - Get statistics for Ethernet device N
* ``/ethdev/xstats,N`` - Get extended statistics for Ethernet device N
* ``/eal/mempool_list`` - List all mempools
* ``/mempool/info,N`` - Get information about mempool N

See `Examples`_ section for usage examples based on the results of these telemetry commands.

Examples
--------

Monitor received packets on Ethernet device 0::

   dpdk-telemetry-watcher.py /ethdev/stats,0.ipackets

Monitor received and transmitted packets on device 0::

   dpdk-telemetry-watcher.py /ethdev/stats,0.ipackets /ethdev/stats,0.opackets

Monitor a DPDK application with a custom file-prefix::

   dpdk-telemetry-watcher.py -f myapp /ethdev/stats,0.ipackets

List all running DPDK applications::

   dpdk-telemetry-watcher.py -l


Dependencies
------------

The tool requires:

* Python 3
* The ``dpdk-telemetry.py`` script (in the same directory or in PATH)

For monitoring, a DPDK application with telemetry enabled must be running,
though the watcher can start before the application and will connect automatically.
