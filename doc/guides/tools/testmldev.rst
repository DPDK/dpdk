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
  ML tests are divided into two groups: Device and Model tests.
  Test name should be one of the following supported tests.

  **ML Device Tests** ::

    device_ops

  **ML Model Tests** ::

    model_ops


``--dev_id <n>``
  Set the device ID of the ML device to be used for the test.
  Default value is ``0``.

``--socket_id <n>``
  Set the socket ID of the application resources.
  Default value is ``SOCKET_ID_ANY``.

``--models <model_list>``
  Set the list of model files to be used for the tests.
  Application expects the ``model_list`` in comma separated form
  (i.e. ``--models model_A.bin,model_B.bin``).
  Maximum number of models supported by the test is ``8``.

``--debug``
  Enable the tests to run in debug mode.

``--help``
  Print help message.


ML Device Tests
---------------

ML device tests are functional tests to validate ML device API.
Device tests validate the ML device handling configure, close, start and stop APIs.


Application Options
~~~~~~~~~~~~~~~~~~~

Supported command line options for the ``device_ops`` test are following::

   --debug
   --test
   --dev_id
   --socket_id


DEVICE_OPS Test
~~~~~~~~~~~~~~~

Device ops test validates the device configuration and reconfiguration.


Example
^^^^^^^

Command to run ``device_ops`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=device_ops


ML Model Tests
--------------

Model tests are functional tests to validate ML model API.
Model tests validate the functioning of load, start, stop and unload ML models.


Application Options
~~~~~~~~~~~~~~~~~~~

Supported command line options for the ``model_ops`` test are following::

   --debug
   --test
   --dev_id
   --socket_id
   --models

List of model files to be used for the ``model_ops`` test can be specified
through the option ``--models <model_list>`` as a comma separated list.
Maximum number of models supported in the test is ``8``.

.. note::

   * The ``--models <model_list>`` is a mandatory option for running this test.
   * Options not supported by the test are ignored if specified.


MODEL_OPS Test
~~~~~~~~~~~~~~

The test is a collection of multiple sub-tests,
each with a different order of slow-path operations
when handling with `N` number of models.

**Sub-test A:**
executes the sequence of load / start / stop / unload for a model in order,
followed by next model.

.. _figure_mldev_model_ops_subtest_a:

.. figure:: img/mldev_model_ops_subtest_a.*

   Execution sequence of model_ops subtest A.

**Sub-test B:**
executes load for all models, followed by a start for all models.
Upon successful start of all models, stop is invoked for all models followed by unload.

.. _figure_mldev_model_ops_subtest_b:

.. figure:: img/mldev_model_ops_subtest_b.*

   Execution sequence of model_ops subtest B.

**Sub-test C:**
loads all models, followed by a start and stop of all models in order.
Upon completion of stop, unload is invoked for all models.

.. _figure_mldev_model_ops_subtest_c:

.. figure:: img/mldev_model_ops_subtest_c.*

   Execution sequence of model_ops subtest C.

**Sub-test D:**
executes load and start for all models available.
Upon successful start of all models, stop is executed for the models.

.. _figure_mldev_model_ops_subtest_d:

.. figure:: img/mldev_model_ops_subtest_d.*

   Execution sequence of model_ops subtest D.


Example
^^^^^^^

Command to run ``model_ops`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=model_ops --models model_1.bin,model_2.bin,model_3.bin, model_4.bin


Debug mode
----------

ML tests can be executed in debug mode by enabling the option ``--debug``.
Execution of tests in debug mode would enable additional prints.
