..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2022 Marvell.

dpdk-test-mldev Application
===========================

The ``dpdk-test-mldev`` tool is a Data Plane Development Kit (DPDK) application
that allows testing various mldev use cases.
This application has a generic framework to add new mldev based test cases
to verify functionality
and measure the performance of inference execution on DPDK ML devices.


Application and Options
-----------------------

The application has a number of command line options:

.. code-block:: console

   dpdk-test-mldev [EAL Options] -- [application options]


EAL Options
~~~~~~~~~~~

The following are the EAL command-line options that can be used
with the ``dpdk-test-mldev`` application.
See the DPDK Getting Started Guides for more information on these options.

``-c <COREMASK>`` or ``-l <CORELIST>``
  Set the hexadecimal bitmask of the cores to run on.
  The corelist is a list of cores to use.

``-a <PCI_ID>``
  Attach a PCI based ML device.
  Specific to drivers using a PCI based ML device.

``--vdev <driver>``
  Add a virtual mldev device.
  Specific to drivers using a ML virtual device.


Application Options
~~~~~~~~~~~~~~~~~~~

The following are the command-line options supported by the test application.

``--test <name>``
  Name of the test to execute.

``--dev_id <n>``
  Set the device ID of the ML device to be used for the test.
  Default value is ``0``.

``--socket_id <n>``
  Set the socket ID of the application resources.
  Default value is ``SOCKET_ID_ANY``.

``--debug``
  Enable the tests to run in debug mode.

``--help``
  Print help message.


Debug mode
----------

ML tests can be executed in debug mode by enabling the option ``--debug``.
Execution of tests in debug mode would enable additional prints.
