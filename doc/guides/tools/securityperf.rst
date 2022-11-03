.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2022 Marvell.

Security Performance Tool
=========================

The ``dpdk-test-security-perf`` tool is a Data Plane Development Kit (DPDK)
utility to test ``rte_security`` session create/destroy rates.
Test covers supported combinations of cipher and auth algorithms.

Limitations
-----------

* Test only ESP tunnel mode with IPv4.

Running the Application
-----------------------

EAL Command-line Options
~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`EAL parameters (Linux) <../linux_gsg/linux_eal_parameters>`
or :doc:`EAL parameters (FreeBSD) <../freebsd_gsg/freebsd_eal_parameters>`
for a list of available EAL command-line options.

Security Performance Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following are the command-line options for the security performance
application.
They must be separated from the EAL options, shown in the previous section,
with a ``--`` separator:

.. code-block:: console

   sudo ./dpdk-test-security-perf -- --nb-sess=163840 --inbound

The command-line options are:

``--help``
  Display a help message and quit.

``--nb-sess``
  Set the number of sessions to be created, default value is 163840.

``--inbound``
  IPsec SA direction to be tested with.
  By default if this option is not provided, outbound direction will be tested.
