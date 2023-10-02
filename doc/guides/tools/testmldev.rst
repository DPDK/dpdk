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
  ML tests are divided into three groups: Device, Model and Inference tests.
  Test name should be one of the following supported tests.

  **ML Device Tests** ::

    device_ops

  **ML Model Tests** ::

    model_ops

  **ML Inference Tests** ::

    inference_ordered
    inference_interleave

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

``--filelist <file_list>``
  Set the list of model, input, output and reference files to be used for the tests.
  Application expects the ``file_list`` to be in comma separated form
  (i.e. ``--filelist <model,input,output>[,reference]``).

  Multiple filelist entries can be specified when running the tests with multiple models.
  Both quantized and dequantized outputs are written to the disk.
  Dequantized output file would have the name specified by the user through ``--filelist`` option.
  A suffix ``.q`` is appended to quantized output filename.
  Maximum number of filelist entries supported by the test is ``8``.

``--repetitions <n>``
  Set the number of inference repetitions to be executed in the test per each model.
  Default value is ``1``.

``--burst_size <n>``
  Set the burst size to be used when enqueuing / dequeuing inferences.
  Default value is ``1``.

``--queue_pairs <n>``
  Set the number of queue-pairs to be used for inference enqueue and dequeue operations.
  Default value is ``1``.

``--queue_size <n>``
  Set the size of queue-pair to be created for inference enqueue / dequeue operations.
  Queue size would translate into ``rte_ml_dev_qp_conf::nb_desc`` field during queue-pair creation.
  Default value is ``1``.

``--tolerance <n>``
  Set the tolerance value in percentage to be used for output validation.
  Default value is ``0``.

``--stats``
  Enable reporting device extended stats.

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
   --queue_pairs
   --queue_size


DEVICE_OPS Test
~~~~~~~~~~~~~~~

Device ops test validates the device configuration and reconfiguration support.
The test configures ML device based on the options
``--queue_pairs`` and ``--queue_size`` specified by the user,
and later reconfigures the ML device with the number of queue pairs and queue size
based on the maximum specified through the device info.


Example
^^^^^^^

Command to run ``device_ops`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=device_ops

Command to run ``device_ops`` test with user options:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=device_ops --queue_pairs <M> --queue_size <N>


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


ML Inference Tests
------------------

Inference tests are a set of tests to validate end-to-end inference execution on ML device.
These tests executes the full sequence of operations required to run inferences
with one or multiple models.


Application Options
~~~~~~~~~~~~~~~~~~~

Supported command line options for inference tests are following::

   --debug
   --test
   --dev_id
   --socket_id
   --filelist
   --repetitions
   --burst_size
   --queue_pairs
   --queue_size
   --tolerance
   --stats

List of files to be used for the inference tests can be specified
through the option ``--filelist <file_list>`` as a comma separated list.
A filelist entry would be of the format
``--filelist <model_file,input_file,output_file>[,reference_file]``
and is used to specify the list of files required to test with a single model.
Multiple filelist entries are supported by the test, one entry per model.
Maximum number of file entries supported by the test is ``8``.

When ``--burst_size <num>`` option is specified for the test,
enqueue and dequeue burst would try to enqueue or dequeue
``num`` number of inferences per each call respectively.

In the inference test, a pair of lcores are mapped to each queue pair.
Minimum number of lcores required for the tests is equal to ``(queue_pairs * 2 + 1)``.

Output validation of inference would be enabled only
when a reference file is specified through the ``--filelist`` option.
Application would additionally consider the tolerance value
provided through ``--tolerance`` option during validation.
When the tolerance values is 0, CRC32 hash of inference output
and reference output are compared.
When the tolerance is non-zero, element wise comparison of output is performed.
Validation is considered as successful only
when all the elements of the output tensor are with in the tolerance range specified.

Enabling ``--stats`` would print the extended stats supported by the driver.

.. note::

   * The ``--filelist <file_list>`` is a mandatory option for running inference tests.
   * Options not supported by the tests are ignored if specified.
   * Element wise comparison is not supported when
     the output dtype is either fp8, fp16 or bfloat16.
     This is applicable only when the tolerance is greater than zero
     and for pre-quantized models only.


INFERENCE_ORDERED Test
~~~~~~~~~~~~~~~~~~~~~~

This is a functional test for validating the end-to-end inference execution on ML device.
This test configures ML device and queue pairs
as per the queue-pair related options (queue_pairs and queue_size) specified by the user.
Upon successful configuration of the device and queue pairs,
the first model specified through the filelist is loaded to the device
and inferences are enqueued by a pool of worker threads to the ML device.
Total number of inferences enqueued for the model are equal to the repetitions specified.
A dedicated pool of worker threads would dequeue the inferences from the device.
The model is unloaded upon completion of all inferences for the model.
The test would continue loading and executing inference requests for all models
specified through ``filelist`` option in an ordered manner.

.. _figure_mldev_inference_ordered:

.. figure:: img/mldev_inference_ordered.*

   Execution of inference_ordered on single model.


Example
^^^^^^^

Example command to run ``inference_ordered`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_ordered --filelist model.bin,input.bin,output.bin

