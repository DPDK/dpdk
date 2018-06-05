..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Canonical Limited. All rights reserved.


dpdk-pmdinfo Application
========================

The ``dpdk-pmdinfo`` tool is a Data Plane Development Kit (DPDK) utility that
can dump a PMDs hardware support info.


Running the Application
-----------------------

The tool has a number of command line options:

.. code-block:: console

   dpdk-pmdinfo [-hrtp] [-d <pci id file] <elf-file>

   -h, --help            Show a short help message and exit
   -r, --raw             Dump as raw json strings
   -d FILE, --pcidb=FILE Specify a pci database to get vendor names from
   -t, --table           Output information on hw support as a hex table
   -p, --plugindir       Scan dpdk for autoload plugins

.. Note::

   * Parameters inside the square brackets represents optional parameters.
