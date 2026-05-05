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

Dependencies
------------

The tool requires:

* Python 3
* The ``dpdk-telemetry.py`` script (in the same directory or in PATH)

For monitoring, a DPDK application with telemetry enabled must be running,
though the watcher can start before the application and will connect automatically.