Example command to run ``inference_ordered`` test with a specific burst size:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_ordered --filelist model.bin,input.bin,output.bin \
        --burst_size 12

Example command to run ``inference_ordered`` test with multiple queue-pairs and queue size:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_ordered --filelist model.bin,input.bin,output.bin \
        --queue_pairs 4 --queue_size 16

Example command to run ``inference_ordered`` with output validation using tolerance of ``1%``:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_ordered --filelist model.bin,input.bin,output.bin,reference.bin \
        --tolerance 1.0


INFERENCE_INTERLEAVE Test
~~~~~~~~~~~~~~~~~~~~~~~~~

This is a stress test for validating the end-to-end inference execution on ML device.
The test configures the ML device and queue pairs
as per the queue-pair related options (queue_pairs and queue_size) specified by the user.
Upon successful configuration of the device and queue pairs,
all models specified through the filelist are loaded to the device.
Inferences for multiple models are enqueued by a pool of worker threads in parallel.
Inference execution by the device is interleaved between multiple models.
Total number of inferences enqueued for a model are equal to the repetitions specified.
An additional pool of threads would dequeue the inferences from the device.
Models would be unloaded upon completion of inferences for all models loaded.

.. _figure_mldev_inference_interleave:

.. figure:: img/mldev_inference_interleave.*

   Execution of inference_interleave on single model.


Example
^^^^^^^

Example command to run ``inference_interleave`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_interleave --filelist model.bin,input.bin,output.bin

Example command to run ``inference_interleave`` test with multiple models:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_interleave --filelist model_A.bin,input_A.bin,output_A.bin \
        --filelist model_B.bin,input_B.bin,output_B.bin

Example command to run ``inference_interleave`` test
with a specific burst size, multiple queue-pairs and queue size:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_interleave --filelist model.bin,input.bin,output.bin \
        --queue_pairs 8 --queue_size 12 --burst_size 16

Example command to run ``inference_interleave`` test
with multiple models and output validation using tolerance of ``2.0%``:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-mldev -c 0xf -a <PCI_ID> -- \
        --test=inference_interleave \
        --filelist model_A.bin,input_A.bin,output_A.bin,reference_A.bin \
        --filelist model_B.bin,input_B.bin,output_B.bin,reference_B.bin \
        --tolerance 2.0


Debug mode
----------

ML tests can be executed in debug mode by enabling the option ``--debug``.
Execution of tests in debug mode would enable additional prints.

When a validation failure is observed, output from that buffer is written to the disk,
with the filenames having similar convention when the test has passed.
Additionally index of the buffer would be appended to the filenames.
