.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

Flow Performance Tool
=====================

Application for rte_flow performance testing.


Compiling the Application
=========================

The ``test-flow-perf`` application is compiled as part of the main compilation
of the DPDK libraries and tools.

Refer to the DPDK Getting Started Guides for details.


Running the Application
=======================

EAL Command-line Options
------------------------

Please refer to :doc:`EAL parameters (Linux) <../linux_gsg/linux_eal_parameters>`
or :doc:`EAL parameters (FreeBSD) <../freebsd_gsg/freebsd_eal_parameters>` for
a list of available EAL command-line options.


Flow Performance Options
------------------------

The following are the command-line options for the flow performance application.
They must be separated from the EAL options, shown in the previous section,
with a ``--`` separator:

.. code-block:: console

	sudo ./dpdk-test-flow-perf -n 4 -w 08:00.0 --

The command line options are:

*	``--help``
	Display a help message and quit.
